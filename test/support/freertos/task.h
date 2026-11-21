#pragma once
#include "FreeRTOS.h"
inline void (*fakeTask)(void*)=nullptr;
inline void* fakeTaskArg=nullptr;
inline bool fakeTaskFail=false;
inline int xTaskCreate(void (*task)(void*),const char*,int,void* arg,int,void*) {
    if (fakeTaskFail) return 0;
    fakeTask=task;fakeTaskArg=arg;return pdPASS;
}
inline void vTaskDelete(void*) {}
inline void runFakeTask() { auto fn=fakeTask;auto arg=fakeTaskArg;fakeTask=nullptr;fakeTaskArg=nullptr;fn(arg); }

inline void (*fakeTaskTick)()=nullptr;
inline void vTaskDelay(uint32_t) { if (fakeTaskTick) fakeTaskTick(); }
