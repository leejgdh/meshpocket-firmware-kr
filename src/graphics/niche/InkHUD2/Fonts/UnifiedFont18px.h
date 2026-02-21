#pragma once

#include "../Core/Font.h"

namespace InkHUD2 {

// Placeholder font data - in production this would be generated from a font file
// Contains basic ASCII (0x20-0x7E) and common symbols

// Font bitmap data (1bpp, MSB first)
// Each glyph is stored row by row, packed into bytes
extern const uint8_t UnifiedFont18px_Bitmap[];

// Glyph data
extern const UnifiedGlyph UnifiedFont18px_Glyphs[];

// Codepoint array (sorted for binary search)
extern const uint32_t UnifiedFont18px_Codepoints[];

// Font definition
extern const UnifiedFont UnifiedFont18px;

// Number of glyphs
constexpr uint16_t UNIFIED_FONT_18PX_GLYPH_COUNT = 95;  // Basic ASCII

} // namespace InkHUD2
