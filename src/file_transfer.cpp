#include "file_transfer.h"
#include "sd_workbench.h"
#include "file_explorer_page.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <freertos/task.h>
namespace portalis {
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
    for(const char* action:{"create","mkdir","rename","delete"}) {
        const std::string route=std::string("/")+action;
        server_.on(route.c_str(),HTTP_POST,[this,action] {if(mutationAllowed())mutate(action);});
    }
    server_.on("/upload",HTTP_POST,[this] {
        if(!authorized()) {discardUpload();return;}
        bool ok=uploadDone_ && uploadOk_ && !cancel_ && server_.header("X-Portalis-Token")==status_.code;
        if(ok)ok=commitSdFile(temporary_,target_,overwrite_);
        if(ok) {temporary_.clear();++uploads_;}
        discardUpload();server_.send(ok?200:uploadConflict_?409:400,"text/plain",ok?"File saved":uploadConflict_?"File already exists":"Upload failed: invalid path, 32 MiB limit, or SD error");uploadDone_=uploadOk_=false;
    },[this] {uploadChunk();});
    server_.onNotFound([this] {server_.send(404,"text/plain","Not found");});
}
bool FileTransfer::mutationAllowed() {
    if(!authorized())return false;
    if(cancel_ || server_.header("X-Portalis-Token")!=status_.code) {server_.send(403,"text/plain","Invalid access token or service stopping");return false;}
    return true;
}
bool FileTransfer::removeTree(const std::string& path,unsigned depth) {
    if(cancel_ || depth>16 || !transferPath(path) || path=="/")return false;
    File item=SD.open(path.c_str());if(!item)return false;
    bool directory=item.isDirectory();item.close();
    if(!directory)return SD.remove(path.c_str());
    // Reopen after each removal; FAT directory iterators can shift when entries are deleted.
    while(!cancel_) {
        File dir=SD.open(path.c_str());if(!dir)return false;File child=dir.openNextFile();
        if(!child) {dir.close();return SD.rmdir(path.c_str());}
        std::string name=child.name();auto slash=name.find_last_of('/');if(slash!=std::string::npos)name.erase(0,slash+1);
        child.close();dir.close();
        if(!transferName(name) || !removeTree(transferChild(path,name),depth+1))return false;
        vTaskDelay(1);
    }
    return false;
}
void FileTransfer::mutate(const char* action) {
    const std::string path=server_.arg("path").c_str();
    if(!transferPath(path) || path=="/") {server_.send(400,"text/plain","Invalid path; SD root cannot be changed");return;}
    const auto slash=path.find_last_of('/');const std::string parent=slash==0?"/":path.substr(0,slash);
    File dir=SD.open(parent.c_str());bool parentOk=dir && dir.isDirectory();dir.close();
    if(!parentOk) {server_.send(404,"text/plain","Parent folder unavailable");return;}
    const std::string operation=action;bool ok=false;
    if(operation=="create" || operation=="mkdir") {
        if(SD.exists(path.c_str())) {server_.send(409,"text/plain","Already exists");return;}
        if(operation=="mkdir")ok=SD.mkdir(path.c_str());
        else {File file=SD.open(path.c_str(),FILE_APPEND);ok=bool(file);file.close();}
    } else {
        if(!SD.exists(path.c_str())) {server_.send(404,"text/plain","Item unavailable");return;}
        if(operation=="rename") {
            const std::string name=server_.arg("name").c_str(),target=transferChild(parent,name);
            if(!transferName(name) || !transferPath(target)) {server_.send(400,"text/plain","Invalid name");return;}
            if(SD.exists(target.c_str())) {server_.send(409,"text/plain","Name already exists");return;}
            ok=SD.rename(path.c_str(),target.c_str());
        } else if(operation=="delete") {
            if(server_.arg("confirm")!="yes") {server_.send(400,"text/plain","Deletion requires confirmation");return;}
            ok=removeTree(path);
        }
    }
    server_.send(ok?200:500,"text/plain",ok?"Done":"SD operation failed or stopped; a folder deletion may be incomplete");
}
void FileTransfer::list() {
    std::string folder=server_.arg("path").c_str();
    long offset=server_.arg("offset").toInt();if(!transferPath(folder) || offset<0 || offset>100000) {server_.send(400,"text/plain","Invalid folder/page");return;}
    File dir=SD.open(folder.c_str());if(!dir || !dir.isDirectory()) {server_.send(404,"text/plain","Folder unavailable");return;}
    JsonDocument doc;auto files=doc["files"].to<JsonArray>();size_t seen=0;bool more=false;
    for(File file=dir.openNextFile();file;file=dir.openNextFile()) {
        std::string name=file.name();auto slash=name.find_last_of('/');if(slash!=std::string::npos)name.erase(0,slash+1);
        if(transferName(name) && seen++>=size_t(offset)) {
            if(files.size()>=64) {more=true;file.close();break;}
            auto item=files.add<JsonObject>();item["name"]=name;item["size"]=file.size();item["directory"]=file.isDirectory();
        }
        file.close();if(cancel_)break;
    }
    dir.close();doc["more"]=more;doc["path"]=folder;String json;serializeJson(doc,json);server_.send(200,"application/json",json);
}
void FileTransfer::download() {
    std::string path=server_.arg("path").c_str();
    if(!transferPath(path) || path=="/") {server_.send(400,"text/plain","Invalid file");return;}
    const std::string name=path.substr(path.find_last_of('/')+1);File file=SD.open(path.c_str());
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
        bool second=uploadDone_;discardUpload();uploadOk_=uploadConflict_=false;uploadSize_=0;
        std::string name=part.filename.c_str(),folder=server_.arg("path").c_str();
        if(second || cancel_ || !server_.authenticate("portalis",status_.code) || server_.header("X-Portalis-Token")!=status_.code || !transferPath(folder) || !transferName(name))return;
        File dir=SD.open(folder.c_str());bool validDir=dir && dir.isDirectory();dir.close();if(!validDir)return;
        target_=transferChild(folder,name);if(!transferPath(target_))return;
        overwrite_=server_.arg("replace")=="1";
        File existing=SD.open(target_.c_str());bool isDir=existing && existing.isDirectory();existing.close();if(isDir)return;
        if(SD.exists(target_.c_str()) && !overwrite_) {uploadConflict_=true;return;}
        temporary_=target_+".upload";if(SD.exists(temporary_.c_str())) {temporary_.clear();return;}
        upload_=SD.open(temporary_.c_str(),FILE_APPEND);uploadOk_=bool(upload_);
    } else if(part.status==UPLOAD_FILE_WRITE) {
        if(!uploadOk_)return;
        if(cancel_ || part.currentSize>kTransferUploadLimit-uploadSize_ || upload_.write(part.buf,part.currentSize)!=part.currentSize) {uploadOk_=false;discardUpload();return;}uploadSize_+=part.currentSize;
    } else if(part.status==UPLOAD_FILE_END) {if(upload_) {upload_.flush();upload_.close();}uploadDone_=true;}
    else if(part.status==UPLOAD_FILE_ABORTED) {discardUpload();uploadDone_=uploadOk_=false;}
}
bool FileTransfer::start(const char* address) {
    if(busy() || WiFi.status()!=WL_CONNECTED || SD.cardType()==CARD_NONE)return false;
    status_={};std::snprintf(status_.address,sizeof(status_.address),"%s",address);
    // Five easy-to-type characters; omit ambiguous 0/O and 1/I.
    constexpr char alphabet[]="23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
    uint32_t random=esp_random();
    for(size_t i=0;i<sizeof(status_.code)-1;++i) {status_.code[i]=alphabet[random&31];random>>=5;}
    configure();cancel_=false;uploads_=downloads_=0;uploadDone_=uploadOk_=false;pending_=true;worker_=true;status_.running=true;
    if(xTaskCreate(task,"file-transfer",8192,this,1,nullptr)!=pdPASS) {worker_=false;pending_=false;status_.running=false;std::snprintf(status_.message,sizeof(status_.message),"Not enough memory");return false;}return true;
}
void FileTransfer::task(void* arg) {auto* self=static_cast<FileTransfer*>(arg);self->server_.begin();while(!self->cancel_ && WiFi.status()==WL_CONNECTED) {self->server_.handleClient();vTaskDelay(1);}self->discardUpload();self->server_.stop();self->worker_=false;vTaskDelete(nullptr);}
void FileTransfer::stop() {if(pending_) {cancel_=true;status_.stopping=true;}}
void FileTransfer::poll() {if(!pending_)return;bool done=!worker_.load();status_.uploads=uploads_;status_.downloads=downloads_;if(done) {pending_=false;status_.running=status_.stopping=false;std::snprintf(status_.message,sizeof(status_.message),"File transfer stopped");}}
} // namespace portalis
