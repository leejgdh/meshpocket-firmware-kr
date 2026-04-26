/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "TextRenderer.h"
#include <cstring>

namespace InkHUD2 {

TextRenderer::TextRenderer(Buffer* buffer, const Font* font)
    : buffer(buffer), font(font),
      clipX(0), clipY(0),
      clipW(buffer ? buffer->width() : 0),
      clipH(buffer ? buffer->height() : 0) {}

void TextRenderer::setClip(int16_t x, int16_t y, uint16_t w, uint16_t h) {
    clipX = x;
    clipY = y;
    clipW = w;
    clipH = h;
}

void TextRenderer::clearClip() {
    clipX = 0;
    clipY = 0;
    clipW = buffer ? buffer->width() : 0;
    clipH = buffer ? buffer->height() : 0;
}

// === Script detection ===

bool TextRenderer::isCJK(uint32_t cp) {
    return (cp >= 0x3040 && cp <= 0x30FF) ||  // Hiragana + Katakana
           (cp >= 0x4E00 && cp <= 0x9FFF) ||  // CJK Unified Ideographs
           (cp >= 0x3000 && cp <= 0x303F) ||  // CJK Symbols and Punctuation
           (cp >= 0xFF00 && cp <= 0xFFEF) ||  // Fullwidth Forms
           (cp >= 0xAC00 && cp <= 0xD7AF);    // Korean Hangul
}

bool TextRenderer::isLatinOrCyrillic(uint32_t cp) {
    return (cp >= 0x21 && cp <= 0x7E) ||      // Basic Latin (printable)
           (cp >= 0x400 && cp <= 0x4FF) ||    // Cyrillic
           (cp >= 0xC0 && cp <= 0x2FF);       // Latin Extended
}

// === Glyph drawing ===

void TextRenderer::drawGlyphBitmapScaled(int16_t gx, int16_t gy, uint8_t srcW, uint8_t srcH,
                                          const uint8_t* bitmap, uint32_t offset, float scale, Color c) {
    if (!buffer || !bitmap || scale <= 0) return;

    uint8_t dstW = static_cast<uint8_t>(srcW * scale + 0.5f);
    uint8_t dstH = static_cast<uint8_t>(srcH * scale + 0.5f);
    if (dstW == 0) dstW = 1;
    if (dstH == 0) dstH = 1;

    for (uint8_t dy = 0; dy < dstH; dy++) {
        for (uint8_t dx = 0; dx < dstW; dx++) {
            uint8_t sx = static_cast<uint8_t>(dx / scale);
            uint8_t sy = static_cast<uint8_t>(dy / scale);
            if (sx >= srcW) sx = srcW - 1;
            if (sy >= srcH) sy = srcH - 1;

            uint32_t bitIndex = sy * srcW + sx;
            uint32_t byteIdx = offset + (bitIndex >> 3);
            uint8_t bitPos = 7 - (bitIndex & 7);

            if (bitmap[byteIdx] & (1 << bitPos)) {
                int16_t px = gx + dx;
                int16_t py = gy + dy;
                if (px >= clipX && py >= clipY &&
                    px < clipX + static_cast<int16_t>(clipW) &&
                    py < clipY + static_cast<int16_t>(clipH) &&
                    px < static_cast<int16_t>(buffer->width()) &&
                    py < static_cast<int16_t>(buffer->height())) {
                    buffer->setPixel(px, py, c);
                }
            }
        }
    }
}

// === Text ===
// Single body for both unscaled and scaled rendering: the unified function
// always takes a scale parameter (default = Layout::bodyScale at call sites
// via the header). Legacy *Scaled inline wrappers in the header forward
// here.

uint16_t TextRenderer::textWidth(const char* str, float scale) const {
    if (!str || !font || scale <= 0) return 0;

    // Calculate width by summing individually rounded advances
    // to match the actual rendering behavior
    uint16_t width = 0;
    const char* ptr = str;
    uint32_t prevCp = 0;

    while (*ptr) {
        uint32_t cp = Font::decodeUTF8(ptr);
        if (cp == 0) break;

        // Add spacing when transitioning between scripts
        if ((isLatinOrCyrillic(prevCp) && isCJK(cp)) ||
            (isCJK(prevCp) && isLatinOrCyrillic(cp))) {
            width += static_cast<uint16_t>(SCRIPT_TRANSITION_SPACING * scale + 0.5f);
        }

        int16_t glyphIndex = font->findGlyphIndex(cp);
        if (glyphIndex < 0) glyphIndex = font->findGlyphIndex(0xFFFD);
        if (glyphIndex < 0) glyphIndex = font->findGlyphIndex('?');
        if (glyphIndex < 0) continue;

        uint32_t bitmapOffset;
        uint8_t glyphW, glyphH, xAdvance;
        int8_t yOffset;

        if (font->getGlyphInfo(glyphIndex, bitmapOffset, glyphW, glyphH, yOffset, xAdvance)) {
            // Round each advance individually so width matches what text() draws.
            width += static_cast<uint16_t>(xAdvance * scale + 0.5f);
        }
        prevCp = cp;
    }

    return width;
}

void TextRenderer::text(int16_t x, int16_t y, const char* str, Align align, Color c, float scale) {
    if (!str || !font || !buffer || scale <= 0) return;

    int16_t cursorX = x;
    uint16_t scaledWidth = textWidth(str, scale);

    if (align == Align::CENTER) {
        cursorX = x - scaledWidth / 2;
    } else if (align == Align::RIGHT) {
        cursorX = x - scaledWidth;
    }

    const char* ptr = str;
    uint32_t prevCp = 0;

    while (*ptr) {
        uint32_t cp = Font::decodeUTF8(ptr);
        if (cp == 0) break;

        // Add spacing when transitioning between scripts
        if ((isLatinOrCyrillic(prevCp) && isCJK(cp)) ||
            (isCJK(prevCp) && isLatinOrCyrillic(cp))) {
            cursorX += static_cast<int16_t>(SCRIPT_TRANSITION_SPACING * scale + 0.5f);
        }

        int16_t glyphIndex = font->findGlyphIndex(cp);
        if (glyphIndex < 0) glyphIndex = font->findGlyphIndex(0xFFFD);
        if (glyphIndex < 0) glyphIndex = font->findGlyphIndex('?');
        if (glyphIndex < 0) continue;

        uint32_t bitmapOffset;
        uint8_t glyphW, glyphH, xAdvance;
        int8_t yOffset;

        if (font->getGlyphInfo(glyphIndex, bitmapOffset, glyphW, glyphH, yOffset, xAdvance)) {
            const uint8_t* bitmap = font->getBitmap();
            if (bitmap) {
                int16_t scaledYOff = static_cast<int16_t>(yOffset * scale);
                int16_t scaledAscent = static_cast<int16_t>(font->ascent() * scale);
                // Coordinates are screen-absolute, no clip offset added
                int16_t gx = cursorX;
                int16_t gy = y + scaledAscent + scaledYOff;
                drawGlyphBitmapScaled(gx, gy, glyphW, glyphH, bitmap, bitmapOffset, scale, c);
            }
            cursorX += static_cast<int16_t>(xAdvance * scale + 0.5f);
        }
        prevCp = cp;
    }
}

// === Wrapped text ===

uint16_t TextRenderer::textWrapped(int16_t x, int16_t y, uint16_t maxW, const char* str, Color c, float scale) {
    if (!str || !font || !buffer || scale <= 0) return 0;

    uint16_t lineH = static_cast<uint16_t>(font->lineHeight() * scale + 0.5f);
    int16_t cursorX = x;
    int16_t cursorY = y;
    bool prevWasLatin = false;
    bool prevWasCJK = false;

    const char* ptr = str;

    while (*ptr) {
        if (*ptr == '\n') {
            cursorX = x;
            cursorY += lineH;
            ptr++;
            continue;
        }

        if (*ptr == ' ') {
            int16_t spaceIdx = font->findGlyphIndex(' ');
            if (spaceIdx >= 0) {
                uint32_t off;
                uint8_t w, h, xAdv;
                int8_t yOff;
                if (font->getGlyphInfo(spaceIdx, off, w, h, yOff, xAdv)) {
                    cursorX += static_cast<uint8_t>(xAdv * scale + 0.5f);
                }
            }
            ptr++;
            continue;
        }

        uint32_t cp = Font::decodeUTF8(ptr);
        if (cp == 0) break;

        int16_t glyphIndex = font->findGlyphIndex(cp);
        if (glyphIndex < 0) glyphIndex = font->findGlyphIndex(0xFFFD);
        if (glyphIndex < 0) glyphIndex = font->findGlyphIndex('?');
        if (glyphIndex < 0) continue;

        uint32_t bitmapOffset;
        uint8_t glyphW, glyphH, xAdvance;
        int8_t yOffset;

        if (!font->getGlyphInfo(glyphIndex, bitmapOffset, glyphW, glyphH, yOffset, xAdvance)) {
            continue;
        }

        uint8_t scaledXAdv = static_cast<uint8_t>(xAdvance * scale + 0.5f);

        if (isCJK(cp)) {
            if (prevWasLatin && cursorX > x) {
                cursorX += static_cast<uint8_t>(SCRIPT_TRANSITION_SPACING * scale + 0.5f);
            }
            if (cursorX + scaledXAdv > x + maxW && cursorX > x) {
                cursorX = x;
                cursorY += lineH;
            }
            drawGlyphBitmapScaled(cursorX, cursorY + static_cast<int8_t>(font->ascent() * scale + 0.5f) + static_cast<int8_t>(yOffset * scale), glyphW, glyphH, font->getBitmap(), bitmapOffset, scale, c);
            cursorX += scaledXAdv;
            prevWasLatin = false;
            prevWasCJK = true;
        } else {
            if (prevWasCJK && cursorX > x) {
                cursorX += static_cast<uint8_t>(SCRIPT_TRANSITION_SPACING * scale + 0.5f);
            }

            // Find word width
            const char* wordEnd = ptr;
            uint16_t wordWidth = scaledXAdv;
            while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\n') {
                const char* temp = wordEnd;
                uint32_t nextCp = Font::decodeUTF8(temp);
                if (nextCp == 0 || isCJK(nextCp)) break;
                int16_t idx = font->findGlyphIndex(nextCp);
                if (idx < 0) idx = font->findGlyphIndex('?');
                if (idx >= 0) {
                    uint32_t off;
                    uint8_t w, h, xAdv;
                    int8_t yOff;
                    if (font->getGlyphInfo(idx, off, w, h, yOff, xAdv)) {
                        wordWidth += static_cast<uint8_t>(xAdv * scale + 0.5f);
                    }
                }
                wordEnd = temp;
            }

            if (cursorX + wordWidth > x + maxW && cursorX > x) {
                cursorX = x;
                cursorY += lineH;
            }

            bool charByChar = (wordWidth > maxW);
            if (charByChar && cursorX + scaledXAdv > x + maxW && cursorX > x) {
                cursorX = x;
                cursorY += lineH;
            }

            drawGlyphBitmapScaled(cursorX, cursorY + static_cast<int8_t>(font->ascent() * scale + 0.5f) + static_cast<int8_t>(yOffset * scale), glyphW, glyphH, font->getBitmap(), bitmapOffset, scale, c);
            cursorX += scaledXAdv;

            while (ptr < wordEnd) {
                uint32_t wcp = Font::decodeUTF8(ptr);
                int16_t wIdx = font->findGlyphIndex(wcp);
                if (wIdx < 0) wIdx = font->findGlyphIndex('?');
                if (wIdx < 0) continue;

                uint32_t wOff;
                uint8_t wW, wH, wAdv;
                int8_t wYOff;
                if (font->getGlyphInfo(wIdx, wOff, wW, wH, wYOff, wAdv)) {
                    uint8_t scaledWAdv = static_cast<uint8_t>(wAdv * scale + 0.5f);
                    if (charByChar && cursorX + scaledWAdv > x + maxW) {
                        cursorX = x;
                        cursorY += lineH;
                    }
                    drawGlyphBitmapScaled(cursorX, cursorY + static_cast<int8_t>(font->ascent() * scale + 0.5f) + static_cast<int8_t>(wYOff * scale), wW, wH, font->getBitmap(), wOff, scale, c);
                    cursorX += scaledWAdv;
                }
            }
            prevWasLatin = true;
            prevWasCJK = false;
        }
    }

    return cursorY - y + lineH;
}

uint16_t TextRenderer::getWrappedTextHeight(uint16_t maxW, const char* str, float scale) const {
    if (!str || !font || scale <= 0) return 0;

    uint16_t lineH = static_cast<uint16_t>(font->lineHeight() * scale + 0.5f);
    int16_t cursorX = 0;
    uint16_t totalH = lineH;
    bool prevWasLatin = false;
    bool prevWasCJK = false;

    const char* ptr = str;

    while (*ptr) {
        if (*ptr == '\n') {
            cursorX = 0;
            totalH += lineH;
            ptr++;
            continue;
        }

        if (*ptr == ' ') {
            int16_t spaceIdx = font->findGlyphIndex(' ');
            if (spaceIdx >= 0) {
                uint32_t off;
                uint8_t w, h, xAdv;
                int8_t yOff;
                if (font->getGlyphInfo(spaceIdx, off, w, h, yOff, xAdv)) {
                    cursorX += static_cast<uint8_t>(xAdv * scale + 0.5f);
                }
            }
            ptr++;
            continue;
        }

        uint32_t cp = Font::decodeUTF8(ptr);
        if (cp == 0) break;

        int16_t glyphIndex = font->findGlyphIndex(cp);
        if (glyphIndex < 0) glyphIndex = font->findGlyphIndex(0xFFFD);
        if (glyphIndex < 0) glyphIndex = font->findGlyphIndex('?');
        if (glyphIndex < 0) continue;

        uint32_t bitmapOffset;
        uint8_t glyphW, glyphH, xAdvance;
        int8_t yOffset;

        if (!font->getGlyphInfo(glyphIndex, bitmapOffset, glyphW, glyphH, yOffset, xAdvance)) {
            continue;
        }

        uint8_t scaledXAdv = static_cast<uint8_t>(xAdvance * scale + 0.5f);

        if (isCJK(cp)) {
            if (prevWasLatin && cursorX > 0) {
                cursorX += static_cast<uint8_t>(SCRIPT_TRANSITION_SPACING * scale + 0.5f);
            }
            if (cursorX + scaledXAdv > maxW && cursorX > 0) {
                cursorX = 0;
                totalH += lineH;
            }
            cursorX += scaledXAdv;
            prevWasLatin = false;
            prevWasCJK = true;
        } else {
            if (prevWasCJK && cursorX > 0) {
                cursorX += static_cast<uint8_t>(SCRIPT_TRANSITION_SPACING * scale + 0.5f);
            }

            const char* wordEnd = ptr;
            uint16_t wordWidth = scaledXAdv;
            while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\n') {
                const char* temp = wordEnd;
                uint32_t nextCp = Font::decodeUTF8(temp);
                if (nextCp == 0 || isCJK(nextCp)) break;
                int16_t idx = font->findGlyphIndex(nextCp);
                if (idx < 0) idx = font->findGlyphIndex('?');
                if (idx >= 0) {
                    uint32_t off;
                    uint8_t w, h, xAdv;
                    int8_t yOff;
                    if (font->getGlyphInfo(idx, off, w, h, yOff, xAdv)) {
                        wordWidth += static_cast<uint8_t>(xAdv * scale + 0.5f);
                    }
                }
                wordEnd = temp;
            }

            if (cursorX + wordWidth > maxW && cursorX > 0) {
                cursorX = 0;
                totalH += lineH;
            }

            if (wordWidth > maxW) {
                cursorX += scaledXAdv;
                while (ptr < wordEnd) {
                    uint32_t wcp = Font::decodeUTF8(ptr);
                    int16_t wIdx = font->findGlyphIndex(wcp);
                    if (wIdx < 0) wIdx = font->findGlyphIndex('?');
                    if (wIdx < 0) continue;
                    uint32_t wOff;
                    uint8_t wW, wH, wAdv;
                    int8_t wYOff;
                    if (font->getGlyphInfo(wIdx, wOff, wW, wH, wYOff, wAdv)) {
                        uint8_t scaledWAdv = static_cast<uint8_t>(wAdv * scale + 0.5f);
                        if (cursorX + scaledWAdv > maxW) {
                            cursorX = 0;
                            totalH += lineH;
                        }
                        cursorX += scaledWAdv;
                    }
                }
            } else {
                cursorX += wordWidth;
                ptr = wordEnd;
            }
            prevWasLatin = true;
            prevWasCJK = false;
        }
    }

    return totalH;
}

uint16_t TextRenderer::textWrappedTruncated(int16_t x, int16_t y, uint16_t maxW, uint16_t maxH, const char* str, Color c, float scale) {
    if (!str || !font || !buffer || scale <= 0) return 0;

    uint16_t lineH = static_cast<uint16_t>(font->lineHeight() * scale + 0.5f);

    // Simple implementation - just use scaled wrapped with height limit check
    uint16_t height = getWrappedTextHeight(maxW, str, scale);
    if (height <= maxH) {
        return textWrapped(x, y, maxW, str, c, scale);
    }

    // Truncate - render what fits and add ellipsis
    int16_t cursorX = x;
    int16_t cursorY = y;
    int16_t maxY = y + maxH - lineH;
    bool prevWasLatin = false;
    bool prevWasCJK = false;

    const char* ptr = str;

    while (*ptr) {
        if (*ptr == '\n') {
            cursorX = x;
            cursorY += lineH;
            ptr++;
            if (cursorY > maxY) {
                text(cursorX, cursorY - lineH, "...", Align::LEFT, c, scale);
                return cursorY - y;
            }
            continue;
        }

        if (*ptr == ' ') {
            int16_t spaceIdx = font->findGlyphIndex(' ');
            if (spaceIdx >= 0) {
                uint32_t off;
                uint8_t w, h, xAdv;
                int8_t yOff;
                if (font->getGlyphInfo(spaceIdx, off, w, h, yOff, xAdv)) {
                    cursorX += static_cast<uint8_t>(xAdv * scale + 0.5f);
                }
            }
            ptr++;
            continue;
        }

        uint32_t cp = Font::decodeUTF8(ptr);
        if (cp == 0) break;

        int16_t glyphIndex = font->findGlyphIndex(cp);
        if (glyphIndex < 0) glyphIndex = font->findGlyphIndex('?');
        if (glyphIndex < 0) continue;

        uint32_t bitmapOffset;
        uint8_t glyphW, glyphH, xAdvance;
        int8_t yOffset;

        if (!font->getGlyphInfo(glyphIndex, bitmapOffset, glyphW, glyphH, yOffset, xAdvance)) {
            continue;
        }

        uint8_t scaledXAdv = static_cast<uint8_t>(xAdvance * scale + 0.5f);

        if (isCJK(cp)) {
            if (prevWasLatin && cursorX > x) {
                cursorX += static_cast<uint8_t>(SCRIPT_TRANSITION_SPACING * scale + 0.5f);
            }
            if (cursorX + scaledXAdv > x + maxW && cursorX > x) {
                cursorX = x;
                cursorY += lineH;
                if (cursorY > maxY) {
                    text(x + maxW - textWidth("...", scale), cursorY - lineH, "...", Align::LEFT, c, scale);
                    return cursorY - y;
                }
            }
            if (cursorY >= maxY) {
                uint16_t ellipsisW = textWidth("...", scale);
                if (cursorX + scaledXAdv + ellipsisW > x + maxW) {
                    text(cursorX, cursorY, "...", Align::LEFT, c, scale);
                    return cursorY - y + lineH;
                }
            }
            drawGlyphBitmapScaled(cursorX, cursorY + static_cast<int8_t>(font->ascent() * scale + 0.5f) + static_cast<int8_t>(yOffset * scale), glyphW, glyphH, font->getBitmap(), bitmapOffset, scale, c);
            cursorX += scaledXAdv;
            prevWasLatin = false;
            prevWasCJK = true;
        } else {
            if (prevWasCJK && cursorX > x) {
                cursorX += static_cast<uint8_t>(SCRIPT_TRANSITION_SPACING * scale + 0.5f);
            }

            const char* wordEnd = ptr;
            uint16_t wordWidth = scaledXAdv;
            while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\n') {
                const char* temp = wordEnd;
                uint32_t nextCp = Font::decodeUTF8(temp);
                if (nextCp == 0 || isCJK(nextCp)) break;
                int16_t idx = font->findGlyphIndex(nextCp);
                if (idx < 0) idx = font->findGlyphIndex('?');
                if (idx >= 0) {
                    uint32_t off;
                    uint8_t w, h, xAdv;
                    int8_t yOff;
                    if (font->getGlyphInfo(idx, off, w, h, yOff, xAdv)) {
                        wordWidth += static_cast<uint8_t>(xAdv * scale + 0.5f);
                    }
                }
                wordEnd = temp;
            }

            if (cursorX + wordWidth > x + maxW && cursorX > x) {
                cursorX = x;
                cursorY += lineH;
                if (cursorY > maxY) {
                    text(x + maxW - textWidth("...", scale), cursorY - lineH, "...", Align::LEFT, c, scale);
                    return cursorY - y;
                }
            }

            drawGlyphBitmapScaled(cursorX, cursorY + static_cast<int8_t>(font->ascent() * scale + 0.5f) + static_cast<int8_t>(yOffset * scale), glyphW, glyphH, font->getBitmap(), bitmapOffset, scale, c);
            cursorX += scaledXAdv;

            while (ptr < wordEnd) {
                uint32_t wcp = Font::decodeUTF8(ptr);
                int16_t wIdx = font->findGlyphIndex(wcp);
                if (wIdx < 0) wIdx = font->findGlyphIndex('?');
                if (wIdx < 0) continue;

                uint32_t wOff;
                uint8_t wW, wH, wAdv;
                int8_t wYOff;
                if (font->getGlyphInfo(wIdx, wOff, wW, wH, wYOff, wAdv)) {
                    uint8_t scaledWAdv = static_cast<uint8_t>(wAdv * scale + 0.5f);
                    if (cursorX + scaledWAdv > x + maxW) {
                        cursorX = x;
                        cursorY += lineH;
                    }
                    drawGlyphBitmapScaled(cursorX, cursorY + static_cast<int8_t>(font->ascent() * scale + 0.5f) + static_cast<int8_t>(wYOff * scale), wW, wH, font->getBitmap(), wOff, scale, c);
                    cursorX += scaledWAdv;
                }
            }
            prevWasLatin = true;
            prevWasCJK = false;
        }
    }

    return cursorY - y + lineH;
}

} // namespace InkHUD2
