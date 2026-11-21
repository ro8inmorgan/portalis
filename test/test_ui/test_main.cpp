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
    TEST_ASSERT_FALSE(settings.screenAlwaysOn);
    settings.adjust(11,1);TEST_ASSERT_TRUE(settings.screenAlwaysOn);
    settings.adjust(11,-1);TEST_ASSERT_FALSE(settings.screenAlwaysOn);
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
    view.screen = Screen::Home; view.selected = SettingsHome; view.carouselPosition = SettingsHome; drawUi(canvas, view);
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
void test_radar_and_list_fit_dense_and_empty_scans() {
    M5Canvas canvas; ViewModel view; RadarSnapshot snapshot;
    view.radar = &snapshot;
    for (int i = 0; i < 32; ++i) {
        RadarAccessPoint ap; ap.id = i + 1; ap.rssi = -30 - i * 3; ap.channel = 13;
        std::strcpy(ap.ssid, "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456"); std::strcpy(ap.security, "WPA2/3");
        snapshot.add(ap);
    }
    snapshot.total = 300;
    for (auto screen : {Screen::Radar, Screen::ScanList}) {
        view.screen = screen;
        for (size_t selected = 0; selected < snapshot.count; ++selected) {
            view.selected = selected;
            for (uint32_t time = 0; time < 4000; time += 100) { view.now = time; drawUi(canvas, view); }
        }
        snapshot.scanning = true; drawUi(canvas, view);
    }
    snapshot = {};
    for (auto screen : {Screen::Radar, Screen::ScanList}) {
        view.screen = screen; drawUi(canvas, view);
        snapshot.scanning = true; drawUi(canvas, view);
        snapshot.scanning = false; snapshot.failed = true; drawUi(canvas, view);
    }
    TEST_ASSERT_EQUAL(0, canvas.violations);
}
void test_carousel_animation_wrap_and_layout() {
    CarouselMotion motion; M5Canvas canvas; ViewModel view; view.screen = Screen::Home;
    motion.reset(0, 100); motion.move(-1, 100);
    TEST_ASSERT_EQUAL_FLOAT(0, motion.position(100));
    TEST_ASSERT_EQUAL_FLOAT(-1, motion.position(340));
    motion.move(1, 340); TEST_ASSERT_EQUAL_FLOAT(0, motion.position(580));
    motion.reset(2, UINT32_MAX - 100); motion.move(1, UINT32_MAX - 100);
    TEST_ASSERT_EQUAL_FLOAT(3, motion.position(139));
    for (int selected = 0; selected < CarouselMotion::count; ++selected) {
        view.selected = selected;
        for (int frame = -50; frame < CarouselMotion::count*50+50; ++frame) {
            view.carouselPosition = frame / 50.0f; view.now = frame * 50;
            drawUi(canvas, view);
        }
    }
    TEST_ASSERT_EQUAL(0, canvas.violations);
}
void test_probe_screens_fit() {
    M5Canvas canvas; ViewModel view; ProbeConfig config; ProbeStatus status;
    view.probeConfig = &config; view.probeStatus = &status; config.timeoutSeconds = 600;
    config.count = 16;
    for (auto& name : config.ssids) name = "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456";
    strcpy(status.ssid, config.ssids[0].c_str());
    for (int i = 0; i < 32; ++i) status.record(status.ssid, UINT32_MAX, i % 2);
    status.attempts = status.joins = UINT32_MAX; status.channel = 14; status.remaining = 120;
    for (auto screen : {Screen::Probe, Screen::ProbeScope, Screen::ProbeHistory}) {
        view.screen = screen; config.count = 16; status.eventCount = 32;
        for (int selected = 0; selected < 32; ++selected) {
            view.selected = selected;
            for (auto phase : {ProbePhase::Idle, ProbePhase::Listening, ProbePhase::Waiting, ProbePhase::Notice, ProbePhase::Failed}) {
                status.phase = phase; view.probeRunning = phase != ProbePhase::Idle && phase != ProbePhase::Failed;
                for (uint32_t now : {0U, 5000U}) { view.now = now; drawUi(canvas, view); }
            }
        }
        config.count = 0; status.eventCount = 0; drawUi(canvas, view);
    }
    TEST_ASSERT_EQUAL(0, canvas.violations);
}
void test_wifi_tools_screens_fit() {
    M5Canvas canvas;ViewModel v;RadarSnapshot scan;SignalHistory signal;NetworkChanges changes;NetworkStatus net;
    v.radar=&scan;v.signal=&signal;v.changes=&changes;v.network=&net;
    strcpy(signal.target.ssid,"ABCDEFGHIJKLMNOPQRSTUVWXYZ123456");
    for (int i=0;i<60;++i) { signal.samples.add(i%4 ? -30-i : kMissingSample);net.ping.add(i%4,1000); }
    strcpy(net.address,"192.168.100.200");strcpy(net.target,"192.168.100.200");
    v.draft="192.168.100.200";v.networkName=signal.target.ssid;v.passwordLength=64;
    for (int i=0;i<32;++i) { RadarAccessPoint ap;ap.id=i+1;strcpy(ap.ssid,signal.target.ssid);ap.channel=1+i%14;scan.add(ap);scan.tally(ap); }
    scan.generation=1;changes.update(scan);scan.generation=2;scan.points[0].channel=14;changes.update(scan);
    for (auto screen : {Screen::Channels,Screen::ToolNetworks,Screen::Signal,Screen::Changes,Screen::NetworkPassword,Screen::Diagnostics,Screen::Ping,Screen::PingTarget,Screen::NetworkStopping}) {
        v.screen=screen;
        for (int selected=0;selected<32;++selected) {
            v.selected=selected;
            for (auto phase : {NetworkPhase::Connecting,NetworkPhase::Checking,NetworkPhase::Ready,NetworkPhase::Failed}) {
                net.phase=phase;v.channelOverlapView=selected%2;drawUi(canvas,v);
            }
        }
    }
    TEST_ASSERT_EQUAL(0,canvas.violations);
}
void test_app_led_idle_everywhere_and_event_priority() {
    AppLedFeedback led;
    LedPalette palette{{0,0,255},{0,255,0},{255,0,0},{255,255,0}};
    // No active portal or radio is needed for the idle effect.
    for (uint32_t now : {0U,1000U,5000U,UINT32_MAX}) {
        auto c=led.render(now,100,palette,LedAnimation::Steady);
        TEST_ASSERT_EQUAL(100,c.blue);
    }
    led.signal(LedEvent::Move,100);
    auto c=led.render(100,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(255,c.green);
    c=led.render(175,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(100,c.blue);
    led.signal(LedEvent::Confirm,200);led.signal(LedEvent::Move,250);
    c=led.render(250,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(255,c.red);TEST_ASSERT_EQUAL(0,c.green);
    c=led.render(400,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(0,c.red+c.green+c.blue);
    c=led.render(500,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(255,c.red);
    c=led.render(800,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(100,c.blue);
    led.signal(LedEvent::Failure,900);led.signal(LedEvent::Success,950);
    c=led.render(950,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(255,c.red);TEST_ASSERT_EQUAL(255,c.green);
}
void test_app_led_off_zero_wrap_and_portal_resume() {
    AppLedFeedback led;
    LedPalette palette{{0,0,255},{0,255,0},{255,0,0},{255,255,0}};
    led.signal(LedEvent::Confirm,0);
    auto c=led.render(0,100,palette,LedAnimation::Off);TEST_ASSERT_EQUAL(0,c.red+c.green+c.blue);
    c=led.render(1,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(100,c.blue);
    led.signal(LedEvent::Success,2);
    c=led.render(2,0,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(0,c.red+c.green+c.blue);
    led.signal(LedEvent::Move,UINT32_MAX-20);
    c=led.render(25,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(255,c.green);
    c=led.render(80,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(100,c.blue);
    led.observePortal(0,0,1,true,100);
    led.observePortal(1,0,1,true,200);
    c=led.render(200,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(255,c.green);
    led.signal(LedEvent::Success,220);
    c=led.render(220,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(255,c.green);
    led.observePortal(1,1,1,true,250);
    c=led.render(250,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(255,c.red);
    led.observePortal(0,0,1,false,1000);
    c=led.render(1000,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(255,c.red);TEST_ASSERT_EQUAL(255,c.green);
    c=led.render(1600,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(100,c.blue);
    led.observePortal(8,20,2,true,1700);
    c=led.render(1700,100,palette,LedAnimation::Steady);TEST_ASSERT_EQUAL(100,c.blue);
}
void test_lan_scan_screens_and_main_menu_icons_fit() {
    M5Canvas canvas; ViewModel view; NetworkStatus network;
    RangeScanStatus hosts; PortScanStatus ports;
    view.network=&network; view.rangeScan=&hosts; view.portScan=&ports;
    view.rangeStart="192.168.100.100"; view.rangeEnd="192.168.100.254";
    view.scanHost="192.168.100.100"; view.scanPortRange="1-65535";
    view.scanInputLabel="TCP ports (1-65535)"; view.draft="65534-65535";
    for (auto screen:{Screen::RangeSetup,Screen::PortSetup,Screen::ScanInput}) {
        view.screen=screen;
        for (auto phase:{NetworkPhase::Connecting,NetworkPhase::Ready,NetworkPhase::Failed}) {
            network.phase=phase;
            for (int row=0;row<3;++row) { view.selected=row; drawUi(canvas,view); }
        }
    }
    for (auto phase:{ScanPhase::Running,ScanPhase::Complete,ScanPhase::Cancelled,ScanPhase::Failed}) {
        hosts.phase=ports.phase=phase;
        hosts.total=hosts.checked=256; ports.total=ports.checked=65535;
        for (size_t count:{size_t(0),size_t(1),size_t(256)}) {
            hosts.count=ports.count=count; hosts.found=count; ports.found=count ? 65535 : 0;
            for (size_t i=0;i<count;++i) { hosts.hosts[i]={0xc0a86400U+uint32_t(i),350}; ports.ports[i]=65535-i; }
            for (size_t row=0;row<std::max(size_t(1),count);++row) {
                view.selected=row;
                for (uint32_t now:{0U,5000U}) {
                    view.now=now; view.screen=Screen::RangeResults; drawUi(canvas,view);
                    view.screen=Screen::PortResults; drawUi(canvas,view);
                }
            }
        }
    }
    for (int icon:{RangeScanHome,PortScanHome}) {
        view.screen=Screen::Home; view.selected=icon; view.carouselPosition=icon; drawUi(canvas,view);
        bool label=false;
        for (const auto& item:canvas.texts)
            if (item.value==(icon==RangeScanHome ? "IP range scan" : "Host port scan")) label=true;
        TEST_ASSERT_TRUE(label);
    }
    TEST_ASSERT_EQUAL(0,canvas.violations);
}
void test_ssh_screens_fit_and_password_is_masked() {
    M5Canvas canvas;ViewModel view;NetworkStatus net;SshConfig config;SshStatus status;TerminalBuffer terminal;
    net.phase=NetworkPhase::Ready;config.host="192.168.100.100";config.user="tester";config.secret="not-for-display";
    config.auth=SshAuth::PrivateKey;config.keyPath="/ssh/a-long-private-key-filename";
    view.network=&net;view.sshConfig=&config;view.sshStatus=&status;view.terminal=&terminal;
    view.screen=Screen::SshSetup;
    for(int row=0;row<7;++row) {view.selected=row;drawUi(canvas,view);TEST_ASSERT_EQUAL_MESSAGE(0,canvas.violations,"SSH setup");}
    view.screen=Screen::SshInput;view.sshInputSecret=true;view.draft=config.secret.c_str();view.sshInputLabel="Password / passphrase";drawUi(canvas,view);
    for(const auto& text:canvas.texts) TEST_ASSERT_EQUAL(std::string::npos,text.value.find(config.secret));
    std::strcpy(status.fingerprint,"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    for(auto phase:{SshPhase::Connecting,SshPhase::VerifyHost,SshPhase::Authenticating,SshPhase::Active,SshPhase::Closed,SshPhase::Failed,SshPhase::Stopping}) {
        status.phase=phase;view.screen=Screen::SshSession;drawUi(canvas,view);TEST_ASSERT_EQUAL_MESSAGE(0,canvas.violations,std::to_string(static_cast<int>(phase)).c_str());
    }
    for(int icon:{PortScanHome,SshHome,SettingsHome}) {
        view.screen=Screen::Home;view.selected=icon;view.carouselPosition=icon;
        for(uint32_t now=0;now<12000;now+=100) {view.now=now;drawUi(canvas,view);}
    }
    TEST_ASSERT_EQUAL(0,canvas.violations);
}
void test_capture_screens_and_icon_fit() {
    M5Canvas canvas;ViewModel view;CaptureStatus state;view.capture=&state;
    view.screen=Screen::CaptureSetup;
    for(int channel:{1,13}) for(int row:{0,1}) {view.captureChannel=channel;view.selected=row;drawUi(canvas,view);}
    state.channel=13;state.packets=UINT32_MAX;state.dropped=UINT32_MAX;state.bytes=pcap::fileLimit;state.seconds=360000;
    std::strcpy(state.path,"/captures/cap-01234567.pcap");std::strcpy(state.message,"SD write failed; file may be partial");
    for(auto phase:{CapturePhase::Capturing,CapturePhase::Saving,CapturePhase::Saved,CapturePhase::Failed}) {
        state.phase=phase;view.screen=Screen::CaptureLive;drawUi(canvas,view);
    }
    view.screen=Screen::Home;view.selected=CaptureHome;view.carouselPosition=CaptureHome;
    for(uint32_t now=0;now<3000;now+=100) {view.now=now;drawUi(canvas,view);}
    TEST_ASSERT_EQUAL(0,canvas.violations);
}
void test_python_picker_and_output_fit() {
    M5Canvas canvas;ViewModel view;ScriptCatalog scripts;PythonStatus status;TerminalBuffer terminal;
    view.scripts=&scripts;view.python=&status;view.pythonTerminal=&terminal;view.screen=Screen::PythonScripts;drawUi(canvas,view);
    scripts.count=64;scripts.limited=true;for(auto& name:scripts.names) name=std::string(70,'s')+".py";
    for(int selected:{0,3,63}) {view.selected=selected;drawUi(canvas,view);}
    std::strcpy(status.name,"a-long-python-script-name.py");std::strcpy(status.message,"Not enough memory for Python task");
    for(auto phase:{PythonPhase::Running,PythonPhase::Stopping,PythonPhase::Done,PythonPhase::Failed,PythonPhase::Stopped}) {
        status.phase=phase;view.screen=Screen::PythonOutput;drawUi(canvas,view);
    }
    view.screen=Screen::Home;view.selected=PythonHome;view.carouselPosition=PythonHome;
    for(uint32_t now=0;now<3000;now+=100) {view.now=now;drawUi(canvas,view);}
    TEST_ASSERT_EQUAL(0,canvas.violations);
}
void test_workbench_screens_and_icons_fit() {
    M5Canvas canvas;ViewModel view;SurveyStatus survey;TransferStatus transfer;ScriptEditor editor;SshProfiles profiles;TcpConfig tcp;TcpStatus status;TerminalBuffer terminal;NetworkStatus network;
    view.survey=&survey;view.transfer=&transfer;view.editor=&editor;view.sshProfiles=&profiles;view.tcpConfig=&tcp;view.tcpStatus=&status;view.tcpTerminal=&terminal;view.network=&network;
    network.phase=NetworkPhase::Ready;survey.rows=survey.sweeps=survey.missed=UINT32_MAX;survey.seconds=UINT32_MAX;
    std::strcpy(survey.path,"/surveys/survey-01234567.csv");std::strcpy(transfer.address,"192.168.100.100");std::strcpy(transfer.code,"Z9K7W");transfer.uploads=transfer.downloads=UINT32_MAX;
    tcp.host="192.168.100.100";tcp.port=65535;view.actionPort=65535;
    profiles.count=8;for(auto& entry:profiles.entries) {entry.host=tcp.host;entry.user="test-user";}
    editor.reset("long-script-name.py","");for(int row=0;row<25;++row)editor.insert(std::string(70,'x')+"\n");editor.move(0,-1);
    for(Screen screen:{Screen::Survey,Screen::Transfer,Screen::Editor,Screen::EditorName,Screen::EditorExit,Screen::SshHosts,Screen::PortActions,Screen::TcpSetup,Screen::TcpInput,Screen::TcpSession}) {
        view.screen=screen;for(int selected=0;selected<4;++selected)for(uint32_t now:{0U,700U,5000U}) {view.selected=selected;view.now=now;transfer.running=now==700;status.phase=now==0?TcpPhase::Active:now==700?TcpPhase::Failed:TcpPhase::Closed;drawUi(canvas,view);}
    }
    for(int icon:{SurveyHome,TransferHome,TcpHome}) {view.screen=Screen::Home;view.selected=icon;view.carouselPosition=icon;for(uint32_t now=0;now<4000;now+=100) {view.now=now;drawUi(canvas,view);}}
    TEST_ASSERT_EQUAL(0,canvas.violations);
}
void test_screen_idle_timeout_wake_hold_always_on_and_wrap() {
    ScreenIdle idle;idle.reset(100);
    TEST_ASSERT_FALSE(idle.update(10099,false,false));TEST_ASSERT_FALSE(idle.asleep());
    TEST_ASSERT_TRUE(idle.update(10100,false,false));TEST_ASSERT_TRUE(idle.asleep());
    TEST_ASSERT_TRUE(idle.update(10101,true,false));TEST_ASSERT_FALSE(idle.asleep());
    TEST_ASSERT_TRUE(idle.update(25000,true,false)); // Wake key and chords remain consumed until release.
    TEST_ASSERT_FALSE(idle.update(25001,false,false));
    TEST_ASSERT_FALSE(idle.update(25002,true,false)); // Next press reaches the application.
    TEST_ASSERT_FALSE(idle.update(35001,false,false));TEST_ASSERT_TRUE(idle.update(35002,false,false));
    TEST_ASSERT_FALSE(idle.update(60000,false,true));TEST_ASSERT_FALSE(idle.asleep());
    TEST_ASSERT_FALSE(idle.update(90000,false,true));
    TEST_ASSERT_FALSE(idle.update(99999,false,false));TEST_ASSERT_TRUE(idle.update(100000,false,false));
    idle.reset(UINT32_MAX-4999);TEST_ASSERT_FALSE(idle.update(4999,false,false));TEST_ASSERT_TRUE(idle.update(5000,false,false));
}
int main() { UNITY_BEGIN(); RUN_TEST(test_screen_idle_timeout_wake_hold_always_on_and_wrap); RUN_TEST(test_workbench_screens_and_icons_fit); RUN_TEST(test_python_picker_and_output_fit); RUN_TEST(test_capture_screens_and_icon_fit); RUN_TEST(test_ssh_screens_fit_and_password_is_masked); RUN_TEST(test_lan_scan_screens_and_main_menu_icons_fit); RUN_TEST(test_app_led_idle_everywhere_and_event_priority); RUN_TEST(test_app_led_off_zero_wrap_and_portal_resume); RUN_TEST(test_wifi_tools_screens_fit); RUN_TEST(test_probe_screens_fit); RUN_TEST(test_carousel_animation_wrap_and_layout); RUN_TEST(test_radar_and_list_fit_dense_and_empty_scans); RUN_TEST(test_led_color_settings_and_animation_modes); RUN_TEST(test_battery_indicator_fits_all_headers); RUN_TEST(test_event_melodies_priority_mute_and_reset); RUN_TEST(test_settings_controls_bounds_and_color_preview); RUN_TEST(test_led_brightness_zero_and_full); RUN_TEST(test_counter_pulse_changes_expire_and_reset); RUN_TEST(test_led_breath_events_priority_and_stop); RUN_TEST(test_animated_frames_fit_screen); RUN_TEST(test_submission_views_fit); RUN_TEST(test_ap_presets_all_pages_fit); RUN_TEST(test_all_screens_fit_large_text); RUN_TEST(test_empty_and_stopped_views_fit); RUN_TEST(test_counter_magnitudes_fit); return UNITY_END(); }
