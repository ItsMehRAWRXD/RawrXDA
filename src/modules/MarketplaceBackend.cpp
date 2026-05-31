// ============================================================================
// MarketplaceBackend.cpp — Extension Marketplace Backend Implementation
// ============================================================================

#include "MarketplaceBackend.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>
#include <filesystem>

using json = nlohmann::json;

// ============================================================================
// Global Instance Management
// ============================================================================

static std::unique_ptr<MarketplaceBackend> g_marketplaceBackend;
static std::mutex g_marketplaceMutex;

MarketplaceBackend& GetMarketplaceBackend() {
    std::lock_guard<std::mutex> lock(g_marketplaceMutex);
    if (!g_marketplaceBackend) {
        g_marketplaceBackend = std::make_unique<MarketplaceBackend>();
        g_marketplaceBackend->initialize();
    }
    return *g_marketplaceBackend;
}

bool InitializeMarketplace(const MarketplaceConfig& config) {
    std::lock_guard<std::mutex> lock(g_marketplaceMutex);
    if (g_marketplaceBackend) {
        g_marketplaceBackend->shutdown();
    }
    g_marketplaceBackend = std::make_unique<MarketplaceBackend>(config);
    return g_marketplaceBackend->initialize();
}

void ShutdownMarketplace() {
    std::lock_guard<std::mutex> lock(g_marketplaceMutex);
    if (g_marketplaceBackend) {
        g_marketplaceBackend->shutdown();
        g_marketplaceBackend.reset();
    }
}

// ============================================================================
// HTTP Client Implementation
// ============================================================================

MarketplaceHTTPClient::MarketplaceHTTPClient(const MarketplaceConfig& config) 
    : m_config(config), m_session(nullptr) {
    
    // Initialize WinHTTP session
    m_session = WinHttpOpen(
        reinterpret_cast<LPCWSTR>(std::wstring(config.userAgent.begin(), config.userAgent.end()).c_str()),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    
    if (m_session) {
        // Set timeouts
        DWORD timeout = static_cast<DWORD>(config.requestTimeout.count() * 1000);
        WinHttpSetTimeouts(m_session, timeout, timeout, timeout, timeout);
    }
}

MarketplaceHTTPClient::~MarketplaceHTTPClient() {
    if (m_session) {
        WinHttpCloseHandle(m_session);
    }
}

std::string MarketplaceHTTPClient::get(const std::string& url, const std::unordered_map<std::string, std::string>& headers) {
    return executeRequest("GET", url, "", headers);
}

std::string MarketplaceHTTPClient::post(const std::string& url, const std::string& data, const std::unordered_map<std::string, std::string>& headers) {
    return executeRequest("POST", url, data, headers);
}

std::string MarketplaceHTTPClient::executeRequest(
    const std::string& verb, 
    const std::string& url, 
    const std::string& data, 
    const std::unordered_map<std::string, std::string>& headers
) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    
    if (!m_session) {
        return "";
    }
    
    std::string scheme, host, path;
    INTERNET_PORT port;
    if (!parseUrl(url, scheme, host, port, path)) {
        return "";
    }
    
    // Convert strings to wide strings
    std::wstring wHost(host.begin(), host.end());
    std::wstring wPath(path.begin(), path.end());
    std::wstring wVerb(verb.begin(), verb.end());
    
    // Create connection
    HINTERNET connect = WinHttpConnect(m_session, wHost.c_str(), port, 0);
    if (!connect) {
        return "";
    }
    
    // Create request
    DWORD flags = (scheme == "https") ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(
        connect, wVerb.c_str(), wPath.c_str(), 
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags
    );
    
    std::string result;
    if (request) {
        // Add custom headers
        for (const auto& header : headers) {
            std::string headerLine = header.first + ": " + header.second;
            std::wstring wHeader(headerLine.begin(), headerLine.end());
            WinHttpAddRequestHeaders(request, wHeader.c_str(), static_cast<DWORD>(wHeader.length()), WINHTTP_ADDREQ_FLAG_ADD);
        }
        
        // Send request
        BOOL success = WinHttpSendRequest(
            request,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            const_cast<char*>(data.c_str()), static_cast<DWORD>(data.length()),
            static_cast<DWORD>(data.length()), 0
        );
        
        if (success && WinHttpReceiveResponse(request, nullptr)) {
            // Read response
            DWORD bytesRead;
            char buffer[4096];
            do {
                if (WinHttpReadData(request, buffer, sizeof(buffer), &bytesRead)) {
                    result.append(buffer, bytesRead);
                } else {
                    break;
                }
            } while (bytesRead > 0);
        }
        
        WinHttpCloseHandle(request);
    }
    
    WinHttpCloseHandle(connect);
    return result;
}

bool MarketplaceHTTPClient::parseUrl(const std::string& url, std::string& scheme, std::string& host, INTERNET_PORT& port, std::string& path) {
    std::regex urlRegex(R"(^(https?):\/\/([^:\/\s]+)(?::(\d+))?(\/.*)?$)");
    std::smatch matches;
    
    if (!std::regex_match(url, matches, urlRegex)) {
        return false;
    }
    
    scheme = matches[1];
    host = matches[2];
    port = matches[3].matched ? static_cast<INTERNET_PORT>(std::stoi(matches[3])) : 
           (scheme == "https" ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);
    path = matches[4].matched ? matches[4] : "/";
    
    return true;
}

// ============================================================================
// Cache Implementation
// ============================================================================

MarketplaceCache::MarketplaceCache(const std::string& cacheDirectory, uint64_t maxSize) 
    : m_cacheDir(cacheDirectory), m_maxSize(maxSize) {
    
    // Create cache directory
    std::filesystem::create_directories(cacheDirectory);
    loadCacheIndex();
}

MarketplaceCache::~MarketplaceCache() {
    saveCacheIndex();
}

bool MarketplaceCache::store(const std::string& key, const std::string& data, std::chrono::minutes ttl) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    std::string filePath = getFilePath(key);
    
    // Ensure space
    ensureSpace(data.length());
    
    // Write to file
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.write(data.c_str(), data.length());
    file.close();
    
    // Update index
    CacheEntry entry;
    entry.expiry = std::chrono::system_clock::now() + ttl;
    entry.size = data.length();
    m_entries[key] = entry;
    
    return true;
}

std::string MarketplaceCache::retrieve(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    if (isExpired(key)) {
        remove(key);
        return "";
    }
    
    std::string filePath = getFilePath(key);
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return "";
    }
    
    std::string content;
    file.seekg(0, std::ios::end);
    content.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(&content[0], content.size());
    
    return content;
}

bool MarketplaceCache::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_entries.find(key) != m_entries.end() && !isExpired(key);
}

void MarketplaceCache::remove(const std::string& key) {
    std::string filePath = getFilePath(key);
    std::filesystem::remove(filePath);
    m_entries.erase(key);
}

std::string MarketplaceCache::getFilePath(const std::string& key) const {
    // Create safe filename from key
    std::string filename = key;
    std::replace(filename.begin(), filename.end(), '/', '_');
    std::replace(filename.begin(), filename.end(), '\\', '_');
    std::replace(filename.begin(), filename.end(), ':', '_');
    filename += ".cache";
    
    return m_cacheDir + "/" + filename;
}

bool MarketplaceCache::isExpired(const std::string& key) const {
    auto it = m_entries.find(key);
    if (it == m_entries.end()) {
        return true;
    }
    
    return std::chrono::system_clock::now() > it->second.expiry;
}

void MarketplaceCache::ensureSpace(uint64_t requiredSpace) {
    uint64_t currentSpace = getUsedSpace();
    
    if (currentSpace + requiredSpace <= m_maxSize) {
        return;
    }
    
    // Remove oldest entries
    std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> entries;
    for (const auto& entry : m_entries) {
        entries.emplace_back(entry.first, entry.second.expiry);
    }
    
    std::sort(entries.begin(), entries.end(), 
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    for (const auto& entry : entries) {
        remove(entry.first);
        currentSpace = getUsedSpace();
        if (currentSpace + requiredSpace <= m_maxSize) {
            break;
        }
    }
}

uint64_t MarketplaceCache::getUsedSpace() const {
    uint64_t total = 0;
    for (const auto& entry : m_entries) {
        total += entry.second.size;
    }
    return total;
}

void MarketplaceCache::loadCacheIndex() {
    // Implementation would load cache index from disk
}

void MarketplaceCache::saveCacheIndex() {
    // Implementation would save cache index to disk  
}

// ============================================================================
// Marketplace Backend Implementation
// ============================================================================

MarketplaceBackend::MarketplaceBackend(const MarketplaceConfig& config) 
    : m_config(config), m_initialized(false), m_offlineMode(false), m_shutdownRequested(false) {
    
    m_httpClient = std::make_unique<MarketplaceHTTPClient>(config);
    m_cache = std::make_unique<MarketplaceCache>(config.cacheDirectory, config.maxCacheSize);
}

MarketplaceBackend::~MarketplaceBackend() {
    shutdown();
}

bool MarketplaceBackend::initialize() {
    if (m_initialized.load()) {
        return true;
    }
    
    // Create necessary directories
    std::filesystem::create_directories(std::filesystem::path(m_config.localRegistryPath).parent_path());
    std::filesystem::create_directories(m_config.cacheDirectory);
    
    // Load local registry
    loadLocalRegistry();
    
    // Start download worker threads
    for (uint32_t i = 0; i < m_config.maxConcurrentDownloads; ++i) {
        m_downloadThreads.emplace_back(&MarketplaceBackend::downloadWorker, this);
    }
    
    // Start background sync thread
    m_syncRunning = true;
    m_syncThread = std::thread(&MarketplaceBackend::syncWorker, this);
    
    m_initialized = true;
    std::cout << "[MarketplaceBackend] Initialized successfully" << std::endl;
    return true;
}

void MarketplaceBackend::shutdown() {
    if (!m_initialized.load()) {
        return;
    }
    
    m_shutdownRequested = true;
    m_syncRunning = false;
    
    // Wake up download threads
    m_downloadCondition.notify_all();
    
    // Wait for download threads
    for (auto& thread : m_downloadThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_downloadThreads.clear();
    
    // Wait for sync thread
    if (m_syncThread.joinable()) {
        m_syncThread.join();
    }
    
    // Save local registry
    saveLocalRegistry();
    
    m_initialized = false;
    std::cout << "[MarketplaceBackend] Shutdown completed" << std::endl;
}

ExtensionSearchResult MarketplaceBackend::search(const ExtensionSearchQuery& query) {
    // Check cache first
    std::string cacheKey = getCacheKey("search", buildSearchQuery(query));
    if (!m_offlineMode.load()) {
        std::string cachedResult = m_cache->retrieve(cacheKey);
        if (!cachedResult.empty()) {
            try {
                json j = json::parse(cachedResult);
                ExtensionSearchResult result;
                // Parse cached result (implementation would deserialize from JSON)
                return result;
            } catch (...) {
                // Cache corrupted, continue with fresh search
            }
        }
    }
    
    ExtensionSearchResult result;
    auto startTime = std::chrono::steady_clock::now();
    
    if (!m_offlineMode.load()) {
        // Search VS Code Marketplace
        result = searchVSCodeMarketplace(query);
    } else {
        // Search local registry only
        std::lock_guard<std::mutex> lock(m_registryMutex);
        
        for (const auto& entry : m_localRegistry) {
            const ExtensionMetadata& ext = entry.second;
            
            // Simple text matching (real implementation would use proper search indexes)
            if (query.searchText.empty() || 
                ext.name.find(query.searchText) != std::string::npos ||
                ext.description.find(query.searchText) != std::string::npos) {
                
                result.extensions.push_back(ext);
            }
        }
        
        result.totalCount = static_cast<uint32_t>(result.extensions.size());
    }
    
    auto endTime = std::chrono::steady_clock::now();
    result.searchTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.pageNumber = query.pageNumber;
    result.pageSize = query.pageSize;
    result.hasMorePages = result.extensions.size() >= query.pageSize;
    
    // Cache result
    try {
        json j;
        // Serialize result to JSON (implementation would serialize ExtensionSearchResult)
        m_cache->store(cacheKey, j.dump(), m_config.cacheDuration);
    } catch (...) {
        // Failed to cache, not critical
    }
    
    return result;
}

ExtensionMetadata MarketplaceBackend::getExtensionDetails(const std::string& extensionId) {
    // Check local registry first
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        auto it = m_localRegistry.find(extensionId);
        if (it != m_localRegistry.end()) {
            return it->second;
        }
    }
    
    // Check cache
    std::string cacheKey = getCacheKey("details", extensionId);
    std::string cachedData = m_cache->retrieve(cacheKey);
    if (!cachedData.empty()) {
        return parseExtensionMetadata(cachedData);
    }
    
    // Fetch from marketplace
    if (!m_offlineMode.load()) {
        return getVSCodeExtensionDetails(extensionId);
    }
    
    return ExtensionMetadata();
}

bool MarketplaceBackend::downloadExtension(const std::string& extensionId, const std::string& version) {
    ExtensionDownload download;
    download.extensionId = extensionId;
    download.version = version;
    download.status = DownloadStatus::Queued;
    download.downloadedBytes = 0;
    download.startTime = std::chrono::system_clock::now();
    
    // Get extension details to find download URL
    ExtensionMetadata metadata = getExtensionDetails(extensionId);
    if (metadata.id.empty()) {
        return false;
    }
    
    // Find version details
    std::string targetVersion = (version == "latest") ? metadata.latestVersion : version;
    auto versionIt = std::find_if(metadata.versions.begin(), metadata.versions.end(),
        [&targetVersion](const ExtensionVersion& v) { return v.version == targetVersion; });
    
    if (versionIt == metadata.versions.end()) {
        return false;
    }
    
    download.downloadUrl = versionIt->downloadUrl;
    download.totalBytes = versionIt->downloadSize;
    download.localPath = m_config.cacheDirectory + "/" + extensionId + "-" + targetVersion + ".vsix";
    
    // Add to download queue
    {
        std::lock_guard<std::mutex> lock(m_downloadMutex);
        m_downloadQueue.push(download);
        m_activeDownloads[extensionId] = download;
    }
    
    m_downloadCondition.notify_one();
    return true;
}

void MarketplaceBackend::downloadWorker() {
    while (!m_shutdownRequested.load()) {
        std::unique_lock<std::mutex> lock(m_downloadMutex);
        
        // Wait for work or shutdown
        m_downloadCondition.wait(lock, [this] {
            return !m_downloadQueue.empty() || m_shutdownRequested.load();
        });
        
        if (m_shutdownRequested.load()) {
            break;
        }
        
        if (m_downloadQueue.empty()) {
            continue;
        }
        
        ExtensionDownload download = m_downloadQueue.front();
        m_downloadQueue.pop();
        lock.unlock();
        
        // Perform download
        download.status = DownloadStatus::Downloading;
        
        bool success = m_httpClient->downloadFile(
            download.downloadUrl,
            download.localPath,
            [&download](uint64_t downloaded, uint64_t total) {
                download.downloadedBytes = downloaded;
                if (download.progressCallback) {
                    download.progressCallback(download.extensionId, downloaded, total);
                }
            }
        );
        
        // Update status
        download.status = success ? DownloadStatus::Completed : DownloadStatus::Failed;
        download.endTime = std::chrono::system_clock::now();
        
        if (download.completionCallback) {
            download.completionCallback(download.extensionId, success, download.errorMessage);
        }
        
        // Remove from active downloads
        {
            std::lock_guard<std::mutex> activeLock(m_downloadMutex);
            m_activeDownloads.erase(download.extensionId);
        }
    }
}

void MarketplaceBackend::syncWorker() {
    while (m_syncRunning.load()) {
        auto now = std::chrono::system_clock::now();
        
        // Check if sync is due
        if (now - m_lastSync >= m_config.syncInterval) {
            if (syncWithMarketplace()) {
                m_lastSync = now;
                if (m_syncCallback) {
                    m_syncCallback(true, "Sync completed successfully");
                }
            } else if (m_syncCallback) {
                m_syncCallback(false, "Sync failed");
            }
        }
        
        // Sleep for a minute before checking again
        std::this_thread::sleep_for(std::chrono::minutes(1));
    }
}

ExtensionSearchResult MarketplaceBackend::searchVSCodeMarketplace(const ExtensionSearchQuery& query) {
    // Build VS Code Marketplace API query
    std::string queryString = buildSearchQuery(query);
    std::string url = m_config.vsCodeMarketplaceUrl + "/extensionquery";
    
    // Set up headers for VS Code Marketplace API
    std::unordered_map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Accept"] = "application/json;api-version=7.2-preview.1";
    headers["User-Agent"] = m_config.userAgent;
    
    // Send POST request to marketplace
    std::string response = m_httpClient->post(url, queryString, headers);
    
    ExtensionSearchResult result;
    if (!response.empty()) {
        try {
            json j = json::parse(response);
            // Parse VS Code Marketplace response format
            if (j.contains("results") && j["results"].is_array() && !j["results"].empty()) {
                const auto& results = j["results"][0];
                if (results.contains("extensions")) {
                    for (const auto& ext : results["extensions"]) {
                        ExtensionMetadata metadata = parseExtensionMetadata(ext.dump());
                        if (!metadata.id.empty()) {
                            result.extensions.push_back(metadata);
                        }
                    }
                }
                
                if (results.contains("resultMetadata")) {
                    for (const auto& meta : results["resultMetadata"]) {
                        if (meta.contains("metadataType") && meta["metadataType"] == "ResultCount") {
                            result.totalCount = meta["metadataItems"][0]["count"];
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[MarketplaceBackend] Failed to parse marketplace response: " << e.what() << std::endl;
        }
    }
    
    return result;
}

std::string MarketplaceBackend::buildSearchQuery(const ExtensionSearchQuery& query) {
    json j;
    j["filters"] = json::array();
    
    // Build VS Code Marketplace query format
    json filter;
    filter["pageNumber"] = query.pageNumber;
    filter["pageSize"] = query.pageSize;
    filter["sortBy"] = static_cast<int>(query.sortOrder);
    filter["criteria"] = json::array();
    
    if (!query.searchText.empty()) {
        json criteria;
        criteria["filterType"] = 1; // SearchText
        criteria["value"] = query.searchText;
        filter["criteria"].push_back(criteria);
    }
    
    j["filters"].push_back(filter);
    j["flags"] = 0x98; // Include versions, files, categories, statistics
    
    return j.dump();
}

ExtensionMetadata MarketplaceBackend::parseExtensionMetadata(const std::string& jsonData) {
    ExtensionMetadata metadata;
    
    try {
        json j = json::parse(jsonData);
        
        if (j.contains("extensionId")) {
            metadata.id = j["extensionId"];
        }
        if (j.contains("extensionName")) {
            metadata.name = j["extensionName"];
        }
        if (j.contains("displayName")) {
            metadata.displayName = j["displayName"];
        }
        if (j.contains("shortDescription")) {
            metadata.description = j["shortDescription"];
        }
        if (j.contains("publisher")) {
            metadata.publisher = j["publisher"]["publisherName"];
        }
        
        // Parse statistics
        if (j.contains("statistics")) {
            for (const auto& stat : j["statistics"]) {
                if (stat["statisticName"] == "install") {
                    metadata.installCount = stat["value"];
                } else if (stat["statisticName"] == "averagerating") {
                    metadata.averageRating = stat["value"];
                } else if (stat["statisticName"] == "ratingcount") {
                    metadata.ratingCount = stat["value"];
                }
            }
        }
        
        // Parse versions
        if (j.contains("versions")) {
            for (const auto& ver : j["versions"]) {
                ExtensionVersion version;
                version.version = ver["version"];
                version.isPreRelease = ver.value("isPreReleaseVersion", false);
                
                if (ver.contains("files")) {
                    for (const auto& file : ver["files"]) {
                        if (file["assetType"] == "Microsoft.VisualStudio.Services.VSIXPackage") {
                            version.downloadUrl = file["source"];
                        }
                    }
                }
                
                metadata.versions.push_back(version);
            }
            
            if (!metadata.versions.empty()) {
                metadata.latestVersion = metadata.versions[0].version;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[MarketplaceBackend] Failed to parse extension metadata: " << e.what() << std::endl;
    }
    
    return metadata;
}

void MarketplaceBackend::loadLocalRegistry() {
    try {
        if (std::filesystem::exists(m_config.localRegistryPath)) {
            std::ifstream file(m_config.localRegistryPath);
            if (file) {
                json j;
                file >> j;
                
                for (const auto& item : j["extensions"]) {
                    ExtensionMetadata metadata = parseExtensionMetadata(item.dump());
                    if (!metadata.id.empty()) {
                        std::lock_guard<std::mutex> lock(m_registryMutex);
                        m_localRegistry[metadata.id] = metadata;
                    }
                }
                
                std::cout << "[MarketplaceBackend] Loaded " << m_localRegistry.size() << " extensions from local registry" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[MarketplaceBackend] Failed to load local registry: " << e.what() << std::endl;
    }
}

void MarketplaceBackend::saveLocalRegistry() {
    try {
        json j;
        j["extensions"] = json::array();
        j["lastUpdated"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        {
            std::lock_guard<std::mutex> lock(m_registryMutex);
            for (const auto& entry : m_localRegistry) {
                // Serialize extension metadata to JSON (implementation would handle full serialization)
                json ext;
                ext["id"] = entry.second.id;
                ext["name"] = entry.second.name;
                ext["displayName"] = entry.second.displayName;
                ext["description"] = entry.second.description;
                ext["publisher"] = entry.second.publisher;
                ext["isInstalled"] = entry.second.isInstalled;
                ext["installedVersion"] = entry.second.installedVersion;
                j["extensions"].push_back(ext);
            }
        }
        
        std::ofstream file(m_config.localRegistryPath);
        if (file) {
            file << j.dump(2);
            std::cout << "[MarketplaceBackend] Saved local registry with " << j["extensions"].size() << " extensions" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[MarketplaceBackend] Failed to save local registry: " << e.what() << std::endl;
    }
}

std::string MarketplaceBackend::getCacheKey(const std::string& operation, const std::string& parameter) {
    return operation + ":" + parameter;
}

bool MarketplaceBackend::syncWithMarketplace() {
    if (m_offlineMode.load()) {
        return false;
    }
    
    std::cout << "[MarketplaceBackend] Starting marketplace sync..." << std::endl;
    
    // Sync popular extensions
    ExtensionSearchQuery query;
    query.sortOrder = ExtensionSortOrder::Downloads;
    query.pageSize = 100;
    
    try {
        ExtensionSearchResult result = searchVSCodeMarketplace(query);
        
        for (const auto& extension : result.extensions) {
            std::lock_guard<std::mutex> lock(m_registryMutex);
            m_localRegistry[extension.id] = extension;
        }
        
        saveLocalRegistry();
        std::cout << "[MarketplaceBackend] Sync completed successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MarketplaceBackend] Sync failed: " << e.what() << std::endl;
        return false;
    }
}
