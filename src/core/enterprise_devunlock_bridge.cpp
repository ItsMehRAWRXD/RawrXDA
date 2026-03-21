// ============================================================================
// enterprise_devunlock_bridge.cpp — Enterprise_DevUnlock (C ABI)
// ============================================================================
// extern "C" int64_t Enterprise_DevUnlock() for Win32IDE when monolithic ASM that
// exported this symbol is not in ASM_KERNEL_SOURCES. Gate: RAWRXD_ENTERPRISE_DEV=1.
//
// NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace {
void enterpriseLogLine(const char* line) {
    if (!line) {
        return;
    }
#ifdef _WIN32
    ::OutputDebugStringA(line);
    ::OutputDebugStringA("\n");
#endif
    std::fputs(line, stderr);
    std::fputc('\n', stderr);
}
} // namespace

// ============================================================================
// Extern declarations — symbols provided by ASM or stubs
// ============================================================================
extern "C" {
    // These are provided by either the ASM or the stubs file
    extern int32_t g_800B_Unlocked;
    extern uint64_t g_EnterpriseFeatures;

    // Forward-declare the init function (ASM or stubs)
    int64_t Enterprise_InitLicenseSystem();

    // These are provided by the ASM module
    int32_t Enterprise_GetLicenseStatus();
}

// The stubs have a g_licenseState struct but it's internal — we use the
// public globals instead.

// ============================================================================
// MurmurHash3 Finalizer — lightweight hash for dev unlock brute-force
// ============================================================================
namespace {
    inline uint64_t fmix64(uint64_t k) {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdULL;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53ULL;
        k ^= k >> 33;
        return k;
    }

    void murmurhash3_dev(const void* key, int len, uint64_t seed,
                         uint64_t* out_h1, uint64_t* out_h2) {
        const uint8_t* data = (const uint8_t*)key;
        const int nblocks = len / 16;

        uint64_t h1 = seed;
        uint64_t h2 = seed;
        const uint64_t c1 = 0x87c37b91114253d5ULL;
        const uint64_t c2 = 0x4cf5ad432745937fULL;

        const uint64_t* blocks = (const uint64_t*)(data);
        for (int i = 0; i < nblocks; i++) {
            uint64_t k1 = blocks[i * 2 + 0];
            uint64_t k2 = blocks[i * 2 + 1];

            k1 *= c1; k1 = (k1 << 31) | (k1 >> 33); k1 *= c2; h1 ^= k1;
            h1 = (h1 << 27) | (h1 >> 37); h1 += h2; h1 = h1 * 5 + 0x52dce729;
            k2 *= c2; k2 = (k2 << 33) | (k2 >> 31); k2 *= c1; h2 ^= k2;
            h2 = (h2 << 31) | (h2 >> 33); h2 += h1; h2 = h2 * 5 + 0x38495ab5;
        }

        h1 ^= len;
        h2 ^= len;
        h1 += h2;
        h2 += h1;
        h1 = fmix64(h1);
        h2 = fmix64(h2);
        h1 += h2;
        h2 += h1;

        *out_h1 = h1;
        *out_h2 = h2;
    }
}

// ============================================================================
// Enterprise_DevUnlock — Always compiled, regardless of RAWR_HAS_MASM
// ============================================================================
// Brute-forces a license_hash such that (murmur3(hwid, hash) & 0xFF) == 0xAA.
// Only active when RAWRXD_ENTERPRISE_DEV=1 environment variable is set.
// Returns: 1 if unlocked, 0 if env not set or failed.
// ============================================================================
extern "C" int64_t Enterprise_DevUnlock() {
    // Check environment gate
    if (std::getenv("RAWRXD_ENTERPRISE_DEV") == nullptr) {
        enterpriseLogLine("[Enterprise] Dev unlock requires RAWRXD_ENTERPRISE_DEV=1");
        return 0;
    }

    // Make sure license system is initialized (provides HWID)
    Enterprise_InitLicenseSystem();

    // Get current HWID from the license system
    // We use a simple CPUID-based hash as the HWID for the brute-force
    uint64_t hwid = 0;
#ifdef _MSC_VER
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    hwid = ((uint64_t)cpuInfo[1] << 32) | (uint64_t)cpuInfo[3];
#else
    hwid = 0xDEADBEEFCAFE0000ULL;
#endif

    uint64_t payload[2] = { hwid, 0 };

    enterpriseLogLine("[Enterprise] Dev Unlock: brute-forcing license hash...");

    for (uint64_t x = 0; x < 0x100000000ULL; x++) {
        payload[1] = x;
        uint64_t h1, h2;
        murmurhash3_dev(payload, 16, 0xDA4DED42ULL, &h1, &h2);
        if ((h1 & 0xFF) == 0xAA) {
            // Success — unlock all features
            g_800B_Unlocked = 1;
            g_EnterpriseFeatures = 0xFFFFFFFFFFFFFFFFULL;

            std::cout << "[Enterprise] Dev Unlock SUCCEEDED — all features enabled\n";
            std::cout << "[Enterprise]   License hash: 0x" << std::hex << x << std::dec << "\n";
            std::cout << "[Enterprise]   HWID: 0x" << std::hex << hwid << std::dec << "\n";
            return 1;
        }
    }

    std::cout << "[Enterprise] Dev Unlock FAILED — no valid hash found\n";
    return 0;
}

#endif // RAWR_HAS_MASM
