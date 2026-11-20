#pragma once
#include "model.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <functional>

namespace portalis {
class Portal {
    WebServer server_{80};
    DNSServer dns_;
    TicketGate tickets_;
    PageTemplate page_;
    bool running_ = false;
    uint32_t started_ = 0, session_ = 0, confirmations_ = 0, stoppedSeconds_ = 0;
    std::function<bool(Event, const std::string&)> onEvent_;
    void sendPage(bool done);
    void securityHeaders(const std::string& nonce = "");
public:
    void init(std::function<bool(Event, const std::string&)> onEvent);
    bool start(const std::string& ssid, const PageTemplate& page, uint32_t session);
    void stop();
    void poll();
    bool running() const { return running_; }
    uint8_t clients() const;
    uint32_t session() const { return session_; }
    uint32_t count() const { return confirmations_; }
    uint32_t elapsed() const;
};
} // namespace portalis
