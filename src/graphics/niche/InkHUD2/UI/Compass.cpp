/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "Compass.h"
#include "../Text/TextRenderer.h"
#include <cmath>

namespace InkHUD2 {

Compass::Compass(Buffer* buffer, const Layout* layout)
    : buffer(buffer), layout(layout) {}

void Compass::render(int16_t cx, int16_t cy, uint16_t radius, int32_t headingDeg) {
    if (!buffer || !layout) return;

    // Draw compass circle
    drawCircle(cx, cy, radius);

    // Draw N marker at top
    // Simple "N" using pixels
    int16_t nY = cy - radius - 2;
    int16_t nX = cx;

    // Draw a simple N (5 pixels wide, 5 tall)
    // |   |
    // |\  |
    // | \ |
    // |  \|
    // |   |
    int16_t nW = 4;
    int16_t nH = 5;
    int16_t nLeft = nX - nW / 2;
    int16_t nTop = nY - nH;

    // Left vertical
    for (int16_t i = 0; i < nH; i++) {
        buffer->setPixel(nLeft, nTop + i, Color::BLACK);
    }
    // Right vertical
    for (int16_t i = 0; i < nH; i++) {
        buffer->setPixel(nLeft + nW, nTop + i, Color::BLACK);
    }
    // Diagonal
    for (int16_t i = 0; i < nH; i++) {
        int16_t dx = i * nW / (nH - 1);
        buffer->setPixel(nLeft + dx, nTop + i, Color::BLACK);
    }

    // Draw center dot
    buffer->setPixel(cx, cy, Color::BLACK);
    buffer->setPixel(cx + 1, cy, Color::BLACK);
    buffer->setPixel(cx - 1, cy, Color::BLACK);
    buffer->setPixel(cx, cy + 1, Color::BLACK);
    buffer->setPixel(cx, cy - 1, Color::BLACK);

    // Draw heading arrow if available
    if (headingDeg >= 0) {
        drawArrow(cx, cy, radius, headingDeg);
    }
}

void Compass::drawCircle(int16_t cx, int16_t cy, uint16_t r) {
    if (!buffer) return;

    // Midpoint circle algorithm
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        buffer->setPixel(cx + x, cy + y, Color::BLACK);
        buffer->setPixel(cx + y, cy + x, Color::BLACK);
        buffer->setPixel(cx - y, cy + x, Color::BLACK);
        buffer->setPixel(cx - x, cy + y, Color::BLACK);
        buffer->setPixel(cx - x, cy - y, Color::BLACK);
        buffer->setPixel(cx - y, cy - x, Color::BLACK);
        buffer->setPixel(cx + y, cy - x, Color::BLACK);
        buffer->setPixel(cx + x, cy - y, Color::BLACK);

        y++;
        err += 2 * y + 1;
        if (2 * err + 1 > 2 * x) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

void Compass::drawArrow(int16_t cx, int16_t cy, uint16_t radius, int32_t headingDeg) {
    if (!buffer) return;

    // Convert degrees to radians (0 = North = up, clockwise)
    // Screen coordinates: Y increases downward
    // So 0 degrees (North) = -90 in standard math coords
    float rad = (headingDeg - 90) * 3.14159f / 180.0f;

    // Arrow tip at edge of circle (with small margin)
    int16_t tipX = cx + static_cast<int16_t>((radius - 3) * cosf(rad));
    int16_t tipY = cy + static_cast<int16_t>((radius - 3) * sinf(rad));

    // Arrow base (near center)
    int16_t baseX = cx + static_cast<int16_t>(6 * cosf(rad));
    int16_t baseY = cy + static_cast<int16_t>(6 * sinf(rad));

    // Draw arrow shaft using Bresenham's line algorithm
    int16_t dx = tipX > baseX ? tipX - baseX : baseX - tipX;
    int16_t dy = tipY > baseY ? tipY - baseY : baseY - tipY;
    int16_t sx = baseX < tipX ? 1 : -1;
    int16_t sy = baseY < tipY ? 1 : -1;
    int16_t err = dx - dy;

    int16_t x = baseX, y = baseY;
    while (true) {
        buffer->setPixel(x, y, Color::BLACK);
        if (x == tipX && y == tipY) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }

    // Draw arrowhead (two short lines from tip)
    float headAngle1 = rad + 2.5f;  // ~143 degrees offset
    float headAngle2 = rad - 2.5f;
    int16_t headLen = 6;

    int16_t h1x = tipX - static_cast<int16_t>(headLen * cosf(rad - 0.5f));
    int16_t h1y = tipY - static_cast<int16_t>(headLen * sinf(rad - 0.5f));
    int16_t h2x = tipX - static_cast<int16_t>(headLen * cosf(rad + 0.5f));
    int16_t h2y = tipY - static_cast<int16_t>(headLen * sinf(rad + 0.5f));

    // Draw head line 1
    dx = tipX > h1x ? tipX - h1x : h1x - tipX;
    dy = tipY > h1y ? tipY - h1y : h1y - tipY;
    sx = h1x < tipX ? 1 : -1;
    sy = h1y < tipY ? 1 : -1;
    err = dx - dy;
    x = h1x; y = h1y;
    while (true) {
        buffer->setPixel(x, y, Color::BLACK);
        if (x == tipX && y == tipY) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }

    // Draw head line 2
    dx = tipX > h2x ? tipX - h2x : h2x - tipX;
    dy = tipY > h2y ? tipY - h2y : h2y - tipY;
    sx = h2x < tipX ? 1 : -1;
    sy = h2y < tipY ? 1 : -1;
    err = dx - dy;
    x = h2x; y = h2y;
    while (true) {
        buffer->setPixel(x, y, Color::BLACK);
        if (x == tipX && y == tipY) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

} // namespace InkHUD2
