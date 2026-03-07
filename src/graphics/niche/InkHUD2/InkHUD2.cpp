/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "InkHUD2.h"
#include "Modules/MessageModule.h"
#include "Modules/BatteryModule.h"
#include "Modules/NodeListModule.h"
#include <new>  // for std::nothrow
#include <Arduino.h>  // for millis()

namespace InkHUD2 {

InkHUD2::InkHUD2() : concurrency::OSThread("InkHUD2") {
    // Start disabled - will be enabled after init()
    OSThread::disable();
}

InkHUD2& InkHUD2::instance() {
    static InkHUD2 inst;
    return inst;
}

// Helper to cleanup partial initialization
void InkHUD2::cleanupInit() {
    delete buffer;
    buffer = nullptr;
    delete layoutPtr;
    layoutPtr = nullptr;
    delete fontPtr;
    fontPtr = nullptr;
    driver = nullptr;
}

bool InkHUD2::init(DisplayDriver* drv, const NicheGraphics::CJKFont* cjkFont, uint8_t rotation, float fontScale) {
    if (initialized) return true;
    if (!drv || !cjkFont) return false;

    driver = drv;

    if (!driver->init()) {
        driver = nullptr;
        return false;
    }

    // Create frame buffer with rotation
    buffer = new (std::nothrow) Buffer(driver->width(), driver->height(), rotation);
    if (!buffer || !buffer->isValid()) {
        cleanupInit();
        return false;
    }

    // Create font wrapper using CJKFont
    fontPtr = new (std::nothrow) Font(cjkFont, fontScale);
    if (!fontPtr) {
        cleanupInit();
        return false;
    }

    // Create layout based on logical display size and font
    layoutPtr = new (std::nothrow) Layout(buffer->width(), buffer->height(), fontPtr);
    if (!layoutPtr) {
        cleanupInit();
        return false;
    }

    initialized = true;
    fullRefreshRequested = true;

    // Enable the OSThread
    OSThread::setIntervalFromNow(100);
    OSThread::enabled = true;

    return true;
}

void InkHUD2::shutdown() {
    if (!initialized) return;

    // Stop event observers
    eventsInstance.stop();

    if (driver) {
        driver->sleep();
    }

    delete buffer;
    buffer = nullptr;

    delete layoutPtr;
    layoutPtr = nullptr;

    delete fontPtr;
    fontPtr = nullptr;

    driver = nullptr;
    initialized = false;
}

// OSThread callback - runs periodically
int32_t InkHUD2::runOnce() {
    if (!initialized) {
        return 1000;  // Check again in 1 second
    }

    // Handle delayed operations (e.g., early node sync on ESP32)
    eventsInstance.tick();

    // Check for auto-transitions in system modules
    pipeInstance.checkAutoTransitions();

    // Check if pipe requested full refresh
    if (pipeInstance.needsFullRefresh()) {
        fullRefreshRequested = true;
    }

    // Check if we need to render
    bool pipeNeedsUpdate = pipeInstance.needsUpdate();

    if (pipeNeedsUpdate || fullRefreshRequested) {
        // CRITICAL: Don't render while display is busy!
        // Buffer must not change while finalizeUpdate() copies it to old memory.
        // This prevents ghosting from race condition.
        if (driver->busy()) {
            return 50;  // Try again soon
        }

        render();
        lastUpdateTime = millis();
    }

    return minUpdateInterval;  // Run again after interval
}

void InkHUD2::update() {
    if (!initialized) return;

    // Check for auto-transitions in system modules
    pipeInstance.checkAutoTransitions();

    // Rate limit updates for e-ink
    uint32_t now = millis();
    if (now - lastUpdateTime < minUpdateInterval) {
        return;
    }

    // Check if pipe requested full refresh (e.g., first module switch)
    if (pipeInstance.needsFullRefresh()) {
        fullRefreshRequested = true;
    }

    // Check if we need to render
    bool pipeNeedsUpdate = pipeInstance.needsUpdate();

    if (!pipeNeedsUpdate && !fullRefreshRequested) {
        return;
    }

    render();
    lastUpdateTime = now;
}

void InkHUD2::onEvent(const Event& e) {
    if (!initialized) return;
    pipeInstance.dispatchEvent(e);
}

void InkHUD2::onInput(Input input) {
    if (!initialized) return;

    // Check if any system module handles input
    bool handled = pipeInstance.dispatchInputAndCheck(input);

    // If not handled by system module, handle SELECT at HUD level
    // Note: BACK is already dispatched to slot module by dispatchInputAndCheck
    if (!handled && input == Input::SELECT) {
        cycleSlot(0);
    }
}

void InkHUD2::addModule(Module* module) {
    pipeInstance.addModule(module);
}

void InkHUD2::addSystemModule(SystemModule* module) {
    pipeInstance.addSystemModule(module);
}

void InkHUD2::removeModule(Module* module) {
    pipeInstance.removeModule(module);
}

void InkHUD2::requestFullRefresh() {
    fullRefreshRequested = true;
}

void InkHUD2::connectEvents(MessageModule* msgModule, BatteryModule* batModule, NodeListModule* nodeModule) {
    eventsInstance.begin(msgModule, batModule, nodeModule);
}

void InkHUD2::setSlotCount(uint8_t count) {
    pipeInstance.setSlotCount(count);
}

void InkHUD2::cycleSlot(uint8_t slot) {
    pipeInstance.cycleSlot(slot);
}

void InkHUD2::setRotation(uint8_t r) {
    if (buffer) {
        uint16_t oldW = buffer->width();
        uint16_t oldH = buffer->height();

        buffer->setRotation(r);

        // Recreate layout if dimensions changed (rotation 0/2 vs 1/3)
        if (buffer->width() != oldW || buffer->height() != oldH) {
            delete layoutPtr;
            layoutPtr = new Layout(buffer->width(), buffer->height(), fontPtr);
        }

        // FULL refresh required: old memory contains image in previous orientation,
        // differential refresh would compare misaligned pixel positions
        fullRefreshRequested = true;
    }
}

void InkHUD2::render() {
    if (!buffer || !driver) return;

    // Clear buffer
    buffer->clear();

    // Create render context
    RenderContext ctx(buffer, fontPtr, layoutPtr);

    // Let Pipe render all modules
    pipeInstance.render(ctx, *layoutPtr);

    // Display health: Only do FULL refresh when explicitly requested
    // (boot, after boot logo, etc). During active use, always use FAST.
    // This matches old InkHUD behavior where FULL only happens during
    // "maintenance" periods when user is NOT interacting.
    bool doFullRefresh = fullRefreshRequested;

    // Transfer to display
    driver->update(buffer->getData(), doFullRefresh);

    // Clear flags
    pipeInstance.clearUpdateFlags();
    fullRefreshRequested = false;
}

} // namespace InkHUD2
