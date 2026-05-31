// ============================================================================
// marketplace_discovery_backend.h — Extension Marketplace Discovery & Sync
// ============================================================================
// PURPOSE:
//   Backend service for VS Code Marketplace discovery, search, and sync operations.
//   Handles:
//   - Search extensions by keywords
//   - Browse by category/tag
//   - Fetch trending/featured extensions
//   - Sync full marketplace catalog (cached)
//   - Update indicators (new versions, updates available)
//   - Offline fallback mode
//
// Architecture: C++20 | Win32 | Async operations | No exceptions | Qt-free
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <cstdint>

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Discovery Types
// ============================================================================

enum class DiscoveryCategory {
    AllExtensions,
    Featured,
    Trending,
    Popular,
    Recommended,
    NewReleases,
    
    // Specific categories
    Programming,
    Themes,
    Snippets,
    Linters,
    Debuggers,
    Formatters,
    Testing,
    Keymaps,
    Visualization,
    Other,
};

struct DiscoveryFilter {
    DiscoveryCategory category = DiscoveryCategory::AllExtensions;
    std::string searchQuery;
    std::string language;               // Filter by language support
    std::string publisher;              // Filter by publisher
    std::vector<std::string> tags;      // Multiple tags (AND filter)
    
    bool sortByRating = true;           // vs sortByInstalls
    bool onlyVerified = false;          // Only Microsoft verified
    bool includeDeprecated = false;     // Include deprecated extensions
    int pageNumber = 1;
    int pageSize = 50;
};

struct ExtensionDiscoveryResult {
    std::string id;                     // "publisher.extensionName"
    std::string displayName;
    std::string publisher;
    std::string description;
    std::string icon;
    std::string version;
    double averageRating = 0.0;
    int installCount = 0;
    int ratingCount = 0;
    std::vector<std::string> tags;
    std::string homepage;
    std::string repository;
    bool isVerified = false;
    bool isDeprecated = false;
    bool isPrerelease = false;
};

struct DiscoveryPageResult {
    bool success = false;
    std::string errorMessage;
    std::vector<ExtensionDiscoveryResult> extensions;
    int totalCount = 0;
    int currentPage = 1;
    int pageSize = 0;
    bool hasMore = false;
};

struct SyncStatistics {
    uint64_t lastSyncTimeMs = 0;
    int totalExtensionsInCache = 0;
    int updatesAvailable = 0;
    size_t cacheSize = 0;              // Bytes
    std::string lastSyncStatus;
};

// ============================================================================
// Marketplace Discovery Backend
// ============================================================================

class MarketplaceDiscoveryBackend {
public:
    explicit MarketplaceDiscoveryBackend();
    ~MarketplaceDiscoveryBackend();

    // ── Search Operations ──────────────────────────────────────────────

    // Search extensions
    DiscoveryPageResult Search(const DiscoveryFilter& filter);

    // Get featured extensions
    DiscoveryPageResult GetFeatured(int page = 1, int pageSize = 50);

    // Get trending extensions
    DiscoveryPageResult GetTrending(int page = 1, int pageSize = 50);

    // Get category extensions
    DiscoveryPageResult GetCategory(DiscoveryCategory category,
                                     int page = 1, int pageSize = 50);

    // Get recommendations based on installed extensions
    DiscoveryPageResult GetRecommendations(const std::vector<std::string>& installedIds,
                                           int count = 10);

    // ── Sync Operations ────────────────────────────────────────────────

    // Sync marketplace metadata (full or incremental)
    bool SyncMarketplace(bool fullSync = false);

    // Async sync with progress callback
    void SyncMarketplaceAsync(
        std::function<void(int downloaded, int total)> onProgress,
        std::function<void(bool success, const std::string& error)> onComplete
    );

    // Cancel ongoing sync
    void CancelSync();

    // ── Cache Management ───────────────────────────────────────────────

    // Check for extension updates
    std::vector<std::string> CheckForUpdates(const std::vector<std::string>& installedIds);

    // Get available versions for extension
    std::vector<std::string> GetAvailableVersions(const std::string& extensionId);

    // Clear cache
    bool ClearCache();

    // ── Statistics & State ─────────────────────────────────────────────

    SyncStatistics GetSyncStatistics() const;
    bool IsOnlineMode() const;
    void SetOfflineMode(bool offline);

    // ── Configuration ──────────────────────────────────────────────────

    // Set cache directory
    bool SetCacheDirectory(const std::string& cachePath);

    // Set max cache size (in MB)
    void SetMaxCacheSize(size_t mb);

    // Enable/disable auto-sync on startup
    void SetAutoSyncOnStartup(bool enabled);

    // ── Direct Extension Lookup ────────────────────────────────────────

    // Get extension details from cache or live
    bool GetExtensionDetails(const std::string& extensionId,
                             ExtensionDiscoveryResult& outResult);

private:
    mutable std::mutex m_lock;

    // Cache storage
    std::string m_cacheDir;
    size_t m_maxCacheSizeBytes = 100 * 1024 * 1024;  // 100 MB default
    std::unordered_map<std::string, ExtensionDiscoveryResult> m_extensionCache;

    // Sync state
    std::atomic<bool> m_isSyncing{false};
    std::atomic<bool> m_offlineMode{false};
    std::atomic<uint64_t> m_lastSyncMs{0};
    std::thread m_syncThread;
    bool m_autoSyncOnStartup = true;

    // Stats
    SyncStatistics m_stats;

    // Callbacks for async operations
    std::function<void(int, int)> m_syncProgressCallback;
    std::function<void(bool, const std::string&)> m_syncCompleteCallback;

    // Internal helpers
    bool LoadCacheFromDisk();
    bool SaveCacheToDisk();
    
    void ExecuteSyncInternal();
    bool FetchExtensionMetadata(const std::string& cursor, std::vector<ExtensionDiscoveryResult>& out);
    
    DiscoveryPageResult ParseCachedResults(const DiscoveryFilter& filter);
    DiscoveryPageResult FetchLiveResults(const DiscoveryFilter& filter);

    size_t CalculateCacheSize() const;
    void PruneCache();
};

// ============================================================================
// Global Helper
// ============================================================================

// Get singleton discovery backend instance
MarketplaceDiscoveryBackend& GetDiscoveryBackend();

// Convenience search function
DiscoveryPageResult SearchExtensions(const std::string& query, int page = 1);

}  // namespace Extensions
}  // namespace RawrXD

#endif  // MARKETPLACE_DISCOVERY_BACKEND_H
