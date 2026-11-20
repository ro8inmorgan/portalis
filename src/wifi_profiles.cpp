#include "wifi_profiles.h"
#include "wifi_tools_model.h"
#include <ArduinoJson.h>
namespace portalis {
int WifiProfiles::find(const std::string& ssid) const {
    for (size_t i=0;i<count;++i) if (entries[i].ssid==ssid) return i;
    return -1;
}
bool WifiProfiles::forget(const std::string& ssid) {
    const int index=find(ssid); if (index<0) return false;
    std::fill(entries[index].password.begin(),entries[index].password.end(),'\0');
    for (size_t i=index;i+1<count;++i) entries[i]=std::move(entries[i+1]);
    entries[--count]={}; return true;
}
bool WifiProfiles::remember(const std::string& ssid, const std::string& password) {
    if (ssid.empty() || ssid.size()>32 || ssid.find('\0')!=std::string::npos ||
        (!password.empty() && !validWifiPassword(password)) || password.find('\0')!=std::string::npos) return false;
    forget(ssid);
    if (count==entries.size()) { std::fill(entries[count-1].password.begin(),entries[count-1].password.end(),'\0'); --count; }
    for (size_t i=count;i>0;--i) entries[i]=std::move(entries[i-1]);
    entries[0]={ssid,password}; ++count; return true;
}
bool WifiProfiles::load(const std::string& json) {
    *this={};
    if (json.empty()) return true;
    if (json.size()>2048) return false;
    JsonDocument doc;
    if (deserializeJson(doc,json,DeserializationOption::NestingLimit(2)) || !doc.is<JsonArray>() || doc.size()>entries.size()) return false;
    WifiProfiles candidate;
    // Restore newest first without reversing the order on each boot.
    for (size_t i=doc.size();i>0;--i) {
        auto value=doc[i-1];
        if (!value["ssid"].is<std::string>() || !value["password"].is<std::string>() ||
            !candidate.remember(value["ssid"].as<std::string>(),value["password"].as<std::string>())) return false;
    }
    *this=std::move(candidate); return true;
}
std::string WifiProfiles::serialize() const {
    JsonDocument doc; auto items=doc.to<JsonArray>();
    for (size_t i=0;i<count;++i) { auto item=items.add<JsonObject>(); item["ssid"]=entries[i].ssid; item["password"]=entries[i].password; }
    std::string result; serializeJson(doc,result); return result;
}
} // namespace portalis
