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

void TextRenderer::drawGlyphBitmap(int16_t gx, int16_t gy, uint8_t width, uint8_t height,
                                    const uint8_t* bitmap, uint32_t offset, Color c) {
    if (!buffer || !bitmap) return;

    uint32_t bitIndex = 0;
    for (uint8_t row = 0; row < height; row++) {
        for (uint8_t col = 0; col < width; col++) {
            uint32_t byteIdx = offset + (bitIndex >> 3);
            uint8_t bitPos = 7 - (bitIndex & 7);
            if (bitmap[byteIdx] & (1 << bitPos)) {
                int16_t px = gx + col;
                int16_t py = gy + row;
                if (px >= 0 && py >= 0 &&
                    px < static_cast<int16_t>(buffer->width()) &&
                    py < static_cast<int16_t>(buffer->height())) {
                    buffer->setPixel(px, py, c);
                }
            }
            bitIndex++;
        }
    }
}

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
                if (px >= 0 && py >= 0 &&
                    px < static_cast<int16_t>(buffer->width()) &&
                    py < static_cast<int16_t>(buffer->height())) {
                    buffer->setPixel(px, py, c);
                }
            }
        }
    }
}

// === Basic text ===

uint16_t TextRenderer::textWidth(const char* str) const {
    if (!str || !font) return 0;

    uint16_t width = 0;
    const char* ptr = str;
    uint32_t prevCp = 0;

    while (*ptr) {
        uint32_t cp = Font::decodeUTF8(ptr);
        if (cp == 0) break;

        // Add spacing when transitioning from Latin/Cyrillic to CJK
        if (isLatinOrCyrillic(prevCp) && isCJK(cp)) {
            width += SCRIPT_TRANSITION_SPACING;
        }

        int16_t glyphIndex = font->findGlyphIndex(cp);
        if (glyphIndex < 0) glyphIndex = font->findGlyphIndex(0xFFFD);
        if (glyphIndex < 0) glyphIndex = font->findGlyphIndex('?');
        if (glyphIndex < 0) continue;

        uint32_t bitmapOffset;
        uint8_t glyphW, glyphH, xAdvance;
        int8_t yOffset;

        if (font->getGlyphInfo(glyphIndex, bitmapOffset, glyphW, glyphH, yOffset, xAdvance)) {
            width += xAdvance;
        }
        prevCp = cp;
    }

    return width;
}

void TextRenderer::text(int16_t x, int16_t y, const char* str, Align align, Color c) {
    if (!str || !font || !buffer) return;

    int16_t cursorX = x;

    if (align == Align::CENTER) {
        cursorX = x - textWidth(str) / 2;
    } else if (align == Align::RIGHT) {
        cursorX = x - textWidth(str);
    }

    const char* ptr = str;
    uint32_t prevCp = 0;

    while (*ptr) {
        uint32_t cp = Font::decodeUTF8(ptr);
        if (cp == 0) break;

        // Add spacing when transitioning from Latin/Cyrillic to CJK
        if (isLatinOrCyrillic(prevCp) && isCJK(cp)) {
            cursorX += SCRIPT_TRANSITION_SPACING;
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
                // Coordinates are screen-absolute, no clip offset added
                int16_t gx = cursorX;
                int16_t gy = y + font->ascent() + yOffset;
                drawGlyphBitmap(gx, gy, glyphW, glyphH, bitmap, bitmapOffset, c);
            }
            cursorX += xAdvance;
        }
        prevCp = cp;
    }
}

// === Scaled text ===

uint16_t TextRenderer::textWidthScaled(const char* str, float scale) const {
    if (!str || !font || scale <= 0) return 0;

    // Calculate width by summing individually rounded advances
    // to match the actual rendering behavior
    uint16_t width = 0;
    const char* ptr = str;
    uint32_t prevCp = 0;

    while (*ptr) {
        uint32_t cp = Font::decodeUTF8(ptr);
        if (cp == 0) break;

        // Add spacing when transitioning from Latin/Cyrillic to CJK
        if (isLatinOrCyrillic(prevCp) && isCJK(cp)) {
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
            // Round each advance individually, same as in textScaled()
            width += static_cast<uint16_t>(xAdvance * scale + 0.5f);
        }
        prevCp = cp;
    }

    return width;
}

void TextRenderer::textScaled(int16_t x, int16_t y, const char* str, float scale, Align align, Color c) {
    if (!str || !font || !buffer || scale <= 0) return;

    int16_t cursorX = x;
    uint16_t scaledWidth = textWidthScaled(str, scale);

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

        // Add spacing when transitioning from Latin/Cyrillic to CJK
        if (isLatinOrCyrillic(prevCp) && isCJK(cp)) {
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

uint16_t TextRenderer::textWrapped(int16_t x, int16_t y, uint16_t maxW, const char* str, Color c) {
    if (!str || !font || !buffer) return 0;

    uint16_t lineH = font->lineHeight();
    int16_t cursorX = x;
    int16_t cursorY = y;
    bool prevWasLatin = false;

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
                    cursorX += xAdv;
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

        if (isCJK(cp)) {
            if (prevWasLatin && cursorX > x) {
                cursorX += SCRIPT_TRANSITION_SPACING;
            }

            if (cursorX + xAdvance > x + maxW && cursorX > x) {
                cursorX = x;
                cursorY += lineH;
            }

            const uint8_t* bitmap = font->getBitmap();
            if (bitmap) {
                int16_t gx = cursorX;
                int16_t gy = cursorY + font->ascent() + yOffset;
                drawGlyphBitmap(gx, gy, glyphW, glyphH, bitmap, bitmapOffset, c);
            }
            cursorX += xAdvance;
            prevWasLatin = false;
        } else {
            const char* wordEnd = ptr;
            uint16_t wordWidth = xAdvance;

            while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\n') {
                const char* temp = wordEnd;
                uint32_t nextCp = Font::decodeUTF8(temp);
                if (nextCp == 0 || isCJK(nextCp)) break;

                int16_t idx = font->findGlyphIndex(nextCp);
                if (idx < 0) idx = font->findGlyphIndex(0xFFFD);
                if (idx < 0) idx = font->findGlyphIndex('?');
                if (idx >= 0) {
                    uint32_t off;
                    uint8_t w, h, xAdv;
                    int8_t yOff;
                    if (font->getGlyphInfo(idx, off, w, h, yOff, xAdv)) {
                        wordWidth += xAdv;
                    }
                }
                wordEnd = temp;
            }

            if (cursorX + wordWidth > x + maxW && cursorX > x) {
                cursorX = x;
                cursorY += lineH;
            }

            bool charByChar = (wordWidth > maxW);

            if (charByChar && cursorX + xAdvance > x + maxW && cursorX > x) {
                cursorX = x;
                cursorY += lineH;
            }

            const uint8_t* bitmap = font->getBitmap();
            if (bitmap) {
                int16_t gx = cursorX;
                int16_t gy = cursorY + font->ascent() + yOffset;
                drawGlyphBitmap(gx, gy, glyphW, glyphH, bitmap, bitmapOffset, c);
            }
            cursorX += xAdvance;

            while (ptr < wordEnd) {
                uint32_t wcp = Font::decodeUTF8(ptr);
                int16_t wIdx = font->findGlyphIndex(wcp);
                if (wIdx < 0) wIdx = font->findGlyphIndex(0xFFFD);
                if (wIdx < 0) wIdx = font->findGlyphIndex('?');
                if (wIdx < 0) continue;

                uint32_t wOff;
                uint8_t wW, wH, wAdv;
                int8_t wYOff;

                if (font->getGlyphInfo(wIdx, wOff, wW, wH, wYOff, wAdv)) {
                    if (charByChar && cursorX + wAdv > x + maxW) {
                        cursorX = x;
                        cursorY += lineH;
                    }
                    const uint8_t* bmp = font->getBitmap();
                    if (bmp) {
                        int16_t gx = cursorX;
                        int16_t gy = cursorY + font->ascent() + wYOff;
                        drawGlyphBitmap(gx, gy, wW, wH, bmp, wOff, c);
                    }
                    cursorX += wAdv;
                }
            }
            prevWasLatin = true;
        }
    }

    return cursorY - y + lineH;
}

uint16_t TextRenderer::getWrappedTextHeight(uint16_t maxW, const char* str) const {
    if (!str || !font) return 0;

    uint16_t lineH = font->lineHeight();
    int16_t cursorX = 0;
    uint16_t totalH = lineH;
    bool prevWasLatin = false;

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
                    cursorX += xAdv;
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

        if (isCJK(cp)) {
            if (prevWasLatin && cursorX > 0) {
                cursorX += SCRIPT_TRANSITION_SPACING;
            }
            if (cursorX + xAdvance > maxW && cursorX > 0) {
                cursorX = 0;
                totalH += lineH;
            }
            cursorX += xAdvance;
            prevWasLatin = false;
        } else {
            const char* wordEnd = ptr;
            uint16_t wordWidth = xAdvance;

            while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\n') {
                const char* temp = wordEnd;
                uint32_t nextCp = Font::decodeUTF8(temp);
                if (nextCp == 0 || isCJK(nextCp)) break;

                int16_t idx = font->findGlyphIndex(nextCp);
                if (idx < 0) idx = font->findGlyphIndex(0xFFFD);
                if (idx < 0) idx = font->findGlyphIndex('?');
                if (idx >= 0) {
                    uint32_t off;
                    uint8_t w, h, xAdv;
                    int8_t yOff;
                    if (font->getGlyphInfo(idx, off, w, h, yOff, xAdv)) {
                        wordWidth += xAdv;
                    }
                }
                wordEnd = temp;
            }

            if (cursorX + wordWidth > maxW && cursorX > 0) {
                cursorX = 0;
                totalH += lineH;
            }

            if (wordWidth > maxW) {
                cursorX += xAdvance;
                while (ptr < wordEnd) {
                    uint32_t wcp = Font::decodeUTF8(ptr);
                    int16_t wIdx = font->findGlyphIndex(wcp);
                    if (wIdx < 0) wIdx = font->findGlyphIndex(0xFFFD);
                if (wIdx < 0) wIdx = font->findGlyphIndex('?');
                    if (wIdx < 0) continue;

                    uint32_t wOff;
                    uint8_t wW, wH, wAdv;
                    int8_t wYOff;

                    if (font->getGlyphInfo(wIdx, wOff, wW, wH, wYOff, wAdv)) {
                        if (cursorX + wAdv > maxW) {
                            cursorX = 0;
                            totalH += lineH;
                        }
                        cursorX += wAdv;
                    }
                }
            } else {
                cursorX += wordWidth;
                ptr = wordEnd;
            }
            prevWasLatin = true;
        }
    }

    return totalH;
}

uint16_t TextRenderer::textWrappedTruncated(int16_t x, int16_t y, uint16_t maxW, uint16_t maxH, const char* str, Color c) {
    if (!str || !font || !buffer) return 0;

    uint16_t lineH = font->lineHeight();

    // If only one line fits, use simple truncation
    if (maxH < lineH * 2) {
        uint16_t fullW = textWidth(str);
        if (fullW <= maxW) {
            text(x, y, str, Align::LEFT, c);
            return lineH;
        }

        const char* ptr = str;
        uint16_t cursorX = 0;
        uint16_t ellipsisW = textWidth("…");
        const char* lastFit = str;

        while (*ptr) {
            uint32_t cp = Font::decodeUTF8(ptr);
            if (cp == 0) break;

            int16_t idx = font->findGlyphIndex(cp);
            if (idx < 0) idx = font->findGlyphIndex(0xFFFD);
                if (idx < 0) idx = font->findGlyphIndex('?');
            if (idx < 0) continue;

            uint32_t off;
            uint8_t w, h, xAdv;
            int8_t yOff;
            if (font->getGlyphInfo(idx, off, w, h, yOff, xAdv)) {
                if (cursorX + xAdv + ellipsisW > maxW) break;
                cursorX += xAdv;
                lastFit = ptr;
            }
        }

        size_t len = lastFit - str;
        char truncated[256];
        if (len >= sizeof(truncated) - 4) len = sizeof(truncated) - 5;
        memcpy(truncated, str, len);
        truncated[len] = '.';
        truncated[len+1] = '.';
        truncated[len+2] = '.';
        truncated[len+3] = '\0';
        text(x, y, truncated, Align::LEFT, c);
        return lineH;
    }

    // Multiple lines - render with truncation
    int16_t cursorX = x;
    int16_t cursorY = y;
    int16_t maxY = y + maxH - lineH;
    bool prevWasLatin = false;

    const char* ptr = str;

    while (*ptr) {
        if (*ptr == '\n') {
            cursorX = x;
            cursorY += lineH;
            ptr++;
            if (cursorY > maxY && *ptr) {
                text(cursorX, cursorY - lineH, "…", Align::LEFT, c);
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
                    cursorX += xAdv;
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

        if (isCJK(cp)) {
            if (prevWasLatin && cursorX > x) {
                cursorX += SCRIPT_TRANSITION_SPACING;
            }

            // Check if this char needs line wrap
            if (cursorX + xAdvance > x + maxW && cursorX > x) {
                // Would wrap - check if we're at max height
                if (cursorY + lineH > maxY) {
                    // No room for next line - add "…" at end of current line
                    text(cursorX + 2, cursorY, "…", Align::LEFT, c);
                    return cursorY - y + lineH;
                }
                cursorX = x;
                cursorY += lineH;
            }

            // On last line - check if char + ellipsis fits
            if (cursorY >= maxY) {
                uint16_t ellipsisW = textWidth("…");
                if (cursorX + xAdvance + 2 + ellipsisW > x + maxW) {
                    text(cursorX + 2, cursorY, "…", Align::LEFT, c);
                    return cursorY - y + lineH;
                }
            }

            const uint8_t* bitmap = font->getBitmap();
            if (bitmap) {
                int16_t gx = cursorX;
                int16_t gy = cursorY + font->ascent() + yOffset;
                drawGlyphBitmap(gx, gy, glyphW, glyphH, bitmap, bitmapOffset, c);
            }
            cursorX += xAdvance;
            prevWasLatin = false;
        } else {
            const char* wordEnd = ptr;
            uint16_t wordWidth = xAdvance;

            while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\n') {
                const char* temp = wordEnd;
                uint32_t nextCp = Font::decodeUTF8(temp);
                if (nextCp == 0 || isCJK(nextCp)) break;

                int16_t idx = font->findGlyphIndex(nextCp);
                if (idx < 0) idx = font->findGlyphIndex(0xFFFD);
                if (idx < 0) idx = font->findGlyphIndex('?');
                if (idx >= 0) {
                    uint32_t off;
                    uint8_t w, h, xAdv;
                    int8_t yOff;
                    if (font->getGlyphInfo(idx, off, w, h, yOff, xAdv)) {
                        wordWidth += xAdv;
                    }
                }
                wordEnd = temp;
            }

            if (cursorX + wordWidth > x + maxW && cursorX > x) {
                cursorX = x;
                cursorY += lineH;
                if (cursorY > maxY) {
                    text(x + maxW - textWidth("…"), cursorY - lineH, "…", Align::LEFT, c);
                    return cursorY - y;
                }
            }

            // Check if on last line and there's more content
            if (cursorY >= maxY) {
                const char* check = wordEnd;
                bool hasMore = false;
                while (*check) {
                    if (*check == ' ') { check++; continue; }
                    hasMore = true;
                    break;
                }

                if (hasMore) {
                    uint16_t ellipsisW = textWidth("…");
                    if (cursorX + wordWidth + ellipsisW > x + maxW) {
                        text(cursorX, cursorY, "…", Align::LEFT, c);
                        return cursorY - y + lineH;
                    }
                }
            }

            // Draw word
            bool charByChar = (wordWidth > maxW);

            if (charByChar && cursorX + xAdvance > x + maxW && cursorX > x) {
                cursorX = x;
                cursorY += lineH;
            }

            const uint8_t* bitmap = font->getBitmap();
            if (bitmap) {
                int16_t gx = cursorX;
                int16_t gy = cursorY + font->ascent() + yOffset;
                drawGlyphBitmap(gx, gy, glyphW, glyphH, bitmap, bitmapOffset, c);
            }
            cursorX += xAdvance;

            while (ptr < wordEnd) {
                uint32_t wcp = Font::decodeUTF8(ptr);
                int16_t wIdx = font->findGlyphIndex(wcp);
                if (wIdx < 0) wIdx = font->findGlyphIndex(0xFFFD);
                if (wIdx < 0) wIdx = font->findGlyphIndex('?');
                if (wIdx < 0) continue;

                uint32_t wOff;
                uint8_t wW, wH, wAdv;
                int8_t wYOff;

                if (font->getGlyphInfo(wIdx, wOff, wW, wH, wYOff, wAdv)) {
                    if (charByChar && cursorX + wAdv > x + maxW) {
                        cursorX = x;
                        cursorY += lineH;
                    }
                    const uint8_t* bmp = font->getBitmap();
                    if (bmp) {
                        int16_t gx = cursorX;
                        int16_t gy = cursorY + font->ascent() + wYOff;
                        drawGlyphBitmap(gx, gy, wW, wH, bmp, wOff, c);
                    }
                    cursorX += wAdv;
                }
            }
            prevWasLatin = true;
        }
    }

    return cursorY - y + lineH;
}

} // namespace InkHUD2
