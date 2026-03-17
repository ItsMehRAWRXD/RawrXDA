#pragma once

#include <cstdint>

enum SSOTOwner : uint32_t {
    SSOT_OWNER_NONE = 0,
    SSOT_OWNER_CORE = 1,
    SSOT_OWNER_EXT = 2,
    SSOT_OWNER_AUTO = 3,
    SSOT_OWNER_STUBS = 4,
    SSOT_OWNER_FEATURES = 5,
};

enum SSOTBeaconFlags : uint32_t {
    SSOT_BEACON_NONE = 0,
    SSOT_BEACON_CPP_PROVIDER = 1u << 0,
};

struct HandlerBeacon {
    const char* symbol;
    uint32_t owner;
    uint32_t flags;
    uint32_t hash;
};

extern "C" {
__declspec(dllexport) uint32_t rawr_ssot_active_owner();
__declspec(dllexport) uint32_t rawr_ssot_owner_for_hash(uint32_t hash);
__declspec(dllexport) uint32_t rawr_ssot_owner_for_symbol(const char* symbol);
__declspec(dllexport) const HandlerBeacon* rawr_ssot_beacons_begin();
__declspec(dllexport) uint32_t rawr_ssot_beacons_count();
}
