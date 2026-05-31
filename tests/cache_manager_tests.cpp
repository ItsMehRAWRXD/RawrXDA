// ============================================================================
// Cache Manager Tests — Multi-tier Cache Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "../src/cache/cache_manager.cpp"

using namespace RawrXD::Cache;

// Mock Session Manager
class MockCacheSessionManager : public Core::SessionManager {
public:
    void SetValue(const std::string& key, const std::string& value) override {}
    std::string GetValue(const std::string& key) override { return ""; }
};

TEST_CASE("Cache Manager - Basic Operations", "[cache][storage]") {
    auto sessionManager = std::make_shared<MockCacheSessionManager>();
    CacheManager cache(sessionManager, 1024 * 1024, 10 * 1024 * 1024); // 1MB memory, 10MB disk
    
    SECTION("Default cache state") {
        auto stats = cache.GetStats();
        REQUIRE(stats.totalSize == 0);
        REQUIRE(stats.hitCount == 0);
        REQUIRE(stats.missCount == 0);
    }
    
    SECTION("Put and get value") {
        std::string value = "Hello, Cache!";
        std::vector<uint8_t> data(value.begin(), value.end());
        
        cache.Put("key1", data, CacheTier::MEMORY);
        
        auto retrieved = cache.Get("key1");
        REQUIRE(retrieved.has_value());
        
        std::string result(retrieved->begin(), retrieved->end());
        REQUIRE(result == value);
    }
    
    SECTION("Cache statistics") {
        cache.Put("key1", {1, 2, 3}, CacheTier::MEMORY);
        cache.Put("key2", {4, 5, 6}, CacheTier::MEMORY);
        
        auto stats = cache.GetStats();
        REQUIRE(stats.totalSize == 6);
        
        // Access to trigger hit
        cache.Get("key1");
        
        stats = cache.GetStats();
        REQUIRE(stats.hitCount == 1);
    }
}

TEST_CASE("Cache Manager - Cache Policies", "[cache][policies]") {
    auto sessionManager = std::make_shared<MockCacheSessionManager>();
    CacheManager cache(sessionManager, 1024, 10240); // Small cache for eviction testing
    
    SECTION("LRU eviction policy") {
        cache.SetPolicy(CachePolicy::LRU);
        
        // Fill cache
        for (int i = 0; i < 100; i++) {
            cache.Put("key" + std::to_string(i), {static_cast<uint8_t>(i)}, CacheTier::MEMORY);
        }
        
        // Verify cache size is within limits
        auto stats = cache.GetStats();
        REQUIRE(stats.totalSize <= 1024);
    }
}

TEST_CASE("Cache Manager - Invalidation", "[cache][invalidation]") {
    auto sessionManager = std::make_shared<MockCacheSessionManager>();
    CacheManager cache(sessionManager);
    
    SECTION("Invalidate single key") {
        cache.Put("key1", {1, 2, 3}, CacheTier::MEMORY);
        cache.Put("key2", {4, 5, 6}, CacheTier::MEMORY);
        
        cache.Invalidate("key1");
        
        REQUIRE_FALSE(cache.Get("key1").has_value());
        REQUIRE(cache.Get("key2").has_value());
    }
    
    SECTION("Invalidate by pattern") {
        cache.Put("user:1", {1}, CacheTier::MEMORY);
        cache.Put("user:2", {2}, CacheTier::MEMORY);
        cache.Put("product:1", {3}, CacheTier::MEMORY);
        
        cache.InvalidatePattern("user:");
        
        REQUIRE_FALSE(cache.Get("user:1").has_value());
        REQUIRE_FALSE(cache.Get("user:2").has_value());
        REQUIRE(cache.Get("product:1").has_value());
    }
    
    SECTION("Clear all cache") {
        cache.Put("key1", {1}, CacheTier::MEMORY);
        cache.Put("key2", {2}, CacheTier::MEMORY);
        
        cache.Clear();
        
        auto stats = cache.GetStats();
        REQUIRE(stats.totalSize == 0);
        REQUIRE_FALSE(cache.Get("key1").has_value());
        REQUIRE_FALSE(cache.Get("key2").has_value());
    }
}

TEST_CASE("Cache Manager - Report Generation", "[cache][reporting]") {
    auto sessionManager = std::make_shared<MockCacheSessionManager>();
    CacheManager cache(sessionManager);
    
    SECTION("Generate cache report") {
        cache.Put("entry1", {1, 2, 3, 4, 5}, CacheTier::MEMORY);
        cache.Put("entry2", {6, 7, 8}, CacheTier::MEMORY);
        
        auto report = cache.GenerateCacheReport();
        
        REQUIRE_FALSE(report.empty());
        REQUIRE(report.find("# Cache Report") != std::string::npos);
    }
}

TEST_CASE("Cache Manager - Benchmarks", "[cache][performance]") {
    auto sessionManager = std::make_shared<MockCacheSessionManager>();
    CacheManager cache(sessionManager, 10 * 1024 * 1024, 100 * 1024 * 1024);
    
    // Pre-populate cache
    for (int i = 0; i < 1000; i++) {
        std::string value = "value" + std::to_string(i);
        std::vector<uint8_t> data(value.begin(), value.end());
        cache.Put("key" + std::to_string(i), data, CacheTier::MEMORY);
    }
    
    BENCHMARK("Cache read performance") {
        return cache.Get("key500");
    };
    
    BENCHMARK("Cache write performance") {
        std::vector<uint8_t> data = {1, 2, 3, 4, 5};
        cache.Put("bench_key", data, CacheTier::MEMORY);
        return true;
    };
}
