#pragma once

#include "../Core/RenderContext.h"
#include "../Pipe/Events.h"
#include <cstdint>

namespace InkHUD2 {

// Forward declaration
class Pipe;

// Base module interface
// Modules receive RenderContext - they DO NOT inherit from GFX
class Module {
public:
    virtual ~Module() = default;

    // Rendering - called when module needs to draw
    virtual void onRender(RenderContext& ctx) = 0;

    // Events from firmware (messages, node updates, etc.)
    virtual void onEvent(const Event& e) {}

    // User input (buttons, joystick)
    virtual void onInput(Input input) {}

    // Lifecycle
    virtual void onActivate() {}   // Called when module becomes active
    virtual void onDeactivate() {} // Called when module is deactivated

    // Module requests
    void requestUpdate();      // Request screen refresh
    void requestFullRefresh(); // Request full e-ink refresh (clears ghosting)
    void requestAutoshow();    // Request to be shown automatically

    // Module state
    bool isActive() const { return active; }
    bool needsUpdate() const { return updateRequested; }

    void clearUpdateRequest() { updateRequested = false; }

    // Set by Pipe
    void setPipe(Pipe* p) { pipe = p; }

    // If true, this module captures all input while active (used by submenus)
    bool handlesInput = false;

protected:
    Pipe* pipe = nullptr;
    bool active = false;
    bool updateRequested = false;

    friend class Pipe;
};

// System module - overlays and special modules
// These can have special behaviors like always rendering or locking input
class SystemModule : public Module {
public:
    // handlesInput is inherited from Module

    // If true, other modules cannot render while this is active
    bool lockRendering = false;

    // If true, this module renders on top of others (overlay)
    bool alwaysRender = false;

    // Priority for overlays (higher = renders later = on top)
    uint8_t priority = 0;

    // Called periodically to check for time-based transitions
    virtual void checkAutoTransition() {}
};

// Module implementation file will need Pipe definition
// See Module.cpp for requestUpdate/requestAutoshow implementation

} // namespace InkHUD2
