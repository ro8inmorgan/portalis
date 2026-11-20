#pragma once
#include "model.h"
#include <array>
#include <vector>

namespace portalis {
struct PortalConfig {
    std::array<std::string, 32> apNames;
    size_t count = 0;
};
struct BrowserEntry { std::string label; std::string path; bool directory = false; };
struct BrowserPage {
    std::array<BrowserEntry, 12> entries;
    size_t count = 0, total = 0, offset = 0;
    std::string directory = "/";
};
struct SubmissionEntry { uint32_t offset = 0; Event event; size_t fields = 0; };
struct SubmissionView { Event event; std::vector<std::string> lines; };
class Storage {
    bool mounted_ = false;
    bool logHealthy_ = true;
public:
    bool begin();
    bool ready() const { return mounted_; }
    bool logHealthy() const { return logHealthy_; }
    bool loadConfig(PortalConfig& result, std::string& error);
    bool browse(std::string path, size_t offset, BrowserPage& result, bool includeBuiltIn = true);
    bool loadTemplate(const std::string& path, PageTemplate& result, std::string& error);
    bool listSubmissions(std::vector<SubmissionEntry>& entries, std::string& error);
    bool readSubmission(const SubmissionEntry& entry, SubmissionView& result, std::string& error);
    void restore(History& history);
    bool append(Event event);
    bool appendSubmission(Event event, const std::string& fields);
};
} // namespace portalis
