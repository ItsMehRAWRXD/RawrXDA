/*
 * RawrXD PE Generator & Encoder v1.0.0
 * C/C++ Header File
 * Pure MASM x64 Implementation - Zero Dependencies
 */

#ifndef RAWRXD_PE_GENERATOR_H
#define RAWRXD_PE_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * CONSTANTS
 * ==========================================================================
 */

/* PE Magic Numbers */
#define PE_MAGIC_DOS        0x5A4D
#define PE_MAGIC_NT         0x00004550
#define PE_MAGIC_OPT32      0x010B
#define PE_MAGIC_OPT64      0x020B

/* Section Characteristics */
#define SEC_CODE            0x00000020
#define SEC_INITIALIZED     0x00000040
#define SEC_UNINITIALIZED   0x00000080
#define SEC_DISCARDABLE     0x02000000
#define SEC_NOT_CACHED      0x04000000
#define SEC_NOT_PAGED       0x08000000
#define SEC_SHARED          0x10000000
#define SEC_EXECUTE         0x20000000
#define SEC_READ            0x40000000
#define SEC_WRITE           0x80000000

/* Subsystems */
#define SUBSYSTEM_NATIVE    1
#define SUBSYSTEM_WINDOWS   2
#define SUBSYSTEM_CONSOLE   3
#define SUBSYSTEM_POSIX     7
#define SUBSYSTEM_EFI       10

/* Encoding Algorithms */
#define ENC_XOR             0
#define ENC_RC4             1
#define ENC_AES128          2
#define ENC_AES256          3
#define ENC_CHACHA20        4
#define ENC_POLYMORPHIC     5

/* Relocation Types */
#define REL_BASED_HIGHLOW   3
#define REL_BASED_DIR64     10

/* ==========================================================================
 * STRUCTURES
 * ==========================================================================
 */

typedef struct _SECTION_ENTRY {
    char        szName[8];
    uint32_t    dwVirtualSize;
    uint32_t    dwVirtualAddress;
    uint32_t    dwRawSize;
    uint32_t    dwRawAddress;
    uint32_t    dwRelocAddress;
    uint32_t    dwLineNumbers;
    uint16_t    dwRelocCount;
    uint16_t    dwLineNumberCount;
    uint32_t    dwCharacteristics;
    void*       pRawData;
    uint8_t     bEncoded;
    uint8_t     bEncrypted;
} SECTION_ENTRY, *PSECTION_ENTRY;

typedef struct _IMPORT_ENTRY {
    const char* szDllName;
    uint32_t    dwDllNameHash;
    const char** pFunctionNames;
    uint32_t*   pFunctionHashes;
    uint32_t    dwFunctionCount;
    uint32_t    dwIAT_RVA;
    uint32_t    dwINT_RVA;
} IMPORT_ENTRY, *PIMPORT_ENTRY;

typedef struct _ENCODER_STATE {
    void*       pInputBuffer;
    void*       pOutputBuffer;
    uint32_t    dwBufferSize;
    void*       dwKeySchedule;
    uint8_t     bKey[32];
    uint8_t     bIV[16];
    uint32_t    dwRounds;
    uint32_t    dwAlgorithm;
} ENCODER_STATE, *PENCODER_STATE;

typedef struct _PE_GEN_CONTEXT {
    /* Output buffer */
    void*       pOutputBuffer;
    uint32_t    dwOutputSize;
    uint32_t    dwMaxSize;
    
    /* PE Headers */
    void*       pDosHeader;
    void*       pNtHeaders;
    void*       pFileHeader;
    void*       pOptionalHeader;
    void*       pSectionHeaders;
    
    /* Build state */
    uint32_t    dwNumSections;
    uint64_t    dwImageBase;
    uint32_t    dwEntryPointRVA;
    uint32_t    dwSectionAlignment;
    uint32_t    dwFileAlignment;
    uint32_t    dwHeadersSize;
    uint32_t    dwImageSize;
    
    /* Encoding settings */
    uint8_t     bEncodeSections;
    uint8_t     bEncodeImports;
    uint8_t     bPolymorphicStub;
    uint8_t     bEncryptResources;
    uint32_t    dwEncodingType;
    uint8_t     bEncryptionKey[32];
    
    /* Metadata */
    uint32_t    dwSubsystem;
    uint16_t    dwCharacteristics;
    uint8_t     bIsDLL;
} PE_GEN_CONTEXT, *PPE_GEN_CONTEXT;

/* ==========================================================================
 * PE GENERATOR API
 * ==========================================================================
 */

/**
 * Initialize PE generation context
 * @param pContext      Pointer to PE_GEN_CONTEXT structure
 * @param dwMaxSize     Maximum size for output buffer
 * @param bIsDLL        TRUE for DLL, FALSE for EXE
 * @return              TRUE on success, FALSE on failure
 */
bool __stdcall PeGenInitialize(
    PPE_GEN_CONTEXT pContext,
    uint32_t dwMaxSize,
    bool bIsDLL
);

/**
 * Create DOS and NT headers
 * @param pContext      Pointer to initialized context
 * @param pImageBase    Desired image base address
 * @param dwSubsystem   Windows subsystem (CONSOLE, WINDOWS, etc.)
 * @return              TRUE on success
 */
bool __stdcall PeGenCreateHeaders(
    PPE_GEN_CONTEXT pContext,
    uint64_t pImageBase,
    uint32_t dwSubsystem
);

/**
 * Add a section to the PE
 * @param pContext      Pointer to context
 * @param pSection      Pointer to SECTION_ENTRY
 * @return              TRUE on success
 */
bool __stdcall PeGenAddSection(
    PPE_GEN_CONTEXT pContext,
    PSECTION_ENTRY pSection
);

/**
 * Calculate PE checksum
 * @param pContext      Pointer to context
 * @return              Checksum value
 */
uint32_t __stdcall PeGenCalculateChecksum(
    PPE_GEN_CONTEXT pContext
);

/**
 * Write generated PE to file
 * @param pContext      Pointer to context
 * @param pFilePath     Output file path (ANSI string)
 * @return              TRUE on success
 */
bool __stdcall PeGenWriteToFile(
    PPE_GEN_CONTEXT pContext,
    const char* pFilePath
);

/**
 * Cleanup and free resources
 * @param pContext      Pointer to context
 * @return              TRUE on success
 */
bool __stdcall PeGenCleanup(
    PPE_GEN_CONTEXT pContext
);

/* ==========================================================================
 * ENCODER API
 * ==========================================================================
 */

/**
 * Initialize encoder with specified algorithm
 * @param pEncoderState Pointer to ENCODER_STATE
 * @param dwAlgorithm   Algorithm (ENC_XOR, ENC_RC4, etc.)
 * @return              TRUE on success
 */
bool __stdcall EncoderInitialize(
    PENCODER_STATE pEncoderState,
    uint32_t dwAlgorithm
);

/**
 * XOR encode/decode (symmetric)
 * @param pData         Data buffer
 * @param dwLength      Data length
 * @param pKey          Key buffer
 * @param dwKeyLen      Key length
 * @return              Bytes processed
 */
uint32_t __stdcall EncoderXOR(
    void* pData,
    uint32_t dwLength,
    const uint8_t* pKey,
    uint32_t dwKeyLen
);

/**
 * RC4 stream cipher
 * @param pData         Data buffer
 * @param dwLength      Data length
 * @param pKey          Key buffer
 * @param dwKeyLen      Key length
 * @return              Bytes processed
 */
uint32_t __stdcall EncoderRC4(
    void* pData,
    uint32_t dwLength,
    const uint8_t* pKey,
    uint32_t dwKeyLen
);

/**
 * AES encryption (AES-NI required)
 * @param pData         Data buffer
 * @param dwLength      Data length (must be 16-byte aligned)
 * @param pKey          128-bit or 256-bit key
 * @param bEncrypt      TRUE=encrypt, FALSE=decrypt
 * @return              Bytes processed
 */
uint32_t __stdcall EncoderAES(
    void* pData,
    uint32_t dwLength,
    const uint8_t* pKey,
    bool bEncrypt
);

/**
 * ChaCha20 stream cipher
 * @param pData         Data buffer
 * @param dwLength      Data length
 * @param pKey          256-bit key
 * @param pNonce        96-bit nonce
 * @return              Bytes processed
 */
uint32_t __stdcall EncoderChaCha20(
    void* pData,
    uint32_t dwLength,
    const uint8_t* pKey,
    const uint8_t* pNonce
);

/**
 * Generate polymorphic encoder/decoder
 * @param pContext      Context pointer
 * @param pOriginalCode Original code buffer
 * @param dwCodeSize    Code size
 * @return              Pointer to encoded buffer (RAX), size in RDX
 */
void* __stdcall EncoderPolymorphic(
    void* pContext,
    void* pOriginalCode,
    uint32_t dwCodeSize
);

/* ==========================================================================
 * UTILITY API
 * ==========================================================================
 */

/**
 * Calculate FNV-1a hash of string
 * @param pString       String to hash
 * @param dwLength      Length (0 for null-terminated)
 * @return              32-bit hash value
 */
uint32_t __stdcall HashStringFNV1a(
    const char* pString,
    uint32_t dwLength
);

/**
 * Generate cryptographically secure random bytes
 * @param pBuffer       Output buffer
 * @param dwLength      Number of bytes
 * @return              TRUE on success
 */
bool __stdcall GenerateRandomBytes(
    uint8_t* pBuffer,
    uint32_t dwLength
);

/**
 * Generate pseudo-random bytes (fast, not secure)
 * @param pBuffer       Output buffer
 * @param dwLength      Number of bytes
 * @return              Bytes written
 */
uint32_t __stdcall GeneratePseudoRandom(
    uint8_t* pBuffer,
    uint32_t dwLength
);

/**
 * Create example PE (demonstration)
 * @return              TRUE on success
 */
bool __stdcall PeGenCreateExample(void);

/* ==========================================================================
 * INLINE HELPERS
 * ==========================================================================
 */

#ifdef RAWRXD_PE_GENERATOR_IMPL

/**
 * Quick PE generation helper
 */
static inline bool QuickGeneratePE(
    const char* szOutput,
    uint8_t* pCode,
    uint32_t dwCodeSize,
    bool bIsDLL
) {
    PE_GEN_CONTEXT ctx;
    SECTION_ENTRY section = {0};
    
    if (!PeGenInitialize(&ctx, 0x200000, bIsDLL))
        return false;
    
    if (!PeGenCreateHeaders(&ctx, 0x140000000ULL, SUBSYSTEM_CONSOLE))
        return false;
    
    memcpy(section.szName, ".text", 5);
    section.dwVirtualSize = 0x1000;
    section.dwRawSize = (dwCodeSize + 0x1FF) & ~0x1FF;
    section.dwCharacteristics = SEC_CODE | SEC_EXECUTE | SEC_READ;
    section.pRawData = pCode;
    
    if (!PeGenAddSection(&ctx, &section))
        return false;
    
    bool result = PeGenWriteToFile(&ctx, szOutput);
    PeGenCleanup(&ctx);
    
    return result;
}

/**
 * Encode buffer in-place
 */
static inline bool QuickEncode(
    void* pData,
    uint32_t dwLength,
    uint32_t dwAlgorithm,
    const uint8_t* pKey,
    uint32_t dwKeyLen
) {
    ENCODER_STATE state;
    
    if (!EncoderInitialize(&state, dwAlgorithm))
        return false;
    
    switch (dwAlgorithm) {
        case ENC_XOR:
            EncoderXOR(pData, dwLength, pKey, dwKeyLen);
            return true;
        case ENC_RC4:
            EncoderRC4(pData, dwLength, pKey, dwKeyLen);
            return true;
        default:
            return false;
    }
}

#endif /* RAWRXD_PE_GENERATOR_IMPL */

/* ==========================================================================
 * VERSION INFO
 * ==========================================================================
 */

#define RAWRXD_PE_GEN_VERSION_MAJOR     1
#define RAWRXD_PE_GEN_VERSION_MINOR     0
#define RAWRXD_PE_GEN_VERSION_PATCH     0
#define RAWRXD_PE_GEN_VERSION_STRING    "1.0.0"

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_PE_GENERATOR_H */
