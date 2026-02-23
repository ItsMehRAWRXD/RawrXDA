#pragma once
#include <string>
#include <vector>

namespace VSCodeMarketplace {

struct MarketplaceEntry;

bool Query(const std::string& searchTerm, int page, int pageSize, 
           std::vector<MarketplaceEntry>& results);

bool DownloadVsix(const std::string& publisher, const std::string& extName, 
                  const std::string& version, const std::string& destPath);

} // namespace VSCodeMarketplace
