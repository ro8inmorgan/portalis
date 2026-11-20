#pragma once
#include <array>
#include <string>
namespace portalis {
struct WifiProfile { std::string ssid, password; };
class WifiProfiles {
public:
    std::array<WifiProfile,5> entries;
    size_t count=0;
    int find(const std::string& ssid) const;
    bool remember(const std::string& ssid, const std::string& password);
    bool forget(const std::string& ssid);
    bool load(const std::string& json);
    std::string serialize() const;
};
} // namespace portalis
