#pragma once

#include "../Core/RenderContext.h"
#include "../Text/TextRenderer.h"
#include <cstdint>

namespace InkHUD2 {

// Text utilities - truncation, measurement, formatting
class TextUtils {
public:
    // Truncate text to fit within maxWidth, adding "..." if needed
    // Returns truncated string in outBuf
    // scale = text scale factor (1.0 = normal)
    static void truncate(const RenderContext& ctx, const char* text, uint16_t maxWidth,
                         float scale, char* outBuf, size_t outBufSize);

    // Same but using TextRenderer directly
    static void truncate(const TextRenderer* renderer, const char* text, uint16_t maxWidth,
                         float scale, char* outBuf, size_t outBufSize);

    // Check if text fits within maxWidth
    static bool fits(const RenderContext& ctx, const char* text, uint16_t maxWidth, float scale);
    static bool fits(const TextRenderer* renderer, const char* text, uint16_t maxWidth, float scale);

private:
    static constexpr const char* ELLIPSIS = "...";
};

} // namespace InkHUD2
