#include "workbench_model.h"
#include <ArduinoJson.h>
namespace portalis {
bool SshProfiles::forget(size_t index) {if(index>=count)return false;for(size_t i=index;i+1<count;++i)entries[i]=std::move(entries[i+1]);entries[--count]={};return true;}
bool SshProfiles::remember(const SshConfig& config) {
    if(!config.valid())return false;
    for(size_t i=0;i<count;++i)if(entries[i].host==config.host && entries[i].port==config.port && entries[i].user==config.user) {forget(i);break;}
    if(count==entries.size())forget(count-1);
    for(size_t i=count;i>0;--i)entries[i]=std::move(entries[i-1]);
    entries[0]=config;eraseSecret(entries[0].secret);++count;return true;
}
std::string SshProfiles::serialize() const {
    JsonDocument doc;auto array=doc.to<JsonArray>();
    for(size_t i=0;i<count;++i) {const auto& c=entries[i];auto row=array.add<JsonObject>();row["host"]=c.host;row["port"]=c.port;row["user"]=c.user;row["key"]=c.keyPath;row["auth"]=c.auth==SshAuth::PrivateKey?"key":"password";}
    std::string out;serializeJson(doc,out);return out;
}
bool SshProfiles::load(const std::string& json) {
    *this={};if(json.empty())return true;if(json.size()>4096)return false;
    JsonDocument doc;if(deserializeJson(doc,json,DeserializationOption::NestingLimit(2)) || !doc.is<JsonArray>() || doc.size()>8)return false;
    for(size_t i=doc.size();i>0;--i) {
        auto row=doc[i-1];if(!row["host"].is<std::string>() || !row["user"].is<std::string>() || !row["port"].is<unsigned>() || row["port"].as<unsigned>()>65535 || !row["key"].is<std::string>()) {*this={};return false;}
        std::string auth=row["auth"]|"";if(auth!="key" && auth!="password") {*this={};return false;}
        SshConfig c;c.host=row["host"].as<std::string>();c.user=row["user"].as<std::string>();c.port=row["port"].as<unsigned>();c.keyPath=row["key"].as<std::string>();c.auth=auth=="key"?SshAuth::PrivateKey:SshAuth::Password;
        if(!remember(c)) {*this={};return false;}
    }
    return true;
}
} // namespace portalis
