// VSCodeMarketplaceAPI.hpp — VS Code Marketplace API client for RawrXD IDE
// Reverse-engineered from marketplace.visualstudio.com; list extensions and download .vsix
// Uses: extensionquery (POST) for search/list, gallery.vsassets.io for .vsix download

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace VSCodeMarketplace {

struct MarketplaceEntry {
    std::string id;              // "publisher.extensionName"
    std::string publisher;
    std::string extensionName;
    std::string displayName;
    std::string shortDescription;
    std::string version;         // latest version for download
    uint64_t    installCount;
    double      averageRating;
    int         ratingCount;
};

// Query the VS Code Marketplace (search or browse).
// searchTerm: optional; if empty, returns first page of "Microsoft.VisualStudio.Code" target.
// pageSize: max 1000, pageNumber: 1-based.
// Returns true on success and fills out; false on network/parse error.
bool Query(const std::string& searchTerm, int pageSize, int pageNumber,
           std::vector<MarketplaceEntry>& out);

// Fetch a single extension by id "publisher.extensionname" (e.g. "GitHub.copilot").
// Fills version so you can call DownloadVsix.
bool GetById(const std::string& publisherDotExtension, MarketplaceEntry& out);

// Download .vsix to savePath. Uses gallery.vsassets.io URL.
// Returns true if file was written successfully.
bool DownloadVsix(const std::string& publisher, const std::string& extensionName,
                  const std::string& version, const std::string& savePath);

// Build the public marketplace item URL for opening in browser.
std::string ItemUrl(const std::string& publisher, const std::string& extensionName);

} // namespace VSCodeMarketplace
