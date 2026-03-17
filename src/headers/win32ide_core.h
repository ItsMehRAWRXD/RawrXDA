#ifndef WIN32IDE_CORE_H_
#define WIN32IDE_CORE_H_

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>
#include <unordered_map>

// Forward declarations
struct HWND__;
typedef HWND__* HWND;

namespace nlohmann { class json; }

// ============================================================================
// CATEGORY 3 - RawrXD Core Systems
// ============================================================================

/**
 * @namespace RawrXD::License
 * @brief License management and feature gating
 */
namespace RawrXD::License {

enum class FeatureID : uint32_t {
    BasicGGUFLoading = 0,
    COUNT = 65
};

} // namespace RawrXD::License

/**
 * @namespace RawrXD::Flags
 * @brief Feature flag runtime system
 */
namespace RawrXD::Flags {

/**
 * @class FeatureFlagsRuntime
 * @brief Singleton for runtime feature flag management
 */
class FeatureFlagsRuntime {
public:
    /**
     * Get singleton instance
     * @return Reference to singleton instance
     */
    static FeatureFlagsRuntime& Instance();

    /**
     * Check if feature is enabled
     * @param feature Feature ID to check
     * @return true if feature is enabled
     */
    bool isEnabled(RawrXD::License::FeatureID feature) const;

    /**
     * Refresh feature flags from license
     */
    void refreshFromLicense();

private:
    FeatureFlagsRuntime();
    ~FeatureFlagsRuntime() = default;

    FeatureFlagsRuntime(const FeatureFlagsRuntime&) = delete;
    FeatureFlagsRuntime& operator=(const FeatureFlagsRuntime&) = delete;

    std::unordered_map<uint32_t, bool> m_flags;
};

} // namespace RawrXD::Flags

/**
 * @namespace RawrXD::Parity
 * @brief Parity checking and cursor synchronization
 */
namespace RawrXD::Parity {

/**
 * Verify feature-module wiring between components
 * @param hwnd Window handle for context
 * @return 0 if all feature-module checks passed, error code otherwise
 */
int verifyFeaturesWiring(void* hwnd);

} // namespace RawrXD::Parity

/**
 * @namespace VSCodeMarketplace
 * @brief VS Code extension marketplace integration
 */
namespace VSCodeMarketplace {

/**
 * @struct MarketplaceEntry
 * @brief Entry retrieved from VS Code marketplace
 */
struct MarketplaceEntry {
    std::string id;           ///< Extension ID
    std::string name;         ///< Extension name
    std::string publisher;    ///< Publisher name
    std::string version;      ///< Extension version
};

/**
 * Query VS Code marketplace for extensions
 * @param query Search query string
 * @param page Page number (0-indexed)
 * @param pageSize Items per page
 * @param results Output vector of marketplace entries
 * @return true if query successful
 */
bool Query(const std::string& query, int page, int pageSize, std::vector<MarketplaceEntry>& results);

/**
 * Download VSIX extension from marketplace
 * @param publisher Publisher ID
 * @param name Extension name
 * @param version Version string
 * @param outPath Output path for downloaded VSIX file
 * @return true if download successful
 */
bool DownloadVsix(const std::string& publisher, const std::string& name, 
                  const std::string& version, const std::string& outPath);

} // namespace VSCodeMarketplace

// ============================================================================
// Extern C Functions
// ============================================================================

/**
 * Set the active model path for local parity checking
 * @param modelPath Path to model file
 * @return true if path set successfully
 */
extern "C" {
    bool __cdecl LocalParity_SetModelPath(const wchar_t* modelPath);
}

#endif // WIN32IDE_CORE_H_
