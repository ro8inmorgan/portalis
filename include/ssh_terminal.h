#pragma once
#include "ssh_model.h"
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace portalis {
class SshTerminal {
    enum class Kind { State, Fingerprint, Output };
    struct Event { Kind kind; SshPhase phase=SshPhase::Idle; uint16_t length=0; char text[192]={}; };
    struct Input { uint8_t length=0; char bytes[8]={}; };
    QueueHandle_t events_=nullptr, input_=nullptr;
    std::atomic<bool> workerRunning_{false}, cancel_{false};
    std::atomic<int> trust_{0};
    SshConfig config_;
    SshStatus status_;
    TerminalBuffer terminal_;
    static void task(void*);
    bool emit(const Event&);
    void state(SshPhase,const char*);
public:
    bool start(const SshConfig&);
    void stop();
    void trustHost() { trust_.store(1); }
    bool send(const char* bytes,size_t size);
    void poll();
    bool busy() const;
    const SshStatus& status() const { return status_; }
    const TerminalBuffer& terminal() const { return terminal_; }
};
} // namespace portalis
