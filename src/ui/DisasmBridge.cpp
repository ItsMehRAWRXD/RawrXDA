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
            
            // Decode instruction length using x86 prefix analysis
            size_t len = DecodeInstructionLength(buffer + offset, size - offset);
            if (len == 0) len = 1; // Minimum advance to prevent infinite loop
            
            chunk.length = (uint8_t)len;
            memcpy(chunk.raw_bytes, buffer + offset, len > 15 ? 15 : len);
            
            // Format mnemonic from raw bytes
            FormatMnemonic(buffer + offset, len, chunk.mnemonic, sizeof(chunk.mnemonic));
            
            out_chunks.push_back(chunk);
            offset += len;
        }
        return true;
    }
    
private:
    static size_t DecodeInstructionLength(const uint8_t* data, size_t maxLen) {
        if (maxLen == 0) return 0;
        
        size_t len = 1;
        size_t i = 0;
        
        // Skip legacy prefixes (0x66, 0x67, 0xF0, 0xF2, 0xF3, REX 0x40-0x4F)
        while (i < maxLen && i < 15) {
            uint8_t b = data[i];
            if ((b >= 0x40 && b <= 0x4F) || // REX prefixes
                b == 0x66 || b == 0x67 ||       // Operand/address size override
                b == 0xF0 || b == 0xF2 || b == 0xF3 || // Lock/rep
                b == 0x2E || b == 0x3E ||       // CS/DS segment override
                b == 0x26 || b == 0x36 ||       // ES/SS segment override
                b == 0x64 || b == 0x65) {       // FS/GS segment override
                i++;
                len++;
            } else {
                break;
            }
        }
        
        if (i >= maxLen) return len;
        
        // Opcode byte
        uint8_t opcode = data[i];
        i++;
        
        // Two-byte opcode escape
        if (opcode == 0x0F && i < maxLen) {
            i++; // Skip second opcode byte
            len++;
        }
        
        // ModR/M byte (most instructions have this)
        if (i < maxLen) {
            uint8_t modrm = data[i];
            uint8_t mod = (modrm >> 6) & 0x3;
            uint8_t rm = modrm & 0x7;
            
            if (mod != 3 && rm == 4 && i + 1 < maxLen) {
                // SIB byte present
                i++;
                len++;
            }
            
            if (mod == 1) {
                // 8-bit displacement
                len++;
            } else if (mod == 2 || (mod == 0 && rm == 5)) {
                // 32-bit displacement
                len += 4;
            }
            
            i++;
            len++;
        }
        
        // Immediate operand (simplified: many opcodes have imm8/imm32)
        // This is a heuristic - real decoder needs full opcode table
        if (len < maxLen && (opcode >= 0xB0 && opcode <= 0xBF)) {
            // MOV imm
            len += (opcode >= 0xB8) ? 4 : 1;
        }
        
        return len > maxLen ? maxLen : len;
    }
    
    static void FormatMnemonic(const uint8_t* data, size_t len, char* out, size_t outSize) {
        if (len == 0) {
            snprintf(out, outSize, "nop");
            return;
        }
        
        // Simple mnemonic lookup for common opcodes
        switch (data[0]) {
            case 0x90: snprintf(out, outSize, "nop"); break;
            case 0xC3: snprintf(out, outSize, "ret"); break;
            case 0xC2: snprintf(out, outSize, "retn"); break;
            case 0xE8: snprintf(out, outSize, "call"); break;
            case 0xE9: snprintf(out, outSize, "jmp"); break;
            case 0xEB: snprintf(out, outSize, "jmp short"); break;
            case 0x50: snprintf(out, outSize, "push rax"); break;
            case 0x51: snprintf(out, outSize, "push rcx"); break;
            case 0x52: snprintf(out, outSize, "push rdx"); break;
            case 0x53: snprintf(out, outSize, "push rbx"); break;
            case 0x58: snprintf(out, outSize, "pop rax"); break;
            case 0x59: snprintf(out, outSize, "pop rcx"); break;
            case 0x5A: snprintf(out, outSize, "pop rdx"); break;
            case 0x5B: snprintf(out, outSize, "pop rbx"); break;
            case 0x88: snprintf(out, outSize, "mov byte"); break;
            case 0x89: snprintf(out, outSize, "mov dword"); break;
            case 0x8B: snprintf(out, outSize, "mov reg"); break;
            case 0x0F: snprintf(out, outSize, "extended"); break;
            default:   snprintf(out, outSize, "db 0x%02X", data[0]); break;
        }
    }
};

} // namespace rawrxd
