#include "probe_test.h"
#include <WiFi.h>
#include <esp_system.h>

namespace portalis {
ProbeTest* ProbeTest::instance_ = nullptr;
void ProbeTest::securityHeaders(const std::string& nonce) {
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
    server_.sendHeader("Referrer-Policy", "no-referrer");
    const std::string scripts = nonce.empty() ? "script-src 'none'" : "script-src 'nonce-" + nonce + "'";
    const std::string policy = "default-src 'none'; style-src 'unsafe-inline'; img-src data:; form-action 'self'; connect-src 'self'; base-uri 'none'; frame-ancestors 'none'; " + scripts;
    server_.sendHeader("Content-Security-Policy", policy.c_str());
}
void ProbeTest::sendPage() {
    if (!active_ || !schedule_.active()) { server_.send(503, "text/plain", "Test is stopped."); return; }
    char ticket[17];
    snprintf(ticket, sizeof(ticket), "%08lx%08lx", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    tickets_.issue(ticket, millis());
    securityHeaders(ticket);
    const std::string html = renderProbeNotice(config_.noticeHtml, ticket);
    server_.send(200, "text/html; charset=utf-8", html.c_str());
}
void ProbeTest::init(std::function<bool(Event, const std::string&)> onFeedback) {
    instance_ = this;
    onFeedback_ = std::move(onFeedback);
    server_.on("/", HTTP_GET, [this]() { sendPage(); });
    server_.on("/confirm", HTTP_POST, [this]() {
        securityHeaders();
        if (!active_ || !schedule_.active()) { server_.send(503, "text/plain", "Test is stopped."); return; }
        const String body = server_.arg("plain");
        if (body.length() > kSubmissionLimit) { server_.send(413, "text/plain", "Feedback is too large."); return; }
        Submission submission;
        if (!parseSubmission(std::string(body.c_str(), body.length()), submission)) {
            server_.send(400, "text/plain", "Invalid submission (maximum 64 named fields)."); return;
        }
        if (!tickets_.valid(submission.ticket, millis())) {
            server_.send(409, "text/plain", "This form expired or was already submitted. Reload to try again."); return;
        }
        if (!onFeedback_ || !onFeedback_({session_, static_cast<uint32_t>(millis() - started_) / 1000}, submission.fields)) {
            server_.send(507, "text/plain", "Could not save feedback. Check the SD card and retry."); return;
        }
        tickets_.consume(submission.ticket, millis());
        ++confirmations_;
        server_.send(200, "text/plain", "Saved");
    });
    server_.on("/done", HTTP_GET, [this]() {
        securityHeaders();
        server_.send(200, "text/html; charset=utf-8", "<!doctype html><html lang=\"en\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Feedback saved</title><body><h1>Feedback saved</h1><p>Thank you. You can disconnect now.</p></body></html>");
        finishPending_ = true;
        finishAt_ = millis() + 2000;
    });
    server_.onNotFound([this]() {
        if (server_.method() != HTTP_GET) { securityHeaders(); server_.send(405, "text/plain", "Method not allowed"); return; }
        // Previous catch-all: sendPage();
        securityHeaders();
        server_.sendHeader("Location", "http://172.0.0.1/");
        server_.send(302, "text/plain", "Open the local test feedback page.");
    });
}
void ProbeTest::receive(void* packet, wifi_promiscuous_pkt_type_t type) {
    auto* self = instance_;
    if (!self || !self->receiving_.load(std::memory_order_relaxed) || type != WIFI_PKT_MGMT) return;
    const auto* wifi = static_cast<const wifi_promiscuous_pkt_t*>(packet);
    ProbeCandidate candidate;
    if (!parseProbe(wifi->payload, wifi->rx_ctrl.sig_len, candidate)) return;
    candidate.channel = wifi->rx_ctrl.channel;
    candidate.capturedAt = millis();
    xQueueSend(self->queue_, &candidate, 0); // Wi-Fi task callback, not an ISR.
}
void ProbeTest::disableReceiver() {
    receiving_.store(false, std::memory_order_relaxed);
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    if (queue_) xQueueReset(queue_);
}
bool ProbeTest::listen() {
    if (!WiFi.mode(WIFI_STA)) return false;
    WiFi.setAutoReconnect(false); WiFi.disconnect(false, false);
    wifi_country_t country;
    if (esp_wifi_get_country(&country) == ESP_OK && country.schan >= 1 && country.nchan && country.schan + country.nchan <= 15) {
        firstChannel_ = country.schan; channelCount_ = country.nchan;
    }
    status_.channel = firstChannel_;
    wifi_promiscuous_filter_t filter{}; filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    if (esp_wifi_set_promiscuous_filter(&filter) != ESP_OK ||
        esp_wifi_set_channel(status_.channel, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
        esp_wifi_set_promiscuous_rx_cb(receive) != ESP_OK) return false;
    xQueueReset(queue_);
    receiving_.store(true, std::memory_order_relaxed);
    if (esp_wifi_set_promiscuous(true) != ESP_OK) { receiving_.store(false); return false; }
    status_.phase = ProbePhase::Listening; hopped_ = millis(); return true;
}
bool ProbeTest::start(const ProbeConfig& config) {
    if (active_ || config.noticePath.empty() || config.noticeHtml.empty() || !config.permits("test") ||
        config.timeoutSeconds < ProbeConfig::minTimeoutSeconds || config.timeoutSeconds > ProbeConfig::maxTimeoutSeconds ||
        config.cooldownSeconds < 60 || config.cooldownSeconds > 86400 || (WiFi.getMode() & WIFI_MODE_AP)) return false;
    if (!queue_) queue_ = xQueueCreate(8, sizeof(ProbeCandidate));
    if (!queue_) return false;
    tickets_.clear();
    session_ = esp_random();
    if (!session_) session_ = 1;
    config_ = config; schedule_.reset(); status_ = {}; confirmations_ = 0; started_ = millis();
    if (!listen()) { disableReceiver(); WiFi.mode(WIFI_OFF); status_.phase = ProbePhase::Failed; return false; }
    active_ = true; return true;
}
void ProbeTest::beginAttempt(const ProbeCandidate& candidate) {
    const uint32_t now = millis();
    if (!schedule_.begin(config_, candidate, now)) return;
    disableReceiver();
    ++status_.attempts; std::strncpy(status_.ssid, candidate.ssid, 32);
    status_.ssid[32] = '\0'; status_.channel = candidate.channel;
    const IPAddress ip(172, 0, 0, 1), mask(255, 255, 255, 0);
    if (!WiFi.mode(WIFI_AP) || !WiFi.softAPConfig(ip, ip, mask) ||
        // Previously DNS was also started here: || !dns_.start(53, "*", ip)
        !WiFi.softAP(candidate.ssid, nullptr, candidate.channel, 0, 4)) {
        endAttempt(); return;
    }
    dns_.setErrorReplyCode(DNSReplyCode::NoError);
    // Previous failure cleanup left the schedule active with the radio off:
    // if (!dns_.start(53, "*", ip)) { WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF); return; }
    if (!dns_.start(53, "*", ip)) { endAttempt(); return; }
    tickets_.clear();
    server_.begin(); status_.phase = ProbePhase::Waiting;
    status_.remaining = schedule_.remaining(millis());
}
void ProbeTest::endAttempt() {
    const bool connected = schedule_.joined();
    // Only SSID, elapsed test time and aggregate connection result are retained.
    status_.record(schedule_.target().ssid, static_cast<uint32_t>(millis() - started_) / 1000, connected);
    schedule_.end(millis()); status_.remaining = 0;
    tickets_.clear();
    server_.stop(); dns_.stop(); WiFi.softAPdisconnect(true);
    if (!listen()) { disableReceiver(); WiFi.mode(WIFI_OFF); active_ = false; status_.phase = ProbePhase::Failed; }
}
uint8_t ProbeTest::clients() const {
    return active_ && schedule_.active() ? WiFi.softAPgetStationNum() : 0;
}
void ProbeTest::poll() {
    if (!active_) return;
    const uint32_t now = millis();
    if (schedule_.active()) {
        dns_.processNextRequest(); server_.handleClient();
        if (finishPending_ && static_cast<int32_t>(millis() - finishAt_) >= 0) {
            finishPending_ = false;
            endAttempt();
            return;
        }
        if (WiFi.softAPgetStationNum() && schedule_.observeConnection(now)) {
            ++status_.joins; status_.phase = ProbePhase::Notice;
        }
        status_.remaining = schedule_.remaining(now);
        if (schedule_.finished(now)) endAttempt();
        return;
    }
    // Process a bounded number per loop, so keyboard/animation remain responsive.
    ProbeCandidate candidate;
    for (int i = 0; i < 8 && xQueueReceive(queue_, &candidate, 0) == pdTRUE; ++i) {
        if (static_cast<uint32_t>(now - candidate.capturedAt) > 2000) continue;
        beginAttempt(candidate);
        if (schedule_.active() || !active_) return;
    }
    if (static_cast<uint32_t>(now - hopped_) >= 220) {
        status_.channel = firstChannel_ + ((status_.channel - firstChannel_ + 1) % channelCount_);
        esp_wifi_set_channel(status_.channel, WIFI_SECOND_CHAN_NONE); hopped_ = now;
    }
}
void ProbeTest::stop() {
    if (!active_) return;
    disableReceiver();
    if (schedule_.active()) {
        status_.record(schedule_.target().ssid, static_cast<uint32_t>(millis() - started_) / 1000, schedule_.joined());
        schedule_.end(millis());
    }
    tickets_.clear();
    server_.stop(); dns_.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
    active_ = false; status_.phase = ProbePhase::Idle; status_.remaining = 0;
}
} // namespace portalis
