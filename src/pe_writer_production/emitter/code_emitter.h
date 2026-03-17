// ============================================================================
// Code Emitter - Advanced Machine Code Generation
// Supports x64, x86, and ARM64 instruction emission with relocations
// ============================================================================

#pragma once

#include "../pe_writer.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace pewriter {

// ============================================================================
// INSTRUCTION ENCODING STRUCTURES
// ============================================================================

struct Instruction {
    std::vector<uint8_t> bytes;
    std::vector<RelocationEntry> relocations;
    uint32_t size;
};

struct Label {
    std::string name;
    uint32_t offset;
    bool resolved;
};

// ============================================================================
// CODE EMITTER CLASS
// ============================================================================

class CodeEmitter {
public:
    CodeEmitter();
    ~CodeEmitter() = default;

    void setArchitecture(PEArchitecture arch);
    bool emitSection(const CodeSection& section);

    // x64 Instruction Emission
    bool emitMOV_R64_IMM64(uint8_t reg, uint64_t imm);
    bool emitCALL_REL32(uint32_t targetRVA);
    bool emitRET();
    bool emitPUSH_R64(uint8_t reg);
    bool emitPOP_R64(uint8_t reg);
    bool emitSUB_RSP_IMM8(uint8_t imm);
    bool emitADD_RSP_IMM8(uint8_t imm);
    bool emitMOV_RCX_IMM32(uint32_t imm);
    bool emitINT3();

    // Function prologue/epilogue
    bool emitFunctionPrologue();
    bool emitFunctionEpilogue();

    // Labels and jumps
    bool createLabel(const std::string& name);
    bool emitJMP_LABEL(const std::string& label);
    bool emitJE_LABEL(const std::string& label);
    bool emitJNE_LABEL(const std::string& label);

    // Data emission
    bool emitDB(uint8_t byte);
    bool emitDW(uint16_t word);
    bool emitDD(uint32_t dword);
    bool emitDQ(uint64_t qword);

    // Get emitted code
    const std::vector<uint8_t>& getCode() const;
    const std::vector<RelocationEntry>& getRelocations() const;

    // Reset emitter
    void reset();

private:
    // Architecture-specific encoding
    bool encodeREX(bool w, bool r, bool x, bool b);
    bool encodeModRM(uint8_t mod, uint8_t reg, uint8_t rm);
    bool encodeSIB(uint8_t scale, uint8_t index, uint8_t base);

    // Utility functions
    void addByte(uint8_t byte);
    void addWord(uint16_t word);
    void addDword(uint32_t dword);
    void addQword(uint64_t qword);
    void addRelocation(const RelocationEntry& reloc);

    // Label management
    bool resolveLabel(const std::string& name, uint32_t targetOffset);
    uint32_t getLabelOffset(const std::string& name) const;

    // Member variables
    PEArchitecture architecture_;
    std::vector<uint8_t> code_;
    std::vector<RelocationEntry> relocations_;
    std::unordered_map<std::string, Label> labels_;
    uint32_t currentOffset_;

    // x64 register encoding
    static constexpr uint8_t RAX = 0;
    static constexpr uint8_t RCX = 1;
    static constexpr uint8_t RDX = 2;
    static constexpr uint8_t RBX = 3;
    static constexpr uint8_t RSP = 4;
    static constexpr uint8_t RBP = 5;
    static constexpr uint8_t RSI = 6;
    static constexpr uint8_t RDI = 7;
    static constexpr uint8_t R8 = 8;
    static constexpr uint8_t R9 = 9;
    static constexpr uint8_t R10 = 10;
    static constexpr uint8_t R11 = 11;
    static constexpr uint8_t R12 = 12;
    static constexpr uint8_t R13 = 13;
    static constexpr uint8_t R14 = 14;
    static constexpr uint8_t R15 = 15;

    // Instruction opcodes
    static constexpr uint8_t OPCODE_MOV_R64_IMM64 = 0xB8;
    static constexpr uint8_t OPCODE_CALL_REL32 = 0xE8;
    static constexpr uint8_t OPCODE_RET = 0xC3;
    static constexpr uint8_t OPCODE_PUSH_R64 = 0x50;
    static constexpr uint8_t OPCODE_POP_R64 = 0x58;
    static constexpr uint8_t OPCODE_SUB_RSP_IMM8 = 0x83;
    static constexpr uint8_t OPCODE_ADD_RSP_IMM8 = 0x83;
    static constexpr uint8_t OPCODE_INT3 = 0xCC;
    static constexpr uint8_t OPCODE_JMP_REL32 = 0xE9;
    static constexpr uint8_t OPCODE_JE_REL32 = 0x84;
    static constexpr uint8_t OPCODE_JNE_REL32 = 0x85;
};

} // namespace pewriter