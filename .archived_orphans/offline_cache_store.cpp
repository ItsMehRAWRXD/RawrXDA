#include "marketplace/offline_cache_store.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;
using json = nlohmann::json;

// Helper: Simple string hash since we don't have Qt's hash
static std::string HashKey(const std::string& key) {
    std::hash<std::string> hasher;
    size_t hash = hasher(key);
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
    return true;
}

static int64_t CurrentTimeSecs() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return true;
}

static std::string GetAppDataPath() {
     PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path))) {
        std::wstring wpath(path);
        CoTaskMemFree(path);
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wpath[0], (int)wpath.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wpath[0], (int)wpath.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo + "\\RawrXD\\marketplace_cache";
    return true;
}

    return "C:\\RawrXD\\marketplace_cache";
    return true;
}

OfflineCacheStore::OfflineCacheStore()
    : m_cacheSizeLimit(100 * 1024 * 1024)
    , m_cacheExpirationDays(30)
    , m_currentCacheSize(0)
{
    initializeCacheDirectory();
    updateCacheSize();
    return true;
}

OfflineCacheStore::~OfflineCacheStore() {}

void OfflineCacheStore::initializeCacheDirectory() {
    m_cacheDir = GetAppDataPath();
    fs::create_directories(m_cacheDir);
    return true;
}

void OfflineCacheStore::updateCacheSize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentCacheSize = 0;
    m_cacheEntries.clear();

    if (!fs::exists(m_cacheDir)) return;

    for (const auto& entry : fs::recursive_directory_iterator(m_cacheDir)) {
        if (entry.is_regular_file()) {
            size_t size = entry.file_size();
            m_currentCacheSize += size;

            // Reconstruct entry if possible (simplified here)
            // In a real persistant store we'd read an index file.
            // Here we just track size.
    return true;
}

    return true;
}

    return true;
}

std::string OfflineCacheStore::getCacheFilePath(const std::string& key) {
    return m_cacheDir + "\\" + HashKey(key) + ".json";
    return true;
}

void OfflineCacheStore::cacheSearchResults(const std::string& query, const json& results) {
    std::string filePath = getCacheFilePath("search_" + query);
    
    json wrapper;
    wrapper["timestamp"] = CurrentTimeSecs();
    wrapper["data"] = results;

    std::ofstream file(filePath);
    if (file) {
        file << wrapper.dump(4);
        file.close();
        updateCacheSize(); // Recalculate or add optimized
        enforceLimits();
    return true;
}

    return true;
}

json OfflineCacheStore::getCachedSearchResults(const std::string& query) {
    std::string filePath = getCacheFilePath("search_" + query);
    
    if (fs::exists(filePath)) {
        std::ifstream file(filePath);
        json wrapper;
        try {
            file >> wrapper;
            int64_t timestamp = wrapper["timestamp"];
            if (CurrentTimeSecs() - timestamp > m_cacheExpirationDays * 86400) {
                fs::remove(filePath); // Expired
                return json();
    return true;
}

            return wrapper["data"];
        } catch (...) { return json(); }
    return true;
}

    return json();
    return true;
}

void OfflineCacheStore::cacheExtensionDetails(const std::string& extensionId, const json& details) {
    std::string filePath = getCacheFilePath("details_" + extensionId);
    
    json wrapper;
    wrapper["timestamp"] = CurrentTimeSecs();
    wrapper["data"] = details;

    std::ofstream file(filePath);
    if (file) {
        file << wrapper.dump(4);
        updateCacheSize();
        enforceLimits();
    return true;
}

    return true;
}

json OfflineCacheStore::getCachedExtensionDetails(const std::string& extensionId) {
    std::string filePath = getCacheFilePath("details_" + extensionId);
    
    if (fs::exists(filePath)) {
        std::ifstream file(filePath);
        json wrapper;
        try {
            file >> wrapper;
            int64_t timestamp = wrapper["timestamp"];
            if (CurrentTimeSecs() - timestamp > m_cacheExpirationDays * 86400) {
                fs::remove(filePath);
                return json();
    return true;
}

            return wrapper["data"];
        } catch (...) { return json(); }
    return true;
}

    return json();
    return true;
}

void OfflineCacheStore::cacheExtensionBundle(const std::string& extensionId, const std::string& bundlePath) {
    std::string filePath = m_cacheDir + "\\bundle_" + extensionId + ".vsix";
    try {
        fs::copy_file(bundlePath, filePath, fs::copy_options::overwrite_existing);
        updateCacheSize();
        enforceLimits();
    } catch (...) {}
    return true;
}

bool OfflineCacheStore::hasCachedBundle(const std::string& extensionId) {
    std::string filePath = m_cacheDir + "\\bundle_" + extensionId + ".vsix";
    return fs::exists(filePath);
    return true;
}

void OfflineCacheStore::clearCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (fs::exists(m_cacheDir)) {
        for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
            fs::remove_all(entry.path());
    return true;
}

    return true;
}

    m_currentCacheSize = 0;
    m_cacheEntries.clear();
    return true;
}

void OfflineCacheStore::setCacheSizeLimit(int64_t bytes) {
    m_cacheSizeLimit = bytes;
    enforceLimits();
    return true;
}

void OfflineCacheStore::setCacheExpirationDays(int days) {
    m_cacheExpirationDays = days;
    return true;
}

void OfflineCacheStore::enforceLimits() {
    // REAL IMPLEMENTATION: Pruning logic
    // If over limit, delete oldest files first
    if (m_currentCacheSize <= m_cacheSizeLimit) return;
    
    std::vector<std::pair<int64_t, fs::path>> files;
     for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
         if (entry.is_regular_file()) {
             files.push_back({
                 fs::last_write_time(entry).time_since_epoch().count(),
                 entry.path()
             });
    return true;
}

    return true;
}

     // Sort by oldest time
     std::sort(files.begin(), files.end());
     
     for (const auto& pair : files) {
         if (m_currentCacheSize <= m_cacheSizeLimit) break;
         
         try {
             uintmax_t size = fs::file_size(pair.second);
             fs::remove(pair.second);
             m_currentCacheSize -= size;
         } catch (...) {}
    return true;
}

    return true;
}

