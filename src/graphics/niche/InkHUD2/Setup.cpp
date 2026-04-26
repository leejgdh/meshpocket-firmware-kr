#include "Setup.h"

#include "InkHUD2.h"
#include "Drivers/EInkAdapter.h"
#include "Events.h"
#include "Core/Settings.h"
#include "Modules/BatteryModule.h"
#include "Modules/BootModule.h"
#include "Modules/CannedMessageModule.h"
#include "Modules/MenuModule.h"
#include "Modules/MessageModule.h"
#include "Modules/NodeListModule.h"
#include "Modules/MapModule.h"

#include "graphics/niche/Drivers/Backlight/LatchingBacklight.h"
#include "graphics/niche/Inputs/TwoButton.h"
#include "graphics/niche/Fonts/CJK/UnifiedFont18px.h"

#include "modules/PositionModule.h"
#include "mesh/Channels.h"
#include "mesh/NodeDB.h"
#include "mesh/generated/meshtastic/channel.pb.h"
#include "mesh/generated/meshtastic/admin.pb.h"
#include "main.h"
#include "configuration.h"

#include <functional>

// Stringify macro for firmware version
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

namespace InkHUD2 {

// Static module instances
static BatteryModule* batteryModule = nullptr;
static BootModule* bootModule = nullptr;
static MenuModule* menuModule = nullptr;
static MessageModule* messageModule = nullptr;
static NodeListModule* nodeListModule = nullptr;
static MapModule* mapModule = nullptr;
static CannedMessageModule* cannedMessageModule = nullptr;
static Events* inkhud2Events = nullptr;

// Menu state
static bool gpsEnabled = true;
static bool backlightOn = false;
static const char* rotationOptions4[] = {"0", "90", "180", "270"};  // For square-ish screens
static const char* rotationOptions2[] = {"0", "180"};               // For elongated screens
static bool isElongatedScreen = false;  // Set in setup() based on aspect ratio

// Alerts state (index 0-7 = channels, 8 = DM)
static bool alertsEnabled[9] = {true, true, true, true, true, true, true, true, true};

// Menu callbacks
static std::function<void()> actionPing;
static std::function<void()> actionBackup;
static std::function<void()> actionRestore;
static std::function<void()> actionShutDown;
static std::function<void()> onHidePINChange;
static std::function<void()> onBacklightChange;
static std::function<void()> onRotationChange;

// Menu items
static MenuItem* mainMenuItems = nullptr;
static const uint8_t MENU_ITEM_COUNT = 6;

static MenuItem* screenSubMenu = nullptr;
static const uint8_t SCREEN_SUBMENU_COUNT = 3;

static MenuItem* systemSubMenu = nullptr;
static const uint8_t SYSTEM_SUBMENU_COUNT = 5;

static MenuItem* alertsSubMenu = nullptr;
static uint8_t alertsSubMenuCount = 0;
static std::function<void()>* alertsOnChange = nullptr;
static char** alertsChannelNames = nullptr;

// Store config for callbacks
static Config currentConfig;

void setup(NicheGraphics::Drivers::EInk* driver, const Config& config)
{
    using namespace NicheGraphics;

    currentConfig = config;

    Serial.println(F("[InkHUD2] setup() start"));

    // Create adapter
    static EInkAdapter* adapter = new EInkAdapter(driver);

    // Determine if screen is elongated (aspect ratio > 1.5)
    uint16_t maxDim = std::max(driver->width, driver->height);
    uint16_t minDim = std::min(driver->width, driver->height);
    isElongatedScreen = (minDim > 0) && (maxDim * 10 / minDim > 15);  // >1.5 ratio

    // Initialize InkHUD2 with standard 18px font
    // Apply saved rotation offset to default rotation
    uint8_t savedRotation = Settings::instance().getRotation();
    uint8_t initialRotation = (savedRotation + config.defaultRotation) % 4;

    InkHUD2& hud = InkHUD2::instance();
    if (!hud.init(adapter, &NicheGraphics::UnifiedFont18px, initialRotation, 1.0f)) {
        Serial.println(F("[InkHUD2] ERROR: init() failed!"));
        return;
    }

    // Create modules
    batteryModule = new BatteryModule();
    bootModule = new BootModule();
    menuModule = new MenuModule();
    messageModule = new MessageModule();
    nodeListModule = new NodeListModule();
    mapModule = new MapModule();
    cannedMessageModule = new CannedMessageModule();

    // Set up menu actions
    actionPing = []() {
        Serial.println(F("[Menu] Ping"));
        if (positionModule) {
            positionModule->sendOurPosition();
        }
        menuModule->showAlert("Position broadcast sent to mesh network.");
    };

    actionBackup = []() {
        Serial.println(F("[Menu] Backup"));
        nodeDB->backupUserPreferences();
        menuModule->showAlert("Golden backup saved!");
    };

    actionRestore = []() {
        Serial.println(F("[Menu] Restore"));
        menuModule->showAlert("Restoring...", true);
        InkHUD2::instance().requestFullRefresh();
        InkHUD2::instance().update();

        if (nodeDB->restorePreferences(meshtastic_AdminMessage_BackupLocation_FLASH)) {
            menuModule->showAlert("Restored! Rebooting...", true);
            InkHUD2::instance().requestFullRefresh();
            InkHUD2::instance().update();
            delay(1000);
            rebootAtMsec = millis() + 500;
        } else {
            menuModule->showAlert("No backup found!");
        }
    };

    actionShutDown = []() {
        Serial.println(F("[Menu] Shut Down"));
        menuModule->startShutdown();
    };

    onHidePINChange = []() {
        // Use global config (::config), not the local Config parameter
        bool isFixedPIN = (::config.bluetooth.mode == meshtastic_Config_BluetoothConfig_PairingMode_FIXED_PIN);

        if (::config.bluetooth.hide_pin) {
            if (!isFixedPIN) {
                ::config.bluetooth.hide_pin = false;
                menuModule->showAlert("Only for Fixed PIN mode");
                return;
            }
            menuModule->showAlert("Saving settings...", true);
            InkHUD2::instance().update();
            nodeDB->saveToDisk();
            menuModule->showAlert("PIN hidden. To disable: set Random PIN in app");
        } else {
            if (isFixedPIN) {
                ::config.bluetooth.hide_pin = true;
                menuModule->showAlert("PIN hidden. To disable: set Random PIN in app");
                return;
            }
            nodeDB->saveToDisk();
        }
    };

    onBacklightChange = []() {
        if (!currentConfig.hasBacklight) {
            backlightOn = false;  // Reset toggle
            menuModule->showAlert("No backlight on this device");
            return;
        }
        Drivers::LatchingBacklight* bl = Drivers::LatchingBacklight::getInstance();
        if (backlightOn) {
            bl->latch();
        } else {
            bl->off();
        }
    };

    onRotationChange = []() {
        // rotationIndex maps directly: 0=0°, 1=90°, 2=180°, 3=270°
        uint8_t rotationIndex = Settings::instance().getRotation();
        uint8_t bufferRotation = (rotationIndex + currentConfig.defaultRotation) % 4;
        InkHUD2::instance().setRotation(bufferRotation);
        // Save to persistent storage
        Settings::instance().save();
    };

    // Create Screen submenu
    screenSubMenu = new MenuItem[SCREEN_SUBMENU_COUNT];
    screenSubMenu[0].label = "< Back";
    screenSubMenu[0].type = MenuItemType::BACK;

    screenSubMenu[1].label = "Backlight";
    screenSubMenu[1].type = MenuItemType::TOGGLE;
    screenSubMenu[1].toggleValue = &backlightOn;
    screenSubMenu[1].onChange = &onBacklightChange;

    screenSubMenu[2].label = "Rotation";
    screenSubMenu[2].type = MenuItemType::VALUE;
    screenSubMenu[2].value.options = rotationOptions4;
    screenSubMenu[2].value.currentIndex = Settings::instance().rotationPtr();
    screenSubMenu[2].value.optionCount = 4;
    screenSubMenu[2].onChange = &onRotationChange;

    // Create System submenu
    systemSubMenu = new MenuItem[SYSTEM_SUBMENU_COUNT];
    systemSubMenu[0].label = "< Back";
    systemSubMenu[0].type = MenuItemType::BACK;

    systemSubMenu[1].label = "Hide PIN";
    systemSubMenu[1].type = MenuItemType::TOGGLE;
    systemSubMenu[1].toggleValue = Settings::instance().hidePINPtr();
    systemSubMenu[1].onChange = &onHidePINChange;

    systemSubMenu[2].label = "Backup";
    systemSubMenu[2].type = MenuItemType::ACTION;
    systemSubMenu[2].action = &actionBackup;

    systemSubMenu[3].label = "Restore";
    systemSubMenu[3].type = MenuItemType::ACTION;
    systemSubMenu[3].action = &actionRestore;

    systemSubMenu[4].label = "Shut Down";
    systemSubMenu[4].type = MenuItemType::ACTION;
    systemSubMenu[4].action = &actionShutDown;

    // Count configured channels
    uint8_t maxChannels = channels.getNumChannels();
    uint8_t configuredCount = 0;
    uint8_t configuredChannels[8];

    for (uint8_t i = 0; i < maxChannels && configuredCount < 8; i++) {
        auto& ch = channels.getByIndex(i);
        if (ch.role != meshtastic_Channel_Role_DISABLED) {
            configuredChannels[configuredCount++] = i;
        }
    }

    // Create Alerts submenu
    alertsSubMenuCount = configuredCount + 2;  // Back + channels + DM
    alertsSubMenu = new MenuItem[alertsSubMenuCount];
    alertsOnChange = new std::function<void()>[alertsSubMenuCount];
    alertsChannelNames = new char*[configuredCount];

    alertsSubMenu[0].label = "< Back";
    alertsSubMenu[0].type = MenuItemType::BACK;

    for (uint8_t i = 0; i < configuredCount; i++) {
        uint8_t chIdx = configuredChannels[i];
        const char* name = channels.getName(chIdx);
        alertsChannelNames[i] = new char[16];
        strncpy(alertsChannelNames[i], name, 15);
        alertsChannelNames[i][15] = '\0';

        uint8_t menuIdx = i + 1;
        alertsSubMenu[menuIdx].label = alertsChannelNames[i];
        alertsSubMenu[menuIdx].type = MenuItemType::TOGGLE;
        alertsSubMenu[menuIdx].toggleValue = &alertsEnabled[chIdx];
        alertsOnChange[menuIdx] = [chIdx]() {};
        alertsSubMenu[menuIdx].onChange = &alertsOnChange[menuIdx];
    }

    uint8_t dmIdx = configuredCount + 1;
    alertsSubMenu[dmIdx].label = "DM";
    alertsSubMenu[dmIdx].type = MenuItemType::TOGGLE;
    alertsSubMenu[dmIdx].toggleValue = &alertsEnabled[8];
    alertsOnChange[dmIdx] = []() {};
    alertsSubMenu[dmIdx].onChange = &alertsOnChange[dmIdx];

    // Create main menu
    mainMenuItems = new MenuItem[MENU_ITEM_COUNT];

    mainMenuItems[0].label = "GPS";
    mainMenuItems[0].type = MenuItemType::TOGGLE;
    mainMenuItems[0].toggleValue = &gpsEnabled;

    mainMenuItems[1].label = "Ping";
    mainMenuItems[1].type = MenuItemType::ACTION;
    mainMenuItems[1].action = &actionPing;

    mainMenuItems[2].label = "Alerts";
    mainMenuItems[2].type = MenuItemType::SUBMENU;
    mainMenuItems[2].submenu = alertsSubMenu;
    mainMenuItems[2].submenuCount = alertsSubMenuCount;

    mainMenuItems[3].label = "Screen";
    mainMenuItems[3].type = MenuItemType::SUBMENU;
    mainMenuItems[3].submenu = screenSubMenu;
    mainMenuItems[3].submenuCount = SCREEN_SUBMENU_COUNT;

    mainMenuItems[4].label = "System";
    mainMenuItems[4].type = MenuItemType::SUBMENU;
    mainMenuItems[4].submenu = systemSubMenu;
    mainMenuItems[4].submenuCount = SYSTEM_SUBMENU_COUNT;

    mainMenuItems[5].label = "Exit";
    mainMenuItems[5].type = MenuItemType::BACK;

    // Configure menu
    menuModule->setRootMenu(mainMenuItems, MENU_ITEM_COUNT);
    menuModule->setFirmwareVersion(STRINGIFY(APP_VERSION_SHORT));

    // Link modules
    nodeListModule->setMenuModule(menuModule);
    mapModule->setMenuModule(menuModule);
    cannedMessageModule->setMenuModule(menuModule);

    // Configure message channels
    messageModule->addDMChannel();
    for (uint8_t i = 0; i < configuredCount; i++) {
        uint8_t chIdx = configuredChannels[i];
        messageModule->addChannel(chIdx, alertsChannelNames[i], true);
    }
    messageModule->setMyNodeNum(nodeDB->getNodeNum());

    // Register modules
    hud.addSystemModule(batteryModule);
    hud.addSystemModule(bootModule);
    hud.addSystemModule(menuModule);

    hud.addModule(nodeListModule);
    hud.addModule(messageModule);
    hud.addModule(cannedMessageModule);
    hud.addModule(mapModule);

    // Configure
    hud.setSlotCount(1);
    batteryModule->setLevel(100);

    // Boot state
    bootModule->setState(BootState::LOGO);
    hud.requestFullRefresh();

    // Start events
    inkhud2Events = new Events();
    inkhud2Events->begin(messageModule, batteryModule, nodeListModule, mapModule, menuModule);

    // Backlight
    if (config.hasBacklight && config.backlightPin >= 0) {
        Drivers::LatchingBacklight* backlight = Drivers::LatchingBacklight::getInstance();
        backlight->setPin(config.backlightPin);
    }

    // Buttons
    Inputs::TwoButton* buttons = Inputs::TwoButton::getInstance();

    if (config.mainButtonPin >= 0) {
        buttons->setWiring(0, config.mainButtonPin);
        buttons->setTiming(0, config.mainButtonDebounce, config.mainButtonLongPress);
        buttons->setHandlerShortPress(0, []() {
            InkHUD2::instance().onInput(Input::SELECT);
        });
        buttons->setHandlerLongPress(0, []() {
            InkHUD2::instance().onInput(Input::BACK);
        });
    }

    if (config.hasAuxButton && config.auxButtonPin >= 0) {
        buttons->setWiring(1, config.auxButtonPin);
        buttons->setTiming(1, config.auxButtonDebounce, config.auxButtonLongPress);

        if (config.hasBacklight) {
            // Backlight control
            Drivers::LatchingBacklight* backlight = Drivers::LatchingBacklight::getInstance();
            buttons->setHandlerDown(1, [backlight]() { backlight->peek(); });
            buttons->setHandlerLongPress(1, [backlight]() { backlight->latch(); });
            buttons->setHandlerShortPress(1, [backlight]() { backlight->off(); });
        } else {
            // Scroll control (short = down, long = top)
            buttons->setHandlerShortPress(1, []() {
                InkHUD2::instance().onInput(Input::DOWN);
            });
            buttons->setHandlerLongPress(1, []() {
                InkHUD2::instance().onInput(Input::UP);
            });
        }
    }

    buttons->start();

    // Initial render
    hud.update();

    Serial.println(F("[InkHUD2] setup() complete"));
}

} // namespace InkHUD2
