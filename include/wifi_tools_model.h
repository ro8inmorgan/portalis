#pragma once
#include "radar.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace portalis {
constexpr int kMissingSample = -32768;
template<size_t N> struct SampleHistory {
    std::array<int, N> values{};
    size_t count = 0;
    void add(int value) {
        if (count == N) { std::move(values.begin() + 1, values.end(), values.begin()); --count; }
        values[count++] = value;
    }
    int latest() const { return count ? values[count - 1] : kMissingSample; }
};
struct SignalHistory {
    RadarAccessPoint target;
    SampleHistory<60> samples;
    uint32_t generation = 0;
    void select(const RadarAccessPoint& ap, uint32_t currentGeneration) {
        *this = {}; target = ap; generation = currentGeneration;
    }
    void update(const RadarSnapshot& scan) {
        if (scan.scanning || scan.generation == generation) return;
        generation = scan.generation;
        if (!scan.failed && scan.trackedId == target.id && scan.trackedFound) {
            target = scan.trackedPoint; samples.add(target.rssi);
        } else samples.add(kMissingSample);
    }
    int average() const {
        int sum=0, n=0;
        for (size_t i=0;i<samples.count;++i) if (samples.values[i]!=kMissingSample) { sum+=samples.values[i]; ++n; }
        return n ? sum/n : kMissingSample;
    }
};
struct NetworkChange {
    char text[96] = {};
    uint32_t at = 0;
};
class NetworkChanges {
    RadarSnapshot previous_;
    bool baseline_ = false;
    uint32_t generation_ = 0;
    void add(const RadarAccessPoint& ap, const char* change, uint32_t now) {
        for (size_t i=std::min(count, events.size()-1); i>0; --i) events[i]=events[i-1];
        std::snprintf(events[0].text, sizeof(events[0].text), "%s: %s [%04X]", change, ap.ssid, static_cast<unsigned>(ap.id & 0xffff));
        events[0].at=now;
        count=std::min(count+1, events.size());
    }
public:
    std::array<NetworkChange, 32> events{};
    size_t count=0;
    bool limited=false;
    void reset() { *this={}; }
    bool hasBaseline() const { return baseline_; }
    void update(const RadarSnapshot& scan) {
        if (scan.scanning || scan.failed || scan.generation==generation_) return;
        generation_=scan.generation;
        limited=scan.total>scan.points.size();
        if (baseline_) {
            for (size_t i=0;i<scan.count;++i) {
                const auto& ap=scan.points[i]; const int old=previous_.find(ap.id);
                if (old<0) { add(ap,"Seen",scan.updatedAt); continue; }
                const auto& before=previous_.points[old];
                if (before.channel!=ap.channel) {
                    char change[24]; std::snprintf(change,sizeof(change),"Channel %u > %u",before.channel,ap.channel); add(ap,change,scan.updatedAt);
                }
                if (std::strcmp(before.security,ap.security)) add(ap,"Security changed",scan.updatedAt);
                if (std::strcmp(before.ssid,ap.ssid)) add(ap,"Name changed",scan.updatedAt);
            }
            // A truncated scan cannot prove that an older AP is absent.
            if (!limited) for (size_t i=0;i<previous_.count;++i)
                if (scan.find(previous_.points[i].id)<0) add(previous_.points[i],"Not seen",scan.updatedAt);
        }
        previous_=scan; baseline_=true;
    }
};
struct PingStatistics {
    SampleHistory<60> samples;
    uint32_t sent=0, received=0, minimum=0, maximum=0;
    uint64_t totalMs=0;
    void add(bool success, uint32_t ms) {
        ++sent;
        if (success) {
            if (!received) minimum=maximum=ms;
            else { minimum=std::min(minimum,ms); maximum=std::max(maximum,ms); }
            ++received; totalMs+=ms;
        }
        samples.add(success ? static_cast<int>(ms) : kMissingSample);
    }
    uint32_t lossPercent() const { return sent ? static_cast<uint64_t>(sent-received)*100/sent : 0; }
    uint32_t average() const { return received ? totalMs/received : 0; }
};
enum class NetworkPhase { Idle, Connecting, Checking, Ready, Failed, Stopping };
enum class CheckState { Pending, Pass, NoReply, Error };
struct NetworkStatus {
    NetworkPhase phase=NetworkPhase::Idle;
    bool pingMode=false, paused=false;
    char ssid[33]={}, address[16]={}, gateway[16]={}, dnsServer[16]={}, target[16]={}, message[80]={};
    CheckState dhcp=CheckState::Pending, gatewayCheck=CheckState::Pending, dns=CheckState::Pending, internet=CheckState::Pending;
    PingStatistics ping;
};
inline bool validWifiPassword(const std::string& password) {
    if (password.size()>=8 && password.size()<=63) return true;
    if (password.size()!=64) return false;
    return std::all_of(password.begin(), password.end(), [](unsigned char c) { return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'); });
}
inline bool validPingTarget(const std::string& value) {
    unsigned parts=0, digits=0, number=0;
    for (char c : value) {
        if (c=='.') { if (!digits || number>255 || ++parts>3) return false; digits=number=0; }
        else if (c>='0' && c<='9') { if (++digits>3) return false; number=number*10+c-'0'; }
        else return false;
    }
    return parts==3 && digits && number<=255 && value!="0.0.0.0" && value!="255.255.255.255";
}
} // namespace portalis
