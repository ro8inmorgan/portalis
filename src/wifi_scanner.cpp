#include "wifi_scanner.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <cstdio>

namespace portalis {
namespace {
const char* securityLabel(wifi_auth_mode_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA1/2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "ENT";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/3";
        default: return "SECURE";
    }
}
}
bool WifiScanner::start() {
    if (active_) return true;
    // A separate mode: never change channels underneath the running portal.
    if (WiFi.getMode() & WIFI_MODE_AP) return false;
    snapshot_ = {};
    WiFi.setAutoReconnect(false);
    if (!WiFi.mode(WIFI_STA)) return false;
    WiFi.disconnect(false, false);
    active_ = true;
    beginScan();
    return true;
}
void WifiScanner::beginScan() {
    WiFi.scanDelete();
    // Async, hidden APs included, passive (no directed/broadcast probe requests).
    const int result = WiFi.scanNetworks(true, true, true, 180);
    scanStarted_ = millis();
    snapshot_.scanning = result == WIFI_SCAN_RUNNING || result >= 0;
    snapshot_.failed = !snapshot_.scanning;
    nextAt_ = millis() + 5000;
}
void WifiScanner::refresh() {
    if (active_ && !snapshot_.scanning) beginScan();
}
void WifiScanner::poll() {
    if (!active_) return;
    const uint32_t now = millis();
    if (!snapshot_.scanning) {
        if (static_cast<int32_t>(now - nextAt_) >= 0) beginScan();
        return;
    }
    const int result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING && static_cast<uint32_t>(now - scanStarted_) < 10000) return;
    if (result < 0) {
        esp_wifi_scan_stop();
        WiFi.scanDelete();
        // Discard stale dots on failure rather than showing old proximity as live.
        snapshot_.count = 0; snapshot_.total = 0;
        snapshot_.scanning = false; snapshot_.failed = true; ++snapshot_.generation;
        nextAt_ = now + 5000;
        return;
    }
    RadarSnapshot next;
    next.total = static_cast<uint16_t>(result);
    next.updatedAt = now;
    next.generation = snapshot_.generation + 1;
    for (int i = 0; i < result; ++i) {
        // The pinned Arduino core exposes the raw record accessor without the
        // uint8_t index limit of SSID()/RSSI(), so dense scans are not truncated.
        const auto* info = static_cast<const wifi_ap_record_t*>(WiFiScanClass::getScanInfoByIndex(i));
        if (!info || info->rssi >= 0) continue;
        RadarAccessPoint ap;
        ap.id = radarId(info->bssid);
        ap.rssi = info->rssi;
        ap.channel = info->primary;
        std::snprintf(ap.security, sizeof(ap.security), "%s", securityLabel(info->authmode));
        size_t n = 0;
        for (; n < 32 && info->ssid[n]; ++n) {
            const uint8_t c = info->ssid[n];
            ap.ssid[n] = c >= 32 && c <= 126 ? static_cast<char>(c) : '?';
        }
        ap.ssid[n] = '\0';
        if (!n) std::snprintf(ap.ssid, sizeof(ap.ssid), "Hidden %04X", static_cast<unsigned>(ap.id & 0xffff));
        next.add(ap);
    }
    WiFi.scanDelete(); // Free the driver's scan result array between sweeps.
    snapshot_ = next;
    nextAt_ = now + 5000;
}
void WifiScanner::stop() {
    if (!active_) return;
    active_ = false;
    esp_wifi_scan_stop();
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
    snapshot_ = {};
}
} // namespace portalis
