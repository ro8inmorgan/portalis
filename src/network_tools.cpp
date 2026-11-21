#include "network_tools.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_wifi.h>
#include <lwip/inet.h>
#include <freertos/task.h>

namespace portalis {
void NetworkTools::reply(esp_ping_handle_t handle, void* arg) {
    auto* self=static_cast<NetworkTools*>(arg);
    Event event{EventKind::Reply,handle};
    esp_ping_get_profile(handle,ESP_PING_PROF_TIMEGAP,&event.ms,sizeof(event.ms));
    xQueueSend(self->events_,&event,portMAX_DELAY);
}
void NetworkTools::timeout(esp_ping_handle_t handle, void* arg) {
    Event event{EventKind::Timeout,handle};
    xQueueSend(static_cast<NetworkTools*>(arg)->events_,&event,portMAX_DELAY);
}
void NetworkTools::end(esp_ping_handle_t handle, void* arg) {
    Event event{EventKind::End,handle};
    xQueueSend(static_cast<NetworkTools*>(arg)->events_,&event,portMAX_DELAY);
}
void NetworkTools::checksTask(void* arg) {
    auto* self=static_cast<NetworkTools*>(arg);
    Event event{EventKind::Checks}; IPAddress resolved;
    // The pinned core's DNS resolver may wait up to 15 seconds. Keep it off the
    // UI task; ownership stays with this mode until the worker finishes.
    event.dns=WiFi.hostByName("example.com",resolved)==1 && static_cast<uint32_t>(resolved)!=0;
    if (!self->cancel_.load()) {
        WiFiClient client;
        event.internet=client.connect(IPAddress(1,1,1,1),443,2000)==1;
        client.stop();
    }
    xQueueSend(self->events_,&event,portMAX_DELAY);
    self->workerRunning_.store(false);
    vTaskDelete(nullptr);
}
bool NetworkTools::startPing(const char* target, bool continuous) {
    if (ping_) {
        stopPing(); pendingTarget_=target; pendingContinuous_=continuous; return true;
    }
    esp_ping_config_t config=ESP_PING_DEFAULT_CONFIG();
    if (!ipaddr_aton(target,&config.target_addr)) return false;
    config.count=continuous ? ESP_PING_COUNT_INFINITE : 3;
    config.interval_ms=1000; config.timeout_ms=1000;
    esp_ping_callbacks_t callbacks{};
    callbacks.cb_args=this; callbacks.on_ping_success=reply; callbacks.on_ping_timeout=timeout; callbacks.on_ping_end=end;
    if (esp_ping_new_session(&config,&callbacks,&ping_)!=ESP_OK) { ping_=nullptr; return false; }
    if (esp_ping_start(ping_)!=ESP_OK) { esp_ping_delete_session(ping_); ping_=nullptr; return false; }
    return true;
}
void NetworkTools::stopPing() {
    pendingTarget_.clear();
    if (!ping_ || pingStopping_) return;
    // Wait for on_ping_end before deleting/replacing the session. This also
    // prevents late callbacks from being mistaken for a newly allocated handle.
    pingStopping_=true; esp_ping_stop(ping_);
}
bool NetworkTools::begin(const RadarAccessPoint& ap, const std::string& password, bool pingMode, bool connectionOnly) {
    if (busy() || workerRunning_.load() || (WiFi.getMode() & WIFI_MODE_AP) || !ap.connectable) return false;
    if (!events_) events_=xQueueCreate(16,sizeof(Event));
    if (!events_) return false;
    xQueueReset(events_);
    status_={}; status_.pingMode=pingMode;
    connectionOnly_=connectionOnly;
    std::snprintf(status_.ssid,sizeof(status_.ssid),"%s",ap.ssid);
    WiFi.persistent(false); WiFi.setAutoReconnect(false);
    if (!WiFi.mode(WIFI_STA)) return false;
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK) { WiFi.mode(WIFI_OFF); return false; }
    WiFi.disconnect(false,true);
    uint8_t bssid[6]; for (int i=0;i<6;++i) bssid[i]=(ap.id >> ((5-i)*8)) & 255;
    cancel_.store(false);
    status_.phase=NetworkPhase::Connecting; started_=millis();
    WiFi.begin(ap.rawSsid, password.empty() ? nullptr : password.c_str(), ap.channel, bssid);
    return true;
}
void NetworkTools::startChecks() {
    status_.phase=NetworkPhase::Checking;
    status_.gatewayCheck=status_.dns=status_.internet=CheckState::Pending;
    status_.ping={}; cancel_.store(false);
    if (!validPingTarget(status_.gateway) || !startPing(status_.gateway,false)) status_.gatewayCheck=CheckState::Error;
    workerRunning_.store(true);
    if (xTaskCreate(checksTask,"portalis-check",4096,this,1,nullptr)!=pdPASS) {
        workerRunning_.store(false); status_.dns=status_.internet=CheckState::Error;
    }
}
void NetworkTools::finishStop() {
    WiFi.disconnect(true,true); WiFi.mode(WIFI_OFF);
    status_.phase=NetworkPhase::Idle;
}
void NetworkTools::stop() {
    if (!busy()) return;
    cancel_.store(true); stopPing(); status_.phase=NetworkPhase::Stopping;
    if (!workerRunning_.load() && !ping_) finishStop();
}
void NetworkTools::poll() {
    if (!events_) return;
    Event event;
    while (xQueueReceive(events_,&event,0)==pdTRUE) {
        if (event.kind==EventKind::Checks) {
            if (status_.phase==NetworkPhase::Checking && !cancel_.load()) {
                status_.dns=event.dns ? CheckState::Pass : CheckState::NoReply;
                status_.internet=event.internet ? CheckState::Pass : CheckState::NoReply;
            }
        } else if (event.handle && event.handle==ping_) {
            if (event.kind==EventKind::Reply || event.kind==EventKind::Timeout) {
                if (pingStopping_) continue;
                status_.ping.add(event.kind==EventKind::Reply,event.ms);
                if (!status_.pingMode && event.kind==EventKind::Reply) status_.gatewayCheck=CheckState::Pass;
            } else if (event.kind==EventKind::End) {
                esp_ping_delete_session(ping_); ping_=nullptr; pingStopping_=false;
                if (!pendingTarget_.empty() && !cancel_.load()) {
                    const auto target=pendingTarget_; pendingTarget_.clear();
                    if (!startPing(target.c_str(),pendingContinuous_)) status_.paused=true;
                }
                if (!status_.pingMode && status_.gatewayCheck==CheckState::Pending) status_.gatewayCheck=CheckState::NoReply;
            }
        }
    }
    if (status_.phase==NetworkPhase::Stopping) {
        if (!workerRunning_.load() && !ping_) finishStop();
        return;
    }
    if (status_.phase==NetworkPhase::Connecting) {
        if (WiFi.status()==WL_CONNECTED && static_cast<uint32_t>(WiFi.localIP())!=0) {
            status_.dhcp=CheckState::Pass;
            std::snprintf(status_.address,sizeof(status_.address),"%s",WiFi.localIP().toString().c_str());
            std::snprintf(status_.gateway,sizeof(status_.gateway),"%s",WiFi.gatewayIP().toString().c_str());
            std::snprintf(status_.dnsServer,sizeof(status_.dnsServer),"%s",WiFi.dnsIP().toString().c_str());
            if (connectionOnly_) status_.phase=NetworkPhase::Ready;
            else if (status_.pingMode) {
                status_.phase=NetworkPhase::Ready;
                if (!setTarget(status_.gateway)) {
                    status_.paused=true;
                    std::snprintf(status_.message,sizeof(status_.message),"No gateway. T: set target");
                }
            } else startChecks();
        } else if (static_cast<uint32_t>(millis()-started_)>=30000) {
            status_.phase=NetworkPhase::Failed; status_.dhcp=CheckState::NoReply;
            std::snprintf(status_.message,sizeof(status_.message),"Join/DHCP timed out");
        }
    } else if (status_.phase==NetworkPhase::Ready || status_.phase==NetworkPhase::Checking) {
        if (WiFi.status()!=WL_CONNECTED) {
            stopPing(); cancel_.store(true); status_.phase=NetworkPhase::Failed;
            std::snprintf(status_.message,sizeof(status_.message),"Wi-Fi disconnected");
        } else if (status_.phase==NetworkPhase::Checking && !workerRunning_.load() && !ping_)
            status_.phase=NetworkPhase::Ready;
    }
}
bool NetworkTools::setTarget(const std::string& target) {
    if (!status_.pingMode || status_.phase!=NetworkPhase::Ready || !validPingTarget(target)) return false;
    if (!startPing(target.c_str(),true)) return false;
    std::snprintf(status_.target,sizeof(status_.target),"%s",target.c_str());
    status_.message[0]='\0'; status_.paused=false; status_.ping={}; return true;
}
void NetworkTools::togglePause() {
    if (!status_.pingMode || status_.phase!=NetworkPhase::Ready) return;
    if (!status_.paused) { stopPing(); status_.paused=true; }
    else if (validPingTarget(status_.target) && startPing(status_.target,true)) status_.paused=false;
}
void NetworkTools::refresh() {
    if (connectionOnly_ || status_.phase!=NetworkPhase::Ready || workerRunning_.load()) return;
    if (!status_.pingMode) startChecks();
    else status_.ping={};
}
} // namespace portalis
