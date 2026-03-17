#pragma once

#include <cstdint>
#include <vector>
#include <cstring>

// ==============================================================================
// ENCODERCTX - Matches MASM structure layout exactly
// ==============================================================================
#define ENC_XOR_CHAIN      0x01
#define ENC_ROLLING_XOR    0x02
#define ENC_RC4_STREAM     0x03
#define ENC_AESNI_CTR      0x04
#define ENC_POLYMORPHIC    0x05
#define ENC_AVX512_MIX     0x06

#define ENC_FL_INPLACE     0x0001
#define ENC_FL_POLYMORPH   0x0002
#define ENC_FL_ENTROPY     0x0004
#define ENC_FL_ANTIEMUL    0x0008

#pragma pack(push, 1)
struct EncoderCtx {
    void*       pInput;         // Input buffer
    void*       pOutput;        // Output buffer
    uint64_t    cbSize;         // Buffer size
    void*       pKey;           // Crypto key
    uint32_t    cbKeyLen;       // Key length
    uint32_t    dwAlgorithm;    // Algorithm ID
    uint32_t    dwRounds;       // Iteration count
    uint32_t    dwFlags;        // Encoder flags
    void*       pScratch;       // 64-byte aligned scratch space
};
#pragma pack(pop)

// ==============================================================================
// C++ WRAPPER CLASS
// ==============================================================================
namespace RawrXD {

    class PolymorphicEncoder {
    public:
        PolymorphicEncoder() : m_ctx{}, m_keyBuffer(256) {
            m_ctx.dwAlgorithm = ENC_XOR_CHAIN;
            m_ctx.dwRounds = 1;
            m_ctx.dwFlags = 0;
        }

        ~PolymorphicEncoder() = default;

        // Configure encoder
        void SetAlgorithm(uint32_t algo) { m_ctx.dwAlgorithm = algo; }
        void SetFlags(uint32_t flags) { m_ctx.dwFlags = flags; }
        void SetRounds(uint32_t rounds) { m_ctx.dwRounds = rounds; }

        // Set encryption key
        void SetKey(const void* pKey, uint32_t keyLen) {
            if (keyLen > m_keyBuffer.size()) {
                m_keyBuffer.resize(keyLen);
            }
            std::memcpy(m_keyBuffer.data(), pKey, keyLen);
            m_ctx.pKey = m_keyBuffer.data();
            m_ctx.cbKeyLen = keyLen;
        }

        // Generate random key
        void GenerateKey(uint32_t keyLen) {
            if (keyLen > m_keyBuffer.size()) {
                m_keyBuffer.resize(keyLen);
            }
            // In production: use CryptGenRandom or similar
            for (uint32_t i = 0; i < keyLen; ++i) {
                m_keyBuffer[i] = static_cast<uint8_t>(rand() & 0xFF);
            }
            m_ctx.pKey = m_keyBuffer.data();
            m_ctx.cbKeyLen = keyLen;
        }

        // Encode data
        int64_t Encode(const void* pInput, void* pOutput, uint64_t size) {
            m_ctx.pInput = const_cast<void*>(pInput);
            m_ctx.pOutput = pOutput;
            m_ctx.cbSize = size;

            return Rawshell_EncodeUniversal(&m_ctx);
        }

        // Decode data (symmetric algorithms only)
        int64_t Decode(const void* pInput, void* pOutput, uint64_t size) {
            m_ctx.pInput = const_cast<void*>(pInput);
            m_ctx.pOutput = pOutput;
            m_ctx.cbSize = size;

            return Rawshell_DecodeUniversal(&m_ctx);
        }

        // Get context for direct ASM calls
        EncoderCtx* GetContext() { return &m_ctx; }

    private:
        EncoderCtx m_ctx;
        std::vector<uint8_t> m_keyBuffer;
    };

} // namespace RawrXD

// ==============================================================================
// C FUNCTION DECLARATIONS (Implemented in RawrXD_PolymorphicEncoder.asm)
// ==============================================================================
extern "C" {
    int64_t __cdecl Rawshell_EncodeUniversal(EncoderCtx* ctx);
    int64_t __cdecl Rawshell_DecodeUniversal(EncoderCtx* ctx);
    int64_t __cdecl Rawshell_GeneratePolymorphicKey(void* pBuffer, uint32_t length);
    int64_t __cdecl Rawshell_EncodePE(void* pPEBase, const char* pSectionName, EncoderCtx* ctx);
    void    __cdecl Rawshell_MutateEngine(void* pEngineCode, uint64_t size);
    void    __cdecl Rawshell_ScatterObfuscate(void* pInput, void** pOutputs, uint64_t fragmentCount, uint64_t fragmentSize);
    int64_t __cdecl Rawshell_Base64EncodeBinary(void* pInput, char* pOutput, uint64_t length);
    int64_t __cdecl Rawshell_AVX512_BulkTransform(void* pSource, void* pDest, uint64_t length, void* pMask);
}
