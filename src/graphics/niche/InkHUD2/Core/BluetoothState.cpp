/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "BluetoothState.h"
#include "BluetoothStatus.h"

namespace InkHUD2 {

BluetoothState::BluetoothState() {
    // Subscribe to bluetooth status updates
    bluetoothStatusObserver.observe(&bluetoothStatus->onNewStatus);
}

int BluetoothState::onBluetoothStatusUpdate(const meshtastic::Status *status) {
    // Check status type
    if (status->getStatusType() != STATUS_TYPE_BLUETOOTH) {
        return 0;
    }

    const auto *btStatus = static_cast<const meshtastic::BluetoothStatus *>(status);
    auto connState = btStatus->getConnectionState();

    bool wasPairing = pairing;

    // When pairing begins
    if (connState == meshtastic::BluetoothStatus::ConnectionState::PAIRING) {
        // Parse passkey string to uint32
        std::string passkey = btStatus->getPasskey();
        uint32_t code = 0;
        for (char c : passkey) {
            if (c >= '0' && c <= '9') {
                code = code * 10 + (c - '0');
            }
        }
        pairingCode = code;
        pairing = true;
    } else {
        pairing = false;
    }

    // Set changed flag if state changed
    if (wasPairing != pairing) {
        stateChangedFlag = true;
    }

    return 0;
}

} // namespace InkHUD2
