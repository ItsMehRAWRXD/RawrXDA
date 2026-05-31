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
#include <nlohmann/json.hpp>
#include <mutex>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;
using json = nlohmann::json;

static std::string HashKey(const std::string& key) {
    std::hash<std::string> hasher;
    size_t hash = hasher(key);
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

static int64_t CurrentTimeSecs() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static std::string GetAppDataPath() {
    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path))) {
        std::wstring wpath(path);
        CoTaskMemFree(path);
        int n = WideCharToMultiByte(CP_UTF8, 0, &wpath[0], (int)wpath.size(), NULL, 0, NULL, NULL);
        std::string s(n, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wpath[0], (int)wpath.size(), &s[0], n, NULL, NULL);
        return s + "\\RawrXD\\marketplace_cache";
    }
    return "C:\\RawrXD\\marketplace_cache";
}

OfflineCacheStore::OfflineCacheStore()
    : m_cacheSizeLimit(100 * 1024 * 1024)
    , m_cacheExpirationDays(30)
    , m_currentCacheSize(0)
{
    initializeCacheDirectory();
    updateCacheSize();
}

OfflineCacheStore::~OfflineCacheStore() {}

void OfflineCacheStore::initializeCacheDirectory() {
    m_cacheDir = GetAppDataPath();
    fs::create_directories(m_cacheDir);
}

void OfflineCacheStore::updateCacheSize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentCacheSize = 0;
    m_cacheEntries.clear();
    if (!fs::exists(m_cacheDir)) return;
    for (const auto& entry : fs::recursive_directory_iterator(m_cacheDir)) {
        if (entry.is_regular_file()) m_currentCacheSize += (int64_t)entry.file_size();
    }
}

std::string OfflineCacheStore::getCacheFilePath(const std::string& key) {
    return m_cacheDir + "\\" + HashKey(key) + ".json";
}

void OfflineCacheStore::cacheSearchResults(const std::string& query, const std::string& resultsJson) {
    std::string filePath = getCacheFilePath("search_" + query);
    json wrapper;
    wrapper["timestamp"] = CurrentTimeSecs();
    try { wrapper["data"] = json::parse(resultsJson); } catch(...) { wrapper["data"] = resultsJson; }
    std::ofstream file(filePath);
    if (file) { file << wrapper.dump(4); file.close(); updateCacheSize(); enforceLimits(); }
}

std::string OfflineCacheStore::getCachedSearchResults(const std::string& query) {
    std::string filePath = getCacheFilePath("search_" + query);
    if (!fs::exists(filePath)) return {};
    std::ifstream file(filePath);
    try {
        std::string _fc((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()); json wrapper = json::parse(_fc);
        int64_t ts = wrapper.value("timestamp", (int64_t)0);
        if (CurrentTimeSecs() - ts > (int64_t)m_cacheExpirationDays * 86400) { fs::remove(filePath); return {}; }
        return wrapper["data"].dump();
    } catch (...) { return {}; }
}

void OfflineCacheStore::cacheExtensionDetails(const std::string& extensionId, const std::string& detailsJson) {
    std::string filePath = getCacheFilePath("details_" + extensionId);
    json wrapper;
    wrapper["timestamp"] = CurrentTimeSecs();
    try { wrapper["data"] = json::parse(detailsJson); } catch(...) { wrapper["data"] = detailsJson; }
    std::ofstream file(filePath);
    if (file) { file << wrapper.dump(4); updateCacheSize(); enforceLimits(); }
}

std::string OfflineCacheStore::getCachedExtensionDetails(const std::string& extensionId) {
    std::string filePath = getCacheFilePath("details_" + extensionId);
    if (!fs::exists(filePath)) return {};
    std::ifstream file(filePath);
    try {
        std::string _fc((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()); json wrapper = json::parse(_fc);
        int64_t ts = wrapper.value("timestamp", (int64_t)0);
        if (CurrentTimeSecs() - ts > (int64_t)m_cacheExpirationDays * 86400) { fs::remove(filePath); return {}; }
        return wrapper["data"].dump();
    } catch (...) { return {}; }
}

void OfflineCacheStore::cacheExtensionBundle(const std::string& extensionId, const std::string& bundlePath) {
    std::string filePath = m_cacheDir + "\\bundle_" + extensionId + ".vsix";
    try { fs::copy_file(bundlePath, filePath, fs::copy_options::overwrite_existing); updateCacheSize(); enforceLimits(); } catch (...) {}
}

bool OfflineCacheStore::hasCachedBundle(const std::string& extensionId) {
    return fs::exists(m_cacheDir + "\\bundle_" + extensionId + ".vsix");
}

void OfflineCacheStore::clearCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (fs::exists(m_cacheDir)) {
        for (const auto& entry : fs::directory_iterator(m_cacheDir)) fs::remove_all(entry.path());
    }
    m_currentCacheSize = 0;
    m_cacheEntries.clear();
    if (m_onCacheCleared) m_onCacheCleared();
}

void OfflineCacheStore::setCacheSizeLimit(int64_t bytes) {
    m_cacheSizeLimit = bytes;
    enforceLimits();
}

void OfflineCacheStore::setCacheExpirationDays(int days) {
    m_cacheExpirationDays = days;
}

void OfflineCacheStore::cleanupExpiredEntries() {
    if (!fs::exists(m_cacheDir)) return;
    int64_t now = CurrentTimeSecs();
    for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
        if (!entry.is_regular_file()) continue;
        std::ifstream f(entry.path());
        try {
            std::string _fc2((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); json w = json::parse(_fc2);
            if (now - w.value("timestamp", (int64_t)0) > (int64_t)m_cacheExpirationDays * 86400)
                fs::remove(entry.path());
        } catch (...) {}
    }
}

void OfflineCacheStore::enforceLimits() {
    if (m_currentCacheSize <= m_cacheSizeLimit) return;
    std::vector<std::pair<int64_t, fs::path>> files;
    for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
        if (entry.is_regular_file())
            files.push_back({ fs::last_write_time(entry).time_since_epoch().count(), entry.path() });
    }
    std::sort(files.begin(), files.end());
    for (const auto& p : files) {
        if (m_currentCacheSize <= m_cacheSizeLimit) break;
        try { uintmax_t sz = fs::file_size(p.second); fs::remove(p.second); m_currentCacheSize -= (int64_t)sz; } catch (...) {}
    }
}

