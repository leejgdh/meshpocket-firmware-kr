#pragma once

#include "configuration.h"

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "graphics/niche/Drivers/EInk/LCMEN2R13ECC1.h"
#include "graphics/niche/Inputs/TwoButton.h"

// ============================================================================
// InkHUD2 - Korean (Hangul) UI
// ============================================================================
#ifdef USE_INKHUD2

#include "graphics/niche/InkHUD2/Setup.h"

void setupNicheGraphics()
{
    using namespace NicheGraphics;

    Serial.println(F("[NicheGfx] setupNicheGraphics() start"));

    SPI1.begin();

    Drivers::EInk *driver = new Drivers::LCMEN2R13ECC1;
    driver->begin(&SPI1, PIN_EINK_DC, PIN_EINK_CS, PIN_EINK_BUSY, PIN_EINK_RES);

    InkHUD2::Config config;
    config.hasBacklight = false;
    config.mainButtonPin = Inputs::TwoButton::getUserButtonPin();
    config.mainButtonDebounce = 75;
    config.mainButtonLongPress = 500;
    config.hasAuxButton = false;
    config.defaultRotation = 2;

    InkHUD2::setup(driver, config);

    Serial.println(F("[NicheGfx] setupNicheGraphics() complete"));
}

// ============================================================================
// InkHUD (Original) - kept for fallback / non-Korean builds
// ============================================================================
#else

#include "graphics/niche/InkHUD/InkHUD.h"

#include "graphics/niche/InkHUD/Applets/User/AllMessage/AllMessageApplet.h"
#include "graphics/niche/InkHUD/Applets/User/DM/DMApplet.h"
#include "graphics/niche/InkHUD/Applets/User/FavoritesMap/FavoritesMapApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Heard/HeardApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Positions/PositionsApplet.h"
#include "graphics/niche/InkHUD/Applets/User/RecentsList/RecentsListApplet.h"
#include "graphics/niche/InkHUD/Applets/User/ThreadedMessage/ThreadedMessageApplet.h"

void setupNicheGraphics()
{
    using namespace NicheGraphics;

    SPI1.begin();

    Drivers::EInk *driver = new Drivers::LCMEN2R13ECC1;
    driver->begin(&SPI1, PIN_EINK_DC, PIN_EINK_CS, PIN_EINK_BUSY, PIN_EINK_RES);

    InkHUD::InkHUD *inkhud = InkHUD::InkHUD::getInstance();
    inkhud->setDriver(driver);
    inkhud->setDisplayResilience(10, 1.5);

    InkHUD::Applet::fontLarge = FREESANS_12PT_WIN1253;
    InkHUD::Applet::fontMedium = FREESANS_9PT_WIN1253;
    InkHUD::Applet::fontSmall = FREESANS_6PT_WIN1253;

    inkhud->persistence->settings.userTiles.maxCount = 2;
    inkhud->persistence->settings.rotation = 3;
    inkhud->persistence->settings.userTiles.count = 1;
    inkhud->persistence->settings.optionalMenuItems.nextTile = true;

    inkhud->addApplet("All Messages", new InkHUD::AllMessageApplet, true, true);
    inkhud->addApplet("DMs", new InkHUD::DMApplet);
    inkhud->addApplet("Channel 0", new InkHUD::ThreadedMessageApplet(0));
    inkhud->addApplet("Channel 1", new InkHUD::ThreadedMessageApplet(1));
    inkhud->addApplet("Positions", new InkHUD::PositionsApplet, true);
    inkhud->addApplet("Favorites Map", new InkHUD::FavoritesMapApplet);
    inkhud->addApplet("Recents List", new InkHUD::RecentsListApplet);
    inkhud->addApplet("Heard", new InkHUD::HeardApplet, true, false, 0);

    inkhud->begin();

    Inputs::TwoButton *buttons = Inputs::TwoButton::getInstance();
    buttons->setWiring(0, Inputs::TwoButton::getUserButtonPin());
    buttons->setHandlerShortPress(0, [inkhud]() { inkhud->shortpress(); });
    buttons->setHandlerLongPress(0, [inkhud]() { inkhud->longpress(); });
    buttons->start();
}

#endif // USE_INKHUD2

#endif // MESHTASTIC_INCLUDE_NICHE_GRAPHICS
