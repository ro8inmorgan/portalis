#pragma once
#include "wifi_tools_model.h"
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <ping/ping_sock.h>

namespace portalis {
class NetworkTools {
    enum class EventKind { Reply, Timeout, End, Checks };
    struct Event { EventKind kind; esp_ping_handle_t handle=nullptr; uint32_t ms=0; bool dns=false, internet=false; };
    QueueHandle_t events_=nullptr;
    esp_ping_handle_t ping_=nullptr;
    bool pingStopping_=false, pendingContinuous_=false;
    bool connectionOnly_=false;
    std::string pendingTarget_;
    std::atomic<bool> workerRunning_{false}, cancel_{false};
    NetworkStatus status_;
    uint32_t started_=0;
    static void reply(esp_ping_handle_t, void*);
    static void timeout(esp_ping_handle_t, void*);
    static void end(esp_ping_handle_t, void*);
    static void checksTask(void*);
    bool startPing(const char* target, bool continuous);
    void stopPing();
    void startChecks();
    void finishStop();
public:
    bool begin(const RadarAccessPoint&, const std::string& password, bool pingMode, bool connectionOnly=false);
    void poll();
    void stop();
    bool busy() const { return status_.phase!=NetworkPhase::Idle; }
    const NetworkStatus& status() const { return status_; }
    bool setTarget(const std::string& target);
    void togglePause();
    void refresh();
};
} // namespace portalis
