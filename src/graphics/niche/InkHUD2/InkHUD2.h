#pragma once

#include "Core/Buffer.h"
#include "Core/Font.h"
#include "Core/Layout.h"
#include "Core/RenderContext.h"
#include "Events.h"
#include "Pipe/Pipe.h"
#include "concurrency/OSThread.h"
#include "graphics/niche/Fonts/CJK/CJKFont.h"
#include <cstdint>

namespace InkHUD2 {

// Forward declarations
class DisplayDriver;
class MessageModule;
class BatteryModule;

// Display driver interface - implemented per-platform
class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;

    // Initialize display hardware
    virtual bool init() = 0;

    // Get display dimensions
    virtual uint16_t width() const = 0;
    virtual uint16_t height() const = 0;

    // Check if display is busy (update in progress)
    virtual bool busy() const { return false; }

    // Transfer buffer to display
    virtual void update(const uint8_t* data, bool fullRefresh) = 0;

    // Put display in low-power mode
    virtual void sleep() = 0;

    // Wake display from sleep
    virtual void wake() = 0;
};

// InkHUD2 - main orchestration singleton with OSThread for periodic updates
class InkHUD2 : protected concurrency::OSThread {
public:
    // Singleton access
    static InkHUD2& instance();

    // Lifecycle
    bool init(DisplayDriver* driver, const NicheGraphics::CJKFont* cjkFont, uint8_t rotation = 3, float fontScale = 1.0f);
    void shutdown();

    // Main loop integration
    void update();  // Call from main loop

    // Event injection from firmware
    void onEvent(const Event& e);
    void onInput(Input input);

    // Module management
    void addModule(Module* module);
    void addSystemModule(SystemModule* module);
    void removeModule(Module* module);

    // Display control
    void requestFullRefresh();
    void setSlotCount(uint8_t count);
    void cycleSlot(uint8_t slot);
    void setRotation(uint8_t r);  // 0=0°, 1=90°, 2=180°, 3=270°

    // Access (for modules that need it)
    const Layout& layout() const { return *layoutPtr; }
    const Font& font() const { return *fontPtr; }
    Pipe& pipe() { return pipeInstance; }

    // Connect to Meshtastic event system
    void connectEvents(MessageModule* msgModule, BatteryModule* batModule, NodeListModule* nodeModule);

protected:
    // OSThread callback - called periodically by scheduler
    int32_t runOnce() override;

private:
    InkHUD2();
    ~InkHUD2() = default;

    // Non-copyable
    InkHUD2(const InkHUD2&) = delete;
    InkHUD2& operator=(const InkHUD2&) = delete;

    void render();
    void cleanupInit();  // Helper for cleanup on init failure

    // Core components (owned)
    Buffer* buffer = nullptr;
    Font* fontPtr = nullptr;
    Layout* layoutPtr = nullptr;

    // Driver (not owned)
    DisplayDriver* driver = nullptr;

    // Event/module management
    Pipe pipeInstance;
    Events eventsInstance;

    // State
    bool initialized = false;
    bool fullRefreshRequested = true;  // First render is always full
    uint32_t lastUpdateTime = 0;
    uint32_t minUpdateInterval = 100;  // ms between updates (e-ink friendly)

    // Note: No automatic FULL refresh counter. FULL only when explicitly requested.
    // Old InkHUD does FULL only during "maintenance" when user is idle (1+ min).
    // During active use, always FAST refresh.
};

} // namespace InkHUD2
