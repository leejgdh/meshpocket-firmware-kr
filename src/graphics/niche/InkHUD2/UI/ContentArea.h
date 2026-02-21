#pragma once

#include "../Core/Buffer.h"
#include "../Core/Layout.h"
#include <cstdint>

namespace InkHUD2 {

// ContentArea - represents the main content region
// Calculates bounds between StatusBar and Footer
struct ContentArea {
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;

    // Padding-adjusted bounds
    int16_t left() const { return x; }
    int16_t top() const { return y; }
    int16_t right() const { return x + w; }
    int16_t bottom() const { return y + h; }

    // With padding
    int16_t innerLeft(uint16_t padding) const { return x + padding; }
    int16_t innerRight(uint16_t padding) const { return x + w - padding; }
    uint16_t innerWidth(uint16_t padding) const { return w - 2 * padding; }
};

// Helper to calculate content area - align right edge with battery icon
inline ContentArea calculateContentArea(const Layout* layout,
                                        int16_t statusBarBottom, int16_t footerTop) {
    ContentArea area;
    if (layout) {
        uint16_t margin = layout->margin();
        // Right edge at screenW - margin - 1 to align with battery
        area.x = margin;
        area.y = statusBarBottom;
        area.w = layout->width() - 2 * margin - 2;  // align with battery
        area.h = (footerTop > statusBarBottom) ? (footerTop - statusBarBottom) : 0;
    } else {
        // Fallback without layout - assume 200x200 screen
        constexpr uint16_t defaultMargin = 1;
        constexpr uint16_t defaultWidth = 200;
        area.x = defaultMargin;
        area.y = statusBarBottom;
        area.w = defaultWidth - 2 * defaultMargin - 2;
        area.h = (footerTop > statusBarBottom) ? (footerTop - statusBarBottom) : 0;
    }
    return area;
}

} // namespace InkHUD2
