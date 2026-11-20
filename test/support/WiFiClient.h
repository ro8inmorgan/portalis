#pragma once
#include "WiFi.h"
inline bool fakeTcpSuccess=true;
struct WiFiClient { int connect(IPAddress,int,int) { return fakeTcpSuccess; } void stop() {} };
