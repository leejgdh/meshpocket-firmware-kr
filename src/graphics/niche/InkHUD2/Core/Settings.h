#pragma once

#include <cstdint>
#include "mesh/NodeDB.h"  // For extern config

namespace InkHUD2 {

// Global settings for InkHUD2
// Wrapper around Meshtastic config for type-safe access
// Some settings stored in Meshtastic config, others in separate file
class Settings {
public:
    static Settings& instance() {
        static Settings s;
        return s;
    }

    // Load/save InkHUD2-specific settings from/to file
    void load();
    void save();

    // PIN visibility on pairing screen (stored in config.bluetooth.hide_pin)
    bool getHidePIN() const { return config.bluetooth.hide_pin; }
    void setHidePIN(bool value) { config.bluetooth.hide_pin = value; }
    bool* hidePINPtr() { return &config.bluetooth.hide_pin; }

    // Screen rotation (0-3, stored in InkHUD2 settings file)
    uint8_t getRotation() const { return rotation; }
    void setRotation(uint8_t value) { rotation = value % 4; }
    uint8_t* rotationPtr() { return &rotation; }

private:
    Settings() { load(); }
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    // InkHUD2-specific settings (not in Meshtastic config)
    uint8_t rotation = 0;

    static constexpr const char* SETTINGS_FILE = "/prefs/inkhud2.dat";
};

} // namespace InkHUD2
