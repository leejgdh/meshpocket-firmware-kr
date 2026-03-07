/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "Settings.h"
#include "FSCommon.h"
#include <Arduino.h>

namespace InkHUD2 {

void Settings::load() {
    auto file = FSCom.open(SETTINGS_FILE, FILE_O_READ);
    if (!file) {
        // File doesn't exist yet, use defaults
        return;
    }

    // Simple binary format: [version:1][rotation:1]
    uint8_t version = 0;
    if (file.read(&version, 1) != 1) {
        file.close();
        return;
    }

    if (version == 1) {
        uint8_t rot = 0;
        if (file.read(&rot, 1) == 1) {
            rotation = rot % 4;
        }
    }

    file.close();
}

void Settings::save() {
    auto file = FSCom.open(SETTINGS_FILE, FILE_O_WRITE);
    if (!file) {
        return;
    }

    // Simple binary format: [version:1][rotation:1]
    uint8_t version = 1;
    file.write(&version, 1);
    file.write(&rotation, 1);

    file.close();
}

} // namespace InkHUD2
