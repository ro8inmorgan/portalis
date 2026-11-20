#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstdio>
struct IPAddress {
    uint32_t value=0;
    IPAddress()=default;
    IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):value((a<<24)|(b<<16)|(c<<8)|d) {}
    explicit operator uint32_t() const { return value; }
    std::string toString() const { char out[16];std::snprintf(out,sizeof(out),"%u.%u.%u.%u",value>>24,(value>>16)&255,(value>>8)&255,value&255);return out; }
};
constexpr int WL_CONNECTED=3, WL_DISCONNECTED=6;
inline bool fakeDnsSuccess=true;
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
    int connectionStatus=WL_DISCONNECTED;
    bool persistentStorage=true;
    IPAddress address, gateway{192,168,1,1};
    void persistent(bool value) { persistentStorage=value; }
    void disconnect(bool off, bool) { connectionStatus=WL_DISCONNECTED;if (off) currentMode=WIFI_OFF; }
    int begin(const char*,const char*,int,const uint8_t*) { return connectionStatus; }
    int status() const { return connectionStatus; }
    IPAddress localIP() const { return address; }
    IPAddress gatewayIP() const { return gateway; }
    IPAddress dnsIP() const { return gateway; }
    int hostByName(const char*,IPAddress& ip) { ip=IPAddress(93,184,216,34);return fakeDnsSuccess; }
    void scanDelete() { ++deletes; fakeResults.clear(); if (scanState >= 0) scanState = WIFI_SCAN_FAILED; }
    int scanNetworks(bool async, bool hidden, bool passive, int) {
        ++starts; lastAsync = async; lastHidden = hidden; lastPassive = passive;
        return scanState = scanSuccess ? WIFI_SCAN_RUNNING : WIFI_SCAN_FAILED;
    }
    int scanComplete() { return scanState; }
};
inline FakeWiFi WiFi;
