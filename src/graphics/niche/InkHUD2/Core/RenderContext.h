#pragma once

#include "Buffer.h"
#include "Font.h"
#include "Layout.h"
#include <cstdint>

namespace InkHUD2 {

// Spacing between Latin/Cyrillic and CJK text
constexpr uint8_t CJK_TRANSITION_SPACING = 4;

enum class Align : uint8_t {
    LEFT,
    CENTER,
    RIGHT
};

enum class VAlign : uint8_t {
    TOP,
    MIDDLE,
    BOTTOM
};

// RenderContext - passed to modules for drawing
// Modules call methods, NOT inherit from this class
class RenderContext {
public:
    RenderContext(Buffer* buffer, const Font* font, const Layout* layout)
        : buffer(buffer), font(font), layout(layout),
          clipX(0), clipY(0),
          clipW(buffer ? buffer->width() : 0),
          clipH(buffer ? buffer->height() : 0) {}

    // Set clipping region (for slot-based rendering)
    void setClip(Rect r) {
        clipX = r.x;
        clipY = r.y;
        clipW = r.w;
        clipH = r.h;
    }

    void setClip(int16_t x, int16_t y, uint16_t w, uint16_t h) {
        clipX = x;
        clipY = y;
        clipW = w;
        clipH = h;
    }

    void clearClip() {
        clipX = 0;
        clipY = 0;
        clipW = buffer ? buffer->width() : 0;
        clipH = buffer ? buffer->height() : 0;
    }

    Rect clip() const { return {clipX, clipY, clipW, clipH}; }

    // Relative coordinates (0.0-1.0) to absolute
    int16_t X(float f) const { return clipX + static_cast<int16_t>(f * clipW); }
    int16_t Y(float f) const { return clipY + static_cast<int16_t>(f * clipH); }

    // Current clip dimensions
    uint16_t width() const { return clipW; }
    uint16_t height() const { return clipH; }

    // Layout access
    const Layout* getLayout() const { return layout; }
    const Font* getFont() const { return font; }

    // === Primitives ===

    void pixel(int16_t x, int16_t y, Color c = Color::BLACK);

    void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color c = Color::BLACK);

    void hLine(int16_t x, int16_t y, uint16_t w, Color c = Color::BLACK);
    void vLine(int16_t x, int16_t y, uint16_t h, Color c = Color::BLACK);

    void rect(int16_t x, int16_t y, uint16_t w, uint16_t h, Color c = Color::BLACK);
    void fillRect(int16_t x, int16_t y, uint16_t w, uint16_t h, Color c = Color::BLACK);

    void circle(int16_t cx, int16_t cy, uint16_t r, Color c = Color::BLACK);
    void fillCircle(int16_t cx, int16_t cy, uint16_t r, Color c = Color::BLACK);

    void roundRect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, Color c = Color::BLACK);
    void fillRoundRect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, Color c = Color::BLACK);

    void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color c = Color::BLACK);
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color c = Color::BLACK);

    // === Text ===

    void text(int16_t x, int16_t y, const char* str, Align align = Align::LEFT, Color c = Color::BLACK);

    // Scaled text (scale 0.0-1.0, where 1.0 = normal size)
    void textScaled(int16_t x, int16_t y, const char* str, float scale, Align align = Align::LEFT, Color c = Color::BLACK);

    uint16_t textWidth(const char* str) const;
    uint16_t textWidthScaled(const char* str, float scale) const;

    // Returns height used
    uint16_t textWrapped(int16_t x, int16_t y, uint16_t maxW, const char* str, Color c = Color::BLACK);

    // Wrapped text with height limit - truncates with "..." if too long
    // Returns actual height used
    uint16_t textWrappedTruncated(int16_t x, int16_t y, uint16_t maxW, uint16_t maxH, const char* str, Color c = Color::BLACK);

    // Calculate wrapped text height without rendering
    uint16_t getWrappedTextHeight(uint16_t maxW, const char* str) const;

    // === Standard UI elements ===

    void header(const char* text, Color c = Color::BLACK);
    uint16_t headerHeight() const { return layout ? layout->headerHeight() : 24; }

    void separator(int16_t y, Color c = Color::BLACK);

    // === Effects ===

    void hatch(Rect r, uint8_t spacing = 4, Color c = Color::BLACK);

    void clear() { if (buffer) buffer->clearRect(clip()); }

    // Direct buffer access (for drivers)
    Buffer* getBuffer() { return buffer; }

private:
    void setPixelClipped(int16_t x, int16_t y, Color c);
    bool inClip(int16_t x, int16_t y) const;
    void drawGlyphBitmap(int16_t gx, int16_t gy, uint8_t width, uint8_t height,
                         const uint8_t* bitmap, uint32_t offset, Color c);
    void drawGlyphBitmapScaled(int16_t gx, int16_t gy, uint8_t srcW, uint8_t srcH,
                               const uint8_t* bitmap, uint32_t offset, float scale, Color c);

    Buffer* buffer;
    const Font* font;
    const Layout* layout;

    int16_t clipX, clipY;
    uint16_t clipW, clipH;
};

} // namespace InkHUD2
