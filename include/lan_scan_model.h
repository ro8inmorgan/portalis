#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace portalis {
inline bool parseScanIp(const std::string& text, uint32_t& out) {
    uint32_t value=0, octet=0; unsigned parts=0, digits=0;
    for (char c : text) {
        if (c=='.') {
            if (!digits || ++parts>3) return false;
            value=(value<<8)|octet; octet=digits=0;
        } else if (c>='0' && c<='9') {
            if (++digits>3 || (octet=octet*10+c-'0')>255) return false;
        } else return false;
    }
    if (parts!=3 || !digits) return false;
    value=(value<<8)|octet;
    if ((value>>24)==0 || (value>>24)==127 || (value>>24)>=224) return false;
    out=value; return true;
}
inline std::string scanIpText(uint32_t ip) {
    char text[16]; std::snprintf(text,sizeof(text),"%u.%u.%u.%u",
        unsigned(ip>>24),unsigned((ip>>16)&255),unsigned((ip>>8)&255),unsigned(ip&255));
    return text;
}
inline bool scanRange(const std::string& from, const std::string& to, uint32_t& first, uint32_t& last) {
    return parseScanIp(from,first) && parseScanIp(to,last) && last>=first && last-first<256;
}
inline bool scanPorts(const std::string& text, uint16_t& first, uint16_t& last) {
    const auto dash=text.find('-');
    auto number=[](const std::string& part, uint16_t& out) {
        if (part.empty() || part.size()>5) return false;
        uint32_t value=0;
        for (char c : part) { if (c<'0'||c>'9') return false; value=value*10+c-'0'; }
        if (!value || value>65535) return false;
        out=value; return true;
    };
    if (!number(text.substr(0,dash),first)) return false;
    if (dash==std::string::npos) last=first;
    else if (!number(text.substr(dash+1),last)) return false;
    return last>=first;
}
enum class ScanPhase { Idle, Running, Complete, Cancelled, Failed };
struct ScanProgress {
    ScanPhase phase=ScanPhase::Idle;
    uint32_t checked=0, total=0, found=0;
    bool running() const { return phase==ScanPhase::Running; }
};
struct FoundHost { uint32_t ip=0, ms=0; };
struct RangeScanStatus : ScanProgress {
    std::array<FoundHost,256> hosts{};
    size_t count=0;
    uint32_t current=0;
};
struct PortScanStatus : ScanProgress {
    std::array<uint16_t,256> ports{};
    size_t count=0;
    uint32_t host=0;
};
} // namespace portalis
