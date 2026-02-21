#pragma once

#include <cstdint>
#include <functional>

namespace InkHUD2 {

// Menu item types
enum class MenuItemType : uint8_t {
    ACTION,     // Executes a callback
    TOGGLE,     // On/off toggle
    SUBMENU,    // Opens another menu
    VALUE,      // Shows current value, cycles on select
    LABEL,      // Read-only label with value (no interaction)
    BACK        // Goes back to parent menu
};

struct MenuItem {
    const char* label = nullptr;
    MenuItemType type = MenuItemType::LABEL;

    union {
        bool* toggleValue;
        struct {
            const char** options;
            uint8_t* currentIndex;
            uint8_t optionCount;
        } value;
        struct MenuItem* submenu;
        std::function<void()>* action;
        const char* labelValue;  // For LABEL type (read-only)
    };

    // Number of items (for submenu)
    uint8_t submenuCount = 0;

    // Callback after toggle/value change (outside union - can coexist)
    std::function<void()>* onChange = nullptr;
};

} // namespace InkHUD2
