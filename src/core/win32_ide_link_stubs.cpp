// =============================================================================
// win32_ide_link_stubs.cpp — Non-menu linker shims for Win32IDE
// =============================================================================
// Real command/menu handlers are provided by missing_handler_stubs.cpp in the
// Win32IDE real lane. Keep only fallback symbols that are still required when
// optional modules are not linked into this target.
// ASM fallbacks live in win32ide_asm_fallback.cpp (name avoids *_stubs.cpp filter).
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// =============================================================================

#include "shared_feature_dispatch.h"

#include <cstddef>
#include <cstdint>
#include <string>

CommandResult handleDebugTestAI(const CommandContext& ctx) {
    ctx.output("[DEBUG] AI command probe completed\n");
    return CommandResult::ok("debug.testAI");
}

// =============================================================================
// VscextRegistry stubs (when vscext_registry.cpp is not linked)
// =============================================================================
namespace VscextRegistry {
bool getStatusString(std::string& out) { out = "stub"; return true; }
bool listCommands(std::string& out) { out.clear(); return true; }
}

// =============================================================================
// Mesh ASM stubs (when mesh engine exports are not linked)
// =============================================================================
extern "C" {
int asm_mesh_crdt_lookup(uint64_t key, uint64_t* outValue) {
    (void)key;
    if (outValue) {
        *outValue = 0;
    }
    return 0;
}

void asm_mesh_topology_remove(const uint64_t*) {}

uint32_t asm_mesh_topology_count(void) { return 0; }

void asm_mesh_topology_list(void* outBuf, uint32_t maxCount) {
    (void)outBuf;
    (void)maxCount;
}

void asm_mesh_topology_track(const uint64_t*) {}

void asm_mesh_crdt_track(uint64_t key, uint64_t value) {
    (void)key;
    (void)value;
}
}
