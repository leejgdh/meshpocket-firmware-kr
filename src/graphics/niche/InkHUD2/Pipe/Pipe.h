#pragma once

#include "../Core/RenderContext.h"
#include "../Modules/Module.h"
#include "Events.h"
#include <vector>

namespace InkHUD2 {

// Maximum number of slots for regular modules
constexpr uint8_t MAX_SLOTS = 4;

// Pipe - manages module lifecycle, events, and rendering order
class Pipe {
public:
    Pipe() = default;
    ~Pipe() = default;

    // === Module Registration ===

    // Add a regular module (renders in slots)
    void addModule(Module* module);

    // Add a system module (overlays, notifications, etc.)
    void addSystemModule(SystemModule* module);

    // Remove a module
    void removeModule(Module* module);

    // === Slot Management ===

    // Set number of visible slots (1, 2, or 4)
    void setSlotCount(uint8_t count);
    uint8_t getSlotCount() const { return slotCount; }

    // Assign module to slot
    void setSlotModule(uint8_t slot, Module* module);
    Module* getSlotModule(uint8_t slot) const;

    // Cycle to next module in slot
    void cycleSlot(uint8_t slot);

    // === Event Distribution ===

    // Dispatch event to all modules
    void dispatchEvent(const Event& e);

    // Send input to focused module (or system module if it handles input)
    void dispatchInput(Input input);

    // Same as dispatchInput but returns true if handled by a system module
    bool dispatchInputAndCheck(Input input);

    // === Rendering ===

    // Render all visible modules to context
    void render(RenderContext& ctx, const Layout& layout);

    // Check if any module needs update
    bool needsUpdate() const;

    // Check if full refresh is needed
    bool needsFullRefresh() const { return fullRefreshNeeded; }

    // Clear update flags after rendering
    void clearUpdateFlags();

    // === Focus ===

    // Which slot has focus (receives input)
    void setFocusSlot(uint8_t slot);
    uint8_t getFocusSlot() const { return focusSlot; }

    // === Module Requests ===

    void requestUpdate(Module* from);
    void requestFullRefresh();  // Request full e-ink refresh
    void requestAutoshow(Module* from);

    // === Lifecycle ===

    // Check for auto-transitions in system modules (e.g., boot -> ready)
    void checkAutoTransitions();

private:
    // All registered modules
    std::vector<Module*> modules;

    // System modules (overlays)
    std::vector<SystemModule*> systemModules;

    // Slot assignments
    Module* slots[MAX_SLOTS] = {nullptr};
    uint8_t slotCount = 1;
    uint8_t focusSlot = 0;

    // Current module index for cycling
    size_t currentModuleIndex[MAX_SLOTS] = {0};

    // Module requesting autoshow
    Module* autoshowRequest = nullptr;

    bool updateNeeded = false;
    bool fullRefreshNeeded = false;
};

} // namespace InkHUD2
