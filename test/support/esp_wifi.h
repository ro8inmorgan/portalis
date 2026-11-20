#pragma once
#include "WiFi.h"
inline int esp_wifi_scan_stop() { WiFi.scanState = WIFI_SCAN_FAILED; return 0; }

constexpr int WIFI_STORAGE_RAM=0;
inline int esp_wifi_set_storage(int) { return 0; }
