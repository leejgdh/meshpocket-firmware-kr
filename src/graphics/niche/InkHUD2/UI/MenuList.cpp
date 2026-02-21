/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "MenuList.h"

namespace InkHUD2 {

void MenuList::selectNext() {
    if (itemCount == 0) return;
    selectedIndex = (selectedIndex + 1) % itemCount;
}

void MenuList::selectPrev() {
    if (itemCount == 0) return;
    if (selectedIndex > 0) {
        selectedIndex--;
    } else {
        selectedIndex = itemCount - 1;
    }
}

bool MenuList::activateSelected() {
    if (!menuItems || selectedIndex >= itemCount) {
        return false;
    }

    MenuItem& item = menuItems[selectedIndex];

    switch (item.type) {
        case MenuItemType::TOGGLE:
            if (item.toggleValue) {
                *item.toggleValue = !*item.toggleValue;
                if (item.onChange && *item.onChange) {
                    (*item.onChange)();
                }
                return true;
            }
            break;

        case MenuItemType::VALUE:
            if (item.value.currentIndex && item.value.optionCount > 0) {
                *item.value.currentIndex = (*item.value.currentIndex + 1) % item.value.optionCount;
                if (item.onChange && *item.onChange) {
                    (*item.onChange)();
                }
                return true;
            }
            break;

        case MenuItemType::ACTION:
        case MenuItemType::SUBMENU:
        case MenuItemType::BACK:
        case MenuItemType::LABEL:
            // Caller handles these
            return false;
    }

    return false;
}

MenuItem* MenuList::getSelectedItem() {
    if (menuItems && selectedIndex < itemCount) {
        return &menuItems[selectedIndex];
    }
    return nullptr;
}

const MenuItem* MenuList::getSelectedItem() const {
    if (menuItems && selectedIndex < itemCount) {
        return &menuItems[selectedIndex];
    }
    return nullptr;
}

uint16_t MenuList::itemHeight(const Layout* layout) {
    if (!layout) return 18;
    return layout->menuLineHeight() + layout->menuItemPadding();
}

void MenuList::render(RenderContext& ctx, int16_t startY, int16_t endY, uint16_t margin) {
    const Layout* layout = ctx.getLayout();
    if (!layout || !menuItems || itemCount == 0) return;

    uint16_t itemH = itemHeight(layout);
    uint16_t itemSpacing = layout->menuItemSpacing();

    // Calculate how many items fit
    uint8_t visibleCount = (endY - startY) / (itemH + itemSpacing);
    if (visibleCount == 0) visibleCount = 1;

    // Adjust scroll offset to keep selected visible
    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    } else if (selectedIndex >= scrollOffset + visibleCount) {
        scrollOffset = selectedIndex - visibleCount + 1;
    }

    int16_t y = startY;

    for (uint8_t i = scrollOffset; i < itemCount && y + itemH <= endY; ++i) {
        bool selected = (i == selectedIndex);
        renderItem(ctx, layout, menuItems[i], selected, y, margin);
        y += itemH + itemSpacing;
    }
}

void MenuList::renderItem(RenderContext& ctx, const Layout* layout, const MenuItem& item, bool selected,
                          int16_t y, uint16_t margin) {
    if (!layout) return;

    uint16_t itemH = itemHeight(layout);
    uint16_t textInset = layout->textInset();
    uint16_t elemSpacing = layout->elementSpacing();

    // Selection box outline
    if (selected) {
        ctx.rect(margin, y - elemSpacing, ctx.width() - 2 * margin, itemH, Color::BLACK);
    }

    Color textColor = Color::BLACK;

    // Item label
    ctx.textScaled(margin + textInset, y, item.label, Layout::menuScale, Align::LEFT, textColor);

    // Right-side value/indicator
    int16_t rightX = ctx.width() - margin - textInset;

    switch (item.type) {
        case MenuItemType::TOGGLE:
            if (item.toggleValue) {
                ctx.textScaled(rightX, y, *item.toggleValue ? "ON" : "OFF", Layout::menuScale, Align::RIGHT, textColor);
            }
            break;

        case MenuItemType::VALUE:
            if (item.value.options && item.value.currentIndex) {
                uint8_t idx = *item.value.currentIndex;
                if (idx < item.value.optionCount) {
                    ctx.textScaled(rightX, y, item.value.options[idx], Layout::menuScale, Align::RIGHT, textColor);
                }
            }
            break;

        case MenuItemType::SUBMENU:
            ctx.textScaled(rightX, y, ">", Layout::menuScale, Align::RIGHT, textColor);
            break;

        case MenuItemType::LABEL:
            if (item.labelValue) {
                ctx.textScaled(rightX, y, item.labelValue, Layout::menuScale, Align::RIGHT, textColor);
            }
            break;

        case MenuItemType::BACK:
        case MenuItemType::ACTION:
            // No indicator
            break;
    }
}

} // namespace InkHUD2
