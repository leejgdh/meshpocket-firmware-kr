#pragma once

#include <cstdint>
#include "main.h"  // For bluetoothStatus, CallbackObserver

namespace InkHUD2 {

// Tracks Bluetooth pairing state
// Singleton - access via BluetoothState::instance()
class BluetoothState {
public:
    static BluetoothState& instance() {
        static BluetoothState s;
        return s;
    }

    // Check if currently in pairing mode
    bool isPairing() const { return pairing; }

    // Get current pairing code (6 digits)
    uint32_t getPairingCode() const { return pairingCode; }

    // Check if state changed since last check (auto-clears flag)
    bool stateChanged() {
        bool changed = stateChangedFlag;
        stateChangedFlag = false;
        return changed;
    }

private:
    BluetoothState();
    BluetoothState(const BluetoothState&) = delete;
    BluetoothState& operator=(const BluetoothState&) = delete;

    int onBluetoothStatusUpdate(const meshtastic::Status *status);

    bool pairing = false;
    uint32_t pairingCode = 0;
    bool stateChangedFlag = false;

    // Bluetooth status observer
    CallbackObserver<BluetoothState, const meshtastic::Status *> bluetoothStatusObserver =
        CallbackObserver<BluetoothState, const meshtastic::Status *>(this, &BluetoothState::onBluetoothStatusUpdate);
};

} // namespace InkHUD2
