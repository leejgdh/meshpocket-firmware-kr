#pragma once

#include "configuration.h"

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "graphics/niche/Drivers/Backlight/LatchingBacklight.h"
#include "graphics/niche/Drivers/EInk/GDEY0154D67.h"
#include "graphics/niche/Inputs/TwoButton.h"

// ============================================================================
// InkHUD2 - New Architecture (experimental)
// ============================================================================
#ifdef USE_INKHUD2

#include "graphics/niche/InkHUD2/InkHUD2.h"
#include "graphics/niche/InkHUD2/Drivers/EInkAdapter.h"
#include "graphics/niche/InkHUD2/Events.h"
#include "graphics/niche/Fonts/CJK/UnifiedFont18px.h"
#include "modules/PositionModule.h"
#include "mesh/Channels.h"
#include "mesh/generated/meshtastic/channel.pb.h"
#include <functional>
#include "graphics/niche/InkHUD2/Core/Settings.h"
#include "graphics/niche/InkHUD2/Modules/BatteryModule.h"
#include "graphics/niche/InkHUD2/Modules/BootModule.h"
#include "graphics/niche/InkHUD2/Modules/MenuModule.h"
#include "graphics/niche/InkHUD2/Modules/MessageModule.h"
#include "graphics/niche/InkHUD2/Modules/NodeListModule.h"
#include "graphics/niche/InkHUD2/Modules/NotificationModule.h"
#include "graphics/niche/InkHUD2/Modules/MapModule.h"

// Static module instances
static InkHUD2::BatteryModule* batteryModule = nullptr;
static InkHUD2::NotificationModule* notificationModule = nullptr;
static InkHUD2::BootModule* bootModule = nullptr;
static InkHUD2::MenuModule* menuModule = nullptr;
static InkHUD2::MessageModule* messageModule = nullptr;
static InkHUD2::NodeListModule* nodeListModule = nullptr;
static InkHUD2::MapModule* mapModule = nullptr;
static InkHUD2::Events* inkhud2Events = nullptr;

// Menu state
static bool gpsEnabled = true;
static bool backlightOn = false;
static uint8_t rotationIndex = 0;  // User sees "0°" as current normal position
static const char* rotationOptions[] = {"0", "90", "180", "270"};

// Alerts state - notifications enabled per channel (index 0-7 = channels, 8 = DM)
static bool alertsEnabled[9] = {true, true, true, true, true, true, true, true, true};

// Menu actions (initialized in setupNicheGraphics)
static std::function<void()> actionPing;
static std::function<void()> actionBackup;
static std::function<void()> actionShutDown;
static std::function<void()> onHidePINChange;

// onChange callbacks for Screen submenu
static std::function<void()> onBacklightChange;
static std::function<void()> onRotationChange;

// Menu items (initialized in setupNicheGraphics)
static InkHUD2::MenuItem* mainMenuItems = nullptr;
static const uint8_t MENU_ITEM_COUNT = 6;

// Screen submenu
static InkHUD2::MenuItem* screenSubMenu = nullptr;
static const uint8_t SCREEN_SUBMENU_COUNT = 3;

// System submenu
static InkHUD2::MenuItem* systemSubMenu = nullptr;
static const uint8_t SYSTEM_SUBMENU_COUNT = 4;

// Alerts submenu (dynamically sized based on configured channels)
static InkHUD2::MenuItem* alertsSubMenu = nullptr;
static uint8_t alertsSubMenuCount = 0;
static std::function<void()>* alertsOnChange = nullptr;  // Array of callbacks
static char** alertsChannelNames = nullptr;  // Channel names for menu labels

// Firmware version string (stringify the macro)
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

// Double-click handled in InkHUD2::onInput

void setupNicheGraphics()
{
    using namespace NicheGraphics;

    Serial.println(F("[NicheGfx] setupNicheGraphics() start"));

    Serial.println(F("[NicheGfx] SPI1.begin()..."));
    SPI1.begin();
    Serial.println(F("[NicheGfx] SPI1 ready"));

    // Initialize e-ink driver
    Serial.println(F("[NicheGfx] creating GDEY0154D67..."));
    Drivers::EInk *driver = new Drivers::GDEY0154D67;
    Serial.println(F("[NicheGfx] calling driver->begin()..."));
    driver->begin(&SPI1, PIN_EINK_DC, PIN_EINK_CS, PIN_EINK_BUSY, PIN_EINK_RES);
    Serial.println(F("[NicheGfx] driver ready"));

    // Create adapter for InkHUD2
    Serial.println(F("[NicheGfx] creating EInkAdapter..."));
    static InkHUD2::EInkAdapter* adapter = new InkHUD2::EInkAdapter(driver);
    Serial.println(F("[NicheGfx] adapter ready"));

    // Initialize InkHUD2
    Serial.println(F("[NicheGfx] getting InkHUD2 instance..."));
    InkHUD2::InkHUD2& hud = InkHUD2::InkHUD2::instance();
    Serial.println(F("[NicheGfx] calling hud.init()..."));
    if (!hud.init(adapter, &NicheGraphics::UnifiedFont18px)) {
        Serial.println(F("[NicheGfx] ERROR: hud.init() failed!"));
        return;
    }
    Serial.println(F("[NicheGfx] hud initialized"));

    // Create modules
    Serial.println(F("[NicheGfx] creating modules..."));
    batteryModule = new InkHUD2::BatteryModule();
    notificationModule = new InkHUD2::NotificationModule();
    bootModule = new InkHUD2::BootModule();
    menuModule = new InkHUD2::MenuModule();
    messageModule = new InkHUD2::MessageModule();
    nodeListModule = new InkHUD2::NodeListModule();
    mapModule = new InkHUD2::MapModule();

    // Set up menu actions
    actionPing = []() {
        Serial.println(F("[Menu] Ping - broadcasting position"));
        // Send position broadcast
        if (positionModule) {
            positionModule->sendOurPosition();
        }
        // Show confirmation alert
        menuModule->showAlert("Position broadcast sent to mesh network.");
    };
    actionBackup = []() {
        Serial.println(F("[Menu] Manual Backup (golden snapshot)"));
        // Create USER backup - survives auto-backup corruption
        nodeDB->backupUserPreferences();
        menuModule->showAlert("Golden backup saved!");
    };
    actionShutDown = []() {
        Serial.println(F("[Menu] Shut Down"));
        // Show shutdown screen and trigger shutdown
        menuModule->startShutdown();
    };

    onHidePINChange = []() {
        Serial.print(F("[Menu] Hide PIN: "));
        Serial.println(config.bluetooth.hide_pin ? "Yes" : "No");
        // Save config immediately so setting persists
        nodeDB->saveToDisk();
    };

    // Define onChange callbacks for Screen submenu
    onBacklightChange = []() {
        Drivers::LatchingBacklight* bl = Drivers::LatchingBacklight::getInstance();
        if (backlightOn) {
            bl->latch();  // Turn on and keep on
        } else {
            bl->off();
        }
        Serial.print(F("[Menu] Backlight: "));
        Serial.println(backlightOn ? "ON" : "OFF");
    };

    onRotationChange = []() {
        // Map user-friendly degrees to buffer rotation
        // User "0°" = current normal = buffer rotation 3
        // Formula: bufferRotation = (userIndex + 3) % 4
        uint8_t bufferRotation = (rotationIndex + 3) % 4;
        InkHUD2::InkHUD2::instance().setRotation(bufferRotation);
        Serial.print(F("[Menu] Rotation: "));
        Serial.print(rotationOptions[rotationIndex]);
        Serial.print(F(" (buffer: "));
        Serial.print(bufferRotation);
        Serial.println(F(")"));
    };

    // Create Screen submenu (Back first for focus on entry)
    screenSubMenu = new InkHUD2::MenuItem[SCREEN_SUBMENU_COUNT];

    screenSubMenu[0].label = "< Back";
    screenSubMenu[0].type = InkHUD2::MenuItemType::BACK;

    screenSubMenu[1].label = "Backlight";
    screenSubMenu[1].type = InkHUD2::MenuItemType::TOGGLE;
    screenSubMenu[1].toggleValue = &backlightOn;
    screenSubMenu[1].onChange = &onBacklightChange;

    screenSubMenu[2].label = "Rotation";
    screenSubMenu[2].type = InkHUD2::MenuItemType::VALUE;
    screenSubMenu[2].value.options = rotationOptions;
    screenSubMenu[2].value.currentIndex = &rotationIndex;
    screenSubMenu[2].value.optionCount = 4;
    screenSubMenu[2].onChange = &onRotationChange;

    // Create System submenu (Back first)
    systemSubMenu = new InkHUD2::MenuItem[SYSTEM_SUBMENU_COUNT];

    systemSubMenu[0].label = "< Back";
    systemSubMenu[0].type = InkHUD2::MenuItemType::BACK;

    systemSubMenu[1].label = "Hide PIN";
    systemSubMenu[1].type = InkHUD2::MenuItemType::TOGGLE;
    systemSubMenu[1].toggleValue = InkHUD2::Settings::instance().hidePINPtr();
    systemSubMenu[1].onChange = &onHidePINChange;

    systemSubMenu[2].label = "Backup";
    systemSubMenu[2].type = InkHUD2::MenuItemType::ACTION;
    systemSubMenu[2].action = &actionBackup;

    systemSubMenu[3].label = "Shut Down";
    systemSubMenu[3].type = InkHUD2::MenuItemType::ACTION;
    systemSubMenu[3].action = &actionShutDown;

    // Count actually configured channels (not DISABLED)
    uint8_t maxChannels = channels.getNumChannels();
    uint8_t configuredCount = 0;
    uint8_t configuredChannels[8];  // Store indices of configured channels

    for (uint8_t i = 0; i < maxChannels && configuredCount < 8; i++) {
        // Only include channels that are not disabled
        auto& ch = channels.getByIndex(i);
        if (ch.role != meshtastic_Channel_Role_DISABLED) {
            configuredChannels[configuredCount++] = i;
            Serial.print(F("[NicheGfx] Channel "));
            Serial.print(i);
            Serial.print(F(" enabled: "));
            Serial.println(channels.getName(i));
        }
    }

    Serial.print(F("[NicheGfx] Configured channels: "));
    Serial.println(configuredCount);

    // Create Alerts submenu: Back + configured channels + DM
    alertsSubMenuCount = configuredCount + 2;  // Back + channels + DM

    alertsSubMenu = new InkHUD2::MenuItem[alertsSubMenuCount];
    alertsOnChange = new std::function<void()>[alertsSubMenuCount];
    alertsChannelNames = new char*[configuredCount];

    // Back first (index 0)
    alertsSubMenu[0].label = "< Back";
    alertsSubMenu[0].type = InkHUD2::MenuItemType::BACK;

    // Add channel toggles (starting at index 1)
    for (uint8_t i = 0; i < configuredCount; i++) {
        uint8_t chIdx = configuredChannels[i];

        // Copy channel name
        const char* name = channels.getName(chIdx);
        alertsChannelNames[i] = new char[16];
        strncpy(alertsChannelNames[i], name, 15);
        alertsChannelNames[i][15] = '\0';

        uint8_t menuIdx = i + 1;  // Offset by 1 for Back
        alertsSubMenu[menuIdx].label = alertsChannelNames[i];
        alertsSubMenu[menuIdx].type = InkHUD2::MenuItemType::TOGGLE;
        alertsSubMenu[menuIdx].toggleValue = &alertsEnabled[chIdx];  // Use actual channel index

        // Create onChange callback
        alertsOnChange[menuIdx] = [chIdx]() {
            Serial.print(F("[Menu] Alerts for ch"));
            Serial.print(chIdx);
            Serial.print(F(": "));
            Serial.println(alertsEnabled[chIdx] ? "ON" : "OFF");
        };
        alertsSubMenu[menuIdx].onChange = &alertsOnChange[menuIdx];
    }

    // Add DM toggle at the end
    uint8_t dmIdx = configuredCount + 1;
    alertsSubMenu[dmIdx].label = "DM";
    alertsSubMenu[dmIdx].type = InkHUD2::MenuItemType::TOGGLE;
    alertsSubMenu[dmIdx].toggleValue = &alertsEnabled[8];  // DM at index 8
    alertsOnChange[dmIdx] = []() {
        Serial.print(F("[Menu] Alerts for DM: "));
        Serial.println(alertsEnabled[8] ? "ON" : "OFF");
    };
    alertsSubMenu[dmIdx].onChange = &alertsOnChange[dmIdx];

    // Create main menu items
    // Order: GPS, Ping, Alerts, Screen, System, Exit
    mainMenuItems = new InkHUD2::MenuItem[MENU_ITEM_COUNT];

    mainMenuItems[0].label = "GPS";
    mainMenuItems[0].type = InkHUD2::MenuItemType::TOGGLE;
    mainMenuItems[0].toggleValue = &gpsEnabled;

    mainMenuItems[1].label = "Ping";
    mainMenuItems[1].type = InkHUD2::MenuItemType::ACTION;
    mainMenuItems[1].action = &actionPing;

    mainMenuItems[2].label = "Alerts";
    mainMenuItems[2].type = InkHUD2::MenuItemType::SUBMENU;
    mainMenuItems[2].submenu = alertsSubMenu;
    mainMenuItems[2].submenuCount = alertsSubMenuCount;

    mainMenuItems[3].label = "Screen";
    mainMenuItems[3].type = InkHUD2::MenuItemType::SUBMENU;
    mainMenuItems[3].submenu = screenSubMenu;
    mainMenuItems[3].submenuCount = SCREEN_SUBMENU_COUNT;

    mainMenuItems[4].label = "System";
    mainMenuItems[4].type = InkHUD2::MenuItemType::SUBMENU;
    mainMenuItems[4].submenu = systemSubMenu;
    mainMenuItems[4].submenuCount = SYSTEM_SUBMENU_COUNT;

    mainMenuItems[5].label = "Exit";
    mainMenuItems[5].type = InkHUD2::MenuItemType::BACK;

    // Configure menu
    menuModule->setRootMenu(mainMenuItems, MENU_ITEM_COUNT);
    menuModule->setFirmwareVersion(STRINGIFY(APP_VERSION_SHORT));

    // Link NodeListModule to MenuModule
    nodeListModule->setMenuModule(menuModule);

    // Link MapModule to MenuModule (for showing alerts)
    mapModule->setMenuModule(menuModule);

    // Configure message channels: DM first, then configured channels
    messageModule->addDMChannel();  // DM first (no chat style)
    for (uint8_t i = 0; i < configuredCount; i++) {
        uint8_t chIdx = configuredChannels[i];
        messageModule->addChannel(chIdx, alertsChannelNames[i], true);  // chat style
    }
    // Set own node number for outgoing message detection
    messageModule->setMyNodeNum(nodeDB->getNodeNum());
    Serial.println(F("[NicheGfx] modules created"));

    // Register system modules (overlays)
    Serial.println(F("[NicheGfx] registering system modules..."));
    hud.addSystemModule(batteryModule);
    hud.addSystemModule(notificationModule);
    hud.addSystemModule(bootModule);
    hud.addSystemModule(menuModule);

    // Register user modules (NodeList first - shown after boot)
    Serial.println(F("[NicheGfx] registering user modules..."));
    hud.addModule(nodeListModule);
    hud.addModule(messageModule);
    hud.addModule(mapModule);

    // Configure
    Serial.println(F("[NicheGfx] configuring..."));
    hud.setSlotCount(1);
    batteryModule->setLevel(100);

    // Configure boot module
    Serial.println(F("[NicheGfx] setting boot state..."));
    bootModule->setState(InkHUD2::BootState::LOGO);
    hud.requestFullRefresh();

    // Start event system (connects to Meshtastic message, power, and node systems)
    Serial.println(F("[NicheGfx] starting events..."));
    inkhud2Events = new InkHUD2::Events();
    inkhud2Events->begin(messageModule, batteryModule, nodeListModule, mapModule);

    // Backlight
    Serial.println(F("[NicheGfx] setting up backlight..."));
    Drivers::LatchingBacklight *backlight = Drivers::LatchingBacklight::getInstance();
    backlight->setPin(PIN_EINK_BL);

    // Button input
    Serial.println(F("[NicheGfx] setting up buttons..."));
    Inputs::TwoButton *buttons = Inputs::TwoButton::getInstance();

    buttons->setWiring(0, Inputs::TwoButton::getUserButtonPin());
    buttons->setTiming(0, 75, 400);  // 400ms for longpress (channel switch)
    buttons->setHandlerShortPress(0, []() {
        Serial.println(F("[BTN] SHORT PRESS"));
        // Short press = switch between modules
        InkHUD2::InkHUD2::instance().onInput(InkHUD2::Input::SELECT);
    });
    buttons->setHandlerLongPress(0, []() {
        Serial.println(F("[BTN] LONG PRESS"));
        // Long press = switch channels within Messages
        InkHUD2::InkHUD2::instance().onInput(InkHUD2::Input::BACK);
    });

    buttons->setWiring(1, PIN_BUTTON_TOUCH);
    buttons->setTiming(1, 50, 5000);
    buttons->setHandlerDown(1, [backlight]() { backlight->peek(); });
    buttons->setHandlerLongPress(1, [backlight]() { backlight->latch(); });
    buttons->setHandlerShortPress(1, [backlight]() { backlight->off(); });

    buttons->start();

    // Force initial render - update() checks flags and renders
    Serial.println(F("[NicheGfx] forcing initial render..."));
    hud.update();
    Serial.println(F("[NicheGfx] initial render done"));

    Serial.println(F("[NicheGfx] setupNicheGraphics() COMPLETE"));
}

// ============================================================================
// InkHUD (Original Architecture)
// ============================================================================
#else

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
    // [CJK] UNIFIED mode: 100% bitmap font rendering (Noto Sans JP Regular)
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
