/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "MenuList.h"
#include "TextUtils.h"

namespace InkHUD2 {

void MenuList::selectNext() {
    if (itemCount > 0) {
        selectedIndex = (selectedIndex + 1) % itemCount;
    }
}

void MenuList::selectPrev() {
    if (itemCount > 0) {
        if (selectedIndex > 0) {
            selectedIndex--;
        } else {
            selectedIndex = itemCount - 1;
        }
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
    return layout->bodyLineHeight() + layout->menuItemPadding();
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
    float scale = Layout::bodyScale;

    // Right-side value/indicator
    int16_t rightX = ctx.width() - margin - textInset;

    // Calculate right-side width for truncation
    uint16_t rightW = 0;
    switch (item.type) {
        case MenuItemType::TOGGLE:
            rightW = ctx.textWidth("OFF", scale) + elemSpacing;
            break;
        case MenuItemType::VALUE:
            if (item.value.options && item.value.currentIndex) {
                uint8_t idx = *item.value.currentIndex;
                if (idx < item.value.optionCount) {
                    rightW = ctx.textWidth(item.value.options[idx], scale) + elemSpacing;
                }
            }
            break;
        case MenuItemType::SUBMENU:
            rightW = ctx.textWidth(">", scale) + elemSpacing;
            break;
        case MenuItemType::LABEL:
            if (item.labelValue) {
                rightW = ctx.textWidth(item.labelValue, scale) + elemSpacing;
            }
            break;
        default:
            break;
    }

    // Calculate max width for label
    int16_t labelX = margin + textInset;
    uint16_t maxLabelW = ctx.width() - 2 * margin - 2 * textInset - rightW;

    // Render label with truncation if needed
    char truncBuf[64];
    TextUtils::truncate(ctx, item.label, maxLabelW, scale, truncBuf, sizeof(truncBuf));
    ctx.text(labelX, y, truncBuf, Align::LEFT, textColor, scale);

    switch (item.type) {
        case MenuItemType::TOGGLE:
            if (item.toggleValue) {
                ctx.text(rightX, y, *item.toggleValue ? "ON" : "OFF", Align::RIGHT, textColor, scale);
            }
            break;

        case MenuItemType::VALUE:
            if (item.value.options && item.value.currentIndex) {
                uint8_t idx = *item.value.currentIndex;
                if (idx < item.value.optionCount) {
                    ctx.text(rightX, y, item.value.options[idx], Align::RIGHT, textColor, scale);
                }
            }
            break;

        case MenuItemType::SUBMENU:
            ctx.text(rightX, y, ">", Align::RIGHT, textColor, scale);
            break;

        case MenuItemType::LABEL:
            if (item.labelValue) {
                ctx.text(rightX, y, item.labelValue, Align::RIGHT, textColor, scale);
            }
            break;

        case MenuItemType::BACK:
        case MenuItemType::ACTION:
            // No indicator
            break;
    }
}

} // namespace InkHUD2
