/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
// Example usage of InkHUD2
// This file demonstrates how to integrate InkHUD2 into firmware

#include "InkHUD2.h"
#include "graphics/niche/Fonts/CJK/UnifiedFont18px.h"
#include "Modules/BatteryModule.h"
#include "Modules/BootModule.h"
#include "Modules/MenuModule.h"
#include "Modules/MessageModule.h"
#include "Modules/NodeListModule.h"

namespace InkHUD2 {

// Example display driver (implement per-platform)
class ExampleDriver : public DisplayDriver {
public:
    bool init() override {
        // Initialize e-ink display hardware
        return true;
    }

    uint16_t width() const override { return 200; }
    uint16_t height() const override { return 200; }

    void update(const uint8_t* data, bool fullRefresh) override {
        // Transfer buffer to display
        // fullRefresh = true means do a full refresh (slower, no ghosting)
        // fullRefresh = false means partial refresh (faster, may ghost)
    }

    void sleep() override {
        // Put display in low-power mode
    }

    void wake() override {
        // Wake display from sleep
    }
};

// Example initialization
void example_init() {
    static ExampleDriver driver;

    // Initialize InkHUD2
    InkHUD2& hud = InkHUD2::instance();
    if (!hud.init(&driver, &NicheGraphics::UnifiedFont18px)) {
        // Handle initialization failure
        return;
    }

    // Create and register modules
    static BatteryModule batteryModule;
    static BootModule bootModule;
    static MessageModule messageModule;
    static NodeListModule nodeListModule;
    static MenuModule menuModule;

    // System modules (overlays)
    hud.addSystemModule(&batteryModule);
    hud.addSystemModule(&bootModule);
    hud.addSystemModule(&menuModule);

    // Regular modules (slot-based)
    hud.addModule(&messageModule);
    hud.addModule(&nodeListModule);

    // Configure initial state
    hud.setSlotCount(1);  // Single full-screen slot

    // Connect to Meshtastic event system (for message, power, and node status updates)
    hud.connectEvents(&messageModule, &batteryModule, &nodeListModule);

    // Show boot screen
    bootModule.setState(BootState::LOGO);
}

// Example main loop integration
void example_update() {
    InkHUD2& hud = InkHUD2::instance();

    // Call update in main loop - handles rate limiting internally
    hud.update();
}

// Example event handling (called from firmware)
void example_on_message(uint32_t from, uint32_t to, uint8_t channel, const char* text) {
    InkHUD2& hud = InkHUD2::instance();

    // Dispatch event to modules
    hud.onEvent(Event::messageReceived(from, to, channel));
}

void example_on_button_press() {
    InkHUD2& hud = InkHUD2::instance();

    // Dispatch input
    hud.onInput(Input::SELECT);
}

void example_show_notification(const char* text) {
    // Notifications are handled by the message module
    // when alerts are enabled for a channel
    (void)text;  // Unused
}

// Example menu setup - MenuItem initialization requires explicit construction
// due to union members. See MenuModule.h for MenuItem structure.
// In real usage, create MenuItem instances with proper initialization.

} // namespace InkHUD2
