// ============================================================================
// enterprise_license_stubs.cpp — Stub implementations for ASM license symbols
// ============================================================================
// When the MASM Enterprise License ASM modules are not linked (no .obj),
// these stubs provide safe fallback implementations that:
//   - Always report "Community Edition" (no enterprise features)
//   - Return safe default values
//   - Never crash
//
// Once the ASM modules are assembled and linked, these stubs will be
// overridden by the real implementations via linker precedence.
// ============================================================================

#include <cstdint>
#include <cstddef>

extern "C" {

// --- RawrXD_EnterpriseLicense.asm stubs ---

int64_t Enterprise_InitLicenseSystem()                               { return 0; }
int64_t Enterprise_ValidateLicense()                                 { return -1; }  // Not valid
int32_t Enterprise_CheckFeature(uint64_t /*featureMask*/)            { return 0; }   // No features
int32_t Enterprise_Unlock800BDualEngine()                            { return 0; }   // Denied
int64_t Enterprise_InstallLicense(const void*, uint64_t, const void*){ return -1; }  // Fail
int32_t Enterprise_GetLicenseStatus()                                { return 0; }   // Community
int64_t Enterprise_GetFeatureString(char* buf, uint64_t bufSize) {
    const char* msg = "Community Edition";
    if (buf && bufSize > 0) {
        size_t len = 17;
        if (len >= bufSize) len = bufSize - 1;
        for (size_t i = 0; i < len; i++) buf[i] = msg[i];
        buf[len] = '\0';
        return (int64_t)len;
    }
    return 0;
}
uint64_t Enterprise_GenerateHardwareHash()                           { return 0; }
int32_t  Enterprise_RuntimeIntegrityCheck()                          { return 1; }   // Clean
void     Enterprise_Shutdown()                                       { }

int32_t Titan_CheckEnterpriseUnlock()                                { return 0; }   // Community
int32_t Streaming_CheckEnterpriseBudget(uint64_t /*requestedSize*/)  { return 1; }   // Allow

// --- RawrXD_License_Shield.asm stubs ---

int32_t  IsDebuggerPresent_Native()                                  { return 0; }   // Clean
int32_t  Shield_TimingCheck()                                        { return 1; }   // Clean
uint64_t Shield_GenerateHWID()                                       { return 0; }
int32_t  Shield_VerifyIntegrity()                                    { return 1; }   // Clean
void*    Shield_AES_DecryptShim(const void*)                         { return nullptr; }
void*    Unlock_800B_Kernel(const void*, const void*)                { return nullptr; }
int32_t  Shield_InitializeDefense()                                  { return 1; }   // Pass
int32_t  Shield_CheckHeapFlags()                                     { return 1; }   // Clean
int32_t  Shield_CheckKernelDebug()                                   { return 1; }   // Clean
uint64_t Shield_MurmurHash3_x64(const void*, uint64_t, uint64_t)    { return 0; }
void*    Shield_DecryptKernelEntry(const void*)                      { return nullptr; }

// --- Global flags ---
int32_t  g_800B_Unlocked     = 0;
uint64_t g_EnterpriseFeatures = 0;

// --- FlashAttention_AVX512.asm stubs ---
// When the AVX-512 ASM kernel is not linked, these stubs provide safe
// fallback implementations that report AVX-512 as unavailable.
// With MSVC, the real ASM objects are linked so stubs are excluded.
#ifndef _MSC_VER
int32_t FlashAttention_CheckAVX512()                                 { return 0; }  // Not available
int32_t FlashAttention_Init()                                        { return 0; }  // Not available
int32_t FlashAttention_Forward(void* /*cfg*/)                        { return -1; } // Error
int32_t FlashAttention_GetTileConfig(void* out) {
    if (out) {
        auto* p = reinterpret_cast<int32_t*>(out);
        p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
    }
    return 1;
}
uint64_t g_FlashAttnCalls = 0;
uint64_t g_FlashAttnTiles = 0;
#endif // !_MSC_VER

} // extern "C"
