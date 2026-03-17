// ============================================================================
// extension_marketplace.hpp — Non-Qt Extension Marketplace & Package Manager
// ============================================================================
// Replaces the Qt-based design in IMPLEMENTATION_SUMMARY.md with a pure
// Win32/POSIX marketplace implementation.
//
// Features:
//   - VSIX package download, install, uninstall
//   - Extension dependency resolution (topological)
//   - Local offline cache
//   - Enterprise policy engine (allow/block lists)
//   - Extension search & browse API
//   - Integration with RawrXD_ExtensionHost.asm
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Result type
// ============================================================================
struct ExtResult {
    bool success;
    std::string detail;
    int errorCode;

    static ExtResult ok(const std::string& msg = "OK") {
        ExtResult r;
        r.success = true;
        r.detail = msg;
        r.errorCode = 0;
        return r;
    }

    static ExtResult error(const std::string& msg, int code = -1) {
        ExtResult r;
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Extension Metadata
// ============================================================================
struct ExtensionManifest {
    std::string id;               // "publisher.extensionName"
    std::string name;
    std::string displayName;
    std::string publisher;
    std::string version;          // SemVer "1.2.3"
    std::string description;
    std::string license;
    std::string homepage;
    std::string repository;

    // Categories
    enum class Category : uint8_t {
        LANGUAGE     = 0,
        THEME        = 1,
        SNIPPET      = 2,
        DEBUGGER     = 3,
        FORMATTER    = 4,
        LINTER       = 5,
        AI           = 6,
        SCM          = 7,
        VISUALIZATION = 8,
        TESTING      = 9,
        OTHER        = 10
    };
    std::vector<Category> categories;

    // Activation events
    std::vector<std::string> activationEvents;  // "onLanguage:python", etc.

    // Dependencies on other extensions
    struct Dependency {
        std::string extensionId;
        std::string versionRange;   // ">=1.0.0 <2.0.0"
    };
    std::vector<Dependency> dependencies;

    // Contribution points (menus, commands, views, etc.)
    struct ContributionPoint {
        std::string type;      // "commands", "menus", "viewsContainers", etc.
        std::string payload;   // JSON-encoded contribution data
    };
    std::vector<ContributionPoint> contributions;

    // Extension file info
    std::string vsixPath;           // Local path to .vsix file
    std::string installPath;        // Unpacked install directory
    uint64_t fileSize;
    uint64_t installedAt;           // Epoch timestamp
    uint64_t updatedAt;
    bool isEnabled;
    bool isBuiltIn;

    // Compatibility
    std::string engineVersion;      // Required RawrXD version
    std::vector<std::string> supportedPlatforms;  // "win32-x64", etc.

    ExtensionManifest()
        : fileSize(0), installedAt(0), updatedAt(0),
          isEnabled(true), isBuiltIn(false) {}
};

// ============================================================================
// Extension State
// ============================================================================
enum class ExtensionState : uint8_t {
    NOT_INSTALLED = 0,
    INSTALLED     = 1,
    ENABLED       = 2,
    DISABLED      = 3,
    UPDATING      = 4,
    CORRUPTED     = 5,
    BLOCKED       = 6    // Blocked by enterprise policy
};

// ============================================================================
// Search / Browse
// ============================================================================
struct ExtensionSearchQuery {
    std::string text;              // Free-text search
    ExtensionManifest::Category category;
    enum class SortBy : uint8_t {
        RELEVANCE     = 0,
        INSTALLS      = 1,
        RATING        = 2,
        UPDATED_DATE  = 3,
        NAME          = 4
    };
    SortBy sortBy;
    uint32_t page;
    uint32_t pageSize;

    ExtensionSearchQuery()
        : category(ExtensionManifest::Category::OTHER),
          sortBy(SortBy::RELEVANCE), page(0), pageSize(25) {}
};

struct ExtensionSearchResult {
    ExtensionManifest manifest;
    uint64_t installCount;
    float rating;               // 0.0 - 5.0
    uint32_t ratingCount;
    std::string downloadUrl;
    std::string iconUrl;
};

struct ExtensionSearchResponse {
    std::vector<ExtensionSearchResult> results;
    uint32_t totalCount;
    uint32_t page;
    uint32_t pageSize;
};

// ============================================================================
// Enterprise Policy
// ============================================================================
struct EnterprisePolicyConfig {
    bool enforceAllowList;           // Only allow listed extensions
    bool enforceBlockList;           // Block listed extensions
    bool allowUnknownPublishers;     // Allow extensions from unverified publishers
    bool requireSignatureVerification;  // Require signed .vsix packages
    uint32_t maxExtensions;          // Max installed extensions (0 = unlimited)

    std::vector<std::string> allowedExtensionIds;
    std::vector<std::string> blockedExtensionIds;
    std::vector<std::string> allowedPublishers;
    std::vector<std::string> blockedPublishers;

    // Auto-install list (install on first launch)
    std::vector<std::string> autoInstallIds;

    EnterprisePolicyConfig()
        : enforceAllowList(false), enforceBlockList(false),
          allowUnknownPublishers(true), requireSignatureVerification(false),
          maxExtensions(0) {}
};

// ============================================================================
// Event callback
// ============================================================================
struct MarketplaceEvent {
    enum Type : uint8_t {
        EXTENSION_INSTALLED    = 0,
        EXTENSION_UNINSTALLED  = 1,
        EXTENSION_ENABLED      = 2,
        EXTENSION_DISABLED     = 3,
        EXTENSION_UPDATED      = 4,
        EXTENSION_ACTIVATED    = 5,
        DOWNLOAD_STARTED       = 6,
        DOWNLOAD_COMPLETED     = 7,
        DOWNLOAD_FAILED        = 8,
        POLICY_VIOLATION       = 9
    };

    Type type;
    std::string extensionId;
    const char* detail;
};

using MarketplaceEventCallback = void(*)(const MarketplaceEvent& evt, void* userData);

// ============================================================================
// ExtensionMarketplace — The main manager
// ============================================================================
class ExtensionMarketplace {
public:
    static ExtensionMarketplace& instance();

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------

    // Set the extensions install directory
    ExtResult setInstallDirectory(const std::string& path);

    // Set the offline cache directory
    ExtResult setCacheDirectory(const std::string& path);

    // Set marketplace API endpoint
    ExtResult setRegistryUrl(const std::string& url);

    // Apply enterprise policy
    ExtResult applyPolicy(const EnterprisePolicyConfig& config);

    // -----------------------------------------------------------------------
    // Browse / Search
    // -----------------------------------------------------------------------

    // Search extensions (online or cached)
    ExtResult search(const ExtensionSearchQuery& query,
                     ExtensionSearchResponse& response);

    // Get extension details by ID
    ExtResult getExtensionDetails(const std::string& extensionId,
                                  ExtensionSearchResult& result);

    // List all installed extensions
    std::vector<ExtensionManifest> listInstalled() const;

    // List enabled extensions
    std::vector<ExtensionManifest> listEnabled() const;

    // -----------------------------------------------------------------------
    // Install / Uninstall
    // -----------------------------------------------------------------------

    // Install from registry (download + install)
    ExtResult installFromRegistry(const std::string& extensionId,
                                  const std::string& version = "latest");

    // Install from local .vsix file
    ExtResult installFromVsix(const std::string& vsixPath);

    // Install from a URL
    ExtResult installFromUrl(const std::string& downloadUrl);

    // Uninstall an extension
    ExtResult uninstall(const std::string& extensionId);

    // Update an extension to latest version
    ExtResult update(const std::string& extensionId);

    // Update all extensions
    ExtResult updateAll();

    // -----------------------------------------------------------------------
    // Enable / Disable
    // -----------------------------------------------------------------------

    ExtResult enable(const std::string& extensionId);
    ExtResult disable(const std::string& extensionId);

    // -----------------------------------------------------------------------
    // Extension Host Integration
    // -----------------------------------------------------------------------

    // Activate an extension (load into extension host)
    ExtResult activate(const std::string& extensionId,
                       const std::string& activationEvent = "");

    // Deactivate an extension
    ExtResult deactivate(const std::string& extensionId);

    // Get extension state
    ExtensionState getState(const std::string& extensionId) const;

    // Get extension manifest
    ExtResult getManifest(const std::string& extensionId,
                          ExtensionManifest& manifest) const;

    // -----------------------------------------------------------------------
    // Dependency Resolution
    // -----------------------------------------------------------------------

    // Check if all dependencies for an extension are satisfied
    ExtResult checkDependencies(const std::string& extensionId,
                                std::vector<std::string>& missingDeps);

    // Resolve and install all dependencies
    ExtResult installWithDependencies(const std::string& extensionId);

    // Compute full dependency tree
    ExtResult getDependencyTree(const std::string& extensionId,
                                std::vector<std::string>& orderedDeps);

    // -----------------------------------------------------------------------
    // VSIX Package Operations
    // -----------------------------------------------------------------------

    // Extract .vsix package (ZIP format) to install directory
    ExtResult extractVsix(const std::string& vsixPath,
                          const std::string& targetDir);

    // Parse package.json / extension manifest from .vsix
    ExtResult parseManifest(const std::string& manifestPath,
                            ExtensionManifest& manifest);

    // Verify .vsix package signature (if policy requires)
    ExtResult verifySignature(const std::string& vsixPath);

    // -----------------------------------------------------------------------
    // Offline Cache
    // -----------------------------------------------------------------------

    // Cache an extension package for offline use
    ExtResult cacheExtension(const std::string& extensionId);

    // Clear cache
    ExtResult clearCache();

    // Get cache statistics
    struct CacheStats {
        uint32_t cachedExtensions;
        uint64_t totalCacheSizeBytes;
    };
    CacheStats getCacheStats() const;

    // -----------------------------------------------------------------------
    // Events
    // -----------------------------------------------------------------------

    void addEventListener(MarketplaceEventCallback callback, void* userData);
    void removeEventListener(MarketplaceEventCallback callback);

    // -----------------------------------------------------------------------
    // Statistics
    // -----------------------------------------------------------------------

    struct MarketplaceStats {
        uint32_t totalInstalled;
        uint32_t totalEnabled;
        uint32_t totalDisabled;
        uint32_t totalBlocked;
        uint64_t totalDownloadBytes;
    };

    MarketplaceStats getStats() const;

    void shutdown();

private:
    ExtensionMarketplace();
    ~ExtensionMarketplace();
    ExtensionMarketplace(const ExtensionMarketplace&) = delete;
    ExtensionMarketplace& operator=(const ExtensionMarketplace&) = delete;

    // Policy check
    ExtResult checkPolicy(const ExtensionManifest& manifest);

    // HTTP download (Win32: WinHTTP, POSIX: libcurl)
    ExtResult httpDownload(const std::string& url,
                           const std::string& outputPath);

    // Emit event
    void emitEvent(const MarketplaceEvent& evt);

    // Dependency resolution (topological sort)
    ExtResult resolveDependencies(const std::string& extensionId,
                                  std::vector<std::string>& sorted);

    // SemVer comparison
    static bool semverSatisfies(const std::string& version,
                                const std::string& range);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    mutable std::mutex marketplaceMutex_;

    std::string installDir_;
    std::string cacheDir_;
    std::string registryUrl_;

    // Installed extensions (id → manifest)
    std::unordered_map<std::string, ExtensionManifest> installed_;
    std::unordered_map<std::string, ExtensionState> states_;

    // Enterprise policy
    EnterprisePolicyConfig policy_;

    // Event listeners
    static constexpr size_t MAX_EVENT_LISTENERS = 16;
    struct EventListener {
        MarketplaceEventCallback callback;
        void* userData;
    };
    EventListener eventListeners_[MAX_EVENT_LISTENERS];
    std::atomic<uint32_t> eventListenerCount_;

    // Statistics
    std::atomic<uint64_t> totalDownloadBytes_;
};

} // namespace Extensions
} // namespace RawrXD
