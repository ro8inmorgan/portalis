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
// Shared feedback across all screens. Idle animation is independent of radio
// ownership; short tool/menu pulses and the original portal flashes overlay it.
enum class LedEvent : uint8_t { None, Move, Select, Activity, Success, Join, Leave, Failure, Confirm };
class AppLedFeedback {
    LedActivity idle_;
    LedEvent event_=LedEvent::None;
    uint32_t started_=0, clients_=0, confirmations_=0, session_=0;
    bool portalRunning_=false;
    static uint32_t duration(LedEvent event) {
        switch (event) {
            case LedEvent::Move: return 75;
            case LedEvent::Select: case LedEvent::Activity: return 120;
            case LedEvent::Success: return 160;
            case LedEvent::Failure: return 220;
            case LedEvent::Join: case LedEvent::Leave: case LedEvent::Confirm: return 600;
            default: return 0;
        }
    }
    static int priority(LedEvent event) {
        switch (event) {
            case LedEvent::Confirm:return 4;
            case LedEvent::Failure:case LedEvent::Leave:case LedEvent::Join:return 3;
            case LedEvent::Success:return 2;
            case LedEvent::None:return 0;
            default:return 1;
        }
    }
public:
    void signal(LedEvent event, uint32_t now) {
        if (event==LedEvent::None) return;
        if (static_cast<uint32_t>(now-started_)<duration(event_) && priority(event)<priority(event_)) return;
        event_=event;started_=now;
    }
    void observePortal(uint32_t clients, uint32_t confirmations, uint32_t session, bool running, uint32_t now) {
        if (running && portalRunning_ && session==session_) {
            if (confirmations>confirmations_) signal(LedEvent::Confirm,now);
            else if (clients!=clients_) signal(clients>clients_ ? LedEvent::Join : LedEvent::Leave,now);
        } else if (!running && portalRunning_) signal(LedEvent::Leave,now);
        clients_=clients;confirmations_=confirmations;session_=session;portalRunning_=running;
    }
    LedColor render(uint32_t now, uint8_t brightness, const LedPalette& palette, LedAnimation animation) {
        if (!brightness || animation==LedAnimation::Off) { event_=LedEvent::None;return {}; }
        const auto base=idle_.update(0,0,0,true,now,brightness,palette,animation);
        const uint32_t age=now-started_;
        if (event_==LedEvent::None || age>=duration(event_)) { event_=LedEvent::None;return base; }
        if ((event_==LedEvent::Join || event_==LedEvent::Leave || event_==LedEvent::Confirm) &&
            ! (age<150 || (age>=300 && age<450))) return {};
        const auto color=(event_==LedEvent::Confirm || event_==LedEvent::Success) ? palette.confirm :
            (event_==LedEvent::Leave || event_==LedEvent::Failure) ? palette.disconnect : palette.connect;
        return ledBrightness(color,brightness);
    }
};
} // namespace portalis
