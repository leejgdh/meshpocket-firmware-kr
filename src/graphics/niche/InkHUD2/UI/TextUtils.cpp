/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "TextUtils.h"
#include <cstring>

namespace InkHUD2 {

// Helper: find UTF-8 character boundary (don't cut in the middle of multi-byte char)
static size_t findUtf8Boundary(const char* text, size_t maxLen) {
    if (maxLen == 0) return 0;

    size_t len = maxLen;
    // Walk back if we're in the middle of a UTF-8 sequence
    while (len > 0 && (text[len] & 0xC0) == 0x80) {
        len--;
    }
    return len;
}

void TextUtils::truncate(const RenderContext& ctx, const char* text, uint16_t maxWidth,
                         float scale, char* outBuf, size_t outBufSize) {
    if (!text || outBufSize == 0) {
        if (outBuf && outBufSize > 0) outBuf[0] = '\0';
        return;
    }

    // Check if text fits
    uint16_t textW = ctx.textWidth(text, scale);
    if (textW <= maxWidth) {
        strncpy(outBuf, text, outBufSize - 1);
        outBuf[outBufSize - 1] = '\0';
        return;
    }

    // Need to truncate
    uint16_t ellipsisW = ctx.textWidth(ELLIPSIS, scale);
    if (ellipsisW >= maxWidth) {
        outBuf[0] = '\0';
        return;
    }

    uint16_t availW = maxWidth - ellipsisW;
    size_t len = strlen(text);
    if (len >= outBufSize - 3) len = outBufSize - 4;

    // Find truncation point (UTF-8 aware)
    size_t fitLen = 0;
    for (size_t i = 1; i <= len; i++) {
        strncpy(outBuf, text, i);
        outBuf[i] = '\0';
        if (ctx.textWidth(outBuf, scale) > availW) break;
        fitLen = i;
    }

    // Ensure we don't cut in middle of UTF-8 char
    fitLen = findUtf8Boundary(text, fitLen);

    if (fitLen > 0) {
        strncpy(outBuf, text, fitLen);
        strcpy(outBuf + fitLen, ELLIPSIS);
    } else {
        strcpy(outBuf, ELLIPSIS);
    }
}

void TextUtils::truncate(const TextRenderer* renderer, const char* text, uint16_t maxWidth,
                         float scale, char* outBuf, size_t outBufSize) {
    if (!renderer || !text || outBufSize == 0) {
        if (outBuf && outBufSize > 0) outBuf[0] = '\0';
        return;
    }

    // Check if text fits
    uint16_t textW = renderer->textWidth(text, scale);
    if (textW <= maxWidth) {
        strncpy(outBuf, text, outBufSize - 1);
        outBuf[outBufSize - 1] = '\0';
        return;
    }

    // Need to truncate
    uint16_t ellipsisW = renderer->textWidth(ELLIPSIS, scale);
    if (ellipsisW >= maxWidth) {
        outBuf[0] = '\0';
        return;
    }

    uint16_t availW = maxWidth - ellipsisW;
    size_t len = strlen(text);
    if (len >= outBufSize - 3) len = outBufSize - 4;

    // Find truncation point (UTF-8 aware)
    size_t fitLen = 0;
    for (size_t i = 1; i <= len; i++) {
        strncpy(outBuf, text, i);
        outBuf[i] = '\0';
        if (renderer->textWidth(outBuf, scale) > availW) break;
        fitLen = i;
    }

    // Ensure we don't cut in middle of UTF-8 char
    fitLen = findUtf8Boundary(text, fitLen);

    if (fitLen > 0) {
        strncpy(outBuf, text, fitLen);
        strcpy(outBuf + fitLen, ELLIPSIS);
    } else {
        strcpy(outBuf, ELLIPSIS);
    }
}

bool TextUtils::fits(const RenderContext& ctx, const char* text, uint16_t maxWidth, float scale) {
    if (!text) return true;
    return ctx.textWidth(text, scale) <= maxWidth;
}

bool TextUtils::fits(const TextRenderer* renderer, const char* text, uint16_t maxWidth, float scale) {
    if (!renderer || !text) return true;
    return renderer->textWidth(text, scale) <= maxWidth;
}

} // namespace InkHUD2
