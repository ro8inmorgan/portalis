#pragma once
#include "model.h"
#include "effects.h"
#include "settings.h"
#include "storage.h"
#include <M5GFX.h>

namespace portalis {
enum class Screen { Splash, Setup, Settings, Names, Name, Templates, Live, Stop, History, Submissions, Submission, Error };
struct ViewModel {
    UiSettings settings;
    Screen screen = Screen::Setup;
    const char* ssid = "Portalis Demo";
    const char* draft = "";
    const char* pageName = "Aurora";
    const char* error = "";
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
