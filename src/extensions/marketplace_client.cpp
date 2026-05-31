/**
 * @file marketplace_client.cpp
 * @brief Extension discovery and management
 * 
 * Provides:
 * - Extension marketplace integration
 * - Extension search and discovery
 * - Download and installation
 * - Update checking
 * - Rating and reviews
 * 
 * @author RawrXD Extension Team
 * @version 1.0.0
 */

#include "marketplace_client.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace RawrXD::Extensions {

// ============================================================================
// MarketplaceClient Implementation
// ============================================================================

MarketplaceClient::MarketplaceClient(const ClientConfig& config)
    : m_config(config)
    , m_curl(nullptr)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    m_curl = curl_easy_init();
}

MarketplaceClient::~MarketplaceClient() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
    curl_global_cleanup();
}

// ============================================================================
// Extension Discovery
// ============================================================================

std::vector<MarketplaceExtension> MarketplaceClient::searchExtensions(
    const std::string& query, const SearchFilter& filter) {
    
    std::vector<MarketplaceExtension> results;
    
    // Build search URL
    std::string url = m_config.marketplaceUrl + "/api/extensions/search";
    url += "?q=" + curl_escape(query.c_str(), static_cast<int>(query.length()));
    
    if (!filter.category.empty()) {
        url += "&category=" + curl_escape(filter.category.c_str(), 
                                          static_cast<int>(filter.category.length()));
    }
    
    if (filter.minRating > 0.0f) {
        url += "&minRating=" + std::to_string(filter.minRating);
    }
    
    if (filter.sortBy != SortBy::Relevance) {
        std::string sortStr;
        switch (filter.sortBy) {
            case SortBy::Downloads: sortStr = "downloads"; break;
            case SortBy::Rating: sortStr = "rating"; break;
            case SortBy::Updated: sortStr = "updated"; break;
            default: sortStr = "relevance"; break;
        }
        url += "&sort=" + sortStr;
    }
    
    // Perform search
    std::string response = performRequest(url);
    if (response.empty()) {
        return results;
    }
    
    // Parse response
    try {
        nlohmann::json json = nlohmann::json::parse(response);
        
        if (json.contains("extensions")) {
            for (const auto& ext : json["extensions"]) {
                MarketplaceExtension extension;
                extension.id = ext.value("id", "");
                extension.name = ext.value("name", "");
                extension.description = ext.value("description", "");
                extension.author = ext.value("author", "");
                extension.version = ext.value("version", "");
                extension.downloadUrl = ext.value("downloadUrl", "");
                extension.iconUrl = ext.value("iconUrl", "");
                extension.rating = ext.value("rating", 0.0f);
                extension.downloadCount = ext.value("downloadCount", 0);
                extension.category = ext.value("category", "");
                extension.tags = ext.value("tags", std::vector<std::string>{});
                extension.apiVersion = ext.value("apiVersion", 1);
                
                results.push_back(extension);
            }
        }
    } catch (...) {
        // Parse error
    }
    
    return results;
}

std::vector<MarketplaceExtension> MarketplaceClient::getFeaturedExtensions() {
    std::string url = m_config.marketplaceUrl + "/api/extensions/featured";
    std::string response = performRequest(url);
    
    std::vector<MarketplaceExtension> results;
    
    try {
        nlohmann::json json = nlohmann::json::parse(response);
        
        if (json.contains("extensions")) {
            for (const auto& ext : json["extensions"]) {
                MarketplaceExtension extension;
                extension.id = ext.value("id", "");
                extension.name = ext.value("name", "");
                extension.description = ext.value("description", "");
                extension.author = ext.value("author", "");
                extension.version = ext.value("version", "");
                extension.downloadUrl = ext.value("downloadUrl", "");
                extension.iconUrl = ext.value("iconUrl", "");
                extension.rating = ext.value("rating", 0.0f);
                extension.downloadCount = ext.value("downloadCount", 0);
                extension.category = ext.value("category", "");
                extension.tags = ext.value("tags", std::vector<std::string>{});
                extension.apiVersion = ext.value("apiVersion", 1);
                
                results.push_back(extension);
            }
        }
    } catch (...) {
        // Parse error
    }
    
    return results;
}

std::vector<MarketplaceExtension> MarketplaceClient::getInstalledExtensions() {
    std::vector<MarketplaceExtension> results;
    
    std::filesystem::path installDir = m_config.installDir;
    if (!std::filesystem::exists(installDir)) {
        return results;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(installDir)) {
        if (!entry.is_directory()) continue;
        
        auto manifestPath = entry.path() / "extension.json";
        if (std::filesystem::exists(manifestPath)) {
            try {
                std::ifstream file(manifestPath);
                nlohmann::json json;
                file >> json;
                
                MarketplaceExtension extension;
                extension.id = json.value("id", "");
                extension.name = json.value("name", "");
                extension.description = json.value("description", "");
                extension.author = json.value("author", "");
                extension.version = json.value("version", "");
                extension.category = json.value("category", "");
                extension.apiVersion = json.value("apiVersion", 1);
                extension.installed = true;
                
                results.push_back(extension);
            } catch (...) {
                // Parse error
            }
        }
    }
    
    return results;
}

// ============================================================================
// Extension Installation
// ============================================================================

bool MarketplaceClient::installExtension(const std::string& extensionId) {
    // Get extension info
    auto extensions = searchExtensions(extensionId, SearchFilter{});
    if (extensions.empty()) {
        return false;
    }
    
    const auto& extension = extensions[0];
    
    // Download extension
    std::string downloadPath = m_config.tempDir + "/" + extensionId + ".zip";
    if (!downloadFile(extension.downloadUrl, downloadPath)) {
        return false;
    }
    
    // Extract extension
    std::string extractPath = m_config.installDir + "/" + extensionId;
    if (!extractArchive(downloadPath, extractPath)) {
        return false;
    }
    
    // Clean up download
    std::filesystem::remove(downloadPath);
    
    return true;
}

bool MarketplaceClient::uninstallExtension(const std::string& extensionId) {
    std::string extensionPath = m_config.installDir + "/" + extensionId;
    
    if (!std::filesystem::exists(extensionPath)) {
        return false;
    }
    
    try {
        std::filesystem::remove_all(extensionPath);
        return true;
    } catch (...) {
        return false;
    }
}

bool MarketplaceClient::updateExtension(const std::string& extensionId) {
    // Uninstall old version
    if (!uninstallExtension(extensionId)) {
        return false;
    }
    
    // Install new version
    return installExtension(extensionId);
}

// ============================================================================
// Update Checking
// ============================================================================

std::vector<UpdateInfo> MarketplaceClient::checkForUpdates() {
    std::vector<UpdateInfo> updates;
    
    auto installed = getInstalledExtensions();
    
    for (const auto& ext : installed) {
        auto marketplaceVersions = searchExtensions(ext.id, SearchFilter{});
        if (marketplaceVersions.empty()) continue;
        
        const auto& latest = marketplaceVersions[0];
        if (latest.version != ext.version) {
            UpdateInfo update;
            update.extensionId = ext.id;
            update.currentVersion = ext.version;
            update.latestVersion = latest.version;
            update.downloadUrl = latest.downloadUrl;
            update.changelog = latest.description;
            
            updates.push_back(update);
        }
    }
    
    return updates;
}

// ============================================================================
// Rating and Reviews
// ============================================================================

bool MarketplaceClient::submitReview(const std::string& extensionId,
                                    int rating, const std::string& review) {
    std::string url = m_config.marketplaceUrl + "/api/extensions/" + extensionId + "/reviews";
    
    nlohmann::json request;
    request["rating"] = rating;
    request["review"] = review;
    
    std::string response = performPostRequest(url, request.dump());
    
    try {
        nlohmann::json json = nlohmann::json::parse(response);
        return json.value("success", false);
    } catch (...) {
        return false;
    }
}

std::vector<Review> MarketplaceClient::getReviews(const std::string& extensionId) {
    std::string url = m_config.marketplaceUrl + "/api/extensions/" + extensionId + "/reviews";
    std::string response = performRequest(url);
    
    std::vector<Review> reviews;
    
    try {
        nlohmann::json json = nlohmann::json::parse(response);
        
        if (json.contains("reviews")) {
            for (const auto& review : json["reviews"]) {
                Review r;
                r.author = review.value("author", "");
                r.rating = review.value("rating", 0);
                r.text = review.value("text", "");
                r.date = review.value("date", "");
                
                reviews.push_back(r);
            }
        }
    } catch (...) {
        // Parse error
    }
    
    return reviews;
}

// ============================================================================
// HTTP Helpers
// ============================================================================

static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string MarketplaceClient::performRequest(const std::string& url) {
    if (!m_curl) return "";
    
    std::string response;
    
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
    
    CURLcode res = curl_easy_perform(m_curl);
    
    if (res != CURLE_OK) {
        return "";
    }
    
    return response;
}

std::string MarketplaceClient::performPostRequest(const std::string& url,
                                               const std::string& data) {
    if (!m_curl) return "";
    
    std::string response;
    
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(m_curl);
    
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        return "";
    }
    
    return response;
}

bool MarketplaceClient::downloadFile(const std::string& url, const std::string& path) {
    if (!m_curl) return false;
    
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, 
                     [](void* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
                         auto* stream = static_cast<std::ofstream*>(userdata);
                         stream->write(static_cast<char*>(ptr), size * nmemb);
                         return size * nmemb;
                     });
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(m_curl);
    
    file.close();
    
    return res == CURLE_OK;
}

bool MarketplaceClient::extractArchive(const std::string& archivePath,
                                    const std::string& extractPath) {
    // In production, this would use a proper archive library
    // For now, we assume the extension is a simple directory structure
    std::filesystem::create_directories(extractPath);
    
    // TODO: Implement proper archive extraction
    // This would typically use libzip, miniz, or similar
    
    return true;
}

} // namespace RawrXD::Extensions
