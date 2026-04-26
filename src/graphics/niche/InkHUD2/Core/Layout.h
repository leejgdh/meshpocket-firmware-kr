#pragma once

#include "Buffer.h"
#include "Font.h"
#include <cstdint>
#include <algorithm>

namespace InkHUD2 {

// ============================================================================
// Layout - Dynamic scaling for different screen sizes
//
// Reference screen: 200x200 (T-Echo, ThinkNode M1)
// Scaling based on screen's smaller dimension to maintain readability
//
// Supported screens:
//   - 200x200 (T-Echo, T-Echo Plus, ThinkNode M1) - scale 1.0
//   - 250x122 (T3 S3 E-Paper, Wireless Paper, Vision Master E213, MeshPocket)
//   - 296x128 (Vision Master E290)
// ============================================================================

class Layout {
public:
    // ========================================================================
    // Reference values (for 200x200 screen)
    // ========================================================================
    static constexpr uint16_t REFERENCE_SIZE = 200;
    static constexpr uint16_t MIN_DIMENSION = 100;  // Below this, don't scale down further

    // ========================================================================
    // Font Scales — design system, two tiers only.
    //
    // Modules MUST use one of these two constants. No magic literals
    // (0.67f / 0.78f / etc.), no module-local aliases. See
    // docs/inkhud2-layout-guide.md.
    // ========================================================================
    // Two-tier system. Every textual element MUST use one of these two
    // and only these two. No legacy aliases — call sites that want a
    // different meaning must add a new tier here, not invent local names.
    //
    //   bodyScale   — all primary content (NodeList rows, ChatView
    //                 messages, BootModule pairing strings, MenuModule
    //                 items, footer hints, map labels). The "regular"
    //                 tier.
    //   headerScale — ONLY for top-of-screen headers (StatusBar title).
    //                 Slightly larger than body so the header reads as a
    //                 distinct band. Don't reuse for inline emphasis.
    static constexpr float bodyScale   = 0.67f;
    static constexpr float headerScale = 0.78f;

    // ========================================================================
    // Base values (for REFERENCE_SIZE, scaled dynamically)
    // ========================================================================

    // Minimum values (never go below these)
    static constexpr uint16_t MIN_PADDING = 1;
    static constexpr uint16_t MIN_MARGIN = 1;
    static constexpr uint16_t MIN_SPACING = 1;

    // Reference values (at 200x200)
    static constexpr uint16_t REF_PADDING = 2;
    static constexpr uint16_t REF_MARGIN = 2;
    static constexpr uint16_t REF_ELEMENT_SPACING = 2;
    static constexpr uint16_t REF_SECTION_SPACING = 4;
    static constexpr uint16_t REF_CONTENT_PADDING = 4;
    static constexpr uint16_t REF_DOT_SPACING = 3;
    static constexpr uint16_t REF_TEXT_INSET = 4;
    static constexpr uint16_t REF_VERTICAL_TEXT_GAP = 10;

    // ========================================================================
    // Constructor
    // ========================================================================

    Layout(uint16_t screenW, uint16_t screenH, const Font* font)
        : screenW(screenW), screenH(screenH), font(font)
    {
        // Check if screen is elongated (aspect ratio > 1.5)
        uint16_t maxDim = std::max(screenW, screenH);
        uint16_t minDim = std::min(screenW, screenH);
        bool isElongated = (minDim > 0) && (maxDim * 10 / minDim > 15);

        if (isElongated) {
            // For elongated screens: no scaling, use reference size values directly
            scaleFactor = 1.0f;
        } else {
            // For square-ish screens: scale based on smaller dimension
            minDim = std::max(minDim, MIN_DIMENSION);
            scaleFactor = static_cast<float>(minDim) / static_cast<float>(REFERENCE_SIZE);
        }
    }

    // ========================================================================
    // Scale helper - applies scaling with minimum value
    // ========================================================================

    uint16_t scaled(uint16_t refValue, uint16_t minValue = 1) const {
        uint16_t result = static_cast<uint16_t>(refValue * scaleFactor + 0.5f);
        return std::max(result, minValue);
    }

    float getScaleFactor() const { return scaleFactor; }

    // ========================================================================
    // Screen dimensions
    // ========================================================================

    uint16_t width() const { return screenW; }
    uint16_t height() const { return screenH; }
    bool isWideScreen() const { return screenW > screenH * 1.5f; }
    bool isSquareScreen() const { return screenW == screenH; }
    bool isVertical() const { return screenH > screenW * 1.5f; }  // Tall narrow screen
    bool isNarrow() const { return screenW < 150; }  // Very narrow (e.g., 128px)

    // ========================================================================
    // Font metrics (from font, with scaling for fallback)
    // ========================================================================

    uint16_t lineHeight() const {
        if (font) return font->lineHeight();
        return scaled(18, 12);  // Fallback: 18px ref, min 12px
    }

    // Two-tier line heights matching the two scales.
    uint16_t bodyLineHeight()   const {
        return static_cast<uint16_t>(lineHeight() * bodyScale);
    }
    uint16_t headerLineHeight() const {
        return static_cast<uint16_t>(lineHeight() * headerScale);
    }

    // ========================================================================
    // Spacing (all scaled)
    // ========================================================================

    uint16_t margin() const {
        return scaled(REF_MARGIN, MIN_MARGIN);
    }

    uint16_t padding() const {
        return scaled(REF_PADDING, MIN_PADDING);
    }

    uint16_t elementSpacing() const {
        return scaled(REF_ELEMENT_SPACING, MIN_SPACING);
    }

    uint16_t sectionSpacing() const {
        return scaled(REF_SECTION_SPACING, MIN_SPACING);
    }

    uint16_t slotSpacing() const {
        return sectionSpacing();
    }

    uint16_t contentPadding() const {
        return scaled(REF_CONTENT_PADDING, MIN_PADDING);
    }

    uint16_t dotSpacing() const {
        return scaled(REF_DOT_SPACING, 2);
    }

    uint16_t textInset() const {
        return scaled(REF_TEXT_INSET, 2);
    }

    uint16_t verticalTextGap() const {
        return scaled(REF_VERTICAL_TEXT_GAP, 4);
    }

    // ========================================================================
    // Header
    // ========================================================================

    uint16_t headerHeight() const {
        return padding() + lineHeight() + padding();
    }

    // ========================================================================
    // Menu (scaled from lineHeight)
    // ========================================================================

    uint16_t menuMargin() const {
        return lineHeight() / 3;
    }

    uint16_t statusBarMargin() const {
        return lineHeight() / 6;
    }

    uint16_t menuColumnPadding() const {
        return scaled(6, 2);
    }

    uint16_t menuItemPadding() const {
        return scaled(2, 1);
    }

    uint16_t menuItemSpacing() const {
        return scaled(1, 1);
    }

    uint16_t tipBoxHeight() const {
        return lineHeight() * 4;
    }

    // ========================================================================
    // Battery Icon (proportional to lineHeight)
    // ========================================================================

    uint16_t batteryWidth() const {
        return lineHeight() + scaled(4, 2);
    }

    uint16_t batteryHeight() const {
        // 3/4 of line height - matches envelope icon visual size
        return lineHeight() * 3 / 4;
    }

    uint16_t batteryBumpWidth() const {
        return scaled(2, 1);
    }

    uint16_t batteryBodyInset() const {
        return scaled(1, 1);
    }

    uint16_t batterySlicePad() const {
        return scaled(2, 1);
    }

    uint16_t batteryHatchSpacing() const {
        return scaled(2, 2);
    }

    // ========================================================================
    // Map Module (scaled)
    // ========================================================================

    uint16_t mapEdgePadding() const {
        return scaled(2, 1);
    }

    uint16_t mapContentGap() const {
        return scaled(4, 2);
    }

    uint16_t mapScalePadding() const {
        return scaled(10, 4);
    }

    int16_t clusterRadius() const {
        return static_cast<int16_t>(scaled(12, 6));
    }

    // Node sizes
    uint16_t nodeRadiusDirect() const {
        return scaled(5, 3);
    }

    uint16_t nodeRadiusNear() const {
        return scaled(4, 2);
    }

    uint16_t nodeRadiusFar() const {
        return scaled(3, 2);
    }

    uint16_t clusterNodeRadius() const {
        return scaled(6, 3);
    }

    uint16_t clusterLabelGap() const {
        return scaled(16, 8);
    }

    // Own node (bullseye)
    uint16_t ownNodeDotRadius() const {
        return scaled(3, 2);
    }

    uint16_t ownNodeArmLength() const {
        return scaled(8, 4);
    }

    // Scale bar
    uint16_t scaleBarTargetWidth() const {
        return scaled(40, 20);
    }

    uint16_t scaleBarMinWidth() const {
        return scaled(20, 10);
    }

    uint16_t scaleBarMaxWidth() const {
        return scaled(60, 30);
    }

    uint16_t scaleBarCapHeight() const {
        return scaled(5, 3);
    }

    // ========================================================================
    // NodeList Module (scaled)
    // ========================================================================

    uint16_t nodeCardMargin() const {
        return scaled(2, 1);
    }

    uint16_t nodeTextInset() const {
        return scaled(2, 1);
    }

    uint16_t signalBarsOffset() const {
        return scaled(24, 12);
    }

    uint16_t signalBarWidth() const {
        return scaled(3, 2);
    }

    uint16_t signalBarSpacing() const {
        return scaled(2, 1);
    }

    uint16_t signalBarPadding() const {
        return scaled(4, 2);
    }

    // ========================================================================
    // Boot/Shutdown Module (percentages - no scaling needed)
    // ========================================================================

    static constexpr uint16_t logoWidthPercent = 80;
    static constexpr uint16_t logoHeightPercent = 50;
    static constexpr uint16_t logoCenterOffset = 5;
    static constexpr uint16_t logoTextGap = 10;

    static constexpr uint16_t pairingHeaderY = 25;
    static constexpr uint16_t pairingPinY = 50;
    static constexpr uint16_t pairingNameY = 75;

    static constexpr uint16_t shutdownLogoWidthPercent = 60;
    static constexpr uint16_t shutdownLogoHeightPercent = 40;

    // ========================================================================
    // Footer (scaled)
    // ========================================================================

    uint16_t footerHeight() const {
        return lineHeight() - scaled(4, 2);
    }

    uint16_t tabSize() const {
        return lineHeight() / 2 + 1;
    }

    uint16_t tabGap() const {
        return scaled(2, 1);
    }

    // ========================================================================
    // Compass (scaled)
    // ========================================================================

    uint16_t compassRadius() const {
        return scaled(28, 14);
    }

    uint16_t compassMargin() const {
        return scaled(5, 2);
    }

    // ========================================================================
    // Screen Regions
    // ========================================================================

    // Safe area excludes battery icon
    Rect safeArea() const {
        return {
            static_cast<int16_t>(margin()),
            static_cast<int16_t>(margin()),
            static_cast<uint16_t>(screenW - 2 * margin() - batteryWidth() - padding()),
            static_cast<uint16_t>(screenH - 2 * margin())
        };
    }

    // Full content area
    Rect contentArea() const {
        return {
            static_cast<int16_t>(margin()),
            static_cast<int16_t>(margin()),
            static_cast<uint16_t>(screenW - 2 * margin()),
            static_cast<uint16_t>(screenH - 2 * margin())
        };
    }

    // Battery icon position (top-right, vertically centered in header)
    // Must match StatusBar icon positioning: y = padding + lineH/2 - iconH/2
    Rect batteryRect() const {
        Rect content = contentArea();
        // Same formula as StatusBar: y + lineH/2 - height/2, where y = padding
        int16_t batteryY = content.y + padding() + lineHeight() / 2 - batteryHeight() / 2;
        return {
            static_cast<int16_t>(content.x + content.w - batteryWidth()),
            batteryY,
            batteryWidth(),
            batteryHeight()
        };
    }

    // Notification bar
    Rect notificationRect() const {
        return {
            static_cast<int16_t>(margin()),
            static_cast<int16_t>(margin() + headerHeight() + padding()),
            static_cast<uint16_t>(screenW - 2 * margin()),
            static_cast<uint16_t>(headerHeight())
        };
    }

    // Slot layout for modules (1, 2, or 4 slots)
    Rect slotRect(uint8_t slotIndex, uint8_t totalSlots) const {
        Rect content = contentArea();

        switch (totalSlots) {
            case 1:
                return content;

            case 2: {
                // For wide screens: side by side; for square: stacked
                if (isWideScreen()) {
                    uint16_t slotW = (content.w - slotSpacing()) / 2;
                    return {
                        static_cast<int16_t>(content.x + slotIndex * (slotW + slotSpacing())),
                        content.y,
                        slotW,
                        content.h
                    };
                } else {
                    uint16_t slotH = (content.h - slotSpacing()) / 2;
                    return {
                        content.x,
                        static_cast<int16_t>(content.y + slotIndex * (slotH + slotSpacing())),
                        content.w,
                        slotH
                    };
                }
            }

            case 4:
            default: {
                uint16_t slotW = (content.w - slotSpacing()) / 2;
                uint16_t slotH = (content.h - slotSpacing()) / 2;
                uint8_t col = slotIndex % 2;
                uint8_t row = slotIndex / 2;
                return {
                    static_cast<int16_t>(content.x + col * (slotW + slotSpacing())),
                    static_cast<int16_t>(content.y + row * (slotH + slotSpacing())),
                    slotW,
                    slotH
                };
            }
        }
    }

    // Centered logo area
    Rect logoRect(uint16_t logoW, uint16_t logoH) const {
        return {
            static_cast<int16_t>((screenW - logoW) / 2),
            static_cast<int16_t>((screenH - logoH) / 2),
            logoW,
            logoH
        };
    }

private:
    uint16_t screenW;
    uint16_t screenH;
    const Font* font;
    float scaleFactor;
};

} // namespace InkHUD2
