#pragma once
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace portalis {
constexpr size_t kTemplateLimit = 32 * 1024;
constexpr size_t kSubmissionLimit = 16 * 1024;
constexpr size_t kFieldLimit = 64;
struct Submission { std::string ticket, fields; };
bool parseSubmission(const std::string& body, Submission& result);

inline bool htmlTemplatePath(std::string path) {
    for (char& c : path) c = std::tolower(static_cast<unsigned char>(c));
    return (path.size() > 5 && path.compare(path.size() - 5, 5, ".html") == 0) ||
           (path.size() > 4 && path.compare(path.size() - 4, 4, ".htm") == 0);
}
struct PageTemplate {
    std::string html;
    std::string name = "Aurora";
    std::string title = "You're connected.";
    std::string message = "Welcome to this local connection demo. Tap below to confirm that the page opened on your device.";
    std::string accent = "#69F0C7";
    std::string background = "#101C2B";
};
inline bool validSsid(const std::string& s) {
    if (s.empty() || s.size() > 32 || s.front() == ' ' || s.back() == ' ') return false;
    for (unsigned char c : s) if (c < 32 || c > 126) return false;
    return true;
}
inline bool validColor(const std::string& s) {
    if (s.size() != 7 || s[0] != '#') return false;
    for (size_t i = 1; i < s.size(); ++i) if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}
inline bool validTemplate(const PageTemplate& t) {
    if (!t.html.empty()) return t.html.size() <= kTemplateLimit && t.html.find('\0') == std::string::npos;
    if (t.name.empty() || t.name.size() > 24 || t.title.empty() || t.title.size() > 64 ||
        t.message.empty() || t.message.size() > 240 || !validColor(t.accent) || !validColor(t.background)) return false;
    for (unsigned char c : t.name) if (c < 32 || c > 126) return false;
    return true;
}
inline std::string escapeHtml(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}
inline bool validTicket(const std::string& t) {
    if (t.size() != 16) return false;
    for (unsigned char c : t) if (!std::isxdigit(c)) return false;
    return true;
}

// One-use page tickets; field names are independent of the ticket envelope.
class TicketGate {
    struct Ticket { std::string value; uint32_t issued = 0; bool used = true; };
    std::array<Ticket, 32> tickets_{};
    size_t next_ = 0;
public:
    void clear() { for (auto& t : tickets_) { t.value.clear(); t.used = true; } next_ = 0; }
    void issue(const std::string& ticket, uint32_t now) {
        tickets_[next_] = {ticket, now, false};
        next_ = (next_ + 1) % tickets_.size();
    }
    bool valid(const std::string& ticket, uint32_t now) const {
        if (!validTicket(ticket)) return false;
        for (const auto& t : tickets_)
            if (!t.used && t.value == ticket && static_cast<uint32_t>(now - t.issued) <= 300000) return true;
        return false;
    }
    bool consume(const std::string& ticket, uint32_t now) {
        if (!valid(ticket, now)) return false;
        for (auto& t : tickets_) if (t.value == ticket) t.used = true;
        return true;
    }
};

struct Event { uint32_t session = 0; uint32_t seconds = 0; };
class History {
    std::array<Event, 96> events_{};
    size_t head_ = 0, size_ = 0;
public:
    void add(Event e) { events_[head_] = e; head_ = (head_ + 1) % events_.size(); if (size_ < events_.size()) ++size_; }
    size_t size() const { return size_; }
    Event newest(size_t index) const {
        if (index >= size_) return {};
        return events_[(head_ + events_.size() - 1 - index) % events_.size()];
    }
};
inline bool parseEvent(const std::string& line, Event& out) {
    // Strict CSV: two unsigned decimal numbers only. Bounded lengths and overflow.
    uint64_t a = 0, b = 0; bool comma = false, digitA = false, digitB = false;
    for (char c : line) {
        if (c == ',' && !comma && digitA) { comma = true; continue; }
        if (c < '0' || c > '9') return false;
        uint64_t& n = comma ? b : a;
        n = n * 10 + static_cast<unsigned>(c - '0');
        if (n > UINT32_MAX) return false;
        if (comma) digitB = true; else digitA = true;
    }
    if (!comma || !digitA || !digitB || a == 0) return false;
    out = {static_cast<uint32_t>(a), static_cast<uint32_t>(b)};
    return true;
}
inline std::string elapsedLabel(uint32_t seconds) {
    char out[24];
    std::snprintf(out, sizeof(out), "%lu:%02lu", static_cast<unsigned long>(seconds / 60), static_cast<unsigned long>(seconds % 60));
    return out;
}
std::string renderPage(const PageTemplate& page, const std::string& ticket, bool done = false);
} // namespace portalis
