#include "lan_scanner.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <lwip/inet.h>
#include <freertos/task.h>

namespace portalis {
void LanScanner::reply(esp_ping_handle_t handle, void* arg) {
    auto* self=static_cast<LanScanner*>(arg);
    Event event{Kind::Reply,handle};
    esp_ping_get_profile(handle,ESP_PING_PROF_TIMEGAP,&event.value,sizeof(event.value));
    xQueueSend(self->events_,&event,portMAX_DELAY);
}
void LanScanner::pingEnd(esp_ping_handle_t handle, void* arg) {
    Event event{Kind::PingEnd,handle};
    xQueueSend(static_cast<LanScanner*>(arg)->events_,&event,portMAX_DELAY);
}
bool LanScanner::prepare() {
    if (busy() || WiFi.status()!=WL_CONNECTED || (WiFi.getMode() & WIFI_MODE_AP)) return false;
    if (!events_) events_=xQueueCreate(16,sizeof(Event));
    if (!events_) return false;
    xQueueReset(events_); cancel_.store(false); stopping_=false; return true;
}
bool LanScanner::nextPing() {
    replied_=false; replyMs_=0;
    esp_ping_config_t config=ESP_PING_DEFAULT_CONFIG();
    const auto target=scanIpText(range_.current);
    if (!ipaddr_aton(target.c_str(),&config.target_addr)) return false;
    config.count=1; config.timeout_ms=350; config.interval_ms=1;
    esp_ping_callbacks_t callbacks{};
    callbacks.cb_args=this; callbacks.on_ping_success=reply; callbacks.on_ping_end=pingEnd;
    if (esp_ping_new_session(&config,&callbacks,&ping_)!=ESP_OK) { ping_=nullptr; return false; }
    if (esp_ping_start(ping_)!=ESP_OK) { esp_ping_delete_session(ping_); ping_=nullptr; return false; }
    return true;
}
bool LanScanner::startRange(const std::string& from, const std::string& to) {
    uint32_t first,last;
    if (!scanRange(from,to,first,last) || !prepare()) return false;
    range_={}; range_.phase=ScanPhase::Running; range_.total=last-first+1;
    range_.current=first; lastIp_=last;
    if (!nextPing()) { range_.phase=ScanPhase::Failed; return false; }
    return true;
}
bool LanScanner::startPorts(const std::string& host, const std::string& ports) {
    uint32_t ip; uint16_t first,last;
    if (!parseScanIp(host,ip) || !scanPorts(ports,first,last) || !prepare()) return false;
    ports_={}; ports_.host=ip; ports_.total=uint32_t(last)-first+1; ports_.phase=ScanPhase::Running;
    firstPort_=first; lastPort_=last;
    workerRunning_.store(true);
    if (xTaskCreate(portsTask,"portalis-ports",4096,this,1,nullptr)!=pdPASS) {
        workerRunning_.store(false); ports_.phase=ScanPhase::Failed; return false;
    }
    return true;
}
void LanScanner::portsTask(void* arg) {
    auto* self=static_cast<LanScanner*>(arg);
    const uint32_t host=self->ports_.host;
    const IPAddress ip(host>>24,(host>>16)&255,(host>>8)&255,host&255);
    bool failed=false;
    for (uint32_t port=self->firstPort_; port<=self->lastPort_ && !self->cancel_.load(); ++port) {
        if (WiFi.status()!=WL_CONNECTED) { failed=true; break; }
        WiFiClient client;
        Event event{Kind::Port,nullptr,port,client.connect(ip,port,250)==1};
        client.stop();
        // Keep the UI as the sole owner of result arrays; retain every open port
        // until the bounded result list fills. Cancellation breaks backpressure.
        while (xQueueSend(self->events_,&event,pdMS_TO_TICKS(20))!=pdTRUE)
            if (self->cancel_.load()) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    Event done{Kind::PortsEnd,nullptr,0,failed};
    xQueueSend(self->events_,&done,portMAX_DELAY);
    self->workerRunning_.store(false);
    vTaskDelete(nullptr);
}
void LanScanner::cancel() {
    cancel_.store(true);
    if (ping_ && !stopping_) { stopping_=true; esp_ping_stop(ping_); }
}
void LanScanner::poll() {
    if (!events_) return;
    if (ping_ && WiFi.status()!=WL_CONNECTED) { range_.phase=ScanPhase::Failed; cancel(); }
    Event event;
    // Bounded draining keeps keyboard and rendering responsive under load.
    for (int i=0; i<16 && xQueueReceive(events_,&event,0)==pdTRUE; ++i) {
        if (event.kind==Kind::Port) {
            ++ports_.checked;
            if (event.success) {
                ++ports_.found;
                if (ports_.count<ports_.ports.size()) ports_.ports[ports_.count++]=event.value;
            }
        } else if (event.kind==Kind::PortsEnd) {
            ports_.phase=event.success ? ScanPhase::Failed : cancel_.load() ? ScanPhase::Cancelled : ScanPhase::Complete;
        } else if (event.handle==ping_ && ping_) {
            if (event.kind==Kind::Reply) { replied_=true; replyMs_=event.value; }
            else {
                esp_ping_delete_session(ping_); ping_=nullptr; stopping_=false;
                if (range_.phase==ScanPhase::Failed) continue;
                if (cancel_.load()) { range_.phase=ScanPhase::Cancelled; continue; }
                ++range_.checked;
                if (replied_) {
                    ++range_.found;
                    range_.hosts[range_.count++]={range_.current,replyMs_};
                }
                if (range_.current==lastIp_) range_.phase=ScanPhase::Complete;
                else { ++range_.current; if (!nextPing()) range_.phase=ScanPhase::Failed; }
            }
        }
    }
}
} // namespace portalis
