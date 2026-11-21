#pragma once
#include <algorithm>
#include <cstdint>

namespace portalis {
enum HomeItem { PortalHome, RadarHome, KarmaHome, ChannelsHome, SignalHome,
                DiagnosticsHome, PingHome, ChangesHome, RangeScanHome, PortScanHome, SshHome, SettingsHome };
class CarouselMotion {
    float from_ = 0, target_ = 0;
    uint32_t started_ = 0;
public:
    static constexpr int count = SettingsHome + 1;
    void reset(int index, uint32_t now) { from_ = target_ = index; started_ = now; }
    float position(uint32_t now) const {
        const float t = std::min(1.0f, static_cast<uint32_t>(now - started_) / 240.0f);
        const float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        return from_ + (target_ - from_) * ease;
    }
    void move(int direction, uint32_t now) {
        const float current = position(now);
        // Normalize together so rapid input and wrap-around preserve continuity.
        const int cycles = static_cast<int>(target_) / count;
        from_ = current - cycles * count;
        target_ = target_ - cycles * count + direction;
        started_ = now;
    }
};
} // namespace portalis
