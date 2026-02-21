#pragma once

#include "Module.h"

namespace InkHUD2 {

enum class BootState : uint8_t {
    LOGO,       // Show logo
    PAIRING,    // Show pairing code
    READY       // Boot complete, hide
};

// Boot/startup screen - logo and pairing code
class BootModule : public SystemModule {
public:
    BootModule();

    void onRender(RenderContext& ctx) override;
    void onEvent(const Event& e) override;

    // Set boot state
    void setState(BootState state);
    BootState getState() const { return state; }

    // Set pairing PIN (manual override)
    void setPairingCode(uint32_t code);

    // Check and handle auto-transition (call from update loop)
    void checkAutoTransition() override;

    // Set auto-transition delay (0 = disabled)
    void setAutoTransitionDelay(uint32_t ms) { autoTransitionMs = ms; }

private:
    void renderLogo(RenderContext& ctx);
    void renderPairing(RenderContext& ctx);
    void checkBluetoothPairing();  // Check BluetoothState for pairing

    BootState state = BootState::LOGO;
    BootState prevState = BootState::LOGO;  // State before pairing (to restore)
    uint32_t pairingCode = 0;
    uint32_t logoShownAt = 0;
    uint32_t autoTransitionMs = 2000;  // Auto-transition after 2s
};

} // namespace InkHUD2
