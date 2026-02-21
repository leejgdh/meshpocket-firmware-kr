/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "Pipe.h"
#include <algorithm>

namespace InkHUD2 {

// === Module Registration ===

void Pipe::addModule(Module* module) {
    if (!module) return;

    module->setPipe(this);
    modules.push_back(module);

    // Assign to first empty slot
    for (uint8_t i = 0; i < MAX_SLOTS; ++i) {
        if (!slots[i]) {
            slots[i] = module;
            module->active = true;
            module->onActivate();
            break;
        }
    }
}

void Pipe::addSystemModule(SystemModule* module) {
    if (!module) return;

    module->setPipe(this);
    systemModules.push_back(module);

    // Sort by priority
    std::sort(systemModules.begin(), systemModules.end(),
        [](const SystemModule* a, const SystemModule* b) {
            return a->priority < b->priority;
        });

    module->active = true;
    module->onActivate();
}

void Pipe::removeModule(Module* module) {
    if (!module) return;

    // Remove from slots
    for (uint8_t i = 0; i < MAX_SLOTS; ++i) {
        if (slots[i] == module) {
            slots[i] = nullptr;
        }
    }

    // Remove from modules list
    modules.erase(
        std::remove(modules.begin(), modules.end(), module),
        modules.end()
    );

    // Remove from system modules list (no dynamic_cast due to -fno-rtti)
    for (auto it = systemModules.begin(); it != systemModules.end(); ) {
        if (static_cast<Module*>(*it) == module) {
            it = systemModules.erase(it);
        } else {
            ++it;
        }
    }

    module->active = false;
    module->onDeactivate();
    module->setPipe(nullptr);
}

// === Slot Management ===

void Pipe::setSlotCount(uint8_t count) {
    if (count < 1) count = 1;
    if (count > MAX_SLOTS) count = MAX_SLOTS;
    if (count == 3) count = 4;  // Only 1, 2, or 4 slots supported

    slotCount = count;
    updateNeeded = true;

    // Deactivate modules in slots beyond new count
    for (uint8_t i = count; i < MAX_SLOTS; ++i) {
        if (slots[i] && slots[i]->active) {
            slots[i]->active = false;
            slots[i]->onDeactivate();
        }
    }

    // Activate modules in visible slots
    for (uint8_t i = 0; i < count; ++i) {
        if (slots[i] && !slots[i]->active) {
            slots[i]->active = true;
            slots[i]->onActivate();
        }
    }
}

void Pipe::setSlotModule(uint8_t slot, Module* module) {
    if (slot >= MAX_SLOTS) return;

    // Deactivate old module
    if (slots[slot] && slots[slot]->active) {
        slots[slot]->active = false;
        slots[slot]->onDeactivate();
    }

    slots[slot] = module;

    // Activate new module if slot is visible
    if (module && slot < slotCount) {
        module->active = true;
        module->onActivate();
    }

    updateNeeded = true;
}

Module* Pipe::getSlotModule(uint8_t slot) const {
    if (slot >= MAX_SLOTS) return nullptr;
    return slots[slot];
}

void Pipe::cycleSlot(uint8_t slot) {
    if (slot >= slotCount || modules.empty()) return;

    // Find current module's index
    size_t& idx = currentModuleIndex[slot];
    idx = (idx + 1) % modules.size();

    setSlotModule(slot, modules[idx]);

    // No need to force FULL refresh - FAST is enough
    // Old InkHUD uses FAST for module switching
}

// === Event Distribution ===

void Pipe::dispatchEvent(const Event& e) {
    // Send to all registered modules
    for (auto* module : modules) {
        module->onEvent(e);
    }

    // Send to system modules
    for (auto* sysModule : systemModules) {
        sysModule->onEvent(e);
    }
}

void Pipe::dispatchInput(Input input) {
    dispatchInputAndCheck(input);
}

bool Pipe::dispatchInputAndCheck(Input input) {
    // Check if any system module wants to handle input
    for (auto it = systemModules.rbegin(); it != systemModules.rend(); ++it) {
        if ((*it)->handlesInput && (*it)->active) {
            (*it)->onInput(input);
            return true;  // Handled by system module
        }
    }

    // Check if focused slot module wants to handle input (e.g., in submenu mode)
    if (focusSlot < slotCount && slots[focusSlot]) {
        if (slots[focusSlot]->handlesInput && slots[focusSlot]->active) {
            slots[focusSlot]->onInput(input);
            return true;  // Handled by slot module
        }
        // Otherwise send input but don't mark as "handled"
        slots[focusSlot]->onInput(input);
    }

    return false;  // Not handled
}

// === Rendering ===

void Pipe::render(RenderContext& ctx, const Layout& layout) {
    // Check if rendering is locked by a system module
    bool renderingLocked = false;
    for (const auto* sysModule : systemModules) {
        if (sysModule->lockRendering && sysModule->active) {
            renderingLocked = true;
            break;
        }
    }

    // Render slot modules (unless locked)
    if (!renderingLocked) {
        for (uint8_t i = 0; i < slotCount; ++i) {
            if (slots[i] && slots[i]->active) {
                Rect slotRect = layout.slotRect(i, slotCount);
                ctx.setClip(slotRect);
                slots[i]->onRender(ctx);
            }
        }
    }

    // Reset clip for system modules
    ctx.clearClip();

    // Render system modules that always render (overlays)
    for (auto* sysModule : systemModules) {
        if (sysModule->active && (sysModule->alwaysRender || renderingLocked)) {
            sysModule->onRender(ctx);
        }
    }
}

bool Pipe::needsUpdate() const {
    if (updateNeeded) return true;

    for (const auto* module : modules) {
        if (module->needsUpdate()) return true;
    }

    for (const auto* sysModule : systemModules) {
        if (sysModule->needsUpdate()) return true;
    }

    return false;
}

void Pipe::clearUpdateFlags() {
    updateNeeded = false;
    fullRefreshNeeded = false;

    for (auto* module : modules) {
        module->clearUpdateRequest();
    }

    for (auto* sysModule : systemModules) {
        sysModule->clearUpdateRequest();
    }
}

// === Focus ===

void Pipe::setFocusSlot(uint8_t slot) {
    if (slot < slotCount) {
        focusSlot = slot;
    }
}

// === Module Requests ===

void Pipe::requestUpdate(Module* from) {
    if (from) {
        from->updateRequested = true;
    }
    updateNeeded = true;
}

void Pipe::requestFullRefresh() {
    fullRefreshNeeded = true;
    updateNeeded = true;
}

void Pipe::requestAutoshow(Module* from) {
    autoshowRequest = from;
    updateNeeded = true;

    // Find and activate the module in first slot
    for (size_t i = 0; i < modules.size(); ++i) {
        if (modules[i] == from) {
            currentModuleIndex[0] = i;
            setSlotModule(0, from);
            break;
        }
    }
}

// === Lifecycle ===

void Pipe::checkAutoTransitions() {
    // Call checkAutoTransition on system modules that support it
    for (auto* sysModule : systemModules) {
        sysModule->checkAutoTransition();
    }
}

} // namespace InkHUD2
