#pragma once
#include "workbench_model.h"
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace portalis {
class TcpTerminal {
    struct Event {TcpPhase phase=TcpPhase::Idle;uint16_t size=0;char bytes[192]={};};
    struct Input {uint8_t size=0;char bytes[8]={};};
    QueueHandle_t events_=nullptr,input_=nullptr;
    std::atomic<bool> worker_{false},cancel_{false};bool pending_=false;
    TcpConfig config_;TcpStatus status_;TerminalBuffer terminal_;
    static void task(void*);bool emit(const Event&);
public:
    bool start(const TcpConfig&);
    bool send(const char*,size_t);
    void stop();void poll();
    bool busy() const {return pending_;}
    const TcpStatus& status() const {return status_;}
    const TerminalBuffer& terminal() const {return terminal_;}
};
} // namespace portalis
