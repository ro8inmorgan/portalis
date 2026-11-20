#pragma once
#include <SD.h>
#include <string>

namespace portalis {
inline constexpr const char kDefaultPortalConfig[] = R"JSON({
  "ap_names": [
    "Portalis Demo",
    "Portalis Guest",
    "Portalis Workshop"
  ],
  "cooldown_seconds": 120,
  "notice_template": ""
}
)JSON";
// Create only a missing config. Existing settings (including malformed files)
// are never overwritten during a mount or reload.
inline bool ensurePortalConfig(std::string& error) {
    if (SD.exists("/portalis.json")) return true;
    File file = SD.open("/portalis.json", FILE_APPEND);
    if (!file) { error = "Cannot create /portalis.json on SD."; return false; }
    const size_t expected = sizeof(kDefaultPortalConfig) - 1;
    const size_t written = file.write(reinterpret_cast<const uint8_t*>(kDefaultPortalConfig), expected);
    file.flush();
    file.close();
    File check = SD.open("/portalis.json");
    std::string contents(expected, '\0');
    const bool valid = check && !check.isDirectory() && check.size() == expected &&
        check.readBytes(&contents[0], expected) == expected && contents == kDefaultPortalConfig;
    check.close();
    if (written != expected || !valid) {
        SD.remove("/portalis.json");
        error = "Could not save /portalis.json on SD."; return false;
    }
    return true;
}
} // namespace portalis
