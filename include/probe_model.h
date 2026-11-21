#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <optional>

namespace portalis {
struct ProbeCandidate {
    std::array<uint8_t, 6> device{};
    char ssid[33] = {};
    uint8_t channel = 1;
    uint32_t capturedAt = 0;
};
struct ProbeConfig {
    std::array<std::string, 16> ssids{};
    size_t count = 0;
    uint32_t cooldownSeconds = 120;
    uint32_t timeoutSeconds = 120;
    static constexpr uint32_t minTimeoutSeconds = 30, maxTimeoutSeconds = 600;
    std::string noticeHtml; // Loaded once from the optional SD notice template.
    std::string noticePath;
    bool permits(const char* ssid) const {
        // Previous unrestricted behavior, retained for reference:
        return ssid && *ssid;
    }
};
// Parse only named probe requests, with strict bounds. Never interpret wildcard
// probes as permission to advertise a network. Called in the Wi-Fi task.
inline bool parseProbe(const uint8_t* bytes, size_t size, ProbeCandidate& out) {
    if (!bytes || size < 26 || (bytes[0] & 0xfc) != 0x40 || (bytes[0] & 3) != 0) return false;
    if (bytes[10] & 1) return false;
    bool nonzero = false;
    for (int i = 0; i < 6; ++i) nonzero |= bytes[10 + i] != 0;
    if (!nonzero) return false;
    for (size_t pos = 24; pos + 2 <= size;) {
        const uint8_t type = bytes[pos], length = bytes[pos + 1]; pos += 2;
        if (length > size - pos) return false;
        if (type == 0) {
            if (!length || length > 32) return false;
            for (uint8_t i = 0; i < length; ++i) if (bytes[pos + i] < 32 || bytes[pos + i] > 126) return false;
            out = {};
            std::memcpy(out.device.data(), bytes + 10, 6);
            std::memcpy(out.ssid, bytes + pos, length); out.ssid[length] = '\0';
            return true;
        }
        pos += length;
    }
    return false;
}
class ProbeSchedule {
    struct Cooldown { std::array<uint8_t,6> device{}; uint32_t endedAt = 0; bool used = false; };
    std::array<Cooldown, 32> backlog_{};
    ProbeCandidate target_{};
    bool active_ = false, joined_ = false;
    uint32_t started_ = 0, timeoutMs_ = 120000;
    int slot_ = -1;
public:
    void reset() { *this = {}; }
    bool begin(const ProbeConfig& scope, const ProbeCandidate& candidate, uint32_t now) {
        if (active_ || !scope.permits(candidate.ssid) ||
            scope.timeoutSeconds < ProbeConfig::minTimeoutSeconds ||
            scope.timeoutSeconds > ProbeConfig::maxTimeoutSeconds) return false;
        int free = -1;
        for (size_t i = 0; i < backlog_.size(); ++i) {
            const auto& item = backlog_[i];
            const bool cooling = item.used && static_cast<uint32_t>(now - item.endedAt) < scope.cooldownSeconds * 1000U;
            if (cooling && item.device == candidate.device) return false;
            if (!cooling && free < 0) free = i;
        }
        // Keep every unexpired entry; do not evict and immediately retry devices.
        if (free < 0) return false;
        timeoutMs_ = scope.timeoutSeconds * 1000U;
        slot_ = free; target_ = candidate; started_ = now; joined_ = false; active_ = true;
        return true;
    }
    bool observeConnection(uint32_t now) {
        if (!active_ || joined_) return false;
        joined_ = true;
        started_ = now;
        return true;
    }
    bool finished(uint32_t now) const {
        return active_ && static_cast<uint32_t>(now - started_) >= timeoutMs_;
    }
    uint32_t remaining(uint32_t now) const {
        if (!active_ || finished(now)) return 0;
        const uint32_t remaining = timeoutMs_ - static_cast<uint32_t>(now - started_);
        return (remaining + 999) / 1000;
    }
    void end(uint32_t now) {
        if (!active_) return;
        backlog_[slot_] = {target_.device, now, true}; active_ = false;
    }
    bool active() const { return active_; }
    bool joined() const { return joined_; }
    const ProbeCandidate& target() const { return target_; }
};
enum class ProbePhase { Idle, Listening, Waiting, Notice, Failed };
struct ProbeEvent { char ssid[33] = {}; uint32_t seconds = 0; bool connected = false; };
struct ProbeStatus {
    ProbePhase phase = ProbePhase::Idle;
    uint32_t attempts = 0, joins = 0, remaining = 0;
    uint8_t channel = 1;
    char ssid[33] = {};
    std::array<ProbeEvent, 32> events{};
    size_t eventCount = 0;
    void record(const char* name, uint32_t seconds, bool connected) {
        for (size_t i = std::min(eventCount, events.size() - 1); i > 0; --i) events[i] = events[i - 1];
        events[0] = {}; std::strncpy(events[0].ssid, name, 32);
        events[0].seconds = seconds; events[0].connected = connected;
        if (eventCount < events.size()) ++eventCount;
    }
};
inline constexpr const char kProbeNotice[] = "";
std::string renderProbeNotice(const std::string& html, const std::string& ticket = "");
// No override uses portalis.json; an empty path means no template selected.
bool loadProbeConfig(ProbeConfig& config, std::string& error, const std::optional<std::string>& selectedTemplate = std::nullopt);
} // namespace portalis
