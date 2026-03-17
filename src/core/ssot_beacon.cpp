#include "ssot_beacon.h"

#include <cstring>

namespace {

constexpr uint32_t fnv1a_const(const char* s) {
    uint32_t h = 2166136261u;
    while (*s) {
        h ^= static_cast<uint8_t>(*s++);
        h *= 16777619u;
    }
    return h;
}

uint32_t fnv1a_runtime(const char* s) {
    if (!s) return 0;
    uint32_t h = 2166136261u;
    while (*s) {
        h ^= static_cast<uint8_t>(*s++);
        h *= 16777619u;
    }
    return h;
}

#if defined(RAWR_SSOT_CORE) && (RAWR_SSOT_CORE == 1)
constexpr uint32_t kActiveOwner = SSOT_OWNER_CORE;
#elif defined(RAWR_SSOT_EXT) && (RAWR_SSOT_EXT == 1)
constexpr uint32_t kActiveOwner = SSOT_OWNER_EXT;
#elif defined(RAWR_SSOT_AUTO) && (RAWR_SSOT_AUTO == 1)
constexpr uint32_t kActiveOwner = SSOT_OWNER_AUTO;
#elif defined(RAWR_SSOT_STUBS) && (RAWR_SSOT_STUBS == 1)
constexpr uint32_t kActiveOwner = SSOT_OWNER_STUBS;
#elif defined(RAWR_SSOT_FEATURES) && (RAWR_SSOT_FEATURES == 1)
constexpr uint32_t kActiveOwner = SSOT_OWNER_FEATURES;
#else
constexpr uint32_t kActiveOwner = SSOT_OWNER_NONE;
#endif

#define HB(symbol_literal) \
    { symbol_literal, kActiveOwner, SSOT_BEACON_CPP_PROVIDER, fnv1a_const(symbol_literal) }

// Focused beacon set for current EXT ownership lane and ASM fallback checks.
constexpr HandlerBeacon kBeacons[] = {
#include "ssot_beacon_table.inc"
};

#undef HB

}  // namespace

extern "C" __declspec(dllexport) uint32_t rawr_ssot_active_owner() {
    return kActiveOwner;
}

extern "C" __declspec(dllexport) uint32_t rawr_ssot_owner_for_hash(uint32_t hash) {
    for (const auto& beacon : kBeacons) {
        if (beacon.hash == hash) return beacon.owner;
    }
    return SSOT_OWNER_NONE;
}

extern "C" __declspec(dllexport) uint32_t rawr_ssot_owner_for_symbol(const char* symbol) {
    if (!symbol || !*symbol) return SSOT_OWNER_NONE;
    return rawr_ssot_owner_for_hash(fnv1a_runtime(symbol));
}

extern "C" __declspec(dllexport) const HandlerBeacon* rawr_ssot_beacons_begin() {
    return &kBeacons[0];
}

extern "C" __declspec(dllexport) uint32_t rawr_ssot_beacons_count() {
    return static_cast<uint32_t>(sizeof(kBeacons) / sizeof(kBeacons[0]));
}
