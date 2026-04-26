/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "NodeListModule.h"
#include "MenuModule.h"
#include "../Text/TextRenderer.h"
#include "../UI/StatusBar.h"
#include "../UI/ContentArea.h"
#include "../UI/HeaderText.h"
#include "gps/RTC.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace InkHUD2 {

// Wi-Fi/RSS-style icon for MQTT-bridged nodes: three concentric quarter
// arcs (top-right quadrant) blooming out of an origin dot at bottom-left.
// Line A's text runs at full lineHeight, so the icon can be sized to match.
// 2px stroke (arc at r and r+1) at radii 3,7,11 — 4px radial spacing leaves
// a clean 2px gap between bands.
namespace {
constexpr int16_t WIFI_RADII[3] = {3, 7, 11};
constexpr int16_t WIFI_W = 13;  // max radius + stroke + origin
constexpr int16_t WIFI_H = 13;

void drawArcQuadrant(RenderContext& ctx, int16_t cx, int16_t cy, int16_t r, Color c) {
    // Bresenham midpoint, top-right quadrant only: from (r, 0) up to (0, r).
    // setPixelClipped is private on RenderContext, so we draw 1x1 fillRects.
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;
    while (x >= y) {
        ctx.fillRect(cx + x, cy - y, 1, 1, c);
        ctx.fillRect(cx + y, cy - x, 1, 1, c);
        if (err <= 0) {
            y++;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

void drawWifiIcon(RenderContext& ctx, int16_t left, int16_t top, Color c) {
    int16_t cx = left;                  // origin at bottom-left of bounding box
    int16_t cy = top + WIFI_H - 1;
    for (int i = 0; i < 3; ++i) {
        // 2px stroke: arc at r and r+1, leaving a 2px gap before the next band.
        drawArcQuadrant(ctx, cx, cy, WIFI_RADII[i], c);
        drawArcQuadrant(ctx, cx, cy, WIFI_RADII[i] + 1, c);
    }
    // Origin "source" dot — 2x2 block to anchor the bloom visually.
    ctx.fillRect(cx, cy - 1, 2, 2, c);
}
}  // namespace


NodeListModule::NodeListModule() {
    nodes.reserve(64);
}

void NodeListModule::onRender(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    Buffer* buffer = ctx.getBuffer();
    const Font* font = ctx.getFont();
    if (!buffer || !font) return;

    uint16_t padding = layout->padding();

    // Create TextRenderer
    TextRenderer textRenderer(buffer, font);
    textRenderer.setClip(ctx.clip().x, ctx.clip().y, ctx.clip().w, ctx.clip().h);

    // Build header text with smart truncation
    HeaderText header(HeaderText::Type::CUSTOM);
    switch (currentView) {
        case NodeListView::ALL:
            header = HeaderText(HeaderText::Type::ALL_NODES, static_cast<int>(nodes.size()));
            break;
        case NodeListView::RECENT:
            header = HeaderText(HeaderText::Type::HEARD, static_cast<int>(sortedNodes().size()));
            break;
        case NodeListView::FAVORITES: {
            size_t count = 0;
            for (const auto& n : nodes) if (n.isFavorite) count++;
            header = HeaderText(HeaderText::Type::FAVORITES, static_cast<int>(count));
            break;
        }
    }

    // === Render StatusBar ===
    // Calculate max title width (leave space for battery icon)
    Rect batRect = layout->batteryRect();
    uint16_t iconW = layout->lineHeight();
    uint16_t maxTitleW = batRect.x - layout->margin() - padding - iconW - padding * 2;
    const char* title = header.getText(&textRenderer, maxTitleW, StatusBar::TITLE_SCALE);

    StatusBar statusBar(buffer, layout, &textRenderer);
    int16_t contentTop = statusBar.render(layout->margin() + padding, title, StatusBar::Icon::USERS);

    // === Calculate content area (no footer for NodeList) ===
    int16_t contentBottom = ctx.height();
    ContentArea content = calculateContentArea(layout, contentTop, contentBottom);

    // === Render node list ===
    renderNodeList(ctx, content);
}

void NodeListModule::onEvent(const Event& e) {
    switch (e.type) {
        case EventType::NODE_DISCOVERED:
        case EventType::NODE_UPDATED:
            requestUpdate();
            break;

        case EventType::NODE_LOST:
            removeNode(e.data.node.nodeNum);
            break;

        default:
            break;
    }
}

void NodeListModule::onInput(Input input) {
    switch (input) {
        case Input::LEFT:
        case Input::RIGHT:
            cycleView();
            break;

        case Input::DOWN:
            // Scroll down one node (only if there are hidden nodes below)
            {
                auto sorted = sortedNodes();
                size_t totalNodes = sorted.size();
                // Only scroll if there are more nodes than fit on screen
                if (totalNodes > visibleNodeCount && scrollOffset < totalNodes - visibleNodeCount) {
                    scrollOffset++;
                    requestUpdate();
                }
            }
            break;

        case Input::UP:
            // Return to beginning
            if (scrollOffset > 0) {
                scrollOffset = 0;
                requestUpdate();
            }
            break;

        // LONG_PRESS is handled at the HUD level (cycles to next layout) —
        // we just don't claim it, so it falls through with handlesInput=false.

        case Input::DOUBLE_TAP:
            // Double tap on a module's main screen opens that module's menu.
            // For NodeList the "module menu" is the global system menu.
            if (menuModule) {
                menuModule->open();
            }
            break;

        default:
            break;
    }
}

void NodeListModule::setView(NodeListView view) {
    if (currentView != view) {
        currentView = view;
        scrollOffset = 0;
        requestUpdate();
    }
}

void NodeListModule::cycleView() {
    switch (currentView) {
        case NodeListView::ALL:
            setView(NodeListView::RECENT);
            break;
        case NodeListView::RECENT:
            setView(NodeListView::FAVORITES);
            break;
        case NodeListView::FAVORITES:
            setView(NodeListView::ALL);
            break;
    }
}

void NodeListModule::updateNode(const NodeEntry& node) {
    // Update existing or add new
    for (auto& n : nodes) {
        if (n.nodeNum == node.nodeNum) {
            n = node;
            requestUpdate();
            return;
        }
    }
    nodes.push_back(node);
    requestUpdate();
}

void NodeListModule::removeNode(uint32_t nodeNum) {
    nodes.erase(
        std::remove_if(nodes.begin(), nodes.end(),
            [nodeNum](const NodeEntry& n) { return n.nodeNum == nodeNum; }),
        nodes.end()
    );
    requestUpdate();
}

void NodeListModule::clearNodes() {
    nodes.clear();
    scrollOffset = 0;
    requestUpdate();
}

void NodeListModule::toggleFavorite(uint32_t nodeNum) {
    for (auto& n : nodes) {
        if (n.nodeNum == nodeNum) {
            n.isFavorite = !n.isFavorite;
            requestUpdate();
            return;
        }
    }
}

std::vector<NodeEntry> NodeListModule::sortedNodes() const {
    std::vector<NodeEntry> result;

    // Filter based on view
    for (const auto& n : nodes) {
        switch (currentView) {
            case NodeListView::ALL:
            case NodeListView::RECENT:
                result.push_back(n);
                break;
            case NodeListView::FAVORITES:
                if (n.isFavorite) {
                    result.push_back(n);
                }
                break;
        }
    }

    // Sort
    switch (currentView) {
        case NodeListView::ALL:
            // Sort alphabetically by long name
            std::sort(result.begin(), result.end(),
                [](const NodeEntry& a, const NodeEntry& b) {
                    return strcmp(a.longName, b.longName) < 0;
                });
            break;

        case NodeListView::RECENT:
        case NodeListView::FAVORITES:
            // Sort by last heard (newest first)
            std::sort(result.begin(), result.end(),
                [](const NodeEntry& a, const NodeEntry& b) {
                    return a.lastHeard > b.lastHeard;
                });
            break;
    }

    return result;
}

void NodeListModule::renderNodeList(RenderContext& ctx, const ContentArea& content) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    // Card = two body-tier lines. Both lines share the same scale so the
    // pair reads as a single block. Section-spacing gap between cards keeps
    // adjacent nodes from visually merging.
    uint16_t rowLineH = layout->bodyLineHeight();
    uint16_t cardH = rowLineH * 2;
    uint16_t cardMargin = layout->sectionSpacing();

    int16_t y = content.top();
    auto sorted = sortedNodes();

    if (sorted.empty()) {
        visibleNodeCount = 0;
        ctx.text(ctx.width() / 2, content.top() + content.h / 2, "No nodes", Align::CENTER, Color::BLACK);
        return;
    }

    // Calculate how many nodes fit on screen
    visibleNodeCount = (content.h + cardMargin) / (cardH + cardMargin);

    // Render visible nodes (two lines per node)
    for (size_t i = scrollOffset; i < sorted.size() && y < content.bottom() - cardH; ++i) {
        renderNodeRow(ctx, sorted[i], y);
        y += cardH + cardMargin;
    }
}

void NodeListModule::renderNodeRow(RenderContext& ctx, const NodeEntry& node, int16_t y) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    // Two-tier system: rows use body. Both lines share the scale.
    float rowScale = Layout::bodyScale;
    float shortNameScale = rowScale;
    float longNameScale  = rowScale;
    uint16_t shortLineH = layout->bodyLineHeight();
    uint16_t longLineH  = shortLineH;

    Color textColor = Color::BLACK;

    // Two-line layout:
    // Line A (top): shortName left, [hops] + signal bars right
    // Line B (bottom): longName left, distance right

    int16_t lineAY = y;
    int16_t lineBY = y + shortLineH;

    // Get layout values
    uint16_t textInset = layout->textInset();
    uint16_t nodeTextInset = layout->nodeTextInset();
    uint16_t elemSpacing = layout->elementSpacing();

    // === Line A: Short name + [hops] + signal bars ===
    const char* shortName = (node.shortName[0] != '\0') ? node.shortName : "?";
    // Always draw scaled (no isElongated branch) so line A matches line B's
    // tier exactly. ctx.text() defaults to body scale and would break parity.
    ctx.textScaled(nodeTextInset, lineAY, shortName, shortNameScale, Align::LEFT, textColor);

    // Right side of Line A: [time] [signal bars OR "MQ"]
    // Hops moved out — they were cluttering the row and overlapping nearby
    // columns; users care most about "when last seen" + "how strong".
    bool hasTime = (node.lastHeard != 0);

    // Line A's right slot draws bars / Wi-Fi icon as raw pixels (not font
    // glyphs), so the small nodeTextInset is enough — no extra margin.
    uint16_t rightMargin = nodeTextInset;

    uint16_t barW = layout->signalBarWidth();
    uint16_t barSpacing = layout->signalBarSpacing();
    uint16_t barsWidth = 3 * barW + 2 * barSpacing;

    int16_t rightSlotX = ctx.width() - rightMargin;     // right edge of rightmost element
    int16_t rightSlotLeftX = rightSlotX - barsWidth;    // left edge of rightmost element

    if (node.viaMqtt) {
        // Wi-Fi-style icon, vertically centered against the signal-bar slot.
        int16_t iconLeft = rightSlotX - WIFI_W;
        int16_t iconTop = lineAY + (int16_t)((shortLineH > WIFI_H) ? (shortLineH - WIFI_H) / 2 : 0);
        drawWifiIcon(ctx, iconLeft, iconTop, textColor);
        rightSlotLeftX = iconLeft;
    } else if (node.snr != 0) {
        renderSignalBars(ctx, layout, rightSlotLeftX, lineAY, node.snr, shortLineH);
    }

    // Last-heard time, sitting to the left of the rightmost slot with a
    // generous gap so it doesn't crowd into the bars/MQ tag.
    if (hasTime) {
        std::string ts = formatLastHeard(node.lastHeard);
        if (!ts.empty()) {
            int16_t timeRightX = rightSlotLeftX - elemSpacing * 2;
            ctx.textScaled(timeRightX, lineAY, ts.c_str(), shortNameScale, Align::RIGHT, textColor);
        }
    }

    // === Line B: Long name + distance (smaller font) ===
    bool hasDistance = (node.distanceMeters != NodeEntry::DISTANCE_UNKNOWN);

    // Same right-edge margin as Line A so the trailing 'm'/'km' glyph isn't
    // clipped by the display border.
    uint16_t rightEdgeMargin = nodeTextInset + elemSpacing;

    // Divider X - where right-side info starts (only distance now)
    uint16_t dividerX;
    if (hasDistance) {
        dividerX = ctx.width() - ctx.textWidthScaled("999km", longNameScale) - rightEdgeMargin;
        ctx.textScaled(ctx.width() - rightEdgeMargin, lineBY, formatDistance(node.distanceMeters).c_str(),
                       longNameScale, Align::RIGHT, textColor);
    } else {
        dividerX = ctx.width() - rightEdgeMargin;
    }

    // Long name on left (truncated with ellipsis if needed)
    std::string longName = (node.longName[0] != '\0') ? node.longName : shortName;
    std::string truncatedName = truncateWithEllipsis(ctx, longName, dividerX - nodeTextInset - elemSpacing, longNameScale);

    ctx.textScaled(nodeTextInset, lineBY, truncatedName.c_str(), longNameScale, Align::LEFT, textColor);

    // Hatch fade effect for long names (only if truncated)
    if (longName.length() > truncatedName.length() - 3) {  // -3 for "..."
        int16_t hatchLeft = dividerX - longLineH - elemSpacing;
        int16_t hatchWidth = longLineH;
        hatchRegion(ctx, hatchLeft, lineBY, hatchWidth, longLineH, elemSpacing, Color::WHITE);
    }
}

void NodeListModule::renderSignalBars(RenderContext& ctx, const Layout* layout, int16_t x, int16_t y, int16_t snr, uint16_t height) {
    if (!layout) return;

    // SNR comes as raw dB value (not multiplied)
    // Typical LoRa range: -20dB (barely works) to +10dB (excellent)
    // Map to 0-3 bars: 0=none, 1=weak, 2=good, 3=excellent
    uint8_t bars;

    if (snr < -12) bars = 0;       // Very weak (< -12 dB)
    else if (snr < -5) bars = 1;   // Weak (-12 to -5 dB)
    else if (snr < 3) bars = 2;    // Good (-5 to +3 dB)
    else bars = 3;                  // Excellent (> +3 dB)

    uint16_t barW = layout->signalBarWidth();
    uint16_t barSpacing = layout->signalBarSpacing();
    uint16_t maxBarH = height - layout->signalBarPadding();
    uint16_t elemSpacing = layout->elementSpacing();

    // 3 bars aligned to bottom of the line (y + height)
    for (uint8_t i = 0; i < 3; ++i) {
        uint16_t barH = (maxBarH * (i + 1)) / 3;
        int16_t barX = x + i * (barW + barSpacing);
        int16_t barY = y + height - elemSpacing - barH;

        if (i < bars) {
            ctx.fillRect(barX, barY, barW, barH, Color::BLACK);
        } else {
            ctx.rect(barX, barY, barW, barH, Color::BLACK);
        }
    }
}

std::string NodeListModule::formatDistance(int32_t meters) const {
    if (meters < 1000) {
        return std::to_string(meters) + "m";
    } else {
        int km = meters / 1000;
        return std::to_string(km) + "km";
    }
}

// Hybrid absolute-time format. Within the last 24h we render HH:MM in the
// device tz; older than that we drop to MM/DD so the user can still tell
// "today vs yesterday vs last week" from a 5-character slot. main.cpp
// installs the device's tzdef via setenv("TZ", ...) + tzset() at boot, so
// localtime() honours it. Returns empty string if RTC is unsynced.
std::string NodeListModule::formatLastHeard(uint32_t epoch) const {
    if (epoch == 0) return std::string();
    time_t t = (time_t)epoch;
    struct tm* lt = localtime(&t);
    if (!lt) return std::string();

    // Compare against current wall clock from RTC (same epoch source as
    // node->last_heard, which Events.cpp's syncNodes copies from NodeDB).
    uint32_t nowSec = getTime(false);
    long ageSec = (nowSec > epoch) ? (long)(nowSec - epoch) : 0;

    char buf[8];
    if (ageSec < 24 * 3600) {
        snprintf(buf, sizeof(buf), "%02d:%02d", lt->tm_hour, lt->tm_min);
    } else {
        snprintf(buf, sizeof(buf), "%02d/%02d", lt->tm_mon + 1, lt->tm_mday);
    }
    return std::string(buf);
}

std::string NodeListModule::truncateWithEllipsis(const RenderContext& ctx, const std::string& text, uint16_t maxWidth, float scale) const {
    if (ctx.textWidthScaled(text.c_str(), scale) <= maxWidth) {
        return text;
    }

    std::string result = text;
    const char* ellipsis = "...";
    uint16_t ellipsisWidth = ctx.textWidthScaled(ellipsis, scale);

    while (!result.empty() && ctx.textWidthScaled(result.c_str(), scale) + ellipsisWidth > maxWidth) {
        // Handle UTF-8: check if last byte is continuation byte
        size_t len = result.length();
        while (len > 0 && (result[len - 1] & 0xC0) == 0x80) {
            len--;
        }
        if (len > 0) len--;  // Remove the start byte
        result = result.substr(0, len);
    }

    return result + ellipsis;
}

void NodeListModule::hatchRegion(RenderContext& ctx, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t spacing, Color color) {
    // Draw diagonal hatch lines for fade effect
    for (int16_t px = x; px < x + (int16_t)w; px++) {
        for (int16_t py = y; py < y + (int16_t)h; py++) {
            // Checkerboard pattern based on position
            if ((px + py) % (spacing * 2) < spacing) {
                ctx.pixel(px, py, color);
            }
        }
    }
}

} // namespace InkHUD2
