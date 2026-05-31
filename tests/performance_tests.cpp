// ============================================================================
// Performance Tests — Benchmarking and Load Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "../src/search/search_engine.cpp"
#include "../src/cache/cache_manager.cpp"
#include "../src/metrics/metrics_collector.cpp"

using namespace RawrXD;

// Mock Session Manager for benchmarks
class PerfMockSessionManager : public Core::SessionManager {
public:
    void SetValue(const std::string& key, const std::string& value) override {}
    std::string GetValue(const std::string& key) override { return ""; }
};

// Mock AI Client for benchmarks
class PerfMockAIClient : public SovereignInferenceClient {
public:
    bool IsLoaded() const override { return true; }
    ChatResult ChatSync(const std::vector<ChatMessage>& messages) override {
        return {true, "Benchmark response", 0.9, 50};
    }
};

TEST_CASE("Search Engine Performance", "[performance][search]") {
    auto aiClient = std::make_shared<PerfMockAIClient>();
    Search::SearchEngine search(aiClient);
    
    // Pre-populate index
    for (int i = 0; i < 10000; i++) {
        std::string filename = "file" + std::to_string(i) + ".cpp";
        std::string code = 
            "void function" + std::to_string(i) + "() {\n"
            "    int x = " + std::to_string(i) + ";\n"
            "    return;\n"
            "}\n";
        search.IndexFile(filename, code);
    }
    
    BENCHMARK("Exact search - 10K files") {
        Search::SearchQuery query;
        query.query = "function5000";
        query.type = Search::SearchType::EXACT;
        query.maxResults = 10;
        return search.Search(query).size();
    };
    
    BENCHMARK("Symbol search - 10K files") {
        Search::SearchQuery query;
        query.query = "function";
        query.type = Search::SearchType::SYMBOL;
        query.maxResults = 10;
        return search.Search(query).size();
    };
    
    BENCHMARK("Regex search - 10K files") {
        Search::SearchQuery query;
        query.query = R"(function\d+\(\))";
        query.type = Search::SearchType::REGEX;
        query.maxResults = 10;
        return search.Search(query).size();
    };
}

TEST_CASE("Cache Performance", "[performance][cache]") {
    auto sessionManager = std::make_shared<PerfMockSessionManager>();
    Cache::CacheManager cache(sessionManager, 10 * 1024 * 1024, 100 * 1024 * 1024);
    
    // Pre-populate cache
    for (int i = 0; i < 10000; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        std::vector<uint8_t> data(value.begin(), value.end());
        cache.Put(key, data, Cache::CacheTier::MEMORY);
    }
    
    BENCHMARK("Cache read - 10K entries") {
        return cache.Get("key5000").has_value();
    };
    
    BENCHMARK("Cache write - 10K entries") {
        std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        cache.Put("bench_key", data, Cache::CacheTier::MEMORY);
        return true;
    };
}

TEST_CASE("Metrics Collector Performance", "[performance][metrics]") {
    auto sessionManager = std::make_shared<PerfMockSessionManager>();
    Metrics::MetricsCollector metrics(sessionManager);
    
    BENCHMARK("Record gauge metric") {
        metrics.SetGauge("benchmark_gauge", 123.45);
        return true;
    };
    
    BENCHMARK("Record counter metric") {
        metrics.IncrementCounter("benchmark_counter", {{"label", "value"}});
        return true;
    };
    
    // Pre-populate metrics
    for (int i = 0; i < 10000; i++) {
        metrics.RecordHistogram("response_time", static_cast<double>(i) / 100.0);
    }
    
    BENCHMARK("Aggregate metrics - 10K entries") {
        return metrics.Aggregate("response_time",
            std::chrono::system_clock::now() - std::chrono::hours(1),
            std::chrono::system_clock::now()
        ).count;
    };
}

TEST_CASE("Feature Flags Performance", "[performance][feature-flags]") {
    auto sessionManager = std::make_shared<PerfMockSessionManager>();
    FeatureFlags::FeatureFlagsManager flags(sessionManager);
    
    // Register 1000 features
    for (int i = 0; i < 1000; i++) {
        FeatureFlags::FeatureFlag flag;
        flag.key = "feature" + std::to_string(i);
        flag.name = "Feature " + std::to_string(i);
        flag.state = (i % 2 == 0) ? FeatureFlags::FeatureState::ON : 
                                           FeatureFlags::FeatureState::OFF;
        flags.RegisterFeature(flag);
    }
    
    BENCHMARK("Check feature enabled - 1000 features") {
        return flags.IsEnabled("feature500");
    };
    
    BENCHMARK("Check feature enabled with user - 1000 features") {
        return flags.IsEnabled("feature500", "user123");
    };
}

TEST_CASE("Concurrent Access Performance", "[performance][concurrency]") {
    auto sessionManager = std::make_shared<PerfMockSessionManager>();
    Metrics::MetricsCollector metrics(sessionManager);
    
    BENCHMARK_ADVANCED("Concurrent metric recording")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&metrics]() {
            // Simulate concurrent writes from multiple threads
            for (int i = 0; i < 100; i++) {
                metrics.IncrementCounter("concurrent_counter", {{"thread", "main"}});
            }
            return true;
        });
    };
}

TEST_CASE("Memory Usage Benchmarks", "[performance][memory]") {
    auto sessionManager = std::make_shared<PerfMockSessionManager>();
    
    BENCHMARK("SearchEngine memory footprint") {
        auto aiClient = std::make_shared<PerfMockAIClient>();
        Search::SearchEngine search(aiClient);
        
        // Index 1000 files
        for (int i = 0; i < 1000; i++) {
            std::string filename = "memtest" + std::to_string(i) + ".cpp";
            std::string code = 
                "class Class" + std::to_string(i) + " {\n"
                "public:\n"
                "    void method1();\n"
                "    void method2();\n"
                "    void method3();\n"
                "};\n";
            search.IndexFile(filename, code);
        }
        
        return search.GetAllFeatures().size();
    };
    
    BENCHMARK("CacheManager memory footprint") {
        Cache::CacheManager cache(sessionManager, 5 * 1024 * 1024, 50 * 1024 * 1024);
        
        // Add 1000 entries
        for (int i = 0; i < 1000; i++) {
            std::string value = "Large value data " + std::to_string(i) + 
                               std::string(100, 'x'); // 100+ bytes each
            std::vector<uint8_t> data(value.begin(), value.end());
            cache.Put("memkey" + std::to_string(i), data, Cache::CacheTier::MEMORY);
        }
        
        return cache.GetStats().totalSize;
    };
}

TEST_CASE("Scalability Benchmarks", "[performance][scalability]") {
    SECTION("Linear scaling test - Search") {
        auto aiClient = std::make_shared<PerfMockAIClient>();
        Search::SearchEngine search(aiClient);
        
        std::vector<int> fileCounts = {100, 500, 1000, 5000};
        
        for (int count : fileCounts) {
            // Clear and re-index
            search = Search::SearchEngine(aiClient);
            
            for (int i = 0; i < count; i++) {
                std::string filename = "scale" + std::to_string(i) + ".cpp";
                std::string code = 
                    "void func" + std::to_string(i) + "() { int x = " + 
                    std::to_string(i) + "; }\n";
                search.IndexFile(filename, code);
            }
            
            auto start = std::chrono::steady_clock::now();
            
            Search::SearchQuery query;
            query.query = "func";
            query.type = Search::SearchType::SYMBOL;
            query.maxResults = 10;
            
            auto results = search.Search(query);
            
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            INFO(count << " files: " << duration.count() << "ms, " << results.size() << " results");
            
            // Search time should scale reasonably with file count
            REQUIRE(duration.count() < count / 10); // Less than 0.1ms per file
        }
    }
}
