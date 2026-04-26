/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "StatusBar.h"
#include "TextUtils.h"
#include <cstring>

namespace InkHUD2 {

StatusBar::StatusBar(Buffer* buffer, const Layout* layout, TextRenderer* text)
    : buffer(buffer), layout(layout), textRenderer(text) {}

uint16_t StatusBar::height() const {
    if (!layout) return 18;  // Fallback
    return layout->lineHeight() + layout->padding() * 2;
}

int16_t StatusBar::render(int16_t y, const char* title, Icon icon, bool centered) {
    if (!buffer || !layout || !textRenderer) return y;

    uint16_t padding = layout->padding();
    uint16_t lineH = layout->lineHeight();

    int16_t headerCenterY = y + lineH / 2;

    // Centered mode: just title in center, no icon
    if (centered) {
        uint8_t scaledTextH = static_cast<uint8_t>(lineH * Layout::headerScale + 0.5f);
        int16_t textY = headerCenterY - scaledTextH / 2;
        int16_t centerX = buffer->width() / 2;
        textRenderer->text(centerX, textY, title, Align::CENTER, Color::BLACK, Layout::headerScale);

        // Separator
        int16_t sepY = y + lineH + 2;
        drawSeparator(sepY);
        return sepY + 4;
    }

    // Normal mode: icon + left-aligned title

    // Icon dimensions
    uint8_t iconW = lineH;
    uint8_t iconH = lineH * 2 / 3;
    int16_t iconY = headerCenterY - iconH / 2;

    // Draw icon
    switch (icon) {
        case Icon::ENVELOPE:
            drawEnvelopeIcon(padding, iconY, iconW, iconH);
            break;
        case Icon::USERS:
            drawUsersIcon(padding, iconY, iconW, iconH);
            break;
        case Icon::GEAR:
            drawGearIcon(padding, iconY, iconW, iconH);
            break;
        case Icon::INFO:
            drawInfoIcon(padding, iconY, iconW, iconH);
            break;
        case Icon::MAP:
            drawMapIcon(padding, iconY, iconW, iconH);
            break;
        case Icon::NONE:
        default:
            break;
    }

    // Title text - limit width to not overlap battery
    int16_t textX = (icon != Icon::NONE) ? (padding + iconW + padding) : padding;
    uint8_t scaledTextH = static_cast<uint8_t>(lineH * Layout::headerScale + 0.5f);
    int16_t textY = headerCenterY - scaledTextH / 2;

    // Calculate max width for title (leave space for battery)
    Rect batRect = layout->batteryRect();
    uint16_t maxTitleW = batRect.x - textX - padding;

    // Truncate title if needed
    char truncBuf[64];
    TextUtils::truncate(textRenderer, title, maxTitleW, Layout::headerScale, truncBuf, sizeof(truncBuf));
    textRenderer->text(textX, textY, truncBuf, Align::LEFT, Color::BLACK, Layout::headerScale);

    // Separator
    int16_t sepY = y + lineH + 2;
    drawSeparator(sepY);

    // Return content start Y
    return sepY + 4;
}

void StatusBar::drawEnvelopeIcon(int16_t x, int16_t y, uint8_t w, uint8_t h) {
    if (!buffer) return;

    // Rectangle body
    for (int16_t i = 0; i < w; i++) {
        buffer->setPixel(x + i, y, Color::BLACK);
        buffer->setPixel(x + i, y + h - 1, Color::BLACK);
    }
    for (int16_t i = 0; i < h; i++) {
        buffer->setPixel(x, y + i, Color::BLACK);
        buffer->setPixel(x + w - 1, y + i, Color::BLACK);
    }

    // V-shaped flap
    int16_t midX = x + w / 2;
    int16_t midY = y + h / 2;

    // Left diagonal
    int16_t dx = midX - x;
    int16_t dy = midY - y;
    if (dx > 0) {
        for (int16_t i = 0; i <= dx; i++) {
            int16_t py = y + (i * dy / dx);
            buffer->setPixel(x + i, py, Color::BLACK);
        }

        // Right diagonal
        for (int16_t i = 0; i <= dx; i++) {
            int16_t py = y + (i * dy / dx);
            buffer->setPixel(x + w - 1 - i, py, Color::BLACK);
        }
    }
}

void StatusBar::drawUsersIcon(int16_t x, int16_t y, uint8_t w, uint8_t h) {
    if (!buffer) return;

    // Simple two-person icon
    int16_t r = h / 4;
    int16_t cx1 = x + w / 3;
    int16_t cx2 = x + w * 2 / 3;
    int16_t cy = y + r + 1;

    // Heads (circles)
    for (int16_t dy = -r; dy <= r; dy++) {
        for (int16_t dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy <= r*r) {
                buffer->setPixel(cx1 + dx, cy + dy, Color::BLACK);
                buffer->setPixel(cx2 + dx, cy + dy, Color::BLACK);
            }
        }
    }

    // Bodies (arcs at bottom)
    int16_t bodyY = y + h - 2;
    int16_t bodyR = w / 4;
    for (int16_t dx = -bodyR; dx <= bodyR; dx++) {
        buffer->setPixel(cx1 + dx, bodyY, Color::BLACK);
        buffer->setPixel(cx2 + dx, bodyY, Color::BLACK);
    }
}

void StatusBar::drawGearIcon(int16_t x, int16_t y, uint8_t w, uint8_t h) {
    if (!buffer) return;

    int16_t cx = x + w / 2;
    int16_t cy = y + h / 2;
    int16_t outerR = (w < h ? w : h) / 2 - 1;
    int16_t innerR = outerR / 2;

    // Outer circle with teeth
    for (int16_t dy = -outerR; dy <= outerR; dy++) {
        for (int16_t dx = -outerR; dx <= outerR; dx++) {
            int16_t dist2 = dx*dx + dy*dy;
            if (dist2 <= outerR*outerR && dist2 >= (outerR-1)*(outerR-1)) {
                buffer->setPixel(cx + dx, cy + dy, Color::BLACK);
            }
        }
    }

    // Inner circle (hole)
    for (int16_t dy = -innerR; dy <= innerR; dy++) {
        for (int16_t dx = -innerR; dx <= innerR; dx++) {
            if (dx*dx + dy*dy <= innerR*innerR) {
                buffer->setPixel(cx + dx, cy + dy, Color::BLACK);
            }
        }
    }
}

void StatusBar::drawInfoIcon(int16_t x, int16_t y, uint8_t w, uint8_t h) {
    if (!buffer) return;

    int16_t cx = x + w / 2;
    int16_t cy = y + h / 2;
    int16_t r = (w < h ? w : h) / 2 - 1;

    // Circle outline
    for (int16_t dy = -r; dy <= r; dy++) {
        for (int16_t dx = -r; dx <= r; dx++) {
            int16_t dist2 = dx*dx + dy*dy;
            if (dist2 <= r*r && dist2 >= (r-1)*(r-1)) {
                buffer->setPixel(cx + dx, cy + dy, Color::BLACK);
            }
        }
    }

    // "i" letter
    buffer->setPixel(cx, cy - r/2, Color::BLACK);  // Dot
    for (int16_t dy = 0; dy <= r/2; dy++) {
        buffer->setPixel(cx, cy + dy, Color::BLACK);  // Stem
    }
}

void StatusBar::drawMapIcon(int16_t x, int16_t y, uint8_t w, uint8_t h) {
    if (!buffer) return;

    // Simple map pin / location marker icon
    int16_t cx = x + w / 2;
    int16_t pinTop = y + 1;
    int16_t pinBottom = y + h - 1;

    // Circle head (top half of icon)
    int16_t headR = w / 4;
    int16_t headY = pinTop + headR + 1;

    // Draw circle outline
    for (int16_t dy = -headR; dy <= headR; dy++) {
        for (int16_t dx = -headR; dx <= headR; dx++) {
            int16_t dist2 = dx*dx + dy*dy;
            if (dist2 <= headR*headR && dist2 >= (headR-1)*(headR-1)) {
                buffer->setPixel(cx + dx, headY + dy, Color::BLACK);
            }
        }
    }

    // Fill center dot
    int16_t dotR = headR / 2;
    if (dotR < 1) dotR = 1;
    for (int16_t dy = -dotR; dy <= dotR; dy++) {
        for (int16_t dx = -dotR; dx <= dotR; dx++) {
            if (dx*dx + dy*dy <= dotR*dotR) {
                buffer->setPixel(cx + dx, headY + dy, Color::BLACK);
            }
        }
    }

    // Pointed bottom (triangle)
    int16_t pointY = pinBottom;
    int16_t baseY = headY + headR;
    for (int16_t py = baseY; py <= pointY; py++) {
        int16_t progress = py - baseY;
        int16_t total = pointY - baseY;
        int16_t halfW = (total > 0) ? (headR * (total - progress) / total) : headR;
        buffer->setPixel(cx - halfW, py, Color::BLACK);
        buffer->setPixel(cx + halfW, py, Color::BLACK);
    }
}

void StatusBar::drawSeparator(int16_t y) {
    if (!buffer) return;

    uint16_t w = buffer->width();
    // Dotted line (every 2 pixels)
    for (int16_t x = 0; x < static_cast<int16_t>(w); x += 2) {
        buffer->setPixel(x, y, Color::BLACK);
    }
}

} // namespace InkHUD2
