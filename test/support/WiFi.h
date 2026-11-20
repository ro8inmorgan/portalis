#pragma once
#include <cstdint>
#include <vector>
constexpr int WIFI_OFF = 0, WIFI_STA = 1, WIFI_MODE_AP = 2;
constexpr int WIFI_SCAN_RUNNING = -1, WIFI_SCAN_FAILED = -2;
enum wifi_auth_mode_t { WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK,
    WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA2_ENTERPRISE, WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK };
struct wifi_ap_record_t { uint8_t bssid[6] = {}; uint8_t ssid[33] = {}; uint8_t primary = 1; int8_t rssi = -50; wifi_auth_mode_t authmode = WIFI_AUTH_WPA2_PSK; };
inline uint32_t fakeMillis = 0;
inline uint32_t millis() { return fakeMillis; }
inline std::vector<wifi_ap_record_t> fakeResults;
struct WiFiScanClass { static void* getScanInfoByIndex(int i) { return &fakeResults.at(i); } };
struct FakeWiFi {
    int currentMode = 0, scanState = WIFI_SCAN_FAILED, starts = 0, deletes = 0;
    bool autoReconnect = true, modeSuccess = true, scanSuccess = true;
    bool lastAsync = false, lastHidden = false, lastPassive = false;
    int getMode() const { return currentMode; }
    bool mode(int m) { if (!modeSuccess) return false; currentMode = m; return true; }
    void setAutoReconnect(bool enabled) { autoReconnect = enabled; }
    void disconnect(bool, bool) {}
    void scanDelete() { ++deletes; fakeResults.clear(); if (scanState >= 0) scanState = WIFI_SCAN_FAILED; }
    int scanNetworks(bool async, bool hidden, bool passive, int) {
        ++starts; lastAsync = async; lastHidden = hidden; lastPassive = passive;
        return scanState = scanSuccess ? WIFI_SCAN_RUNNING : WIFI_SCAN_FAILED;
    }
    int scanComplete() { return scanState; }
};
inline FakeWiFi WiFi;
