// at_web_bridge.cpp - Implementation
#include "at_web_bridge.h"

#include <unordered_map>

namespace rawrxd::parity {

void AtWebBridge::set_search_provider(WebSearchProviderFn fn) {
    std::lock_guard lk(mu_); search_fn_ = std::move(fn);
}
void AtWebBridge::set_fetch_provider(WebFetchProviderFn fn) {
    std::lock_guard lk(mu_); fetch_fn_ = std::move(fn);
}
void AtWebBridge::set_cache_ttl(std::chrono::seconds s) {
    std::lock_guard lk(mu_); ttl_ = s;
}

std::vector<WebResult> AtWebBridge::search(const std::string& query, std::size_t top_k) {
    auto now = std::chrono::steady_clock::now();
    WebSearchProviderFn fn;
    std::chrono::seconds ttl;
    {
        std::lock_guard lk(mu_);
        ++total_;
        auto it = search_cache_.find(query);
        if (it != search_cache_.end() && it->second.expires > now) {
            ++hits_;
            auto r = it->second.results;
            if (r.size() > top_k) r.resize(top_k);
            return r;
        }
        fn = search_fn_;
        ttl = ttl_;
    }
    if (!fn) return {};
    auto results = fn(query);
    {
        std::lock_guard lk(mu_);
        CacheEntry e{ now + ttl, results };
        search_cache_[query] = std::move(e);
        // Cap cache at 128 entries (FIFO-ish eviction).
        if (search_cache_.size() > 128) {
            auto it = search_cache_.begin();
            for (auto cur = search_cache_.begin(); cur != search_cache_.end(); ++cur)
                if (cur->second.expires < it->second.expires) it = cur;
            search_cache_.erase(it);
        }
    }
    if (results.size() > top_k) results.resize(top_k);
    return results;
}

std::optional<std::string> AtWebBridge::fetch(const std::string& url) {
    auto now = std::chrono::steady_clock::now();
    WebFetchProviderFn fn;
    std::chrono::seconds ttl;
    {
        std::lock_guard lk(mu_);
        auto it = page_cache_.find(url);
        if (it != page_cache_.end() && it->second.expires > now) return it->second.body;
        fn = fetch_fn_;
        ttl = ttl_;
    }
    if (!fn) return std::nullopt;
    auto body = fn(url);
    if (!body) return std::nullopt;
    {
        std::lock_guard lk(mu_);
        PageEntry e{ now + ttl, *body };
        page_cache_[url] = std::move(e);
        if (page_cache_.size() > 64) page_cache_.erase(page_cache_.begin());
    }
    return body;
}

void AtWebBridge::clear_cache() {
    std::lock_guard lk(mu_);
    search_cache_.clear(); page_cache_.clear();
}

std::uint64_t AtWebBridge::search_requests() const {
    std::lock_guard lk(mu_); return total_;
}
std::uint64_t AtWebBridge::search_cache_hits() const {
    std::lock_guard lk(mu_); return hits_;
}

} // namespace rawrxd::parity
