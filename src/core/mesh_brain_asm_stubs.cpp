// mesh_brain_asm_stubs.cpp — Fallback implementations for mesh ASM symbols
// not exported by RawrXD_MeshBrain.asm.

#include <cstdint>

extern "C" {

int asm_mesh_crdt_lookup(uint64_t /*key*/, uint64_t* outValue) {
    if (outValue) {
        *outValue = 0;
    }
    return 0;
}

void asm_mesh_topology_remove(const uint64_t* /*nodeId*/) {}

uint32_t asm_mesh_topology_count(void) {
    return 0;
}

void asm_mesh_topology_list(void* /*outBuf*/, uint32_t /*maxCount*/) {}

}