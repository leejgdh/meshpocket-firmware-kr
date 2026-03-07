#pragma once

#include <cstdint>
#include "graphics/niche/Fonts/CJK/CJKFont.h"

namespace InkHUD2 {

// Unified font format - no encoding modes, UTF-8 input only
struct UnifiedGlyph {
    uint32_t bitmapOffset;  // Offset into bitmap array
    uint8_t width;          // Glyph width in pixels
    uint8_t height;         // Glyph height in pixels
    int8_t xOffset;         // X offset from cursor
    int8_t yOffset;         // Y offset from baseline (negative = above)
    uint8_t xAdvance;       // Cursor advance (includes padding)
};

struct UnifiedFont {
    const uint8_t* bitmap;           // Packed glyph bitmaps (1bpp)
    const UnifiedGlyph* glyphs;      // Glyph data array
    const uint32_t* codepoints;      // Sorted codepoint array for binary search
    uint16_t glyphCount;             // Number of glyphs
    uint8_t lineHeight;              // Line height (includes padding)
    uint8_t ascent;                  // Distance from baseline to top
    uint8_t descent;                 // Distance from baseline to bottom
    uint8_t cellWidth;               // Max glyph cell width
    uint8_t cellHeight;              // Max glyph cell height
};

// Font class - wraps CJKFont for use with InkHUD2
class Font {
public:
    // Constructor for CJKFont (the actual font we use)
    explicit Font(const NicheGraphics::CJKFont* cjkFont, float scale = 1.0f)
        : cjkFont(cjkFont), scale(scale) {}

    // Legacy constructor for UnifiedFont (placeholder)
    explicit Font(const UnifiedFont* font) : unifiedFont(font) {}

    // Find glyph index by codepoint (binary search)
    int16_t findGlyphIndex(uint32_t codepoint) const;

    // Get glyph bitmap info
    bool getGlyphInfo(int16_t glyphIndex, uint32_t& bitmapOffset, uint8_t& width,
                      uint8_t& height, int8_t& yOffset, uint8_t& xAdvance) const;

    // Get bitmap data pointer
    const uint8_t* getBitmap() const;

    // Decode UTF-8 and return codepoint, advance pointer
    static uint32_t decodeUTF8(const char*& ptr);

    // Metrics
    uint8_t lineHeight() const;
    uint8_t ascent() const;
    static uint8_t descent() { return 0; }  // CJKFont doesn't have descent
    uint8_t cellWidth() const;
    uint8_t cellHeight() const;
    float getScale() const { return scale; }

    // Check which font type is being used
    bool isCJKFont() const { return cjkFont != nullptr; }
    const UnifiedFont* raw() const { return unifiedFont; }

private:
    const NicheGraphics::CJKFont* cjkFont = nullptr;
    const UnifiedFont* unifiedFont = nullptr;
    float scale = 1.0f;
};

} // namespace InkHUD2
