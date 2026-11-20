#pragma once
#include <cstdint>
#include <cmath>
namespace portalis {
constexpr uint32_t kSplashDuration = 1800;
class CounterPulse {
    uint32_t previous_ = 0, started_ = 0;
    bool initialized_ = false, active_ = false, decreased_ = false;
public:
    void update(uint32_t value, uint32_t now, bool reset = false) {
        if (reset || !initialized_) { active_ = false; decreased_ = false; }
        else if (value != previous_) {
            active_ = true; started_ = now; decreased_ = value < previous_;
        } else if (active_ && static_cast<uint32_t>(now - started_) >= 700) active_ = false;
        previous_ = value; initialized_ = true;
    }
    float strength(uint32_t now) const {
        const uint32_t age = now - started_;
        if (!active_ || age >= 700) return 0;
        const float amount = age < 80 ? age / 80.0f : (700 - age) / 620.0f;
        return amount * amount * (3.0f - 2.0f * amount);
    }
    bool decreased() const { return decreased_; }
};

struct SoundNote { uint16_t frequency = 0, duration = 0; };
class EventSounds {
    uint32_t clients_ = 0, confirmations_ = 0, session_ = 0, noteAt_ = 0;
    bool running_ = false;
    uint8_t melody_ = 0, nextNote_ = 0;
public:
    SoundNote update(uint32_t clients, uint32_t confirmations, uint32_t session, bool running, bool enabled, uint32_t now) {
        const bool reset = !running || !running_ || session != session_ || !enabled;
        if (reset) melody_ = 0;
        else if (confirmations > confirmations_) { melody_ = 2; nextNote_ = 0; noteAt_ = now; }
        else if (clients != clients_ && melody_ != 2) { melody_ = clients > clients_ ? 1 : 3; nextNote_ = 0; noteAt_ = now; }
        clients_ = clients; confirmations_ = confirmations; session_ = session; running_ = running;
        if (!melody_) return {};
        if (nextNote_ && static_cast<uint32_t>(now - noteAt_) < 120) return {};
        const uint16_t join[] = {660, 880};
        const uint16_t confirm[] = {880, 1175, 1568};
        const uint16_t leave[] = {880, 550};
        const uint8_t length = melody_ == 2 ? 3 : 2;
        if (nextNote_ >= length) { melody_ = 0; return {}; }
        const SoundNote note{melody_ == 2 ? confirm[nextNote_] : melody_ == 3 ? leave[nextNote_] : join[nextNote_], 85};
        ++nextNote_; noteAt_ = now;
        return note;
    }
};

struct LedColor { uint8_t red = 0, green = 0, blue = 0; };
enum class LedAnimation : uint8_t { Breathing, Steady, Blink, Rainbow, Off };
struct LedPalette {
    LedColor breathing{0, 85, 255}, connect{0, 112, 255}, confirm{0, 255, 48}, disconnect{192, 64, 0};
};
inline LedColor ledIntensity(LedColor color, uint8_t level) {
    return {static_cast<uint8_t>(color.red * level / 255),
            static_cast<uint8_t>(color.green * level / 255),
            static_cast<uint8_t>(color.blue * level / 255)};
}
inline LedColor ledRainbow(uint32_t now) {
    const uint32_t phase = now % 6000, segment = phase / 1000;
    const uint8_t rising = (phase % 1000) * 255 / 1000, falling = 255 - rising;
    switch (segment) {
        case 0: return {255, rising, 0};
        case 1: return {falling, 255, 0};
        case 2: return {0, 255, rising};
        case 3: return {0, falling, 255};
        case 4: return {rising, 0, 255};
        default: return {255, 0, falling};
    }
}

inline LedColor ledBrightness(LedColor color, uint8_t percent) {
    if (percent > 100) percent = 100;
    return {static_cast<uint8_t>(color.red * percent / 100),
            static_cast<uint8_t>(color.green * percent / 100),
            static_cast<uint8_t>(color.blue * percent / 100)};
}
class LedActivity {
    uint32_t clients_ = 0, confirmations_ = 0, session_ = 0, started_ = 0;
    bool running_ = false;
    enum class Flash { None, Join, Leave, Confirm } flash_ = Flash::None;
public:
    LedColor update(uint32_t clients, uint32_t confirmations, uint32_t session, bool running, uint32_t now, uint8_t brightnessPercent = 80, const LedPalette& palette = LedPalette{}, LedAnimation animation = LedAnimation::Breathing) {
        const bool reset = !running || !running_ || session != session_;
        if (reset) flash_ = Flash::None;
        else {
            if (flash_ != Flash::None && static_cast<uint32_t>(now - started_) >= 600) flash_ = Flash::None;
            // Confirmations have priority over simultaneous station changes.
            if (confirmations > confirmations_) { flash_ = Flash::Confirm; started_ = now; }
            else if (clients != clients_ && flash_ != Flash::Confirm) {
                flash_ = clients > clients_ ? Flash::Join : Flash::Leave; started_ = now;
            }
        }
        clients_ = clients; confirmations_ = confirmations; session_ = session; running_ = running;
        if (!running || animation == LedAnimation::Off) return {};
        if (flash_ != Flash::None) {
            const uint32_t age = now - started_;
            if (age < 150 || (age >= 300 && age < 450)) {
                if (flash_ == Flash::Confirm) return ledBrightness(palette.confirm, brightnessPercent);
                if (flash_ == Flash::Join) return ledBrightness(palette.connect, brightnessPercent);
                return ledBrightness(palette.disconnect, brightnessPercent);
            }
            return {};
        }
        const float wave = (1.0f - std::cos((now % 3200) * (6.2831853f / 3200))) * 0.5f;
        uint8_t intensity = 100;
        LedColor idle = palette.breathing;
        if (animation == LedAnimation::Breathing) intensity = static_cast<uint8_t>(8 + 92 * wave * wave);
        else if (animation == LedAnimation::Blink && (now % 1400) >= 700) intensity = 0;
        else if (animation == LedAnimation::Rainbow) idle = ledRainbow(now);
        return ledBrightness(ledIntensity(idle, intensity), brightnessPercent);
    }
};
} // namespace portalis
