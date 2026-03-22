#include "Win32IDE.h"

#include <cstdint>

namespace {
int g_lastExtensionCommandId = 0;
uint64_t g_extensionCommandCount = 0;
}

void Win32IDE::handleExtensionCommand(int commandId) {
    g_lastExtensionCommandId = commandId;
    g_extensionCommandCount += 1;
}
