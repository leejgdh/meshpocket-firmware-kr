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

// Helper to draw a single glyph bitmap (handles PROGMEM and scaling)
void RenderContext::drawGlyphBitmap(int16_t gx, int16_t gy, uint8_t width, uint8_t height,
                                     const uint8_t* bitmap, uint32_t offset, Color c) {
    const uint8_t* bitmapData = bitmap + offset;
    float scale = font->getScale();

    if (scale == 1.0f) {
        // Native size - direct rendering
        for (uint8_t row = 0; row < height; ++row) {
            for (uint8_t col = 0; col < width; ++col) {
                size_t bitIndex = static_cast<size_t>(row) * width + col;
                size_t byteIndex = bitIndex / 8;
                uint8_t bitOffset = 7 - (bitIndex % 8);  // MSB first

                uint8_t byte = font->isCJKFont() ? pgm_read_byte(&bitmapData[byteIndex]) : bitmapData[byteIndex];
                if (byte & (1 << bitOffset)) {
                    setPixelClipped(gx + col, gy + row, c);
                }
            }
        }
    } else {
        // Scaled rendering - nearest neighbor
        // Note: produces poor quality for bitmap fonts, avoid using scale != 1.0
        uint8_t srcWidth = font->isCJKFont() ? 18 : width;  // Original font width
        uint8_t srcHeight = font->isCJKFont() ? 18 : height;
        uint8_t dstWidth = static_cast<uint8_t>(srcWidth * scale + 0.5f);
        uint8_t dstHeight = static_cast<uint8_t>(srcHeight * scale + 0.5f);

        for (uint8_t dstRow = 0; dstRow < dstHeight; ++dstRow) {
            uint8_t srcRow = static_cast<uint8_t>(dstRow / scale);
            if (srcRow >= srcHeight) srcRow = srcHeight - 1;

            for (uint8_t dstCol = 0; dstCol < dstWidth; ++dstCol) {
                uint8_t srcCol = static_cast<uint8_t>(dstCol / scale);
                if (srcCol >= srcWidth) srcCol = srcWidth - 1;

                size_t bitIndex = static_cast<size_t>(srcRow) * srcWidth + srcCol;
                size_t byteIndex = bitIndex / 8;
                uint8_t bitOffset = 7 - (bitIndex % 8);

                uint8_t byte = font->isCJKFont() ? pgm_read_byte(&bitmapData[byteIndex]) : bitmapData[byteIndex];
                if (byte & (1 << bitOffset)) {
                    setPixelClipped(gx + dstCol, gy + dstRow, c);
                }
            }
        }
    }
}


// Draw glyph bitmap with explicit scale (for scaled text rendering)
void RenderContext::drawGlyphBitmapScaled(int16_t gx, int16_t gy, uint8_t srcW, uint8_t srcH,
                                           const uint8_t* bitmap, uint32_t offset, float scale, Color c) {
    const uint8_t* bitmapData = bitmap + offset;
    uint8_t dstW = static_cast<uint8_t>(srcW * scale + 0.5f);
    uint8_t dstH = static_cast<uint8_t>(srcH * scale + 0.5f);

    for (uint8_t dstRow = 0; dstRow < dstH; ++dstRow) {
        uint8_t srcRow = static_cast<uint8_t>(dstRow / scale);
        if (srcRow >= srcH) srcRow = srcH - 1;

        for (uint8_t dstCol = 0; dstCol < dstW; ++dstCol) {
            uint8_t srcCol = static_cast<uint8_t>(dstCol / scale);
            if (srcCol >= srcW) srcCol = srcW - 1;

            size_t bitIndex = static_cast<size_t>(srcRow) * srcW + srcCol;
            size_t byteIndex = bitIndex / 8;
            uint8_t bitOffset = 7 - (bitIndex % 8);

            uint8_t byte = font->isCJKFont() ? pgm_read_byte(&bitmapData[byteIndex]) : bitmapData[byteIndex];
            if (byte & (1 << bitOffset)) {
                setPixelClipped(gx + dstCol, gy + dstRow, c);
            }
        }
    }
}

void RenderContext::text(int16_t x, int16_t y, const char* str, Align align, Color c, float scale) {
    if (!str || !font || !font->isCJKFont()) return;  // Only CJK font supports scaling

    // Calculate width for alignment
    uint16_t tw = textWidth(str, scale);
    if (align == Align::CENTER) {
        x -= tw / 2;
    } else if (align == Align::RIGHT) {
        x -= tw;
    }

    // CJK font metrics (unscaled source)
    const uint8_t srcW = 18;
    const uint8_t srcH = 18;
    const int8_t srcYOffset = -18;
    // Round negative values properly (toward negative infinity)
    int8_t scaledYOffset = static_cast<int8_t>(srcYOffset * scale - 0.5f);
    uint8_t scaledAscent = static_cast<uint8_t>(18 * scale + 0.5f);

    int16_t cursorX = clipX + x;
    int16_t cursorY = clipY + y + scaledAscent;

    const char* ptr = str;
    uint32_t prevCp = 0;
    while (*ptr) {
        uint32_t cp = Font::decodeUTF8(ptr);
        if (cp == 0) break;

        // Add spacing when transitioning between Latin/Cyrillic and CJK
        bool prevIsLatin = (prevCp >= 0x21 && prevCp <= 0x7E) ||
                           (prevCp >= 0x400 && prevCp <= 0x4FF) ||
                           (prevCp >= 0xC0 && prevCp <= 0x2FF);
        bool currIsLatin = (cp >= 0x21 && cp <= 0x7E) ||
                           (cp >= 0x400 && cp <= 0x4FF) ||
                           (cp >= 0xC0 && cp <= 0x2FF);
        bool prevIsCJK = (prevCp >= 0x3040 && prevCp <= 0x30FF) ||
                         (prevCp >= 0x4E00 && prevCp <= 0x9FFF);
        bool currIsCJK = (cp >= 0x3040 && cp <= 0x30FF) ||
                         (cp >= 0x4E00 && cp <= 0x9FFF);
        if ((prevIsLatin && currIsCJK) || (prevIsCJK && currIsLatin)) {
            cursorX += static_cast<int16_t>(CJK_TRANSITION_SPACING * scale + 0.5f);
        }

        int16_t glyphIndex = font->findGlyphIndex(cp);

        // Try fallback characters if glyph not found
        if (glyphIndex < 0) {
            glyphIndex = font->findGlyphIndex(0xFFFD);  // � replacement character
        }
        if (glyphIndex < 0) {
            glyphIndex = font->findGlyphIndex('?');
        }
        if (glyphIndex < 0) {
            prevCp = cp;
            continue;
        }

        // Get glyph info (font returns unscaled since font.scale=1.0)
        uint32_t bitmapOffset;
        uint8_t glyphW, glyphH, xAdvance;
        int8_t yOffset;

        if (!font->getGlyphInfo(glyphIndex, bitmapOffset, glyphW, glyphH, yOffset, xAdvance)) {
            prevCp = cp;
            continue;
        }

        // Scale the xAdvance from glyph data
        uint8_t scaledXAdvance = static_cast<uint8_t>(xAdvance * scale + 0.5f);

        // Draw glyph bitmap with explicit scale
        const uint8_t* bitmap = font->getBitmap();
        if (bitmap) {
            int16_t gx = cursorX;
            int16_t gy = cursorY + scaledYOffset;
            drawGlyphBitmapScaled(gx, gy, srcW, srcH, bitmap, bitmapOffset, scale, c);
        }

        cursorX += scaledXAdvance;
        prevCp = cp;
    }
}

uint16_t RenderContext::textWidth(const char* str, float scale) const {
    if (!str || !font) return 0;

    uint16_t totalW = 0;
    const char* ptr = str;
    uint32_t prevCp = 0;

    while (*ptr) {
        uint32_t cp = Font::decodeUTF8(ptr);
        if (cp == 0) break;

        // Add spacing when transitioning between Latin/Cyrillic and CJK
        bool prevIsLatin = (prevCp >= 0x21 && prevCp <= 0x7E) ||
                           (prevCp >= 0x400 && prevCp <= 0x4FF) ||
                           (prevCp >= 0xC0 && prevCp <= 0x2FF);
        bool currIsLatin = (cp >= 0x21 && cp <= 0x7E) ||
                           (cp >= 0x400 && cp <= 0x4FF) ||
                           (cp >= 0xC0 && cp <= 0x2FF);
        bool prevIsCJK = (prevCp >= 0x3040 && prevCp <= 0x30FF) ||
                         (prevCp >= 0x4E00 && prevCp <= 0x9FFF);
        bool currIsCJK = (cp >= 0x3040 && cp <= 0x30FF) ||
                         (cp >= 0x4E00 && cp <= 0x9FFF);
        if ((prevIsLatin && currIsCJK) || (prevIsCJK && currIsLatin)) {
            totalW += static_cast<uint16_t>(CJK_TRANSITION_SPACING * scale + 0.5f);
        }

        int16_t glyphIndex = font->findGlyphIndex(cp);
        if (glyphIndex < 0) {
            glyphIndex = font->findGlyphIndex(0xFFFD);  // � replacement character
        }
        if (glyphIndex < 0) {
            glyphIndex = font->findGlyphIndex('?');
        }
        if (glyphIndex < 0) {
            prevCp = cp;
            continue;
        }

        // Get actual xAdvance from glyph
        uint32_t bitmapOffset;
        uint8_t w, h, xAdvance;
        int8_t yOffset;

        if (font->getGlyphInfo(glyphIndex, bitmapOffset, w, h, yOffset, xAdvance)) {
            totalW += static_cast<uint8_t>(xAdvance * scale + 0.5f);
        }
        prevCp = cp;
    }

    return totalW;
}

// Helper: check if codepoint is CJK (allows line break after)
static bool isCJKCodepoint(uint32_t cp) {
    // CJK Unified Ideographs
    if (cp >= 0x4E00 && cp <= 0x9FFF) return true;
    // CJK Extension A
    if (cp >= 0x3400 && cp <= 0x4DBF) return true;
    // Hiragana
    if (cp >= 0x3040 && cp <= 0x309F) return true;
    // Katakana
    if (cp >= 0x30A0 && cp <= 0x30FF) return true;
    // CJK Symbols and Punctuation
    if (cp >= 0x3000 && cp <= 0x303F) return true;
    // Fullwidth Forms
    if (cp >= 0xFF00 && cp <= 0xFFEF) return true;
    // Hangul Syllables (Korean)
    if (cp >= 0xAC00 && cp <= 0xD7AF) return true;
    return false;
}

uint16_t RenderContext::textWrapped(int16_t x, int16_t y, uint16_t maxW, const char* str, Color c, float scale) {
    TextRenderer tr(buffer, font);
    tr.setClip(clipX, clipY, clipW, clipH);
    return tr.textWrappedScaled(clipX + x, clipY + y, maxW, str, scale, c);
}

uint16_t RenderContext::textWrappedTruncated(int16_t x, int16_t y, uint16_t maxW, uint16_t maxH, const char* str, Color c, float scale) {
    TextRenderer tr(buffer, font);
    tr.setClip(clipX, clipY, clipW, clipH);
    return tr.textWrappedTruncatedScaled(clipX + x, clipY + y, maxW, maxH, str, scale, c);
}

uint16_t RenderContext::getWrappedTextHeight(uint16_t maxW, const char* str, float scale) const {
    TextRenderer tr(buffer, font);
    return tr.getWrappedTextHeightScaled(maxW, str, scale);
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
