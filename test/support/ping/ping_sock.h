#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include "esp_err.h"
constexpr int ESP_PING_PROF_TIMEGAP=1, ESP_PING_COUNT_INFINITE=0;
struct ip_addr_t { uint32_t value=0; };
struct FakePing;
using esp_ping_handle_t=FakePing*;
struct esp_ping_callbacks_t {
    void* cb_args=nullptr;
    void (*on_ping_success)(esp_ping_handle_t,void*)=nullptr;
    void (*on_ping_timeout)(esp_ping_handle_t,void*)=nullptr;
    void (*on_ping_end)(esp_ping_handle_t,void*)=nullptr;
};
struct esp_ping_config_t { uint32_t count=5, interval_ms=1000,timeout_ms=1000;ip_addr_t target_addr; };
#define ESP_PING_DEFAULT_CONFIG() esp_ping_config_t{}
struct FakePing { esp_ping_callbacks_t cb;bool started=false,stopped=false,deleted=false;uint32_t ms=12; };
inline std::vector<FakePing*> fakePings;
inline int fakeDeletedRunning=0;
inline int esp_ping_new_session(const esp_ping_config_t*,const esp_ping_callbacks_t* cb,esp_ping_handle_t* out) {
    *out=new FakePing{*cb};fakePings.push_back(*out);return ESP_OK;
}
inline int esp_ping_start(esp_ping_handle_t p) { p->started=true;return ESP_OK; }
inline int esp_ping_stop(esp_ping_handle_t p) { p->stopped=true;return ESP_OK; }
inline int esp_ping_delete_session(esp_ping_handle_t p) { if (p->started&&!p->stopped) ++fakeDeletedRunning;p->deleted=true;return ESP_OK; }
inline int esp_ping_get_profile(esp_ping_handle_t p,int,void* data,size_t) { std::memcpy(data,&p->ms,sizeof(p->ms));return ESP_OK; }
inline void fakePingReply(FakePing* p) { p->cb.on_ping_success(p,p->cb.cb_args); }
inline void fakePingTimeout(FakePing* p) { p->cb.on_ping_timeout(p,p->cb.cb_args); }
inline void fakePingEnd(FakePing* p) { p->stopped=true;p->cb.on_ping_end(p,p->cb.cb_args); }
