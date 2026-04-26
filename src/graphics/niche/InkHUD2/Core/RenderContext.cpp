/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "RenderContext.h"
#include "../Text/TextRenderer.h"
#include <algorithm>
#include <cmath>
#include <Arduino.h>  // For pgm_read_byte

namespace InkHUD2 {

// === Helper functions ===

bool RenderContext::inClip(int16_t x, int16_t y) const {
    return x >= clipX && x < clipX + clipW && y >= clipY && y < clipY + clipH;
}

void RenderContext::setPixelClipped(int16_t x, int16_t y, Color c) {
    if (buffer && inClip(x, y)) {
        buffer->setPixel(x, y, c);
    }
}

// === Primitives ===

void RenderContext::pixel(int16_t x, int16_t y, Color c) {
    setPixelClipped(clipX + x, clipY + y, c);
}

void RenderContext::hLine(int16_t x, int16_t y, uint16_t w, Color c) {
    int16_t ax = clipX + x;
    int16_t ay = clipY + y;
    for (uint16_t i = 0; i < w; ++i) {
        setPixelClipped(ax + i, ay, c);
    }
}

void RenderContext::vLine(int16_t x, int16_t y, uint16_t h, Color c) {
    int16_t ax = clipX + x;
    int16_t ay = clipY + y;
    for (uint16_t i = 0; i < h; ++i) {
        setPixelClipped(ax, ay + i, c);
    }
}

// Bresenham line algorithm
void RenderContext::line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color c) {
    x0 += clipX;
    y0 += clipY;
    x1 += clipX;
    y1 += clipY;

    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;

    while (true) {
        setPixelClipped(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void RenderContext::rect(int16_t x, int16_t y, uint16_t w, uint16_t h, Color c) {
    hLine(x, y, w, c);
    hLine(x, y + h - 1, w, c);
    vLine(x, y, h, c);
    vLine(x + w - 1, y, h, c);
}

void RenderContext::fillRect(int16_t x, int16_t y, uint16_t w, uint16_t h, Color c) {
    for (uint16_t j = 0; j < h; ++j) {
        hLine(x, y + j, w, c);
    }
}

// Midpoint circle algorithm
void RenderContext::circle(int16_t cx, int16_t cy, uint16_t r, Color c) {
    cx += clipX;
    cy += clipY;

    int16_t x = r;
    int16_t y = 0;
    int16_t err = 1 - r;

    while (x >= y) {
        setPixelClipped(cx + x, cy + y, c);
        setPixelClipped(cx - x, cy + y, c);
        setPixelClipped(cx + x, cy - y, c);
        setPixelClipped(cx - x, cy - y, c);
        setPixelClipped(cx + y, cy + x, c);
        setPixelClipped(cx - y, cy + x, c);
        setPixelClipped(cx + y, cy - x, c);
        setPixelClipped(cx - y, cy - x, c);

        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x + 1);
        }
    }
}

void RenderContext::fillCircle(int16_t cx, int16_t cy, uint16_t r, Color c) {
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 1 - r;

    while (x >= y) {
        hLine(cx - x, cy + y, 2 * x + 1, c);
        hLine(cx - x, cy - y, 2 * x + 1, c);
        hLine(cx - y, cy + x, 2 * y + 1, c);
        hLine(cx - y, cy - x, 2 * y + 1, c);

        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x + 1);
        }
    }
}

void RenderContext::roundRect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, Color c) {
    // Clamp radius to avoid underflow
    uint16_t maxR = (w < h ? w : h) / 2;
    if (r > maxR) r = maxR;
    if (r == 0 || w < 2 || h < 2) {
        rect(x, y, w, h, c);
        return;
    }

    // Horizontal lines (excluding corners)
    hLine(x + r, y, w - 2 * r, c);
    hLine(x + r, y + h - 1, w - 2 * r, c);
    // Vertical lines (excluding corners)
    vLine(x, y + r, h - 2 * r, c);
    vLine(x + w - 1, y + r, h - 2 * r, c);

    // Corner arcs using midpoint algorithm
    int16_t px = r;
    int16_t py = 0;
    int16_t err = 1 - r;

    while (px >= py) {
        // Top-right
        setPixelClipped(clipX + x + w - 1 - r + px, clipY + y + r - py, c);
        setPixelClipped(clipX + x + w - 1 - r + py, clipY + y + r - px, c);
        // Top-left
        setPixelClipped(clipX + x + r - px, clipY + y + r - py, c);
        setPixelClipped(clipX + x + r - py, clipY + y + r - px, c);
        // Bottom-right
        setPixelClipped(clipX + x + w - 1 - r + px, clipY + y + h - 1 - r + py, c);
        setPixelClipped(clipX + x + w - 1 - r + py, clipY + y + h - 1 - r + px, c);
        // Bottom-left
        setPixelClipped(clipX + x + r - px, clipY + y + h - 1 - r + py, c);
        setPixelClipped(clipX + x + r - py, clipY + y + h - 1 - r + px, c);

        py++;
        if (err < 0) {
            err += 2 * py + 1;
        } else {
            px--;
            err += 2 * (py - px + 1);
        }
    }
}

void RenderContext::fillRoundRect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, Color c) {
    // Clamp radius to avoid underflow
    uint16_t maxR = (w < h ? w : h) / 2;
    if (r > maxR) r = maxR;
    if (r == 0 || w < 2 || h < 2) {
        fillRect(x, y, w, h, c);
        return;
    }

    // Central rectangle
    fillRect(x + r, y, w - 2 * r, h, c);
    // Side rectangles
    fillRect(x, y + r, r, h - 2 * r, c);
    fillRect(x + w - r, y + r, r, h - 2 * r, c);

    // Corner circles using midpoint algorithm
    int16_t px = r;
    int16_t py = 0;
    int16_t err = 1 - r;

    while (px >= py) {
        // Top corners
        hLine(x + r - px, y + r - py, w - 2 * r + 2 * px, c);
        hLine(x + r - py, y + r - px, w - 2 * r + 2 * py, c);
        // Bottom corners
        hLine(x + r - px, y + h - 1 - r + py, w - 2 * r + 2 * px, c);
        hLine(x + r - py, y + h - 1 - r + px, w - 2 * r + 2 * py, c);

        py++;
        if (err < 0) {
            err += 2 * py + 1;
        } else {
            px--;
            err += 2 * (py - px + 1);
        }
    }
}

void RenderContext::triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color c) {
    line(x0, y0, x1, y1, c);
    line(x1, y1, x2, y2, c);
    line(x2, y2, x0, y0, c);
}

void RenderContext::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color c) {
    // Sort vertices by y coordinate
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y1 > y2) { std::swap(y1, y2); std::swap(x1, x2); }
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }

    if (y0 == y2) {
        // Degenerate case - horizontal line
        int16_t minX = std::min({x0, x1, x2});
        int16_t maxX = std::max({x0, x1, x2});
        hLine(minX, y0, maxX - minX + 1, c);
        return;
    }

    // Scanline fill
    auto interpolateX = [](int16_t y, int16_t y0, int16_t x0, int16_t y1, int16_t x1) -> int16_t {
        if (y1 == y0) return x0;
        return x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    };

    for (int16_t y = y0; y <= y2; ++y) {
        int16_t xa, xb;

        if (y < y1) {
            xa = interpolateX(y, y0, x0, y1, x1);
            xb = interpolateX(y, y0, x0, y2, x2);
        } else {
            xa = interpolateX(y, y1, x1, y2, x2);
            xb = interpolateX(y, y0, x0, y2, x2);
        }

        if (xa > xb) std::swap(xa, xb);
        hLine(xa, y, xb - xa + 1, c);
    }
}

// === Text ===
// All glyph rendering delegates to TextRenderer (single owner of font math
// and per-pixel clipping). Coordinates are clip-relative on entry; we
// translate to absolute and forward the clip rect so TextRenderer's
// per-pixel clip honors our slot.

void RenderContext::text(int16_t x, int16_t y, const char* str,
                          Align align, Color c, float scale) {
    if (!buffer) return;
    TextRenderer tr(buffer, font);
    tr.setClip(clipX, clipY, clipW, clipH);
    tr.text(clipX + x, clipY + y, str, align, c, scale);
}

uint16_t RenderContext::textWidth(const char* str, float scale) const {
    if (!buffer) return 0;
    TextRenderer tr(const_cast<Buffer*>(buffer), font);
    return tr.textWidth(str, scale);
}

uint16_t RenderContext::textWrapped(int16_t x, int16_t y, uint16_t maxW, const char* str, Color c, float scale) {
    TextRenderer tr(buffer, font);
    tr.setClip(clipX, clipY, clipW, clipH);
    return tr.textWrapped(clipX + x, clipY + y, maxW, str, c, scale);
}

uint16_t RenderContext::textWrappedTruncated(int16_t x, int16_t y, uint16_t maxW, uint16_t maxH, const char* str, Color c, float scale) {
    TextRenderer tr(buffer, font);
    tr.setClip(clipX, clipY, clipW, clipH);
    return tr.textWrappedTruncated(clipX + x, clipY + y, maxW, maxH, str, c, scale);
}

uint16_t RenderContext::getWrappedTextHeight(uint16_t maxW, const char* str, float scale) const {
    TextRenderer tr(buffer, font);
    return tr.getWrappedTextHeight(maxW, str, scale);
}

// === Standard UI elements ===

void RenderContext::header(const char* text, Color c) {
    if (!layout) return;

    uint16_t pad = layout->padding();
    int16_t y = pad;
    int16_t textY = y + pad;

    // Draw header text
    this->text(pad, textY, text, Align::LEFT, c);

    // Draw separator line below header
    separator(y + layout->headerHeight(), c);
}

void RenderContext::separator(int16_t y, Color c) {
    // Dotted line (every other pixel) - original InkHUD style
    for (int16_t x = 0; x < static_cast<int16_t>(clipW); x += 2) {
        pixel(x, y, c);
    }
}

// === Effects ===

void RenderContext::hatch(Rect r, uint8_t spacing, Color c) {
    // Guard against infinite loop
    if (spacing == 0) spacing = 1;

    // r is in clip-relative coordinates, convert to absolute for calculations
    int16_t absX = clipX + r.x;
    int16_t absY = clipY + r.y;

    // Diagonal lines from top-left to bottom-right
    for (int16_t offset = 0; offset < r.w + r.h; offset += spacing) {
        int16_t x0 = absX + offset;
        int16_t y0 = absY;
        int16_t x1 = absX;
        int16_t y1 = absY + offset;

        // Clip to rect
        if (x0 >= absX + r.w) {
            int16_t excess = x0 - (absX + r.w - 1);
            x0 = absX + r.w - 1;
            y0 += excess;
        }
        if (y1 >= absY + r.h) {
            int16_t excess = y1 - (absY + r.h - 1);
            y1 = absY + r.h - 1;
            x1 += excess;
        }

        if (x0 >= absX && y0 < absY + r.h && x1 < absX + r.w && y1 >= absY) {
            // Draw line - convert back to clip-relative since line() adds clipX/Y
            line(x0 - clipX, y0 - clipY, x1 - clipX, y1 - clipY, c);
        }
    }
}

} // namespace InkHUD2
