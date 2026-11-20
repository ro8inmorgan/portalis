#include "ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace portalis {
namespace {
uint16_t BG = 0x0843, PANEL = 0x10E6, ACCENT = 0x6F99;
constexpr uint16_t FG = 0xF7BE, MUTED = 0x9CF3, GOLD = 0xFD6A;
constexpr int W = 240, H = 135;
void text(M5Canvas& c, const std::string& value, int x, int y, uint16_t color = FG, float size = 2) {
    c.setTextColor(color); c.setTextSize(size); c.setTextDatum(top_left); c.drawString(value.c_str(), x, y);
}
std::string shorten(const std::string& s, size_t width) { return s.size() <= width ? s : s.substr(0, width - 2) + ".."; }
void ticker(M5Canvas& c, const std::string& value, int x, int y, int width, uint32_t time, uint16_t color = FG, float size = 2) {
    c.setTextSize(size);
    const int overflow = c.textWidth(value.c_str()) - width;
    int offset = 0;
    if (overflow > 0) {
        const uint32_t travel = overflow * 30;
        const uint32_t phase = time % (travel + 2400);
        offset = phase < 1200 ? 0 : (phase < 1200 + travel ? (phase - 1200) / 30 : overflow);
    }
    c.setClipRect(x, y, width, static_cast<int>(8 * size));
    text(c, value, x - offset, y, color, size);
    c.clearClipRect();
}
void footer(M5Canvas& c, const std::string& value) {
    c.fillRect(0, 116, W, 19, PANEL); text(c, value, 6, 120, MUTED, 1.5f);
}
void title(M5Canvas& c, const char* label) { text(c, label, 6, 4, ACCENT); c.drawFastHLine(6, 23, 228, PANEL); }
void selectedRow(M5Canvas& c, const std::string& value, int row, bool active, uint32_t time) {
    const int y = 28 + row * 21;
    if (active) c.fillRoundRect(3, y - 2, 234, 20, 3, ACCENT);
    text(c, active ? ">" : " ", 6, y, active ? BG : MUTED);
    if (active) ticker(c, value, 22, y, 210, time, BG);
    else text(c, shorten(value, 17), 22, y, FG);
}
void wrapped(M5Canvas& c, const std::string& value, int y, int maxRows, uint16_t color = FG) {
    size_t offset = 0;
    for (int row = 0; row < maxRows && offset < value.size(); ++row) {
        while (offset < value.size() && value[offset] == ' ') ++offset;
        size_t length = std::min(size_t(19), value.size() - offset);
        if (offset + length < value.size()) {
            const size_t space = value.rfind(' ', offset + length);
            if (space != std::string::npos && space > offset) length = space - offset;
        }
        text(c, value.substr(offset, length), 6, y + row * 18, color);
        offset += length;
    }
}
void orbit(M5Canvas& c, int x, int y, uint32_t now, bool active) {
    c.drawCircle(x, y, 9, active ? ACCENT : MUTED);
    c.drawCircle(x, y, 4, active ? ACCENT : MUTED);
    if (!active) return;
    const float angle = now * 0.002f;
    c.fillCircle(x + static_cast<int>(9 * std::cos(angle)), y + static_cast<int>(9 * std::sin(angle)), 2, FG);
}
uint16_t blendColor(uint16_t base, uint16_t accent, float amount) {
    const auto channel = [amount](int a, int b) { return static_cast<uint16_t>(a + (b - a) * amount); };
    return (channel((base >> 11) & 31, (accent >> 11) & 31) << 11) |
           (channel((base >> 5) & 63, (accent >> 5) & 63) << 5) |
           channel(base & 31, accent & 31);
}
void counter(M5Canvas& c, int x, int width, const char* label, const std::string& value,
             uint16_t numberColor, uint16_t accent, float pulse, bool strong) {
    pulse = std::max(0.0f, std::min(1.0f, pulse));
    const uint16_t fill = blendColor(PANEL, accent, pulse * (strong ? 0.32f : 0.20f));
    c.fillRoundRect(x, 46, width, 46, 5, blendColor(PANEL, accent, pulse * 0.85f));
    const int inset = pulse > 0.5f ? 2 : 1;
    c.fillRoundRect(x + inset, 46 + inset, width - 2 * inset, 46 - 2 * inset, 4, fill);
    text(c, label, x + 6, 50, blendColor(MUTED, FG, pulse), 1.5f);
    text(c, value, x + 6, 66 - static_cast<int>(pulse * 2),
         blendColor(numberColor, FG, pulse), 3 + pulse * (strong ? 0.25f : 0.15f));
}
void battery(M5Canvas& c, int percent) {
    const bool known = percent >= 0;
    percent = std::max(0, std::min(100, percent));
    const uint16_t color = !known ? MUTED : percent <= 20 ? GOLD : ACCENT;
    c.fillRoundRect(172, 6, 18, 11, 2, color);
    c.fillRect(190, 9, 2, 5, color);
    c.fillRect(174, 8, 14, 7, BG);
    if (known) c.fillRect(174, 8, percent * 14 / 100, 7, color);
    text(c, known ? std::to_string(percent) + "%" : "--%", 196, 5, color, 1.5f);
}
std::string countLabel(uint32_t value) {
    if (value < 10000) return std::to_string(value);
    char out[16];
    if (value < 100000) std::snprintf(out, sizeof(out), "%.1fk", value / 1000.0);
    else if (value < 1000000) std::snprintf(out, sizeof(out), "%.0fk", value / 1000.0);
    else if (value < 10000000) std::snprintf(out, sizeof(out), "%.1fM", value / 1000000.0);
    else if (value < 1000000000) std::snprintf(out, sizeof(out), "%.0fM", value / 1000000.0);
    else std::snprintf(out, sizeof(out), "%.1fB", value / 1000000000.0);
    return out;
}
}
void drawUi(M5Canvas& c, const ViewModel& v) {
    UiSettings settings = v.settings; settings.validate();
    BG = kBackgrounds[settings.background].background; PANEL = kBackgrounds[settings.background].panel;
    ACCENT = kAccents[settings.accent].color;
    c.clearClipRect(); c.fillSprite(BG); c.setFont(&fonts::Font0); c.setTextWrap(false); c.setTextDatum(top_left);
    const uint32_t elapsed = v.now - v.screenSince;
    switch (v.screen) {
        case Screen::Splash: {
            const float reveal = std::min(1.0f, elapsed / 350.0f);
            orbit(c, 120, 28, v.now, true);
            const uint16_t logo = blendColor(BG, ACCENT, reveal);
            text(c, "PORTALIS", 24, 54, logo, 4);
            // A clipped highlight moves across the existing lettering.
            const int sweep = -48 + static_cast<int>(std::min(elapsed, kSplashDuration) * 312 / kSplashDuration);
            const int left = std::max(24, sweep), right = std::min(216, sweep + 48);
            if (right > left) {
                c.setClipRect(left, 54, right - left, 32);
                text(c, "PORTALIS", 24, 54, blendColor(logo, FG, reveal), 4);
                c.clearClipRect();
            }
            text(c, "LOCAL CONNECTION PORTAL", 21, 98, blendColor(BG, MUTED, reveal), 1.5f);
            c.fillRect(24, 120, 192, 2, PANEL);
            c.fillRect(24, 120, std::min(elapsed, kSplashDuration) * 192 / kSplashDuration, 2, ACCENT);
            break;
        }
        case Screen::Setup: {
            title(c, "PORTALIS"); text(c, v.sdReady ? "SD" : "NO SD", 115, 7, MUTED, 1.5f);
            const std::array<std::string, 5> labels{{"AP: " + std::string(v.ssid), "Page: " + std::string(v.pageName),
                "Start portal", "Submissions", "Settings"}};
            const int first = std::max(0, v.selected - 3);
            for (int row = 0; row < 4 && first + row < 5; ++row)
                selectedRow(c, labels[first + row], row, first + row == v.selected, elapsed);
            footer(c, ";/. Move ENT Select H Log"); break;
        }
        case Screen::Settings: {
            title(c, "SETTINGS");
            const std::array<std::string, kSettingsCount> labels{{
                "Accent: " + std::string(kAccents[settings.accent].name),
                "Background: " + std::string(kBackgrounds[settings.background].name),
                "LED: " + std::to_string(settings.ledBrightness) + "%",
                std::string("Menu sounds: ") + (settings.sounds ? "On" : "Off"),
                "Volume: " + std::to_string(settings.volume) + "%",
                std::string("Event sounds: ") + (settings.eventSounds ? "On" : "Off"),
                "Breathing: " + std::string(kLedColors[settings.ledColors[0]].name),
                "Connect: " + std::string(kLedColors[settings.ledColors[1]].name),
                "Confirm: " + std::string(kLedColors[settings.ledColors[2]].name),
                "Disconnect: " + std::string(kLedColors[settings.ledColors[3]].name),
                "LED mode: " + std::string(kLedAnimations[settings.ledAnimation])
            }};
            const int first = std::max(0, v.selected - 3);
            for (int row = 0; row < 4 && first + row < kSettingsCount; ++row)
                selectedRow(c, labels[first + row], row, first + row == v.selected, elapsed);
            footer(c, ";/. Move ,/ Set ESC Save"); break;
        }
        case Screen::Names: {
            title(c, "AP NAMES");
            const int count = v.config ? static_cast<int>(v.config->count) : 0;
            const int first = (v.selected / 4) * 4;
            for (int row = 0; row < 4 && first + row <= count; ++row) {
                const int index = first + row;
                const std::string label = index == 0 ? "Type a name..." : v.config->apNames[index - 1];
                selectedRow(c, label, row, v.selected == index, elapsed);
            }
            if (!count) {
                text(c, "Presets: /portalis.json", 6, 59, MUTED, 1.5f);
                text(c, "Add ap_names on SD", 6, 79, MUTED, 1.5f);
            }
            footer(c, "ENT Select R Reload Esc"); break;
        }
        case Screen::Name:
            title(c, "AP NAME");
            text(c, "Up to 32 characters", 6, 29, MUTED, 1.5f);
            c.fillRoundRect(3, 48, 234, 47, 4, PANEL);
            text(c, std::string(v.draft).substr(0, 19), 6, 53);
            text(c, std::string(v.draft).substr(std::min(size_t(19), strlen(v.draft))), 6, 73);
            if ((v.now / 500) % 2 == 0) {
                const size_t len = strlen(v.draft);
                c.fillRect(6 + (len % 19) * 12, len < 19 ? 68 : 88, 10, 2, ACCENT);
            }
            footer(c, "Enter Save  Esc Cancel"); break;
        case Screen::Templates: {
            title(c, "SD TEMPLATES");
            if (!v.browser) break;
            const auto& b = *v.browser;
            // Display three entries, leaving one full readable row for the path.
            ticker(c, b.directory, 6, 28, 228, elapsed, MUTED, 1.5f);
            const int first = (v.selected / 3) * 3;
            for (int row = 0; row < 3; ++row) {
                const int index = first + row;
                if (index >= static_cast<int>(b.count)) break;
                const auto& e = b.entries[index];
                const int y = 48 + row * 21;
                const bool chosen = index == v.selected;
                if (chosen) c.fillRoundRect(3, y - 2, 234, 20, 3, ACCENT);
                text(c, e.directory ? ">" : "*", 6, y, chosen ? BG : MUTED);
                if (chosen) ticker(c, e.label, 22, y, 210, elapsed, BG);
                else text(c, shorten(e.label, 17), 22, y);
            }
            if (!b.count) text(c, "No HTML templates", 6, 58, MUTED);
            footer(c, "ENT Open DEL Up R Scan"); break;
        }
        case Screen::Live:
            text(c, "PORTALIS", 6, 3, ACCENT); text(c, v.running ? "LIVE" : "STOPPED", 105, 5, v.running ? ACCENT : GOLD, 1.5f);
            ticker(c, v.ssid, 6, 25, 228, elapsed);
            counter(c, 4, 112, "CLIENTS", std::to_string(v.clients), FG,
                    v.clientsDecreased ? GOLD : ACCENT, v.clientsPulse, false);
            counter(c, 123, 113, "CONFIRMED", countLabel(v.count), ACCENT, ACCENT, v.confirmedPulse, true);
            text(c, "172.0.0.1", 6, 99, MUTED, 1.5f);
            ticker(c, elapsedLabel(v.seconds), 135, 99, 98, elapsed, FG, 1.5f);
            footer(c, v.running ? "S Forms C Config H Log X" : "S Forms C Config ENT Setup"); break;
        case Screen::Stop:
            title(c, "STOP PORTAL?");
            text(c, "Disconnect clients", 6, 33); text(c, "and turn off Wi-Fi.", 6, 53);
            text(c, "History stays saved.", 6, 83, MUTED, 1.5f);
            footer(c, "Enter Stop  Esc Back"); break;
        case Screen::History: {
            title(c, "HISTORY");
            text(c, !v.sdReady ? "RAM" : v.logHealthy ? "SD" : "ERR", 115, 7, v.logHealthy ? ACCENT : GOLD, 1.5f);
            if (!v.history || !v.history->size()) { text(c, "No confirmations", 6, 45); text(c, "Tap the page button.", 6, 71, MUTED, 1.5f); }
            else for (size_t row = 0; row < 4 && v.historyOffset + row < v.history->size(); ++row) {
                const auto event = v.history->newest(v.historyOffset + row);
                const std::string label = "S" + std::to_string(event.session) + " +" + elapsedLabel(event.seconds);
                ticker(c, label, 6, 29 + row * 21, 228, elapsed);
            }
            footer(c, ";/. Scroll  Esc Back"); break;
        }
        case Screen::Submissions: {
            title(c, "SUBMISSIONS");
            const size_t count = v.submissions ? v.submissions->size() : 0;
            if (!count) text(c, "No saved forms", 6, 45);
            const size_t first = (v.selected / 4) * 4;
            for (size_t row = 0; row < 4 && first + row < count; ++row) {
                const auto& entry = (*v.submissions)[count - 1 - first - row];
                const std::string label = "S" + std::to_string(entry.event.session) + " +" + elapsedLabel(entry.event.seconds) +
                                          " (" + std::to_string(entry.fields) + ")";
                selectedRow(c, label, row, first + row == static_cast<size_t>(v.selected), elapsed);
            }
            footer(c, "ENT Open R Refresh Esc"); break;
        }
        case Screen::Submission: {
            title(c, "SUBMISSION");
            if (v.submission) {
                ticker(c, "S" + std::to_string(v.submission->event.session) + " +" + elapsedLabel(v.submission->event.seconds), 6, 28, 228, elapsed, MUTED, 1.5f);
                for (size_t row = 0; row < 4 && v.submissionOffset + row < v.submission->lines.size(); ++row)
                    text(c, v.submission->lines[v.submissionOffset + row], 6, 45 + row * 18);
            }
            footer(c, ";/. Scroll  Esc Back"); break;
        }
        case Screen::Error:
            title(c, "NOTICE"); wrapped(c, v.error, 29, 4, GOLD); footer(c, "Enter / Esc Back"); break;
    }
    if (v.screen != Screen::Splash) battery(c, v.batteryPercent);
}
} // namespace portalis
