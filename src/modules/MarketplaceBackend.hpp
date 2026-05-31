// ============================================================================
// MarketplaceBackend.hpp — Extension Marketplace Search & Sync Backend
// ============================================================================
//
// Phase 29B: Marketplace Infrastructure Completion
//
// Purpose:
//   Provides complete backend infrastructure for extension marketplace.
//   Implements search, filtering, metadata sync, and catalog management.
//   Compatible with VS Code Marketplace API and local registry.
//
// Features:
//   - VS Code Marketplace API Integration
//   - Local Extension Registry Management
//   - Search & Filtering with Elasticsearch-style queries
//   - Bulk Catalog Synchronization
//   - Extension Metadata Caching
//   - Version Management & Update Checks
//   - Download & Installation Queuing
//   - Offline Support with Local Cache
//
// Design:
//   - Thread-safe async operations
//   - HTTP/HTTPS client with retry logic
//   - JSON-based API responses
//   - Configurable sync intervals
//   - Bandwidth throttling for large downloads
//   - Compatible with existing MarketplacePanel UI
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <chrono>
#include <future>

#pragma comment(lib, "winhttp.lib")

// ============================================================================
// Extension Metadata Structures
// ============================================================================

struct ExtensionVersion {
    std::string version;
    std::chrono::system_clock::time_point lastUpdated;
    std::string downloadUrl;
    uint64_t downloadSize;
    std::string sha256Hash;
    bool isPreRelease;
    std::vector<std::string> engines;  // VS Code version compatibility
    std::vector<std::string> dependencies;
    std::string changelog;
};

struct ExtensionMetadata {
    std::string id;                     // publisher.name
    std::string name;
    std::string displayName;
    std::string description;
    std::string publisher;
    std::string category;
    std::vector<std::string> tags;
    std::vector<std::string> keywords;
    std::string iconUrl;
    std::string homepageUrl;
    std::string repositoryUrl;
    std::string licenseUrl;
    std::string license;
    
    // Statistics
    uint64_t downloadCount;
    uint64_t installCount;
    double averageRating;
    uint32_t ratingCount;
    
    // Versions
    std::vector<ExtensionVersion> versions;
    std::string latestVersion;
    std::string latestPreReleaseVersion;
    
    // Marketplace metadata
    std::chrono::system_clock::time_point publishedDate;
    std::chrono::system_clock::time_point lastUpdated;
    bool isVerified;
    bool isSponsored;
    
    // Local metadata
    bool isInstalled;
    std::string installedVersion;
    bool hasUpdate;
    bool isEnabled;
    std::chrono::system_clock::time_point lastSyncTime;
};

// ============================================================================
// Search & Filter Structures
// ============================================================================

enum class ExtensionSortOrder {
    Relevance,
    Name, 
    Downloads,
    Rating,
    Updated,
    Published,
    Random
};

enum class ExtensionFilterCategory {
    All,
    AI,
    Azure,
    Chat,
    DataScience,
    Debuggers,
    ExtensionPacks,
    Education,
    Formatters,
    Keymaps,
    LanguagePacks,
    Linters,
    MachineLearning,
    Notebooks,
    ProgrammingLanguages,
    SCMProviders,
    Snippets,
    Testing,
    Themes,
    Visualization,
    Other
};

struct ExtensionSearchQuery {
    std::string searchText;
    std::vector<ExtensionFilterCategory> categories;
    std::vector<std::string> tags;
    ExtensionSortOrder sortOrder;
    uint32_t pageSize;
    uint32_t pageNumber;
    bool includePreRelease;
    bool verifiedPublishersOnly;
    std::string targetVSCodeVersion;
    
    ExtensionSearchQuery() : sortOrder(ExtensionSortOrder::Relevance), pageSize(50), pageNumber(0), includePreRelease(false), verifiedPublishersOnly(false) {}
};

struct ExtensionSearchResult {
    std::vector<ExtensionMetadata> extensions;
    uint32_t totalCount;
    uint32_t pageNumber;
    uint32_t pageSize;
    bool hasMorePages;
    std::chrono::milliseconds searchTime;
    std::string queryId;  // For debugging
};

// ============================================================================
// Download & Installation Structures
// ============================================================================

enum class DownloadStatus {
    Queued,
    Downloading,
    Completed,
    Failed,
    Cancelled,
    Paused
};

struct ExtensionDownload {
    std::string extensionId;
    std::string version;
    std::string downloadUrl;
    std::string localPath;
    uint64_t totalBytes;
    std::atomic<uint64_t> downloadedBytes;
    DownloadStatus status;
    std::string errorMessage;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    
    // Progress callback: (extensionId, downloadedBytes, totalBytes)
    std::function<void(const std::string&, uint64_t, uint64_t)> progressCallback;
    
    // Completion callback: (extensionId, success, errorMessage)
    std::function<void(const std::string&, bool, const std::string&)> completionCallback;
};

// ============================================================================
// Marketplace Backend Configuration
// ============================================================================

struct MarketplaceConfig {
    std::string vsCodeMarketplaceUrl;
    std::string localRegistryPath;
    std::string cacheDirectory;
    std::string userAgent;
    uint32_t maxConcurrentDownloads;
    uint32_t maxRetries;
    std::chrono::seconds requestTimeout;
    std::chrono::minutes cacheDuration;
    std::chrono::hours syncInterval;
    uint64_t maxCacheSize;  // bytes
    bool enableCompression;
    bool enableOfflineMode;
    std::string proxyUrl;
    std::string proxyAuth;
    
    static MarketplaceConfig getDefault() {
        MarketplaceConfig config;
        config.vsCodeMarketplaceUrl = "https://marketplace.visualstudio.com/_apis/public/gallery";
        config.localRegistryPath = "extensions/registry.json";
        config.cacheDirectory = "extensions/cache/";
        config.userAgent = "RawrXD-Win32IDE/1.0 VSCode-Compatible";
        config.maxConcurrentDownloads = 4;
        config.maxRetries = 3;
        config.requestTimeout = std::chrono::seconds(30);
        config.cacheDuration = std::chrono::minutes(60);
        config.syncInterval = std::chrono::hours(24);
        config.maxCacheSize = 1024 * 1024 * 1024;  // 1 GB
        config.enableCompression = true;
        config.enableOfflineMode = true;
        return config;
    }
};

// ============================================================================
// HTTP Client for Marketplace API
// ============================================================================

class MarketplaceHTTPClient {
public:
    MarketplaceHTTPClient(const MarketplaceConfig& config);
    ~MarketplaceHTTPClient();
    
    // Synchronous HTTP operations
    std::string get(const std::string& url, const std::unordered_map<std::string, std::string>& headers = {});
    std::string post(const std::string& url, const std::string& data, const std::unordered_map<std::string, std::string>& headers = {});
    
    // Asynchronous operations
    std::future<std::string> getAsync(const std::string& url, const std::unordered_map<std::string, std::string>& headers = {});
    std::future<std::string> postAsync(const std::string& url, const std::string& data, const std::unordered_map<std::string, std::string>& headers = {});
    
    // File download with progress
    bool downloadFile(
        const std::string& url, 
        const std::string& localPath,
        std::function<void(uint64_t, uint64_t)> progressCallback = nullptr
    );
    
    // Configuration
    void setConfig(const MarketplaceConfig& config);
    void setProxy(const std::string& proxyUrl, const std::string& proxyAuth = "");
    
private:
    MarketplaceConfig m_config;
    HINTERNET m_session;
    std::mutex m_sessionMutex;
    
    // Helper methods
    std::string executeRequest(const std::string& verb, const std::string& url, const std::string& data, const std::unordered_map<std::string, std::string>& headers);
    bool parseUrl(const std::string& url, std::string& scheme, std::string& host, INTERNET_PORT& port, std::string& path);
    std::string handleRedirect(HINTERNET request, const std::string& originalUrl);
};

// ============================================================================
// Local Registry & Cache Management
// ============================================================================

class MarketplaceCache {
public:
    MarketplaceCache(const std::string& cacheDirectory, uint64_t maxSize);
    ~MarketplaceCache();
    
    // Cache operations
    bool store(const std::string& key, const std::string& data, std::chrono::minutes ttl = std::chrono::minutes(60));
    std::string retrieve(const std::string& key);
    bool exists(const std::string& key);
    void remove(const std::string& key);
    void clear();
    
    // Cache management
    void cleanup();  // Remove expired entries
    uint64_t getSize() const;
    uint64_t getUsedSpace() const;
    std::vector<std::string> listKeys() const;
    
private:
    std::string m_cacheDir;
    uint64_t m_maxSize;
    mutable std::mutex m_cacheMutex;
    
    struct CacheEntry {
        std::chrono::system_clock::time_point expiry;
        uint64_t size;
    };
    
    std::unordered_map<std::string, CacheEntry> m_entries;
    
    std::string getFilePath(const std::string& key) const;
    bool isExpired(const std::string& key) const;
    void loadCacheIndex();
    void saveCacheIndex();
    void ensureSpace(uint64_t requiredSpace);
};

// ============================================================================
// Main Marketplace Backend
// ============================================================================

class MarketplaceBackend {
public:
    MarketplaceBackend(const MarketplaceConfig& config = MarketplaceConfig::getDefault());
    ~MarketplaceBackend();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    
    // Search & Discovery
    ExtensionSearchResult search(const ExtensionSearchQuery& query);
    std::future<ExtensionSearchResult> searchAsync(const ExtensionSearchQuery& query);
    std::vector<ExtensionMetadata> getPopularExtensions(uint32_t count = 20);
    std::vector<ExtensionMetadata> getFeaturedExtensions();
    std::vector<ExtensionMetadata> getRecentlyUpdated(uint32_t count = 20);
    
    // Extension Details
    ExtensionMetadata getExtensionDetails(const std::string& extensionId);
    std::vector<ExtensionVersion> getExtensionVersions(const std::string& extensionId);
    std::vector<ExtensionMetadata> getExtensionDependencies(const std::string& extensionId);
    std::vector<ExtensionMetadata> getSimilarExtensions(const std::string& extensionId);
    
    // Installation & Downloads
    bool downloadExtension(const std::string& extensionId, const std::string& version = "latest");
    bool installExtension(const std::string& extensionId, const std::string& version = "latest");
    bool uninstallExtension(const std::string& extensionId);
    bool updateExtension(const std::string& extensionId);
    
    // Download Management
    void setDownloadProgressCallback(std::function<void(const std::string&, uint64_t, uint64_t)> callback);
    void setDownloadCompletionCallback(std::function<void(const std::string&, bool, const std::string&)> callback);
    std::vector<ExtensionDownload> getActiveDownloads() const;
    bool cancelDownload(const std::string& extensionId);
    bool pauseDownload(const std::string& extensionId);
    bool resumeDownload(const std::string& extensionId);
    
    // Local Registry Management
    std::vector<ExtensionMetadata> getInstalledExtensions();
    std::vector<ExtensionMetadata> getUpdatableExtensions();
    bool refreshLocalRegistry();
    bool syncWithMarketplace();
    
    // Configuration
    void setConfig(const MarketplaceConfig& config);
    MarketplaceConfig getConfig() const;
    void setOfflineMode(bool enabled);
    bool isOfflineMode() const;
    
    // Statistics & Analytics
    uint32_t getTotalExtensionCount();
    std::vector<std::string> getPopularCategories();
    std::unordered_map<std::string, uint32_t> getCategoryStats();
    
    // Event callbacks
    void setExtensionInstallCallback(std::function<void(const std::string&, bool)> callback);
    void setExtensionUpdateCallback(std::function<void(const std::string&, const std::string&)> callback);
    void setMarketplaceSyncCallback(std::function<void(bool, const std::string&)> callback);
    
private:
    MarketplaceConfig m_config;
    std::unique_ptr<MarketplaceHTTPClient> m_httpClient;
    std::unique_ptr<MarketplaceCache> m_cache;
    
    // State management
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_offlineMode;
    mutable std::mutex m_stateMutex;
    
    // Download management
    std::queue<ExtensionDownload> m_downloadQueue;
    std::vector<std::thread> m_downloadThreads;
    std::unordered_map<std::string, ExtensionDownload> m_activeDownloads;
    std::mutex m_downloadMutex;
    std::condition_variable m_downloadCondition;
    std::atomic<bool> m_shutdownRequested;
    
    // Callbacks
    std::function<void(const std::string&, uint64_t, uint64_t)> m_progressCallback;
    std::function<void(const std::string&, bool, const std::string&)> m_completionCallback;
    std::function<void(const std::string&, bool)> m_installCallback;
    std::function<void(const std::string&, const std::string&)> m_updateCallback;
    std::function<void(bool, const std::string&)> m_syncCallback;
    
    // Background sync
    std::thread m_syncThread;
    std::atomic<bool> m_syncRunning;
    std::chrono::system_clock::time_point m_lastSync;
    
    // Local registry
    std::unordered_map<std::string, ExtensionMetadata> m_localRegistry;
    mutable std::mutex m_registryMutex;
    
    // Helper methods
    void downloadWorker();
    void syncWorker();
    
    // VS Code Marketplace API
    ExtensionSearchResult searchVSCodeMarketplace(const ExtensionSearchQuery& query);
    ExtensionMetadata getVSCodeExtensionDetails(const std::string& extensionId);
    std::string buildSearchQuery(const ExtensionSearchQuery& query);
    ExtensionMetadata parseExtensionMetadata(const std::string& jsonData);
    
    // Local registry operations
    void loadLocalRegistry();
    void saveLocalRegistry();
    void updateLocalExtension(const ExtensionMetadata& extension);
    void removeLocalExtension(const std::string& extensionId);
    
    // Cache operations
    std::string getCacheKey(const std::string& operation, const std::string& parameter);
    
    // Utility
    std::string categoryToString(ExtensionFilterCategory category) const;
    ExtensionFilterCategory stringToCategory(const std::string& category) const;
    bool checkVSCodeCompatibility(const std::vector<std::string>& engines, const std::string& vsCodeVersion) const;
};

// ============================================================================
// Global Marketplace Backend Instance
// ============================================================================

// Singleton instance for global access
MarketplaceBackend& GetMarketplaceBackend();

// Initialize marketplace with custom configuration
bool InitializeMarketplace(const MarketplaceConfig& config = MarketplaceConfig::getDefault());

// Shutdown marketplace backend
void ShutdownMarketplace();
