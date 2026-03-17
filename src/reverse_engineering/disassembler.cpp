// ============================================================================
// RawrXD Disassembler - x86/x64 Instruction Decoder Implementation
// Full length decoding + CFG analysis + Pattern recognition
// Pure Win32, No External Dependencies
// ============================================================================

#include "disassembler.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// Opcode Tables
// ============================================================================

// ModR/M required for opcode (1=yes, 0=no, -1=special)
static const int8_t g_modrm1Byte[256] = {
    // 0x00-0x0F: ADD, OR
    1, 1, 1, 1, 0, 0, 0, 0,  1, 1, 1, 1, 0, 0, 0, 0,
    // 0x10-0x1F: ADC, SBB
    1, 1, 1, 1, 0, 0, 0, 0,  1, 1, 1, 1, 0, 0, 0, 0,
    // 0x20-0x2F: AND, SUB
    1, 1, 1, 1, 0, 0, 0, 0,  1, 1, 1, 1, 0, 0, 0, 0,
    // 0x30-0x3F: XOR, CMP
    1, 1, 1, 1, 0, 0, 0, 0,  1, 1, 1, 1, 0, 0, 0, 0,
    // 0x40-0x4F: INC/DEC or REX
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0x50-0x5F: PUSH/POP
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0x60-0x6F
    0, 0, 1, 1, 0, 0, 0, 0,  0, 1, 0, 1, 0, 0, 0, 0,
    // 0x70-0x7F: Jcc short
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0x80-0x8F: Group 1, MOV, LEA, etc
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
    // 0x90-0x9F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0xA0-0xAF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0xB0-0xBF: MOV imm
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0xC0-0xCF: Shifts, RET, MOV, etc
    1, 1, 0, 0, 1, 1, 1, 1,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0xD0-0xDF: Shifts (without imm), FPU
    1, 1, 1, 1, 0, 0, 0, 0,  1, 1, 1, 1, 1, 1, 1, 1,
    // 0xE0-0xEF: LOOP, CALL, JMP
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0xF0-0xFF: LOCK, HLT, Group 3/4/5
    0, 0, 0, 0, 0, 0, 1, 1,  0, 0, 0, 0, 0, 0, 1, 1
};

// Immediate size for opcodes (0=none, 1=imm8, 2=imm16, 4=imm32, 8=imm64, -1=depends)
static const int8_t g_imm1Byte[256] = {
    // 0x00-0x0F
    0, 0, 0, 0, 1, 4, 0, 0,  0, 0, 0, 0, 1, 4, 0, 0,
    // 0x10-0x1F
    0, 0, 0, 0, 1, 4, 0, 0,  0, 0, 0, 0, 1, 4, 0, 0,
    // 0x20-0x2F
    0, 0, 0, 0, 1, 4, 0, 0,  0, 0, 0, 0, 1, 4, 0, 0,
    // 0x30-0x3F
    0, 0, 0, 0, 1, 4, 0, 0,  0, 0, 0, 0, 1, 4, 0, 0,
    // 0x40-0x4F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0x50-0x5F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0x60-0x6F
    0, 0, 0, 0, 0, 0, 0, 0,  4, 4, 1, 1, 0, 0, 0, 0,
    // 0x70-0x7F: Jcc short (imm8)
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
    // 0x80-0x8F
    1, 4, 1, 1, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0x90-0x9F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 6, 0, 0, 0, 0, 0,
    // 0xA0-0xAF: MOV moffs
    4, 4, 4, 4, 0, 0, 0, 0,  1, 4, 0, 0, 0, 0, 0, 0,
    // 0xB0-0xBF: MOV imm
    1, 1, 1, 1, 1, 1, 1, 1, -1,-1,-1,-1,-1,-1,-1,-1,
    // 0xC0-0xCF
    1, 1, 2, 0, 0, 0, 1, 4,  4, 0, 2, 0, 0, 1, 0, 0,
    // 0xD0-0xDF
    0, 0, 0, 0, 1, 1, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    // 0xE0-0xEF
    1, 1, 1, 1, 1, 1, 1, 1,  4, 4, 6, 1, 0, 0, 0, 0,
    // 0xF0-0xFF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0
};

// Instruction mnemonics
static const char* g_mnemonicTable[] = {
    "unknown", "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp",
    "push", "pop", "mov", "lea", "call", "jmp", "ret", "nop",
    "test", "xchg", "inc", "dec", "mul", "div", "imul", "idiv",
    "shl", "shr", "sar", "sal", "rol", "ror", "rcl", "rcr", "not", "neg",
    "int", "syscall", "cpuid", "rdtsc", "hlt", "leave", "enter",
    "movzx", "movsx", "movsxd", "cmov", "set", "loop",
    "movs", "cmps", "scas", "lods", "stos", "rep", "repne",
    "jo", "jno", "jb", "jae", "jz", "jnz", "jbe", "ja",
    "js", "jns", "jp", "jnp", "jl", "jge", "jle", "jg",
    "bt", "bts", "btr", "btc", "bsf", "bsr", "popcnt", "lzcnt", "tzcnt",
    "int3", "ud2", "mfence", "lfence", "sfence", "clflush", "prefetch",
    "xgetbv", "xsetbv", "rdpmc", "rdmsr", "wrmsr", "cpuid", "xsave", "xrstor"
};

static const char* g_jccMnemonics[] = {
    "jo", "jno", "jb", "jae", "jz", "jnz", "jbe", "ja",
    "js", "jns", "jp", "jnp", "jl", "jge", "jle", "jg"
};

static const char* g_setccMnemonics[] = {
    "seto", "setno", "setb", "setae", "setz", "setnz", "setbe", "seta",
    "sets", "setns", "setp", "setnp", "setl", "setge", "setle", "setg"
};

static const char* g_cmovccMnemonics[] = {
    "cmovo", "cmovno", "cmovb", "cmovae", "cmovz", "cmovnz", "cmovbe", "cmova",
    "cmovs", "cmovns", "cmovp", "cmovnp", "cmovl", "cmovge", "cmovle", "cmovg"
};

// Register names
static const char* g_reg64Names[] = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                                       "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
static const char* g_reg32Names[] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
                                       "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" };
static const char* g_reg16Names[] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
                                       "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w" };
static const char* g_reg8Names[] = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
                                      "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" };
static const char* g_reg8NamesRex[] = { "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
                                         "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" };
static const char* g_segNames[] = { "es", "cs", "ss", "ds", "fs", "gs" };
static const char* g_xmmNames[] = { "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
                                     "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" };

// ============================================================================
// Constructor
// ============================================================================

Disassembler::Disassembler()
    : m_architecture(Architecture::X64)
    , m_is64Bit(true)
{
}

void Disassembler::SetArchitecture(Architecture arch)
{
    m_architecture = arch;
    m_is64Bit = (arch == Architecture::X64);
}

// ============================================================================
// Instruction Length Decoding
// ============================================================================

uint32_t Disassembler::GetInstructionLength(const uint8_t* code, size_t maxSize)
{
    if (maxSize == 0) return 0;

    const uint8_t* p = code;
    const uint8_t* end = code + maxSize;
    
    bool hasRex = false;
    bool rexW = false;
    bool has66 = false;
    bool has67 = false;

    // Parse prefixes
    while (p < end) {
        uint8_t b = *p;
        
        // Legacy prefixes
        if (b == 0x66) { has66 = true; ++p; continue; }
        if (b == 0x67) { has67 = true; ++p; continue; }
        if (b == 0xF0 || b == 0xF2 || b == 0xF3 ||
            b == 0x2E || b == 0x3E || b == 0x26 || b == 0x36 ||
            b == 0x64 || b == 0x65) {
            ++p; continue;
        }
        
        // REX prefix (x64 only)
        if (m_is64Bit && b >= 0x40 && b <= 0x4F) {
            hasRex = true;
            rexW = (b & 0x08) != 0;
            ++p;
            continue;
        }
        
        break;
    }

    if (p >= end) return static_cast<uint32_t>(p - code);

    uint32_t prefixLen = static_cast<uint32_t>(p - code);
    uint8_t opcode = *p++;

    // VEX prefix
    if ((opcode == 0xC4 || opcode == 0xC5) && p < end) {
        uint32_t vexLen = (opcode == 0xC5) ? 2 : 3;
        if (p + vexLen - 1 > end) return prefixLen + 1;
        
        p += vexLen - 1;
        if (p >= end) return prefixLen + vexLen;
        
        // VEX opcode
        ++p;
        if (p >= end) return prefixLen + vexLen + 1;
        
        // ModR/M
        uint8_t modrm = *p++;
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t rm = modrm & 7;
        
        uint32_t len = prefixLen + vexLen + 2;
        
        // SIB
        if (mod != 3 && rm == 4) {
            if (p < end) { ++p; ++len; }
        }
        
        // Displacement
        if (mod == 0 && rm == 5) len += 4;
        else if (mod == 1) len += 1;
        else if (mod == 2) len += 4;
        
        // Most VEX instructions have imm8
        ++len;
        
        return len;
    }

    // EVEX prefix
    if (opcode == 0x62 && p + 3 <= end) {
        p += 3; // Skip EVEX bytes
        if (p >= end) return static_cast<uint32_t>(p - code);
        
        ++p; // Opcode
        if (p >= end) return static_cast<uint32_t>(p - code);
        
        uint8_t modrm = *p++;
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t rm = modrm & 7;
        
        uint32_t len = static_cast<uint32_t>(p - code);
        
        if (mod != 3 && rm == 4) {
            if (p < end) { ++p; ++len; }
        }
        
        if (mod == 0 && rm == 5) len += 4;
        else if (mod == 1) len += 1;
        else if (mod == 2) len += 4;
        
        ++len; // imm8
        return len;
    }

    // 2-byte escape
    if (opcode == 0x0F) {
        if (p >= end) return prefixLen + 1;
        uint8_t opcode2 = *p++;
        
        // 3-byte escape (0F 38 / 0F 3A)
        if (opcode2 == 0x38 || opcode2 == 0x3A) {
            if (p >= end) return prefixLen + 2;
            ++p; // Third opcode byte
            if (p >= end) return prefixLen + 3;
            
            uint8_t modrm = *p++;
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm = modrm & 7;
            
            uint32_t len = prefixLen + 4;
            
            if (mod != 3 && rm == 4) {
                if (p < end) { ++p; ++len; }
            }
            
            if (mod == 0 && rm == 5) len += 4;
            else if (mod == 1) len += 1;
            else if (mod == 2) len += 4;
            
            // 0F 3A has imm8
            if (opcode2 == 0x3A) len += 1;
            
            return len;
        }
        
        // Standard 2-byte opcodes
        // Jcc long
        if (opcode2 >= 0x80 && opcode2 <= 0x8F) {
            return prefixLen + 2 + 4; // opcode + rel32
        }
        
        // SETcc, CMOVcc - have ModR/M
        if ((opcode2 >= 0x90 && opcode2 <= 0x9F) ||
            (opcode2 >= 0x40 && opcode2 <= 0x4F)) {
            if (p >= end) return prefixLen + 2;
            uint8_t modrm = *p++;
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm = modrm & 7;
            
            uint32_t len = prefixLen + 3;
            
            if (mod != 3 && rm == 4) {
                if (p < end) { ++p; ++len; }
            }
            
            if (mod == 0 && rm == 5) len += 4;
            else if (mod == 1) len += 1;
            else if (mod == 2) len += 4;
            
            return len;
        }
        
        // SYSCALL, SYSRET, CPUID, etc. (no operands)
        if (opcode2 == 0x05 || opcode2 == 0x07 || opcode2 == 0xA2 || 
            opcode2 == 0x31 || opcode2 == 0x0B) {
            return prefixLen + 2;
        }
        
        // MOVZX, MOVSX (ModR/M)
        if (opcode2 == 0xB6 || opcode2 == 0xB7 || opcode2 == 0xBE || opcode2 == 0xBF) {
            if (p >= end) return prefixLen + 2;
            uint8_t modrm = *p++;
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm = modrm & 7;
            
            uint32_t len = prefixLen + 3;
            
            if (mod != 3 && rm == 4) {
                if (p < end) { ++p; ++len; }
            }
            
            if (mod == 0 && rm == 5) len += 4;
            else if (mod == 1) len += 1;
            else if (mod == 2) len += 4;
            
            return len;
        }
        
        // Default 2-byte with ModR/M
        if (p >= end) return prefixLen + 2;
        uint8_t modrm = *p++;
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t rm = modrm & 7;
        
        uint32_t len = prefixLen + 3;
        
        if (mod != 3 && rm == 4) {
            if (p < end) { ++p; ++len; }
        }
        
        if (mod == 0 && rm == 5) len += 4;
        else if (mod == 1) len += 1;
        else if (mod == 2) len += 4;
        
        return len;
    }

    // Single-byte opcode
    int8_t needsModRM = g_modrm1Byte[opcode];
    int8_t immSize = g_imm1Byte[opcode];
    
    uint32_t len = prefixLen + 1;
    
    // Handle ModR/M
    if (needsModRM && p < end) {
        uint8_t modrm = *p++;
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t rm = modrm & 7;
        uint8_t reg = (modrm >> 3) & 7;
        
        ++len;
        
        // SIB byte
        if (mod != 3 && rm == 4) {
            if (p < end) { ++p; ++len; }
        }
        
        // Displacement
        if (mod == 0 && rm == 5) {
            len += has67 ? 2 : 4;
            p += has67 ? 2 : 4;
        } else if (mod == 1) {
            len += 1;
            ++p;
        } else if (mod == 2) {
            len += has67 ? 2 : 4;
            p += has67 ? 2 : 4;
        }
        
        // Group instruction immediates
        if (opcode == 0x80 || opcode == 0x82) {
            len += 1; // imm8
        } else if (opcode == 0x81) {
            len += has66 ? 2 : 4;
        } else if (opcode == 0x83) {
            len += 1; // sign-extended imm8
        } else if (opcode == 0xC0 || opcode == 0xC1) {
            len += 1; // shift imm8
        } else if (opcode == 0xC6) {
            len += 1; // MOV imm8
        } else if (opcode == 0xC7) {
            len += has66 ? 2 : 4;
        } else if (opcode == 0xF6) {
            if (reg == 0 || reg == 1) len += 1; // TEST imm8
        } else if (opcode == 0xF7) {
            if (reg == 0 || reg == 1) len += has66 ? 2 : 4; // TEST imm
        }
        
        return len;
    }
    
    // Handle immediate
    if (immSize > 0) {
        len += immSize;
    } else if (immSize == -1) {
        // Variable immediate (MOV reg, imm)
        if (opcode >= 0xB8 && opcode <= 0xBF) {
            if (rexW) {
                len += 8; // imm64
            } else if (has66) {
                len += 2; // imm16
            } else {
                len += 4; // imm32
            }
        }
    }
    
    return len;
}

// ============================================================================
// Basic Disassembly
// ============================================================================

Instruction Disassembler::DisassembleOne(const uint8_t* code, size_t maxSize, uint64_t address)
{
    Instruction inst;
    inst.address = address;
    
    if (maxSize == 0) {
        inst.length = 1;
        inst.mnemonic = "db";
        inst.operandsStr = "0x00";
        inst.bytes.push_back(0);
        return inst;
    }
    
    DecodeInstruction(code, maxSize, inst);
    return inst;
}

std::vector<Instruction> Disassembler::Disassemble(const uint8_t* code, size_t size, uint64_t baseAddress)
{
    std::vector<Instruction> result;
    
    size_t offset = 0;
    uint64_t addr = baseAddress;
    
    while (offset < size) {
        Instruction inst = DisassembleOne(code + offset, size - offset, addr);
        result.push_back(inst);
        
        offset += inst.length;
        addr += inst.length;
    }
    
    return result;
}

// ============================================================================
// Instruction Decoding
// ============================================================================

void Disassembler::DecodeInstruction(const uint8_t* code, size_t size, Instruction& inst)
{
    inst.length = GetInstructionLength(code, size);
    if (inst.length == 0 || inst.length > size) {
        inst.length = 1;
        inst.mnemonic = "db";
        char buf[8];
        snprintf(buf, sizeof(buf), "0x%02X", code[0]);
        inst.operandsStr = buf;
        inst.bytes.assign(code, code + 1);
        return;
    }
    
    inst.bytes.assign(code, code + inst.length);
    
    const uint8_t* p = code;
    const uint8_t* end = code + inst.length;
    
    // Decode prefixes
    DecodePrefixes(p, end, inst);
    
    if (p >= end) {
        inst.mnemonic = "???";
        return;
    }
    
    // Decode opcode and operands
    DecodeOpcode(p, end, inst);
    
    // Format output
    FormatMnemonic(inst);
    FormatOperands(inst);
}

void Disassembler::DecodePrefixes(const uint8_t*& p, const uint8_t* end, Instruction& inst)
{
    while (p < end) {
        uint8_t b = *p;
        
        switch (b) {
            case 0xF0: inst.hasLock = true; ++p; continue;
            case 0xF2: inst.hasRepne = true; ++p; continue;
            case 0xF3: inst.hasRep = true; ++p; continue;
            case 0x66: inst.has66 = true; ++p; continue;
            case 0x67: inst.has67 = true; ++p; continue;
            case 0x2E: inst.segmentOverride = 2; ++p; continue; // CS
            case 0x3E: inst.segmentOverride = 4; ++p; continue; // DS
            case 0x26: inst.segmentOverride = 1; ++p; continue; // ES
            case 0x36: inst.segmentOverride = 3; ++p; continue; // SS
            case 0x64: inst.segmentOverride = 5; ++p; continue; // FS
            case 0x65: inst.segmentOverride = 6; ++p; continue; // GS
        }
        
        // REX prefix (x64 only)
        if (m_is64Bit && b >= 0x40 && b <= 0x4F) {
            inst.hasRex = true;
            inst.rexW = (b & 0x08) != 0;
            inst.rexR = (b & 0x04) != 0;
            inst.rexX = (b & 0x02) != 0;
            inst.rexB = (b & 0x01) != 0;
            ++p;
            continue;
        }
        
        break;
    }
}

void Disassembler::DecodeOpcode(const uint8_t*& p, const uint8_t* end, Instruction& inst)
{
    if (p >= end) return;
    
    uint8_t opcode = *p++;
    
    // VEX prefix
    if (opcode == 0xC4 || opcode == 0xC5) {
        inst.hasVex = true;
        // Simplified VEX handling
        inst.mnemonic = "v???";
        return;
    }
    
    // 2-byte escape
    if (opcode == 0x0F) {
        if (p >= end) {
            inst.mnemonic = "???";
            return;
        }
        DecodeTwoByteOpcode(p, end, *p++, inst);
        return;
    }
    
    // Single-byte opcode
    DecodeOneByteOpcode(p, end, opcode, inst);
}

void Disassembler::DecodeOneByteOpcode(const uint8_t*& p, const uint8_t* end, uint8_t opcode, Instruction& inst)
{
    // PUSH reg
    if (opcode >= 0x50 && opcode <= 0x57) {
        inst.type = InstructionType::Push;
        inst.mnemonic = "push";
        inst.operandCount = 1;
        inst.operands[0].type = OperandType::Register;
        inst.operands[0].size = m_is64Bit ? 8 : 4;
        inst.operands[0].reg = GetRegister((opcode - 0x50) | (inst.rexB ? 8 : 0), inst.hasRex, inst.operands[0].size);
        return;
    }
    
    // POP reg
    if (opcode >= 0x58 && opcode <= 0x5F) {
        inst.type = InstructionType::Pop;
        inst.mnemonic = "pop";
        inst.operandCount = 1;
        inst.operands[0].type = OperandType::Register;
        inst.operands[0].size = m_is64Bit ? 8 : 4;
        inst.operands[0].reg = GetRegister((opcode - 0x58) | (inst.rexB ? 8 : 0), inst.hasRex, inst.operands[0].size);
        return;
    }
    
    // MOV reg, imm8
    if (opcode >= 0xB0 && opcode <= 0xB7) {
        inst.type = InstructionType::Mov;
        inst.mnemonic = "mov";
        inst.operandCount = 2;
        inst.operands[0].type = OperandType::Register;
        inst.operands[0].size = 1;
        inst.operands[0].reg = GetRegister((opcode - 0xB0) | (inst.rexB ? 8 : 0), inst.hasRex, 1);
        inst.operands[1].type = OperandType::Immediate;
        inst.operands[1].size = 1;
        if (p < end) inst.operands[1].displacement = *p;
        return;
    }
    
    // MOV reg, imm
    if (opcode >= 0xB8 && opcode <= 0xBF) {
        inst.type = InstructionType::Mov;
        inst.mnemonic = "mov";
        inst.operandCount = 2;
        inst.operands[0].type = OperandType::Register;
        
        if (inst.rexW) {
            inst.operands[0].size = 8;
            inst.operands[1].size = 8;
            if (p + 8 <= end) {
                inst.operands[1].displacement = *reinterpret_cast<const int64_t*>(p);
            }
        } else if (inst.has66) {
            inst.operands[0].size = 2;
            inst.operands[1].size = 2;
            if (p + 2 <= end) {
                inst.operands[1].displacement = *reinterpret_cast<const int16_t*>(p);
            }
        } else {
            inst.operands[0].size = 4;
            inst.operands[1].size = 4;
            if (p + 4 <= end) {
                inst.operands[1].displacement = *reinterpret_cast<const int32_t*>(p);
            }
        }
        
        inst.operands[0].reg = GetRegister((opcode - 0xB8) | (inst.rexB ? 8 : 0), inst.hasRex, inst.operands[0].size);
        inst.operands[1].type = OperandType::Immediate;
        return;
    }
    
    // RET
    if (opcode == 0xC3) {
        inst.type = InstructionType::Ret;
        inst.mnemonic = "ret";
        inst.isControlFlow = true;
        inst.isReturn = true;
        return;
    }
    
    // RET imm16
    if (opcode == 0xC2) {
        inst.type = InstructionType::Ret;
        inst.mnemonic = "ret";
        inst.operandCount = 1;
        inst.operands[0].type = OperandType::Immediate;
        inst.operands[0].size = 2;
        if (p + 2 <= end) {
            inst.operands[0].displacement = *reinterpret_cast<const uint16_t*>(p);
        }
        inst.isControlFlow = true;
        inst.isReturn = true;
        return;
    }
    
    // CALL rel32
    if (opcode == 0xE8) {
        inst.type = InstructionType::Call;
        inst.mnemonic = "call";
        inst.operandCount = 1;
        inst.operands[0].type = OperandType::RelOffset;
        inst.operands[0].size = 4;
        if (p + 4 <= end) {
            int32_t rel = *reinterpret_cast<const int32_t*>(p);
            inst.branchTarget = inst.address + inst.length + rel;
            inst.operands[0].displacement = inst.branchTarget;
        }
        inst.isControlFlow = true;
        inst.isCall = true;
        return;
    }
    
    // JMP rel32
    if (opcode == 0xE9) {
        inst.type = InstructionType::Jmp;
        inst.mnemonic = "jmp";
        inst.operandCount = 1;
        inst.operands[0].type = OperandType::RelOffset;
        inst.operands[0].size = 4;
        if (p + 4 <= end) {
            int32_t rel = *reinterpret_cast<const int32_t*>(p);
            inst.branchTarget = inst.address + inst.length + rel;
            inst.operands[0].displacement = inst.branchTarget;
        }
        inst.isControlFlow = true;
        inst.isBranch = true;
        return;
    }
    
    // JMP rel8
    if (opcode == 0xEB) {
        inst.type = InstructionType::Jmp;
        inst.mnemonic = "jmp";
        inst.operandCount = 1;
        inst.operands[0].type = OperandType::RelOffset;
        inst.operands[0].size = 1;
        if (p < end) {
            int8_t rel = *reinterpret_cast<const int8_t*>(p);
            inst.branchTarget = inst.address + inst.length + rel;
            inst.operands[0].displacement = inst.branchTarget;
        }
        inst.isControlFlow = true;
        inst.isBranch = true;
        return;
    }
    
    // Short Jcc (0x70-0x7F)
    if (opcode >= 0x70 && opcode <= 0x7F) {
        inst.type = InstructionType::Jcc;
        inst.mnemonic = g_jccMnemonics[opcode - 0x70];
        inst.operandCount = 1;
        inst.operands[0].type = OperandType::RelOffset;
        inst.operands[0].size = 1;
        if (p < end) {
            int8_t rel = *reinterpret_cast<const int8_t*>(p);
            inst.branchTarget = inst.address + inst.length + rel;
            inst.operands[0].displacement = inst.branchTarget;
        }
        inst.isControlFlow = true;
        inst.isBranch = true;
        inst.isConditional = true;
        inst.readsFlags = true;
        return;
    }
    
    // NOP
    if (opcode == 0x90 && !inst.hasRex) {
        inst.type = InstructionType::Nop;
        inst.mnemonic = "nop";
        return;
    }
    
    // XCHG rAX, reg
    if (opcode >= 0x91 && opcode <= 0x97) {
        inst.type = InstructionType::Xchg;
        inst.mnemonic = "xchg";
        inst.operandCount = 2;
        inst.operands[0].type = OperandType::Register;
        inst.operands[1].type = OperandType::Register;
        inst.operands[0].size = inst.rexW ? 8 : (inst.has66 ? 2 : 4);
        inst.operands[1].size = inst.operands[0].size;
        inst.operands[0].reg = GetRegister(0, inst.hasRex, inst.operands[0].size);
        inst.operands[1].reg = GetRegister((opcode - 0x90) | (inst.rexB ? 8 : 0), inst.hasRex, inst.operands[1].size);
        return;
    }
    
    // INT3
    if (opcode == 0xCC) {
        inst.type = InstructionType::Int3;
        inst.mnemonic = "int3";
        return;
    }
    
    // INT imm8
    if (opcode == 0xCD) {
        inst.type = InstructionType::Int;
        inst.mnemonic = "int";
        inst.operandCount = 1;
        inst.operands[0].type = OperandType::Immediate;
        inst.operands[0].size = 1;
        if (p < end) inst.operands[0].displacement = *p;
        return;
    }
    
    // HLT
    if (opcode == 0xF4) {
        inst.type = InstructionType::Hlt;
        inst.mnemonic = "hlt";
        inst.isPrivileged = true;
        return;
    }
    
    // LEAVE
    if (opcode == 0xC9) {
        inst.type = InstructionType::Leave;
        inst.mnemonic = "leave";
        return;
    }
    
    // LEA
    if (opcode == 0x8D && p < end) {
        inst.type = InstructionType::Lea;
        inst.mnemonic = "lea";
        uint8_t modrm = *p++;
        DecodeModRM(p, end, inst, modrm);
        return;
    }
    
    // Handle ModR/M opcodes
    if (g_modrm1Byte[opcode] && p < end) {
        uint8_t modrm = *p++;
        
        // Group 1: ADD/OR/ADC/SBB/AND/SUB/XOR/CMP with imm
        if (opcode >= 0x80 && opcode <= 0x83) {
            static const char* grp1[] = { "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp" };
            uint8_t op = (modrm >> 3) & 7;
            inst.mnemonic = grp1[op];
            DecodeModRM(p, end, inst, modrm);
            return;
        }
        
        // MOV r/m, r or MOV r, r/m
        if ((opcode >= 0x88 && opcode <= 0x8B) || opcode == 0x8E || opcode == 0x8C) {
            inst.type = InstructionType::Mov;
            inst.mnemonic = "mov";
            DecodeModRM(p, end, inst, modrm);
            return;
        }
        
        // TEST
        if (opcode == 0x84 || opcode == 0x85) {
            inst.type = InstructionType::Test;
            inst.mnemonic = "test";
            inst.modifiesFlags = true;
            DecodeModRM(p, end, inst, modrm);
            return;
        }
        
        // Group 3: TEST/NOT/NEG/MUL/IMUL/DIV/IDIV
        if (opcode == 0xF6 || opcode == 0xF7) {
            static const char* grp3[] = { "test", "test", "not", "neg", "mul", "imul", "div", "idiv" };
            uint8_t op = (modrm >> 3) & 7;
            inst.mnemonic = grp3[op];
            inst.modifiesFlags = true;
            DecodeModRM(p, end, inst, modrm);
            return;
        }
        
        // Group 4/5: INC/DEC/CALL/JMP/PUSH
        if (opcode == 0xFE || opcode == 0xFF) {
            static const char* grp5[] = { "inc", "dec", "call", "call far", "jmp", "jmp far", "push", "???" };
            uint8_t op = (modrm >> 3) & 7;
            inst.mnemonic = grp5[op];
            if (op == 2 || op == 3) {
                inst.isCall = true;
                inst.isControlFlow = true;
            } else if (op == 4 || op == 5) {
                inst.isBranch = true;
                inst.isControlFlow = true;
            }
            DecodeModRM(p, end, inst, modrm);
            return;
        }
    }
    
    // ALU operations (ADD/OR/ADC/SBB/AND/SUB/XOR/CMP)
    uint8_t aluOp = opcode >> 3;
    if ((opcode & 0xC0) == 0 && aluOp < 8) {
        static const char* aluNames[] = { "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp" };
        inst.mnemonic = aluNames[aluOp];
        inst.modifiesFlags = true;
        
        if (g_modrm1Byte[opcode] && p < end) {
            uint8_t modrm = *p++;
            DecodeModRM(p, end, inst, modrm);
        }
        return;
    }
    
    // Default fallback
    inst.mnemonic = "???";
    char buf[32];
    snprintf(buf, sizeof(buf), "(opcode: 0x%02X)", opcode);
    inst.comment = buf;
}

void Disassembler::DecodeTwoByteOpcode(const uint8_t*& p, const uint8_t* end, uint8_t opcode, Instruction& inst)
{
    // 3-byte escape
    if (opcode == 0x38 || opcode == 0x3A) {
        if (p < end) {
            if (opcode == 0x38) {
                DecodeThreeByteOpcode38(p, end, *p++, inst);
            } else {
                DecodeThreeByteOpcode3A(p, end, *p++, inst);
            }
        }
        return;
    }
    
    // Long Jcc
    if (opcode >= 0x80 && opcode <= 0x8F) {
        inst.type = InstructionType::Jcc;
        inst.mnemonic = g_jccMnemonics[opcode - 0x80];
        inst.operandCount = 1;
        inst.operands[0].type = OperandType::RelOffset;
        inst.operands[0].size = 4;
        if (p + 4 <= end) {
            int32_t rel = *reinterpret_cast<const int32_t*>(p);
            inst.branchTarget = inst.address + inst.length + rel;
            inst.operands[0].displacement = inst.branchTarget;
        }
        inst.isControlFlow = true;
        inst.isBranch = true;
        inst.isConditional = true;
        inst.readsFlags = true;
        return;
    }
    
    // SETcc
    if (opcode >= 0x90 && opcode <= 0x9F) {
        inst.type = InstructionType::Setcc;
        inst.mnemonic = g_setccMnemonics[opcode - 0x90];
        if (p < end) {
            uint8_t modrm = *p++;
            DecodeModRM(p, end, inst, modrm);
        }
        inst.readsFlags = true;
        return;
    }
    
    // CMOVcc
    if (opcode >= 0x40 && opcode <= 0x4F) {
        inst.type = InstructionType::Cmov;
        inst.mnemonic = g_cmovccMnemonics[opcode - 0x40];
        if (p < end) {
            uint8_t modrm = *p++;
            DecodeModRM(p, end, inst, modrm);
        }
        inst.isConditional = true;
        inst.readsFlags = true;
        return;
    }
    
    // MOVZX
    if (opcode == 0xB6 || opcode == 0xB7) {
        inst.type = InstructionType::Movzx;
        inst.mnemonic = "movzx";
        if (p < end) {
            uint8_t modrm = *p++;
            DecodeModRM(p, end, inst, modrm);
        }
        return;
    }
    
    // MOVSX
    if (opcode == 0xBE || opcode == 0xBF) {
        inst.type = InstructionType::Movsx;
        inst.mnemonic = "movsx";
        if (p < end) {
            uint8_t modrm = *p++;
            DecodeModRM(p, end, inst, modrm);
        }
        return;
    }
    
    // SYSCALL
    if (opcode == 0x05) {
        inst.type = InstructionType::Syscall;
        inst.mnemonic = "syscall";
        inst.isPrivileged = true;
        return;
    }
    
    // SYSRET
    if (opcode == 0x07) {
        inst.mnemonic = "sysret";
        inst.isPrivileged = true;
        inst.isReturn = true;
        inst.isControlFlow = true;
        return;
    }
    
    // CPUID
    if (opcode == 0xA2) {
        inst.type = InstructionType::Cpuid;
        inst.mnemonic = "cpuid";
        return;
    }
    
    // RDTSC
    if (opcode == 0x31) {
        inst.type = InstructionType::Rdtsc;
        inst.mnemonic = "rdtsc";
        return;
    }
    
    // UD2
    if (opcode == 0x0B) {
        inst.type = InstructionType::Ud2;
        inst.mnemonic = "ud2";
        return;
    }
    
    // PUSH/POP FS/GS
    if (opcode == 0xA0) { inst.mnemonic = "push"; inst.operandsStr = "fs"; return; }
    if (opcode == 0xA1) { inst.mnemonic = "pop"; inst.operandsStr = "fs"; return; }
    if (opcode == 0xA8) { inst.mnemonic = "push"; inst.operandsStr = "gs"; return; }
    if (opcode == 0xA9) { inst.mnemonic = "pop"; inst.operandsStr = "gs"; return; }
    
    // IMUL r, r/m
    if (opcode == 0xAF) {
        inst.type = InstructionType::Imul;
        inst.mnemonic = "imul";
        if (p < end) {
            uint8_t modrm = *p++;
            DecodeModRM(p, end, inst, modrm);
        }
        inst.modifiesFlags = true;
        return;
    }
    
    // BT/BTS/BTR/BTC
    if (opcode == 0xA3 || opcode == 0xAB || opcode == 0xB3 || opcode == 0xBB) {
        static const char* btNames[] = { "bt", "bts", "btr", "btc" };
        int idx = (opcode == 0xA3) ? 0 : (opcode == 0xAB) ? 1 : (opcode == 0xB3) ? 2 : 3;
        inst.mnemonic = btNames[idx];
        if (p < end) {
            uint8_t modrm = *p++;
            DecodeModRM(p, end, inst, modrm);
        }
        inst.modifiesFlags = true;
        return;
    }
    
    // BSF/BSR
    if (opcode == 0xBC || opcode == 0xBD) {
        inst.mnemonic = (opcode == 0xBC) ? "bsf" : "bsr";
        if (p < end) {
            uint8_t modrm = *p++;
            DecodeModRM(p, end, inst, modrm);
        }
        inst.modifiesFlags = true;
        return;
    }
    
    // XADD
    if (opcode == 0xC0 || opcode == 0xC1) {
        inst.mnemonic = "xadd";
        if (p < end) {
            uint8_t modrm = *p++;
            DecodeModRM(p, end, inst, modrm);
        }
        return;
    }
    
    // CMPXCHG
    if (opcode == 0xB0 || opcode == 0xB1) {
        inst.mnemonic = "cmpxchg";
        if (p < end) {
            uint8_t modrm = *p++;
            DecodeModRM(p, end, inst, modrm);
        }
        inst.modifiesFlags = true;
        return;
    }
    
    // NOP (multi-byte)
    if (opcode == 0x1F) {
        inst.type = InstructionType::Nop;
        inst.mnemonic = "nop";
        // Skip ModR/M
        if (p < end) {
            uint8_t modrm = *p++;
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm = modrm & 7;
            if (mod != 3 && rm == 4 && p < end) ++p; // SIB
            if (mod == 0 && rm == 5) p += 4;
            else if (mod == 1) ++p;
            else if (mod == 2) p += 4;
        }
        return;
    }
    
    // MFENCE/LFENCE/SFENCE
    if (opcode == 0xAE && p < end) {
        uint8_t modrm = *p;
        uint8_t op = (modrm >> 3) & 7;
        if (modrm >= 0xE8 && modrm <= 0xEF) {
            inst.mnemonic = "lfence";
            return;
        }
        if (modrm >= 0xF0 && modrm <= 0xF7) {
            inst.mnemonic = "mfence";
            return;
        }
        if (modrm >= 0xF8) {
            inst.mnemonic = "sfence";
            return;
        }
    }
    
    // Default - has ModR/M
    if (p < end) {
        uint8_t modrm = *p++;
        DecodeModRM(p, end, inst, modrm);
    }
    
    inst.mnemonic = "0F " + std::to_string(opcode);
}

void Disassembler::DecodeThreeByteOpcode38(const uint8_t*& p, const uint8_t* end, uint8_t opcode, Instruction& inst)
{
    // POPCNT (F3 0F B8)
    if (opcode == 0xF0 && inst.hasRep) {
        inst.mnemonic = "popcnt";
    }
    // CRC32
    else if (opcode == 0xF0 || opcode == 0xF1) {
        inst.mnemonic = "crc32";
    }
    else {
        inst.mnemonic = "0F38";
    }
    
    if (p < end) {
        uint8_t modrm = *p++;
        DecodeModRM(p, end, inst, modrm);
    }
}

void Disassembler::DecodeThreeByteOpcode3A(const uint8_t*& p, const uint8_t* end, uint8_t opcode, Instruction& inst)
{
    inst.mnemonic = "0F3A";
    
    if (p < end) {
        uint8_t modrm = *p++;
        DecodeModRM(p, end, inst, modrm);
    }
    // Skip immediate
}

void Disassembler::DecodeModRM(const uint8_t*& p, const uint8_t* end, Instruction& inst, uint8_t modrm)
{
    uint8_t mod = (modrm >> 6) & 3;
    uint8_t reg = (modrm >> 3) & 7;
    uint8_t rm = modrm & 7;
    
    // Determine operand size
    uint8_t opSize = 4;
    if (inst.rexW) opSize = 8;
    else if (inst.has66) opSize = 2;
    
    // REG operand
    inst.operands[0].type = OperandType::Register;
    inst.operands[0].size = opSize;
    inst.operands[0].reg = GetRegister(reg | (inst.rexR ? 8 : 0), inst.hasRex, opSize);
    
    // R/M operand
    if (mod == 3) {
        // Register direct
        inst.operands[1].type = OperandType::Register;
        inst.operands[1].size = opSize;
        inst.operands[1].reg = GetRegister(rm | (inst.rexB ? 8 : 0), inst.hasRex, opSize);
    } else {
        // Memory
        inst.operands[1].type = OperandType::Memory;
        inst.operands[1].size = opSize;
        
        if (rm == 4) {
            // SIB byte follows
            DecodeSIB(p, end, inst.operands[1], mod, inst.rexX, inst.rexB);
        } else if (mod == 0 && rm == 5) {
            // RIP-relative (x64) or disp32 (x86)
            if (m_is64Bit) {
                inst.operands[1].isRipRelative = true;
                inst.operands[1].base = RegisterId::RIP;
            }
            if (p + 4 <= end) {
                inst.operands[1].displacement = *reinterpret_cast<const int32_t*>(p);
                p += 4;
            }
        } else {
            inst.operands[1].base = GetRegister(rm | (inst.rexB ? 8 : 0), inst.hasRex, 8);
            
            if (mod == 1) {
                // disp8
                if (p < end) {
                    inst.operands[1].displacement = *reinterpret_cast<const int8_t*>(p);
                    ++p;
                }
            } else if (mod == 2) {
                // disp32
                if (p + 4 <= end) {
                    inst.operands[1].displacement = *reinterpret_cast<const int32_t*>(p);
                    p += 4;
                }
            }
        }
    }
    
    inst.operandCount = 2;
}

void Disassembler::DecodeSIB(const uint8_t*& p, const uint8_t* end, Operand& op, uint8_t mod, bool rexX, bool rexB)
{
    if (p >= end) return;
    
    uint8_t sib = *p++;
    uint8_t scale = 1 << ((sib >> 6) & 3);
    uint8_t index = (sib >> 3) & 7;
    uint8_t base = sib & 7;
    
    op.scale = scale;
    
    // Index register (RSP/R12 means no index)
    if (index != 4) {
        op.index = GetRegister(index | (rexX ? 8 : 0), true, 8);
    }
    
    // Base register
    if (mod == 0 && base == 5) {
        // disp32 only (no base) or RBP+disp32
        op.base = RegisterId::NONE;
        if (p + 4 <= end) {
            op.displacement = *reinterpret_cast<const int32_t*>(p);
            p += 4;
        }
    } else {
        op.base = GetRegister(base | (rexB ? 8 : 0), true, 8);
        
        if (mod == 1 && p < end) {
            op.displacement = *reinterpret_cast<const int8_t*>(p);
            ++p;
        } else if (mod == 2 && p + 4 <= end) {
            op.displacement = *reinterpret_cast<const int32_t*>(p);
            p += 4;
        }
    }
}

RegisterId Disassembler::GetRegister(uint8_t code, bool rex, uint8_t size)
{
    code &= 0x0F;
    return static_cast<RegisterId>(code);
}

// ============================================================================
// Formatting
// ============================================================================

void Disassembler::FormatMnemonic(Instruction& inst)
{
    // Already set during decoding
}

void Disassembler::FormatOperands(Instruction& inst)
{
    if (inst.operandCount == 0) return;
    
    std::string result;
    for (uint8_t i = 0; i < inst.operandCount && i < 4; ++i) {
        if (i > 0) result += ", ";
        result += FormatOperand(inst.operands[i], inst.address, inst.length);
    }
    inst.operandsStr = result;
}

std::string Disassembler::FormatInstruction(const Instruction& inst)
{
    char buf[256];
    
    // Format address
    snprintf(buf, sizeof(buf), "%016llX  ", (unsigned long long)inst.address);
    std::string result = buf;
    
    // Format bytes
    for (size_t i = 0; i < inst.bytes.size() && i < 8; ++i) {
        snprintf(buf, sizeof(buf), "%02X ", inst.bytes[i]);
        result += buf;
    }
    
    // Pad to fixed width
    while (result.length() < 40) result += ' ';
    
    // Add prefixes
    if (inst.hasLock) result += "lock ";
    if (inst.hasRep) result += "rep ";
    if (inst.hasRepne) result += "repne ";
    
    // Mnemonic
    result += inst.mnemonic;
    
    // Operands
    if (!inst.operandsStr.empty()) {
        while (result.length() < 55) result += ' ';
        result += inst.operandsStr;
    }
    
    // Comment
    if (!inst.comment.empty()) {
        while (result.length() < 80) result += ' ';
        result += "; ";
        result += inst.comment;
    }
    
    return result;
}

std::string Disassembler::FormatOperand(const Operand& op, uint64_t instrAddr, uint32_t instrLen)
{
    char buf[128];
    
    switch (op.type) {
        case OperandType::None:
            return "";
            
        case OperandType::Register:
            return GetRegisterName(op.reg, op.size);
            
        case OperandType::Immediate:
            if (op.displacement >= 0 && op.displacement <= 0xFF) {
                snprintf(buf, sizeof(buf), "0x%02llX", (unsigned long long)op.displacement);
            } else if (op.displacement >= 0 && op.displacement <= 0xFFFF) {
                snprintf(buf, sizeof(buf), "0x%04llX", (unsigned long long)op.displacement);
            } else if (op.displacement >= 0 && (op.displacement & 0xFFFFFFFF00000000ULL) == 0) {
                snprintf(buf, sizeof(buf), "0x%08llX", (unsigned long long)op.displacement);
            } else {
                snprintf(buf, sizeof(buf), "0x%016llX", (unsigned long long)op.displacement);
            }
            return buf;
            
        case OperandType::RelOffset:
            snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)op.displacement);
            return buf;
            
        case OperandType::Memory: {
            std::string result;
            
            // Size prefix
            switch (op.size) {
                case 1: result = "byte ptr "; break;
                case 2: result = "word ptr "; break;
                case 4: result = "dword ptr "; break;
                case 8: result = "qword ptr "; break;
                case 16: result = "xmmword ptr "; break;
                case 32: result = "ymmword ptr "; break;
                default: break;
            }
            
            result += "[";
            bool needsPlus = false;
            
            if (op.isRipRelative) {
                result += "rip";
                needsPlus = true;
            } else if (op.base != RegisterId::NONE) {
                result += GetRegisterName(op.base, 8);
                needsPlus = true;
            }
            
            if (op.index != RegisterId::NONE) {
                if (needsPlus) result += "+";
                result += GetRegisterName(op.index, 8);
                if (op.scale > 1) {
                    snprintf(buf, sizeof(buf), "*%d", op.scale);
                    result += buf;
                }
                needsPlus = true;
            }
            
            if (op.displacement != 0 || !needsPlus) {
                if (op.displacement >= 0) {
                    if (needsPlus) result += "+";
                    snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)op.displacement);
                } else {
                    snprintf(buf, sizeof(buf), "-0x%llX", (unsigned long long)(-op.displacement));
                }
                result += buf;
            }
            
            result += "]";
            return result;
        }
        
        default:
            return "???";
    }
}

std::string Disassembler::GetRegisterName(RegisterId reg, uint8_t size)
{
    uint8_t code = static_cast<uint8_t>(reg) & 0x0F;
    
    if (code < 16) {
        switch (size) {
            case 1: return g_reg8NamesRex[code];
            case 2: return g_reg16Names[code];
            case 4: return g_reg32Names[code];
            case 8: return g_reg64Names[code];
        }
    }
    
    // Segment registers
    if (reg >= RegisterId::ES && reg <= RegisterId::GS) {
        return g_segNames[static_cast<uint8_t>(reg) - static_cast<uint8_t>(RegisterId::ES)];
    }
    
    // XMM registers
    if (reg >= RegisterId::XMM0 && reg <= RegisterId::XMM15) {
        return g_xmmNames[static_cast<uint8_t>(reg) - static_cast<uint8_t>(RegisterId::XMM0)];
    }
    
    return "???";
}

// ============================================================================
// Control Flow Analysis
// ============================================================================

std::vector<BasicBlock> Disassembler::BuildCFG(const uint8_t* code, size_t size, uint64_t entry)
{
    std::vector<BasicBlock> blocks;
    
    // First, disassemble and find all instructions
    std::vector<Instruction> instructions = Disassemble(code, size, entry);
    if (instructions.empty()) return blocks;
    
    // Find leader addresses
    std::unordered_set<uint64_t> leaders;
    FindLeaders(instructions, leaders);
    
    // Build basic blocks
    BuildBlocks(instructions, leaders, blocks);
    
    // Connect blocks
    ConnectBlocks(blocks);
    
    return blocks;
}

void Disassembler::FindLeaders(const std::vector<Instruction>& instructions,
                                std::unordered_set<uint64_t>& leaders)
{
    if (instructions.empty()) return;
    
    // First instruction is always a leader
    leaders.insert(instructions[0].address);
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        const Instruction& inst = instructions[i];
        
        // Target of any branch/jump is a leader
        if (inst.isBranch || inst.isCall) {
            if (inst.branchTarget != 0) {
                leaders.insert(inst.branchTarget);
            }
        }
        
        // Instruction after branch/call is a leader
        if (inst.isControlFlow && i + 1 < instructions.size()) {
            leaders.insert(instructions[i + 1].address);
        }
    }
}

void Disassembler::BuildBlocks(const std::vector<Instruction>& instructions,
                                const std::unordered_set<uint64_t>& leaders,
                                std::vector<BasicBlock>& blocks)
{
    if (instructions.empty()) return;
    
    BasicBlock current;
    current.startAddress = instructions[0].address;
    current.isEntry = true;
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        const Instruction& inst = instructions[i];
        
        // Start new block if this is a leader (except first)
        if (i > 0 && leaders.count(inst.address)) {
            current.endAddress = instructions[i - 1].address;
            blocks.push_back(current);
            
            current = BasicBlock();
            current.startAddress = inst.address;
        }
        
        current.instructions.push_back(inst);
        current.endAddress = inst.address;
        
        // Mark exit blocks
        if (inst.isReturn) {
            current.isExit = true;
        }
    }
    
    // Add last block
    if (!current.instructions.empty()) {
        blocks.push_back(current);
    }
}

void Disassembler::ConnectBlocks(std::vector<BasicBlock>& blocks)
{
    // Build address-to-block index
    std::unordered_map<uint64_t, size_t> blockByStart;
    for (size_t i = 0; i < blocks.size(); ++i) {
        blockByStart[blocks[i].startAddress] = i;
    }
    
    for (size_t i = 0; i < blocks.size(); ++i) {
        BasicBlock& bb = blocks[i];
        if (bb.instructions.empty()) continue;
        
        const Instruction& lastInst = bb.instructions.back();
        
        // Branch target
        if (lastInst.isBranch && lastInst.branchTarget != 0) {
            auto it = blockByStart.find(lastInst.branchTarget);
            if (it != blockByStart.end()) {
                bb.successors.push_back(lastInst.branchTarget);
                blocks[it->second].predecessors.push_back(bb.startAddress);
            }
        }
        
        // Fall-through (conditional branches or normal instructions)
        if (!lastInst.isReturn && 
            !(lastInst.type == InstructionType::Jmp && !lastInst.isConditional)) {
            // Find next block
            if (i + 1 < blocks.size()) {
                bb.successors.push_back(blocks[i + 1].startAddress);
                blocks[i + 1].predecessors.push_back(bb.startAddress);
            }
        }
    }
    
    // Detect loop headers (blocks with back edges)
    for (auto& bb : blocks) {
        for (uint64_t predAddr : bb.predecessors) {
            if (predAddr >= bb.startAddress) {
                bb.isLoopHeader = true;
                break;
            }
        }
    }
}

Function Disassembler::AnalyzeFunction(const uint8_t* code, size_t size, uint64_t entry)
{
    Function func;
    func.startAddress = entry;
    func.blocks = BuildCFG(code, size, entry);
    
    if (!func.blocks.empty()) {
        // Find end address (highest block end)
        uint64_t maxEnd = entry;
        for (const auto& bb : func.blocks) {
            if (bb.endAddress > maxEnd) {
                maxEnd = bb.endAddress;
            }
        }
        func.endAddress = maxEnd;
        
        // Detect frame pointer usage
        if (!func.blocks[0].instructions.empty()) {
            const auto& first = func.blocks[0].instructions[0];
            // Look for "push rbp" / "mov rbp, rsp"
            if (first.type == InstructionType::Push) {
                func.usesFramePointer = true;
            }
        }
        
        // Estimate calling convention
        if (m_is64Bit) {
            func.callingConvention = "ms_x64"; // Windows default
        } else {
            func.callingConvention = "cdecl";
        }
    }
    
    return func;
}

// ============================================================================
// Pattern Recognition
// ============================================================================

std::vector<uint64_t> Disassembler::FindFunctions(const uint8_t* code, size_t size, uint64_t baseAddress)
{
    std::vector<uint64_t> functions;
    
    // Scan for common function prologues
    for (size_t i = 0; i + 8 < size; ) {
        bool found = false;
        uint64_t addr = baseAddress + i;
        
        // Pattern 1: push rbp; mov rbp, rsp (55 48 89 E5)
        if (i + 4 <= size && code[i] == 0x55 && 
            code[i+1] == 0x48 && code[i+2] == 0x89 && code[i+3] == 0xE5) {
            functions.push_back(addr);
            found = true;
        }
        // Pattern 2: push rbp; sub rsp, imm (55 48 83 EC)
        else if (i + 4 <= size && code[i] == 0x55 &&
                 code[i+1] == 0x48 && code[i+2] == 0x83 && code[i+3] == 0xEC) {
            functions.push_back(addr);
            found = true;
        }
        // Pattern 3: sub rsp, imm (48 83 EC)
        else if (i + 3 <= size && code[i] == 0x48 &&
                 code[i+1] == 0x83 && code[i+2] == 0xEC) {
            functions.push_back(addr);
            found = true;
        }
        // Pattern 4: push rbx; sub rsp (53 48 83 EC)
        else if (i + 4 <= size && code[i] == 0x53 &&
                 code[i+1] == 0x48 && code[i+2] == 0x83 && code[i+3] == 0xEC) {
            functions.push_back(addr);
            found = true;
        }
        // Pattern 5: Thunk jmp [rip+disp32] (FF 25)
        else if (i + 6 <= size && code[i] == 0xFF && code[i+1] == 0x25) {
            functions.push_back(addr);
            i += 6;
            continue;
        }
        
        if (found) {
            // Skip to potential next function (scan for RET or INT3 padding)
            size_t j = i + 4;
            while (j < size && j < i + 65536) {
                if (code[j] == 0xC3 || code[j] == 0xCC) {
                    i = j + 1;
                    // Skip INT3 padding
                    while (i < size && code[i] == 0xCC) ++i;
                    break;
                }
                ++j;
            }
            if (j >= size || j >= i + 65536) {
                i = j;
            }
        } else {
            ++i;
        }
    }
    
    return functions;
}

std::vector<uint64_t> Disassembler::FindCallTargets(const uint8_t* code, size_t size, uint64_t baseAddress)
{
    std::vector<uint64_t> targets;
    std::unordered_set<uint64_t> seen;
    
    std::vector<Instruction> disasm = Disassemble(code, size, baseAddress);
    
    for (const auto& inst : disasm) {
        if (inst.isCall && inst.branchTarget != 0) {
            if (seen.find(inst.branchTarget) == seen.end()) {
                seen.insert(inst.branchTarget);
                targets.push_back(inst.branchTarget);
            }
        }
    }
    
    std::sort(targets.begin(), targets.end());
    return targets;
}

std::vector<uint64_t> Disassembler::FindStrings(const uint8_t* code, size_t size, uint64_t baseAddress)
{
    std::vector<uint64_t> stringAddrs;
    
    // Simple string detection: sequences of printable ASCII
    size_t runStart = 0;
    size_t runLen = 0;
    
    for (size_t i = 0; i < size; ++i) {
        uint8_t b = code[i];
        if (b >= 0x20 && b <= 0x7E) {
            if (runLen == 0) runStart = i;
            ++runLen;
        } else if (b == 0 && runLen >= 4) {
            // Found null-terminated string
            stringAddrs.push_back(baseAddress + runStart);
            runLen = 0;
        } else {
            runLen = 0;
        }
    }
    
    return stringAddrs;
}

bool Disassembler::IsFunctionPrologue(const uint8_t* code, size_t size)
{
    if (size < 2) return false;
    
    // push rbp (55)
    if (code[0] == 0x55) return true;
    
    // sub rsp, imm (48 83 EC)
    if (size >= 3 && code[0] == 0x48 && code[1] == 0x83 && code[2] == 0xEC)
        return true;
    
    // push rbx/rdi/rsi
    if (code[0] == 0x53 || code[0] == 0x56 || code[0] == 0x57)
        return true;
    
    return false;
}

bool Disassembler::IsFunctionEpilogue(const uint8_t* code, size_t size)
{
    if (size < 1) return false;
    
    // ret (C3)
    if (code[0] == 0xC3) return true;
    
    // leave; ret (C9 C3)
    if (size >= 2 && code[0] == 0xC9 && code[1] == 0xC3) return true;
    
    // pop rbp; ret (5D C3)
    if (size >= 2 && code[0] == 0x5D && code[1] == 0xC3) return true;
    
    // add rsp, imm; ret (48 83 C4 XX C3)
    if (size >= 5 && code[0] == 0x48 && code[1] == 0x83 && 
        code[2] == 0xC4 && code[4] == 0xC3) return true;
    
    return false;
}

} // namespace ReverseEngineering
} // namespace RawrXD
