#pragma once

#include "graphics/niche/Drivers/EInk/EInk.h"

namespace InkHUD2 {

/**
 * Device-specific configuration for InkHUD2
 * Set these values in nicheGraphics.h before calling setup()
 */
struct Config {
    // Backlight pin (-1 = no backlight)
    int8_t backlightPin = -1;

    // Button pins (-1 = not used)
    int8_t mainButtonPin = -1;      // Primary button (short=select, long=back)
    int8_t auxButtonPin = -1;       // Auxiliary button (e.g., touch for backlight)

    // Button timings (ms)
    uint16_t mainButtonDebounce = 75;
    uint16_t mainButtonLongPress = 400;
    uint16_t auxButtonDebounce = 50;
    uint16_t auxButtonLongPress = 5000;

    // Display rotation (0-3)
    uint8_t defaultRotation = 3;

    // Features
    bool hasBacklight = false;
    bool hasAuxButton = false;
};

/**
 * Initialize InkHUD2 with the given e-ink driver and configuration.
 * This sets up all modules, menus, buttons, and events.
 *
 * @param driver Initialized e-ink driver
 * @param config Device-specific configuration
 */
void setup(NicheGraphics::Drivers::EInk* driver, const Config& config);

} // namespace InkHUD2
