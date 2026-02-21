/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "Font.h"
#include <Arduino.h>  // For pgm_read_*

namespace InkHUD2 {

int16_t Font::findGlyphIndex(uint32_t codepoint) const {
    if (cjkFont) {
        return NicheGraphics::cjkLookup(cjkFont, codepoint);
    }

    // Legacy UnifiedFont path (unused but kept for compatibility)
    if (!unifiedFont || !unifiedFont->codepoints || !unifiedFont->glyphs || unifiedFont->glyphCount == 0) {
        return -1;
    }

    int32_t low = 0;
    int32_t high = static_cast<int32_t>(unifiedFont->glyphCount) - 1;

    while (low <= high) {
        int32_t mid = low + (high - low) / 2;
        uint32_t cp = unifiedFont->codepoints[mid];

        if (cp == codepoint) {
            return static_cast<int16_t>(mid);
        } else if (cp < codepoint) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return -1;
}

bool Font::getGlyphInfo(int16_t glyphIndex, uint32_t& bitmapOffset, uint8_t& width,
                        uint8_t& height, int8_t& yOffset, uint8_t& xAdvance) const {
    if (cjkFont) {
        if (glyphIndex < 0 || glyphIndex >= cjkFont->glyphCount) {
            return false;
        }

        // Read from PROGMEM
        bitmapOffset = pgm_read_dword(&cjkFont->glyphs[glyphIndex].bitmapOffset);
        width = cjkFont->width;
        height = cjkFont->height;
        yOffset = cjkFont->yOffset;
        xAdvance = pgm_read_byte(&cjkFont->glyphs[glyphIndex].xAdvance);

        // Apply scale factor
        if (scale != 1.0f) {
            // Note: bitmap rendering handles scaling, we just report scaled metrics
            width = static_cast<uint8_t>(width * scale + 0.5f);
            height = static_cast<uint8_t>(height * scale + 0.5f);
            yOffset = static_cast<int8_t>(yOffset * scale);
            xAdvance = static_cast<uint8_t>(xAdvance * scale + 0.5f);
        }

        return true;
    }

    // Legacy UnifiedFont path
    if (!unifiedFont || glyphIndex < 0 || glyphIndex >= unifiedFont->glyphCount) {
        return false;
    }

    const UnifiedGlyph& glyph = unifiedFont->glyphs[glyphIndex];
    bitmapOffset = glyph.bitmapOffset;
    width = glyph.width;
    height = glyph.height;
    yOffset = glyph.yOffset;
    xAdvance = glyph.xAdvance;
    return true;
}

const uint8_t* Font::getBitmap() const {
    if (cjkFont) {
        return cjkFont->bitmap;
    }
    if (unifiedFont) {
        return unifiedFont->bitmap;
    }
    return nullptr;
}

uint8_t Font::lineHeight() const {
    if (cjkFont) {
        // CJK fonts are square, height is the line height
        uint8_t h = cjkFont->height;
        if (scale != 1.0f) {
            h = static_cast<uint8_t>(h * scale + 0.5f);
        }
        return h;
    }
    if (unifiedFont) {
        return unifiedFont->lineHeight;
    }
    return 18;  // Default
}

uint8_t Font::ascent() const {
    if (cjkFont) {
        // CJK yOffset is negative (e.g., -18), ascent is the absolute value
        uint8_t a = static_cast<uint8_t>(-cjkFont->yOffset);
        if (scale != 1.0f) {
            a = static_cast<uint8_t>(a * scale + 0.5f);
        }
        return a;
    }
    if (unifiedFont) {
        return unifiedFont->ascent;
    }
    return 14;  // Default
}

uint8_t Font::cellWidth() const {
    if (cjkFont) {
        uint8_t w = cjkFont->width;
        if (scale != 1.0f) {
            w = static_cast<uint8_t>(w * scale + 0.5f);
        }
        return w;
    }
    if (unifiedFont) {
        return unifiedFont->cellWidth;
    }
    return 10;  // Default
}

uint8_t Font::cellHeight() const {
    if (cjkFont) {
        uint8_t h = cjkFont->height;
        if (scale != 1.0f) {
            h = static_cast<uint8_t>(h * scale + 0.5f);
        }
        return h;
    }
    if (unifiedFont) {
        return unifiedFont->cellHeight;
    }
    return 18;  // Default
}

// Helper: check if byte is valid UTF-8 continuation byte and not null
static inline bool isValidContinuation(uint8_t byte) {
    return byte != 0 && (byte & 0xC0) == 0x80;
}

uint32_t Font::decodeUTF8(const char*& ptr) {
    if (!ptr || !*ptr) return 0;

    const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
    uint32_t codepoint;
    uint8_t first = *p++;

    if ((first & 0x80) == 0) {
        // 1-byte (ASCII)
        codepoint = first;
    } else if ((first & 0xE0) == 0xC0) {
        // 2-byte
        if (isValidContinuation(*p)) {
            codepoint = (first & 0x1F) << 6;
            codepoint |= (*p++ & 0x3F);
        } else {
            codepoint = 0xFFFD;  // Invalid or truncated sequence
        }
    } else if ((first & 0xF0) == 0xE0) {
        // 3-byte
        if (isValidContinuation(p[0]) && isValidContinuation(p[1])) {
            codepoint = (first & 0x0F) << 12;
            codepoint |= (*p++ & 0x3F) << 6;
            codepoint |= (*p++ & 0x3F);
        } else {
            codepoint = 0xFFFD;
        }
    } else if ((first & 0xF8) == 0xF0) {
        // 4-byte
        if (isValidContinuation(p[0]) && isValidContinuation(p[1]) && isValidContinuation(p[2])) {
            codepoint = (first & 0x07) << 18;
            codepoint |= (*p++ & 0x3F) << 12;
            codepoint |= (*p++ & 0x3F) << 6;
            codepoint |= (*p++ & 0x3F);
        } else {
            codepoint = 0xFFFD;
        }
    } else {
        // Invalid leading byte
        codepoint = 0xFFFD;
    }

    ptr = reinterpret_cast<const char*>(p);
    return codepoint;
}

} // namespace InkHUD2
