/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "BootModule.h"
#include "../Core/Logo.h"
#include "../Core/Settings.h"
#include "../Core/BluetoothState.h"
#include "../InkHUD2.h"
#include <cstdio>
#include <Arduino.h>  // for millis()
#include "configuration.h"  // provides xstr() macro
#include "mesh/NodeDB.h"

namespace InkHUD2 {

BootModule::BootModule() {
    handlesInput = false;
    lockRendering = true;   // Block other modules during boot
    alwaysRender = false;   // Only render when active
    priority = 200;         // Highest priority

    // Initialize BluetoothState early to catch pairing events
    (void)BluetoothState::instance();
}

void BootModule::onRender(RenderContext& ctx) {
    if (state == BootState::READY) return;

    // Clear entire screen
    ctx.clearClip();
    ctx.clear();

    switch (state) {
        case BootState::LOGO:
            renderLogo(ctx);
            break;
        case BootState::PAIRING:
            renderPairing(ctx);
            break;
        default:
            break;
    }
}

void BootModule::onEvent(const Event& e) {
    if (e.type == EventType::BOOT_COMPLETE) {
        setState(BootState::READY);
    }
}

void BootModule::setState(BootState newState) {
    // Track when logo was first shown (even if already in LOGO state)
    if (newState == BootState::LOGO && logoShownAt == 0) {
        logoShownAt = millis();
    }

    if (state != newState) {
        state = newState;

        // Update system module flags
        if (state == BootState::READY) {
            lockRendering = false;
            active = false;
        } else {
            lockRendering = true;
            active = true;
        }

        requestUpdate();
    }
}

void BootModule::checkAutoTransition() {
    // Check bluetooth pairing state
    checkBluetoothPairing();

    // Auto-transition from logo to ready
    if (state == BootState::LOGO && autoTransitionMs > 0 && logoShownAt > 0) {
        uint32_t elapsed = millis() - logoShownAt;
        if (elapsed >= autoTransitionMs) {
            setState(BootState::READY);
            requestFullRefresh();
        }
    }
}

void BootModule::checkBluetoothPairing() {
    auto& btState = BluetoothState::instance();
    bool isPairing = btState.isPairing();

    // Pairing started - switch to pairing screen
    if (isPairing && state != BootState::PAIRING) {
        pairingCode = btState.getPairingCode();
        prevState = state;
        state = BootState::PAIRING;
        active = true;
        lockRendering = true;
        requestUpdate();
        InkHUD2::instance().requestFullRefresh();
    }
    // Pairing ended - restore previous state
    else if (!isPairing && state == BootState::PAIRING) {
        state = prevState;
        if (state == BootState::READY) {
            active = false;
            lockRendering = false;
        }
        requestUpdate();
        InkHUD2::instance().requestFullRefresh();
    }
}

void BootModule::setPairingCode(uint32_t code) {
    pairingCode = code;
    if (state == BootState::PAIRING) {
        requestUpdate();
    }
}

void BootModule::renderLogo(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    uint16_t w = layout->width();
    uint16_t h = layout->height();
    uint16_t padding = layout->padding();

    // Version in top-left corner
    ctx.text(padding, padding, xstr(APP_VERSION_SHORT), Align::LEFT, Color::BLACK);

    // Node short name in top-right corner
    meshtastic_NodeInfoLite *ourNode = nodeDB->getMeshNode(nodeDB->getNodeNum());
    if (ourNode && ourNode->has_user) {
        ctx.text(w - padding, padding, ourNode->user.short_name, Align::RIGHT, Color::BLACK);
    }

    // Logo region (80% width, 50% height, slightly above center)
    uint16_t logoWLimit = w * Layout::logoWidthPercent / 100;
    uint16_t logoHLimit = h * Layout::logoHeightPercent / 100;
    uint16_t logoW = Logo::getWidth(logoWLimit, logoHLimit);
    uint16_t logoH = Logo::getHeight(logoWLimit, logoHLimit);

    int16_t logoCX = w / 2;
    int16_t logoCY = h / 2 - h * Layout::logoCenterOffset / 100;

    // Draw Meshtastic logo
    Logo::draw(ctx, logoCX, logoCY, logoW, logoH);

    // "Meshtastic" text below logo
    int16_t logoBottom = logoCY + logoH / 2;
    ctx.text(logoCX, logoBottom + h * Layout::logoTextGap / 100, "Meshtastic", Align::CENTER, Color::BLACK);
}

void BootModule::renderPairing(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    uint16_t w = layout->width();
    uint16_t h = layout->height();
    uint16_t lineH = layout->lineHeight();

    int16_t centerX = w / 2;

    // Check if this is fixed PIN mode (predefined PIN)
    bool isFixedPin = (config.bluetooth.mode == meshtastic_Config_BluetoothConfig_PairingMode_FIXED_PIN);
    // Check if PIN should be hidden (user setting)
    bool hidePin = Settings::instance().getHidePIN();

    // Two-tier system — pairing strings are body tier on every layout.
    float headerScale = Layout::bodyScale;
    float textScale = Layout::bodyScale;

    // "Bluetooth" header at Y=25%
    int16_t headerY = h * Layout::pairingHeaderY / 100;
    ctx.textScaled(centerX, headerY, "Bluetooth", headerScale, Align::CENTER, Color::BLACK);

    if (isFixedPin && hidePin) {
        // Fixed PIN mode with hidden PIN - show "Pairing with predefined PIN"
        int16_t midY = h * Layout::pairingPinY / 100;
        ctx.textScaled(centerX, midY - lineH / 2, "Pairing with", textScale, Align::CENTER, Color::BLACK);
        ctx.textScaled(centerX, midY + lineH / 2, "predefined PIN", textScale, Align::CENTER, Color::BLACK);
    } else {
        // Show PIN (hidden or actual)
        // "Enter this code" centered between header and PIN (equal spacing)
        int16_t pinY = h * Layout::pairingPinY / 100;
        int16_t enterCodeY = (headerY + pinY) / 2;
        ctx.textScaled(centerX, enterCodeY, "Enter this code", textScale, Align::CENTER, Color::BLACK);

        // PIN code at Y=50% with space in middle "123 456"
        char pinStr[16];
        if (hidePin) {
            snprintf(pinStr, sizeof(pinStr), "*** ***");
        } else {
            uint32_t first3 = pairingCode / 1000;
            uint32_t last3 = pairingCode % 1000;
            snprintf(pinStr, sizeof(pinStr), "%03lu %03lu", (unsigned long)first3, (unsigned long)last3);
        }
        // PIN code body tier (two-tier system — no separate emphasized
        // tier for PIN; body is already large enough on this display).
        float pinScale = Layout::bodyScale;
        ctx.textScaled(centerX, pinY, pinStr, pinScale, Align::CENTER, Color::BLACK);
    }

    // Device short name at Y=75%
    int16_t nameY = h * Layout::pairingNameY / 100;
    meshtastic_NodeInfoLite *ourNode = nodeDB->getMeshNode(nodeDB->getNodeNum());
    if (ourNode && ourNode->has_user) {
        ctx.textScaled(centerX, nameY, ourNode->user.short_name, textScale, Align::CENTER, Color::BLACK);
    }
}

} // namespace InkHUD2
