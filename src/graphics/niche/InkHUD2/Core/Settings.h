#pragma once

#include <cstdint>
#include "mesh/NodeDB.h"  // For extern config

namespace InkHUD2 {

// Global settings for InkHUD2
// Wrapper around Meshtastic config for type-safe access
class Settings {
public:
    static Settings& instance() {
        static Settings s;
        return s;
    }

    // PIN visibility on pairing screen (stored in config.bluetooth.hide_pin)
    bool getHidePIN() const { return config.bluetooth.hide_pin; }
    void setHidePIN(bool value) { config.bluetooth.hide_pin = value; }

    // Pointer for menu toggle binding
    bool* hidePINPtr() { return &config.bluetooth.hide_pin; }

    // Future settings can be added here as wrappers around config

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
};

} // namespace InkHUD2
