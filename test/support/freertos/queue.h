#pragma once
#include "FreeRTOS.h"
#include <deque>
#include <vector>
#include <cstring>
struct FakeQueue { size_t limit, size;std::deque<std::vector<uint8_t>> items; };
using QueueHandle_t=FakeQueue*;
inline QueueHandle_t xQueueCreate(size_t count,size_t size) { return new FakeQueue{count,size,{}}; }
inline int xQueueSend(QueueHandle_t queue,const void* value,int) {
    if (queue->items.size()==queue->limit) return 0;
    const auto* bytes=static_cast<const uint8_t*>(value);queue->items.emplace_back(bytes,bytes+queue->size);return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t queue,void* value,int) {
    if (queue->items.empty()) return 0;
    std::memcpy(value,queue->items.front().data(),queue->size);queue->items.pop_front();return pdTRUE;
}
inline void xQueueReset(QueueHandle_t queue) { queue->items.clear(); }
inline void vQueueDelete(QueueHandle_t queue) { delete queue; }
