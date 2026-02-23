// ============================================================================
// tamper_detection.cpp — Runtime Binary Integrity / Secure Boot (Sovereign)
// ============================================================================
// Features: TamperDetection, SecureBootChain. Integrity checks and verified boot.
// Production implementation: SHA-256 via Windows CNG (bcrypt.h).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace RawrXD::Sovereign {

// ============================================================================
// SHA-256 Module Integrity Verification (Windows CNG)
// ============================================================================
// Computes SHA-256 over the module image at [base, base+size) and compares
// against expectedHash (32 bytes). Returns true only on exact match.

bool VerifyModuleIntegrity(const void* base, size_t size, const uint8_t* expectedHash) {
    if (!base || size == 0 || !expectedHash) {
        return false;
    }

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS status = 0;
    bool result = false;

    // Open SHA-256 algorithm provider
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status) || !hAlg) {
        return false;
    }

    // Query hash object size for allocation
    DWORD hashObjSize = 0;
    DWORD cbData = 0;
    status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH,
                               reinterpret_cast<PUCHAR>(&hashObjSize),
                               sizeof(hashObjSize), &cbData, 0);
    if (!BCRYPT_SUCCESS(status) || hashObjSize == 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Allocate hash object on heap (no STL allocators in patch code per convention)
    uint8_t* hashObj = static_cast<uint8_t*>(HeapAlloc(GetProcessHeap(), 0, hashObjSize));
    if (!hashObj) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Create hash instance
    status = BCryptCreateHash(hAlg, &hHash, hashObj, hashObjSize, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status) || !hHash) {
        HeapFree(GetProcessHeap(), 0, hashObj);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Feed module data in chunks to avoid large contiguous read issues
    const uint8_t* ptr = static_cast<const uint8_t*>(base);
    size_t remaining = size;
    const size_t chunkSize = 64 * 1024; // 64KB chunks

    while (remaining > 0) {
        ULONG toHash = static_cast<ULONG>(remaining > chunkSize ? chunkSize : remaining);

        // Guard against access violations in the module range
        __try {
            status = BCryptHashData(hHash, const_cast<PUCHAR>(ptr), toHash, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // Memory region inaccessible (e.g. guard page, unmapped)
            BCryptDestroyHash(hHash);
            HeapFree(GetProcessHeap(), 0, hashObj);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return false;
        }

        if (!BCRYPT_SUCCESS(status)) {
            BCryptDestroyHash(hHash);
            HeapFree(GetProcessHeap(), 0, hashObj);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return false;
        }

        ptr += toHash;
        remaining -= toHash;
    }

    // Finalize — get 32-byte SHA-256 digest
    uint8_t computedHash[32] = {};
    status = BCryptFinishHash(hHash, computedHash, 32, 0);
    if (BCRYPT_SUCCESS(status)) {
        // Constant-time comparison to prevent timing side-channels
        volatile uint8_t diff = 0;
        for (int i = 0; i < 32; ++i) {
            diff |= computedHash[i] ^ expectedHash[i];
        }
        result = (diff == 0);
    }

    // Cleanup
    BCryptDestroyHash(hHash);
    HeapFree(GetProcessHeap(), 0, hashObj);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return result;
#else
    // Non-Windows: not implemented (build with OpenSSL for POSIX)
    (void)base; (void)size; (void)expectedHash;
    return false;
#endif
}

// ============================================================================
// Secure Boot Chain Verification
// ============================================================================
// Checks Windows Secure Boot state via GetFirmwareEnvironmentVariable.
// Returns true if Secure Boot is enabled and the system booted through
// a verified chain. Falls back to false if API is unavailable or denied.

bool IsSecureBootChainVerified() {
#ifdef _WIN32
    // Query Secure Boot UEFI variable: {8BE4DF61-...} SecureBoot (BYTE: 0=off, 1=on)
    // This is the standard EFI Global Variable GUID.
    BYTE secureBootValue = 0;
    DWORD bytesReturned = 0;

    // GetFirmwareEnvironmentVariableA may fail with ERROR_INVALID_FUNCTION on
    // legacy BIOS systems or ERROR_ACCESS_DENIED if not running elevated.
    typedef DWORD (WINAPI *PFN_GetFirmwareEnvironmentVariableA)(
        LPCSTR lpName, LPCSTR lpGuid, PVOID pBuffer, DWORD nSize);

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) return false;

    auto pGetFirmwareEnv = reinterpret_cast<PFN_GetFirmwareEnvironmentVariableA>(
        GetProcAddress(hKernel32, "GetFirmwareEnvironmentVariableA"));
    if (!pGetFirmwareEnv) return false;

    bytesReturned = pGetFirmwareEnv(
        "SecureBoot",
        "{8be4df61-93ca-11d2-aa0d-00e098032b8c}",
        &secureBootValue,
        sizeof(secureBootValue));

    if (bytesReturned == 0) {
        // API not available or access denied — cannot verify
        return false;
    }

    return (secureBootValue == 1);
#else
    // POSIX: check /sys/firmware/efi/efivars/SecureBoot-* if available
    return false;
#endif
}

} // namespace RawrXD::Sovereign
