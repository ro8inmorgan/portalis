#include "probe_model.h"
#include "model.h"
#include <ArduinoJson.h>
#include <SD.h>

namespace portalis {
// Use the portal's nonce-bound form handler; template scripts remain blocked.
std::string renderProbeNotice(const std::string& html, const std::string& ticket) {
    if (html.empty()) return kProbeNotice;
    PageTemplate page;
    page.html = html;
    return renderPage(page, ticket);
}
bool loadProbeConfig(ProbeConfig& config, std::string& error) {
    config = {}; // Fail closed: never retain a previous scope after a bad reload.

    JsonDocument doc;


    ProbeConfig candidate;
    if (!doc["cooldown_seconds"].isNull()) {
        if (!doc["cooldown_seconds"].is<uint32_t>()) { error = "Cooldown must be 60 to 86400 seconds."; return false; }
        candidate.cooldownSeconds = doc["cooldown_seconds"].as<uint32_t>();
        if (candidate.cooldownSeconds < 60 || candidate.cooldownSeconds > 86400) { error = "Cooldown must be 60 to 86400 seconds."; return false; }
    }
    for (JsonVariant value : doc["approved_ssids"].as<JsonArray>()) {
        if (!value.is<const char*>() || candidate.count == candidate.ssids.size()) { error = "Use up to 16 explicit SSID names."; return false; }
        const JsonString name = value.as<JsonString>();
        if (name.size() != strlen(name.c_str()) || !validSsid(name.c_str())) { error = "SSID names must be 1-32 printable characters."; return false; }
        if (!candidate.permits(name.c_str())) candidate.ssids[candidate.count++] = name.c_str();
    }
    if (!doc["notice_template"].isNull()) {
        if (!doc["notice_template"].is<const char*>()) { error = "notice_template must be an HTML path."; return false; }
        const JsonString value = doc["notice_template"].as<JsonString>();
        const std::string path = value.c_str();
        if (value.size() != path.size() || path.empty() || path.front() != '/' ||
            path.size() > 255 || path.find("..") != std::string::npos || !htmlTemplatePath(path)) {
            error = "Use an absolute .html/.htm notice path on SD."; return false;
        }
        File notice = SD.open(path.c_str());
        if (!notice || notice.isDirectory() || !notice.size() || notice.size() > kTemplateLimit) {
            error = "Notice template missing, empty, or larger than 32 KB."; return false;
        }
        candidate.noticeHtml.resize(notice.size());
        const size_t count = notice.readBytes(&candidate.noticeHtml[0], candidate.noticeHtml.size());
        notice.close();
        if (count != candidate.noticeHtml.size() || candidate.noticeHtml.find('\0') != std::string::npos) {
            error = "Could not read a valid HTML notice."; return false;
        }
    }
    config = candidate;
    error.clear();
    return true;
}
} // namespace portalis
