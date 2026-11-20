#include "storage.h"
#include "portal_defaults.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <algorithm>

namespace portalis {
static constexpr const char* kLog = "/portalis/events.csv";
static constexpr size_t kLogLimit = 256 * 1024;

bool Storage::begin() {
    if (mounted_) SD.end();
    SPI.begin(40, 39, 14, 12);
    mounted_ = SD.begin(12, SPI, 10000000) && SD.cardType() != CARD_NONE;
    logHealthy_ = mounted_;
    if (mounted_) { std::string error; ensurePortalConfig(error); }
    return mounted_;
}

bool Storage::loadConfig(PortalConfig& result, std::string& error) {
    result = PortalConfig{};
    error.clear();
    // A config is optional; manual naming always remains available.
    if (!mounted_) return true;
    if (!ensurePortalConfig(error)) return false;
    File file = SD.open("/portalis.json");
    if (!file || file.isDirectory() || file.size() > 4096) {
        error = "Cannot read portalis.json (max 4 KB)."; return false;
    }
    JsonDocument doc;
    const auto parseError = deserializeJson(doc, file, DeserializationOption::NestingLimit(2));
    file.close();
    if (parseError || !doc.is<JsonObject>() || !doc["ap_names"].is<JsonArray>()) {
        error = "portalis.json needs an ap_names array."; return false;
    }
    const auto names = doc["ap_names"].as<JsonArrayConst>();
    PortalConfig candidate;
    if (names.size() > candidate.apNames.size()) {
        error = "Use at most 32 AP names in portalis.json."; return false;
    }
    for (JsonVariantConst value : names) {
        if (!value.is<std::string>() || !validSsid(value.as<std::string>())) {
            error = "AP names: 1-32 ASCII characters, no edge spaces."; return false;
        }
        candidate.apNames[candidate.count++] = value.as<std::string>();
    }
    result = std::move(candidate);
    return true;
}

bool Storage::browse(std::string path, size_t offset, BrowserPage& result, bool includeBuiltIn) {
    result = BrowserPage{};
    result.directory = path;
    result.offset = offset;
    // Sort the complete directory before pagination, not each page separately.
    std::vector<BrowserEntry> entries;
    if (includeBuiltIn && path == "/") entries.push_back({"Aurora (built-in)", "", false});
    if (mounted_) {
        File dir = SD.open(path.c_str());
        if (!dir || !dir.isDirectory()) { result.total = entries.size(); return false; }
        File file = dir.openNextFile();
        while (file) {
            std::string name = file.name();
            const size_t slash = name.find_last_of('/');
            if (slash != std::string::npos) name = name.substr(slash + 1);
            const std::string full = (path == "/" ? "/" : path + "/") + name;
            const bool isDir = file.isDirectory();
            const bool html = htmlTemplatePath(name);
            if (!name.empty() && name[0] != '.' && full.size() <= 180 && (isDir || html)) {
                entries.push_back({name, full, isDir});
            }
            file.close();
            file = dir.openNextFile();
        }
        dir.close();
    }
    std::sort(entries.begin(), entries.end(), [](const BrowserEntry& a, const BrowserEntry& b) {
        if (a.path.empty() != b.path.empty()) return a.path.empty();
        if (a.directory != b.directory) return a.directory;
        const size_t length = std::min(a.label.size(), b.label.size());
        for (size_t i = 0; i < length; ++i) {
            const int left = std::tolower(static_cast<unsigned char>(a.label[i]));
            const int right = std::tolower(static_cast<unsigned char>(b.label[i]));
            if (left != right) return left < right;
        }
        if (a.label.size() != b.label.size()) return a.label.size() < b.label.size();
        return a.label < b.label; // deterministic order for case-only differences
    });
    result.total = entries.size();
    for (size_t i = offset; i < entries.size() && result.count < result.entries.size(); ++i)
        result.entries[result.count++] = std::move(entries[i]);
    return true;
}

bool Storage::loadTemplate(const std::string& path, PageTemplate& result, std::string& error) {
    if (path.empty()) { result = PageTemplate{}; return true; }
    if (!htmlTemplatePath(path)) { error = "Choose an .html or .htm template."; return false; }
    if (!mounted_) { error = "Insert an SD card and press R in the picker."; return false; }
    File file = SD.open(path.c_str());
    if (!file || file.isDirectory() || file.size() == 0 || file.size() > kTemplateLimit) {
        error = "Cannot open HTML template (1 byte to 32 KB)."; return false;
    }
    PageTemplate temp;
    temp.name = path.substr(path.find_last_of('/') + 1).substr(0, 24);
    temp.html.resize(file.size());
    const size_t count = file.readBytes(&temp.html[0], temp.html.size());
    file.close();
    if (count != temp.html.size() || !validTemplate(temp)) {
        error = "Invalid or incomplete HTML template."; return false;
    }
    result = std::move(temp);
    return true;
}

bool Storage::appendSubmission(Event event, const std::string& fields) {
    if (!mounted_) { logHealthy_ = false; return false; }
    JsonDocument doc;
    if (deserializeJson(doc, fields) || !doc.is<JsonArray>()) { logHealthy_ = false; return false; }
    JsonDocument record;
    record["session"] = event.session;
    record["elapsed_seconds"] = event.seconds;
    record["fields"] = doc.as<JsonArrayConst>();
    if (record.overflowed()) { logHealthy_ = false; return false; }
    std::string line;
    serializeJson(record, line);
    line += '\n';
    if (!SD.exists("/portalis") && !SD.mkdir("/portalis")) { logHealthy_ = false; return false; }
    File file = SD.open("/portalis/submissions.jsonl", FILE_APPEND);
    if (!file || file.size() + line.size() > kLogLimit) { logHealthy_ = false; return false; }
    logHealthy_ = file.write(reinterpret_cast<const uint8_t*>(line.data()), line.size()) == line.size();
    file.flush(); file.close();
    return logHealthy_;
}

namespace {
bool submissionRecord(const std::string& line, JsonDocument& doc) {
    if (deserializeJson(doc, line, DeserializationOption::NestingLimit(4)) ||
        !doc["session"].is<uint32_t>() || doc["session"].as<uint32_t>() == 0 ||
        !doc["elapsed_seconds"].is<uint32_t>() || !doc["fields"].is<JsonArray>() ||
        doc["fields"].size() > kFieldLimit) return false;
    for (JsonVariantConst field : doc["fields"].as<JsonArrayConst>())
        if (!field["name"].is<std::string>() || !field["value"].is<std::string>()) return false;
    return true;
}
// Font0 is ASCII. Keep the stored UTF-8 untouched, substituting unsupported
// glyphs only in the small device viewer. Preserve newlines and wrap long values.
void addDisplayLines(std::vector<std::string>& lines, const std::string& value) {
    std::string row;
    for (unsigned char ch : value) {
        if (ch == '\r') continue;
        if (ch == '\n') { lines.push_back(row); row.clear(); continue; }
        if (ch >= 0x80 && ch < 0xc0) continue;
        row += ch == '\t' ? ' ' : (ch >= 32 && ch < 127 ? static_cast<char>(ch) : '?');
        if (row.size() == 19) { lines.push_back(row); row.clear(); }
    }
    if (!row.empty() || value.empty()) lines.push_back(row);
}
}

bool Storage::listSubmissions(std::vector<SubmissionEntry>& entries, std::string& error) {
    entries.clear(); error.clear();
    if (!mounted_) { error = "Insert SD to view saved submissions."; return false; }
    if (!SD.exists("/portalis/submissions.jsonl")) return true;
    File file = SD.open("/portalis/submissions.jsonl");
    if (!file || file.isDirectory() || file.size() > kLogLimit) {
        error = "Cannot read submission log (max 256 KB)."; return false;
    }
    std::string line;
    uint32_t offset = 0, start = 0;
    bool overflow = false;
    while (file.available()) {
        const char ch = file.read(); ++offset;
        if (ch == '\n') {
            JsonDocument doc;
            if (!overflow && submissionRecord(line, doc))
                entries.push_back({start, {doc["session"].as<uint32_t>(), doc["elapsed_seconds"].as<uint32_t>()}, doc["fields"].size()});
            line.clear(); overflow = false; start = offset;
        } else if (line.size() < kSubmissionLimit + 256) line += ch;
        else overflow = true;
    }
    file.close();
    // Incomplete trailing records are ignored; successful writes always end in LF.
    return true;
}

bool Storage::readSubmission(const SubmissionEntry& entry, SubmissionView& result, std::string& error) {
    result = SubmissionView{}; error.clear();
    if (!mounted_) { error = "Insert SD to view saved submissions."; return false; }
    File file = SD.open("/portalis/submissions.jsonl");
    if (!file || file.isDirectory() || !file.seek(entry.offset)) {
        error = "Cannot open submission. Refresh the list."; return false;
    }
    std::string line; bool complete = false;
    while (file.available() && line.size() <= kSubmissionLimit + 256) {
        const char ch = file.read();
        if (ch == '\n') { complete = true; break; }
        line += ch;
    }
    file.close();
    JsonDocument doc;
    if (!complete || !submissionRecord(line, doc) ||
        doc["session"].as<uint32_t>() != entry.event.session ||
        doc["elapsed_seconds"].as<uint32_t>() != entry.event.seconds) {
        error = "Submission changed or unreadable. Refresh list."; return false;
    }
    result.event = entry.event;
    for (JsonVariantConst field : doc["fields"].as<JsonArrayConst>()) {
        addDisplayLines(result.lines, field["name"].as<std::string>() + ":");
        if (field["filename"].is<std::string>()) {
            addDisplayLines(result.lines, "File: " + field["filename"].as<std::string>());
            addDisplayLines(result.lines, "Type: " + field["type"].as<std::string>());
            addDisplayLines(result.lines, "Contents on SD");
        } else addDisplayLines(result.lines, field["value"].as<std::string>());
        result.lines.push_back("");
    }
    if (result.lines.empty()) result.lines.push_back("No form fields.");
    return true;
}

void Storage::restore(History& history) {
    if (!mounted_) return;
    File file = SD.open(kLog);
    if (!file) return;
    std::string line;
    bool overflow = false;
    // Only our bounded event log is read, never arbitrary submission files.
    size_t bytes = 0;
    while (file.available() && bytes++ <= kLogLimit) {
        const char c = file.read();
        if (c == '\n') {
            Event event;
            if (!overflow && parseEvent(line, event)) history.add(event);
            line.clear(); overflow = false;
        } else if (c != '\r') {
            if (line.size() < 32) line += c; else overflow = true;
        }
    }
    file.close();
}

bool Storage::append(Event event) {
    if (!mounted_) { logHealthy_ = false; return false; }
    if (!SD.exists("/portalis") && !SD.mkdir("/portalis")) { logHealthy_ = false; return false; }
    File file = SD.open(kLog, FILE_APPEND);
    if (!file || file.size() >= kLogLimit) { logHealthy_ = false; return false; }
    if (file.size() == 0 && file.print("session,elapsed_seconds\n") != 24) {
        logHealthy_ = false; return false;
    }
    char line[40];
    const int length = snprintf(line, sizeof(line), "%lu,%lu\n", static_cast<unsigned long>(event.session),
                                static_cast<unsigned long>(event.seconds));
    if (file.size() + length > kLogLimit) { logHealthy_ = false; return false; }
    logHealthy_ = file.write(reinterpret_cast<const uint8_t*>(line), length) == static_cast<size_t>(length);
    file.flush(); file.close();
    return logHealthy_;
}
} // namespace portalis
