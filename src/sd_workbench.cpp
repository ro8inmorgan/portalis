#include "sd_workbench.h"
#include "python_model.h"
#include <esp_system.h>
#include <cstdio>
namespace portalis {
bool commitSdFile(const std::string& temp,const std::string& target,bool overwrite) {
    bool exists=SD.exists(target.c_str());std::string backup=target+".bak";
    if((exists && !overwrite) || SD.exists(backup.c_str())) return false;
    if(exists && !SD.rename(target.c_str(),backup.c_str())) return false;
    if(!SD.rename(temp.c_str(),target.c_str())) {if(exists)SD.rename(backup.c_str(),target.c_str());return false;}
    if(exists) SD.remove(backup.c_str());return true;
}
bool loadEditor(ScriptEditor& editor,const std::string& name,bool create) {
    if(!simpleFileName(name) || !scriptName(name)) {editor.message="Use a simple .py filename";return false;}
    std::string path="/scripts/"+name;
    if(create) {
        if(SD.exists(path.c_str())) {editor.message="File already exists";return false;}
        if(!SD.exists("/scripts") && !SD.mkdir("/scripts")) {editor.message="Cannot create /scripts";return false;}
        editor.reset(name,"",true);return true;
    }
    File file=SD.open(path.c_str());
    if(!file || file.isDirectory() || file.size()>ScriptEditor::limit) {editor.message="Missing file or over 8 KB";return false;}
    std::string text(file.size(),'\0');size_t read=text.empty()?0:file.readBytes(&text[0],text.size());file.close();
    if(read!=text.size()) {editor.message="Could not read script";return false;}
    // Editing is ASCII; refuse unsupported bytes instead of silently changing source.
    std::string normalized;for(unsigned char c:text) {
        if(c=='\r')continue;
        if(c=='\t')normalized+="    ";else if(c=='\n' || (c>=32 && c<127))normalized+=c;
        else {editor.message="Editor supports ASCII text";return false;}
    }
    if(normalized.size()>ScriptEditor::limit) {editor.message="Expanded tabs exceed 8 KB";return false;}
    editor.reset(name,normalized,normalized!=text);return true;
}
bool saveEditor(ScriptEditor& editor) {
    std::string path="/scripts/"+editor.name,temp=path+".tmp";
    if(!simpleFileName(editor.name) || !scriptName(editor.name) || editor.text.size()>ScriptEditor::limit || SD.exists(temp.c_str())) {editor.message="Invalid name or recovery .tmp exists";return false;}
    File file=SD.open(temp.c_str(),FILE_APPEND);
    if(!file) {editor.message="Cannot write to SD";return false;}
    bool ok=file.write(reinterpret_cast<const uint8_t*>(editor.text.data()),editor.text.size())==editor.text.size();file.flush();file.close();
    if(ok)ok=commitSdFile(temp,path,true);
    if(!ok) {SD.remove(temp.c_str());editor.message="Save failed; check SD / .bak";return false;}
    editor.dirty=false;editor.message="Saved";return true;
}
bool SurveyRecorder::write(const std::string& row) {
    if(bytes_+row.size()>8*1024*1024) {stop();std::snprintf(status_.message,sizeof(status_.message),"8 MB limit; saved");return false;}
    if(file_.write(reinterpret_cast<const uint8_t*>(row.data()),row.size())!=row.size()) {stop();status_.failed=true;std::snprintf(status_.message,sizeof(status_.message),"SD write failed");return false;}
    bytes_+=row.size();return true;
}
bool SurveyRecorder::start(uint32_t now,uint32_t generation) {
    if(status_.recording)return false;status_={};bytes_=0;started_=now;generation_=generation;
    if(SD.cardType()==CARD_NONE || (!SD.exists("/surveys") && !SD.mkdir("/surveys"))) {status_.failed=true;std::snprintf(status_.message,sizeof(status_.message),"SD card unavailable");return false;}
    bool fresh=false;for(int i=0;i<16;++i) {std::snprintf(status_.path,sizeof(status_.path),"/surveys/survey-%08lx.csv",static_cast<unsigned long>(esp_random()));if(!SD.exists(status_.path)) {fresh=true;break;}}
    if(!fresh || !(file_=SD.open(status_.path,FILE_APPEND))) {status_.failed=true;std::snprintf(status_.message,sizeof(status_.message),"Cannot create survey file");return false;}
    status_.recording=true;
    if(!write("elapsed_ms,sweep,status,total_seen,ssid,bssid,channel,rssi_dbm,security\r\n"))return false;
    file_.flush();return true;
}
void SurveyRecorder::observe(const RadarSnapshot& snapshot,uint32_t now) {
    if(!status_.recording)return;status_.seconds=(now-started_)/1000;
    if(snapshot.scanning || snapshot.generation==generation_)return;generation_=snapshot.generation;++status_.sweeps;
    std::string prefix=std::to_string(uint32_t(now-started_))+","+std::to_string(status_.sweeps)+",";
    if(snapshot.failed) {++status_.missed;write(prefix+"failed,0,,,,,\r\n");}
    else if(!snapshot.count) write(prefix+"empty,0,,,,,\r\n");
    else for(size_t i=0;i<snapshot.count;++i) {
        const auto& ap=snapshot.points[i];char mac[18];std::snprintf(mac,sizeof(mac),"%02x:%02x:%02x:%02x:%02x:%02x",unsigned(ap.id>>40)&255,unsigned(ap.id>>32)&255,unsigned(ap.id>>24)&255,unsigned(ap.id>>16)&255,unsigned(ap.id>>8)&255,unsigned(ap.id)&255);
        if(!write(prefix+"ok,"+std::to_string(snapshot.total)+","+csvQuote(ap.ssid)+","+mac+","+std::to_string(ap.channel)+","+std::to_string(ap.rssi)+","+csvQuote(ap.security)+"\r\n"))break;
        ++status_.rows;
    }
    if(file_)file_.flush();
}
void SurveyRecorder::stop() {if(file_) {file_.flush();file_.close();}status_.recording=false;if(!status_.failed)std::snprintf(status_.message,sizeof(status_.message),"Survey saved");}
} // namespace portalis
