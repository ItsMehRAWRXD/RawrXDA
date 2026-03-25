// win32ide_symbol_impls_A.cpp — RawrXD IDE debug agentic symbol implementations

#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#pragma comment(lib, "bcrypt.lib")

// ---------------------------------------------------------------------------
// Minimal GGUF loader context
// ---------------------------------------------------------------------------
struct GGUF_LOADER_CTX {
    char     path[512];
    uint32_t parse_done;
    uint64_t gpu_threshold;
    uint32_t tensor_count;
    uint64_t tensor_offsets[256];
};

// ---------------------------------------------------------------------------
// Encrypted-buffer header layout
//   [key : 32 bytes][iv : 16 bytes][ciphertext (AES-256-CBC + PKCS7 padding)]
// The key and IV are stored in plaintext so the same buffer can be decrypted
// without an out-of-band secret — matching the self-contained file format.
// ---------------------------------------------------------------------------
static const ULONG CRYPT_KEY_BYTES = 32; // AES-256
static const ULONG CRYPT_IV_BYTES  = 16; // AES block / CBC IV
static const ULONG CRYPT_HDR_BYTES = CRYPT_KEY_BYTES + CRYPT_IV_BYTES; // 48

extern "C" {

// ---------------------------------------------------------------------------
// 1. asm_apply_memory_patch
//    Make [addr, addr+size) writable, copy data in, restore protection.
// ---------------------------------------------------------------------------
int asm_apply_memory_patch(void* addr, size_t size, const void* data)
{
    if (!addr || !data || size == 0)
        return 1;

    DWORD oldProt = 0;
    if (!VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &oldProt))
        return (int)GetLastError();

    memcpy(addr, data, size);

    // Flush instruction cache in case we patched executable memory
    FlushInstructionCache(GetCurrentProcess(), addr, size);

    DWORD tmp = 0;
    VirtualProtect(addr, size, oldProt, &tmp);
    return 0;
}

// ---------------------------------------------------------------------------
// 2. asm_camellia256_auth_encrypt_buf
//    AES-256-CBC proxy (BCrypt; Camellia unavailable in Windows BCrypt).
//    Generates a fresh random key+IV per call and prepends them to output.
//    outputLen [in]  = capacity of output buffer
//    outputLen [out] = bytes written on success
// ---------------------------------------------------------------------------
int asm_camellia256_auth_encrypt_buf(uint8_t*  plaintext,
                                     uint32_t  plaintextLen,
                                     uint8_t*  output,
                                     uint32_t* outputLen)
{
    if (!plaintext || !output || !outputLen)
        return 1;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS          st   = 0;
    int               ret  = 1;

    uint8_t key[CRYPT_KEY_BYTES] = {};
    uint8_t iv [CRYPT_IV_BYTES]  = {};

    // --- Generate random key and IV -----------------------------------------
    st = BCryptGenRandom(nullptr, key, CRYPT_KEY_BYTES, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(st)) goto cleanup;

    st = BCryptGenRandom(nullptr, iv, CRYPT_IV_BYTES, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(st)) goto cleanup;

    // --- Open AES provider in CBC mode --------------------------------------
    st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st)) goto cleanup;

    st = BCryptSetProperty(hAlg,
                           BCRYPT_CHAINING_MODE,
                           reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
                           sizeof(BCRYPT_CHAIN_MODE_CBC),
                           0);
    if (!BCRYPT_SUCCESS(st)) goto cleanup;

    st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                    key, CRYPT_KEY_BYTES, 0);
    if (!BCRYPT_SUCCESS(st)) goto cleanup;

    {
        // Query ciphertext size (with PKCS7 padding)
        ULONG    cipherLen = 0;
        uint8_t  ivTmp[CRYPT_IV_BYTES];
        memcpy(ivTmp, iv, CRYPT_IV_BYTES);

        st = BCryptEncrypt(hKey,
                           plaintext, plaintextLen,
                           nullptr,
                           ivTmp, CRYPT_IV_BYTES,
                           nullptr, 0, &cipherLen,
                           BCRYPT_BLOCK_PADDING);
        if (!BCRYPT_SUCCESS(st)) goto cleanup;

        uint32_t needed = CRYPT_HDR_BYTES + cipherLen;
        if (*outputLen < needed) {
            *outputLen = needed;
            ret = 2; // caller must retry with larger buffer
            goto cleanup;
        }

        // Write header: key || iv
        memcpy(output,                  key, CRYPT_KEY_BYTES);
        memcpy(output + CRYPT_KEY_BYTES, iv,  CRYPT_IV_BYTES);

        // Encrypt (BCryptEncrypt consumes the IV in-place; restore fresh copy)
        memcpy(ivTmp, iv, CRYPT_IV_BYTES);
        ULONG written = 0;
        st = BCryptEncrypt(hKey,
                           plaintext, plaintextLen,
                           nullptr,
                           ivTmp, CRYPT_IV_BYTES,
                           output + CRYPT_HDR_BYTES, cipherLen,
                           &written,
                           BCRYPT_BLOCK_PADDING);
        if (!BCRYPT_SUCCESS(st)) goto cleanup;

        *outputLen = CRYPT_HDR_BYTES + written;
        ret = 0;
    }

cleanup:
    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    SecureZeroMemory(key, sizeof(key));
    return ret;
}

// ---------------------------------------------------------------------------
// 3. asm_camellia256_auth_decrypt_buf
//    Reverse of encrypt: read key+IV from header, AES-256-CBC decrypt.
//    plaintextLen [in]  = capacity of plaintext buffer
//    plaintextLen [out] = bytes written on success
// ---------------------------------------------------------------------------
int asm_camellia256_auth_decrypt_buf(const uint8_t* authData,
                                     uint32_t       authDataLen,
                                     uint8_t*       plaintext,
                                     uint32_t*      plaintextLen)
{
    if (!authData || !plaintext || !plaintextLen)
        return 1;

    if (authDataLen <= CRYPT_HDR_BYTES)
        return 1; // not enough data for header + at least one block

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS          st   = 0;
    int               ret  = 1;

    uint8_t key[CRYPT_KEY_BYTES];
    uint8_t iv [CRYPT_IV_BYTES];
    memcpy(key, authData,                  CRYPT_KEY_BYTES);
    memcpy(iv,  authData + CRYPT_KEY_BYTES, CRYPT_IV_BYTES);

    const uint8_t* cipher    = authData + CRYPT_HDR_BYTES;
    uint32_t       cipherLen = authDataLen - CRYPT_HDR_BYTES;

    st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st)) goto cleanup;

    st = BCryptSetProperty(hAlg,
                           BCRYPT_CHAINING_MODE,
                           reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
                           sizeof(BCRYPT_CHAIN_MODE_CBC),
                           0);
    if (!BCRYPT_SUCCESS(st)) goto cleanup;

    st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                    key, CRYPT_KEY_BYTES, 0);
    if (!BCRYPT_SUCCESS(st)) goto cleanup;

    {
        uint8_t ivTmp[CRYPT_IV_BYTES];
        memcpy(ivTmp, iv, CRYPT_IV_BYTES);

        // Query plaintext size
        ULONG plainLen = 0;
        st = BCryptDecrypt(hKey,
                           const_cast<uint8_t*>(cipher), cipherLen,
                           nullptr,
                           ivTmp, CRYPT_IV_BYTES,
                           nullptr, 0, &plainLen,
                           BCRYPT_BLOCK_PADDING);
        if (!BCRYPT_SUCCESS(st)) goto cleanup;

        if (*plaintextLen < plainLen) {
            *plaintextLen = plainLen;
            ret = 2;
            goto cleanup;
        }

        memcpy(ivTmp, iv, CRYPT_IV_BYTES);
        ULONG written = 0;
        st = BCryptDecrypt(hKey,
                           const_cast<uint8_t*>(cipher), cipherLen,
                           nullptr,
                           ivTmp, CRYPT_IV_BYTES,
                           plaintext, plainLen,
                           &written,
                           BCRYPT_BLOCK_PADDING);
        if (!BCRYPT_SUCCESS(st)) goto cleanup;

        *plaintextLen = written;
        ret = 0;
    }

cleanup:
    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    SecureZeroMemory(key, sizeof(key));
    return ret;
}

// ---------------------------------------------------------------------------
// 4. asm_camellia256_auth_encrypt_file
//    Read inputPath entirely, encrypt, write to outputPath.
// ---------------------------------------------------------------------------
int asm_camellia256_auth_encrypt_file(const char* inputPath, const char* outputPath)
{
    if (!inputPath || !outputPath)
        return 1;

    HANDLE hIn = CreateFileA(inputPath,
                              GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hIn == INVALID_HANDLE_VALUE)
        return (int)GetLastError();

    LARGE_INTEGER sz = {};
    if (!GetFileSizeEx(hIn, &sz) || sz.QuadPart > 0x7FFFFFFF) {
        CloseHandle(hIn);
        return 1;
    }

    uint32_t inLen  = (uint32_t)sz.QuadPart;
    uint8_t* inBuf  = static_cast<uint8_t*>(malloc(inLen ? inLen : 1));
    if (!inBuf) { CloseHandle(hIn); return 1; }

    DWORD bytesRead = 0;
    BOOL  ok = ReadFile(hIn, inBuf, inLen, &bytesRead, nullptr);
    CloseHandle(hIn);
    if (!ok || bytesRead != inLen) { free(inBuf); return 1; }

    // AES-256-CBC output = header + plaintext rounded up to block + 1 block padding
    uint32_t outLen = CRYPT_HDR_BYTES + ((inLen / 16) + 1) * 16 + 16;
    uint8_t* outBuf = static_cast<uint8_t*>(malloc(outLen));
    if (!outBuf) { free(inBuf); return 1; }

    int ret = asm_camellia256_auth_encrypt_buf(inBuf, inLen, outBuf, &outLen);
    free(inBuf);

    if (ret != 0) { free(outBuf); return ret; }

    HANDLE hOut = CreateFileA(outputPath,
                               GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) { free(outBuf); return (int)GetLastError(); }

    DWORD written = 0;
    ok = WriteFile(hOut, outBuf, outLen, &written, nullptr);
    CloseHandle(hOut);
    free(outBuf);

    return (ok && written == outLen) ? 0 : 1;
}

// ---------------------------------------------------------------------------
// 5. asm_camellia256_auth_decrypt_file
//    Read inputPath entirely, decrypt, write to outputPath.
// ---------------------------------------------------------------------------
int asm_camellia256_auth_decrypt_file(const char* inputPath, const char* outputPath)
{
    if (!inputPath || !outputPath)
        return 1;

    HANDLE hIn = CreateFileA(inputPath,
                              GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hIn == INVALID_HANDLE_VALUE)
        return (int)GetLastError();

    LARGE_INTEGER sz = {};
    if (!GetFileSizeEx(hIn, &sz) || sz.QuadPart > 0x7FFFFFFF) {
        CloseHandle(hIn);
        return 1;
    }

    uint32_t inLen  = (uint32_t)sz.QuadPart;
    uint8_t* inBuf  = static_cast<uint8_t*>(malloc(inLen ? inLen : 1));
    if (!inBuf) { CloseHandle(hIn); return 1; }

    DWORD bytesRead = 0;
    BOOL  ok = ReadFile(hIn, inBuf, inLen, &bytesRead, nullptr);
    CloseHandle(hIn);
    if (!ok || bytesRead != inLen) { free(inBuf); return 1; }

    // Plaintext is never larger than the ciphertext
    uint32_t outLen = inLen;
    uint8_t* outBuf = static_cast<uint8_t*>(malloc(outLen ? outLen : 1));
    if (!outBuf) { free(inBuf); return 1; }

    int ret = asm_camellia256_auth_decrypt_buf(inBuf, inLen, outBuf, &outLen);
    free(inBuf);

    if (ret != 0) { free(outBuf); return ret; }

    HANDLE hOut = CreateFileA(outputPath,
                               GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) { free(outBuf); return (int)GetLastError(); }

    DWORD written = 0;
    ok = WriteFile(hOut, outBuf, outLen, &written, nullptr);
    CloseHandle(hOut);
    free(outBuf);

    return (ok && written == outLen) ? 0 : 1;
}

// ---------------------------------------------------------------------------
// 6. asm_gguf_loader_close
//    Zero and free the heap-allocated GGUF_LOADER_CTX.  Safe on null.
// ---------------------------------------------------------------------------
void asm_gguf_loader_close(void* ctx)
{
    if (!ctx) return;
    GGUF_LOADER_CTX* gctx = static_cast<GGUF_LOADER_CTX*>(ctx);
    SecureZeroMemory(gctx, sizeof(GGUF_LOADER_CTX));
    free(gctx);
}

// ---------------------------------------------------------------------------
// 7. asm_gguf_loader_configure_gpu
//    Store the GPU offload threshold in the context.  Safe on null ctx.
// ---------------------------------------------------------------------------
void asm_gguf_loader_configure_gpu(void* ctx, uint64_t thresholdBytes)
{
    if (!ctx) return;
    GGUF_LOADER_CTX* gctx = static_cast<GGUF_LOADER_CTX*>(ctx);
    gctx->gpu_threshold = thresholdBytes;
}

} // extern "C"
