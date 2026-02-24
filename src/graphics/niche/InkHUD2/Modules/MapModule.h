#pragma once

#include "Module.h"
#include "../UI/ContentArea.h"
#include "../UI/MenuList.h"
#include "../UI/MenuItem.h"
#include <climits>
#include <cstdint>
#include <string>
#include <vector>

namespace InkHUD2 {

// Forward declarations
class Buffer;
class MenuModule;

// Map module states
enum class MapState : uint8_t {
    MAP,        // Normal map view
    SETTINGS,   // Settings submenu
    POSITION    // My position info with compass
};

// Node entry for map display
struct MapNode {
    static constexpr uint8_t HOPS_UNKNOWN = 255;
    static constexpr int32_t DISTANCE_UNKNOWN = INT32_MIN;

    uint32_t nodeNum = 0;
    char shortName[5] = {0};    // 4 chars + null
    char longName[40] = {0};    // Display name
    int32_t latitude = 0;       // Scaled 1e-7
    int32_t longitude = 0;      // Scaled 1e-7
    int32_t distanceMeters = DISTANCE_UNKNOWN;
    uint8_t hopsAway = HOPS_UNKNOWN;
    bool isFavorite = false;    // From Meshtastic favorites
    bool isOwnNode = false;     // True for our own position

    // Calculated for rendering (relative to map center)
    float northMeters = 0.0f;   // Positive = north
    float eastMeters = 0.0f;    // Positive = east
};

// Cluster of nearby nodes (for dense areas)
struct MapCluster {
    int16_t screenX;
    int16_t screenY;
    std::vector<MapNode*> nodes;  // Nodes in this cluster
};

// Map module - displays mesh nodes on a 2D map
// Inherits from SystemModule to use handlesInput for settings submenu
class MapModule : public SystemModule {
public:
    MapModule();

    void onRender(RenderContext& ctx) override;
    void onEvent(const Event& e) override;
    void onInput(Input input) override;

    // Menu integration for alerts
    void setMenuModule(MenuModule* menu) { menuModule = menu; }

    // Node management (called from Events)
    void updateNode(const MapNode& node);
    void removeNode(uint32_t nodeNum);
    void clearNodes();

    // Own position
    void setOwnPosition(int32_t lat, int32_t lng);
    bool hasOwnPosition() const { return ownLat != 0 || ownLng != 0; }

private:
    // Rendering methods
    void renderMap(RenderContext& ctx);
    void renderSettings(RenderContext& ctx);
    void renderPosition(RenderContext& ctx);

    // Map rendering helpers
    void drawCluster(RenderContext& ctx, const MapCluster& cluster,
                     int16_t mapLeft, int16_t mapTop, uint16_t mapW, uint16_t mapH);
    void drawSingleNode(RenderContext& ctx, const MapNode& node, int16_t x, int16_t y,
                        int16_t mapLeft, int16_t mapTop, uint16_t mapW, uint16_t mapH);
    void drawOwnNode(Buffer* buffer, const Layout* layout, int16_t x, int16_t y);
    void drawScaleBar(RenderContext& ctx, int16_t footerY);

    // Coordinate transformation
    void recalculateMap();
    std::vector<MapNode*> getVisibleNodes();

    // Clustering
    std::vector<MapCluster> clusterNodes(const std::vector<MapNode*>& nodes, int16_t mapCenterX, int16_t mapCenterY, const Layout* layout);

    // Format distance for display
    std::string formatDistance(int32_t meters) const;

    // State
    MapState state = MapState::MAP;
    std::vector<MapNode> allNodes;

    // Settings
    bool showAllNodes = false;  // false = favorites only, true = all nodes with position

    // Settings menu
    MenuList settingsMenu;
    MenuItem settingsItems[3];  // Back, My Position, Show all nodes
    void initSettingsMenu();

    // Format position for alert
    void showPositionAlert();

    // Own position
    int32_t ownLat = 0;
    int32_t ownLng = 0;

    // Map state (recalculated when visible nodes change)
    int32_t centerLat = 0;
    int32_t centerLng = 0;
    float metersToPx = 0.0f;
    float mapRadiusMeters = 1000.0f;

    // Menu module for showing alerts
    MenuModule* menuModule = nullptr;

    // All visual constants are now in Layout.h
};

} // namespace InkHUD2
