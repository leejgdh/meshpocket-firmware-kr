#pragma once

#include <cstdint>

namespace InkHUD2 {

class RenderContext;

// Logo utility class - draws Meshtastic logo (three diagonal lines /|\)
class Logo {
public:
    // Aspect ratio of the logo (width:height)
    static constexpr float ASPECT_RATIO = 1.9f;

    // Draw Meshtastic logo centered at (centerX, centerY) with given size
    static void draw(RenderContext& ctx, int16_t centerX, int16_t centerY,
                     uint16_t width, uint16_t height);

    // Get logo dimensions that fit within limits while maintaining aspect ratio
    static uint16_t getWidth(uint16_t limitW, uint16_t limitH);
    static uint16_t getHeight(uint16_t limitW, uint16_t limitH);
};

} // namespace InkHUD2
