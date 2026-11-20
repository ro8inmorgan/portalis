#include "probe_model.h"
#include "model.h"
#include "portal_defaults.h"
#include <ArduinoJson.h>
#include <SD.h>

namespace portalis {
// Use the portal's nonce-bound form handler; template scripts remain blocked.
std::string renderProbeNotice(const std::string& html, const std::string& ticket) {
    // Previous blank fallback: if (html.empty()) return kProbeNotice;
    PageTemplate page;
    page.html = html;
    page.title = "Wifi Login";
    page.message = "";
    return renderPage(page, ticket);
}
} // namespace portalis

// Qualified definition must match the declaration in probe_model.h; otherwise
// a signature change inside the namespace can silently create another overload.
bool portalis::loadProbeConfig(ProbeConfig& config, std::string& error, const std::optional<std::string>& selectedTemplate) {
    config = {}; // Fail closed: never retain a previous scope after a bad reload.
    if (!ensurePortalConfig(error)) return false;
    File file = SD.open("/portalis.json");
    if (!file || file.isDirectory() || !file.size() || file.size() > 4096) {
        error = "Cannot read /portalis.json (1 byte to 4 KB)."; return false;
    }
    JsonDocument doc;
    const auto parseError = deserializeJson(doc, file, DeserializationOption::NestingLimit(2));
    file.close();

    if (parseError || !doc.is<JsonObject>()) {
        error = "Invalid /portalis.json. Expected a JSON object."; return false;
    }
    ProbeConfig candidate;
    if (!doc["cooldown_seconds"].isNull()) {
        if (!doc["cooldown_seconds"].is<uint32_t>()) { error = "Cooldown must be 60 to 86400 seconds."; return false; }
        candidate.cooldownSeconds = doc["cooldown_seconds"].as<uint32_t>();
        if (candidate.cooldownSeconds < 60 || candidate.cooldownSeconds > 86400) { error = "Cooldown must be 60 to 86400 seconds."; return false; }
    }

    std::string path = selectedTemplate.value_or("");
    if (!selectedTemplate && !doc["notice_template"].isNull()) {
        if (!doc["notice_template"].is<const char*>()) { error = "notice_template must be an HTML path."; return false; }
        const JsonString value = doc["notice_template"].as<JsonString>();
        path = value.c_str();
        if (value.size() != path.size()) { error = "Invalid HTML notice path."; return false; }
    }
    if (!path.empty()) {
        if (path.find('\0') != std::string::npos || path.front() != '/' ||
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
        candidate.noticePath = path;
    }
    config = candidate;
    error.clear();
    return true;
}
