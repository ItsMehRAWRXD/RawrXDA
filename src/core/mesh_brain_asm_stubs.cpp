// mesh_brain_asm_stubs.cpp — Implementations for mesh ASM symbols not exported by RawrXD_MeshBrain.asm.
// RawrXD_MeshBrain.asm exports topology_update and topology_active_count only; these four are C fallbacks.

#include <cstdint>

extern "C" {

// Not in RawrXD_MeshBrain.asm PUBLIC list; provide no-op implementations.
int asm_mesh_crdt_lookup(uint64_t /*key*/, uint64_t* outValue) {
    if (outValue) *outValue = 0;
    return 0; // not found
}

void asm_mesh_topology_remove(const uint64_t* /*nodeId*/) {}

uint32_t asm_mesh_topology_count(void) {
    return 0;
}

void asm_mesh_topology_list(void* /*outBuf*/, uint32_t /*maxCount*/) {}

}
