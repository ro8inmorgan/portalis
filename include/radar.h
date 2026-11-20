#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace portalis {
struct RadarAccessPoint {
    uint64_t id = 0; // BSSID, kept only in RAM for stable dots and selection.
    char ssid[33] = {};
    char rawSsid[33] = {};
    bool connectable = false;
    int16_t rssi = -100;
    uint8_t channel = 0;
    char security[8] = {};
};
struct RadarSnapshot {
    std::array<RadarAccessPoint, 32> points{};
    size_t count = 0;
    uint16_t total = 0;
    bool scanning = false, failed = false;
    std::array<uint16_t, 15> channelCounts{};
    std::array<int16_t, 15> channelStrongest{};
    std::array<uint32_t, 15> channelOverlap{};
    uint64_t trackedId = 0;
    RadarAccessPoint trackedPoint;
    bool trackedFound = false;
    void tally(const RadarAccessPoint& ap) {
        if (ap.channel<1 || ap.channel>14) return;
        auto& count=channelCounts[ap.channel];
        if (!count || ap.rssi>channelStrongest[ap.channel]) channelStrongest[ap.channel]=ap.rssi;
        ++count;
        for (int channel=1; channel<=14; ++channel) {
            const int center=channel==14 ? 2484 : 2407+channel*5;
            const int source=ap.channel==14 ? 2484 : 2407+ap.channel*5;
            const int overlap=std::max(0,22-std::abs(center-source));
            channelOverlap[channel]+=overlap*std::max(1,100+static_cast<int>(ap.rssi));
        }
    }
    uint32_t updatedAt = 0, generation = 0;
    int find(uint64_t id) const {
        for (size_t i = 0; i < count; ++i) if (points[i].id == id) return static_cast<int>(i);
        return -1;
    }
    void add(const RadarAccessPoint& point) {
        const int existing = find(point.id);
        if (existing >= 0) {
            if (point.rssi <= points[existing].rssi) return;
            points[existing] = point;
        } else if (count < points.size()) points[count++] = point;
        else if (point.rssi > points[count - 1].rssi) points[count - 1] = point;
        else return;
        std::sort(points.begin(), points.begin() + count, [](const auto& a, const auto& b) {
            return a.rssi != b.rssi ? a.rssi > b.rssi : a.id < b.id;
        });
    }
};
inline uint64_t radarId(const uint8_t* mac) {
    uint64_t id = 0;
    if (mac) for (int i = 0; i < 6; ++i) id = (id << 8) | mac[i];
    return id;
}
inline int radarRadius(int rssi) {
    // Relative signal bands only, not a propagation model or distance in metres.
    const int clamped = std::max(-95, std::min(-30, rssi));
    return 8 + (-30 - clamped) * 27 / 65;
}
inline float radarAngle(uint64_t id) {
    uint32_t hash = 2166136261U;
    for (int i = 0; i < 6; ++i) { hash ^= (id >> (i * 8)) & 255; hash *= 16777619U; }
    return (hash % 3600) * (6.28318530718f / 3600.0f);
}
inline int radarSelection(const RadarSnapshot& snapshot, uint64_t previousId) {
    const int index = snapshot.find(previousId);
    return index < 0 ? 0 : index;
}
} // namespace portalis
