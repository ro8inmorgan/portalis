#include "portal.h"
#include <WiFi.h>
#include <esp_system.h>

namespace portalis {
void Portal::securityHeaders(const std::string& nonce) {
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
    server_.sendHeader("Referrer-Policy", "no-referrer");
    const std::string scriptPolicy = nonce.empty() ? "script-src 'none'" : "script-src 'nonce-" + nonce + "'";
    const std::string policy = "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; base-uri 'none'; frame-ancestors 'none'; connect-src 'self'; img-src data:; " + scriptPolicy;
    server_.sendHeader("Content-Security-Policy", policy.c_str());
}
void Portal::sendPage(bool done) {
    if (!running_) { server_.send(503, "text/plain", "Portal is stopped."); return; }
    char ticket[17] = {};
    if (!done) {
        snprintf(ticket, sizeof(ticket), "%08lx%08lx", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
        tickets_.issue(ticket, millis());
    }
    const std::string html = renderPage(page_, ticket, done);
    securityHeaders(ticket);
    server_.send(200, "text/html; charset=utf-8", html.c_str());
}
void Portal::init(std::function<bool(Event, const std::string&)> onEvent) {
    onEvent_ = onEvent;
    server_.on("/", HTTP_GET, [this]() { sendPage(false); });
    server_.on("/done", HTTP_GET, [this]() { sendPage(true); });
    server_.on("/confirm", HTTP_POST, [this]() {
        securityHeaders();
        if (!running_) { server_.send(503, "text/plain", "Portal is stopped."); return; }
        const String body = server_.arg("plain");
        if (body.length() > kSubmissionLimit) { server_.send(413, "text/plain", "Submission exceeds 16 KB."); return; }
        Submission submission;
        if (!parseSubmission(std::string(body.c_str(), body.length()), submission)) {
            server_.send(400, "text/plain", "Invalid submission (maximum 64 named fields)."); return;
        }
        if (!tickets_.valid(submission.ticket, millis())) {
            server_.send(409, "text/plain", "This page expired or was already submitted. Reload for a new form."); return;
        }
        if (!onEvent_ || !onEvent_({session_, elapsed()}, submission.fields)) {
            server_.send(507, "text/plain", "Could not save to SD. Check the card and log space, then retry."); return;
        }
        tickets_.consume(submission.ticket, millis());
        ++confirmations_;
        server_.send(200, "text/plain", "Saved");
    });
    server_.onNotFound([this]() {
        securityHeaders();
        if (server_.method() != HTTP_GET) { server_.send(405, "text/plain", "Method not allowed"); return; }
        // Captive-network probes and arbitrary HTTP hosts go to the local page.
        server_.sendHeader("Location", "http://172.0.0.1/"); server_.send(302, "text/plain", "Open the Portalis connection demo.");
    });
}
bool Portal::start(const std::string& ssid, const PageTemplate& page, uint32_t session) {
    if (running_ || !validSsid(ssid) || !validTemplate(page)) return false;
    const IPAddress ip(172, 0, 0, 1), mask(255, 255, 255, 0);
    if (!WiFi.mode(WIFI_AP) || !WiFi.softAPConfig(ip, ip, mask) || !WiFi.softAP(ssid.c_str(), nullptr, 1, 0, 4)) {
        WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF); return false;
    }
    dns_.setErrorReplyCode(DNSReplyCode::NoError);
    if (!dns_.start(53, "*", ip)) { WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF); return false; }
    page_ = page; session_ = session; confirmations_ = 0; started_ = millis(); stoppedSeconds_ = 0;
    tickets_.clear(); running_ = true; server_.begin();
    return true;
}
void Portal::stop() {
    if (!running_) return;
    stoppedSeconds_ = elapsed(); running_ = false;
    server_.stop(); dns_.stop(); tickets_.clear();
    WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
}
void Portal::poll() { if (running_) { dns_.processNextRequest(); server_.handleClient(); } }
uint8_t Portal::clients() const { return running_ ? WiFi.softAPgetStationNum() : 0; }
uint32_t Portal::elapsed() const { return running_ ? static_cast<uint32_t>(millis() - started_) / 1000 : stoppedSeconds_; }
} // namespace portalis
