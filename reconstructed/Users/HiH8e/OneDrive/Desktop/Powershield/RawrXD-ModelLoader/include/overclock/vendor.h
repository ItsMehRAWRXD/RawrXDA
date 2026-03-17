#pragma once
#include <string>

struct AppState;

namespace overclock_vendor {
    bool DetectRyzenMaster(AppState&);
    bool DetectAdrenalinCLI(AppState&);
    bool ApplyCpuOffsetMhz(int offset);      // placeholder, logs action
    bool ApplyCpuTargetAllCoreMhz(int mhz);  // placeholder
    bool ApplyGpuClockOffsetMhz(int offset); // placeholder
    const std::string& LastError();
}
