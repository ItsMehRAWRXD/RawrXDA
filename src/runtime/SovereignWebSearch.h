#pragma once
// =============================================================================
// SovereignWebSearch.h — Phase 49: Local SearXNG WinHTTP search client
// =============================================================================
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace RawrXD::Runtime {

struct SearchResult {
    std::string title;
    std::string url;
    std::string snippet;
};

// Forward-declare cache entry (defined in .cpp)
struct CachedEntry;

class SovereignWebSearch {
public:
    static SovereignWebSearch& instance();

    // Execute a search query.  Results are cached for 5 minutes.
    // searchTerm is capped at 256 chars to guard against injection.
    std::vector<SearchResult> query(const std::string& searchTerm);

    // Flush the in-process result cache.
    void clearCache();

    SovereignWebSearch();
    ~SovereignWebSearch();

private:
    std::string httpGet(const std::string& path);

    std::mutex m_cacheMutex;
    std::map<std::string, CachedEntry> m_cache;
};

} // namespace RawrXD::Runtime
