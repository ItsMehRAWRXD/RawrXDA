// ============================================================================
// Code Emitter Implementation
// Advanced machine code generation with relocation support
// ============================================================================

#include "code_emitter.h"
#include <algorithm>
#include <stdexcept>

namespace pewriter {

// ============================================================================
// CodeEmitter Implementation
// ============================================================================

CodeEmitter::CodeEmitter() : architecture_(PEArchitecture::x64), currentOffset_(0) {}

void CodeEmitter::setArchitecture(PEArchitecture arch) {
    architecture_ = arch;
    reset();
}

bool CodeEmitter::emitSection(const CodeSection& section) {
    // Record the base offset for this section within the emitted code stream
    uint32_t sectionBase = currentOffset_;

    // First pass: scan the raw code bytes for embedded label definitions.
    // Labels are stored as metadata in the section; copy the raw machine code,
    // then resolve any relocation entries that reference known labels.
    code_.insert(code_.end(), section.code.begin(), section.code.end());
    currentOffset_ += static_cast<uint32_t>(section.code.size());

    // Second pass: resolve relocations that reference labels within this section.
    // Walk each pending relocation; if its symbol matches a known label, patch
    // the displacement in the emitted code buffer.
    for (auto& reloc : relocations_) {
        if (reloc.symbol.empty()) continue;

        auto it = labels_.find(reloc.symbol);
        if (it == labels_.end() || !it->second.resolved) continue;

        uint32_t targetOffset = it->second.offset;
        uint32_t patchSite    = reloc.offset;

        if (patchSite + 4 > code_.size()) continue;

        if (reloc.type == 4) { // IMAGE_REL_AMD64_REL32
            // rel32 displacement = target - (patchSite + 4)
            int32_t displacement = static_cast<int32_t>(targetOffset) -
                                   static_cast<int32_t>(patchSite + 4);
            displacement += static_cast<int32_t>(reloc.addend);
            code_[patchSite + 0] = static_cast<uint8_t>(displacement & 0xFF);
            code_[patchSite + 1] = static_cast<uint8_t>((displacement >> 8)  & 0xFF);
            code_[patchSite + 2] = static_cast<uint8_t>((displacement >> 16) & 0xFF);
            code_[patchSite + 3] = static_cast<uint8_t>((displacement >> 24) & 0xFF);
            reloc.symbol.clear(); // Mark resolved
        } else if (reloc.type == 1) { // IMAGE_REL_AMD64_ADDR64
            if (patchSite + 8 > code_.size()) continue;
            uint64_t absAddr = static_cast<uint64_t>(targetOffset) +
                               static_cast<uint64_t>(reloc.addend);
            for (int i = 0; i < 8; ++i) {
                code_[patchSite + i] = static_cast<uint8_t>((absAddr >> (i * 8)) & 0xFF);
            }
            reloc.symbol.clear();
        }
    }

    // Remove fully-resolved relocations
    relocations_.erase(
        std::remove_if(relocations_.begin(), relocations_.end(),
                        [](const RelocationEntry& r) { return r.symbol.empty(); }),
        relocations_.end());

    return true;
}

bool CodeEmitter::emitMOV_R64_IMM64(uint8_t reg, uint64_t imm) {
    if (reg > 15) return false;

    // REX prefix if needed
    if (reg >= 8) {
        encodeREX(true, false, false, true);
    } else {
        encodeREX(true, false, false, false);
    }

    // Opcode + register
    addByte(OPCODE_MOV_R64_IMM64 + (reg & 7));

    // Immediate value
    addQword(imm);

    return true;
}

bool CodeEmitter::emitCALL_REL32(uint32_t targetRVA) {
    addByte(OPCODE_CALL_REL32);

    // Calculate relative displacement
    int32_t displacement = static_cast<int32_t>(targetRVA) - static_cast<int32_t>(currentOffset_ + 4);
    addDword(static_cast<uint32_t>(displacement));

    // Add relocation entry
    RelocationEntry reloc;
    reloc.offset = currentOffset_ - 4;
    reloc.type = 4; // IMAGE_REL_AMD64_REL32
    reloc.symbol = ""; // Will be resolved later
    reloc.addend = targetRVA;
    addRelocation(reloc);

    return true;
}

bool CodeEmitter::emitRET() {
    addByte(OPCODE_RET);
    return true;
}

bool CodeEmitter::emitPUSH_R64(uint8_t reg) {
    if (reg > 15) return false;

    if (reg >= 8) {
        encodeREX(false, false, false, true);
    }

    addByte(OPCODE_PUSH_R64 + (reg & 7));
    return true;
}

bool CodeEmitter::emitPOP_R64(uint8_t reg) {
    if (reg > 15) return false;

    if (reg >= 8) {
        encodeREX(false, false, false, true);
    }

    addByte(OPCODE_POP_R64 + (reg & 7));
    return true;
}

bool CodeEmitter::emitSUB_RSP_IMM8(uint8_t imm) {
    // SUB RSP, imm8 = REX.W (48) + opcode (83) + ModRM (EC = mod:11 reg:5 r/m:4=RSP) + imm8
    addByte(0x48); // REX.W
    addByte(0x83); // SUB r/m64, imm8 opcode
    addByte(0xEC); // ModRM: mod=11, reg=5 (SUB), r/m=4 (RSP)
    addByte(imm);
    return true;
}

bool CodeEmitter::emitADD_RSP_IMM8(uint8_t imm) {
    // ADD RSP, imm8 = REX.W (48) + opcode (83) + ModRM (C4 = mod:11 reg:0 r/m:4=RSP) + imm8
    addByte(0x48); // REX.W
    addByte(0x83); // ADD r/m64, imm8 opcode
    addByte(0xC4); // ModRM: mod=11, reg=0 (ADD), r/m=4 (RSP)
    addByte(imm);
    return true;
}

bool CodeEmitter::emitMOV_RCX_IMM32(uint32_t imm) {
    // MOV ECX, imm32 = B9 + imm32 (5 bytes, zero-extends to RCX on x64)
    // This is more efficient than MOV RCX, imm64 for 32-bit values
    addByte(0xB9); // MOV ECX, imm32 opcode
    addDword(imm);
    return true;
}

bool CodeEmitter::emitINT3() {
    addByte(OPCODE_INT3);
    return true;
}

bool CodeEmitter::emitFunctionPrologue() {
    // Standard x64 function prologue with frame pointer:
    // push rbp; mov rbp, rsp; sub rsp, 28h
    emitPUSH_R64(5); // push rbp (reg 5 = RBP)
    // mov rbp, rsp = REX.W + 89 E5 (MOV r/m64, r64)
    addByte(0x48); // REX.W
    addByte(0x89);
    addByte(0xE5); // ModRM: mod=11, reg=4(RSP), r/m=5(RBP) -> MOV RBP, RSP
    return emitSUB_RSP_IMM8(0x28);
}

bool CodeEmitter::emitFunctionEpilogue() {
    // Standard x64 function epilogue with frame pointer teardown:
    // add rsp, 28h; pop rbp; ret
    if (!emitADD_RSP_IMM8(0x28)) return false;
    if (!emitPOP_R64(5)) return false; // pop rbp
    return emitRET();
}

bool CodeEmitter::createLabel(const std::string& name) {
    Label label;
    label.name = name;
    label.offset = currentOffset_;
    label.resolved = true;
    labels_[name] = label;
    return true;
}

bool CodeEmitter::emitJMP_LABEL(const std::string& label) {
    addByte(OPCODE_JMP_REL32);

    auto it = labels_.find(label);
    if (it != labels_.end() && it->second.resolved) {
        int32_t displacement = static_cast<int32_t>(it->second.offset) -
                             static_cast<int32_t>(currentOffset_ + 4);
        addDword(static_cast<uint32_t>(displacement));
    } else {
        // Unresolved label - add placeholder and relocation
        addDword(0);
        RelocationEntry reloc;
        reloc.offset = currentOffset_ - 4;
        reloc.type = 4; // IMAGE_REL_AMD64_REL32
        reloc.symbol = label;
        reloc.addend = 0;
        addRelocation(reloc);
    }

    return true;
}

bool CodeEmitter::emitJE_LABEL(const std::string& label) {
    addByte(0x0F);
    addByte(OPCODE_JE_REL32);

    // Similar to JMP_LABEL but for conditional jumps
    auto it = labels_.find(label);
    if (it != labels_.end() && it->second.resolved) {
        int32_t displacement = static_cast<int32_t>(it->second.offset) -
                             static_cast<int32_t>(currentOffset_ + 5);
        addDword(static_cast<uint32_t>(displacement));
    } else {
        addDword(0);
        RelocationEntry reloc;
        reloc.offset = currentOffset_ - 4;
        reloc.type = 4;
        reloc.symbol = label;
        reloc.addend = 0;
        addRelocation(reloc);
    }

    return true;
}

bool CodeEmitter::emitJNE_LABEL(const std::string& label) {
    addByte(0x0F);
    addByte(OPCODE_JNE_REL32);

    // Similar to JE_LABEL
    auto it = labels_.find(label);
    if (it != labels_.end() && it->second.resolved) {
        int32_t displacement = static_cast<int32_t>(it->second.offset) -
                             static_cast<int32_t>(currentOffset_ + 5);
        addDword(static_cast<uint32_t>(displacement));
    } else {
        addDword(0);
        RelocationEntry reloc;
        reloc.offset = currentOffset_ - 4;
        reloc.type = 4;
        reloc.symbol = label;
        reloc.addend = 0;
        addRelocation(reloc);
    }

    return true;
}

bool CodeEmitter::emitDB(uint8_t byte) {
    addByte(byte);
    return true;
}

bool CodeEmitter::emitDW(uint16_t word) {
    addWord(word);
    return true;
}

bool CodeEmitter::emitDD(uint32_t dword) {
    addDword(dword);
    return true;
}

bool CodeEmitter::emitDQ(uint64_t qword) {
    addQword(qword);
    return true;
}

const std::vector<uint8_t>& CodeEmitter::getCode() const {
    return code_;
}

const std::vector<RelocationEntry>& CodeEmitter::getRelocations() const {
    return relocations_;
}

void CodeEmitter::reset() {
    code_.clear();
    relocations_.clear();
    labels_.clear();
    currentOffset_ = 0;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool CodeEmitter::encodeREX(bool w, bool r, bool x, bool b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (r) rex |= 0x04;
    if (x) rex |= 0x02;
    if (b) rex |= 0x01;
    addByte(rex);
    return true;
}

bool CodeEmitter::encodeModRM(uint8_t mod, uint8_t reg, uint8_t rm) {
    uint8_t modrm = (mod << 6) | ((reg & 7) << 3) | (rm & 7);
    addByte(modrm);
    return true;
}

bool CodeEmitter::encodeSIB(uint8_t scale, uint8_t index, uint8_t base) {
    uint8_t sib = ((scale & 3) << 6) | ((index & 7) << 3) | (base & 7);
    addByte(sib);
    return true;
}

void CodeEmitter::addByte(uint8_t byte) {
    code_.push_back(byte);
    currentOffset_++;
}

void CodeEmitter::addWord(uint16_t word) {
    code_.push_back(word & 0xFF);
    code_.push_back((word >> 8) & 0xFF);
    currentOffset_ += 2;
}

void CodeEmitter::addDword(uint32_t dword) {
    for (int i = 0; i < 4; ++i) {
        code_.push_back((dword >> (i * 8)) & 0xFF);
    }
    currentOffset_ += 4;
}

void CodeEmitter::addQword(uint64_t qword) {
    for (int i = 0; i < 8; ++i) {
        code_.push_back((qword >> (i * 8)) & 0xFF);
    }
    currentOffset_ += 8;
}

void CodeEmitter::addRelocation(const RelocationEntry& reloc) {
    relocations_.push_back(reloc);
}

bool CodeEmitter::resolveLabel(const std::string& name, uint32_t targetOffset) {
    auto it = labels_.find(name);
    if (it == labels_.end()) return false;

    it->second.offset = targetOffset;
    it->second.resolved = true;
    return true;
}

uint32_t CodeEmitter::getLabelOffset(const std::string& name) const {
    auto it = labels_.find(name);
    return (it != labels_.end() && it->second.resolved) ? it->second.offset : 0;
}

} // namespace pewriter