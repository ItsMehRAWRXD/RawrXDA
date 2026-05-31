/**
 * @file marketplace_client.h
 * @brief Extension discovery and management
 * 
 * @author RawrXD Extension Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>

namespace RawrXD::Extensions {

// ============================================================================
// Sort Options
// ============================================================================

enum class SortBy {
    Relevance,
    Downloads,
    Rating,
    Updated
};

// ============================================================================
// Search Filter
// ============================================================================

struct SearchFilter {
    std::string category;
    float minRating = 0.0f;
    SortBy sortBy = SortBy::Relevance;
    int maxResults = 50;
};

// ============================================================================
// Marketplace Extension
// ============================================================================

struct MarketplaceExtension {
    std::string id;
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    std::string downloadUrl;
    std::string iconUrl;
    float rating = 0.0f;
    int downloadCount = 0;
    std::string category;
    std::vector<std::string> tags;
    int apiVersion = 1;
    bool installed = false;
};

// ============================================================================
// Update Info
// ============================================================================

struct UpdateInfo {
    std::string extensionId;
    std::string currentVersion;
    std::string latestVersion;
    std::string downloadUrl;
    std::string changelog;
};

// ============================================================================
// Review
// ============================================================================

struct Review {
    std::string author;
    int rating = 0;
    std::string text;
    std::string date;
};

// ============================================================================
// Client Configuration
// ============================================================================

struct ClientConfig {
    std::string marketplaceUrl = "https://marketplace.rawrxd.dev";
    std::string installDir = "./extensions";
    std::string tempDir = "./temp";
    std::string apiKey;
    int timeoutMs = 30000;
};

// ============================================================================
// Marketplace Client
// ============================================================================

class MarketplaceClient {
public:
    explicit MarketplaceClient(const ClientConfig& config);
    ~MarketplaceClient();
    
    // Extension discovery
    std::vector<MarketplaceExtension> searchExtensions(const std::string& query,
                                                        const SearchFilter& filter);
    std::vector<MarketplaceExtension> getFeaturedExtensions();
    std::vector<MarketplaceExtension> getInstalledExtensions();
    
    // Extension installation
    bool installExtension(const std::string& extensionId);
    bool uninstallExtension(const std::string& extensionId);
    bool updateExtension(const std::string& extensionId);
    
    // Update checking
    std::vector<UpdateInfo> checkForUpdates();
    
    // Rating and reviews
    bool submitReview(const std::string& extensionId, int rating,
                     const std::string& review);
    std::vector<Review> getReviews(const std::string& extensionId);
    
private:
    std::string performRequest(const std::string& url);
    std::string performPostRequest(const std::string& url, const std::string& data);
    bool downloadFile(const std::string& url, const std::string& path);
    bool extractArchive(const std::string& archivePath, const std::string& extractPath);
    
    ClientConfig m_config;
    void* m_curl;
};

} // namespace RawrXD::Extensions
