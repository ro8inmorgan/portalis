#pragma once
#include "WiFi.h"
#include <deque>
#include <algorithm>
inline bool fakeTcpSuccess=true,fakeTcpConnected=false;
inline std::deque<uint8_t> fakeTcpRead;
inline std::string fakeTcpWritten;
inline size_t fakeTcpWriteLimit=static_cast<size_t>(-1);
struct WiFiClient {
    int connect(IPAddress,int,int) {fakeTcpConnected=fakeTcpSuccess;return fakeTcpSuccess;}
    int connect(const char*,int,int) {fakeTcpConnected=fakeTcpSuccess;return fakeTcpSuccess;}
    void stop() {fakeTcpConnected=false;}
    void setTimeout(uint32_t) {}
    bool connected() {return fakeTcpConnected;}
    int available() {return fakeTcpRead.size();}
    int read() {if(fakeTcpRead.empty())return -1;int result=fakeTcpRead.front();fakeTcpRead.pop_front();return result;}
    size_t write(const uint8_t* bytes,size_t size) {size=std::min(size,fakeTcpWriteLimit);fakeTcpWritten.append(reinterpret_cast<const char*>(bytes),size);return size;}
};
