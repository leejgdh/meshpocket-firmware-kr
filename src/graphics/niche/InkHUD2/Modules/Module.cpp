/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "Module.h"
#include "../Pipe/Pipe.h"

namespace InkHUD2 {

void Module::requestUpdate() {
    updateRequested = true;
    if (pipe) {
        pipe->requestUpdate(this);
    }
}

void Module::requestFullRefresh() {
    updateRequested = true;
    if (pipe) {
        pipe->requestFullRefresh();
    }
}

void Module::requestAutoshow() {
    if (pipe) {
        pipe->requestAutoshow(this);
    }
}

} // namespace InkHUD2
