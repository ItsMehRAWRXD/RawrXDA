// at_web_bridge.h - `@Web` search bridge (Cursor parity)
// Feature 6/15 (Cursor parity).
#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rawrxd::parity {

struct WebResult {
    std::string url;
    std::string title;
    std::string snippet;
    double      rank{0.0};
};

using WebSearchProviderFn = std::function<std::vector<WebResult>(const std::string&)>;
using WebFetchProviderFn  = std::function<std::optional<std::string>(const std::string&)>;

class AtWebBridge {
public:
    void set_search_provider(WebSearchProviderFn fn);
    void set_fetch_provider(WebFetchProviderFn fn);

    // Cached search; cache TTL defaults to 5 minutes.
    std::vector<WebResult> search(const std::string& query, std::size_t top_k = 5);

    // Cached page fetch (text-only output if provider normalises).
    std::optional<std::string> fetch(const std::string& url);

    void set_cache_ttl(std::chrono::seconds s);
    void clear_cache();

    std::uint64_t search_requests() const;
    std::uint64_t search_cache_hits() const;

private:
    struct CacheEntry {
        std::chrono::steady_clock::time_point expires;
        std::vector<WebResult>                results;
    };
    struct PageEntry {
        std::chrono::steady_clock::time_point expires;
        std::string                           body;
    };

    mutable std::mutex mu_;
    WebSearchProviderFn                     search_fn_;
    WebFetchProviderFn                      fetch_fn_;
    std::chrono::seconds                    ttl_{std::chrono::seconds(300)};
    std::unordered_map<std::string, CacheEntry> search_cache_;
    std::unordered_map<std::string, PageEntry>  page_cache_;
    std::uint64_t total_{0};
    std::uint64_t hits_{0};
};

} // namespace rawrxd::parity
