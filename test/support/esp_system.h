#pragma once
#include <cstdint>
inline uint32_t fakeRandom=0;
inline uint32_t esp_random() { return ++fakeRandom; }
