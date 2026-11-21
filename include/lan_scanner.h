#pragma once
#include "lan_scan_model.h"
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <ping/ping_sock.h>

namespace portalis {
class LanScanner {
    enum class Kind { Reply, PingEnd, Port, PortsEnd };
    struct Event { Kind kind; esp_ping_handle_t handle=nullptr; uint32_t value=0; bool success=false; };
    QueueHandle_t events_=nullptr;
    esp_ping_handle_t ping_=nullptr;
    bool stopping_=false, replied_=false;
    uint32_t replyMs_=0, lastIp_=0;
    uint16_t firstPort_=0, lastPort_=0;
    std::atomic<bool> workerRunning_{false}, cancel_{false};
    RangeScanStatus range_;
    PortScanStatus ports_;
    static void reply(esp_ping_handle_t, void*);
    static void pingEnd(esp_ping_handle_t, void*);
    static void portsTask(void*);
    bool prepare();
    bool nextPing();
public:
    bool startRange(const std::string& first, const std::string& last);
    bool startPorts(const std::string& host, const std::string& ports);
    void cancel();
    void poll();
    bool busy() const { return ping_ || workerRunning_.load() || range_.running() || ports_.running(); }
    const RangeScanStatus& range() const { return range_; }
    const PortScanStatus& ports() const { return ports_; }
};
} // namespace portalis
