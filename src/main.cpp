#include <M5Cardputer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp32-hal-rgb-led.h>
#include "portal.h"
#include "storage.h"
#include "ui.h"
#include "wifi_scanner.h"
#include "network_tools.h"
#include "wifi_profiles.h"
#include "probe_test.h"

using namespace portalis;
namespace {
Portal portal;
WifiScanner scanner;
NetworkTools networkTools;
WifiProfiles wifiProfiles;
Preferences wifiPreferences;
bool wifiPreferencesReady=false;
WifiProfile pendingProfile;
SignalHistory signalHistory;
NetworkChanges networkChanges;
RadarAccessPoint networkChoice;
int toolHomeIndex=ChannelsHome;
bool channelOverlapView=false;
ProbeTest probe;
ProbeConfig probeConfig;
CarouselMotion carousel;
int homeReturnIndex = 0;
Storage storage;
History history;
Preferences preferences;
PageTemplate page;
BrowserPage browser;
PortalConfig portalConfig;
UiSettings uiSettings;
Screen settingsReturn = Screen::Setup;
Screen templatesReturn = Screen::Setup;
std::optional<std::string> probeTemplatePath;
std::optional<uint32_t> probeTimeoutSeconds;
std::vector<SubmissionEntry> submissions;
SubmissionView submission;
Screen submissionsReturn = Screen::Setup;
size_t submissionOffset = 0;
int submissionSelected = 0;
Screen screen = Screen::Splash, returnScreen = Screen::Setup, historyReturn = Screen::Setup;
std::string ssid = "Portalis Demo", draft, templatePath, error;
int selected = 0;
size_t historyOffset = 0;
uint32_t screenSince = 0;
bool preferencesReady = false;
M5Canvas canvas(&M5.Display);
alignas(16) uint8_t frameBuffer[240 * 135];

void show(Screen next) { screen = next; selected = 0; screenSince = millis(); }
void openHome(int index = 0) { homeReturnIndex = index; show(Screen::Home); selected = index; carousel.reset(index, millis()); }
void notice(const std::string& message, Screen back) { error = message; returnScreen = back; show(Screen::Error); }
void saveConfig() {
    if (!preferencesReady) return;
    preferences.putString("ssid", ssid.c_str());
    preferences.putString("template", templatePath.c_str());
}
void menuSound(bool select = false) {
    if (!uiSettings.sounds || !uiSettings.volume) return;
    M5.Speaker.setVolume(uiSettings.speakerVolume());
    M5.Speaker.tone(select ? 1400 : 950, select ? 45 : 18, 0, true);
}
void openSettings(Screen back) { settingsReturn = back; show(Screen::Settings); }
bool saveSettings() {
    if (!preferencesReady) return false;
    const bool saved = preferences.putUChar("ui_accent", uiSettings.accent) == 1 &&
        preferences.putUChar("ui_bg", uiSettings.background) == 1 &&
        preferences.putUChar("led_bright", uiSettings.ledBrightness) == 1 &&
        preferences.putUChar("volume", uiSettings.volume) == 1 &&
        preferences.putBool("menu_sounds", uiSettings.sounds) == 1 &&
        preferences.putBool("event_sounds", uiSettings.eventSounds) == 1 &&
        preferences.putUChar("led_idle", uiSettings.ledColors[0]) == 1 &&
        preferences.putUChar("led_join", uiSettings.ledColors[1]) == 1 &&
        preferences.putUChar("led_confirm", uiSettings.ledColors[2]) == 1 &&
        preferences.putUChar("led_leave", uiSettings.ledColors[3]) == 1 &&
        preferences.putUChar("led_animation", uiSettings.ledAnimation) == 1;
    return saved;
}
void openNames(bool remount = false) {
    if (remount) storage.begin();
    show(Screen::Names);
    std::string message;
    if (!storage.loadConfig(portalConfig, message)) notice(message, Screen::Names);
}
void browse(const std::string& directory, size_t offset = 0, bool last = false) {
    if (!storage.browse(directory, offset, browser, templatesReturn != Screen::Probe)) {
        storage.browse("/", 0, browser, templatesReturn != Screen::Probe);
        notice("Folder unavailable. Returned to SD root.", Screen::Templates);
        return;
    }
    screen = Screen::Templates;
    selected = last && browser.count ? browser.count - 1 : 0;
    screenSince = millis();
}
void parentFolder() {
    // Previously the shared picker always returned to Setup.
    if (browser.directory == "/") { show(templatesReturn); return; }
    const size_t slash = browser.directory.find_last_of('/');
    browse(slash == 0 ? "/" : browser.directory.substr(0, slash));
}
void startPortal() {
    // Reload the selected file before each start, so SD edits are reflected.
    PageTemplate current;
    std::string message;
    if (!storage.loadTemplate(templatePath, current, message)) { notice(message, Screen::Setup); return; }
    uint32_t session = preferencesReady ? preferences.getUInt("session", 0) + 1 : (esp_random() & 0xffff);
    if (!session) session = 1;
    if (!portal.start(ssid, current, session)) { notice("AP could not start. Return and try again.", Screen::Setup); return; }
    page = current;
    if (preferencesReady) preferences.putUInt("session", session);
    show(Screen::Live);
}
void openHistory(Screen back) { historyReturn = back; historyOffset = 0; show(Screen::History); }
void openSubmissions(Screen back) {
    submissionsReturn = back;
    show(Screen::Submissions);
    std::string message;
    if (!storage.listSubmissions(submissions, message)) notice(message, back);
}
void reloadProbeConfig() {
    std::string message;
    if (!loadProbeConfig(probeConfig, message, probeTemplatePath)) { notice(message, Screen::Probe); return; }
    if (probeTimeoutSeconds) probeConfig.timeoutSeconds = *probeTimeoutSeconds;
}
void adjustProbeTimeout(int direction) {
    const int seconds = std::max(static_cast<int>(ProbeConfig::minTimeoutSeconds),
        std::min(static_cast<int>(ProbeConfig::maxTimeoutSeconds), static_cast<int>(probeConfig.timeoutSeconds) + direction * 30));
    probeConfig.timeoutSeconds = seconds;
    probeTimeoutSeconds = seconds;
    if (preferencesReady && preferences.putUInt("probe_timeout", seconds) != sizeof(uint32_t))
        notice("Timeout changed; could not save it.", Screen::Probe);
}
void startProbe() {
    reloadProbeConfig();
    if (screen != Screen::Probe) return;
    if (probeConfig.noticePath.empty() || probeConfig.noticeHtml.empty()) {
        notice("Choose an HTML page before starting.", Screen::Probe); return;
    }
    if (!probe.start(probeConfig)) notice("Probe test could not start. Try again.", Screen::Probe);
    else show(Screen::Probe);
}
bool saveWifiProfiles() {
    if (!wifiPreferencesReady) return false;
    const auto json=wifiProfiles.serialize();
    return wifiPreferences.putString("profiles",json.c_str())==json.size();
}
void clearPendingProfile() {
    std::fill(pendingProfile.password.begin(),pendingProfile.password.end(),'\0'); pendingProfile={};
}
void clearNetworkPassword() { std::fill(draft.begin(),draft.end(),'\0'); draft.clear(); }
void stopWifiTool() {
    clearNetworkPassword(); clearPendingProfile(); scanner.stop(); networkTools.stop();
    if (networkTools.busy()) show(Screen::NetworkStopping);
    else openHome(toolHomeIndex);
}
void openWifiTool(int index) {
    toolHomeIndex=index;
    if (portal.running() || probe.running() || networkTools.busy()) {
        notice("Stop the active Wi-Fi mode first.",Screen::Home); return;
    }
    if (!scanner.start()) { notice("Could not start Wi-Fi scan.",Screen::Home); return; }
    if (index==ChannelsHome) show(Screen::Channels);
    else if (index==ChangesHome) { networkChanges.reset(); show(Screen::Changes); }
    else show(Screen::ToolNetworks);
}
void joinToolNetwork() {
    scanner.stop();
    const bool started=networkTools.begin(networkChoice,draft,toolHomeIndex==PingHome);
    clearPendingProfile();
    if (started) pendingProfile={networkChoice.rawSsid,draft};
    clearNetworkPassword();
    if (!started) { notice("Could not join network.",Screen::Home); return; }
    show(toolHomeIndex==PingHome ? Screen::Ping : Screen::Diagnostics);
}
void handleKey(char key) {
    const bool enter = key == '\n', back = key == '\b', escape = key == '`';
    const bool up = key == ';', down = key == '.';
    const Screen before = screen;
    bool settingChanged = false;
    switch (screen) {
        case Screen::Splash:
            openHome();
            break;
        case Screen::Home:
            if (key == ',' || key == '/') {
                const int direction = key == ',' ? -1 : 1;
                selected = (selected + CarouselMotion::count + direction) % CarouselMotion::count;
                carousel.move(direction, millis());
                menuSound();
            } else if (enter) {
                homeReturnIndex = selected;
                if (selected == 0) show(Screen::Setup);
                else if (selected == 1) {
                    if (scanner.start()) show(Screen::Radar);
                    else notice("Stop the portal before scanning Wi-Fi.", Screen::Home);
                } else if (selected == 2) { show(Screen::Probe); reloadProbeConfig(); }
                else if (selected == SettingsHome) openSettings(Screen::Home);
                else openWifiTool(selected);
            }
            break;
        case Screen::Channels:
            if (escape || back) stopWifiTool();
            else if (enter) channelOverlapView=!channelOverlapView;
            else if (up) selected=(selected+13)%14;
            else if (down) selected=(selected+1)%14;
            else if (key=='r' || key=='R') scanner.refresh();
            break;
        case Screen::ToolNetworks:
            if (escape || back) { stopWifiTool(); break; }
            if (key=='r' || key=='R') scanner.refresh();
            if (up && selected>0) --selected;
            if (down && selected+1<static_cast<int>(scanner.snapshot().count)) ++selected;
            if ((key=='f' || key=='F') && toolHomeIndex!=SignalHome && selected>=0 && selected<static_cast<int>(scanner.snapshot().count)) {
                const auto& ap=scanner.snapshot().points[selected];
                if (wifiProfiles.forget(ap.rawSsid) && !saveWifiProfiles()) notice("Forgot for now; save failed.",Screen::ToolNetworks);
            }
            if (enter && selected>=0 && selected<static_cast<int>(scanner.snapshot().count)) {
                networkChoice=scanner.snapshot().points[selected];
                if (toolHomeIndex==SignalHome) {
                    signalHistory.select(networkChoice,scanner.snapshot().generation);
                    scanner.track(networkChoice.id); scanner.refresh(); show(Screen::Signal);
                } else if (!networkChoice.connectable) {
                    notice("Choose a visible open or personal WPA network.",Screen::ToolNetworks);
                } else {
                    clearNetworkPassword();
                    if (!std::strcmp(networkChoice.security,"OPEN")) joinToolNetwork();
                    else {
                        const int saved=wifiProfiles.find(networkChoice.rawSsid);
                        if (saved>=0 && !wifiProfiles.entries[saved].password.empty()) {
                            draft=wifiProfiles.entries[saved].password; joinToolNetwork();
                        } else show(Screen::NetworkPassword);
                    }
                }
            }
            break;
        case Screen::Signal:
            if (escape || back) stopWifiTool();
            else if (enter) { scanner.track(0); show(Screen::ToolNetworks); }
            else if (key=='r' || key=='R') scanner.refresh();
            break;
        case Screen::Changes:
            if (escape || back) stopWifiTool();
            else if (up && selected>0) --selected;
            else if (down && selected+1<static_cast<int>(networkChanges.count)) ++selected;
            else if (key=='r' || key=='R') scanner.refresh();
            else if (key=='c' || key=='C') networkChanges.reset();
            break;
        case Screen::NetworkPassword:
            if (escape) stopWifiTool();
            else if (back) { if (!draft.empty()) { draft.back()='\0'; draft.pop_back(); } }
            else if (enter) {
                if (!validWifiPassword(draft)) notice("Use 8-63 characters or a 64-digit hex key.",Screen::NetworkPassword);
                else joinToolNetwork();
            } else if (key>=32 && key<=126 && draft.size()<64) draft+=key;
            break;
        case Screen::Diagnostics:
        case Screen::Ping:
            if (escape || back) stopWifiTool();
            else if (key=='r' || key=='R') networkTools.refresh();
            else if (screen==Screen::Ping && networkTools.status().phase==NetworkPhase::Ready) {
                if (key=='p' || key=='P') networkTools.togglePause();
                else if (key=='t' || key=='T') { draft=networkTools.status().target; show(Screen::PingTarget); }
            }
            break;
        case Screen::PingTarget:
            if (escape) { draft.clear(); show(Screen::Ping); }
            else if (back) { if (!draft.empty()) draft.pop_back(); }
            else if (enter) {
                if (!networkTools.setTarget(draft)) notice("Enter a valid IPv4 target while connected.",Screen::PingTarget);
                else { draft.clear(); show(Screen::Ping); }
            } else if (((key>='0' && key<='9') || key=='.') && draft.size()<15) draft+=key;
            break;
        case Screen::NetworkStopping:
            break;
        case Screen::Setup:
            if (escape || back) { openHome(); break; }
            if (key == 'h' || key == 'H') { openHistory(Screen::Setup); break; }
            if (up) selected = (selected + 3) % 4;
            if (down) selected = (selected + 1) % 4;
            if (enter) {
                if (selected == 0) openNames();
                else if (selected == 1) { templatesReturn = Screen::Setup; browse("/"); }
                else if (selected == 2) startPortal();
                else if (selected == 3) openSubmissions(Screen::Setup);
            }
            break;
        case Screen::Radar:
        case Screen::ScanList:
            if (escape || back) { scanner.stop(); openHome(1); }
            else if (key == 'r' || key == 'R') scanner.refresh();
            else if (enter) {
                screen = screen == Screen::Radar ? Screen::ScanList : Screen::Radar;
                screenSince = millis();
            } else if (scanner.snapshot().count) {
                const int count = static_cast<int>(scanner.snapshot().count);
                if (up) selected = (selected + count - 1) % count;
                if (down) selected = (selected + 1) % count;
            }
            break;
        case Screen::Probe:
            if (escape || back) { probe.stop(); openHome(2); }
            else if (!probe.running() && up) selected = (selected + 3) % 4;
            else if (!probe.running() && down) selected = (selected + 1) % 4;
            else if (!probe.running() && selected == 1 && (key == ',' || key == '/')) adjustProbeTimeout(key == ',' ? -1 : 1);
            else if (enter) {
                if (probe.running()) show(Screen::ProbeScope);
                else if (selected == 0) { templatesReturn = Screen::Probe; browse("/"); }
                else if (selected == 1) adjustProbeTimeout(1);
                else if (selected == 2) startProbe();
                else if (selected == 3) openSubmissions(Screen::Probe);
            }
            else if (key == 'h' || key == 'H') show(Screen::ProbeHistory);
            else if ((key == 't' || key == 'T') && !probe.running()) {
                templatesReturn = Screen::Probe; browse("/");
            }
            else if (key == 'v' || key == 'V') openSubmissions(Screen::Probe);
            else if ((key == 'r' || key == 'R') && !probe.running()) { storage.begin(); reloadProbeConfig(); }
            else if (key == 's' || key == 'S') {
                if (probe.running()) { probe.stop(); show(Screen::Probe); }
                else startProbe();
            }
            break;
        case Screen::ProbeScope:
        case Screen::ProbeHistory: {
            const int count = screen == Screen::ProbeScope ? probeConfig.count : probe.status().eventCount;
            if (escape || back || enter) show(Screen::Probe);
            else if (up && selected > 0) --selected;
            else if (down && selected + 1 < count) ++selected;
            break;
        }
        case Screen::Settings:
            if (escape || back) {
                if (saveSettings()) { if (settingsReturn == Screen::Home) openHome(SettingsHome); else show(settingsReturn); }
                else notice("Settings active, but could not save to device.", settingsReturn);
                break;
            }
            if (up) selected = (selected + kSettingsCount - 1) % kSettingsCount;
            else if (down) selected = (selected + 1) % kSettingsCount;
            else if (key == ',' || key == '/' || enter) {
                uiSettings.adjust(selected, key == ',' ? -1 : 1);
                settingChanged = true;
                M5.Speaker.setVolume(uiSettings.speakerVolume());
                if (!uiSettings.volume) M5.Speaker.stop();
                if (!uiSettings.sounds) M5.Speaker.stop(0);
                if (!uiSettings.eventSounds) M5.Speaker.stop(1);
            }
            break;
        case Screen::Names: {
            const int count = static_cast<int>(portalConfig.count) + 1;
            if (escape || back) { show(Screen::Setup); break; }
            if (key == 'r' || key == 'R') { openNames(true); break; }
            if (up) selected = (selected + count - 1) % count;
            if (down) selected = (selected + 1) % count;
            if (enter) {
                if (selected == 0) { draft = ssid; show(Screen::Name); }
                else { ssid = portalConfig.apNames[selected - 1]; saveConfig(); show(Screen::Setup); }
            }
            break;
        }
        case Screen::Name:
            if (escape) show(Screen::Names);
            else if (enter) {
                if (!validSsid(draft)) notice("Use 1-32 characters. No leading/trailing spaces.", Screen::Name);
                else { ssid = draft; saveConfig(); show(Screen::Setup); }
            } else if (back) { if (!draft.empty()) draft.pop_back(); }
            else if (key >= 32 && key <= 126 && draft.size() < 32) draft += key;
            break;
        case Screen::Templates:
            // Previous picker exit: if (escape) { show(Screen::Setup); break; }
            if (escape) { show(templatesReturn); break; }
            if (back) { parentFolder(); break; }
            if (key == 'r' || key == 'R') { storage.begin(); browse("/"); break; }
            if (up) {
                if (selected > 0) --selected;
                else if (browser.offset >= browser.entries.size()) browse(browser.directory, browser.offset - browser.entries.size(), true);
            }
            if (down) {
                if (selected + 1 < static_cast<int>(browser.count)) ++selected;
                else if (browser.offset + browser.count < browser.total) browse(browser.directory, browser.offset + browser.entries.size());
            }
            if (enter && browser.count) {
                const BrowserEntry entry = browser.entries[selected];
                if (entry.directory) browse(entry.path);
                else if (templatesReturn == Screen::Probe) {
                    if (entry.path.empty()) { notice("Choose an HTML file on SD.", Screen::Templates); break; }
                    ProbeConfig candidate;
                    std::string message;
                    if (!loadProbeConfig(candidate, message, entry.path)) notice(message, Screen::Templates);
                    else {
                        if (probeTimeoutSeconds) candidate.timeoutSeconds = *probeTimeoutSeconds;
                        probeConfig = std::move(candidate);
                        probeTemplatePath = entry.path;
                        if (preferencesReady) preferences.putString("probe_template", probeTemplatePath->c_str());
                        show(Screen::Probe);
                    }
                }
                else {
                    PageTemplate candidate;
                    std::string message;
                    if (!storage.loadTemplate(entry.path, candidate, message)) notice(message, Screen::Templates);
                    else { page = candidate; templatePath = entry.path; saveConfig(); show(Screen::Setup); }
                }
            }
            break;
        case Screen::Live:
            if (key == 'c' || key == 'C') openSettings(Screen::Live);
            else if (key == 's' || key == 'S') openSubmissions(Screen::Live);
            else if (key == 'h' || key == 'H') openHistory(Screen::Live);
            else if (portal.running() && (key == 'x' || key == 'X' || escape)) show(Screen::Stop);
            else if (!portal.running() && (enter || escape)) show(Screen::Setup);
            break;
        case Screen::Stop:
            if (enter) { portal.stop(); show(Screen::Live); }
            else if (escape || back) show(Screen::Live);
            break;
        case Screen::History:
            if (escape || back || enter) show(historyReturn);
            else if (up && historyOffset > 0) --historyOffset;
            else if (down && historyOffset + 4 < history.size()) ++historyOffset;
            break;
        case Screen::Submissions:
            if (escape || back) show(submissionsReturn);
            else if (key == 'r' || key == 'R') openSubmissions(submissionsReturn);
            else if (up && selected > 0) --selected;
            else if (down && selected + 1 < static_cast<int>(submissions.size())) ++selected;
            else if (enter && !submissions.empty()) {
                std::string message;
                submissionSelected = selected;
                if (!storage.readSubmission(submissions[submissions.size() - 1 - selected], submission, message))
                    notice(message, Screen::Submissions);
                else { submissionOffset = 0; show(Screen::Submission); }
            }
            break;
        case Screen::Submission:
            if (escape || back || enter) { show(Screen::Submissions); selected = submissionSelected; }
            else if (up && submissionOffset > 0) --submissionOffset;
            else if (down && submissionOffset + 4 < submission.lines.size()) ++submissionOffset;
            break;
        case Screen::Error:
            if (enter || escape || back) {
                if (returnScreen == Screen::Home) openHome(homeReturnIndex);
                else show(returnScreen);
            }
            break;
    }
    if (up || down || settingChanged) screenSince = millis();
    if (settingChanged || enter || escape || back || (before != Screen::Name && (up || down)))
        menuSound(enter || settingChanged);
}
void pollKeyboard() {
    // Detect individual character edges rather than relying on isChange(), which
    // only notices a change in the number of keys in the Cardputer library.
    static std::array<bool, 128> previous{};
    static bool oldEnter = false, oldDelete = false;
    std::array<bool, 128> current{};
    const auto& keys = M5Cardputer.Keyboard.keysState();
    for (unsigned char key : keys.word) if (key < current.size()) current[key] = true;
    if (keys.space) current[' '] = true;
    const bool enter = keys.enter && !oldEnter, del = keys.del && !oldDelete;
    const auto before = previous;
    previous = current; oldEnter = keys.enter; oldDelete = keys.del;
    if (enter) { handleKey('\n'); return; }
    if (del) { handleKey('\b'); return; }
    for (size_t key = 32; key < current.size(); ++key) {
        if (current[key] && !before[key]) handleKey(static_cast<char>(key));
    }
}
void render() {
    ViewModel view;
    view.carouselPosition = carousel.position(millis());
    view.radar = &scanner.snapshot();
    view.signal=&signalHistory; view.changes=&networkChanges; view.network=&networkTools.status();
    view.networkName=networkChoice.ssid; view.passwordLength=draft.size();
    view.networkPickerCanJoin=toolHomeIndex!=SignalHome; view.channelOverlapView=channelOverlapView;
    for (size_t i=0;i<scanner.snapshot().count;++i) view.rememberedNetworks[i]=wifiProfiles.find(scanner.snapshot().points[i].rawSsid)>=0;
    view.probeConfig = &probeConfig; view.probeStatus = &probe.status(); view.probeRunning = probe.running();
    view.settings = uiSettings;
    view.screen = screen; view.ssid = ssid.c_str(); view.draft = draft.c_str();
    view.pageName = page.name.c_str(); view.error = error.c_str();
    view.config = &portalConfig; view.browser = &browser; view.history = &history; view.selected = selected;
    view.submissions = &submissions; view.submission = &submission; view.submissionOffset = submissionOffset;
    view.historyOffset = historyOffset; view.running = portal.running();
    view.sdReady = storage.ready(); view.logHealthy = storage.logHealthy();
    view.clients = portal.clients(); view.count = portal.count(); view.session = portal.session();
    view.seconds = portal.elapsed(); view.now = millis(); view.screenSince = screenSince;
    static int batteryPercent = -1;
    static uint32_t batterySampleAt = 0;
    static bool batterySampled = false;
    if (!batterySampled || static_cast<uint32_t>(view.now - batterySampleAt) >= 2000) {
        const int reading = M5.Power.getBatteryLevel();
        batteryPercent = reading < 0 ? -1 : reading > 100 ? 100 : reading;
        batterySampleAt = view.now; batterySampled = true;
    }
    view.batteryPercent = batteryPercent;
    static CounterPulse clientPulse, confirmationPulse;
    static uint32_t lastSession = 0;
    static bool wasRunning = false;
    const bool reset = !view.running || !wasRunning || view.session != lastSession;
    clientPulse.update(view.clients, view.now, reset);
    confirmationPulse.update(view.count, view.now, reset);
    view.clientsPulse = clientPulse.strength(view.now);
    view.confirmedPulse = confirmationPulse.strength(view.now);
    view.clientsDecreased = clientPulse.decreased();
    lastSession = view.session; wasRunning = view.running;
    static EventSounds eventSounds;
    const bool audioEnabled = uiSettings.eventSounds && uiSettings.volume;
    const auto note = eventSounds.update(view.clients, view.count, view.session, view.running, audioEnabled, view.now);
    if (!view.running || !audioEnabled) M5.Speaker.stop(1);
    else if (note.frequency) {
        M5.Speaker.setVolume(uiSettings.speakerVolume());
        M5.Speaker.tone(note.frequency, note.duration, 1, true);
    }
    static LedActivity led;
    const auto palette = uiSettings.ledPalette();
    const auto animation = static_cast<LedAnimation>(uiSettings.ledAnimation);
    LedColor color = led.update(view.clients, view.count, view.session, view.running, view.now,
                                uiSettings.ledBrightness, palette, animation);
    const bool previewLed = screen == Screen::Settings && (selected == 2 || selected >= 6);
    if (previewLed) {
        if (selected >= 7 && selected <= 9) {
            color = ledBrightness(kLedColors[uiSettings.ledColors[selected - 6]].color, uiSettings.ledBrightness);
        } else {
            static LedActivity preview;
            color = preview.update(0, 0, 0, true, view.now, uiSettings.ledBrightness, palette, animation);
        }
    }
    if (animation == LedAnimation::Off) color = {};
    const bool useLedPower = (view.running || previewLed) && uiSettings.ledBrightness != 0 && animation != LedAnimation::Off;
    static bool ledPowered = false;
    // ADV/v1.1 share GPIO38 between backlight and RGB power. Keep power steady
    // while animating; PWM dimming can reset the WS2812. Restore idle brightness.
    if (useLedPower && !ledPowered) M5.Display.setBrightness(255);
    neopixelWrite(21, color.red, color.green, color.blue);
    if (!useLedPower && ledPowered) M5.Display.setBrightness(160);
    ledPowered = useLedPower;
    drawUi(canvas, view);
    canvas.pushSprite(0, 0);
}
}
void setup() {
    Serial.begin(115200);
    // Deassert the optional CapLoRa radio before the shared SD SPI bus is used.
    pinMode(5, OUTPUT); digitalWrite(5, HIGH);
    auto config = M5.config();
    config.internal_spk = true; config.internal_mic = false;
    M5Cardputer.begin(config, true);
    M5.Display.setRotation(0);
    if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
    M5.Display.setBrightness(255);
    neopixelWrite(21, 0, 0, 0);
    M5.Display.setBrightness(160);
    canvas.setColorDepth(8);
    canvas.setBuffer(frameBuffer, 240, 135, 8);
    WiFi.mode(WIFI_OFF);
    wifiPreferencesReady=wifiPreferences.begin("portalis-net",false);
    if (wifiPreferencesReady) wifiProfiles.load(wifiPreferences.getString("profiles", "").c_str());
    preferencesReady = preferences.begin("portalis", false);
    if (preferencesReady) {
        const std::string saved = preferences.getString("ssid", "Portalis Demo").c_str();
        if (validSsid(saved)) ssid = saved;
        templatePath = preferences.getString("template", "").c_str();
        // A missing or old empty preference uses notice_template in portalis.json.
        const std::string savedProbeTemplate = preferences.getString("probe_template", "\n").c_str();
        if (savedProbeTemplate != "\n" && !savedProbeTemplate.empty()) probeTemplatePath = savedProbeTemplate;
        const uint32_t savedProbeTimeout = preferences.getUInt("probe_timeout", 0);
        if (savedProbeTimeout >= ProbeConfig::minTimeoutSeconds && savedProbeTimeout <= ProbeConfig::maxTimeoutSeconds)
            probeTimeoutSeconds = savedProbeTimeout;
        uiSettings.accent = preferences.getUChar("ui_accent", 0);
        uiSettings.background = preferences.getUChar("ui_bg", 0);
        uiSettings.ledBrightness = preferences.getUChar("led_bright", 80);
        uiSettings.volume = preferences.getUChar("volume", 30);
        uiSettings.sounds = preferences.getBool("menu_sounds", true);
        uiSettings.eventSounds = preferences.getBool("event_sounds", true);
        uiSettings.ledColors[0] = preferences.getUChar("led_idle", 0);
        uiSettings.ledColors[1] = preferences.getUChar("led_join", 1);
        uiSettings.ledColors[2] = preferences.getUChar("led_confirm", 2);
        uiSettings.ledColors[3] = preferences.getUChar("led_leave", 3);
        uiSettings.ledAnimation = preferences.getUChar("led_animation", 0);
        uiSettings.validate();
    }
    M5.Speaker.setVolume(uiSettings.speakerVolume());
    storage.begin(); storage.restore(history);
    std::string message;
    if (!storage.loadTemplate(templatePath, page, message)) {
        templatePath.clear(); page = PageTemplate{};
    }
    probe.init([](Event event, const std::string& fields) {
        return storage.appendSubmission(event, fields);
    });
    portal.init([](Event event, const std::string& fields) {
        // Never acknowledge entered data unless it was persisted successfully.
        if ((storage.ready() || fields != "[]") && !storage.appendSubmission(event, fields)) return false;
        history.add(event);
        storage.append(event);
        return true;
    });
    screenSince = millis();
    render();
    Serial.printf("Portalis ready: %dx%d landscape\n", M5.Display.width(), M5.Display.height());
}
void loop() {
    M5Cardputer.update();
    pollKeyboard();
    portal.poll();
    probe.poll();
    networkTools.poll();
    if (!pendingProfile.ssid.empty()) {
        const auto& connection=networkTools.status();
        if (connection.dhcp==CheckState::Pass && (connection.phase==NetworkPhase::Ready || connection.phase==NetworkPhase::Checking)) {
            wifiProfiles.remember(pendingProfile.ssid,pendingProfile.password);
            const bool saved=saveWifiProfiles(); clearPendingProfile();
            if (!saved) notice("Joined, but could not save Wi-Fi profile.",toolHomeIndex==PingHome ? Screen::Ping : Screen::Diagnostics);
        } else if (connection.phase==NetworkPhase::Failed || connection.phase==NetworkPhase::Idle) clearPendingProfile();
    }
    if (screen==Screen::NetworkStopping && !networkTools.busy()) openHome(toolHomeIndex);
    const auto& scan = scanner.snapshot();
    const uint64_t selectedId = selected >= 0 && selected < static_cast<int>(scan.count) ? scan.points[selected].id : 0;
    const uint32_t generation = scan.generation;
    scanner.poll();
    if ((screen == Screen::Radar || screen == Screen::ScanList || screen == Screen::ToolNetworks) && scanner.snapshot().generation != generation)
        selected = radarSelection(scanner.snapshot(), selectedId);
    if (screen==Screen::Signal) signalHistory.update(scanner.snapshot());
    if (screen==Screen::Changes) networkChanges.update(scanner.snapshot());
    if (screen == Screen::Splash && static_cast<uint32_t>(millis() - screenSince) >= kSplashDuration) openHome();
    static uint32_t lastFrame = 0;
    if (millis() - lastFrame >= 50) { lastFrame = millis(); render(); }
    delay(2);
}
