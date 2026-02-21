/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "Logo.h"
#include "RenderContext.h"
#include <cmath>

namespace InkHUD2 {

uint16_t Logo::getWidth(uint16_t limitW, uint16_t limitH) {
    if (limitW > limitH * ASPECT_RATIO)
        return static_cast<uint16_t>(limitH * ASPECT_RATIO);
    return limitW;
}

uint16_t Logo::getHeight(uint16_t limitW, uint16_t limitH) {
    if (limitH > limitW / ASPECT_RATIO)
        return static_cast<uint16_t>(limitW / ASPECT_RATIO);
    return limitH;
}

void Logo::draw(RenderContext& ctx, int16_t centerX, int16_t centerY,
                uint16_t width, uint16_t height) {
    // Meshtastic logo: three diagonal lines forming /|\ pattern
    // Ported from original InkHUD implementation
    // All coordinates are relative to the clip region

    struct Point { int16_t x; int16_t y; };

    int16_t logoTh = static_cast<int16_t>(width * 0.068f);  // Line thickness
    if (logoTh < 1) logoTh = 1;

    int16_t logoL = centerX - (width / 2) + (logoTh / 2);
    int16_t logoT = centerY - (height / 2) + (logoTh / 2);
    int16_t logoW = width - logoTh;
    int16_t logoH = height - logoTh;
    int16_t logoR = logoL + logoW - 1;
    int16_t logoB = logoT + logoH - 1;

    // Helper lambda for mapping values (like Arduino map())
    auto mapVal = [](int x, int in_min, int in_max, int out_min, int out_max) -> int16_t {
        return static_cast<int16_t>((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
    };

    // Points for the three paths
    Point a1 = {mapVal(0, 0, 3, logoL, logoR), logoB};
    Point a2 = {mapVal(1, 0, 3, logoL, logoR), logoT};
    Point b1 = {mapVal(1, 0, 3, logoL, logoR), logoB};
    Point b2 = {mapVal(2, 0, 3, logoL, logoR), logoT};
    Point c1 = {mapVal(2, 0, 3, logoL, logoR), logoT};
    Point c2 = {mapVal(3, 0, 3, logoL, logoR), logoB};

    // Calculate angle for thickening the paths
    int deltaX = abs(a2.x - a1.x);
    int deltaY = abs(a2.y - a1.y);
    float angle = (deltaX > 0) ? atanf(static_cast<float>(deltaY) / deltaX) : 1.5708f;  // 90° if vertical

    // Distance from path center to edge (for thickening)
    float cosAngle = cosf(1.5708f - angle);  // cos(90° - angle)
    float sinAngle = sinf(1.5708f - angle);  // sin(90° - angle)
    int16_t fromPathX = static_cast<int16_t>(cosAngle * logoTh * 0.5f);
    int16_t fromPathY = static_cast<int16_t>(sinAngle * logoTh * 0.5f);

    // Draw path A as quad (two triangles)
    Point aq1 = {static_cast<int16_t>(a1.x - fromPathX), static_cast<int16_t>(a1.y - fromPathY)};
    Point aq2 = {static_cast<int16_t>(a2.x - fromPathX), static_cast<int16_t>(a2.y - fromPathY)};
    Point aq3 = {static_cast<int16_t>(a2.x + fromPathX), static_cast<int16_t>(a2.y + fromPathY)};
    Point aq4 = {static_cast<int16_t>(a1.x + fromPathX), static_cast<int16_t>(a1.y + fromPathY)};
    ctx.fillTriangle(aq1.x, aq1.y, aq2.x, aq2.y, aq3.x, aq3.y, Color::BLACK);
    ctx.fillTriangle(aq1.x, aq1.y, aq3.x, aq3.y, aq4.x, aq4.y, Color::BLACK);

    // Draw path B as quad
    Point bq1 = {static_cast<int16_t>(b1.x - fromPathX), static_cast<int16_t>(b1.y - fromPathY)};
    Point bq2 = {static_cast<int16_t>(b2.x - fromPathX), static_cast<int16_t>(b2.y - fromPathY)};
    Point bq3 = {static_cast<int16_t>(b2.x + fromPathX), static_cast<int16_t>(b2.y + fromPathY)};
    Point bq4 = {static_cast<int16_t>(b1.x + fromPathX), static_cast<int16_t>(b1.y + fromPathY)};
    ctx.fillTriangle(bq1.x, bq1.y, bq2.x, bq2.y, bq3.x, bq3.y, Color::BLACK);
    ctx.fillTriangle(bq1.x, bq1.y, bq3.x, bq3.y, bq4.x, bq4.y, Color::BLACK);

    // Draw path C as quad (note: different offsets for downward slope)
    Point cq1 = {static_cast<int16_t>(c1.x - fromPathX), static_cast<int16_t>(c1.y + fromPathY)};
    Point cq2 = {static_cast<int16_t>(c2.x - fromPathX), static_cast<int16_t>(c2.y + fromPathY)};
    Point cq3 = {static_cast<int16_t>(c2.x + fromPathX), static_cast<int16_t>(c2.y - fromPathY)};
    Point cq4 = {static_cast<int16_t>(c1.x + fromPathX), static_cast<int16_t>(c1.y - fromPathY)};
    ctx.fillTriangle(cq1.x, cq1.y, cq2.x, cq2.y, cq3.x, cq3.y, Color::BLACK);
    ctx.fillTriangle(cq1.x, cq1.y, cq3.x, cq3.y, cq4.x, cq4.y, Color::BLACK);

    // Round the intersection of paths B and C with a filled circle
    if (logoTh > 3) {
        int capRad = static_cast<int>(sqrtf(static_cast<float>(fromPathX * fromPathX + fromPathY * fromPathY)));
        ctx.fillCircle(b2.x, b2.y, capRad, Color::BLACK);
    }
}

} // namespace InkHUD2
