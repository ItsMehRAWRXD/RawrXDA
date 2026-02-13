// ============================================================================
// enterprise_license.h — C++20 Bridge to MASM Enterprise License System (v2)
// ============================================================================
// Provides extern "C" declarations for ASM-exported license functions,
// a Singleton C++20 wrapper with RAII LicenseGuard, and feature enum class.
//
// ASM Sources:
//   src/asm/RawrXD_EnterpriseLicense.asm — Core license logic
//   src/asm/RawrXD_License_Shield.asm    — Anti-tamper & kernel decrypt (v2)
//
// Architecture:
//   EnterpriseLicense::Instance().Initialize()
//    ├─ Shield_InitializeDefense()        [ASM] — 5-layer anti-tamper
//    └─ Enterprise_InitLicenseSystem()    [ASM]
//        ├─ CryptoAPI provider init
//        ├─ RSA public key import
//        ├─ Shield_GenerateHWID()         [ASM]
//        ├─ Registry license load
//        └─ Enterprise_ValidateLicense()  [ASM]
//             ├─ Magic / version / expiry
//             ├─ Hardware fingerprint (MurmurHash3)
//             └─ RSA-4096 signature verify
//
//   Engine registration gate:
//    StreamingEngineRegistry::registerBuiltinEngines()
//     └─ if (EnterpriseLicense::isFeatureEnabled(LicenseFeature::DualEngine800B))
//            registerDualEngine800B()
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <mutex>
#include <functional>
#include <fstream>

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
// EnterpriseFeature enum class (type-safe, composable with operator|)
// ============================================================================
enum class EnterpriseFeature : uint64_t {
    None              = 0x00,
    DualEngine800B    = 0x01,
    AVX512Premium     = 0x02,
    DistributedSwarm  = 0x04,
    GPUQuant4Bit      = 0x08,
    EnterpriseSupport = 0x10,
    UnlimitedContext  = 0x20,
    FlashAttention    = 0x40,
    MultiGPU          = 0x80,
    All               = 0xFF
};

inline EnterpriseFeature operator|(EnterpriseFeature a, EnterpriseFeature b) {
    return static_cast<EnterpriseFeature>(
        static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}

inline EnterpriseFeature operator&(EnterpriseFeature a, EnterpriseFeature b) {
    return static_cast<EnterpriseFeature>(
        static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
}

inline bool HasFlag(EnterpriseFeature set, EnterpriseFeature flag) {
    return (static_cast<uint64_t>(set) & static_cast<uint64_t>(flag)) != 0;
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
    Tampered            = 6,
};

// ============================================================================
// License Change Callback
// ============================================================================
using LicenseChangeCallback = std::function<void(LicenseState oldState, LicenseState newState)>;

// ============================================================================
// ASM Extern Declarations
// ============================================================================
#pragma pack(push, 1)

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

    // --- RawrXD_License_Shield.asm (v2) ---
    
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
    
    /// AES-NI decrypt the 800B kernel entry shim (v1 — simplified)
    /// @param pKey  Pointer to 256-bit AES key
    /// Returns: Pointer to decrypted shim, or NULL
    void* Shield_AES_DecryptShim(const void* pKey);
    
    /// Full 800B kernel unlock with layered defense
    /// @param pKey   License-derived 256-bit AES key
    /// @param pHWID  Expected HWID buffer
    /// Returns: Pointer to decrypted kernel entry, or NULL
    void* Unlock_800B_Kernel(const void* pKey, const void* pHWID);

    /// [v2] Master defense initialization — chains all 5 layers
    /// Returns: 1 if all layers pass, 0 if tamper detected
    int32_t Shield_InitializeDefense();

    /// [v2] Heap flags anti-debug check
    /// Returns: 1 if clean, 0 if debugger via heap flags
    int32_t Shield_CheckHeapFlags();

    /// [v2] Kernel debugger detection via NtQuerySystemInformation
    /// Returns: 1 if no kernel debugger, 0 if detected
    int32_t Shield_CheckKernelDebug();

    /// [v2] Full MurmurHash3 64-bit hash
    /// @param pData  Data pointer
    /// @param len    Data length
    /// @param seed   64-bit seed
    /// Returns: 64-bit hash
    uint64_t Shield_MurmurHash3_x64(const void* pData, uint64_t len, uint64_t seed);

    /// [v2] AES-NI decrypt with CPUID validation + 14-round AES-256
    /// @param pKey  Pointer to 256-bit AES key (32 bytes)
    /// Returns: Pointer to decrypted shim, or NULL if no AES-NI
    void* Shield_DecryptKernelEntry(const void* pKey);

    // --- Global flags (set by ASM, read by C++) ---
    extern int32_t  g_800B_Unlocked;
    extern uint64_t g_EnterpriseFeatures;

    // --- C-link callback exports (called from C++, usable from ASM) ---
    void EnterpriseLog(const char* message);
    void EnterpriseStateChanged(uint32_t oldState, uint32_t newState);
}

#pragma pack(pop)

// ============================================================================
// LicenseGuard — RAII scope guard for feature-gated code paths
// ============================================================================
// Usage:
//   LicenseGuard guard(EnterpriseFeature::DualEngine800B);
//   if (guard) { /* licensed code */ }
//
class LicenseGuard {
public:
    explicit LicenseGuard(EnterpriseFeature required);
    ~LicenseGuard() = default;

    // Non-copyable, non-movable
    LicenseGuard(const LicenseGuard&) = delete;
    LicenseGuard& operator=(const LicenseGuard&) = delete;

    /// Returns true if all required features are licensed
    explicit operator bool() const noexcept { return m_granted; }
    bool isGranted() const noexcept { return m_granted; }

private:
    bool m_granted;
    EnterpriseFeature m_required;
};

// ============================================================================
// C++ Singleton Wrapper Class (v2)
// ============================================================================
// Thread-safe C++20 Singleton interface around the ASM license system.
// Used by StreamingEngineRegistry, Win32IDE, and API Server.

class EnterpriseLicense {
public:
    /// Get the singleton instance (thread-safe, lazy init)
    static EnterpriseLicense& Instance();

    // Non-copyable, non-movable
    EnterpriseLicense(const EnterpriseLicense&) = delete;
    EnterpriseLicense& operator=(const EnterpriseLicense&) = delete;

    /// Initialize the license system. Call once at startup.
    /// Runs Shield_InitializeDefense() first, then Enterprise_InitLicenseSystem().
    /// Returns true on success (even if no license — community mode).
    bool Initialize();
    
    /// Shutdown and release all resources. Call at process exit.
    void Shutdown();
    
    /// Check if a specific feature is enabled (EnterpriseFeature enum).
    bool HasFeature(EnterpriseFeature feature) const;

    /// Check if a specific feature bitmask is enabled (raw uint64).
    bool HasFeatureMask(uint64_t featureMask) const;
    
    /// Check if 800B Dual-Engine is unlocked (includes integrity).
    bool Is800BUnlocked() const;
    
    /// Check if any enterprise features are active.
    bool IsEnterprise() const;
    
    /// Get current license state.
    LicenseState GetState() const;
    
    /// Get human-readable feature string for UI display.
    std::string GetFeatureString() const;
    
    /// Get current machine's hardware fingerprint.
    uint64_t GetHardwareHash() const;
    
    /// Install a new license from raw memory.
    bool InstallLicense(const void* licenseBlob, size_t blobSize,
                        const void* signature);

    /// Install a license from a file (.rawrlic format: blob + 512-byte RSA sig)
    bool InstallLicenseFromFile(const std::wstring& path);

    /// Install a license from a file (narrow string overload)
    bool InstallLicenseFromFile(const std::string& path);
    
    /// Re-validate the current license.
    bool Revalidate();
    
    /// Check if a given allocation size is within the license budget.
    bool CheckAllocationBudget(uint64_t requestedBytes) const;
    
    /// Get the active feature bitmask (for diagnostics).
    uint64_t GetFeatureMask() const;
    
    /// Get license edition string ("Community", "Enterprise", "Trial").
    const char* GetEditionName() const;

    /// Get maximum model size allowed by current license tier (GB)
    uint64_t GetMaxModelSizeGB() const;

    /// Get maximum context length allowed by current license tier (tokens)
    uint64_t GetMaxContextLength() const;

    /// Get shield defense state bitmask (5 bits, one per layer)
    uint32_t GetShieldState() const;

    /// Register a callback for license state changes
    void OnLicenseChange(LicenseChangeCallback callback);

    // ========================================================================
    // Static compatibility shims (legacy API — delegates to Instance())
    // These maintain backward compatibility with existing callsites in
    // streaming_engine_registry.cpp and other modules.
    // ========================================================================
    static bool initialize()                           { return Instance().Initialize(); }
    static void shutdown()                             { Instance().Shutdown(); }
    static bool isFeatureEnabled(uint64_t mask)        { return Instance().HasFeatureMask(mask); }
    static bool is800BUnlocked()                       { return Instance().Is800BUnlocked(); }
    static bool isEnterprise()                         { return Instance().IsEnterprise(); }
    static LicenseState getState()                     { return Instance().GetState(); }
    static std::string getFeatureString()              { return Instance().GetFeatureString(); }
    static uint64_t getHardwareHash()                  { return Instance().GetHardwareHash(); }
    static bool installLicense(const void* b, size_t s, const void* sig) 
                                                       { return Instance().InstallLicense(b, s, sig); }
    static bool revalidate()                           { return Instance().Revalidate(); }
    static bool checkAllocationBudget(uint64_t bytes)  { return Instance().CheckAllocationBudget(bytes); }
    static uint64_t getFeatureMask()                   { return Instance().GetFeatureMask(); }
    static const char* getEditionName()                { return Instance().GetEditionName(); }

private:
    EnterpriseLicense() = default;
    ~EnterpriseLicense() = default;

    bool m_initialized = false;
    mutable std::mutex m_mutex;
    LicenseState m_lastState = LicenseState::Invalid;
    std::vector<LicenseChangeCallback> m_callbacks;

    void notifyStateChange(LicenseState oldState, LicenseState newState);
};

} // namespace RawrXD
