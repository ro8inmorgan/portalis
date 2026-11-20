#pragma once
#include "radar.h"
namespace portalis {
class WifiScanner {
    RadarSnapshot snapshot_;
    bool active_ = false;
    uint32_t nextAt_ = 0, scanStarted_ = 0;
    void beginScan();
public:
    bool start();
    void stop();
    void refresh();
    void poll();
    const RadarSnapshot& snapshot() const { return snapshot_; }
};
} // namespace portalis
