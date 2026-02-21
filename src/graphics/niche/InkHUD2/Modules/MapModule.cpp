/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "MapModule.h"
#include "MenuModule.h"
#include "../Text/TextRenderer.h"
#include "../UI/StatusBar.h"
#include "../UI/ContentArea.h"
#include "gps/GeoCoord.h"
#include "NodeDB.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace InkHUD2 {

MapModule::MapModule() {
    allNodes.reserve(64);
    initSettingsMenu();
}

void MapModule::initSettingsMenu() {
    // Back item
    settingsItems[0].label = "< Back";
    settingsItems[0].type = MenuItemType::BACK;

    // Show all nodes toggle
    settingsItems[1].label = "Show all nodes";
    settingsItems[1].type = MenuItemType::TOGGLE;
    settingsItems[1].toggleValue = &showAllNodes;

    settingsMenu.setItems(settingsItems, 2);
}

void MapModule::onRender(RenderContext& ctx) {
    switch (state) {
        case MapState::MAP:
            renderMap(ctx);
            break;
        case MapState::SETTINGS:
            renderSettings(ctx);
            break;
    }
}

void MapModule::onEvent(const Event& e) {
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

void MapModule::onInput(Input input) {
    switch (state) {
        case MapState::MAP:
            if (input == Input::BACK) {
                // Long press opens settings
                state = MapState::SETTINGS;
                handlesInput = true;  // Capture input while in settings
                requestUpdate();
            }
            break;

        case MapState::SETTINGS:
            if (input == Input::SELECT) {
                // Short press = move to next menu item
                settingsMenu.selectNext();
                requestUpdate();
            } else if (input == Input::BACK) {
                // Long press = activate selected item
                MenuItem* item = settingsMenu.getSelectedItem();
                if (!item) break;

                if (item->type == MenuItemType::BACK) {
                    // "Back" - return to map
                    state = MapState::MAP;
                    handlesInput = false;
                    settingsMenu.setSelectedIndex(0);  // Reset for next time
                    requestUpdate();
                } else if (item->type == MenuItemType::TOGGLE) {
                    // Toggle handled by activateSelected
                    if (settingsMenu.activateSelected()) {
                        recalculateMap();
                        // Show alert via MenuModule
                        if (menuModule) {
                            if (showAllNodes) {
                                menuModule->showAlert("Showing all nodes with GPS position on map.");
                            } else {
                                menuModule->showAlert("Showing only favorite nodes on map.");
                            }
                        }
                        requestUpdate();
                    }
                }
            }
            break;
    }
}

void MapModule::updateNode(const MapNode& node) {
    for (auto& n : allNodes) {
        if (n.nodeNum == node.nodeNum) {
            n = node;  // Full update including isFavorite
            requestUpdate();
            return;
        }
    }
    allNodes.push_back(node);
    requestUpdate();
}

void MapModule::removeNode(uint32_t nodeNum) {
    allNodes.erase(
        std::remove_if(allNodes.begin(), allNodes.end(),
            [nodeNum](const MapNode& n) { return n.nodeNum == nodeNum; }),
        allNodes.end()
    );
    requestUpdate();
}

void MapModule::clearNodes() {
    allNodes.clear();
    requestUpdate();
}

void MapModule::setOwnPosition(int32_t lat, int32_t lng) {
    if (ownLat != lat || ownLng != lng) {
        ownLat = lat;
        ownLng = lng;
        recalculateMap();
        requestUpdate();
    }
}

std::vector<MapNode*> MapModule::getVisibleNodes() {
    std::vector<MapNode*> result;
    for (auto& n : allNodes) {
        if (n.latitude == 0 && n.longitude == 0) continue;  // No position

        if (showAllNodes || n.isFavorite) {
            result.push_back(&n);
        }
    }
    return result;
}

void MapModule::recalculateMap() {
    auto visible = getVisibleNodes();

    centerLat = ownLat;
    centerLng = ownLng;

    if (!hasOwnPosition()) {
        return;
    }

    float maxDistance = 100.0f;
    for (auto* node : visible) {
        if (node->distanceMeters > 0 && node->distanceMeters != MapNode::DISTANCE_UNKNOWN) {
            maxDistance = std::max(maxDistance, static_cast<float>(node->distanceMeters));
        }
    }

    mapRadiusMeters = maxDistance * 1.2f;

    float centerLatDeg = centerLat * 1e-7f;
    float centerLngDeg = centerLng * 1e-7f;

    for (auto* node : visible) {
        float nodeLat = node->latitude * 1e-7f;
        float nodeLng = node->longitude * 1e-7f;

        float bearing = GeoCoord::bearing(centerLatDeg, centerLngDeg, nodeLat, nodeLng);
        float distance = GeoCoord::latLongToMeter(centerLatDeg, centerLngDeg, nodeLat, nodeLng);

        node->northMeters = distance * cosf(bearing);
        node->eastMeters = distance * sinf(bearing);
    }
}

std::vector<MapCluster> MapModule::clusterNodes(const std::vector<MapNode*>& nodes,
                                                  int16_t mapCenterX, int16_t mapCenterY,
                                                  const Layout* layout) {
    std::vector<MapCluster> clusters;
    std::vector<bool> assigned(nodes.size(), false);

    for (size_t i = 0; i < nodes.size(); ++i) {
        if (assigned[i]) continue;

        // Calculate screen position for this node
        int16_t x = mapCenterX + static_cast<int16_t>(nodes[i]->eastMeters * metersToPx);
        int16_t y = mapCenterY - static_cast<int16_t>(nodes[i]->northMeters * metersToPx);

        MapCluster cluster;
        cluster.screenX = x;
        cluster.screenY = y;
        cluster.nodes.push_back(nodes[i]);
        assigned[i] = true;

        // Find all nearby unassigned nodes
        for (size_t j = i + 1; j < nodes.size(); ++j) {
            if (assigned[j]) continue;

            int16_t nx = mapCenterX + static_cast<int16_t>(nodes[j]->eastMeters * metersToPx);
            int16_t ny = mapCenterY - static_cast<int16_t>(nodes[j]->northMeters * metersToPx);

            // Check if within cluster radius
            int16_t dx = nx - cluster.screenX;
            int16_t dy = ny - cluster.screenY;
            if (dx * dx + dy * dy <= layout->clusterRadius() * layout->clusterRadius()) {
                cluster.nodes.push_back(nodes[j]);
                assigned[j] = true;

                // Update cluster center to average
                cluster.screenX = (cluster.screenX + nx) / 2;
                cluster.screenY = (cluster.screenY + ny) / 2;
            }
        }

        clusters.push_back(cluster);
    }

    return clusters;
}

void MapModule::renderMap(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    Buffer* buffer = ctx.getBuffer();
    const Font* font = ctx.getFont();
    if (!buffer || !font) return;

    uint16_t padding = layout->padding();
    uint16_t lineH = layout->lineHeight();

    // Create TextRenderer
    TextRenderer textRenderer(buffer, font);
    textRenderer.setClip(ctx.clip().x, ctx.clip().y, ctx.clip().w, ctx.clip().h);

    // === Header ===
    auto visible = getVisibleNodes();
    char title[32];
    size_t count = visible.size();
    if (showAllNodes) {
        snprintf(title, sizeof(title), "Map: %d node%s", (int)count, count == 1 ? "" : "s");
    } else {
        snprintf(title, sizeof(title), "Map: %d fav%s", (int)count, count == 1 ? "" : "s");
    }

    // === Render StatusBar ===
    StatusBar statusBar(buffer, layout, &textRenderer);
    int16_t contentTop = statusBar.render(padding, title, StatusBar::Icon::MAP);

    // === Footer area for scale bar ===
    uint16_t footerH = lineH;
    int16_t footerTop = ctx.height() - footerH - layout->slotSpacing();

    // === Calculate content area ===
    ContentArea content = calculateContentArea(layout, contentTop, footerTop);

    // === Map content area ===
    int16_t mapLeft = content.left() + layout->mapEdgePadding();
    uint16_t mapW = content.innerWidth(layout->mapEdgePadding());
    uint16_t mapH = content.h - layout->mapContentGap();

    int16_t mapCenterX = content.left() + content.w / 2;
    int16_t mapCenterY = content.top() + content.h / 2;

    // Check for valid own position
    if (!hasOwnPosition()) {
        const char* statusLine1;
        const char* statusLine2;

        if (config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT) {
            statusLine1 = "No GPS";
            statusLine2 = "Module not present";
        } else if (config.position.gps_mode != meshtastic_Config_PositionConfig_GpsMode_ENABLED) {
            statusLine1 = "GPS disabled";
            statusLine2 = "Enable in settings";
        } else {
            statusLine1 = "No position";
            statusLine2 = "Initializing...";
        }

        ctx.text(mapCenterX, mapCenterY - layout->verticalTextGap(), statusLine1, Align::CENTER, Color::BLACK);
        ctx.textScaled(mapCenterX, mapCenterY + layout->verticalTextGap(), statusLine2, Layout::smallScale, Align::CENTER, Color::BLACK);
        drawScaleBar(ctx, footerTop);
        return;
    }

    // Check for visible nodes
    if (visible.empty()) {
        if (showAllNodes) {
            ctx.text(mapCenterX, mapCenterY - layout->verticalTextGap(), "No nodes", Align::CENTER, Color::BLACK);
            ctx.textScaled(mapCenterX, mapCenterY + layout->verticalTextGap(), "with GPS position", Layout::smallScale, Align::CENTER, Color::BLACK);
        } else {
            ctx.text(mapCenterX, mapCenterY - layout->verticalTextGap(), "No favorites", Align::CENTER, Color::BLACK);
            ctx.textScaled(mapCenterX, mapCenterY + layout->verticalTextGap(), "Add in Meshtastic app", Layout::smallScale, Align::CENTER, Color::BLACK);
        }
        drawScaleBar(ctx, footerTop);
        return;
    }

    // Recalculate map state
    recalculateMap();

    // Calculate scale
    float mapRadius = std::min(mapW, mapH) / 2.0f - layout->mapScalePadding();
    metersToPx = mapRadius / mapRadiusMeters;

    // Cluster nearby nodes
    auto clusters = clusterNodes(visible, mapCenterX, mapCenterY, layout);

    // Draw clusters
    for (const auto& cluster : clusters) {
        drawCluster(ctx, cluster, content.left(), content.top(), content.w, content.h);
    }

    // Draw own node last (on top)
    drawOwnNode(buffer, layout, mapCenterX, mapCenterY);

    // Draw scale bar in footer
    drawScaleBar(ctx, footerTop);
}

void MapModule::renderSettings(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    Buffer* buffer = ctx.getBuffer();
    if (!buffer) return;

    uint16_t lineH = layout->lineHeight();
    uint16_t margin = layout->menuMargin();

    // Clear screen
    ctx.clear();

    // === Title at top ===
    ctx.text(ctx.width() / 2, margin, "Map Settings", Align::CENTER, Color::BLACK);

    // Dotted separator
    int16_t sepY = margin + lineH + layout->elementSpacing();
    for (int16_t x = margin; x < ctx.width() - margin; x += layout->dotSpacing()) {
        buffer->setPixel(x, sepY, Color::BLACK);
    }

    // === Hint at bottom (reserve space) ===
    uint16_t hintLineH = layout->hintLineHeight();
    int16_t hintY = ctx.height() - hintLineH - layout->elementSpacing();

    // === Menu items using MenuList ===
    int16_t menuStartY = sepY + layout->sectionSpacing() * 2;
    settingsMenu.render(ctx, menuStartY, hintY, margin);

    // === Hint at bottom ===
    ctx.textScaled(ctx.width() / 2, hintY, "Click: next  Hold: select", Layout::hintScale, Align::CENTER, Color::BLACK);
}

void MapModule::drawCluster(RenderContext& ctx, const MapCluster& cluster,
                            int16_t mapLeft, int16_t mapTop, uint16_t mapW, uint16_t mapH) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;
    if (cluster.nodes.empty()) return;

    if (cluster.nodes.size() == 1) {
        // Single node - draw normally
        drawSingleNode(ctx, *cluster.nodes[0], cluster.screenX, cluster.screenY,
                       mapLeft, mapTop, mapW, mapH);
        return;
    }

    // Multiple nodes - draw cluster indicator
    Buffer* buffer = ctx.getBuffer();
    if (!buffer) return;

    int16_t x = cluster.screenX;
    int16_t y = cluster.screenY;

    // Clamp within content area
    uint16_t radius = layout->clusterNodeRadius();
    int16_t xMin = mapLeft + radius + layout->elementSpacing();
    int16_t xMax = mapLeft + mapW - radius - layout->elementSpacing();
    int16_t yMin = mapTop + radius + layout->elementSpacing();
    int16_t yMax = mapTop + mapH - radius - layout->clusterLabelGap();

    if (x < xMin) x = xMin;
    if (x > xMax) x = xMax;
    if (y < yMin) y = yMin;
    if (y > yMax) y = yMax;

    // Draw filled circle
    ctx.fillCircle(x, y, radius, Color::BLACK);

    // Draw count number in white (inverted)
    char countStr[8];
    snprintf(countStr, sizeof(countStr), "%d", (int)cluster.nodes.size());
    ctx.textScaled(x, y - 5, countStr, Layout::smallScale, Align::CENTER, Color::WHITE);

    // Draw label below with first node's short name + count
    char label[16];
    const char* firstName = cluster.nodes[0]->shortName[0] ? cluster.nodes[0]->shortName : "?";
    snprintf(label, sizeof(label), "%s+%d", firstName, (int)(cluster.nodes.size() - 1));
    ctx.textScaled(x, y + radius + layout->elementSpacing(), label, Layout::smallScale, Align::CENTER, Color::BLACK);
}

void MapModule::drawSingleNode(RenderContext& ctx, const MapNode& node, int16_t x, int16_t y,
                               int16_t mapLeft, int16_t mapTop, uint16_t mapW, uint16_t mapH) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    // Node radius based on hops
    uint16_t radius;
    if (node.hopsAway == 0) {
        radius = layout->nodeRadiusDirect();
    } else if (node.hopsAway <= 2) {
        radius = layout->nodeRadiusNear();
    } else {
        radius = layout->nodeRadiusFar();
    }

    // Get label dimensions
    const char* name = (node.shortName[0] != '\0') ? node.shortName : "?";
    uint16_t labelW = ctx.textWidthScaled(name, Layout::smallScale);
    uint16_t labelH = layout->verticalTextGap();  // Approximate scaled text height

    // Clamp node position within content area, accounting for label
    int16_t xMin = mapLeft + labelW / 2 + layout->elementSpacing();
    int16_t xMax = mapLeft + mapW - labelW / 2 - layout->elementSpacing();
    int16_t yMin = mapTop + radius + layout->elementSpacing();
    int16_t yMax = mapTop + mapH - radius - labelH - layout->contentPadding();

    if (x < xMin) x = xMin;
    if (x > xMax) x = xMax;
    if (y < yMin) y = yMin;
    if (y > yMax) y = yMax;

    // Draw node dot
    ctx.fillCircle(x, y, radius, Color::BLACK);

    // Draw label centered below node
    ctx.textScaled(x, y + radius + layout->elementSpacing(), name, Layout::smallScale, Align::CENTER, Color::BLACK);
}

void MapModule::drawOwnNode(Buffer* buffer, const Layout* layout, int16_t x, int16_t y) {
    if (!buffer || !layout) return;

    // Bullseye crosshair - center dot
    int16_t dotR = layout->ownNodeDotRadius();
    for (int16_t dy = -dotR; dy <= dotR; dy++) {
        for (int16_t dx = -dotR; dx <= dotR; dx++) {
            if (dx*dx + dy*dy <= dotR*dotR) {
                buffer->setPixel(x + dx, y + dy, Color::BLACK);
            }
        }
    }

    // Crosshair arms
    int16_t armLen = layout->ownNodeArmLength();
    for (int16_t i = dotR + 1; i <= armLen; i++) {
        buffer->setPixel(x - i, y, Color::BLACK);
        buffer->setPixel(x + i, y, Color::BLACK);
        buffer->setPixel(x, y - i, Color::BLACK);
        buffer->setPixel(x, y + i, Color::BLACK);
    }
}

void MapModule::drawScaleBar(RenderContext& ctx, int16_t footerY) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    Buffer* buffer = ctx.getBuffer();
    if (!buffer) return;

    uint16_t padding = layout->padding();
    uint16_t footerH = layout->lineHeight();

    // Scale bar in footer area (left side)
    int16_t barX = padding + layout->elementSpacing();
    int16_t barY = footerY + layout->slotSpacing() + footerH / 2;

    // If no scale yet, show placeholder
    if (metersToPx <= 0) {
        ctx.textScaled(barX, footerY + layout->slotSpacing() + layout->elementSpacing(), "---", Layout::smallScale, Align::LEFT, Color::BLACK);
        return;
    }

    // Calculate bar dimensions
    uint16_t targetBarW = layout->scaleBarTargetWidth();
    float barMeters = targetBarW / metersToPx;

    // Round to nice number
    float nice = 10.0f;
    if (barMeters >= 10000) nice = 10000.0f;
    else if (barMeters >= 5000) nice = 5000.0f;
    else if (barMeters >= 1000) nice = 1000.0f;
    else if (barMeters >= 500) nice = 500.0f;
    else if (barMeters >= 100) nice = 100.0f;
    else if (barMeters >= 50) nice = 50.0f;

    uint16_t barW = static_cast<uint16_t>(nice * metersToPx);
    if (barW < layout->scaleBarMinWidth()) barW = layout->scaleBarMinWidth();
    if (barW > layout->scaleBarMaxWidth()) barW = layout->scaleBarMaxWidth();

    // Draw bar
    ctx.hLine(barX, barY, barW, Color::BLACK);
    // End caps
    ctx.vLine(barX, barY - layout->elementSpacing(), layout->scaleBarCapHeight(), Color::BLACK);
    ctx.vLine(barX + barW - 1, barY - layout->elementSpacing(), layout->scaleBarCapHeight(), Color::BLACK);

    // Label
    std::string label = formatDistance(static_cast<int32_t>(nice));
    ctx.textScaled(barX + barW + layout->textInset(), footerY + layout->slotSpacing() + layout->elementSpacing(), label.c_str(), Layout::smallScale, Align::LEFT, Color::BLACK);
}

std::string MapModule::formatDistance(int32_t meters) const {
    if (meters < 0) return "?";

    char buf[16];
    if (meters < 1000) {
        snprintf(buf, sizeof(buf), "%ldm", (long)meters);
    } else if (meters < 10000) {
        snprintf(buf, sizeof(buf), "%.1fkm", meters / 1000.0f);
    } else {
        snprintf(buf, sizeof(buf), "%ldkm", (long)(meters / 1000));
    }
    return std::string(buf);
}

} // namespace InkHUD2
