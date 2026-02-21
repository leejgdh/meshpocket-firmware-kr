#pragma once

#include "Module.h"
#include "../UI/MenuList.h"
#include "../UI/MenuItem.h"
#include <cstdint>
#include <vector>
#include <functional>

namespace InkHUD2 {

// Menu view state
enum class MenuView : uint8_t {
    MENU,           // Normal menu list
    ALERT,          // Alert message (e.g., after Ping)
    SHUTDOWN,       // "Shutting down..." screen
    SHUTDOWN_FINAL  // Final logo with node name (shown after shutdown)
};

// Menu module - settings and first-boot tips
class MenuModule : public SystemModule {
public:
    MenuModule();
    ~MenuModule() override;

    void onRender(RenderContext& ctx) override;
    void onInput(Input input) override;

    // Menu control
    void open();
    void close();
    bool isOpen() const { return visible; }

    // Build menu structure
    void setRootMenu(MenuItem* items, uint8_t count);

    // First-boot tips
    void showTip(const char* tipText);
    void dismissTip();

    // Actions
    void showAlert(const char* message);  // Show alert with "Click: return to menu"
    void startShutdown();                  // Show shutdown screen and trigger shutdown

    // Status bar data
    void setTime(uint8_t hours, uint8_t minutes) { statusHours = hours; statusMinutes = minutes; }
    void setBatteryVoltage(float volts) { batteryVoltage = volts; }
    void setChannelUtil(uint8_t percent) { channelUtil = percent; }
    void setDutyCycle(uint8_t percent) { dutyCycle = percent; }
    void setFirmwareVersion(const char* ver) { firmwareVersion = ver; }

private:
    void renderMenu(RenderContext& ctx);
    void renderTip(RenderContext& ctx);
    void renderAlert(RenderContext& ctx);
    void renderShutdown(RenderContext& ctx);
    void renderShutdownFinal(RenderContext& ctx);

    // Menu state
    bool visible = false;
    MenuView currentView = MenuView::MENU;
    const char* alertMessage = nullptr;
    bool alertFromExternal = false;  // True if alert was shown from outside (not from menu action)
    MenuItem* rootMenu = nullptr;
    uint8_t rootMenuCount = 0;

    MenuItem* currentMenu = nullptr;
    uint8_t currentMenuCount = 0;
    uint8_t selectedIndex = 0;

    // Menu stack for nested menus
    struct MenuLevel {
        MenuItem* menu;
        uint8_t count;
        uint8_t selectedIndex;
    };
    std::vector<MenuLevel> menuStack;

    // Reusable menu list renderer
    MenuList menuList;

    // Tip state
    bool tipVisible = false;
    const char* tipText = nullptr;

    // Status bar data
    uint8_t statusHours = 0;
    uint8_t statusMinutes = 0;
    float batteryVoltage = 0.0f;
    uint8_t channelUtil = 0;
    uint8_t dutyCycle = 0;
    const char* firmwareVersion = "2.7.x";

    // Helper for rendering
    void renderStatusBar(RenderContext& ctx);

    // Font scales - use Layout::smallScale, Layout::menuScale, Layout::hintScale
};

} // namespace InkHUD2
