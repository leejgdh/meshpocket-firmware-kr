#pragma once

#include "../Core/Layout.h"
#include "../Core/RenderContext.h"
#include "MenuItem.h"
#include <cstdint>

namespace InkHUD2 {

// MenuList - reusable menu item list renderer
// Used by: MenuModule (main menu), MapModule (settings), other modules with settings
class MenuList {
public:
    MenuList() = default;

    // Set menu items to display
    void setItems(MenuItem* items, uint8_t count) {
        menuItems = items;
        itemCount = count;
        if (selectedIndex >= count) selectedIndex = 0;
    }

    // Selection state
    void setSelectedIndex(uint8_t idx) { selectedIndex = idx; }
    uint8_t getSelectedIndex() const { return selectedIndex; }
    uint8_t getItemCount() const { return itemCount; }

    // Navigation
    void selectNext();
    void selectPrev();

    // Activate selected item - returns true if handled (toggle/value changed)
    // For ACTION/SUBMENU/BACK, returns false (caller must handle)
    bool activateSelected();

    // Get selected item
    MenuItem* getSelectedItem();
    const MenuItem* getSelectedItem() const;

    // Render menu list within given bounds
    // Uses Layout from RenderContext
    // startY: where to start rendering items
    // endY: where to stop (leave room for hint)
    // margin: horizontal margin from edges
    void render(RenderContext& ctx, int16_t startY, int16_t endY, uint16_t margin);

    // Calculate item height (uses Layout for font metrics)
    static uint16_t itemHeight(const Layout* layout);

private:
    MenuItem* menuItems = nullptr;
    uint8_t itemCount = 0;
    uint8_t selectedIndex = 0;
    uint8_t scrollOffset = 0;

    // Render single item
    void renderItem(RenderContext& ctx, const Layout* layout, const MenuItem& item, bool selected,
                    int16_t y, uint16_t margin);
};

} // namespace InkHUD2
