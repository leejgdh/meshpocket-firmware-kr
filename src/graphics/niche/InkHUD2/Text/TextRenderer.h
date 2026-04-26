#pragma once

#include "../Core/Font.h"
#include "../Core/Buffer.h"
#include "../Core/Layout.h"
#include "../Core/RenderContext.h"  // For Align enum
#include <cstdint>

namespace InkHUD2 {

// TextRenderer - handles all text rendering logic
// Encapsulates: wrapping, truncation, Latin→CJK spacing, scaling
class TextRenderer {
public:
    TextRenderer(Buffer* buffer, const Font* font);

    void setClip(int16_t x, int16_t y, uint16_t w, uint16_t h);
    void clearClip();

    // === Text ===
    // All text functions take an optional `scale` parameter as the last
    // argument. Default = Layout::bodyScale. Pass Layout::headerScale for
    // emphasis. Modules should rarely need anything else.

    void text(int16_t x, int16_t y, const char* str,
              Align align = Align::LEFT, Color c = Color::BLACK,
              float scale = Layout::bodyScale);

    uint16_t textWidth(const char* str, float scale = Layout::bodyScale) const;

    // Returns height used
    uint16_t textWrapped(int16_t x, int16_t y, uint16_t maxW, const char* str,
                         Color c = Color::BLACK, float scale = Layout::bodyScale);

    // Wrapped with height limit - truncates with "..." if too long
    uint16_t textWrappedTruncated(int16_t x, int16_t y, uint16_t maxW, uint16_t maxH,
                                  const char* str, Color c = Color::BLACK,
                                  float scale = Layout::bodyScale);

    // Calculate wrapped text height without rendering
    uint16_t getWrappedTextHeight(uint16_t maxW, const char* str,
                                  float scale = Layout::bodyScale) const;


    // === Font access ===
    const Font* getFont() const { return font; }
    uint16_t lineHeight() const { return font ? font->lineHeight() : 18; }

    // === Script detection (for external use) ===
    static bool isCJK(uint32_t cp);
    static bool isLatinOrCyrillic(uint32_t cp);

private:
    void drawGlyphBitmapScaled(int16_t gx, int16_t gy, uint8_t srcW, uint8_t srcH,
                               const uint8_t* bitmap, uint32_t offset, float scale, Color c);

    Buffer* buffer;
    const Font* font;

    int16_t clipX, clipY;
    uint16_t clipW, clipH;

    // Latin→CJK spacing constant
    static constexpr uint8_t SCRIPT_TRANSITION_SPACING = 2;
};

} // namespace InkHUD2
