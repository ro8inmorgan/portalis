#pragma once
#include "WiFi.h"
inline int esp_wifi_scan_stop() { WiFi.scanState = WIFI_SCAN_FAILED; return 0; }

constexpr int WIFI_STORAGE_RAM=0;
inline int esp_wifi_set_storage(int) { return 0; }
#include "esp_err.h"
constexpr int WIFI_SECOND_CHAN_NONE=0;
constexpr uint32_t WIFI_PROMIS_FILTER_MASK_MGMT=1,WIFI_PROMIS_FILTER_MASK_CTRL=2,WIFI_PROMIS_FILTER_MASK_DATA=4,WIFI_PROMIS_CTRL_FILTER_MASK_ALL=0xffff;
enum wifi_promiscuous_pkt_type_t { WIFI_PKT_MGMT,WIFI_PKT_CTRL,WIFI_PKT_DATA,WIFI_PKT_MISC };
struct wifi_pkt_rx_ctrl_t { uint16_t sig_len=0;uint8_t channel=1,rx_state=0;int8_t rssi=-40; };
struct wifi_promiscuous_pkt_t { wifi_pkt_rx_ctrl_t rx_ctrl;uint8_t payload[4096]{}; };
struct wifi_country_t { uint8_t schan=1,nchan=13; };
struct wifi_promiscuous_filter_t { uint32_t filter_mask; };
inline void (*fakePromiscuousCallback)(void*,wifi_promiscuous_pkt_type_t)=nullptr;
inline bool fakePromiscuous=false,fakeRadioFail=false;
inline int esp_wifi_get_country(wifi_country_t* country) { *country={};return ESP_OK; }
inline int esp_wifi_set_channel(uint8_t,int) { return fakeRadioFail ? -1 : ESP_OK; }
inline int esp_wifi_set_promiscuous_filter(const wifi_promiscuous_filter_t*) { return ESP_OK; }
inline int esp_wifi_set_promiscuous_ctrl_filter(const wifi_promiscuous_filter_t*) { return ESP_OK; }
inline int esp_wifi_set_promiscuous_rx_cb(void (*callback)(void*,wifi_promiscuous_pkt_type_t)) { fakePromiscuousCallback=callback;return ESP_OK; }
inline int esp_wifi_set_promiscuous(bool active) { fakePromiscuous=active;return ESP_OK; }
