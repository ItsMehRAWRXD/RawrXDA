// ============================================================================
// enterprise_license.h — C++ Bridge to MASM Enterprise License System
// ============================================================================
// Provides extern "C" declarations for ASM-exported license functions and
// a C++ wrapper class for integration with StreamingEngineRegistry.
//
// ASM Sources:
//   src/asm/RawrXD_EnterpriseLicense.asm — Core license logic
//   src/asm/RawrXD_License_Shield.asm    — Anti-tamper & kernel decrypt
//
// Architecture:
//   License_Init()
//    └─ Enterprise_InitLicenseSystem()   [ASM]
//        ├─ CryptoAPI provider init
//        ├─ RSA public key import
//        ├─ Shield_GenerateHWID()        [ASM]
//        ├─ Registry license load
//        └─ Enterprise_ValidateLicense() [ASM]
//             ├─ Magic / version / expiry
//             ├─ Hardware fingerprint
//             └─ RSA-4096 signature verify
//
//   Engine registration gate:
//    StreamingEngineRegistry::registerBuiltinEngines()
//     └─ if (EnterpriseLicense::isFeatureEnabled(FEATURE_800B))
//            registerDualEngine800B()
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <mutex>

namespace RawrXD {

// ============================================================================
// Feature Bitmasks (must match RawrXD_Common.inc)
// ============================================================================
namespace LicenseFeature {
    constexpr uint64_t DualEngine800B     = 0x00000001;
    constexpr uint64_t AVX512Premium      = 0x00000002;
    constexpr uint64_t DistributedSwarm   = 0x00000004;
    constexpr uint64_t GPUQuant4Bit       = 0x00000008;
    constexpr uint64_t EnterpriseSupport  = 0x00000010;
    constexpr uint64_t UnlimitedContext   = 0x00000020;
    constexpr uint64_t FlashAttention     = 0x00000040;
    constexpr uint64_t MultiGPU           = 0x00000080;

    // Combined masks for tier checks
    constexpr uint64_t CommunityFeatures  = 0;
    constexpr uint64_t ProFeatures        = DualEngine800B | AVX512Premium | FlashAttention;
    constexpr uint64_t EnterpriseAll      = 0x000000FF;
}

// ============================================================================
// License States (must match RawrXD_Common.inc)
// ============================================================================
enum class LicenseState : uint32_t {
    Invalid             = 0,
    ValidTrial          = 1,
    ValidEnterprise     = 2,
    ValidOEM            = 3,
    Expired             = 4,
    HardwareMismatch    = 5,
};

// ============================================================================
// ASM Extern Declarations
// ============================================================================
// These are the raw function pointers exported from the MASM object files.
// They follow Windows x64 calling convention (RCX, RDX, R8, R9).

extern "C" {
    // --- RawrXD_EnterpriseLicense.asm ---
    
    /// Initialize the license subsystem (crypto, registry, validation)
    /// Returns: 0 = success, NTSTATUS error code otherwise
    int64_t Enterprise_InitLicenseSystem();
    
    /// Full cryptographic license validation
    /// Returns: 0 = valid, NTSTATUS error code otherwise
    int64_t Enterprise_ValidateLicense();
    
    /// Check if a specific feature is enabled
    /// @param featureMask  Feature bitmask to check
    /// Returns: 1 if enabled, 0 if disabled
    int32_t Enterprise_CheckFeature(uint64_t featureMask);
    
    /// Attempt to unlock 800B Dual-Engine (includes integrity check)
    /// Returns: 1 if unlocked, 0 if denied
    int32_t Enterprise_Unlock800BDualEngine();
    
    /// Install a new license from memory
    /// @param pLicense    Pointer to license blob
    /// @param cbLicense   Size of license blob
    /// @param pSignature  Pointer to RSA signature (512 bytes)
    /// Returns: 0 = success, NTSTATUS error code otherwise
    int64_t Enterprise_InstallLicense(const void* pLicense, uint64_t cbLicense,
                                       const void* pSignature);
    
    /// Get current license state enum
    /// Returns: LicenseState value
    int32_t Enterprise_GetLicenseStatus();
    
    /// Get human-readable feature string
    /// @param pBuffer  Output buffer
    /// @param cbBuffer Buffer size
    /// Returns: Number of bytes written
    int64_t Enterprise_GetFeatureString(char* pBuffer, uint64_t cbBuffer);
    
    /// Generate 64-bit hardware fingerprint
    /// Returns: 64-bit HWID hash
    uint64_t Enterprise_GenerateHardwareHash();
    
    /// Runtime integrity check (anti-tamper)
    /// Returns: 1 if clean, 0 if tampered
    int32_t Enterprise_RuntimeIntegrityCheck();
    
    /// Cleanup license subsystem
    void Enterprise_Shutdown();
    
    /// Integration hook: check if enterprise engines should register
    /// Returns: 1 if enterprise features available, 0 if community
    int32_t Titan_CheckEnterpriseUnlock();
    
    /// Integration hook: check allocation budget
    /// @param requestedSize  Requested allocation in bytes
    /// Returns: 1 if approved, 0 if over community limit
    int32_t Streaming_CheckEnterpriseBudget(uint64_t requestedSize);

    // --- RawrXD_License_Shield.asm ---
    
    /// Direct PEB anti-debug check (bypasses API hooks)
    /// Returns: 1 if debugger detected, 0 if clean
    int32_t IsDebuggerPresent_Native();
    
    /// RDTSC timing-based debugger detection
    /// Returns: 1 if timing clean, 0 if debugger detected
    int32_t Shield_TimingCheck();
    
    /// Generate hardware ID fingerprint
    /// Returns: 64-bit HWID
    uint64_t Shield_GenerateHWID();
    
    /// Verify .text section integrity
    /// Returns: 1 if clean, 0 if tampered
    int32_t Shield_VerifyIntegrity();
    
    /// AES-NI decrypt the 800B kernel entry shim
    /// @param pKey  Pointer to 256-bit AES key
    /// Returns: Pointer to decrypted shim, or NULL
    void* Shield_AES_DecryptShim(const void* pKey);
    
    /// Full 800B kernel unlock with layered defense
    /// @param pKey   License-derived 256-bit AES key
    /// @param pHWID  Expected HWID buffer
    /// Returns: Pointer to decrypted kernel entry, or NULL
    void* Unlock_800B_Kernel(const void* pKey, const void* pHWID);

    // --- Global flags (set by ASM, read by C++) ---
    extern int32_t  g_800B_Unlocked;
    extern uint64_t g_EnterpriseFeatures;
}

// ============================================================================
// C++ Wrapper Class
// ============================================================================
// Thread-safe C++ interface around the ASM license system.
// Used by StreamingEngineRegistry and Win32IDE for clean integration.

class EnterpriseLicense {
public:
    /// Initialize the license system. Call once at startup.
    /// Returns true on success (even if no license found — community mode).
    static bool initialize();
    
    /// Shutdown and release all resources. Call at process exit.
    static void shutdown();
    
    /// Check if a specific feature is enabled.
    static bool isFeatureEnabled(uint64_t featureMask);
    
    /// Check if 800B Dual-Engine is unlocked (includes integrity).
    static bool is800BUnlocked();
    
    /// Check if any enterprise features are active.
    static bool isEnterprise();
    
    /// Get current license state.
    static LicenseState getState();
    
    /// Get human-readable feature string for UI display.
    static std::string getFeatureString();
    
    /// Get current machine's hardware fingerprint.
    static uint64_t getHardwareHash();
    
    /// Install a new license.
    /// Returns true on success.
    static bool installLicense(const void* licenseBlob, size_t blobSize,
                                const void* signature);
    
    /// Re-validate the current license (e.g., after time change).
    static bool revalidate();
    
    /// Check if a given allocation size is within the license budget.
    /// Community: limited to 16GB (70B models).
    /// Enterprise: unlimited.
    static bool checkAllocationBudget(uint64_t requestedBytes);
    
    /// Get the active feature bitmask (for diagnostics).
    static uint64_t getFeatureMask();
    
    /// Get license edition string ("Community", "Enterprise", "Trial").
    static const char* getEditionName();

private:
    static bool s_initialized;
    static std::mutex s_mutex;
};

} // namespace RawrXD
