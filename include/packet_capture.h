#pragma once
#include "packet_capture_model.h"
#include <atomic>
#include <SD.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace portalis {
class PacketCapture {
    struct Packet {
        uint64_t at=0;
        uint16_t length=0,original=0;
        uint8_t channel=1;
        int8_t rssi=0;
        uint8_t data[pcap::payloadLimit]{};
    };
    static PacketCapture* instance_;
    QueueHandle_t queue_=nullptr;
    File file_;
    Packet incoming_,writing_; // Never allocate frame-sized buffers on Wi-Fi/UI stacks.
    std::array<uint8_t,16+pcap::snapLength> record_{};
    std::atomic<bool> receiving_{false},worker_{false},stop_{false},failed_{false},limit_{false};
    std::atomic<unsigned> callbacks_{0};
    std::atomic<uint32_t> packets_{0},dropped_{0},bytes_{0};
    uint64_t started_=0;
    CaptureStatus status_;
    bool pending_=false;
    static void receive(void*,wifi_promiscuous_pkt_type_t);
    static void writeTask(void*);
    void disableReceiver();
    void release();
public:
    bool start(uint8_t channel);
    void stop();
    void poll();
    bool busy() const { return pending_; }
    const CaptureStatus& status() const { return status_; }
};
} // namespace portalis
