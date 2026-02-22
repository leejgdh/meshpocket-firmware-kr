#pragma once

#include "configuration.h"

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "graphics/niche/Drivers/EInk/GDEY0154D67.h"
#include "graphics/niche/Inputs/TwoButton.h"

// ============================================================================
// InkHUD2 - New Architecture
// ============================================================================
#ifdef USE_INKHUD2

#include "graphics/niche/InkHUD2/Setup.h"

void setupNicheGraphics()
{
    using namespace NicheGraphics;

    Serial.println(F("[NicheGfx] setupNicheGraphics() start"));

    // Initialize SPI for e-ink
    SPI1.begin();

    // Initialize e-ink driver (device-specific)
    Drivers::EInk* driver = new Drivers::GDEY0154D67;
    driver->begin(&SPI1, PIN_EINK_DC, PIN_EINK_CS, PIN_EINK_BUSY, PIN_EINK_RES);

    // Configure InkHUD2 (device-specific settings)
    InkHUD2::Config config;
    config.backlightPin = PIN_EINK_BL;
    config.hasBacklight = true;
    config.mainButtonPin = Inputs::TwoButton::getUserButtonPin();
    config.mainButtonDebounce = 75;
    config.mainButtonLongPress = 400;
    config.auxButtonPin = PIN_BUTTON_TOUCH;
    config.hasAuxButton = true;
    config.auxButtonDebounce = 50;
    config.auxButtonLongPress = 5000;
    config.defaultRotation = 3;

    // Initialize InkHUD2 with common setup
    InkHUD2::setup(driver, config);

    Serial.println(F("[NicheGfx] setupNicheGraphics() complete"));
}

// ============================================================================
// InkHUD (Original Architecture)
// ============================================================================
#else

#include "graphics/niche/Drivers/Backlight/LatchingBacklight.h"
#include "graphics/niche/InkHUD/Applets/User/AllMessage/AllMessageApplet.h"
#include "graphics/niche/InkHUD/Applets/User/DM/DMApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Heard/HeardApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Positions/PositionsApplet.h"
#include "graphics/niche/InkHUD/Applets/User/RecentsList/RecentsListApplet.h"
#include "graphics/niche/InkHUD/Applets/User/ThreadedMessage/ThreadedMessageApplet.h"
#include "graphics/niche/InkHUD/InkHUD.h"

void setupNicheGraphics()
{
    using namespace NicheGraphics;

    SPI1.begin();

    Drivers::EInk *driver = new Drivers::GDEY0154D67;
    driver->begin(&SPI1, PIN_EINK_DC, PIN_EINK_CS, PIN_EINK_BUSY, PIN_EINK_RES);

    InkHUD::InkHUD *inkhud = InkHUD::InkHUD::getInstance();
    inkhud->setDriver(driver);
    inkhud->setDisplayResilience(20, 1.5);
    // [CJK] UNIFIED mode: 100% bitmap font rendering
    InkHUD::Applet::fontLarge = UNIFIED_12PT;
    InkHUD::Applet::fontMedium = UNIFIED_9PT;
    InkHUD::Applet::fontSmall = UNIFIED_6PT;
    inkhud->persistence->settings.userTiles.maxCount = 2;
    inkhud->persistence->settings.rotation = 3;
    inkhud->persistence->settings.optionalFeatures.batteryIcon = true;
    inkhud->persistence->settings.optionalMenuItems.backlight = true;

    Drivers::LatchingBacklight *backlight = Drivers::LatchingBacklight::getInstance();
    backlight->setPin(PIN_EINK_BL);

    inkhud->addApplet("All Messages", new InkHUD::AllMessageApplet, true, true);
    inkhud->addApplet("DMs", new InkHUD::DMApplet);
    inkhud->addApplet("Channel 0", new InkHUD::ThreadedMessageApplet(0));
    inkhud->addApplet("Channel 1", new InkHUD::ThreadedMessageApplet(1));
    inkhud->addApplet("Positions", new InkHUD::PositionsApplet, true);
    inkhud->addApplet("Recents List", new InkHUD::RecentsListApplet);
    inkhud->addApplet("Heard", new InkHUD::HeardApplet, true, false, 0);

    inkhud->begin();

    Inputs::TwoButton *buttons = Inputs::TwoButton::getInstance();

    buttons->setWiring(0, Inputs::TwoButton::getUserButtonPin());
    buttons->setTiming(0, 75, 500);
    buttons->setHandlerShortPress(0, [inkhud]() { inkhud->shortpress(); });
    buttons->setHandlerLongPress(0, [inkhud]() { inkhud->longpress(); });

    buttons->setWiring(1, PIN_BUTTON_TOUCH);
    buttons->setTiming(1, 50, 5000);
    buttons->setHandlerDown(1, [inkhud, backlight]() {
        backlight->peek();
        inkhud->persistence->settings.optionalMenuItems.backlight = false;
    });
    buttons->setHandlerLongPress(1, [backlight]() { backlight->latch(); });
    buttons->setHandlerShortPress(1, [backlight]() { backlight->off(); });

    buttons->start();
}

#endif // USE_INKHUD2

#endif // MESHTASTIC_INCLUDE_NICHE_GRAPHICS
