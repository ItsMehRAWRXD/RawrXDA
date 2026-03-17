#include "Win32SwarmBridge.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>

namespace RawrXD::Bridge {

// IAT slot indices from audit
constexpr int IAT_SLOT_INITIALIZE_SWARM = 20;
constexpr int IAT_SLOT_EXECUTE_SWARM = 54;
constexpr int IAT_SLOT_SHUTDOWN_SWARM = 55;

bool RegisterSwarmBridgeWithIAT() {
    // Resolve runtime patcher exports dynamically to avoid hard link dependencies.
    using InstallHookFn = int(__cdecl*)(const char*, void*);
    using GetHookFn = void*(__cdecl*)(uint64_t);

    HMODULE self = GetModuleHandleA(nullptr);
    if (!self) return false;

    auto installHook = reinterpret_cast<InstallHookFn>(GetProcAddress(self, "RAWRXD_InstallHook"));
    auto getHook = reinterpret_cast<GetHookFn>(GetProcAddress(self, "RAWRXD_GetHook"));

    if (!installHook) {
        OutputDebugStringA("[SwarmIAT] runtime_patcher not present - skipping IAT bind");
        return false;
    }

    auto* initProc = reinterpret_cast<void*>(&Win32IDE_initializeSwarmSystem);
    bool success = installHook("Win32IDE_initializeSwarmSystem", initProc) != 0;

    if (success) {
        OutputDebugStringA("[SwarmIAT] Slot 20 bound to Win32IDE_initializeSwarmSystem\n");

        if (getHook) {
            void* execSwarm = getHook(IAT_SLOT_EXECUTE_SWARM);
            if (!execSwarm) {
                OutputDebugStringA("[SwarmIAT] WARNING: SubAgentManager slot 54 not yet resolved\n");
            }
        }
    } else {
        OutputDebugStringA("[SwarmIAT] FAILED to bind slot 20\n");
    }

    return success;
}

} // namespace RawrXD::Bridge
