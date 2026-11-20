#pragma once
#include <array>
#include <cstdint>
#include "effects.h"

namespace portalis {
struct AccentOption { const char* name; uint16_t color; };
struct BackgroundOption { const char* name; uint16_t background, panel; };
inline constexpr std::array<AccentOption, 6> kAccents{{
    {"Mint", 0x6F99}, {"Ocean", 0x6DFF}, {"Violet", 0xC4FF},
    {"Amber", 0xFD6A}, {"Rose", 0xFCB9}, {"White", 0xF7BE}
}};
inline constexpr std::array<BackgroundOption, 3> kBackgrounds{{
    {"Navy", 0x0843, 0x10E6}, {"Charcoal", 0x1082, 0x2124}, {"Plum", 0x1804, 0x3088}
}};
struct LedOption { const char* name; LedColor color; };
inline constexpr std::array<LedOption, 8> kLedColors{{
    {"Blue", {0,85,255}}, {"Cyan", {0,112,255}}, {"Green", {0,255,48}}, {"Amber", {192,64,0}},
    {"Violet", {160,0,255}}, {"Red", {255,0,0}}, {"White", {255,255,255}}, {"Mint", {64,255,160}}
}};
inline constexpr std::array<const char*, 5> kLedAnimations{{"Breathing", "Steady", "Blink", "Rainbow", "Off"}};
inline constexpr int kSettingsCount = 11;
struct UiSettings {
    uint8_t accent = 0, background = 0, ledBrightness = 80, volume = 30;
    std::array<uint8_t, 4> ledColors{{0, 1, 2, 3}};
    uint8_t ledAnimation = 0;
    bool sounds = true, eventSounds = true;
    void validate() {
        if (accent >= kAccents.size()) accent = 0;
        if (background >= kBackgrounds.size()) background = 0;
        if (ledBrightness > 100) ledBrightness = 80;
        if (volume > 100) volume = 30;
        for (size_t i = 0; i < ledColors.size(); ++i) if (ledColors[i] >= kLedColors.size()) ledColors[i] = i;
        if (ledAnimation >= kLedAnimations.size()) ledAnimation = 0;
    }
    void adjust(int row, int direction) {
        validate();
        if (row == 0) accent = (accent + kAccents.size() + direction) % kAccents.size();
        else if (row == 1) background = (background + kBackgrounds.size() + direction) % kBackgrounds.size();
        else if (row == 3) sounds = !sounds;
        else if (row == 5) eventSounds = !eventSounds;
        else if (row >= 6 && row <= 9) ledColors[row - 6] = (ledColors[row - 6] + kLedColors.size() + direction) % kLedColors.size();
        else if (row == 10) ledAnimation = (ledAnimation + kLedAnimations.size() + direction) % kLedAnimations.size();
        else if (row == 2 || row == 4) {
            auto& value = row == 2 ? ledBrightness : volume;
            const int next = static_cast<int>(value) + direction * 10;
            value = next < 0 ? 0 : next > 100 ? 100 : next;
        }
    }
    LedPalette ledPalette() const {
        return {kLedColors[ledColors[0] % kLedColors.size()].color, kLedColors[ledColors[1] % kLedColors.size()].color,
                kLedColors[ledColors[2] % kLedColors.size()].color, kLedColors[ledColors[3] % kLedColors.size()].color};
    }
    uint8_t speakerVolume() const { return static_cast<uint8_t>((volume > 100 ? 100 : volume) * 255 / 100); }
};
} // namespace portalis
