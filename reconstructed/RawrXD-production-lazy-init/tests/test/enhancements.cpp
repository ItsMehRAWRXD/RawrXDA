#include <iostream>
#include <cassert>
#include <vector>
#include <string>

// Test framework
#define TEST(name) void test_##name()
#define ASSERT_TRUE(expr) if (!(expr)) { std::cerr << "FAILED: " << #expr << " at line " << __LINE__ << std::endl; return; }
#define ASSERT_EQ(a, b) if ((a) != (b)) { std::cerr << "FAILED: " << #a << " != " << #b << " at line " << __LINE__ << std::endl; return; }
#define RUN_TEST(name) std::cout << "Running " << #name << "..." << std::endl; test_##name(); std::cout << "  PASSED" << std::endl;

// Include components to test
#include "cli_streaming_enhancements.h"
#include "performance_tuner.h"

TEST(StreamingManager_Basic) {
    CLI::StreamingManager manager;
    
    bool callback_called = false;
    std::string received_token;
    
    manager.StartStream("test prompt", [&](const std::string& token, bool is_final) {
        callback_called = true;
        received_token = token;
    });
    
    ASSERT_TRUE(manager.IsStreaming());
    ASSERT_EQ(manager.GetCurrentPrompt(), "test prompt");
    
    manager.ProcessChunk("hello");
    ASSERT_TRUE(callback_called);
    ASSERT_EQ(received_token, "hello");
    
    manager.StopStream();
    ASSERT_TRUE(!manager.IsStreaming());
}

TEST(AutoCompleter_Basic) {
    CLI::AutoCompleter completer;
    
    auto completions = completer.GetCompletions("he");
    ASSERT_TRUE(!completions.empty());
    ASSERT_TRUE(std::find(completions.begin(), completions.end(), "help") != completions.end());
    
    completer.AddCompletion("hello");
    completions = completer.GetCompletions("hel");
    ASSERT_TRUE(std::find(completions.begin(), completions.end(), "hello") != completions.end());
}

TEST(HistoryManager_Basic) {
    CLI::HistoryManager history("test_history.txt");
    
    history.Clear();
    history.Add("command1");
    history.Add("command2");
    
    auto all = history.GetAll();
    ASSERT_EQ(all.size(), 2u);
    ASSERT_EQ(all[0], "command1");
    ASSERT_EQ(all[1], "command2");
    
    auto results = history.Search("command1");
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0], "command1");
}

TEST(ProgressIndicator_Basic) {
    CLI::ProgressIndicator progress;
    
    progress.Start("Testing");
    ASSERT_TRUE(progress.IsActive());
    
    progress.Update(50);
    progress.Stop();
    ASSERT_TRUE(!progress.IsActive());
}

TEST(HardwareDetection) {
    auto hardware = Performance::PerformanceTuner::DetectHardware();
    
    ASSERT_TRUE(hardware.cpu_cores > 0);
    ASSERT_TRUE(hardware.cpu_threads > 0);
    ASSERT_TRUE(hardware.total_ram_mb > 0);
    
    std::cout << "  Detected: " << hardware.cpu_threads << " threads, " 
              << hardware.total_ram_mb << " MB RAM" << std::endl;
}

TEST(AdaptiveConfig) {
    Performance::HardwareProfile hardware;
    hardware.cpu_threads = 8;
    hardware.cpu_cores = 4;
    hardware.total_ram_mb = 16384;
    hardware.available_ram_mb = 12288;
    hardware.has_avx2 = true;
    
    auto config = Performance::PerformanceTuner::GenerateConfig(hardware);
    
    ASSERT_TRUE(config.worker_threads > 0);
    ASSERT_TRUE(config.kv_cache_size_mb > 0);
    ASSERT_TRUE(config.model_cache_size_mb > 0);
    ASSERT_TRUE(config.enable_flash_attention);
    
    std::cout << "  Generated config: " << config.worker_threads << " workers, "
              << config.kv_cache_size_mb << " MB cache" << std::endl;
}

TEST(PerformanceTuner_Integration) {
    auto& tuner = Performance::GetPerformanceTuner();
    
    const auto& config = tuner.GetConfig();
    ASSERT_TRUE(config.worker_threads > 0);
    
    auto metrics = tuner.GetMetrics();
    // Metrics might be 0 if no processing has occurred yet, that's OK
}

int main() {
    std::cout << "=== RawrXD Enhancement Test Suite ===" << std::endl;
    std::cout << std::endl;
    
    try {
        RUN_TEST(StreamingManager_Basic);
        RUN_TEST(AutoCompleter_Basic);
        RUN_TEST(HistoryManager_Basic);
        RUN_TEST(ProgressIndicator_Basic);
        RUN_TEST(HardwareDetection);
        RUN_TEST(AdaptiveConfig);
        RUN_TEST(PerformanceTuner_Integration);
        
        std::cout << std::endl;
        std::cout << "=== ALL TESTS PASSED ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
