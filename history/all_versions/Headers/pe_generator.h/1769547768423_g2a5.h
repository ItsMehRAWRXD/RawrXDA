/*
 * RawrXD PE Generator - C/C++ Header
 * Portable Executable (PE32+) generator for x86-64
 * Pure MASM64 implementation
 */

#ifndef PE_GENERATOR_H
#define PE_GENERATOR_H

#include <stdint.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   PE GENERATION FUNCTIONS
   ============================================================================ */

/* Initialize PE builder context */
extern int __fastcall PE_InitBuilder(
    void** ppContext,
    uint32_t nCodeSize,
    uint32_t nDataSize
);

/* Build DOS header (e_lfanew = 64) */
extern int __fastcall PE_BuildDosHeader(void* pContext);

/* Build PE headers (COFF + Optional Header) */
extern int __fastcall PE_BuildPeHeaders(
    void* pContext,
    uint32_t nImageBase,
    uint32_t nEntryPoint
);

/* Serialize complete PE to buffer */
extern int __fastcall PE_Serialize(
    void* pContext,
    uint8_t** ppOutput,
    uint32_t* pnSize
);

/* Write PE to disk file */
extern int __fastcall PE_WriteFile(
    const char* pszFilename,
    uint8_t* pBuffer,
    uint32_t nSize
);

/* Cleanup PE context */
extern void __fastcall PE_Cleanup(void* pContext);

/* Generate minimal PE from code buffer */
extern int __fastcall PE_GenerateSimple(
    const uint8_t* pCodeBuffer,
    uint32_t nCodeSize,
    const char* pszOutputFile,
    uint32_t nImageBase,
    uint32_t nEntryPoint
);

/* ============================================================================
   X64 ENCODER (Pure MASM Implementation)
   ============================================================================ */

typedef struct {
    uint8_t  bREX;           /* REX prefix */
    uint8_t  bOpcode;        /* Instruction opcode */
    uint8_t  bModRM;         /* ModRM byte */
    uint8_t  bSIB;           /* SIB byte */
    uint32_t dwDisplacement; /* 32-bit displacement */
    uint64_t qImmediate;     /* 64-bit immediate */
    uint8_t  bEncoded[15];   /* Encoded instruction (max 15 bytes) */
    uint8_t  nSize;          /* Actual encoded size */
} INSTRUCTION;

/* Encode MOV reg64, imm64 */
extern int __fastcall EncodeMovRegImm64(
    INSTRUCTION* pInst,
    uint8_t bReg,
    uint64_t qImm
);

/* Encode PUSH reg64 */
extern int __fastcall EncodePushReg(
    INSTRUCTION* pInst,
    uint8_t bReg
);

/* Encode POP reg64 */
extern int __fastcall EncodePopReg(
    INSTRUCTION* pInst,
    uint8_t bReg
);

/* Encode RET */
extern int __fastcall EncodeRet(INSTRUCTION* pInst);

/* Encode NOP */
extern int __fastcall EncodeNop(
    INSTRUCTION* pInst,
    uint8_t nLength
);

/* Encode SYSCALL */
extern int __fastcall EncodeSyscall(INSTRUCTION* pInst);

/* ============================================================================
   ASSEMBLER (Two-pass with Label Resolution)
   ============================================================================ */

typedef struct {
    char    szLabel[32];     /* Label name */
    uint32_t nOffset;        /* Offset in code */
} LABEL_STRUCT;

typedef struct {
    uint32_t nInstOffset;    /* Instruction offset */
    uint32_t nRefOffset;     /* Reference location */
    uint32_t nLabelIndex;    /* Target label index */
} FIXUP_STRUCT;

/* Initialize assembler */
extern int __fastcall AsmInit(
    void** ppAsm,
    uint32_t nMaxLabels,
    uint32_t nMaxFixups
);

/* Clean up assembler */
extern void __fastcall AsmCleanup(void* pAsm);

/* Add label to symbol table */
extern int __fastcall AddLabel(
    void* pAsm,
    const char* pszLabel,
    uint32_t nOffset
);

/* Find label by name */
extern int __fastcall FindLabel(
    void* pAsm,
    const char* pszLabel,
    uint32_t* pnOffset
);

/* Emit instruction bytes */
extern int __fastcall EmitByte(void* pAsm, uint8_t bValue);
extern int __fastcall EmitDword(void* pAsm, uint32_t dwValue);
extern int __fastcall EmitQword(void* pAsm, uint64_t qValue);

/* ============================================================================
   ERROR CODES
   ============================================================================ */

#define PE_SUCCESS                  0
#define PE_ERROR_INVALID_CONTEXT    1
#define PE_ERROR_BUFFER_OVERFLOW    2
#define PE_ERROR_FILE_ERROR         3
#define PE_ERROR_INVALID_PARAMETER  4
#define PE_ERROR_ENCODING_FAILED    5
#define PE_ERROR_MEMORY_FAILED      6

/* ============================================================================
   REGISTER DEFINITIONS (for encoder)
   ============================================================================ */

#define ENC_RAX     0
#define ENC_RCX     1
#define ENC_RDX     2
#define ENC_RBX     3
#define ENC_RSP     4
#define ENC_RBP     5
#define ENC_RSI     6
#define ENC_RDI     7
#define ENC_R8      8
#define ENC_R9      9
#define ENC_R10     10
#define ENC_R11     11
#define ENC_R12     12
#define ENC_R13     13
#define ENC_R14     14
#define ENC_R15     15

/* ============================================================================
   USAGE EXAMPLE - Generate Simple PE
   ============================================================================ */

/*
    // Create simple shellcode
    uint8_t code[] = {
        0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,  // MOV RAX, 1
        0x0F, 0x05                                   // SYSCALL
    };

    // Generate PE executable
    PE_GenerateSimple(code, sizeof(code), "output.exe", 0x400000, 0);
*/

#ifdef __cplusplus
}
#endif

#endif /* PE_GENERATOR_H */
