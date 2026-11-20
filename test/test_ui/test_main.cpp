#include <unity.h>
#include "ui.h"
#include "../../src/ui.cpp"
using namespace portalis;
void setUp() {} void tearDown() {}
void test_all_screens_fit_large_text() {
    M5Canvas c; ViewModel v; History history; BrowserPage browser;
    v.history = &history; v.browser = &browser;
    v.ssid = "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456"; v.draft = v.ssid;
    v.error = "Check text lengths and #RRGGBB colors.";
    v.pageName = "A long custom page name";
    browser.directory = "/A rather long directory name/examples";
    browser.count = 12;
    for (auto& e : browser.entries) e = {"a-very-long-template-name.json", "/a.json", false};
    for (int i = 0; i < 96; ++i) history.add({UINT32_MAX, UINT32_MAX});
    for (auto screen : {Screen::Splash, Screen::Setup, Screen::Settings, Screen::Names, Screen::Name, Screen::Templates, Screen::Live, Screen::Stop, Screen::History, Screen::Submissions, Screen::Submission, Screen::Error}) {
        for (uint32_t now : {0U, 500U, 5000U, 9000U}) {
            v.screen = screen; v.now = now; v.count = UINT32_MAX; v.seconds = UINT32_MAX;
            v.running = true; drawUi(c, v);
            for (const auto& t : c.texts) TEST_ASSERT_TRUE(t.scale >= 1.5f);
        }
    }
    TEST_ASSERT_EQUAL(0, c.violations);
}
void test_empty_and_stopped_views_fit() {
    M5Canvas c; ViewModel v; BrowserPage browser; History history;
    v.browser = &browser; v.history = &history;
    for (auto screen : {Screen::Names, Screen::Name, Screen::Templates, Screen::History, Screen::Live}) { v.screen = screen; drawUi(c, v); }
    TEST_ASSERT_EQUAL(0, c.violations);
}
void test_counter_magnitudes_fit() {
    M5Canvas c; ViewModel v; v.screen = Screen::Live;
    for (uint32_t n : {0U, 9999U, 10000U, 99999U, 999999U, 9999999U, 999999999U, UINT32_MAX}) { v.count = n; drawUi(c, v); }
    TEST_ASSERT_EQUAL(0, c.violations);
}
void test_ap_presets_all_pages_fit() {
    M5Canvas c; ViewModel v; PortalConfig config;
    config.count = config.apNames.size();
    for (auto& name : config.apNames) name = "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456";
    v.screen = Screen::Names; v.config = &config;
    for (size_t index = 0; index <= config.count; ++index) {
        v.selected = index;
        for (uint32_t now : {0U, 5000U, 9000U}) { v.now = now; drawUi(c, v); }
    }
    TEST_ASSERT_EQUAL(0, c.violations);
}
void test_submission_views_fit() {
    M5Canvas canvas; ViewModel view;
    std::vector<SubmissionEntry> entries(10, {0, {UINT32_MAX, UINT32_MAX}, 64});
    SubmissionView submission; submission.event = {UINT32_MAX, UINT32_MAX};
    submission.lines.assign(20, std::string(19, 'W'));
    view.submissions = &entries; view.submission = &submission;
    for (int index = 0; index < 10; ++index) {
        view.screen = Screen::Submissions; view.selected = index;
        view.now = 5000; drawUi(canvas, view);
    }
    view.screen = Screen::Submission;
    for (size_t offset = 0; offset < 20; ++offset) { view.submissionOffset = offset; drawUi(canvas, view); }
    TEST_ASSERT_EQUAL(0, canvas.violations);
}
void test_counter_pulse_changes_expire_and_reset() {
    CounterPulse pulse;
    pulse.update(0, 0); TEST_ASSERT_EQUAL_FLOAT(0, pulse.strength(0));
    pulse.update(1, 100); TEST_ASSERT_FALSE(pulse.decreased());
    TEST_ASSERT_EQUAL_FLOAT(1, pulse.strength(180));
    pulse.update(1, 800); TEST_ASSERT_EQUAL_FLOAT(0, pulse.strength(800));
    pulse.update(0, 900); TEST_ASSERT_TRUE(pulse.decreased());
    TEST_ASSERT_EQUAL_FLOAT(1, pulse.strength(980));
    pulse.update(8, 1000, true); TEST_ASSERT_EQUAL_FLOAT(0, pulse.strength(1080));
    pulse.update(9, UINT32_MAX - 50); TEST_ASSERT_TRUE(pulse.strength(29) > 0.99f);
    pulse.update(9, 700); TEST_ASSERT_EQUAL_FLOAT(0, pulse.strength(700));
}
void test_led_breath_events_priority_and_stop() {
    LedActivity led;
    auto color = led.update(0, 0, 1, true, 0);
    TEST_ASSERT_EQUAL(6, color.blue);
    color = led.update(0, 0, 1, true, 1600); TEST_ASSERT_EQUAL(80, color.blue);
    color = led.update(1, 0, 1, true, 1700); TEST_ASSERT_EQUAL(204, color.blue);
    color = led.update(1, 0, 1, true, 1900); TEST_ASSERT_EQUAL(0, color.blue);
    color = led.update(1, 0, 1, true, 2000); TEST_ASSERT_EQUAL(204, color.blue);
    color = led.update(1, 1, 1, true, 2050); TEST_ASSERT_EQUAL(204, color.green);
    color = led.update(2, 1, 1, true, 2100); TEST_ASSERT_EQUAL(204, color.green);
    color = led.update(2, 1, 1, true, 2700); TEST_ASSERT_LESS_OR_EQUAL(80, color.blue);
    color = led.update(1, 1, 1, true, 2800); TEST_ASSERT_EQUAL(153, color.red);
    color = led.update(0, 1, 1, false, 2850);
    TEST_ASSERT_EQUAL(0, color.red + color.green + color.blue);
    color = led.update(0, 0, 2, true, 2900); TEST_ASSERT_LESS_OR_EQUAL(80, color.blue);
}
void test_animated_frames_fit_screen() {
    M5Canvas canvas; ViewModel view;
    view.screen = Screen::Splash;
    for (uint32_t time = 0; time <= kSplashDuration + 100; time += 25) { view.now = time; drawUi(canvas, view); }
    view.screen = Screen::Live; view.clients = 4; view.count = UINT32_MAX;
    for (float pulse : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        view.clientsPulse = pulse; view.confirmedPulse = pulse; drawUi(canvas, view);
        view.clientsDecreased = true; drawUi(canvas, view);
    }
    TEST_ASSERT_EQUAL(0, canvas.violations);
}
void test_settings_controls_bounds_and_color_preview() {
    UiSettings settings;
    settings.adjust(0, -1); TEST_ASSERT_EQUAL(5, settings.accent);
    settings.adjust(0, 1); TEST_ASSERT_EQUAL(0, settings.accent);
    settings.adjust(1, -1); TEST_ASSERT_EQUAL(2, settings.background);
    settings.adjust(1, 1); TEST_ASSERT_EQUAL(0, settings.background);
    for (int i = 0; i < 20; ++i) { settings.adjust(2, 1); settings.adjust(4, 1); }
    TEST_ASSERT_EQUAL(100, settings.ledBrightness); TEST_ASSERT_EQUAL(255, settings.speakerVolume());
    for (int i = 0; i < 20; ++i) { settings.adjust(2, -1); settings.adjust(4, -1); }
    TEST_ASSERT_EQUAL(0, settings.ledBrightness); TEST_ASSERT_EQUAL(0, settings.speakerVolume());
    settings.adjust(3, 1); TEST_ASSERT_FALSE(settings.sounds);
    settings.adjust(5, 1); TEST_ASSERT_FALSE(settings.eventSounds);
    settings.adjust(5, -1); TEST_ASSERT_TRUE(settings.eventSounds);
    settings.accent = 255; settings.background = 255; settings.ledBrightness = 255; settings.volume = 255;
    settings.validate(); TEST_ASSERT_EQUAL(0, settings.accent); TEST_ASSERT_EQUAL(0, settings.background);
    TEST_ASSERT_EQUAL(80, settings.ledBrightness); TEST_ASSERT_EQUAL(30, settings.volume);
    M5Canvas canvas; ViewModel view; view.screen = Screen::Settings;
    for (size_t accent = 0; accent < kAccents.size(); ++accent)
        for (size_t background = 0; background < kBackgrounds.size(); ++background)
            for (int row = 0; row < kSettingsCount; ++row) {
                view.settings.accent = accent; view.settings.background = background; view.selected = row;
                drawUi(canvas, view);
                TEST_ASSERT_EQUAL(kAccents[accent].color, canvas.texts[0].color);
            }
    view.screen = Screen::Setup; view.selected = 4; drawUi(canvas, view);
    bool found = false;
    for (const auto& text : canvas.texts) if (text.value == "Settings") found = true;
    TEST_ASSERT_TRUE(found); TEST_ASSERT_EQUAL(0, canvas.violations);
}
void test_led_brightness_zero_and_full() {
    LedActivity led;
    auto color = led.update(0, 0, 1, true, 1600, 100); TEST_ASSERT_EQUAL(100, color.blue);
    color = led.update(1, 0, 1, true, 1700, 100); TEST_ASSERT_EQUAL(255, color.blue);
    color = led.update(1, 1, 1, true, 1750, 0); TEST_ASSERT_EQUAL(0, color.red + color.green + color.blue);
    color = led.update(1, 1, 1, true, 1800, 100); TEST_ASSERT_EQUAL(255, color.green);
    color = ledBrightness({255, 128, 64}, 50);
    TEST_ASSERT_EQUAL(127, color.red); TEST_ASSERT_EQUAL(64, color.green); TEST_ASSERT_EQUAL(32, color.blue);
}
void test_event_melodies_priority_mute_and_reset() {
    EventSounds sounds;
    TEST_ASSERT_EQUAL(0, sounds.update(0, 0, 1, true, true, 0).frequency);
    TEST_ASSERT_EQUAL(660, sounds.update(1, 0, 1, true, true, 10).frequency);
    TEST_ASSERT_EQUAL(0, sounds.update(1, 0, 1, true, true, 50).frequency);
    TEST_ASSERT_EQUAL(880, sounds.update(1, 0, 1, true, true, 130).frequency);
    TEST_ASSERT_EQUAL(0, sounds.update(1, 0, 1, true, true, 250).frequency);
    TEST_ASSERT_EQUAL(880, sounds.update(2, 1, 1, true, true, 300).frequency);
    TEST_ASSERT_EQUAL(1175, sounds.update(3, 1, 1, true, true, 420).frequency);
    TEST_ASSERT_EQUAL(1568, sounds.update(3, 1, 1, true, true, 540).frequency);
    TEST_ASSERT_EQUAL(0, sounds.update(3, 1, 1, true, true, 660).frequency);
    TEST_ASSERT_EQUAL(880, sounds.update(2, 1, 1, true, true, 700).frequency);
    TEST_ASSERT_EQUAL(550, sounds.update(2, 1, 1, true, true, 820).frequency);
    TEST_ASSERT_EQUAL(0, sounds.update(3, 2, 1, true, false, 850).frequency);
    TEST_ASSERT_EQUAL(0, sounds.update(3, 2, 1, true, true, 900).frequency);
    TEST_ASSERT_EQUAL(880, sounds.update(3, 3, 1, true, true, 1000).frequency);
    TEST_ASSERT_EQUAL(0, sounds.update(0, 3, 1, false, true, 1010).frequency);
    TEST_ASSERT_EQUAL(0, sounds.update(0, 0, 2, true, true, 1100).frequency);
    TEST_ASSERT_EQUAL(660, sounds.update(1, 0, 2, true, true, UINT32_MAX - 50).frequency);
    TEST_ASSERT_EQUAL(880, sounds.update(1, 0, 2, true, true, 70).frequency);
}
void test_led_color_settings_and_animation_modes() {
    UiSettings settings;
    for (int row = 6; row <= 9; ++row) {
        settings.ledColors[row - 6] = 0;
        settings.adjust(row, -1); TEST_ASSERT_EQUAL(7, settings.ledColors[row - 6]);
        settings.adjust(row, 1); TEST_ASSERT_EQUAL(0, settings.ledColors[row - 6]);
    }
    settings.adjust(10, -1); TEST_ASSERT_EQUAL(4, settings.ledAnimation);
    settings.adjust(10, 1); TEST_ASSERT_EQUAL(0, settings.ledAnimation);
    settings.ledColors.fill(255); settings.ledAnimation = 255; settings.validate();
    for (size_t i = 0; i < 4; ++i) TEST_ASSERT_EQUAL(i, settings.ledColors[i]);
    TEST_ASSERT_EQUAL(0, settings.ledAnimation);
    const LedPalette colors{{255,0,0}, {255,255,255}, {0,0,255}, {0,255,0}};
    LedActivity led;
    TEST_ASSERT_EQUAL(80, led.update(0,0,1,true,1600,80,colors).red);
    auto color = led.update(1,0,1,true,1700,80,colors);
    TEST_ASSERT_EQUAL(204, color.red); TEST_ASSERT_EQUAL(204, color.green);
    TEST_ASSERT_EQUAL(204, led.update(1,1,1,true,1800,80,colors).blue);
    TEST_ASSERT_EQUAL(204, led.update(0,1,1,true,2500,80,colors).green);
    LedActivity modes;
    TEST_ASSERT_EQUAL(80, modes.update(0,0,1,true,0,80,{},LedAnimation::Steady).blue);
    TEST_ASSERT_EQUAL(0, modes.update(0,0,1,true,800,80,{},LedAnimation::Blink).blue);
    TEST_ASSERT_EQUAL(80, modes.update(0,0,1,true,1400,80,{},LedAnimation::Blink).blue);
    TEST_ASSERT_EQUAL(80, modes.update(0,0,1,true,0,80,{},LedAnimation::Rainbow).red);
    TEST_ASSERT_EQUAL(80, modes.update(0,0,1,true,2000,80,{},LedAnimation::Rainbow).green);
    color = modes.update(1,1,1,true,2100,80,{},LedAnimation::Off);
    TEST_ASSERT_EQUAL(0, color.red + color.green + color.blue);
}
void test_battery_indicator_fits_all_headers() {
    M5Canvas canvas; ViewModel view;
    for (int level : {-1, 0, 1, 20, 21, 99, 100, 150}) {
        view.batteryPercent = level;
        for (auto screen : {Screen::Setup, Screen::Settings, Screen::Names, Screen::Name,
                            Screen::Templates, Screen::Live, Screen::Stop, Screen::History,
                            Screen::Submissions, Screen::Submission, Screen::Error}) {
            view.screen = screen; drawUi(canvas, view);
            const auto& label = canvas.texts.back();
            TEST_ASSERT_EQUAL_STRING(level < 0 ? "--%" : (std::to_string(std::min(100, level)) + "%").c_str(), label.value.c_str());
            TEST_ASSERT_EQUAL(level < 0 ? MUTED : level <= 20 ? GOLD : ACCENT, label.color);
        }
    }
    TEST_ASSERT_EQUAL(0, canvas.violations);
}
int main() { UNITY_BEGIN(); RUN_TEST(test_led_color_settings_and_animation_modes); RUN_TEST(test_battery_indicator_fits_all_headers); RUN_TEST(test_event_melodies_priority_mute_and_reset); RUN_TEST(test_settings_controls_bounds_and_color_preview); RUN_TEST(test_led_brightness_zero_and_full); RUN_TEST(test_counter_pulse_changes_expire_and_reset); RUN_TEST(test_led_breath_events_priority_and_stop); RUN_TEST(test_animated_frames_fit_screen); RUN_TEST(test_submission_views_fit); RUN_TEST(test_ap_presets_all_pages_fit); RUN_TEST(test_all_screens_fit_large_text); RUN_TEST(test_empty_and_stopped_views_fit); RUN_TEST(test_counter_magnitudes_fit); return UNITY_END(); }
