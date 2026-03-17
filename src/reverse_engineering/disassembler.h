#pragma once
// ============================================================================
// RawrXD Disassembler - x86/x64 Instruction Decoder
// Full length decoding + CFG analysis + Pattern recognition
// Pure Win32, No External Dependencies
// ============================================================================

#ifndef RAWRXD_DISASSEMBLER_H
#define RAWRXD_DISASSEMBLER_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// Instruction Structures
// ============================================================================

enum class InstructionType : uint8_t {
    Unknown = 0,
    // Arithmetic
    Add, Sub, Mul, Div, Imul, Idiv, Neg, Inc, Dec,
    // Logical
    And, Or, Xor, Not, Test, Cmp,
    // Shift/Rotate
    Shl, Shr, Sar, Sal, Rol, Ror, Rcl, Rcr,
    // Data Transfer
    Mov, Movzx, Movsx, Movsxd, Lea, Xchg, Cmov,
    // Stack
    Push, Pop, Enter, Leave,
    // Control Flow
    Call, Ret, Jmp, Jcc, Loop, Int, Syscall, Sysenter,
    // String
    Movs, Cmps, Scas, Lods, Stos, Rep,
    // Misc
    Nop, Hlt, Cpuid, Rdtsc, Xgetbv, Ud2, Int3,
    // FPU
    Fld, Fst, Fadd, Fsub, Fmul, Fdiv,
    // SSE/AVX
    Movaps, Movups, Movdqa, Movdqu, Movss, Movsd,
    Addps, Addpd, Addss, Addsd,
    Subps, Subpd, Subss, Subsd,
    Mulps, Mulpd, Mulss, Mulsd,
    Divps, Divpd, Divss, Divsd,
    Xorps, Xorpd, Andps, Andpd, Orps, Orpd,
    Cmpps, Cmppd, Cmpss, Cmpsd,
    // Memory
    Prefetch, Clflush, Mfence, Lfence, Sfence,
    // Set
    Setcc,
    // Bit manipulation
    Bt, Bts, Btr, Btc, Bsf, Bsr, Popcnt, Lzcnt, Tzcnt
};

enum class OperandType : uint8_t {
    None = 0,
    Register,       // General purpose register
    Immediate,      // Immediate value
    Memory,         // Memory operand [base + index*scale + disp]
    RelOffset,      // Relative offset (for branches)
    FPRegister,     // FPU stack register
    MMXRegister,    // MMX register
    XMMRegister,    // XMM/YMM/ZMM register
    Segment,        // Segment register
    Control,        // Control register
    Debug           // Debug register
};

enum class RegisterId : uint8_t {
    // 64-bit
    RAX = 0, RCX, RDX, RBX, RSP, RBP, RSI, RDI,
    R8, R9, R10, R11, R12, R13, R14, R15,
    // Segment
    ES = 32, CS, SS, DS, FS, GS,
    // FPU
    ST0 = 40, ST1, ST2, ST3, ST4, ST5, ST6, ST7,
    // MMX
    MM0 = 48, MM1, MM2, MM3, MM4, MM5, MM6, MM7,
    // XMM
    XMM0 = 64, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
    XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15,
    // Special
    RIP = 128, RFLAGS, NONE = 255
};

struct Operand {
    OperandType     type;
    uint8_t         size;           // Operand size in bytes (1, 2, 4, 8, 16, 32, 64)
    RegisterId      reg;            // Register for reg operands
    RegisterId      base;           // Base register for memory
    RegisterId      index;          // Index register for memory
    uint8_t         scale;          // Scale factor (1, 2, 4, 8)
    int64_t         displacement;   // Displacement or immediate
    bool            isRipRelative;  // RIP-relative addressing
    
    Operand() : type(OperandType::None), size(0), reg(RegisterId::NONE),
                base(RegisterId::NONE), index(RegisterId::NONE), scale(1),
                displacement(0), isRipRelative(false) {}
};

struct Instruction {
    uint64_t            address;        // Virtual address
    uint32_t            length;         // Instruction length in bytes
    std::vector<uint8_t> bytes;         // Raw instruction bytes
    
    InstructionType     type;           // Instruction classification
    std::string         mnemonic;       // Mnemonic string
    std::string         operandsStr;    // Formatted operands
    std::string         comment;        // Auto-generated comment
    
    Operand             operands[4];    // Up to 4 operands
    uint8_t             operandCount;   // Number of operands
    
    // Prefixes
    bool                hasLock;
    bool                hasRep;
    bool                hasRepne;
    bool                has66;          // Operand size override
    bool                has67;          // Address size override
    uint8_t             segmentOverride; // 0=none, ES=1, CS=2, SS=3, DS=4, FS=5, GS=6
    
    // REX/VEX
    bool                hasRex;
    bool                rexW;
    bool                rexR;
    bool                rexX;
    bool                rexB;
    bool                hasVex;
    uint8_t             vexL;           // Vector length (0=128, 1=256, 2=512)
    
    // Control flow info
    bool                isControlFlow;
    bool                isBranch;
    bool                isCall;
    bool                isReturn;
    bool                isConditional;
    uint64_t            branchTarget;   // Target address for branches
    
    // Flags
    bool                isPrivileged;
    bool                modifiesFlags;
    bool                readsFlags;
    
    Instruction() : address(0), length(0), type(InstructionType::Unknown),
                    operandCount(0), hasLock(false), hasRep(false),
                    hasRepne(false), has66(false), has67(false),
                    segmentOverride(0), hasRex(false), rexW(false),
                    rexR(false), rexX(false), rexB(false), hasVex(false),
                    vexL(0), isControlFlow(false), isBranch(false),
                    isCall(false), isReturn(false), isConditional(false),
                    branchTarget(0), isPrivileged(false), modifiesFlags(false),
                    readsFlags(false) {}
};

// ============================================================================
// Basic Block Structure
// ============================================================================

struct BasicBlock {
    uint64_t                startAddress;
    uint64_t                endAddress;         // Address of last instruction
    std::vector<Instruction> instructions;
    
    std::vector<uint64_t>   successors;         // Successor block addresses
    std::vector<uint64_t>   predecessors;       // Predecessor block addresses
    
    bool                    isEntry;            // Function entry block
    bool                    isExit;             // Contains RET
    bool                    isLoopHeader;       // Dominates a back-edge
    
    // Liveness info
    std::unordered_set<RegisterId> liveIn;
    std::unordered_set<RegisterId> liveOut;
    std::unordered_set<RegisterId> defs;
    std::unordered_set<RegisterId> uses;
    
    BasicBlock() : startAddress(0), endAddress(0), isEntry(false),
                   isExit(false), isLoopHeader(false) {}
};

// ============================================================================
// Function Structure
// ============================================================================

struct Function {
    uint64_t                startAddress;
    uint64_t                endAddress;
    std::string             name;
    
    std::vector<BasicBlock> blocks;
    
    // Stack frame info
    uint32_t                stackFrameSize;
    bool                    usesFramePointer;
    
    // Calling convention hint
    std::string             callingConvention;  // "cdecl", "stdcall", "fastcall", "ms_x64", "sysv_x64"
    
    // References
    std::vector<uint64_t>   callsTo;            // Functions this calls
    std::vector<uint64_t>   calledFrom;         // Functions that call this
    
    Function() : startAddress(0), endAddress(0), stackFrameSize(0),
                 usesFramePointer(false) {}
};

// ============================================================================
// Disassembler Class
// ============================================================================

class Disassembler {
public:
    enum class Architecture { X86, X64 };

    Disassembler();
    ~Disassembler() = default;

    // ========================================================================
    // Configuration
    // ========================================================================
    void SetArchitecture(Architecture arch);
    Architecture GetArchitecture() const { return m_architecture; }
    bool Is64Bit() const { return m_architecture == Architecture::X64; }

    // ========================================================================
    // Basic Disassembly
    // ========================================================================
    std::vector<Instruction> Disassemble(const uint8_t* code, size_t size, uint64_t baseAddress);
    Instruction DisassembleOne(const uint8_t* code, size_t maxSize, uint64_t address);
    
    // ========================================================================
    // Formatting
    // ========================================================================
    std::string FormatInstruction(const Instruction& inst);
    std::string FormatOperand(const Operand& op, uint64_t instrAddr, uint32_t instrLen);
    std::string GetRegisterName(RegisterId reg, uint8_t size);
    
    // ========================================================================
    // Control Flow Analysis
    // ========================================================================
    std::vector<BasicBlock> BuildCFG(const uint8_t* code, size_t size, uint64_t entry);
    Function AnalyzeFunction(const uint8_t* code, size_t size, uint64_t entry);
    
    // ========================================================================
    // Pattern Recognition
    // ========================================================================
    std::vector<uint64_t> FindFunctions(const uint8_t* code, size_t size, uint64_t baseAddress);
    std::vector<uint64_t> FindCallTargets(const uint8_t* code, size_t size, uint64_t baseAddress);
    std::vector<uint64_t> FindStrings(const uint8_t* code, size_t size, uint64_t baseAddress);
    
    // Prologue/epilogue detection
    bool IsFunctionPrologue(const uint8_t* code, size_t size);
    bool IsFunctionEpilogue(const uint8_t* code, size_t size);
    
    // ========================================================================
    // Instruction Length
    // ========================================================================
    uint32_t GetInstructionLength(const uint8_t* code, size_t maxSize);

private:
    // Internal decoding
    void DecodeInstruction(const uint8_t* code, size_t size, Instruction& inst);
    void DecodePrefixes(const uint8_t*& p, const uint8_t* end, Instruction& inst);
    void DecodeOpcode(const uint8_t*& p, const uint8_t* end, Instruction& inst);
    void DecodeModRM(const uint8_t*& p, const uint8_t* end, Instruction& inst, uint8_t modrm);
    void DecodeSIB(const uint8_t*& p, const uint8_t* end, Operand& op, uint8_t mod, bool rexX, bool rexB);
    void DecodeImmediate(const uint8_t*& p, const uint8_t* end, Operand& op, uint8_t size);
    void DecodeDisplacement(const uint8_t*& p, const uint8_t* end, Operand& op, uint8_t size);
    
    // Opcode handlers
    void DecodeOneByteOpcode(const uint8_t*& p, const uint8_t* end, uint8_t opcode, Instruction& inst);
    void DecodeTwoByteOpcode(const uint8_t*& p, const uint8_t* end, uint8_t opcode, Instruction& inst);
    void DecodeThreeByteOpcode38(const uint8_t*& p, const uint8_t* end, uint8_t opcode, Instruction& inst);
    void DecodeThreeByteOpcode3A(const uint8_t*& p, const uint8_t* end, uint8_t opcode, Instruction& inst);
    void DecodeGroupOpcode(const uint8_t*& p, const uint8_t* end, uint8_t group, uint8_t modrm, Instruction& inst);
    
    // Helper methods
    void FormatMnemonic(Instruction& inst);
    void FormatOperands(Instruction& inst);
    RegisterId GetRegister(uint8_t code, bool rex, uint8_t size);
    
    // CFG building
    void FindLeaders(const std::vector<Instruction>& instructions,
                     std::unordered_set<uint64_t>& leaders);
    void BuildBlocks(const std::vector<Instruction>& instructions,
                     const std::unordered_set<uint64_t>& leaders,
                     std::vector<BasicBlock>& blocks);
    void ConnectBlocks(std::vector<BasicBlock>& blocks);
    
    Architecture m_architecture;
    bool m_is64Bit;
};

} // namespace ReverseEngineering
} // namespace RawrXD

#endif // RAWRXD_DISASSEMBLER_H
