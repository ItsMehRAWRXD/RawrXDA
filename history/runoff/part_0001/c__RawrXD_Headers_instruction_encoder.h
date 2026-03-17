/*
 * RawrXD Instruction Encoder - C/C++ Header
 * Pure MASM64 x86-64 instruction encoding library
 * Compatible with: VC++, Clang, GCC (with PE linking)
 */

#ifndef INSTRUCTION_ENCODER_H
#define INSTRUCTION_ENCODER_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   TYPES AND STRUCTURES
   ============================================================================ */

/* Instruction encoder context - 96 bytes, cache-aligned */
typedef struct {
    uint8_t*  pOutput;           /* Output buffer */
    uint64_t  nCapacity;         /* Buffer size */
    uint64_t  nOffset;           /* Current write position */
    uint64_t  nLastSize;         /* Size of last instruction encoded */
    uint8_t   bREX;              /* Current REX prefix byte */
    uint8_t   bOpcode;           /* Primary opcode byte */
    uint8_t   bOpcode1;          /* Secondary opcode (2-byte opcodes) */
    uint8_t   bOpcodeLen;        /* 1 or 2 opcode bytes */
    uint8_t   bModRM;            /* ModRM byte */
    uint8_t   bSIB;              /* SIB byte */
    uint32_t  dwDisplacement;    /* 32-bit displacement */
    uint8_t   bDispLen;          /* 0=none, 1=8-bit, 4=32-bit */
    uint64_t  qImmediate;        /* 64-bit immediate */
    uint8_t   bImmLen;           /* Immediate size: 1,2,4,8 bytes */
    uint8_t   bError;            /* Error code (0=none) */
    uint8_t   _padding[33];      /* Pad to 96 bytes cache-aligned */
} ENCODER_CTX;

/* Error codes */
#define ENC_ERROR_NONE              0
#define ENC_ERROR_BUFFER_OVERFLOW   1
#define ENC_ERROR_INVALID_OPERAND   2
#define ENC_ERROR_INVALID_REG       3
#define ENC_ERROR_ENCODING_FAILED   4

/* ============================================================================
   LOW-LEVEL ENCODER API (Modular Building)
   ============================================================================ */

/* Context Management */
extern void __fastcall Encoder_Init(ENCODER_CTX* pCtx, uint8_t* pBuffer, uint64_t nCapacity);
extern void __fastcall Encoder_Reset(ENCODER_CTX* pCtx);
extern uint8_t* __fastcall Encoder_GetBuffer(ENCODER_CTX* pCtx);
extern uint64_t __fastcall Encoder_GetSize(ENCODER_CTX* pCtx);
extern uint64_t __fastcall Encoder_GetLastSize(ENCODER_CTX* pCtx);
extern uint8_t __fastcall Encoder_GetError(ENCODER_CTX* pCtx);

/* Opcode Setting */
extern void __fastcall Encoder_SetOpcode(ENCODER_CTX* pCtx, uint8_t bOpcode);
extern void __fastcall Encoder_SetOpcode2(ENCODER_CTX* pCtx, uint8_t bOpcode1, uint8_t bOpcode2);

/* REX Prefix */
extern void __fastcall Encoder_SetREX(ENCODER_CTX* pCtx, uint8_t bW, uint8_t bR, uint8_t bX, uint8_t bB);
extern void __fastcall Encoder_SetREX_W(ENCODER_CTX* pCtx);  /* REX.W for 64-bit operand */
extern void __fastcall Encoder_SetREX_R(ENCODER_CTX* pCtx);  /* REX.R for register (modrm.reg) */
extern void __fastcall Encoder_SetREX_X(ENCODER_CTX* pCtx);  /* REX.X for index register (sib.index) */
extern void __fastcall Encoder_SetREX_B(ENCODER_CTX* pCtx);  /* REX.B for base/rm register */

/* ModRM Byte - Register-to-Register */
extern void __fastcall Encoder_SetModRM_RegReg(ENCODER_CTX* pCtx, uint8_t bReg1, uint8_t bReg2);

/* ModRM Byte - Register and Memory with various addressing */
extern void __fastcall Encoder_SetModRM_RegMem_Indirect(ENCODER_CTX* pCtx, uint8_t bReg, uint8_t bBase);
extern void __fastcall Encoder_SetModRM_RegMem_BaseDiff32(ENCODER_CTX* pCtx, uint8_t bReg, uint8_t bBase, uint32_t dwDisp);
extern void __fastcall Encoder_SetModRM_RegMem_ScaledIndex(ENCODER_CTX* pCtx, uint8_t bReg, uint8_t bBase, uint8_t bIndex, uint8_t bScale);
extern void __fastcall Encoder_SetModRM_RegMem_Complex(ENCODER_CTX* pCtx, uint8_t bReg, uint8_t bBase, uint8_t bIndex, uint8_t bScale, uint32_t dwDisp);

/* SIB Byte (Scale-Index-Base) */
extern void __fastcall Encoder_SetSIB(ENCODER_CTX* pCtx, uint8_t bScale, uint8_t bIndex, uint8_t bBase);

/* Displacement */
extern void __fastcall Encoder_SetDisplacement8(ENCODER_CTX* pCtx, int8_t bDisp);
extern void __fastcall Encoder_SetDisplacement32(ENCODER_CTX* pCtx, int32_t dwDisp);

/* Immediate Value */
extern void __fastcall Encoder_SetImmediate(ENCODER_CTX* pCtx, uint64_t qValue, uint8_t bSize);
extern void __fastcall Encoder_SetImmediate64(ENCODER_CTX* pCtx, uint64_t qValue);  /* MovImm64 */
extern void __fastcall Encoder_SetImmediate32(ENCODER_CTX* pCtx, int32_t dwValue);
extern void __fastcall Encoder_SetImmediate8(ENCODER_CTX* pCtx, int8_t bValue);

/* Complete Instruction Encoding */
extern uint64_t __fastcall Encoder_EncodeInstruction(ENCODER_CTX* pCtx);

/* ============================================================================
   HIGH-LEVEL INSTRUCTION ENCODERS (15 Core x64 Instructions)
   ============================================================================ */

/* MOV Instruction Family */
extern uint64_t __fastcall Encode_MOV_R64_R64(ENCODER_CTX* pCtx, uint8_t bDest, uint8_t bSrc);
extern uint64_t __fastcall Encode_MOV_R64_IMM64(ENCODER_CTX* pCtx, uint8_t bDest, uint64_t qImm);
extern uint64_t __fastcall Encode_MOV_M64_R64(ENCODER_CTX* pCtx, uint8_t bBase, uint32_t dwDisp, uint8_t bSrc);
extern uint64_t __fastcall Encode_MOV_R64_M64(ENCODER_CTX* pCtx, uint8_t bDest, uint8_t bBase, uint32_t dwDisp);

/* Stack Instructions */
extern uint64_t __fastcall Encode_PUSH_R64(ENCODER_CTX* pCtx, uint8_t bReg);
extern uint64_t __fastcall Encode_POP_R64(ENCODER_CTX* pCtx, uint8_t bReg);

/* Control Flow */
extern uint64_t __fastcall Encode_CALL_REL32(ENCODER_CTX* pCtx, int32_t dwRel32);
extern uint64_t __fastcall Encode_RET(ENCODER_CTX* pCtx);
extern uint64_t __fastcall Encode_NOP(ENCODER_CTX* pCtx, uint8_t bLen);

/* Arithmetic & Logic */
extern uint64_t __fastcall Encode_LEA_R64_M(ENCODER_CTX* pCtx, uint8_t bDest, uint8_t bBase, uint32_t dwDisp);
extern uint64_t __fastcall Encode_ADD_R64_R64(ENCODER_CTX* pCtx, uint8_t bDest, uint8_t bSrc);
extern uint64_t __fastcall Encode_SUB_R64_IMM8(ENCODER_CTX* pCtx, uint8_t bDest, int8_t bImm);
extern uint64_t __fastcall Encode_CMP_R64_R64(ENCODER_CTX* pCtx, uint8_t bReg1, uint8_t bReg2);

/* Jumps */
extern uint64_t __fastcall Encode_JMP_REL32(ENCODER_CTX* pCtx, int32_t dwRel32);
extern uint64_t __fastcall Encode_Jcc_REL32(ENCODER_CTX* pCtx, uint8_t bCondition, int32_t dwRel32);

/* System */
extern uint64_t __fastcall Encode_SYSCALL(ENCODER_CTX* pCtx);

/* Exchange */
extern uint64_t __fastcall Encode_XCHG_R64_R64(ENCODER_CTX* pCtx, uint8_t bReg1, uint8_t bReg2);

/* ============================================================================
   UTILITY FUNCTIONS
   ============================================================================ */

extern void __fastcall Encode_ModRM_RegMem(ENCODER_CTX* pCtx, uint8_t bReg, uint8_t bBase, uint32_t dwDisp);

/* ============================================================================
   REGISTER DEFINITIONS
   ============================================================================ */

#define REG_RAX     0
#define REG_RCX     1
#define REG_RDX     2
#define REG_RBX     3
#define REG_RSP     4
#define REG_RBP     5
#define REG_RSI     6
#define REG_RDI     7
#define REG_R8      8
#define REG_R9      9
#define REG_R10     10
#define REG_R11     11
#define REG_R12     12
#define REG_R13     13
#define REG_R14     14
#define REG_R15     15

/* Condition Codes for Jcc */
#define COND_O      0   /* Overflow */
#define COND_NO     1   /* No Overflow */
#define COND_B      2   /* Below */
#define COND_NB     3   /* Not Below */
#define COND_E      4   /* Equal */
#define COND_NE     5   /* Not Equal */
#define COND_BE     6   /* Below or Equal */
#define COND_A      7   /* Above */
#define COND_S      8   /* Sign */
#define COND_NS     9   /* No Sign */
#define COND_P      10  /* Parity */
#define COND_NP     11  /* No Parity */
#define COND_L      12  /* Less */
#define COND_GE     13  /* Greater or Equal */
#define COND_LE     14  /* Less or Equal */
#define COND_G      15  /* Greater */

/* ============================================================================
   USAGE EXAMPLE
   ============================================================================ */

/*
    // Initialize encoder context
    uint8_t buffer[256];
    ENCODER_CTX ctx;
    Encoder_Init(&ctx, buffer, sizeof(buffer));

    // Encode MOV RAX, 0x1234567890ABCDEF
    Encode_MOV_R64_IMM64(&ctx, REG_RAX, 0x1234567890ABCDEF);

    // Encode SYSCALL
    Encode_SYSCALL(&ctx);

    // Get encoded bytes
    uint8_t* pEncoded = Encoder_GetBuffer(&ctx);
    uint64_t nSize = Encoder_GetSize(&ctx);

    // Use encoded bytes...
*/

#ifdef __cplusplus
}
#endif

#endif /* INSTRUCTION_ENCODER_H */
