#pragma once

#include "../Core/Buffer.h"
#include "../Core/Layout.h"
#include "../Core/RenderContext.h"
#include <cstdint>

namespace InkHUD2 {

// Compass - draws a compass circle with N marker and optional heading arrow
class Compass {
public:
    Compass(Buffer* buffer, const Layout* layout);

    // Draw compass at given center position
    // headingDeg: -1 = no heading, 0-359 = heading in degrees (0 = North)
    void render(int16_t cx, int16_t cy, uint16_t radius, int32_t headingDeg = -1);

    // Draw compass and return recommended width (for layout calculations)
    uint16_t width(uint16_t radius) const { return radius * 2 + 4; }

private:
    void drawCircle(int16_t cx, int16_t cy, uint16_t r);
    void drawArrow(int16_t cx, int16_t cy, uint16_t radius, int32_t headingDeg);

    Buffer* buffer;
    const Layout* layout;
};

} // namespace InkHUD2
