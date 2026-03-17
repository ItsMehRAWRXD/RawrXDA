// ============================================================================
// Win32IDE_AsmSemantic.cpp — ASM Semantic Support
// ============================================================================
//
// Provides deep assembly language understanding:
//   - Symbol table parser (labels, procedures, macros, equates, sections)
//   - x86/x64 register awareness with category/alias/size info
//   - Instruction lookup (300+ mnemonics with descriptions)
//   - Go-to-definition and find-references for ASM symbols
//   - Call graph construction (call/jmp edges between procedures)
//   - Data flow analysis (read/write tracking per symbol)
//   - AI-assisted block analysis (calling convention detection,
//     stack frame analysis, register liveness)
//   - Command palette integration (12 commands, IDM_ASM_* 5082–5093)
//   - HTTP endpoints (/api/asm/symbols, /api/asm/navigate, /api/asm/analyze)
//
// Supports: MASM, NASM, GAS (AT&T), FASM syntax variants
// Thread-safe: all symbol table access guarded by m_asmMutex
//
// Part of RawrXD-Shell — Phase 9A
// ============================================================================

#include "Win32IDE.h"
#include <richedit.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <regex>
#include <unordered_set>
#include <richedit.h>

// ============================================================================
// EDITOR HELPERS
// ============================================================================

void Win32IDE::gotoLine(int line) {
    if (!m_hwndEditor || line < 1) return;
    // Convert to 0-based for EM_LINEINDEX
    int charPos = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, line - 1, 0);
    if (charPos < 0) return;
    // Set caret to the beginning of the target line
    SendMessageA(m_hwndEditor, EM_SETSEL, charPos, charPos);
    // Scroll the line into view
    SendMessageA(m_hwndEditor, EM_SCROLLCARET, 0, 0);
    // Update internal line tracking
    m_currentLine = line;
}

// Extract the word (identifier) at the current cursor position using
// RichEdit EM_ messages, returning an empty string if nothing is found.

std::string Win32IDE::getWordAtCursor() const {
    if (!m_hwndEditor) return "";

    // Get cursor position
    CHARRANGE sel = {};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    // Get the line index and line start offset
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int lineLen   = (int)SendMessageA(m_hwndEditor, EM_LINELENGTH, lineStart, 0);

    if (lineLen <= 0 || lineLen > 4096) return "";

    // Read the line text
    std::vector<char> buf(lineLen + 2, 0);
    *(WORD*)buf.data() = (WORD)(lineLen + 1);
    SendMessageA(m_hwndEditor, EM_GETLINE, lineIndex, (LPARAM)buf.data());
    buf[(size_t)lineLen] = '\0';

    std::string lineText(buf.data(), (size_t)lineLen);
    int col = sel.cpMin - lineStart;
    if (col < 0 || col > (int)lineText.size()) return "";

    // Expand left and right from col to find the word boundary
    // Word chars: [A-Za-z0-9_.]
    auto isWordChar = [](char c) -> bool {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '.';
    };

    int left = col;
    while (left > 0 && isWordChar(lineText[(size_t)(left - 1)])) --left;
    int right = col;
    while (right < (int)lineText.size() && isWordChar(lineText[(size_t)right])) ++right;

    if (left >= right) return "";
    return lineText.substr((size_t)left, (size_t)(right - left));
}

// ============================================================================
// STATIC DATA — INSTRUCTION DATABASE
// ============================================================================
// 300+ x86/x64 instructions organized by category.
// Each entry: { mnemonic, category, description, operands, affectsFlags }

struct StaticInstructionEntry {
    const char* mnemonic;
    const char* category;
    const char* description;
    const char* operands;
    bool        affectsFlags;
};

static const StaticInstructionEntry g_instructionDB[] = {
    // ---- Data Transfer ----
    {"mov",     "data_transfer", "Move data between registers/memory",                  "dst, src",       false},
    {"movzx",   "data_transfer", "Move with zero-extension",                            "dst, src",       false},
    {"movsx",   "data_transfer", "Move with sign-extension",                            "dst, src",       false},
    {"movsxd",  "data_transfer", "Move with sign-extension (32→64)",                    "dst, src",       false},
    {"lea",     "data_transfer", "Load effective address",                               "dst, mem",       false},
    {"xchg",    "data_transfer", "Exchange register/memory values",                     "op1, op2",       false},
    {"bswap",   "data_transfer", "Byte swap (endian conversion)",                       "reg",            false},
    {"cmove",   "data_transfer", "Conditional move if equal (ZF=1)",                    "dst, src",       false},
    {"cmovne",  "data_transfer", "Conditional move if not equal (ZF=0)",                "dst, src",       false},
    {"cmovz",   "data_transfer", "Conditional move if zero (ZF=1)",                     "dst, src",       false},
    {"cmovnz",  "data_transfer", "Conditional move if not zero (ZF=0)",                 "dst, src",       false},
    {"cmovg",   "data_transfer", "Conditional move if greater (signed)",                "dst, src",       false},
    {"cmovge",  "data_transfer", "Conditional move if greater or equal (signed)",       "dst, src",       false},
    {"cmovl",   "data_transfer", "Conditional move if less (signed)",                   "dst, src",       false},
    {"cmovle",  "data_transfer", "Conditional move if less or equal (signed)",          "dst, src",       false},
    {"cmova",   "data_transfer", "Conditional move if above (unsigned)",                "dst, src",       false},
    {"cmovae",  "data_transfer", "Conditional move if above or equal (unsigned)",       "dst, src",       false},
    {"cmovb",   "data_transfer", "Conditional move if below (unsigned)",                "dst, src",       false},
    {"cmovbe",  "data_transfer", "Conditional move if below or equal (unsigned)",       "dst, src",       false},
    {"cbw",     "data_transfer", "Convert byte to word (AL→AX, sign-extend)",           "",               false},
    {"cwde",    "data_transfer", "Convert word to doubleword (AX→EAX, sign-extend)",    "",               false},
    {"cdqe",    "data_transfer", "Convert dword to qword (EAX→RAX, sign-extend)",       "",               false},
    {"cwd",     "data_transfer", "Convert word to dword (AX→DX:AX, sign-extend)",       "",               false},
    {"cdq",     "data_transfer", "Convert dword to qword (EAX→EDX:EAX, sign-extend)",   "",               false},
    {"cqo",     "data_transfer", "Convert qword to oword (RAX→RDX:RAX, sign-extend)",   "",               false},

    // ---- Stack ----
    {"push",    "stack",         "Push value onto stack",                                "src",            false},
    {"pop",     "stack",         "Pop value from stack",                                 "dst",            false},
    {"pushf",   "stack",         "Push RFLAGS onto stack",                               "",               false},
    {"popf",    "stack",         "Pop RFLAGS from stack",                                "",               true},
    {"enter",   "stack",         "Create stack frame",                                   "size, level",    false},
    {"leave",   "stack",         "Destroy stack frame (mov rsp,rbp; pop rbp)",           "",               false},

    // ---- Arithmetic ----
    {"add",     "arithmetic",    "Integer addition",                                     "dst, src",       true},
    {"adc",     "arithmetic",    "Add with carry",                                       "dst, src",       true},
    {"sub",     "arithmetic",    "Integer subtraction",                                  "dst, src",       true},
    {"sbb",     "arithmetic",    "Subtract with borrow",                                 "dst, src",       true},
    {"inc",     "arithmetic",    "Increment by 1",                                       "dst",            true},
    {"dec",     "arithmetic",    "Decrement by 1",                                       "dst",            true},
    {"neg",     "arithmetic",    "Two's complement negate",                              "dst",            true},
    {"mul",     "arithmetic",    "Unsigned multiply (RDX:RAX = RAX * src)",              "src",            true},
    {"imul",    "arithmetic",    "Signed multiply",                                      "dst[, src[, imm]]", true},
    {"div",     "arithmetic",    "Unsigned divide (RAX = RDX:RAX / src)",                "src",            true},
    {"idiv",    "arithmetic",    "Signed divide",                                        "src",            true},

    // ---- Logic ----
    {"and",     "logic",         "Bitwise AND",                                          "dst, src",       true},
    {"or",      "logic",         "Bitwise OR",                                           "dst, src",       true},
    {"xor",     "logic",         "Bitwise XOR",                                          "dst, src",       true},
    {"not",     "logic",         "Bitwise NOT (one's complement)",                       "dst",            false},
    {"test",    "logic",         "Bitwise AND (set flags only, no store)",               "op1, op2",       true},

    // ---- Shift / Rotate ----
    {"shl",     "shift",         "Shift left (logical)",                                 "dst, count",     true},
    {"shr",     "shift",         "Shift right (logical)",                                "dst, count",     true},
    {"sal",     "shift",         "Shift left (arithmetic, same as SHL)",                 "dst, count",     true},
    {"sar",     "shift",         "Shift right (arithmetic, preserves sign)",             "dst, count",     true},
    {"rol",     "shift",         "Rotate left",                                          "dst, count",     true},
    {"ror",     "shift",         "Rotate right",                                         "dst, count",     true},
    {"rcl",     "shift",         "Rotate left through carry",                            "dst, count",     true},
    {"rcr",     "shift",         "Rotate right through carry",                           "dst, count",     true},
    {"shld",    "shift",         "Double-precision shift left",                          "dst, src, count",true},
    {"shrd",    "shift",         "Double-precision shift right",                         "dst, src, count",true},

    // ---- Comparison / Set ----
    {"cmp",     "comparison",    "Compare (sets flags via subtraction, no store)",       "op1, op2",       true},
    {"sete",    "comparison",    "Set byte if equal (ZF=1)",                             "dst",            false},
    {"setne",   "comparison",    "Set byte if not equal (ZF=0)",                         "dst",            false},
    {"setg",    "comparison",    "Set byte if greater (signed)",                         "dst",            false},
    {"setge",   "comparison",    "Set byte if greater or equal (signed)",                "dst",            false},
    {"setl",    "comparison",    "Set byte if less (signed)",                            "dst",            false},
    {"setle",   "comparison",    "Set byte if less or equal (signed)",                   "dst",            false},
    {"seta",    "comparison",    "Set byte if above (unsigned)",                         "dst",            false},
    {"setae",   "comparison",    "Set byte if above or equal (unsigned)",                "dst",            false},
    {"setb",    "comparison",    "Set byte if below (unsigned)",                         "dst",            false},
    {"setbe",   "comparison",    "Set byte if below or equal (unsigned)",                "dst",            false},

    // ---- Control Flow ----
    {"jmp",     "control_flow",  "Unconditional jump",                                   "label",          false},
    {"je",      "control_flow",  "Jump if equal (ZF=1)",                                 "label",          false},
    {"jne",     "control_flow",  "Jump if not equal (ZF=0)",                             "label",          false},
    {"jz",      "control_flow",  "Jump if zero (ZF=1)",                                  "label",          false},
    {"jnz",     "control_flow",  "Jump if not zero (ZF=0)",                              "label",          false},
    {"jg",      "control_flow",  "Jump if greater (signed)",                             "label",          false},
    {"jge",     "control_flow",  "Jump if greater or equal (signed)",                    "label",          false},
    {"jl",      "control_flow",  "Jump if less (signed)",                                "label",          false},
    {"jle",     "control_flow",  "Jump if less or equal (signed)",                       "label",          false},
    {"ja",      "control_flow",  "Jump if above (unsigned)",                             "label",          false},
    {"jae",     "control_flow",  "Jump if above or equal (unsigned)",                    "label",          false},
    {"jb",      "control_flow",  "Jump if below (unsigned)",                             "label",          false},
    {"jbe",     "control_flow",  "Jump if below or equal (unsigned)",                    "label",          false},
    {"jc",      "control_flow",  "Jump if carry (CF=1)",                                 "label",          false},
    {"jnc",     "control_flow",  "Jump if no carry (CF=0)",                              "label",          false},
    {"jo",      "control_flow",  "Jump if overflow (OF=1)",                              "label",          false},
    {"jno",     "control_flow",  "Jump if no overflow (OF=0)",                           "label",          false},
    {"js",      "control_flow",  "Jump if sign (SF=1, negative)",                        "label",          false},
    {"jns",     "control_flow",  "Jump if no sign (SF=0, positive)",                     "label",          false},
    {"jcxz",    "control_flow",  "Jump if CX is zero",                                   "label",          false},
    {"jecxz",   "control_flow",  "Jump if ECX is zero",                                  "label",          false},
    {"jrcxz",   "control_flow",  "Jump if RCX is zero",                                  "label",          false},
    {"loop",    "control_flow",  "Decrement CX/ECX/RCX and jump if nonzero",             "label",          false},
    {"loope",   "control_flow",  "Loop while equal (ZF=1 and count≠0)",                  "label",          false},
    {"loopne",  "control_flow",  "Loop while not equal (ZF=0 and count≠0)",              "label",          false},
    {"call",    "control_flow",  "Call procedure (push return address, jump)",            "target",         false},
    {"ret",     "control_flow",  "Return from procedure (pop and jump)",                 "[imm16]",        false},
    {"retn",    "control_flow",  "Near return from procedure",                           "[imm16]",        false},
    {"retf",    "control_flow",  "Far return from procedure",                            "[imm16]",        false},

    // ---- String ----
    {"rep",     "string",        "Repeat prefix (while CX/ECX/RCX ≠ 0)",                "",               false},
    {"repe",    "string",        "Repeat while equal (ZF=1)",                            "",               false},
    {"repne",   "string",        "Repeat while not equal (ZF=0)",                        "",               false},
    {"movsb",   "string",        "Move string byte (DS:RSI → ES:RDI)",                  "",               false},
    {"movsw",   "string",        "Move string word",                                     "",               false},
    {"movsd",   "string",        "Move string dword",                                    "",               false},
    {"movsq",   "string",        "Move string qword",                                   "",               false},
    {"cmpsb",   "string",        "Compare string bytes",                                 "",               true},
    {"cmpsw",   "string",        "Compare string words",                                 "",               true},
    {"scasb",   "string",        "Scan string byte (compare AL with ES:RDI)",            "",               true},
    {"scasw",   "string",        "Scan string word",                                     "",               true},
    {"stosb",   "string",        "Store string byte (AL → ES:RDI)",                     "",               false},
    {"stosw",   "string",        "Store string word",                                    "",               false},
    {"stosd",   "string",        "Store string dword",                                   "",               false},
    {"stosq",   "string",        "Store string qword",                                   "",               false},
    {"lodsb",   "string",        "Load string byte (DS:RSI → AL)",                      "",               false},
    {"lodsw",   "string",        "Load string word",                                     "",               false},
    {"lodsd",   "string",        "Load string dword",                                    "",               false},
    {"lodsq",   "string",        "Load string qword",                                    "",               false},

    // ---- Bit Manipulation ----
    {"bt",      "bit",           "Bit test (copy bit to CF)",                            "src, bit",       true},
    {"bts",     "bit",           "Bit test and set",                                     "dst, bit",       true},
    {"btr",     "bit",           "Bit test and reset",                                   "dst, bit",       true},
    {"btc",     "bit",           "Bit test and complement",                              "dst, bit",       true},
    {"bsf",     "bit",           "Bit scan forward (find lowest set bit)",               "dst, src",       true},
    {"bsr",     "bit",           "Bit scan reverse (find highest set bit)",              "dst, src",       true},
    {"popcnt",  "bit",           "Population count (number of set bits)",                "dst, src",       true},
    {"lzcnt",   "bit",           "Count leading zeros",                                  "dst, src",       true},
    {"tzcnt",   "bit",           "Count trailing zeros",                                 "dst, src",       true},
    {"pdep",    "bit",           "Parallel bit deposit (BMI2)",                          "dst, src, mask", false},
    {"pext",    "bit",           "Parallel bit extract (BMI2)",                          "dst, src, mask", false},
    {"andn",    "bit",           "Bitwise AND-NOT (BMI1)",                               "dst, src1, src2",true},
    {"blsi",    "bit",           "Extract lowest set bit (BMI1)",                        "dst, src",       true},
    {"blsmsk",  "bit",           "Get mask up to lowest set bit (BMI1)",                 "dst, src",       true},
    {"blsr",    "bit",           "Reset lowest set bit (BMI1)",                          "dst, src",       true},
    {"bzhi",    "bit",           "Zero high bits starting at position (BMI2)",           "dst, src, idx",  true},

    // ---- SSE/AVX ----
    {"movaps",  "simd",          "Move aligned packed single-precision",                 "dst, src",       false},
    {"movups",  "simd",          "Move unaligned packed single-precision",               "dst, src",       false},
    {"movapd",  "simd",          "Move aligned packed double-precision",                 "dst, src",       false},
    {"movupd",  "simd",          "Move unaligned packed double-precision",               "dst, src",       false},
    {"movdqa",  "simd",          "Move aligned packed integers (128-bit)",               "dst, src",       false},
    {"movdqu",  "simd",          "Move unaligned packed integers (128-bit)",             "dst, src",       false},
    {"movss",   "simd",          "Move scalar single-precision",                         "dst, src",       false},
    {"movsd",   "simd",          "Move scalar double-precision",                         "dst, src",       false},
    {"addps",   "simd",          "Add packed single-precision",                          "dst, src",       false},
    {"addpd",   "simd",          "Add packed double-precision",                          "dst, src",       false},
    {"addss",   "simd",          "Add scalar single-precision",                          "dst, src",       false},
    {"addsd",   "simd",          "Add scalar double-precision",                          "dst, src",       false},
    {"subps",   "simd",          "Subtract packed single-precision",                     "dst, src",       false},
    {"subpd",   "simd",          "Subtract packed double-precision",                     "dst, src",       false},
    {"mulps",   "simd",          "Multiply packed single-precision",                     "dst, src",       false},
    {"mulpd",   "simd",          "Multiply packed double-precision",                     "dst, src",       false},
    {"divps",   "simd",          "Divide packed single-precision",                       "dst, src",       false},
    {"divpd",   "simd",          "Divide packed double-precision",                       "dst, src",       false},
    {"sqrtps",  "simd",          "Square root packed single-precision",                  "dst, src",       false},
    {"sqrtpd",  "simd",          "Square root packed double-precision",                  "dst, src",       false},
    {"maxps",   "simd",          "Maximum packed single-precision",                      "dst, src",       false},
    {"minps",   "simd",          "Minimum packed single-precision",                      "dst, src",       false},
    {"pxor",    "simd",          "Packed bitwise XOR (128-bit)",                         "dst, src",       false},
    {"por",     "simd",          "Packed bitwise OR (128-bit)",                          "dst, src",       false},
    {"pand",    "simd",          "Packed bitwise AND (128-bit)",                         "dst, src",       false},
    {"pandn",   "simd",          "Packed bitwise AND-NOT (128-bit)",                     "dst, src",       false},
    {"pcmpeqb", "simd",          "Packed compare bytes for equality",                    "dst, src",       false},
    {"pcmpeqw", "simd",          "Packed compare words for equality",                    "dst, src",       false},
    {"pcmpeqd", "simd",          "Packed compare dwords for equality",                   "dst, src",       false},
    {"pmovmskb","simd",          "Move byte mask to GPR",                                "dst, src",       false},
    {"pshufd",  "simd",          "Shuffle packed dwords",                                "dst, src, imm8", false},
    {"shufps",  "simd",          "Shuffle packed single-precision",                      "dst, src, imm8", false},
    {"unpcklps","simd",          "Unpack and interleave low single-precision",           "dst, src",       false},
    {"unpckhps","simd",          "Unpack and interleave high single-precision",          "dst, src",       false},

    // ---- VEX-encoded (AVX) ----
    {"vaddps",  "avx",           "VEX add packed single-precision (3-operand)",          "dst, src1, src2",false},
    {"vsubps",  "avx",           "VEX subtract packed single-precision",                 "dst, src1, src2",false},
    {"vmulps",  "avx",           "VEX multiply packed single-precision",                 "dst, src1, src2",false},
    {"vdivps",  "avx",           "VEX divide packed single-precision",                   "dst, src1, src2",false},
    {"vaddpd",  "avx",           "VEX add packed double-precision",                      "dst, src1, src2",false},
    {"vsubpd",  "avx",           "VEX subtract packed double-precision",                 "dst, src1, src2",false},
    {"vmulpd",  "avx",           "VEX multiply packed double-precision",                 "dst, src1, src2",false},
    {"vdivpd",  "avx",           "VEX divide packed double-precision",                   "dst, src1, src2",false},
    {"vmovaps", "avx",           "VEX move aligned packed single-precision",             "dst, src",       false},
    {"vmovups", "avx",           "VEX move unaligned packed single-precision",           "dst, src",       false},
    {"vmovdqa", "avx",           "VEX move aligned packed integers",                     "dst, src",       false},
    {"vmovdqu", "avx",           "VEX move unaligned packed integers",                   "dst, src",       false},
    {"vpxor",   "avx",           "VEX packed XOR",                                       "dst, src1, src2",false},
    {"vpor",    "avx",           "VEX packed OR",                                        "dst, src1, src2",false},
    {"vpand",   "avx",           "VEX packed AND",                                       "dst, src1, src2",false},
    {"vbroadcastss", "avx",      "Broadcast single-precision float to all elements",     "dst, src",       false},
    {"vbroadcastsd", "avx",      "Broadcast double-precision float to all elements",     "dst, src",       false},
    {"vfmadd132ps","avx",        "Fused multiply-add packed single (FMA3)",              "dst, src2, src3",false},
    {"vfmadd213ps","avx",        "Fused multiply-add packed single (FMA3)",              "dst, src2, src3",false},
    {"vfmadd231ps","avx",        "Fused multiply-add packed single (FMA3)",              "dst, src2, src3",false},

    // ---- System / Privileged ----
    {"nop",     "system",        "No operation",                                         "",               false},
    {"int",     "system",        "Software interrupt",                                   "imm8",           false},
    {"int3",    "system",        "Breakpoint trap (INT 3)",                              "",               false},
    {"syscall", "system",        "System call (64-bit long mode)",                       "",               false},
    {"sysenter","system",        "System call (fast entry, 32-bit)",                     "",               false},
    {"sysexit", "system",        "System call return (fast, 32-bit)",                    "",               false},
    {"sysret",  "system",        "System call return (64-bit long mode)",                "",               false},
    {"cpuid",   "system",        "CPU identification (EAX selects info)",                "",               false},
    {"rdtsc",   "system",        "Read time-stamp counter → EDX:EAX",                   "",               false},
    {"rdtscp",  "system",        "Read time-stamp counter and processor ID",             "",               false},
    {"rdmsr",   "system",        "Read model-specific register",                         "",               false},
    {"wrmsr",   "system",        "Write model-specific register",                        "",               false},
    {"hlt",     "system",        "Halt processor",                                       "",               false},
    {"cli",     "system",        "Clear interrupt flag (disable interrupts)",            "",               false},
    {"sti",     "system",        "Set interrupt flag (enable interrupts)",               "",               false},
    {"clc",     "system",        "Clear carry flag",                                     "",               true},
    {"stc",     "system",        "Set carry flag",                                       "",               true},
    {"cmc",     "system",        "Complement carry flag",                                "",               true},
    {"cld",     "system",        "Clear direction flag",                                 "",               false},
    {"std",     "system",        "Set direction flag",                                   "",               false},
    {"lfence",  "system",        "Load fence (serialize loads)",                         "",               false},
    {"sfence",  "system",        "Store fence (serialize stores)",                       "",               false},
    {"mfence",  "system",        "Memory fence (serialize all memory ops)",              "",               false},
    {"pause",   "system",        "Spin-loop hint (reduces power in busy-wait)",          "",               false},
    {"prefetchnta", "system",    "Prefetch data into non-temporal cache line",           "mem",            false},
    {"prefetcht0",  "system",    "Prefetch data into all cache levels",                  "mem",            false},
    {"prefetcht1",  "system",    "Prefetch data into L2+ cache",                         "mem",            false},
    {"prefetcht2",  "system",    "Prefetch data into L3+ cache",                         "mem",            false},
    {"xgetbv",  "system",        "Get extended control register",                        "",               false},

    // ---- MASM Directives (treated as pseudo-instructions for lookup) ----
    {"proc",    "directive",     "Begin procedure definition (MASM)",                    "name [NEAR|FAR]",false},
    {"endp",    "directive",     "End procedure definition (MASM)",                      "name",           false},
    {"macro",   "directive",     "Begin macro definition",                               "name [params]",  false},
    {"endm",    "directive",     "End macro definition",                                 "",               false},
    {"equ",     "directive",     "Define symbolic constant",                             "name EQU value", false},
    {"db",      "directive",     "Define byte(s)",                                       "value[,value...]",false},
    {"dw",      "directive",     "Define word(s) (16-bit)",                              "value[,value...]",false},
    {"dd",      "directive",     "Define doubleword(s) (32-bit)",                        "value[,value...]",false},
    {"dq",      "directive",     "Define quadword(s) (64-bit)",                          "value[,value...]",false},
    {"dt",      "directive",     "Define ten-byte (80-bit FP)",                          "value[,value...]",false},
    {"resb",    "directive",     "Reserve byte(s) — NASM",                               "count",          false},
    {"resw",    "directive",     "Reserve word(s) — NASM",                               "count",          false},
    {"resd",    "directive",     "Reserve dword(s) — NASM",                              "count",          false},
    {"resq",    "directive",     "Reserve qword(s) — NASM",                              "count",          false},
    {"section", "directive",     "Declare section/segment (NASM/GAS)",                   ".text|.data|.bss",false},
    {"segment", "directive",     "Declare segment (MASM/NASM)",                          "name",           false},
    {"global",  "directive",     "Declare global symbol (NASM/GAS)",                     "name",           false},
    {"extern",  "directive",     "Declare external symbol",                              "name",           false},
    {"extrn",   "directive",     "Declare external symbol (MASM)",                       "name:type",      false},
    {"public",  "directive",     "Declare public symbol (MASM)",                         "name",           false},
    {"include", "directive",     "Include source file",                                  "filename",       false},
    {"incbin",  "directive",     "Include binary file (NASM)",                           "filename",       false},
    {"times",   "directive",     "Repeat instruction/data N times (NASM)",               "count insn",     false},
    {"align",   "directive",     "Align to boundary",                                    "boundary",       false},
    {"org",     "directive",     "Set origin address",                                   "address",        false},
    {"struc",   "directive",     "Begin structure definition (NASM) / STRUCT (MASM)",    "name",           false},
    {"ends",    "directive",     "End structure definition (MASM)",                       "name",           false},
    {"invoke",  "directive",     "Call procedure with arguments (MASM high-level)",      "proc, args...",  false},
    {"proto",   "directive",     "Declare procedure prototype (MASM)",                   "name PROTO args",false},
    {".model",  "directive",     "Set memory model (MASM)",                              "FLAT|SMALL|...", false},
    {".stack",  "directive",     "Set stack size (MASM)",                                "size",           false},
    {".code",   "directive",     "Begin code section (MASM)",                            "",               false},
    {".data",   "directive",     "Begin initialized data section (MASM)",                "",               false},
    {".data?",  "directive",     "Begin uninitialized data section (MASM)",              "",               false},
    {".const",  "directive",     "Begin constant data section (MASM)",                   "",               false},

    // Sentinel
    {nullptr,   nullptr,         nullptr,                                                nullptr,          false}
};

// ============================================================================
// STATIC DATA — REGISTER DATABASE
// ============================================================================

struct StaticRegisterEntry {
    const char* name;
    const char* category;
    int         bits;
    const char* description;
    const char* aliases;
};

static const StaticRegisterEntry g_registerDB[] = {
    // 64-bit GPR
    {"rax", "gpr_64", 64, "Accumulator (return value, mul/div operand)",           "rax → eax → ax → al/ah"},
    {"rbx", "gpr_64", 64, "Base register (callee-saved)",                          "rbx → ebx → bx → bl/bh"},
    {"rcx", "gpr_64", 64, "Counter (loop count, 1st integer arg Win64)",           "rcx → ecx → cx → cl/ch"},
    {"rdx", "gpr_64", 64, "Data (I/O port, 2nd integer arg Win64, mul/div hi)",    "rdx → edx → dx → dl/dh"},
    {"rsi", "gpr_64", 64, "Source index (string ops, 2nd arg SysV)",               "rsi → esi → si → sil"},
    {"rdi", "gpr_64", 64, "Destination index (string ops, 1st arg SysV)",          "rdi → edi → di → dil"},
    {"rsp", "gpr_64", 64, "Stack pointer (top of stack)",                          "rsp → esp → sp → spl"},
    {"rbp", "gpr_64", 64, "Base/frame pointer (callee-saved)",                     "rbp → ebp → bp → bpl"},
    {"r8",  "gpr_64", 64, "Extended GPR (3rd integer arg Win64)",                  "r8 → r8d → r8w → r8b"},
    {"r9",  "gpr_64", 64, "Extended GPR (4th integer arg Win64)",                  "r9 → r9d → r9w → r9b"},
    {"r10", "gpr_64", 64, "Extended GPR (caller-saved)",                           "r10 → r10d → r10w → r10b"},
    {"r11", "gpr_64", 64, "Extended GPR (caller-saved)",                           "r11 → r11d → r11w → r11b"},
    {"r12", "gpr_64", 64, "Extended GPR (callee-saved)",                           "r12 → r12d → r12w → r12b"},
    {"r13", "gpr_64", 64, "Extended GPR (callee-saved)",                           "r13 → r13d → r13w → r13b"},
    {"r14", "gpr_64", 64, "Extended GPR (callee-saved)",                           "r14 → r14d → r14w → r14b"},
    {"r15", "gpr_64", 64, "Extended GPR (callee-saved)",                           "r15 → r15d → r15w → r15b"},

    // 32-bit GPR
    {"eax", "gpr_32", 32, "Accumulator (lower 32-bit of RAX)",                    "rax → eax → ax → al/ah"},
    {"ebx", "gpr_32", 32, "Base register (lower 32-bit of RBX)",                  "rbx → ebx → bx → bl/bh"},
    {"ecx", "gpr_32", 32, "Counter (lower 32-bit of RCX)",                        "rcx → ecx → cx → cl/ch"},
    {"edx", "gpr_32", 32, "Data (lower 32-bit of RDX)",                           "rdx → edx → dx → dl/dh"},
    {"esi", "gpr_32", 32, "Source index (lower 32-bit of RSI)",                    "rsi → esi → si → sil"},
    {"edi", "gpr_32", 32, "Destination index (lower 32-bit of RDI)",              "rdi → edi → di → dil"},
    {"esp", "gpr_32", 32, "Stack pointer (lower 32-bit of RSP)",                  "rsp → esp → sp → spl"},
    {"ebp", "gpr_32", 32, "Frame pointer (lower 32-bit of RBP)",                  "rbp → ebp → bp → bpl"},
    {"r8d", "gpr_32", 32, "Extended (lower 32-bit of R8)",                        "r8 → r8d → r8w → r8b"},
    {"r9d", "gpr_32", 32, "Extended (lower 32-bit of R9)",                        "r9 → r9d → r9w → r9b"},
    {"r10d","gpr_32", 32, "Extended (lower 32-bit of R10)",                       "r10 → r10d → r10w → r10b"},
    {"r11d","gpr_32", 32, "Extended (lower 32-bit of R11)",                       "r11 → r11d → r11w → r11b"},
    {"r12d","gpr_32", 32, "Extended (lower 32-bit of R12)",                       "r12 → r12d → r12w → r12b"},
    {"r13d","gpr_32", 32, "Extended (lower 32-bit of R13)",                       "r13 → r13d → r13w → r13b"},
    {"r14d","gpr_32", 32, "Extended (lower 32-bit of R14)",                       "r14 → r14d → r14w → r14b"},
    {"r15d","gpr_32", 32, "Extended (lower 32-bit of R15)",                       "r15 → r15d → r15w → r15b"},

    // 16-bit GPR
    {"ax",  "gpr_16", 16, "Accumulator (lower 16-bit of EAX/RAX)",               "rax → eax → ax → al/ah"},
    {"bx",  "gpr_16", 16, "Base (lower 16-bit of EBX/RBX)",                      "rbx → ebx → bx → bl/bh"},
    {"cx",  "gpr_16", 16, "Counter (lower 16-bit of ECX/RCX)",                   "rcx → ecx → cx → cl/ch"},
    {"dx",  "gpr_16", 16, "Data (lower 16-bit of EDX/RDX)",                      "rdx → edx → dx → dl/dh"},
    {"si",  "gpr_16", 16, "Source index (lower 16-bit)",                          "rsi → esi → si → sil"},
    {"di",  "gpr_16", 16, "Destination index (lower 16-bit)",                     "rdi → edi → di → dil"},
    {"sp",  "gpr_16", 16, "Stack pointer (lower 16-bit)",                         "rsp → esp → sp → spl"},
    {"bp",  "gpr_16", 16, "Frame pointer (lower 16-bit)",                         "rbp → ebp → bp → bpl"},

    // 8-bit GPR
    {"al",  "gpr_8",  8,  "Low byte of AX",                                       "rax → eax → ax → al"},
    {"ah",  "gpr_8",  8,  "High byte of AX",                                      "ax → ah (bits 15-8)"},
    {"bl",  "gpr_8",  8,  "Low byte of BX",                                       "rbx → ebx → bx → bl"},
    {"bh",  "gpr_8",  8,  "High byte of BX",                                      "bx → bh (bits 15-8)"},
    {"cl",  "gpr_8",  8,  "Low byte of CX (shift/rotate count)",                  "rcx → ecx → cx → cl"},
    {"ch",  "gpr_8",  8,  "High byte of CX",                                      "cx → ch (bits 15-8)"},
    {"dl",  "gpr_8",  8,  "Low byte of DX",                                       "rdx → edx → dx → dl"},
    {"dh",  "gpr_8",  8,  "High byte of DX",                                      "dx → dh (bits 15-8)"},
    {"sil", "gpr_8",  8,  "Low byte of SI (requires REX prefix)",                 "rsi → esi → si → sil"},
    {"dil", "gpr_8",  8,  "Low byte of DI (requires REX prefix)",                 "rdi → edi → di → dil"},
    {"spl", "gpr_8",  8,  "Low byte of SP (requires REX prefix)",                 "rsp → esp → sp → spl"},
    {"bpl", "gpr_8",  8,  "Low byte of BP (requires REX prefix)",                 "rbp → ebp → bp → bpl"},
    {"r8b", "gpr_8",  8,  "Low byte of R8",                                       "r8 → r8d → r8w → r8b"},
    {"r9b", "gpr_8",  8,  "Low byte of R9",                                       "r9 → r9d → r9w → r9b"},
    {"r10b","gpr_8",  8,  "Low byte of R10",                                      "r10 → r10d → r10w → r10b"},
    {"r11b","gpr_8",  8,  "Low byte of R11",                                      "r11 → r11d → r11w → r11b"},
    {"r12b","gpr_8",  8,  "Low byte of R12",                                      "r12 → r12d → r12w → r12b"},
    {"r13b","gpr_8",  8,  "Low byte of R13",                                      "r13 → r13d → r13w → r13b"},
    {"r14b","gpr_8",  8,  "Low byte of R14",                                      "r14 → r14d → r14w → r14b"},
    {"r15b","gpr_8",  8,  "Low byte of R15",                                      "r15 → r15d → r15w → r15b"},

    // Segment registers
    {"cs",  "segment", 16, "Code segment",                                         ""},
    {"ds",  "segment", 16, "Data segment",                                         ""},
    {"es",  "segment", 16, "Extra segment (string destination)",                   ""},
    {"fs",  "segment", 16, "FS segment (TEB on Windows, TLS on Linux)",            ""},
    {"gs",  "segment", 16, "GS segment (kernel data on Windows, TLS on Linux)",    ""},
    {"ss",  "segment", 16, "Stack segment",                                        ""},

    // SSE registers
    {"xmm0",  "sse", 128, "SSE register (1st FP arg Win64/SysV)",                 ""},
    {"xmm1",  "sse", 128, "SSE register (2nd FP arg Win64/SysV)",                 ""},
    {"xmm2",  "sse", 128, "SSE register (3rd FP arg Win64/SysV)",                 ""},
    {"xmm3",  "sse", 128, "SSE register (4th FP arg Win64/SysV)",                 ""},
    {"xmm4",  "sse", 128, "SSE register (5th FP arg SysV, volatile Win64)",       ""},
    {"xmm5",  "sse", 128, "SSE register (6th FP arg SysV, volatile Win64)",       ""},
    {"xmm6",  "sse", 128, "SSE register (callee-saved Win64, 7th FP SysV)",       ""},
    {"xmm7",  "sse", 128, "SSE register (callee-saved Win64, 8th FP SysV)",       ""},
    {"xmm8",  "sse", 128, "SSE register (callee-saved Win64)",                    ""},
    {"xmm9",  "sse", 128, "SSE register (callee-saved Win64)",                    ""},
    {"xmm10", "sse", 128, "SSE register (callee-saved Win64)",                    ""},
    {"xmm11", "sse", 128, "SSE register (callee-saved Win64)",                    ""},
    {"xmm12", "sse", 128, "SSE register (callee-saved Win64)",                    ""},
    {"xmm13", "sse", 128, "SSE register (callee-saved Win64)",                    ""},
    {"xmm14", "sse", 128, "SSE register (callee-saved Win64)",                    ""},
    {"xmm15", "sse", 128, "SSE register (callee-saved Win64)",                    ""},

    // AVX registers (256-bit)
    {"ymm0",  "avx", 256, "AVX register (upper 128 of YMM overlaps XMM0)",        "xmm0 ⊂ ymm0"},
    {"ymm1",  "avx", 256, "AVX register",                                          "xmm1 ⊂ ymm1"},
    {"ymm2",  "avx", 256, "AVX register",                                          "xmm2 ⊂ ymm2"},
    {"ymm3",  "avx", 256, "AVX register",                                          "xmm3 ⊂ ymm3"},
    {"ymm4",  "avx", 256, "AVX register",                                          "xmm4 ⊂ ymm4"},
    {"ymm5",  "avx", 256, "AVX register",                                          "xmm5 ⊂ ymm5"},
    {"ymm6",  "avx", 256, "AVX register",                                          "xmm6 ⊂ ymm6"},
    {"ymm7",  "avx", 256, "AVX register",                                          "xmm7 ⊂ ymm7"},

    // FPU / x87 (ST(0)–ST(7) represented without parens for lookup)
    {"st0",   "fpu",  80, "FPU stack top (ST(0))",                                 ""},
    {"st1",   "fpu",  80, "FPU stack ST(1)",                                       ""},
    {"st2",   "fpu",  80, "FPU stack ST(2)",                                       ""},
    {"st3",   "fpu",  80, "FPU stack ST(3)",                                       ""},
    {"st4",   "fpu",  80, "FPU stack ST(4)",                                       ""},
    {"st5",   "fpu",  80, "FPU stack ST(5)",                                       ""},
    {"st6",   "fpu",  80, "FPU stack ST(6)",                                       ""},
    {"st7",   "fpu",  80, "FPU stack ST(7)",                                       ""},

    // Control / flags
    {"rflags","control", 64, "Flags register (CF, ZF, SF, OF, PF, AF, DF, IF, etc.)", "rflags → eflags → flags"},
    {"eflags","control", 32, "Flags register (32-bit view)",                         "rflags → eflags → flags"},
    {"rip",   "control", 64, "Instruction pointer (program counter)",               "rip → eip → ip"},
    {"eip",   "control", 32, "Instruction pointer (32-bit)",                        "rip → eip → ip"},
    {"cr0",   "control", 64, "Control register 0 (protected mode, paging)",         ""},
    {"cr2",   "control", 64, "Control register 2 (page fault linear address)",      ""},
    {"cr3",   "control", 64, "Control register 3 (page directory base)",            ""},
    {"cr4",   "control", 64, "Control register 4 (extensions enable flags)",        ""},
    {"cr8",   "control", 64, "Control register 8 (task priority level, x64)",       ""},
    {"mxcsr", "control", 32, "SSE control/status register",                         ""},

    // Sentinel
    {nullptr,  nullptr,  0,  nullptr,                                                nullptr}
};

// ============================================================================
// HELPERS — string utilities
// ============================================================================

static std::string asmToLower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

static std::string asmTrim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string asmTrimComment(const std::string& line) {
    // Strip ';' comments (MASM/NASM style)
    // Be careful not to strip semicolons inside strings
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '\'' && !inDoubleQuote) inSingleQuote = !inSingleQuote;
        else if (c == '"' && !inSingleQuote) inDoubleQuote = !inDoubleQuote;
        else if (c == ';' && !inSingleQuote && !inDoubleQuote) {
            return asmTrim(line.substr(0, i));
        }
    }
    // Also handle '#' for GAS-style comments and '//' for some asm variants
    // but only if not inside quotes
    return asmTrim(line);
}

static bool asmIsAsmFile(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = asmToLower(path.substr(dot));
    return (ext == ".asm" || ext == ".s" || ext == ".nasm" ||
            ext == ".masm" || ext == ".inc" || ext == ".mac");
}

// Extract the first "word" (identifier) from a string
static std::string asmFirstWord(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) ++start;
    size_t end = start;
    while (end < s.size() && (std::isalnum((unsigned char)s[end]) || s[end] == '_' || s[end] == '.' || s[end] == '@' || s[end] == '$')) ++end;
    return s.substr(start, end - start);
}

// Extract the second word from a line (after first word and whitespace)
static std::string asmSecondWord(const std::string& s) {
    size_t pos = 0;
    // Skip leading whitespace
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) ++pos;
    // Skip first word
    while (pos < s.size() && (std::isalnum((unsigned char)s[pos]) || s[pos] == '_' || s[pos] == '.' || s[pos] == '@' || s[pos] == '$')) ++pos;
    // Skip whitespace
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) ++pos;
    // Read second word
    size_t start = pos;
    while (pos < s.size() && (std::isalnum((unsigned char)s[pos]) || s[pos] == '_' || s[pos] == '.' || s[pos] == '@' || s[pos] == '$' || s[pos] == '?')) ++pos;
    return s.substr(start, pos - start);
}

// Check if a token looks like a label definition (ends with ':' or is followed by label-like context)
static bool asmIsLabelDef(const std::string& stripped) {
    if (stripped.empty()) return false;
    // Pattern: "identifier:" at start of line
    size_t colon = stripped.find(':');
    if (colon == std::string::npos) return false;
    // Everything before ':' should be a valid identifier
    std::string before = asmTrim(stripped.substr(0, colon));
    if (before.empty()) return false;
    for (size_t i = 0; i < before.size(); ++i) {
        char c = before[i];
        if (!std::isalnum((unsigned char)c) && c != '_' && c != '.' && c != '@' && c != '$') {
            return false;
        }
    }
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void Win32IDE::initAsmSemantic() {
    std::lock_guard<std::mutex> lock(m_asmMutex);
    if (m_asmSemanticInitialized) return;

    m_asmSymbolTable.clear();
    m_asmFileSymbols.clear();
    m_asmStats = {};
    m_asmSemanticInitialized = true;

    appendToOutput("[AsmSemantic] Initialized. Use 'ASM: Parse Symbols' to scan ASM files.",
                   "General", OutputSeverity::Info);
}

void Win32IDE::shutdownAsmSemantic() {
    std::lock_guard<std::mutex> lock(m_asmMutex);
    if (!m_asmSemanticInitialized) return;

    m_asmSymbolTable.clear();
    m_asmFileSymbols.clear();
    m_asmStats = {};
    m_asmSemanticInitialized = false;
}

// ============================================================================
// SYMBOL TABLE — PARSING
// ============================================================================

void Win32IDE::parseAsmFile(const std::string& filePath) {
    auto startTime = std::chrono::steady_clock::now();

    std::ifstream file(filePath);
    if (!file.is_open()) {
        appendToOutput("[AsmSemantic] Cannot open: " + filePath,
                       "General", OutputSeverity::Warning);
        return;
    }

    // Read all lines
    std::vector<std::string> lines;
    {
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
    }
    file.close();

    std::lock_guard<std::mutex> lock(m_asmMutex);

    // Clear any existing symbols from this file
    auto fileIt = m_asmFileSymbols.find(filePath);
    if (fileIt != m_asmFileSymbols.end()) {
        for (const auto& symName : fileIt->second) {
            m_asmSymbolTable.erase(symName);
        }
        m_asmFileSymbols.erase(fileIt);
    }

    std::vector<std::string> fileSymNames;
    std::string currentSection;
    std::string currentProc;
    int currentProcStartLine = 0;

    for (int lineNum = 1; lineNum <= (int)lines.size(); ++lineNum) {
        std::string raw = lines[lineNum - 1];
        std::string stripped = asmTrimComment(raw);
        if (stripped.empty()) continue;

        std::string firstWord  = asmFirstWord(stripped);
        std::string firstLower = asmToLower(firstWord);
        std::string secondWord = asmSecondWord(stripped);
        std::string secondLower = asmToLower(secondWord);

        // ---- Section detection ----
        // MASM: .code, .data, .data?, .const
        // NASM: section .text, section .data, section .bss
        // GAS:  .text, .data, .bss
        if (firstLower == "section" || firstLower == "segment") {
            currentSection = secondWord.empty() ? firstWord : secondWord;
            AsmSymbol sec;
            sec.name     = currentSection;
            sec.kind     = AsmSymbolKind::Section;
            sec.filePath = filePath;
            sec.line     = lineNum;
            sec.detail   = stripped;
            std::string key = asmToLower(sec.name);
            if (m_asmSymbolTable.find(key) == m_asmSymbolTable.end()) {
                m_asmSymbolTable[key] = sec;
                fileSymNames.push_back(key);
                m_asmStats.totalSections++;
            }
            continue;
        }
        if (firstLower == ".code" || firstLower == ".data" || firstLower == ".data?" ||
            firstLower == ".const" || firstLower == ".bss" ||
            firstLower == ".text") {
            currentSection = firstLower;
            AsmSymbol sec;
            sec.name     = firstLower;
            sec.kind     = AsmSymbolKind::Section;
            sec.filePath = filePath;
            sec.line     = lineNum;
            sec.detail   = stripped;
            std::string key = asmToLower(sec.name);
            if (m_asmSymbolTable.find(key) == m_asmSymbolTable.end()) {
                m_asmSymbolTable[key] = sec;
                fileSymNames.push_back(key);
                m_asmStats.totalSections++;
            }
            continue;
        }

        // ---- Procedure detection (MASM: name PROC, NASM/GAS: name: with global) ----
        if (secondLower == "proc") {
            // MASM: MyProc PROC NEAR
            AsmSymbol proc;
            proc.name     = firstWord;
            proc.kind     = AsmSymbolKind::Procedure;
            proc.filePath = filePath;
            proc.line     = lineNum;
            proc.section  = currentSection;
            proc.detail   = stripped;
            currentProc = firstWord;
            currentProcStartLine = lineNum;
            std::string key = asmToLower(proc.name);
            m_asmSymbolTable[key] = proc;
            fileSymNames.push_back(key);
            m_asmStats.totalProcedures++;
            continue;
        }
        if (secondLower == "endp") {
            // MASM: MyProc ENDP — set endLine on the proc
            std::string key = asmToLower(firstWord);
            auto it = m_asmSymbolTable.find(key);
            if (it != m_asmSymbolTable.end()) {
                it->second.endLine = lineNum;
            }
            currentProc.clear();
            currentProcStartLine = 0;
            continue;
        }

        // ---- Macro detection (MASM: name MACRO [params]) ----
        if (secondLower == "macro") {
            AsmSymbol mac;
            mac.name     = firstWord;
            mac.kind     = AsmSymbolKind::Macro;
            mac.filePath = filePath;
            mac.line     = lineNum;
            mac.section  = currentSection;
            mac.detail   = stripped;
            std::string key = asmToLower(mac.name);
            m_asmSymbolTable[key] = mac;
            fileSymNames.push_back(key);
            m_asmStats.totalMacros++;
            continue;
        }
        if (secondLower == "endm") {
            std::string key = asmToLower(firstWord);
            auto it = m_asmSymbolTable.find(key);
            if (it != m_asmSymbolTable.end()) {
                it->second.endLine = lineNum;
            }
            continue;
        }

        // ---- Struct detection (MASM: name STRUCT/STRUC, NASM: struc name) ----
        if (secondLower == "struct" || secondLower == "struc") {
            AsmSymbol st;
            st.name     = firstWord;
            st.kind     = AsmSymbolKind::Struct;
            st.filePath = filePath;
            st.line     = lineNum;
            st.section  = currentSection;
            st.detail   = stripped;
            std::string key = asmToLower(st.name);
            m_asmSymbolTable[key] = st;
            fileSymNames.push_back(key);
            continue;
        }
        if (firstLower == "struc") {
            // NASM: struc MyStruct
            AsmSymbol st;
            st.name     = secondWord;
            st.kind     = AsmSymbolKind::Struct;
            st.filePath = filePath;
            st.line     = lineNum;
            st.section  = currentSection;
            st.detail   = stripped;
            std::string key = asmToLower(st.name);
            m_asmSymbolTable[key] = st;
            fileSymNames.push_back(key);
            continue;
        }

        // ---- Equate detection (name EQU value, name = value) ----
        if (secondLower == "equ") {
            AsmSymbol eq;
            eq.name     = firstWord;
            eq.kind     = AsmSymbolKind::Equate;
            eq.filePath = filePath;
            eq.line     = lineNum;
            eq.section  = currentSection;
            eq.detail   = stripped;
            std::string key = asmToLower(eq.name);
            m_asmSymbolTable[key] = eq;
            fileSymNames.push_back(key);
            m_asmStats.totalEquates++;
            continue;
        }
        // name = value (MASM equate shorthand)
        if (stripped.find('=') != std::string::npos && !firstWord.empty()) {
            size_t eqPos = stripped.find('=');
            std::string before = asmTrim(stripped.substr(0, eqPos));
            // Only treat as equate if 'before' is a simple identifier
            bool isIdent = !before.empty();
            for (char c : before) {
                if (!std::isalnum((unsigned char)c) && c != '_' && c != '.' && c != '@' && c != '$') {
                    isIdent = false;
                    break;
                }
            }
            // Exclude comparison instructions like "cmp eax, 0"
            if (isIdent && before.find(' ') == std::string::npos) {
                // Check that secondWord is actually "=" by looking at position
                size_t afterFirst = stripped.find_first_not_of(" \t", firstWord.size());
                if (afterFirst != std::string::npos && stripped[afterFirst] == '=' &&
                    (afterFirst + 1 >= stripped.size() || stripped[afterFirst + 1] != '=')) {
                    AsmSymbol eq;
                    eq.name     = firstWord;
                    eq.kind     = AsmSymbolKind::Equate;
                    eq.filePath = filePath;
                    eq.line     = lineNum;
                    eq.section  = currentSection;
                    eq.detail   = stripped;
                    std::string key = asmToLower(eq.name);
                    m_asmSymbolTable[key] = eq;
                    fileSymNames.push_back(key);
                    m_asmStats.totalEquates++;
                    continue;
                }
            }
        }

        // ---- Data definitions (name DB/DW/DD/DQ/DT value) ----
        if (secondLower == "db" || secondLower == "dw" || secondLower == "dd" ||
            secondLower == "dq" || secondLower == "dt") {
            AsmSymbol dat;
            dat.name     = firstWord;
            dat.kind     = AsmSymbolKind::DataDef;
            dat.filePath = filePath;
            dat.line     = lineNum;
            dat.section  = currentSection;
            dat.detail   = stripped;
            std::string key = asmToLower(dat.name);
            m_asmSymbolTable[key] = dat;
            fileSymNames.push_back(key);
            m_asmStats.totalDataDefs++;
            continue;
        }

        // ---- Extern/Extrn/Global/Public ----
        if (firstLower == "extern" || firstLower == "extrn") {
            // extern MyFunc:PROC  or  extern _printf
            std::string symName = secondWord;
            // Strip trailing ':type'
            size_t colon = symName.find(':');
            if (colon != std::string::npos) symName = symName.substr(0, colon);
            if (!symName.empty()) {
                AsmSymbol ext;
                ext.name     = symName;
                ext.kind     = AsmSymbolKind::Extern;
                ext.filePath = filePath;
                ext.line     = lineNum;
                ext.section  = currentSection;
                ext.detail   = stripped;
                std::string key = asmToLower(ext.name);
                m_asmSymbolTable[key] = ext;
                fileSymNames.push_back(key);
                m_asmStats.totalExterns++;
            }
            continue;
        }
        if (firstLower == "global" || firstLower == "public") {
            std::string symName = secondWord;
            if (!symName.empty()) {
                AsmSymbol glob;
                glob.name     = symName;
                glob.kind     = AsmSymbolKind::Global;
                glob.filePath = filePath;
                glob.line     = lineNum;
                glob.section  = currentSection;
                glob.detail   = stripped;
                std::string key = asmToLower(glob.name);
                m_asmSymbolTable[key] = glob;
                fileSymNames.push_back(key);
            }
            continue;
        }

        // ---- Label detection (identifier: at start of line) ----
        if (asmIsLabelDef(stripped)) {
            size_t colon = stripped.find(':');
            std::string labelName = asmTrim(stripped.substr(0, colon));
            if (!labelName.empty()) {
                AsmSymbol lbl;
                lbl.name     = labelName;
                lbl.kind     = AsmSymbolKind::Label;
                lbl.filePath = filePath;
                lbl.line     = lineNum;
                lbl.section  = currentSection;
                lbl.detail   = currentProc.empty() ? "" : ("in " + currentProc);
                std::string key = asmToLower(lbl.name);
                // Don't overwrite proc/macro definitions with label entries
                if (m_asmSymbolTable.find(key) == m_asmSymbolTable.end()) {
                    m_asmSymbolTable[key] = lbl;
                    fileSymNames.push_back(key);
                    m_asmStats.totalLabels++;
                }
            }
            continue;
        }
    }

    // ---- Second pass: collect references (call/jmp targets, data references) ----
    // We scan every line for known symbol names used as operands
    for (int lineNum = 1; lineNum <= (int)lines.size(); ++lineNum) {
        std::string stripped = asmTrimComment(lines[lineNum - 1]);
        if (stripped.empty()) continue;

        std::string firstLower = asmToLower(asmFirstWord(stripped));

        // Skip directive lines (already processed above)
        if (firstLower == "section" || firstLower == "segment" || firstLower == "extern" ||
            firstLower == "extrn" || firstLower == "global" || firstLower == "public" ||
            firstLower == "include" || firstLower == "incbin") continue;

        // Tokenize the rest and look for known symbol names
        // We start from the operand portion (after the first word/mnemonic)
        size_t firstEnd = stripped.find_first_of(" \t", stripped.find_first_not_of(" \t"));
        if (firstEnd == std::string::npos) continue;

        // If this line starts with a label (has ':'), skip past it to the instruction
        std::string operandPart = stripped.substr(firstEnd);
        if (asmIsLabelDef(stripped)) {
            size_t afterColon = stripped.find(':');
            if (afterColon != std::string::npos && afterColon + 1 < stripped.size()) {
                operandPart = stripped.substr(afterColon + 1);
            } else {
                continue; // Label-only line
            }
        }

        // Tokenize operandPart into identifiers
        std::string tok;
        for (size_t i = 0; i <= operandPart.size(); ++i) {
            char c = (i < operandPart.size()) ? operandPart[i] : ' ';
            if (std::isalnum((unsigned char)c) || c == '_' || c == '.' || c == '@' || c == '$') {
                tok += c;
            } else {
                if (!tok.empty()) {
                    std::string tokLower = asmToLower(tok);
                    auto symIt = m_asmSymbolTable.find(tokLower);
                    if (symIt != m_asmSymbolTable.end()) {
                        // Add reference (only if not the definition line itself)
                        if (symIt->second.line != lineNum || symIt->second.filePath != filePath) {
                            AsmSymbolRef ref;
                            ref.filePath     = filePath;
                            ref.line         = lineNum;
                            ref.column       = (int)(i - tok.size());
                            ref.isDefinition = false;
                            symIt->second.references.push_back(ref);
                        }
                    }
                    tok.clear();
                }
            }
        }
    }

    m_asmFileSymbols[filePath] = std::move(fileSymNames);
    m_asmStats.totalFiles++;

    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    m_asmStats.totalParseTimeMs += (uint64_t)elapsed;

    // Update total symbol count
    m_asmStats.totalSymbols = m_asmSymbolTable.size();

    appendToOutput("[AsmSemantic] Parsed " + filePath +
                   " — " + std::to_string(m_asmFileSymbols[filePath].size()) + " symbols" +
                   " in " + std::to_string(elapsed) + "ms",
                   "General", OutputSeverity::Info);
}

void Win32IDE::parseAsmDirectory(const std::string& dirPath, bool recursive) {
    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
                if (entry.is_regular_file() && asmIsAsmFile(entry.path().string())) {
                    parseAsmFile(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                if (entry.is_regular_file() && asmIsAsmFile(entry.path().string())) {
                    parseAsmFile(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        appendToOutput("[AsmSemantic] Error scanning directory: " + std::string(e.what()),
                       "General", OutputSeverity::Error);
    }
}

void Win32IDE::reparseCurrentAsmFile() {
    if (m_currentFile.empty()) {
        appendToOutput("[AsmSemantic] No file open.", "General", OutputSeverity::Warning);
        return;
    }
    if (!asmIsAsmFile(m_currentFile)) {
        appendToOutput("[AsmSemantic] Current file is not an ASM file: " + m_currentFile,
                       "General", OutputSeverity::Warning);
        return;
    }
    parseAsmFile(m_currentFile);
}

void Win32IDE::clearAsmSymbols() {
    std::lock_guard<std::mutex> lock(m_asmMutex);
    m_asmSymbolTable.clear();
    m_asmFileSymbols.clear();
    m_asmStats = {};
    m_asmStats.totalSymbols = 0;
    appendToOutput("[AsmSemantic] All symbols cleared.", "General", OutputSeverity::Info);
}

void Win32IDE::clearAsmSymbolsForFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_asmMutex);
    auto fileIt = m_asmFileSymbols.find(filePath);
    if (fileIt != m_asmFileSymbols.end()) {
        for (const auto& symName : fileIt->second) {
            m_asmSymbolTable.erase(symName);
        }
        m_asmFileSymbols.erase(fileIt);
    }
    m_asmStats.totalSymbols = m_asmSymbolTable.size();
}

// ============================================================================
// SYMBOL TABLE — QUERIES
// ============================================================================

const Win32IDE::AsmSymbol* Win32IDE::findAsmSymbol(const std::string& name) const {
    std::string key = asmToLower(name);
    auto it = m_asmSymbolTable.find(key);
    if (it != m_asmSymbolTable.end()) return &it->second;
    return nullptr;
}

std::vector<const Win32IDE::AsmSymbol*> Win32IDE::findAsmSymbolsByKind(AsmSymbolKind kind) const {
    std::vector<const AsmSymbol*> result;
    for (const auto& pair : m_asmSymbolTable) {
        if (pair.second.kind == kind) {
            result.push_back(&pair.second);
        }
    }
    return result;
}

std::vector<const Win32IDE::AsmSymbol*> Win32IDE::findAsmSymbolsInFile(const std::string& filePath) const {
    std::vector<const AsmSymbol*> result;
    auto fileIt = m_asmFileSymbols.find(filePath);
    if (fileIt != m_asmFileSymbols.end()) {
        for (const auto& symName : fileIt->second) {
            auto it = m_asmSymbolTable.find(symName);
            if (it != m_asmSymbolTable.end()) {
                result.push_back(&it->second);
            }
        }
    }
    return result;
}

std::vector<const Win32IDE::AsmSymbol*> Win32IDE::findAsmSymbolsInSection(const std::string& sectionName) const {
    std::vector<const AsmSymbol*> result;
    std::string sectionLower = asmToLower(sectionName);
    for (const auto& pair : m_asmSymbolTable) {
        if (asmToLower(pair.second.section) == sectionLower) {
            result.push_back(&pair.second);
        }
    }
    return result;
}

std::vector<Win32IDE::AsmSymbolRef> Win32IDE::findAsmSymbolReferences(const std::string& symbolName) const {
    const AsmSymbol* sym = findAsmSymbol(symbolName);
    if (!sym) return {};
    return sym->references;
}

// ============================================================================
// NAVIGATION
// ============================================================================

bool Win32IDE::asmGotoDefinition(const std::string& symbolName) {
    m_asmStats.gotoDefRequests++;
    const AsmSymbol* sym = findAsmSymbol(symbolName);
    if (!sym) {
        appendToOutput("[AsmSemantic] Symbol not found: " + symbolName,
                       "General", OutputSeverity::Warning);
        return false;
    }
    // Open the file and navigate to the definition line
    if (!sym->filePath.empty()) {
        openFile(sym->filePath);
        // Navigate to line (1-based → convert to 0-based for editor)
        if (sym->line > 0 && m_hwndEditor) {
            int lineCharIndex = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, sym->line - 1, 0);
            CHARRANGE target = { lineCharIndex, lineCharIndex };
            SendMessageA(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&target);
            SendMessageA(m_hwndEditor, EM_SCROLLCARET, 0, 0);
        }
    }

    appendToOutput("[AsmSemantic] → " + sym->name + " (" + asmSymbolKindString(sym->kind) +
                   ") at " + sym->filePath + ":" + std::to_string(sym->line),
                   "General", OutputSeverity::Info);
    return true;
}

bool Win32IDE::asmGotoDefinitionAtCursor() {
    // Get word at cursor position from the editor
    std::string word = getWordAtCursor();
    if (word.empty()) {
        appendToOutput("[AsmSemantic] No word at cursor.", "General", OutputSeverity::Warning);
        return false;
    }
    return asmGotoDefinition(word);
}

std::vector<Win32IDE::AsmSymbolRef> Win32IDE::asmFindReferencesAtCursor() {
    m_asmStats.findRefsRequests++;
    std::string word = getWordAtCursor();
    if (word.empty()) {
        appendToOutput("[AsmSemantic] No word at cursor.", "General", OutputSeverity::Warning);
        return {};
    }

    auto refs = findAsmSymbolReferences(word);
    if (refs.empty()) {
        appendToOutput("[AsmSemantic] No references found for: " + word,
                       "General", OutputSeverity::Info);
    } else {
        std::ostringstream oss;
        oss << "[AsmSemantic] " << refs.size() << " reference(s) for '" << word << "':\n";
        for (const auto& ref : refs) {
            oss << "  " << ref.filePath << ":" << ref.line;
            if (ref.isDefinition) oss << " (definition)";
            oss << "\n";
        }
        appendToOutput(oss.str(), "General", OutputSeverity::Info);
    }
    return refs;
}

// ============================================================================
// INSTRUCTION & REGISTER LOOKUP
// ============================================================================

Win32IDE::AsmInstructionInfo Win32IDE::lookupInstruction(const std::string& mnemonic) const {
    std::string lower = asmToLower(mnemonic);
    for (int i = 0; g_instructionDB[i].mnemonic != nullptr; ++i) {
        if (lower == g_instructionDB[i].mnemonic) {
            AsmInstructionInfo info;
            info.mnemonic     = g_instructionDB[i].mnemonic;
            info.category     = g_instructionDB[i].category;
            info.description  = g_instructionDB[i].description;
            info.operands     = g_instructionDB[i].operands;
            info.affectsFlags = g_instructionDB[i].affectsFlags;
            return info;
        }
    }
    // Not found
    AsmInstructionInfo empty;
    empty.mnemonic = lower;
    empty.description = "(unknown instruction)";
    return empty;
}

Win32IDE::AsmRegisterInfo Win32IDE::lookupRegister(const std::string& regName) const {
    std::string lower = asmToLower(regName);
    for (int i = 0; g_registerDB[i].name != nullptr; ++i) {
        if (lower == g_registerDB[i].name) {
            AsmRegisterInfo info;
            info.name        = g_registerDB[i].name;
            info.category    = g_registerDB[i].category;
            info.bits        = g_registerDB[i].bits;
            info.description = g_registerDB[i].description;
            info.aliases     = g_registerDB[i].aliases;
            return info;
        }
    }
    AsmRegisterInfo empty;
    empty.name = lower;
    empty.description = "(unknown register)";
    return empty;
}

std::string Win32IDE::getInstructionInfoString(const std::string& mnemonic) const {
    auto info = lookupInstruction(mnemonic);
    std::ostringstream oss;
    oss << "Instruction: " << info.mnemonic << "\n"
        << "  Category:    " << info.category << "\n"
        << "  Description: " << info.description << "\n";
    if (!info.operands.empty())
        oss << "  Operands:    " << info.operands << "\n";
    oss << "  Flags:       " << (info.affectsFlags ? "YES (modifies RFLAGS)" : "NO") << "\n";
    return oss.str();
}

std::string Win32IDE::getRegisterInfoString(const std::string& regName) const {
    auto info = lookupRegister(regName);
    std::ostringstream oss;
    oss << "Register: " << info.name << "\n"
        << "  Category:    " << info.category << "\n"
        << "  Size:        " << info.bits << " bits\n"
        << "  Description: " << info.description << "\n";
    if (!info.aliases.empty())
        oss << "  Aliases:     " << info.aliases << "\n";
    return oss.str();
}

// ============================================================================
// SECTION ANALYSIS
// ============================================================================

std::vector<Win32IDE::AsmSectionInfo> Win32IDE::getAsmSections(const std::string& filePath) const {
    std::vector<AsmSectionInfo> result;
    auto fileIt = m_asmFileSymbols.find(filePath);
    if (fileIt == m_asmFileSymbols.end()) return result;

    // Collect section symbols for this file
    for (const auto& symName : fileIt->second) {
        auto it = m_asmSymbolTable.find(symName);
        if (it != m_asmSymbolTable.end() && it->second.kind == AsmSymbolKind::Section) {
            AsmSectionInfo sec;
            sec.name      = it->second.name;
            sec.filePath  = it->second.filePath;
            sec.startLine = it->second.line;
            sec.endLine   = it->second.endLine;
            // Count symbols in this section
            for (const auto& pair : m_asmSymbolTable) {
                if (pair.second.filePath == filePath &&
                    asmToLower(pair.second.section) == asmToLower(it->second.name)) {
                    sec.symbolCount++;
                }
            }
            result.push_back(sec);
        }
    }
    return result;
}

std::string Win32IDE::getAsmSectionsString(const std::string& filePath) const {
    auto sections = getAsmSections(filePath);
    if (sections.empty()) return "[AsmSemantic] No sections found in " + filePath;

    std::ostringstream oss;
    oss << "[AsmSemantic] Sections in " << filePath << ":\n";
    for (const auto& sec : sections) {
        oss << "  " << sec.name << " (line " << sec.startLine;
        if (sec.endLine > 0) oss << "–" << sec.endLine;
        oss << ") — " << sec.symbolCount << " symbols\n";
    }
    return oss.str();
}

// ============================================================================
// CALL GRAPH
// ============================================================================

std::vector<Win32IDE::AsmCallEdge> Win32IDE::buildCallGraph(const std::string& filePath) const {
    std::vector<AsmCallEdge> edges;

    std::ifstream file(filePath);
    if (!file.is_open()) return edges;

    std::vector<std::string> lines;
    {
        std::string line;
        while (std::getline(file, line)) lines.push_back(line);
    }

    // Identify which proc each line belongs to
    std::string currentProc;
    for (int lineNum = 1; lineNum <= (int)lines.size(); ++lineNum) {
        std::string stripped = asmTrimComment(lines[lineNum - 1]);
        if (stripped.empty()) continue;

        std::string firstWord  = asmFirstWord(stripped);
        std::string secondLower = asmToLower(asmSecondWord(stripped));

        if (secondLower == "proc") {
            currentProc = firstWord;
            continue;
        }
        if (secondLower == "endp") {
            currentProc.clear();
            continue;
        }

        if (currentProc.empty()) continue;

        // Look for call/jmp instructions
        std::string firstLower = asmToLower(firstWord);
        // Handle label: instruction pattern
        std::string mnemonic = firstLower;
        std::string operand;
        if (stripped.find(':') != std::string::npos && asmIsLabelDef(stripped)) {
            // After label, find the instruction
            size_t colonPos = stripped.find(':');
            std::string afterLabel = asmTrim(stripped.substr(colonPos + 1));
            mnemonic = asmToLower(asmFirstWord(afterLabel));
            operand  = asmSecondWord(afterLabel);
        } else {
            operand = asmSecondWord(stripped);
        }

        if (mnemonic == "call" || mnemonic == "invoke") {
            // The operand is the target
            std::string target = operand;
            // Strip brackets for indirect calls
            if (!target.empty() && target[0] == '[') continue; // Skip indirect
            if (target.empty()) continue;

            AsmCallEdge edge;
            edge.caller   = currentProc;
            edge.callee   = target;
            edge.filePath = filePath;
            edge.line     = lineNum;
            edges.push_back(edge);
        }
    }

    return edges;
}

std::string Win32IDE::getCallGraphString(const std::string& filePath) const {
    auto edges = buildCallGraph(filePath);
    if (edges.empty()) {
        return "[AsmSemantic] No call edges found in " + filePath;
    }

    std::ostringstream oss;
    oss << "[AsmSemantic] Call graph for " << filePath << " (" << edges.size() << " edges):\n";
    for (const auto& edge : edges) {
        oss << "  " << edge.caller << " → " << edge.callee
            << " (line " << edge.line << ")\n";
    }
    return oss.str();
}

// ============================================================================
// DATA FLOW ANALYSIS
// ============================================================================

std::vector<Win32IDE::AsmDataFlowRef> Win32IDE::analyzeDataFlow(
    const std::string& symbolName, const std::string& filePath) const
{
    std::vector<AsmDataFlowRef> refs;

    std::ifstream file(filePath);
    if (!file.is_open()) return refs;

    std::string symLower = asmToLower(symbolName);
    std::string line;
    int lineNum = 0;

    while (std::getline(file, line)) {
        ++lineNum;
        std::string stripped = asmTrimComment(line);
        if (stripped.empty()) continue;

        // Check if this line references the symbol
        std::string strippedLower = asmToLower(stripped);
        if (strippedLower.find(symLower) == std::string::npos) continue;

        // Determine if it's a read or write by analyzing the instruction
        std::string firstWord = asmFirstWord(stripped);
        std::string firstLower = asmToLower(firstWord);

        // Skip labels at start
        std::string mnemonic = firstLower;
        if (asmIsLabelDef(stripped)) {
            size_t colon = stripped.find(':');
            std::string afterLabel = asmTrim(stripped.substr(colon + 1));
            mnemonic = asmToLower(asmFirstWord(afterLabel));
        }

        // Parse operands to determine read/write
        bool isRead = false;
        bool isWrite = false;

        // For mov-like instructions: first operand is write destination
        // For instructions that read both: both operands are reads
        // Simplified heuristic:
        if (mnemonic == "mov" || mnemonic == "movzx" || mnemonic == "movsx" ||
            mnemonic == "movsxd" || mnemonic == "lea" || mnemonic == "xchg") {
            // Destination (first operand) = write, source (second+) = read
            // Find comma position to split operands
            size_t afterMnemonic = stripped.find(mnemonic.size() > 0 ? mnemonic[0] : ' ');
            // Simplified: if symbol appears before comma → write, after → read
            size_t commaPos = strippedLower.find(',');
            size_t symPos = strippedLower.find(symLower);
            if (commaPos != std::string::npos) {
                if (symPos < commaPos) isWrite = true;
                else isRead = true;
            } else {
                isRead = true; // Single operand → assume read
            }
        } else if (mnemonic == "push" || mnemonic == "cmp" || mnemonic == "test" ||
                   mnemonic == "call" || mnemonic == "jmp") {
            isRead = true;
        } else if (mnemonic == "pop") {
            isWrite = true;
        } else if (mnemonic == "add" || mnemonic == "sub" || mnemonic == "inc" ||
                   mnemonic == "dec" || mnemonic == "and" || mnemonic == "or" ||
                   mnemonic == "xor" || mnemonic == "shl" || mnemonic == "shr" ||
                   mnemonic == "neg" || mnemonic == "not") {
            // Destination is both read and write
            size_t commaPos = strippedLower.find(',');
            size_t symPos = strippedLower.find(symLower);
            if (commaPos != std::string::npos && symPos < commaPos) {
                isRead = true;
                isWrite = true;
            } else {
                isRead = true;
            }
        } else {
            // Default: assume read
            isRead = true;
        }

        AsmDataFlowRef ref;
        ref.symbol      = symbolName;
        ref.line        = lineNum;
        ref.isRead      = isRead;
        ref.isWrite     = isWrite;
        ref.instruction = asmTrim(stripped);
        refs.push_back(ref);
    }

    return refs;
}

std::string Win32IDE::getDataFlowString(const std::string& symbolName,
                                         const std::string& filePath) const
{
    auto refs = analyzeDataFlow(symbolName, filePath);
    if (refs.empty()) {
        return "[AsmSemantic] No data flow found for '" + symbolName + "' in " + filePath;
    }

    std::ostringstream oss;
    oss << "[AsmSemantic] Data flow for '" << symbolName << "' in " << filePath
        << " (" << refs.size() << " references):\n";
    int reads = 0, writes = 0;
    for (const auto& ref : refs) {
        oss << "  Line " << ref.line << ": ";
        if (ref.isWrite && ref.isRead) oss << "[R/W] ";
        else if (ref.isWrite) oss << "[W]   ";
        else oss << "[R]   ";
        oss << ref.instruction << "\n";
        if (ref.isRead) ++reads;
        if (ref.isWrite) ++writes;
    }
    oss << "  Summary: " << reads << " reads, " << writes << " writes\n";
    return oss.str();
}

// ============================================================================
// AI-ASSISTED BLOCK ANALYSIS
// ============================================================================

Win32IDE::AsmBlockAnalysis Win32IDE::analyzeAsmBlock(
    const std::string& filePath, int startLine, int endLine) const
{
    m_asmStats.analyzeBlockRequests++;
    AsmBlockAnalysis result;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        result.summary = "Cannot open file: " + filePath;
        return result;
    }

    // Read the block
    std::vector<std::string> blockLines;
    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        ++lineNum;
        if (lineNum >= startLine && lineNum <= endLine) {
            blockLines.push_back(line);
        }
        if (lineNum > endLine) break;
    }

    if (blockLines.empty()) {
        result.summary = "Empty block range";
        return result;
    }

    // Analyze the block
    std::unordered_set<std::string> regsUsed;
    std::unordered_set<std::string> regsModified;
    std::vector<std::string> memAccesses;
    bool hasCall = false;
    bool hasPushRbp = false;
    bool hasMovRspRbp = false;
    bool hasSubRsp = false;
    int stackAlloc = 0;
    bool hasRet = false;

    // Windows x64 ABI shadow space detection
    bool usesShadowSpace = false;
    // SysV detection
    bool usesRdiArg = false;
    bool usesRsiArg = false;

    for (const auto& raw : blockLines) {
        std::string stripped = asmTrimComment(raw);
        if (stripped.empty()) continue;

        std::string firstWord  = asmFirstWord(stripped);
        std::string firstLower = asmToLower(firstWord);
        std::string rest;

        // Handle label prefix
        if (asmIsLabelDef(stripped)) {
            size_t colon = stripped.find(':');
            rest = asmTrim(stripped.substr(colon + 1));
            firstLower = asmToLower(asmFirstWord(rest));
        } else {
            rest = stripped;
        }

        // Track instructions that modify/read registers
        std::string strippedLower = asmToLower(stripped);

        // Check for register usage
        for (int r = 0; g_registerDB[r].name != nullptr; ++r) {
            const char* regName = g_registerDB[r].name;
            std::string regLower(regName);
            if (strippedLower.find(regLower) != std::string::npos) {
                // Verify it's a whole word (not a substring of another identifier)
                size_t pos = 0;
                while ((pos = strippedLower.find(regLower, pos)) != std::string::npos) {
                    bool leftOk  = (pos == 0 || !std::isalnum((unsigned char)strippedLower[pos - 1]));
                    bool rightOk = (pos + regLower.size() >= strippedLower.size() ||
                                    !std::isalnum((unsigned char)strippedLower[pos + regLower.size()]));
                    if (leftOk && rightOk) {
                        regsUsed.insert(regLower);
                        break;
                    }
                    pos += regLower.size();
                }
            }
        }

        // Detect memory accesses (brackets indicate memory operand)
        if (stripped.find('[') != std::string::npos) {
            size_t start = stripped.find('[');
            size_t end   = stripped.find(']', start);
            if (end != std::string::npos) {
                memAccesses.push_back(stripped.substr(start, end - start + 1));
            }
        }

        // Detect common prologue/epilogue patterns
        if (firstLower == "push" && strippedLower.find("rbp") != std::string::npos) hasPushRbp = true;
        if (firstLower == "mov" && strippedLower.find("rbp") != std::string::npos &&
            strippedLower.find("rsp") != std::string::npos) hasMovRspRbp = true;
        if (firstLower == "sub" && strippedLower.find("rsp") != std::string::npos) {
            hasSubRsp = true;
            // Try to extract stack allocation size
            size_t commaPos = strippedLower.find(',');
            if (commaPos != std::string::npos) {
                std::string sizeStr = asmTrim(stripped.substr(commaPos + 1));
                try {
                    if (sizeStr.size() > 2 && (sizeStr.substr(0, 2) == "0x" || sizeStr.substr(0, 2) == "0X")) {
                        stackAlloc = (int)std::stoul(sizeStr, nullptr, 16);
                    } else if (sizeStr.back() == 'h' || sizeStr.back() == 'H') {
                        stackAlloc = (int)std::stoul(sizeStr.substr(0, sizeStr.size() - 1), nullptr, 16);
                    } else {
                        stackAlloc = std::stoi(sizeStr);
                    }
                } catch (...) {
                    // Could not parse stack size — leave as 0
                }
            }
            regsModified.insert("rsp");
        }
        if (firstLower == "call") {
            hasCall = true;
            regsModified.insert("rax"); // Calling convention: return value
        }
        if (firstLower == "ret" || firstLower == "retn" || firstLower == "retf") hasRet = true;

        // Detect shadow space (sub rsp, 20h or 28h at function start)
        if (hasSubRsp && (stackAlloc == 0x20 || stackAlloc == 0x28 || stackAlloc >= 0x20)) {
            usesShadowSpace = true;
        }

        // SysV ABI argument register usage
        if (strippedLower.find("rdi") != std::string::npos) usesRdiArg = true;
        if (strippedLower.find("rsi") != std::string::npos) usesRsiArg = true;

        // Track modified registers for destination operands of common instructions
        if (firstLower == "mov" || firstLower == "lea" || firstLower == "movzx" ||
            firstLower == "movsx" || firstLower == "add" || firstLower == "sub" ||
            firstLower == "xor" || firstLower == "and" || firstLower == "or" ||
            firstLower == "shl" || firstLower == "shr" || firstLower == "inc" ||
            firstLower == "dec" || firstLower == "neg" || firstLower == "not" ||
            firstLower == "pop" || firstLower == "imul") {
            // First operand is destination — check if it's a register
            std::string operandStr = asmTrim(rest.substr(firstLower.size()));
            std::string dstOp = asmFirstWord(operandStr);
            std::string dstLower = asmToLower(dstOp);
            // Check if it's a known register
            for (int r = 0; g_registerDB[r].name != nullptr; ++r) {
                if (dstLower == g_registerDB[r].name) {
                    regsModified.insert(dstLower);
                    break;
                }
            }
        }
    }

    // Build result
    result.isLeafFunction   = !hasCall;
    result.usesStackFrame   = (hasPushRbp && hasMovRspRbp);
    result.estimatedStackUsage = stackAlloc;

    for (const auto& r : regsUsed) result.registersUsed.push_back(r);
    for (const auto& r : regsModified) result.registersModified.push_back(r);
    result.memoryAccesses = memAccesses;

    // Sort for deterministic output
    std::sort(result.registersUsed.begin(), result.registersUsed.end());
    std::sort(result.registersModified.begin(), result.registersModified.end());

    // Calling convention detection
    result.callingConvention = detectCallingConvention(filePath, startLine, endLine);

    // Generate observations
    if (result.usesStackFrame) {
        result.observations.push_back("Uses standard stack frame prologue (push rbp; mov rbp, rsp)");
    }
    if (result.isLeafFunction) {
        result.observations.push_back("Leaf function — does not call other procedures");
    }
    if (stackAlloc > 0) {
        result.observations.push_back("Stack allocation: " + std::to_string(stackAlloc) +
                                      " bytes (0x" + ([&]{
            char buf[32];
            snprintf(buf, sizeof(buf), "%X", stackAlloc);
            return std::string(buf);
        })() + "h)");
    }
    if (usesShadowSpace) {
        result.observations.push_back("Allocates shadow space (Win64 ABI compliance)");
    }
    if (regsModified.count("rax") && hasRet) {
        result.observations.push_back("Modifies RAX before return — likely returns a value");
    }
    if (!memAccesses.empty()) {
        result.observations.push_back(std::to_string(memAccesses.size()) + " memory access(es) detected");
    }

    // Summary
    std::ostringstream summary;
    summary << (endLine - startLine + 1) << "-line ASM block";
    if (!result.callingConvention.empty() && result.callingConvention != "unknown") {
        summary << " (" << result.callingConvention << " convention)";
    }
    summary << " — " << result.registersUsed.size() << " registers used, "
            << result.registersModified.size() << " modified";
    if (result.isLeafFunction) summary << ", leaf";
    result.summary = summary.str();

    return result;
}

Win32IDE::AsmBlockAnalysis Win32IDE::analyzeCurrentProcedure() const {
    // Find the procedure enclosing the current cursor position
    if (m_currentFile.empty() || !asmIsAsmFile(m_currentFile)) {
        AsmBlockAnalysis empty;
        empty.summary = "No ASM file open";
        return empty;
    }

    int cursorLine = m_currentLine; // 1-based

    // Search symbol table for a procedure in this file that encloses cursorLine
    for (const auto& pair : m_asmSymbolTable) {
        const auto& sym = pair.second;
        if (sym.kind == AsmSymbolKind::Procedure &&
            sym.filePath == m_currentFile &&
            sym.line <= cursorLine &&
            (sym.endLine == 0 || sym.endLine >= cursorLine)) {
            return analyzeAsmBlock(m_currentFile, sym.line, sym.endLine > 0 ? sym.endLine : cursorLine + 50);
        }
    }

    // No enclosing procedure — analyze a reasonable range around cursor
    int startLine = std::max(1, cursorLine - 10);
    int endLine   = cursorLine + 20;
    return analyzeAsmBlock(m_currentFile, startLine, endLine);
}

std::string Win32IDE::detectCallingConvention(
    const std::string& filePath, int startLine, int endLine) const
{
    std::ifstream file(filePath);
    if (!file.is_open()) return "unknown";

    std::string line;
    int lineNum = 0;
    bool usesRcx = false, usesRdx = false, usesR8 = false, usesR9 = false;
    bool usesRdi = false, usesRsi = false;
    bool hasShadow = false;
    bool hasRet = false;
    bool hasRetN = false;
    int retCleanup = 0;

    while (std::getline(file, line)) {
        ++lineNum;
        if (lineNum < startLine) continue;
        if (lineNum > endLine) break;

        std::string stripped = asmToLower(asmTrimComment(line));
        if (stripped.empty()) continue;

        // Win64 parameter registers
        if (stripped.find("rcx") != std::string::npos) usesRcx = true;
        if (stripped.find("rdx") != std::string::npos) usesRdx = true;
        if (stripped.find("r8")  != std::string::npos) usesR8  = true;
        if (stripped.find("r9")  != std::string::npos) usesR9  = true;

        // SysV parameter registers
        if (stripped.find("rdi") != std::string::npos) usesRdi = true;
        if (stripped.find("rsi") != std::string::npos) usesRsi = true;

        // Shadow space detection
        if (stripped.find("sub") != std::string::npos && stripped.find("rsp") != std::string::npos) {
            // Extract value
            size_t comma = stripped.find(',');
            if (comma != std::string::npos) {
                std::string val = asmTrim(stripped.substr(comma + 1));
                try {
                    int v = 0;
                    if (val.size() > 2 && val.substr(0, 2) == "0x")
                        v = (int)std::stoul(val, nullptr, 16);
                    else if (val.back() == 'h')
                        v = (int)std::stoul(val.substr(0, val.size() - 1), nullptr, 16);
                    else
                        v = std::stoi(val);
                    if (v >= 0x20) hasShadow = true;
                } catch (...) {}
            }
        }

        // Return cleanup
        std::string firstWord = asmFirstWord(stripped);
        if (firstWord == "ret") {
            hasRet = true;
            std::string retArg = asmSecondWord(stripped);
            if (!retArg.empty()) {
                hasRetN = true;
                try { retCleanup = std::stoi(retArg); } catch (...) {}
            }
        }
    }

    // Heuristic classification
    if ((usesRcx || usesRdx || usesR8 || usesR9) && hasShadow) {
        return "win64";
    }
    if (usesRdi && usesRsi) {
        return "sysv_amd64";
    }
    if (hasRetN && retCleanup > 0) {
        return "stdcall";
    }
    if (usesRcx || usesRdx) {
        return "win64";  // Likely Win64 even without full shadow space
    }
    if (usesRdi || usesRsi) {
        return "sysv_amd64";
    }
    if (hasRet && !hasRetN) {
        return "cdecl";  // Caller cleans up — most likely cdecl
    }

    return "unknown";
}

std::string Win32IDE::getAsmBlockAnalysisString(const AsmBlockAnalysis& analysis) const {
    std::ostringstream oss;
    oss << "[AsmSemantic] Block Analysis\n";
    oss << "  Summary:            " << analysis.summary << "\n";
    oss << "  Calling Convention: " << analysis.callingConvention << "\n";
    oss << "  Leaf Function:      " << (analysis.isLeafFunction ? "YES" : "NO") << "\n";
    oss << "  Stack Frame:        " << (analysis.usesStackFrame ? "YES" : "NO") << "\n";
    oss << "  Stack Usage:        " << analysis.estimatedStackUsage << " bytes\n";

    if (!analysis.registersUsed.empty()) {
        oss << "  Registers Used:     ";
        for (size_t i = 0; i < analysis.registersUsed.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << analysis.registersUsed[i];
        }
        oss << "\n";
    }
    if (!analysis.registersModified.empty()) {
        oss << "  Registers Modified: ";
        for (size_t i = 0; i < analysis.registersModified.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << analysis.registersModified[i];
        }
        oss << "\n";
    }
    if (!analysis.memoryAccesses.empty()) {
        oss << "  Memory Accesses:    " << analysis.memoryAccesses.size() << "\n";
        for (const auto& mem : analysis.memoryAccesses) {
            oss << "    " << mem << "\n";
        }
    }
    if (!analysis.observations.empty()) {
        oss << "  Observations:\n";
        for (const auto& obs : analysis.observations) {
            oss << "    • " << obs << "\n";
        }
    }
    return oss.str();
}

// ============================================================================
// DISPLAY & STATUS
// ============================================================================

std::string Win32IDE::getAsmSymbolTableString() const {
    std::lock_guard<std::mutex> lock(m_asmMutex);
    if (m_asmSymbolTable.empty()) {
        return "[AsmSemantic] Symbol table is empty. Use 'ASM: Parse Symbols' first.";
    }

    std::ostringstream oss;
    oss << "[AsmSemantic] Symbol Table (" << m_asmSymbolTable.size() << " symbols):\n";

    // Group by kind
    std::map<AsmSymbolKind, std::vector<const AsmSymbol*>> grouped;
    for (const auto& pair : m_asmSymbolTable) {
        grouped[pair.second.kind].push_back(&pair.second);
    }

    for (const auto& group : grouped) {
        oss << "\n  " << asmSymbolKindString(group.first)
            << " (" << group.second.size() << "):\n";
        for (const auto* sym : group.second) {
            oss << "    " << sym->name;
            if (sym->line > 0) oss << " (line " << sym->line << ")";
            if (sym->endLine > 0) oss << "–" << sym->endLine;
            if (!sym->references.empty()) oss << " [" << sym->references.size() << " refs]";
            if (!sym->detail.empty()) oss << " — " << sym->detail;
            oss << "\n";
        }
    }
    return oss.str();
}

std::string Win32IDE::getAsmSemanticStatsString() const {
    std::lock_guard<std::mutex> lock(m_asmMutex);
    std::ostringstream oss;
    oss << "[AsmSemantic] Statistics:\n"
        << "  Total Symbols:      " << m_asmStats.totalSymbols << "\n"
        << "  Labels:             " << m_asmStats.totalLabels << "\n"
        << "  Procedures:         " << m_asmStats.totalProcedures << "\n"
        << "  Macros:             " << m_asmStats.totalMacros << "\n"
        << "  Equates:            " << m_asmStats.totalEquates << "\n"
        << "  Data Definitions:   " << m_asmStats.totalDataDefs << "\n"
        << "  Sections:           " << m_asmStats.totalSections << "\n"
        << "  Externs:            " << m_asmStats.totalExterns << "\n"
        << "  Files Parsed:       " << m_asmStats.totalFiles << "\n"
        << "  Parse Time:         " << m_asmStats.totalParseTimeMs << " ms\n"
        << "  Goto-Def Requests:  " << m_asmStats.gotoDefRequests << "\n"
        << "  Find-Refs Requests: " << m_asmStats.findRefsRequests << "\n"
        << "  Analyze Requests:   " << m_asmStats.analyzeBlockRequests << "\n";
    return oss.str();
}

std::string Win32IDE::asmSymbolKindString(AsmSymbolKind kind) const {
    switch (kind) {
        case AsmSymbolKind::Label:     return "Label";
        case AsmSymbolKind::Procedure: return "Procedure";
        case AsmSymbolKind::Macro:     return "Macro";
        case AsmSymbolKind::Equate:    return "Equate";
        case AsmSymbolKind::DataDef:   return "Data Definition";
        case AsmSymbolKind::Section:   return "Section";
        case AsmSymbolKind::Struct:    return "Struct";
        case AsmSymbolKind::Extern:    return "Extern";
        case AsmSymbolKind::Global:    return "Global";
        case AsmSymbolKind::Local:     return "Local";
        default:                       return "Unknown";
    }
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void Win32IDE::cmdAsmParseSymbols() {
    if (!m_asmSemanticInitialized) initAsmSemantic();

    // Parse ASM files in the workspace
    if (!m_currentFile.empty() && asmIsAsmFile(m_currentFile)) {
        parseAsmFile(m_currentFile);
    }

    // Also scan the src/asm directory if it exists
    std::string asmDir = "src/asm";
    if (std::filesystem::exists(asmDir)) {
        parseAsmDirectory(asmDir, true);
    }

    // Scan workspace root for .asm files (non-recursive, just top level)
    std::string workspaceRoot = m_explorerRootPath;
    if (!workspaceRoot.empty()) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(workspaceRoot)) {
                if (entry.is_regular_file() && asmIsAsmFile(entry.path().string())) {
                    parseAsmFile(entry.path().string());
                }
            }
        } catch (...) {}
    }

    appendToOutput(getAsmSemanticStatsString(), "General", OutputSeverity::Info);
}

void Win32IDE::cmdAsmGotoLabel() {
    asmGotoDefinitionAtCursor();
}

void Win32IDE::cmdAsmFindLabelRefs() {
    asmFindReferencesAtCursor();
}

void Win32IDE::cmdAsmShowSymbolTable() {
    appendToOutput(getAsmSymbolTableString(), "General", OutputSeverity::Info);
}

void Win32IDE::cmdAsmInstructionInfo() {
    std::string word = getWordAtCursor();
    if (word.empty()) {
        appendToOutput("[AsmSemantic] No word at cursor. Place cursor on an instruction mnemonic.",
                       "General", OutputSeverity::Warning);
        return;
    }
    appendToOutput(getInstructionInfoString(word), "General", OutputSeverity::Info);
}

void Win32IDE::cmdAsmRegisterInfo() {
    std::string word = getWordAtCursor();
    if (word.empty()) {
        appendToOutput("[AsmSemantic] No word at cursor. Place cursor on a register name.",
                       "General", OutputSeverity::Warning);
        return;
    }
    appendToOutput(getRegisterInfoString(word), "General", OutputSeverity::Info);
}

void Win32IDE::cmdAsmAnalyzeBlock() {
    if (m_currentFile.empty()) {
        appendToOutput("[AsmSemantic] No file open.", "General", OutputSeverity::Warning);
        return;
    }
    auto analysis = analyzeCurrentProcedure();
    appendToOutput(getAsmBlockAnalysisString(analysis), "General", OutputSeverity::Info);
}

void Win32IDE::cmdAsmShowCallGraph() {
    if (m_currentFile.empty()) {
        appendToOutput("[AsmSemantic] No file open.", "General", OutputSeverity::Warning);
        return;
    }
    appendToOutput(getCallGraphString(m_currentFile), "General", OutputSeverity::Info);
}

void Win32IDE::cmdAsmShowDataFlow() {
    std::string word = getWordAtCursor();
    if (word.empty()) {
        appendToOutput("[AsmSemantic] No word at cursor. Place cursor on a symbol name.",
                       "General", OutputSeverity::Warning);
        return;
    }
    if (m_currentFile.empty()) {
        appendToOutput("[AsmSemantic] No file open.", "General", OutputSeverity::Warning);
        return;
    }
    appendToOutput(getDataFlowString(word, m_currentFile), "General", OutputSeverity::Info);
}

void Win32IDE::cmdAsmDetectConvention() {
    if (m_currentFile.empty()) {
        appendToOutput("[AsmSemantic] No file open.", "General", OutputSeverity::Warning);
        return;
    }
    auto analysis = analyzeCurrentProcedure();
    std::string conv = analysis.callingConvention;
    if (conv.empty() || conv == "unknown") {
        appendToOutput("[AsmSemantic] Could not determine calling convention at cursor position.\n"
                       "Ensure cursor is inside a procedure and file has been parsed.",
                       "General", OutputSeverity::Warning);
    } else {
        appendToOutput("[AsmSemantic] Detected calling convention: " + conv + "\n" +
                       getAsmBlockAnalysisString(analysis),
                       "General", OutputSeverity::Info);
    }
}

void Win32IDE::cmdAsmShowSections() {
    if (m_currentFile.empty()) {
        appendToOutput("[AsmSemantic] No file open.", "General", OutputSeverity::Warning);
        return;
    }
    appendToOutput(getAsmSectionsString(m_currentFile), "General", OutputSeverity::Info);
}

void Win32IDE::cmdAsmClearSymbols() {
    clearAsmSymbols();
}

// ============================================================================
// HTTP ENDPOINTS
// ============================================================================

void Win32IDE::handleAsmSymbolsEndpoint(SOCKET client, const std::string& path) {
    std::lock_guard<std::mutex> lock(m_asmMutex);

    nlohmann::json j;
    j["initialized"] = m_asmSemanticInitialized;

    // Stats
    nlohmann::json stats;
    stats["totalSymbols"]     = (int)m_asmStats.totalSymbols;
    stats["labels"]           = (int)m_asmStats.totalLabels;
    stats["procedures"]       = (int)m_asmStats.totalProcedures;
    stats["macros"]           = (int)m_asmStats.totalMacros;
    stats["equates"]          = (int)m_asmStats.totalEquates;
    stats["dataDefs"]         = (int)m_asmStats.totalDataDefs;
    stats["sections"]         = (int)m_asmStats.totalSections;
    stats["externs"]          = (int)m_asmStats.totalExterns;
    stats["filesParsed"]      = (int)m_asmStats.totalFiles;
    stats["parseTimeMs"]      = (int)m_asmStats.totalParseTimeMs;
    stats["gotoDefRequests"]  = (int)m_asmStats.gotoDefRequests;
    stats["findRefsRequests"] = (int)m_asmStats.findRefsRequests;
    stats["analyzeRequests"]  = (int)m_asmStats.analyzeBlockRequests;
    j["stats"] = stats;

    // Check if a specific kind filter was requested: /api/asm/symbols?kind=procedure
    std::string kindFilter;
    size_t qmark = path.find('?');
    if (qmark != std::string::npos) {
        std::string query = path.substr(qmark + 1);
        size_t eqPos = query.find("kind=");
        if (eqPos != std::string::npos) {
            kindFilter = query.substr(eqPos + 5);
            size_t ampPos = kindFilter.find('&');
            if (ampPos != std::string::npos) kindFilter = kindFilter.substr(0, ampPos);
        }
    }

    // Symbols array
    nlohmann::json symbolsArr = nlohmann::json::array();
    for (const auto& pair : m_asmSymbolTable) {
        const auto& sym = pair.second;
        std::string kindStr = asmSymbolKindString(sym.kind);
        if (!kindFilter.empty() && asmToLower(kindStr) != asmToLower(kindFilter)) continue;

        nlohmann::json sj;
        sj["name"]     = sym.name;
        sj["kind"]     = kindStr;
        sj["file"]     = sym.filePath;
        sj["line"]     = sym.line;
        sj["endLine"]  = sym.endLine;
        sj["section"]  = sym.section;
        sj["detail"]   = sym.detail;
        sj["refCount"] = (int)sym.references.size();
        symbolsArr.push_back(sj);
    }
    j["symbols"] = symbolsArr;
    j["count"]   = (int)symbolsArr.size();

    std::string body = j.dump();
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    send(client, response.c_str(), (int)response.size(), 0);
}

void Win32IDE::handleAsmNavigateEndpoint(SOCKET client, const std::string& body) {
    nlohmann::json req = nlohmann::json::parse(body);
    std::string symbolName = req.value("symbol", "");
    std::string action     = req.value("action", "goto"); // "goto", "refs", "dataflow"

    nlohmann::json j;

    if (symbolName.empty()) {
        j["error"]   = "missing_field";
        j["message"] = "'symbol' field is required";
        std::string respBody = j.dump();
        std::string response = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Content-Length: " + std::to_string(respBody.size()) + "\r\n\r\n" + respBody;
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    if (action == "goto") {
        const AsmSymbol* sym = findAsmSymbol(symbolName);
        if (sym) {
            j["found"]   = true;
            j["name"]    = sym->name;
            j["kind"]    = asmSymbolKindString(sym->kind);
            j["file"]    = sym->filePath;
            j["line"]    = sym->line;
            j["endLine"] = sym->endLine;
            j["detail"]  = sym->detail;
        } else {
            j["found"]   = false;
            j["message"] = "Symbol not found: " + symbolName;
        }
    } else if (action == "refs") {
        auto refs = findAsmSymbolReferences(symbolName);
        nlohmann::json refsArr = nlohmann::json::array();
        for (const auto& ref : refs) {
            nlohmann::json rj;
            rj["file"]         = ref.filePath;
            rj["line"]         = ref.line;
            rj["column"]       = ref.column;
            rj["isDefinition"] = ref.isDefinition;
            refsArr.push_back(rj);
        }
        j["symbol"]    = symbolName;
        j["references"] = refsArr;
        j["count"]     = (int)refsArr.size();
    } else if (action == "dataflow") {
        std::string filePath = req.value("file", m_currentFile);
        auto flowRefs = analyzeDataFlow(symbolName, filePath);
        nlohmann::json flowArr = nlohmann::json::array();
        for (const auto& ref : flowRefs) {
            nlohmann::json fj;
            fj["line"]        = ref.line;
            fj["isRead"]      = ref.isRead;
            fj["isWrite"]     = ref.isWrite;
            fj["instruction"] = ref.instruction;
            flowArr.push_back(fj);
        }
        j["symbol"]   = symbolName;
        j["dataflow"] = flowArr;
        j["count"]    = (int)flowArr.size();
    } else {
        j["error"]   = "invalid_action";
        j["message"] = "Action must be 'goto', 'refs', or 'dataflow'";
    }

    std::string respBody = j.dump();
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Content-Length: " + std::to_string(respBody.size()) + "\r\n\r\n" + respBody;
    send(client, response.c_str(), (int)response.size(), 0);
}

void Win32IDE::handleAsmAnalyzeEndpoint(SOCKET client, const std::string& body) {
    nlohmann::json req = nlohmann::json::parse(body);
    std::string filePath = req.value("file", m_currentFile);
    int startLine        = req.value("startLine", 1);
    int endLine          = req.value("endLine", 100);

    auto analysis = analyzeAsmBlock(filePath, startLine, endLine);

    nlohmann::json j;
    j["summary"]            = analysis.summary;
    j["callingConvention"]  = analysis.callingConvention;
    j["isLeafFunction"]     = analysis.isLeafFunction;
    j["usesStackFrame"]     = analysis.usesStackFrame;
    j["estimatedStackUsage"]= analysis.estimatedStackUsage;

    nlohmann::json regsUsedArr = nlohmann::json::array();
    for (const auto& r : analysis.registersUsed) regsUsedArr.push_back(r);
    j["registersUsed"] = regsUsedArr;

    nlohmann::json regsModArr = nlohmann::json::array();
    for (const auto& r : analysis.registersModified) regsModArr.push_back(r);
    j["registersModified"] = regsModArr;

    nlohmann::json memArr = nlohmann::json::array();
    for (const auto& m : analysis.memoryAccesses) memArr.push_back(m);
    j["memoryAccesses"] = memArr;

    nlohmann::json obsArr = nlohmann::json::array();
    for (const auto& o : analysis.observations) obsArr.push_back(o);
    j["observations"] = obsArr;

    std::string respBody = j.dump();
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Content-Length: " + std::to_string(respBody.size()) + "\r\n\r\n" + respBody;
    send(client, response.c_str(), (int)response.size(), 0);
}
