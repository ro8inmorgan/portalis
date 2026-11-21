#include "file_transfer.h"
#include "sd_workbench.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <freertos/task.h>
namespace portalis {
static const char kTransferPage[]=R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Portalis files</title>
<style>body{font:16px system-ui;background:#0b1220;color:#e5eef7;max-width:760px;margin:auto;padding:24px}h1{color:#73e4cf}button,input,select{font:inherit;padding:10px;margin:6px 0;border-radius:6px}button,a{color:#07352d;background:#73e4cf;border:0;padding:9px;text-decoration:none}li{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:12px 0;border-bottom:1px solid #334155}small{color:#a8bbce}#message{white-space:pre-wrap}</style>
<h1>Portalis files</h1><p>Upload scripts and HTML templates. Download captures and surveys from SD.</p>
<select id="group"><option value="scripts">Python scripts</option><option value="templates">HTML templates</option><option value="captures">Packet captures</option><option value="surveys">Wi-Fi surveys</option></select>
<form id="upload"><p><input type="file" id="file" required></p><label><input type="checkbox" id="replace"> Replace existing file</label><br><button>Upload</button><p><small>Uploads: one file, maximum 32 KiB.</small></p></form>
<p id="message" role="status"></p><ul id="files"></ul><button id="prev">Previous</button> <button id="next">Next</button>
<script>
const token='@@TOKEN@@';let offset=0;const group=document.querySelector('#group'),message=document.querySelector('#message');
async function load(){try{const r=await fetch('/list?group='+group.value+'&offset='+offset);if(!r.ok)throw Error(await r.text());const data=await r.json();const list=document.querySelector('#files');list.replaceChildren();for(const f of data.files){const li=document.createElement('li'),label=document.createElement('span'),a=document.createElement('a');label.textContent=f.name+' ('+f.size+' bytes)';a.textContent='Download';a.href='/download?group='+group.value+'&name='+encodeURIComponent(f.name);a.download=f.name;li.append(label,a);list.append(li);}document.querySelector('#prev').disabled=!offset;document.querySelector('#next').disabled=!data.more;document.querySelector('#upload').hidden=!['scripts','templates'].includes(group.value);}catch(e){message.textContent=e.message;}}
group.onchange=()=>{offset=0;message.textContent='';load();};document.querySelector('#prev').onclick=()=>{offset=Math.max(0,offset-64);load();};document.querySelector('#next').onclick=()=>{offset+=64;load();};
document.querySelector('#upload').onsubmit=async e=>{e.preventDefault();const f=document.querySelector('#file').files[0];if(!f)return;if(f.size>32768){message.textContent='File exceeds 32 KiB';return;}const body=new FormData();body.append('file',f);const button=e.target.querySelector('button');button.disabled=true;message.textContent='Uploading…';try{const r=await fetch('/upload?group='+group.value+'&replace='+(document.querySelector('#replace').checked?'1':'0'),{method:'POST',headers:{'X-Portalis-Token':token},body});message.textContent=await r.text();if(r.ok){document.querySelector('#file').value='';await load();}}catch(e){message.textContent=e.message;}finally{button.disabled=false;}};load();
</script></html>)HTML";
void FileTransfer::discardUpload() {upload_.close();if(!temporary_.empty())SD.remove(temporary_.c_str());temporary_.clear();}
bool FileTransfer::authorized() {
    server_.sendHeader("Cache-Control","no-store");server_.sendHeader("X-Content-Type-Options","nosniff");server_.sendHeader("Referrer-Policy","no-referrer");
    if(server_.authenticate("portalis",status_.code))return true;server_.requestAuthentication();return false;
}
void FileTransfer::configure() {
    if(configured_)return;configured_=true;const char* headers[]={"X-Portalis-Token"};server_.collectHeaders(headers,1);
    server_.on("/",HTTP_GET,[this] {if(!authorized())return;String page(kTransferPage);page.replace("@@TOKEN@@",status_.code);server_.send(200,"text/html; charset=utf-8",page);});
    server_.on("/list",HTTP_GET,[this] {if(authorized())list();});
    server_.on("/download",HTTP_GET,[this] {if(authorized())download();});
    server_.on("/upload",HTTP_POST,[this] {
        if(!authorized()) {discardUpload();return;}
        bool ok=uploadDone_ && uploadOk_ && !cancel_ && server_.header("X-Portalis-Token")==status_.code;
        if(ok)ok=commitSdFile(temporary_,target_,overwrite_);
        if(ok) {temporary_.clear();++uploads_;}
        discardUpload();server_.send(ok?200:400,"text/plain",ok?"File saved":"Upload failed: name, size, existing file, or SD error");uploadDone_=uploadOk_=false;
    },[this] {uploadChunk();});
    server_.onNotFound([this] {server_.send(404,"text/plain","Not found");});
}
void FileTransfer::list() {
    std::string group=server_.arg("group").c_str(),folder=transferFolder(group);
    long offset=server_.arg("offset").toInt();if(folder.empty() || offset<0 || offset>100000) {server_.send(400,"text/plain","Invalid folder/page");return;}
    JsonDocument doc;auto files=doc["files"].to<JsonArray>();size_t seen=0;bool more=false;
    File dir=SD.open(folder.c_str());if(!dir && SD.cardType()==CARD_NONE) {server_.send(500,"text/plain","SD unavailable");return;}
    for(File file=dir?dir.openNextFile():File();file;file=dir.openNextFile()) {
        std::string name=file.name();auto slash=name.find_last_of('/');if(slash!=std::string::npos)name.erase(0,slash+1);
        if(!file.isDirectory() && transferName(group,name) && seen++>=size_t(offset)) {
            if(files.size()>=64) {more=true;file.close();break;}
            auto item=files.add<JsonObject>();item["name"]=name;item["size"]=file.size();
        }
        file.close();if(cancel_)break;
    }
    dir.close();doc["more"]=more;String json;serializeJson(doc,json);server_.send(200,"application/json",json);
}
void FileTransfer::download() {
    std::string group=server_.arg("group").c_str(),name=server_.arg("name").c_str(),folder=transferFolder(group);
    if(folder.empty() || !transferName(group,name)) {server_.send(400,"text/plain","Invalid file");return;}
    std::string path=folder+(folder=="/"?"":"/")+name;File file=SD.open(path.c_str());
    if(!file || file.isDirectory()) {server_.send(404,"text/plain","File unavailable");return;}
    size_t remaining=file.size();server_.sendHeader("Content-Disposition",String("attachment; filename=\"")+name.c_str()+"\"");server_.setContentLength(remaining);server_.send(200,"application/octet-stream","");
    auto client=server_.client();client.setTimeout(1);uint8_t buffer[1024];
    while(remaining && client.connected() && !cancel_) {
        size_t got=file.readBytes(reinterpret_cast<char*>(buffer),std::min(remaining,sizeof(buffer)));if(!got)break;
        size_t sent=0;uint32_t started=millis();while(sent<got && !cancel_ && client.connected() && uint32_t(millis()-started)<3000) {size_t n=client.write(buffer+sent,got-sent);sent+=n;if(!n)vTaskDelay(1);}
        remaining-=sent;if(sent!=got)break;vTaskDelay(1);
    }
    file.close();if(!remaining)++downloads_;else client.stop();
}
void FileTransfer::uploadChunk() {
    auto& part=server_.upload();
    if(part.status==UPLOAD_FILE_START) {
        bool second=uploadDone_;discardUpload();uploadOk_=false;uploadSize_=0;
        std::string group=server_.arg("group").c_str(),name=part.filename.c_str(),folder=transferFolder(group);
        if(second || cancel_ || !server_.authenticate("portalis",status_.code) || server_.header("X-Portalis-Token")!=status_.code || (group!="scripts" && group!="templates") || !transferName(group,name))return;
        if(!SD.exists(folder.c_str()) && !SD.mkdir(folder.c_str()))return;
        target_=folder+(folder=="/"?"":"/")+name;overwrite_=server_.arg("replace")=="1";
        if(SD.exists(target_.c_str()) && !overwrite_)return;
        temporary_=target_+".upload";if(SD.exists(temporary_.c_str())) {temporary_.clear();return;}
        upload_=SD.open(temporary_.c_str(),FILE_APPEND);uploadOk_=bool(upload_);
    } else if(part.status==UPLOAD_FILE_WRITE) {
        if(!uploadOk_)return;
        if(cancel_ || part.currentSize>32768-uploadSize_ || upload_.write(part.buf,part.currentSize)!=part.currentSize) {uploadOk_=false;discardUpload();return;}uploadSize_+=part.currentSize;
    } else if(part.status==UPLOAD_FILE_END) {if(upload_) {upload_.flush();upload_.close();}uploadDone_=true;}
    else if(part.status==UPLOAD_FILE_ABORTED) {discardUpload();uploadDone_=uploadOk_=false;}
}
bool FileTransfer::start(const char* address) {
    if(busy() || WiFi.status()!=WL_CONNECTED || SD.cardType()==CARD_NONE)return false;
    status_={};std::snprintf(status_.address,sizeof(status_.address),"%s",address);std::snprintf(status_.code,sizeof(status_.code),"%08lx%08lx",static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
    configure();cancel_=false;uploads_=downloads_=0;uploadDone_=uploadOk_=false;pending_=true;worker_=true;status_.running=true;
    if(xTaskCreate(task,"file-transfer",8192,this,1,nullptr)!=pdPASS) {worker_=false;pending_=false;status_.running=false;std::snprintf(status_.message,sizeof(status_.message),"Not enough memory");return false;}return true;
}
void FileTransfer::task(void* arg) {auto* self=static_cast<FileTransfer*>(arg);self->server_.begin();while(!self->cancel_ && WiFi.status()==WL_CONNECTED) {self->server_.handleClient();vTaskDelay(1);}self->discardUpload();self->server_.stop();self->worker_=false;vTaskDelete(nullptr);}
void FileTransfer::stop() {if(pending_) {cancel_=true;status_.stopping=true;}}
void FileTransfer::poll() {if(!pending_)return;bool done=!worker_.load();status_.uploads=uploads_;status_.downloads=downloads_;if(done) {pending_=false;status_.running=status_.stopping=false;std::snprintf(status_.message,sizeof(status_.message),"File transfer stopped");}}
} // namespace portalis
