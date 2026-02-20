// ============================================================================
// production_release.h — Phase C: Production Release Engineering
// ============================================================================
// Strip debug symbols, final size optimization, installer + updater logic,
// commercial licensing enforcement (manifest wired).
//
// Architecture:
//   1. ReleaseOptimizer — Strip symbols, compress sections, size audit
//   2. InstallerEngine — NSIS-compatible installer generation
//   3. UpdaterEngine — Delta update checking + download + patching
//   4. LicenseEnforcer — Runtime license gate with feature manifest
//
// Integrations:
//   - EnterpriseLicense (src/core/enterprise_license.h)
//   - Feature Manifest (feature_manifest.json)
//   - StreamingEngineRegistry (src/core/streaming_engine_registry.h)
//
// Pattern: PatchResult-style structured results, no exceptions.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Forward declarations
struct PatchResult;

// ============================================================================
// Build Configuration
// ============================================================================
enum class BuildConfig : uint8_t {
    Debug       = 0,
    Release     = 1,
    RelWithDeb  = 2,    // Release with debug info
    MinSize     = 3,    // Minimum size release
    Production  = 4,    // Full production (stripped, signed, optimized)
};

// ============================================================================
// Size Optimization Flags
// ============================================================================
enum class OptFlag : uint32_t {
    StripDebugSymbols       = 0x0001,
    StripRelocationData     = 0x0002,
    MergeIdenticalSections  = 0x0004,
    CompressResources       = 0x0008,
    RemoveUnusedExports     = 0x0010,
    EnableLinkTimeOpt       = 0x0020,
    StripExceptionData      = 0x0040,
    PackWithUPX             = 0x0080,
    RemoveDebugDirectories  = 0x0100,
    OptimizeImportTable     = 0x0200,
    DeadCodeElimination     = 0x0400,
    StringPooling           = 0x0800,
    FunctionLevelLinking    = 0x1000,
    AllOptimizations        = 0x1FFF,
};

inline OptFlag operator|(OptFlag a, OptFlag b) {
    return static_cast<OptFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool hasFlag(OptFlag flags, OptFlag f) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(f)) != 0;
}

// ============================================================================
// Binary Size Audit Entry
// ============================================================================
struct SizeAuditEntry {
    std::string sectionName;    // .text, .rdata, .data, .rsrc, etc.
    uint64_t    rawSize;        // Bytes on disk
    uint64_t    virtualSize;    // Bytes in memory
    float       percentOfTotal;
    bool        strippable;     // Can be removed/shrunk
};

// ============================================================================
// Release Build Result
// ============================================================================
struct ReleaseResult {
    bool        success;
    const char* detail;
    uint64_t    originalSize;
    uint64_t    optimizedSize;
    float       compressionRatio;
    uint32_t    sectionsStripped;
    uint32_t    symbolsRemoved;

    static ReleaseResult ok(const char* msg, uint64_t orig, uint64_t opt) {
        ReleaseResult r;
        r.success = true;
        r.detail = msg;
        r.originalSize = orig;
        r.optimizedSize = opt;
        r.compressionRatio = (orig > 0) ? (float)opt / (float)orig : 1.0f;
        r.sectionsStripped = 0;
        r.symbolsRemoved = 0;
        return r;
    }

    static ReleaseResult error(const char* msg) {
        ReleaseResult r;
        r.success = false;
        r.detail = msg;
        r.originalSize = 0;
        r.optimizedSize = 0;
        r.compressionRatio = 0.0f;
        r.sectionsStripped = 0;
        r.symbolsRemoved = 0;
        return r;
    }
};

// ============================================================================
// Update Channel
// ============================================================================
enum class UpdateChannel : uint8_t {
    Stable      = 0,
    Beta        = 1,
    Nightly     = 2,
    Canary      = 3,
};

// ============================================================================
// Update Info
// ============================================================================
struct UpdateInfo {
    std::string version;
    std::string channel;
    std::string downloadUrl;
    std::string checksum;       // SHA-256
    uint64_t    fileSize;
    std::string releaseNotes;
    bool        mandatory;
    bool        available;
};

// ============================================================================
// Installer Configuration
// ============================================================================
struct InstallerConfig {
    std::string productName;        // "RawrXD IDE"
    std::string productVersion;     // "15.0.0"
    std::string publisher;          // "ItsMehRawrXD"
    std::string installDir;         // Default: C:\Program Files\RawrXD
    std::string iconPath;           // .ico file
    std::string licensePath;        // EULA text
    bool        createDesktopShortcut;
    bool        createStartMenuEntry;
    bool        addToPath;
    bool        registerFileAssociations;
    std::vector<std::string> fileAssociations; // .gguf, .asm, .cpp, etc.
    std::vector<std::string> components;       // IDE, Engine, Swarm, etc.
};

// ============================================================================
// License Gate Entry (maps feature → requirement)
// ============================================================================
struct LicenseGate {
    std::string     featureName;
    uint64_t        requiredFlags;      // LicenseFeature bitmask
    bool            hardGate;           // true = block, false = soft warning
    const char*     upgradeMessage;     // Shown when feature is gated
};

// ============================================================================
// Production Release Statistics
// ============================================================================
struct ProductionStats {
    std::atomic<uint64_t> licenseChecks{0};
    std::atomic<uint64_t> licenseGrants{0};
    std::atomic<uint64_t> licenseDenials{0};
    std::atomic<uint64_t> updateChecks{0};
    std::atomic<uint64_t> updateDownloads{0};
    std::atomic<uint64_t> buildOptimizations{0};
};

// ============================================================================
// ProductionReleaseEngine — Main Class
// ============================================================================
class ProductionReleaseEngine {
public:
    static ProductionReleaseEngine& instance();

    // ---- Build Optimization ----

    // Audit PE binary sections and report size breakdown.
    std::vector<SizeAuditEntry> auditBinary(const std::string& exePath) const;

    // Strip debug symbols from a PE binary.
    ReleaseResult stripDebugSymbols(const std::string& exePath,
                                     const std::string& outputPath);

    // Apply full size optimization pass.
    ReleaseResult optimizeBinary(const std::string& exePath,
                                  const std::string& outputPath,
                                  OptFlag flags = OptFlag::AllOptimizations);

    // Get total savings from optimization.
    std::string getSizeReport(const std::string& exePath) const;

    // ---- Installer Generation ----

    // Generate NSIS installer script.
    bool generateInstallerScript(const InstallerConfig& config,
                                  const std::string& outputNsi);

    // Build installer (requires NSIS in PATH).
    ReleaseResult buildInstaller(const std::string& nsiScript,
                                  const std::string& outputExe);

    // Generate uninstaller registry entries.
    bool writeUninstallRegistry(const InstallerConfig& config);

    // ---- Auto-Updater ----

    // Check for updates from the update server.
    UpdateInfo checkForUpdate(UpdateChannel channel = UpdateChannel::Stable);

    // Download an update (with progress callback).
    ReleaseResult downloadUpdate(const UpdateInfo& update,
                                  const std::string& destPath);

    // Apply a downloaded update (hot-swap or restart).
    ReleaseResult applyUpdate(const std::string& updatePath);

    // Set update server URL.
    void setUpdateServer(const std::string& url);

    // Get current version.
    std::string getCurrentVersion() const;

    // ---- License Enforcement ----

    // Register a license gate for a feature.
    void registerGate(const LicenseGate& gate);

    // Check if a feature is allowed by the current license.
    bool isFeatureAllowed(const std::string& featureName) const;

    // Check and enforce (returns false + shows upgrade message if denied).
    bool enforceGate(const std::string& featureName) const;

    // Get all registered gates and their status.
    std::vector<std::pair<LicenseGate, bool>> getGateStatus() const;

    // Refresh license state from disk/registry.
    void refreshLicense();

    // ---- Feature Manifest Integration ----

    // Load feature manifest JSON.
    bool loadManifest(const std::string& manifestPath);

    // Get all features and their license requirements.
    std::string getManifestJson() const;

    // ---- Statistics ----
    const ProductionStats& getStats() const { return m_stats; }

    // ---- JSON Serialization ----
    std::string toJson() const;

private:
    ProductionReleaseEngine();
    ~ProductionReleaseEngine();
    ProductionReleaseEngine(const ProductionReleaseEngine&) = delete;
    ProductionReleaseEngine& operator=(const ProductionReleaseEngine&) = delete;

    // Internal: PE section parsing
    bool parsePESections(const std::string& exePath,
                         std::vector<SizeAuditEntry>& sections) const;

    // Internal: Symbol stripping via editbin or manual PE modification
    bool stripSymbolsFromPE(const std::string& exePath,
                             const std::string& outputPath,
                             uint32_t& symbolsRemoved);

    mutable std::mutex                  m_mutex;
    std::vector<LicenseGate>            m_gates;
    std::string                         m_updateServerUrl;
    std::string                         m_currentVersion;
    uint64_t                            m_currentLicenseFlags;
    ProductionStats                     m_stats;
    std::string                         m_manifestJson;
};
