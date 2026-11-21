#pragma once
#include "model.h"
#include "effects.h"
#include "radar.h"
#include "wifi_tools_model.h"
#include "lan_scan_model.h"
#include "ssh_model.h"
#include "packet_capture_model.h"
#include "python_model.h"
#include "probe_model.h"
#include "carousel.h"
#include "settings.h"
#include "storage.h"
#include <M5GFX.h>

namespace portalis {
enum class Screen { Splash, Home, Setup, Settings, Names, Name, Templates, Live, Stop, History, Submissions, Submission, Radar, ScanList, Probe, ProbeScope, ProbeHistory, Channels, ToolNetworks, Signal, Changes, NetworkPassword, Diagnostics, Ping, PingTarget, NetworkStopping, RangeSetup, RangeResults, PortSetup, PortResults, ScanInput, SshSetup, SshInput, SshSession, CaptureSetup, CaptureLive, PythonScripts, PythonOutput, Error };
struct ViewModel {
    UiSettings settings;
    float carouselPosition = 0;
    Screen screen = Screen::Setup;
    const char* ssid = "Portalis Demo";
    const char* draft = "";
    const char* pageName = "Aurora";
    const char* error = "";
    const ProbeConfig* probeConfig = nullptr;
    const ProbeStatus* probeStatus = nullptr;
    bool probeRunning = false;
    const RadarSnapshot* radar = nullptr;
    const SignalHistory* signal = nullptr;
    const NetworkChanges* changes = nullptr;
    const NetworkStatus* network = nullptr;
    const RangeScanStatus* rangeScan = nullptr;
    const PortScanStatus* portScan = nullptr;
    const char* rangeStart = "";
    const char* rangeEnd = "";
    const char* scanHost = "";
    const char* scanPortRange = "1-1024";
    const char* scanInputLabel = "";
    bool scanBusy = false;
    const ScriptCatalog* scripts = nullptr;
    const PythonStatus* python = nullptr;
    const TerminalBuffer* pythonTerminal = nullptr;
    size_t pythonScroll = 0;
    const CaptureStatus* capture = nullptr;
    uint8_t captureChannel = 1;
    const SshConfig* sshConfig = nullptr;
    const SshStatus* sshStatus = nullptr;
    const TerminalBuffer* terminal = nullptr;
    const char* sshInputLabel = "";
    bool sshInputSecret = false;
    size_t terminalScroll = 0;
    const char* networkName = "";
    size_t passwordLength = 0;
    bool networkPickerCanJoin = true;
    bool channelOverlapView = false;
    std::array<bool,32> rememberedNetworks{};
    const PortalConfig* config = nullptr;
    const BrowserPage* browser = nullptr;
    const History* history = nullptr;
    const std::vector<SubmissionEntry>* submissions = nullptr;
    const SubmissionView* submission = nullptr;
    size_t submissionOffset = 0;
    int selected = 0;
    size_t historyOffset = 0;
    bool running = false, sdReady = false, logHealthy = true;
    float clientsPulse = 0, confirmedPulse = 0;
    bool clientsDecreased = false;
    int batteryPercent = -1;
    uint8_t clients = 0;
    uint32_t count = 0, session = 0, seconds = 0, now = 0, screenSince = 0;
};
void drawUi(M5Canvas& canvas, const ViewModel& view);
} // namespace portalis
