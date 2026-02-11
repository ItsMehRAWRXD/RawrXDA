#pragma once

#include <string>

struct AppState;

namespace overclock_vendor {

bool DetectRyzenMaster(AppState& st);
bool DetectAdrenalinCLI(AppState& st);

bool ApplyCpuOffsetMhz(int offset);
bool ApplyCpuTargetAllCoreMhz(int mhz);
bool ApplyGpuClockOffsetMhz(int offset);

const std::string& LastError();

} // namespace overclock_vendor
