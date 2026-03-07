#pragma once

#include "../InkHUD2.h"
#include "graphics/niche/Drivers/EInk/EInk.h"

namespace InkHUD2 {

// Adapter to use existing NicheGraphics EInk drivers with InkHUD2
class EInkAdapter : public DisplayDriver {
public:
    EInkAdapter(NicheGraphics::Drivers::EInk* driver) : einkDriver(driver) {}

    bool init() override {
        // Driver already initialized via begin() before this adapter is created
        return einkDriver != nullptr;
    }

    uint16_t width() const override {
        return einkDriver ? einkDriver->width : 0;
    }

    uint16_t height() const override {
        return einkDriver ? einkDriver->height : 0;
    }

    // Check if display is busy with previous update
    bool busy() const override {
        return einkDriver ? einkDriver->busy() : false;
    }

    void update(const uint8_t* data, bool fullRefresh) override {
        if (!einkDriver) return;

        // Note: caller should check busy() before calling update()
        // to avoid overwriting buffer while finalizeUpdate() needs it

        // Wait for any in-progress update (including finalizeUpdate)
        einkDriver->await();

        // Determine update type
        auto updateType = fullRefresh
            ? NicheGraphics::Drivers::EInk::UpdateTypes::FULL
            : NicheGraphics::Drivers::EInk::UpdateTypes::FAST;

        // Call driver update (cast away const - driver doesn't modify data)
        einkDriver->update(const_cast<uint8_t*>(data), updateType);
    }

    void sleep() override {
        // Wait for update to complete before sleeping
        if (einkDriver) {
            einkDriver->await();
        }
        // Note: actual sleep implementation depends on driver
        // Most e-ink displays automatically enter low-power after update
    }

    void wake() override {
        // Most e-ink displays wake automatically on next command
    }

private:
    NicheGraphics::Drivers::EInk* einkDriver;
};

} // namespace InkHUD2
