#include "packet_capture.h"
#include <WiFi.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <freertos/task.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace portalis {
PacketCapture* PacketCapture::instance_=nullptr;
void PacketCapture::receive(void* buffer,wifi_promiscuous_pkt_type_t type) {
    auto* self=instance_;
    if(!self || !buffer) return;
    self->callbacks_.fetch_add(1);
    if(self->receiving_.load() && (type==WIFI_PKT_MGMT || type==WIFI_PKT_DATA || type==WIFI_PKT_CTRL)) {
        const auto* wifi=static_cast<const wifi_promiscuous_pkt_t*>(buffer);
        if(wifi->rx_ctrl.rx_state==0 && wifi->rx_ctrl.sig_len>=4) {
            auto& packet=self->incoming_; // Driver invokes callbacks serially from its Wi-Fi task.
            packet.at=static_cast<uint64_t>(esp_timer_get_time())-self->started_;
            packet.original=wifi->rx_ctrl.sig_len;packet.length=std::min(size_t(packet.original),pcap::payloadLimit);
            packet.channel=wifi->rx_ctrl.channel;packet.rssi=wifi->rx_ctrl.rssi;
            std::memcpy(packet.data,wifi->payload,packet.length);
            if(xQueueSend(self->queue_,&packet,0)!=pdTRUE) self->dropped_.fetch_add(1);
        }
    }
    self->callbacks_.fetch_sub(1);
}
void PacketCapture::disableReceiver() {
    receiving_.store(false);
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
}
void PacketCapture::release() {
    file_.close();
    if(queue_) { vQueueDelete(queue_);queue_=nullptr; }
    WiFi.mode(WIFI_OFF);pending_=false;
}
bool PacketCapture::start(uint8_t channel) {
    if(busy()) return false;
    status_={};status_.channel=channel;
    auto fail=[&](const char* message) { status_.phase=CapturePhase::Failed;std::snprintf(status_.message,sizeof(status_.message),"%s",message);return false; };
    // Capture is an exclusive radio mode. Never stop an existing AP or station.
    if(WiFi.getMode()!=WIFI_OFF) return fail("Stop the other Wi-Fi tool first");
    if(channel<1 || channel>13) return fail("Choose a channel from 1 to 13");
    if(SD.cardType()==CARD_NONE || (!SD.exists("/captures") && !SD.mkdir("/captures"))) return fail("SD card unavailable");
    bool fresh=false;
    for(int attempt=0;attempt<16;++attempt) {
        std::snprintf(status_.path,sizeof(status_.path),"/captures/cap-%08lx.pcap",static_cast<unsigned long>(esp_random()));
        if(!SD.exists(status_.path)) { fresh=true;break; }
    }
    if(!fresh) return fail("Could not create a unique file");
    file_=SD.open(status_.path,FILE_APPEND);
    const auto header=pcap::header();
    if(!file_ || file_.write(header.data(),header.size())!=header.size()) {
        file_.close();SD.remove(status_.path);return fail("Could not write capture header");
    }
    queue_=xQueueCreate(8,sizeof(Packet));
    if(!queue_) { file_.close();SD.remove(status_.path);return fail("Not enough memory for capture"); }
    WiFi.persistent(false);WiFi.setAutoReconnect(false);
    bool radio=WiFi.mode(WIFI_STA);
    WiFi.disconnect(false,false);
    wifi_country_t country{};
    if(radio && esp_wifi_get_country(&country)==ESP_OK)
        radio=channel>=country.schan && channel<country.schan+country.nchan;
    wifi_promiscuous_filter_t filter{};
    filter.filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_CTRL|WIFI_PROMIS_FILTER_MASK_DATA;
    wifi_promiscuous_filter_t control{};control.filter_mask=WIFI_PROMIS_CTRL_FILTER_MASK_ALL;
    radio=radio && esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE)==ESP_OK &&
        esp_wifi_set_promiscuous_filter(&filter)==ESP_OK &&
        esp_wifi_set_promiscuous_ctrl_filter(&control)==ESP_OK;
    // All counters and the queue are ready before enabling the callback.
    stop_=false;failed_=false;limit_=false;packets_=0;dropped_=0;bytes_=24;
    started_=esp_timer_get_time();instance_=this;
    if(radio) radio=esp_wifi_set_promiscuous_rx_cb(receive)==ESP_OK;
    receiving_=radio;
    if(radio) radio=esp_wifi_set_promiscuous(true)==ESP_OK;
    if(!radio) {
        disableReceiver();while(callbacks_.load()) vTaskDelay(1);
        release();SD.remove(status_.path);return fail("Channel or radio unavailable");
    }
    pending_=true;worker_=true;status_.phase=CapturePhase::Capturing;
    if(xTaskCreate(writeTask,"pcap-sd",4096,this,1,nullptr)!=pdPASS) {
        worker_=false;disableReceiver();while(callbacks_.load()) vTaskDelay(1);
        release();SD.remove(status_.path);return fail("Not enough memory for writer");
    }
    return true;
}
void PacketCapture::writeTask(void* arg) {
    auto* self=static_cast<PacketCapture*>(arg);
    bool disabled=false;
    uint64_t flushed=esp_timer_get_time();
    while(true) {
        if(self->stop_.load() && !disabled) {
            self->disableReceiver();while(self->callbacks_.load()) vTaskDelay(1);
            disabled=true;
        }
        if(xQueueReceive(self->queue_,&self->writing_,pdMS_TO_TICKS(20))==pdTRUE) {
            const auto& packet=self->writing_;
            const size_t length=16+pcap::radioSize+packet.length;
            if(self->bytes_.load()+length>pcap::fileLimit) { self->limit_=true;break; }
            pcap::recordHeader(self->record_.data(),packet.at,packet.length,packet.original,packet.channel,packet.rssi);
            std::memcpy(self->record_.data()+16+pcap::radioSize,packet.data,packet.length);
            if(self->file_.write(self->record_.data(),length)!=length) { self->failed_=true;break; }
            self->bytes_.fetch_add(length);self->packets_.fetch_add(1);
        } else if(disabled) break;
        const uint64_t now=esp_timer_get_time();
        if(now-flushed>=1000000) { self->file_.flush();flushed=now; }
        vTaskDelay(1);
    }
    if(!disabled) { self->disableReceiver();while(self->callbacks_.load()) vTaskDelay(1); }
    // Report buffered packets not saved when a size limit or SD error ends capture.
    while(xQueueReceive(self->queue_,&self->writing_,0)==pdTRUE) self->dropped_.fetch_add(1);
    if(self->failed_ || self->limit_) self->dropped_.fetch_add(1);
    self->file_.flush();self->file_.close();self->worker_=false;
    vTaskDelete(nullptr);
}
void PacketCapture::stop() {
    if(pending_) { stop_=true;status_.phase=CapturePhase::Saving; }
}
void PacketCapture::poll() {
    if(!pending_) return;
    const bool done=!worker_.load(); // Acquire completion before reading the final counters.
    status_.packets=packets_;status_.dropped=dropped_;status_.bytes=bytes_;
    status_.seconds=(static_cast<uint64_t>(esp_timer_get_time())-started_)/1000000;
    if(done) {
        const bool failed=failed_.load();release();status_.phase=failed ? CapturePhase::Failed : CapturePhase::Saved;
        std::snprintf(status_.message,sizeof(status_.message),"%s",failed ? "SD write failed; file may be partial" : limit_.load() ? "32 MB limit reached; saved" : "Capture saved");
    }
}
} // namespace portalis
