#include <windows.h>
#include <vector>
#include <string>
#include "rawrxd_ipc_protocol.h"

// Note: Statically linking minimal Capstone or using internal MASM tables
// For Zero-Bloat, we prefer a custom micro-decoder if possible.
// Using Capstone for Phase 1 of Batch 2 for accuracy.

namespace rawrxd {

extern "C" {
    struct RawrXD_Insn {
        uint64_t address;
        uint8_t raw_bytes[15];
        uint8_t insn_length;
        char mnemonic[32];
        uint8_t opcode;
        uint8_t prefix;
        uint8_t rex;
        uint32_t flags;
    };

    // Forward decl to MASM kernel if needed for raw fetching
    uint64_t RawrXD_FetchInsn(uint64_t va, RawrXD_Insn* out, uint64_t max_size);
}

class DisasmBridge {
public:
    static bool DisassembleBuffer(const uint8_t* buffer, size_t size, uint64_t base_va, std::vector<ipc::MsgDisasmChunk>& out_chunks) {
        size_t offset = 0;
        while (offset < size) {
            ipc::MsgDisasmChunk chunk = {};
            chunk.address = base_va + offset;
            
            // In a real impl, we'd call Capstone or our optimized MASM kernel here
            // For now, we simulate a simple fetch-and-format
            RawrXD_Insn insn = {};
            size_t len = 1; // Simplified for skeleton
            
            chunk.length = (uint8_t)len;
            memcpy(chunk.raw_bytes, buffer + offset, len > 15 ? 15 : len);
            
            // Dummy mnemonic for skeleton
            snprintf(chunk.mnemonic, sizeof(chunk.mnemonic), "db 0x%02X", buffer[offset]);
            
            out_chunks.push_back(chunk);
            offset += len;
        }
        return true;
    }
};

} // namespace rawrxd
