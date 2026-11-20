#pragma once
#include "probe_model.h"
#include "model.h"
#include <functional>
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>

namespace portalis {
class ProbeTest {
    static ProbeTest* instance_;
    static void receive(void* packet, wifi_promiscuous_pkt_type_t type);
    QueueHandle_t queue_ = nullptr;
    std::atomic<bool> receiving_{false};
    WebServer server_{80};
    DNSServer dns_;
    ProbeConfig config_;
    TicketGate tickets_;
    std::function<bool(Event, const std::string&)> onFeedback_;
    uint32_t session_ = 0;
    void securityHeaders(const std::string& nonce = "");
    void sendPage();
    ProbeSchedule schedule_;
    ProbeStatus status_;
    bool active_ = false;
    uint32_t started_ = 0, hopped_ = 0;
    uint8_t firstChannel_ = 1, channelCount_ = 11;
    void disableReceiver();
    bool listen();
    void endAttempt();
    void beginAttempt(const ProbeCandidate& candidate);
public:
    void init(std::function<bool(Event, const std::string&)> onFeedback);
    bool start(const ProbeConfig& config);
    void stop();
    void poll();
    bool running() const { return active_; }
    const ProbeStatus& status() const { return status_; }
};
} // namespace portalis
