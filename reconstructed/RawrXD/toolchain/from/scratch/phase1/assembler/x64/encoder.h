/*==========================================================================
 * Phase 1: x64 Instruction Encoder - Fully Reverse Engineered
 *
 * Encodes x64 instructions from parsed assembly into machine code bytes.
 * Handles the complete x64 encoding including:
 *   - REX prefix generation (W, R, X, B bits)
 *   - ModR/M byte construction
 *   - SIB byte for scaled index addressing
 *   - 8/32-bit displacement encoding
 *   - Immediate operand encoding (imm8, imm16, imm32, imm64)
 *   - RIP-relative addressing (default x64 mode)
 *   - VEX/EVEX prefix stubs for AVX/AVX-512
 *
 * Register encoding (3-bit values, extended by REX.R/B):
 *   RAX=0 RCX=1 RDX=2 RBX=3 RSP=4 RBP=5 RSI=6 RDI=7
 *   R8=0+REX.B  R9=1+REX.B ... R15=7+REX.B
 *
 * ModR/M byte layout: [mod:2][reg:3][rm:3]
 *   mod=00: [rm] or RIP-relative
 *   mod=01: [rm]+disp8
 *   mod=10: [rm]+disp32
 *   mod=11: register direct
 *
 * SIB byte layout: [scale:2][index:3][base:3]
 *   scale: 0=1x, 1=2x, 2=4x, 3=8x
 *=========================================================================*/
#ifndef X64_ENCODER_H
#define X64_ENCODER_H

#include <stdint.h>
#include <stddef.h>

/* ---- Register IDs ---- */
typedef enum {
  REG_RAX=0, REG_RCX=1, REG_RDX=2, REG_RBX=3,
  REG_RSP=4, REG_RBP=5, REG_RSI=6, REG_RDI=7,
  REG_R8=8,  REG_R9=9,  REG_R10=10, REG_R11=11,
  REG_R12=12, REG_R13=13, REG_R14=14, REG_R15=15,
  REG_NONE=0xFF
} x64_reg_t;

/* ---- Operand types ---- */
typedef enum {
  OP_NONE = 0,
  OP_REG,           /* register */
  OP_IMM,           /* immediate value */
  OP_MEM,           /* [base + index*scale + disp] */
  OP_RIP_REL,       /* [rip + disp32] */
  OP_LABEL          /* symbolic label (resolved later) */
} operand_type_t;

/* ---- Operand size ---- */
typedef enum {
  SZ_BYTE = 1,
  SZ_WORD = 2,
  SZ_DWORD = 4,
  SZ_QWORD = 8
} operand_size_t;

/* ---- Memory operand ---- */
typedef struct {
  x64_reg_t base;
  x64_reg_t index;
  uint8_t   scale;    /* 1, 2, 4, 8 */
  int32_t   disp;
  int       has_disp;
} mem_operand_t;

/* ---- Operand ---- */
typedef struct {
  operand_type_t type;
  operand_size_t size;
  union {
    x64_reg_t     reg;
    int64_t       imm;
    mem_operand_t mem;
    char          label[128];
  };
} x64_operand_t;

/* ---- Encoded instruction result ---- */
typedef struct {
  uint8_t  bytes[15];    /* max x64 instruction = 15 bytes */
  int      len;
  uint32_t reloc_offset; /* offset of relocation field within bytes */
  uint16_t reloc_type;   /* 0 = none, else REL_AMD64_* */
  char     reloc_symbol[128];
} x64_encoded_t;

/* ---- Instruction mnemonics ---- */
typedef enum {
  /* Data movement */
  MNEM_MOV, MNEM_MOVZX, MNEM_MOVSX, MNEM_LEA, MNEM_XCHG,
  MNEM_PUSH, MNEM_POP,
  /* Arithmetic */
  MNEM_ADD, MNEM_SUB, MNEM_MUL, MNEM_IMUL, MNEM_DIV, MNEM_IDIV,
  MNEM_INC, MNEM_DEC, MNEM_NEG, MNEM_NOT,
  MNEM_AND, MNEM_OR, MNEM_XOR, MNEM_TEST,
  MNEM_SHL, MNEM_SHR, MNEM_SAR, MNEM_ROL, MNEM_ROR,
  MNEM_CMP,
  /* Control flow */
  MNEM_JMP, MNEM_CALL, MNEM_RET, MNEM_INT,
  MNEM_JE, MNEM_JNE, MNEM_JZ, MNEM_JNZ,
  MNEM_JA, MNEM_JAE, MNEM_JB, MNEM_JBE,
  MNEM_JG, MNEM_JGE, MNEM_JL, MNEM_JLE,
  MNEM_JS, MNEM_JNS, MNEM_JO, MNEM_JNO,
  /* String/misc */
  MNEM_NOP, MNEM_CLC, MNEM_STC, MNEM_CLD, MNEM_STD,
  MNEM_SYSCALL, MNEM_CPUID, MNEM_RDTSC,
  MNEM_CDQ, MNEM_CQO,
  /* SSE basics */
  MNEM_MOVAPS, MNEM_MOVUPS,
  MNEM_UNKNOWN
} x64_mnemonic_t;

/* ---- API ---- */
x64_encoded_t x64_encode(x64_mnemonic_t mnem, x64_operand_t *op1, x64_operand_t *op2);
x64_mnemonic_t x64_parse_mnemonic(const char *s);
x64_reg_t x64_parse_register(const char *s, operand_size_t *out_size);
const char *x64_mnemonic_name(x64_mnemonic_t m);

#endif
