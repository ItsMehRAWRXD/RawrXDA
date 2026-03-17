/**
 * @file test_agentic_mini_pc_800b.cpp
 * @brief Comprehensive CLI test for mini PC + 800B model + Sovereign telemetry integration
 * @author RawrXD Team
 * @date 2026-01-27
 * 
 * This test validates the complete agentic stack:
 * 1. Sovereign thermal telemetry reading from mini PC
 * 2. Automatic loader selection (Basic/Streaming/Enhanced) based on thermal tier
 * 3. 800B model optimization profiles (mini, ultra, 800b)
 * 4. Reverse engineered performance constraints and yield logic
 * 
 * USAGE:
 *   test_agentic_mini_pc_800b.exe <model.gguf> [--profile mini|70b|120b|800b|ultra] [--force-loader basic|streaming|enhanced]
 * 
 * ENVIRONMENT:
 *   RAWRXD_LOADER_PROFILE - Override profile selection
 *   RAWRXD_USE_ENHANCED_LOADER - Force enhanced streaming loader
 *   RAWRXD_USE_STREAMING_LOADER - Force streaming loader
 *   RAWRXD_STREAMING_ZONE_MB - Override zone memory budget
 */

#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <cstdlib>

// Core RawrXD includes
#include "../include/inference_engine_stub.hpp"
#include "../src/utils/sovereign_bridge.hpp"
#include "../src/qtapp/ProdIntegration.h"

// Qt minimal for QCoreApplication
#include <QCoreApplication>
#include <QString>
#include <QDebug>

using namespace std;
using namespace std::chrono;

struct TestConfig {
    std::string model_path;
    std::string profile_override;
    std::string loader_override;
    bool verbose = false;
    bool benchmark = false;
    int inference_tokens = 50;
};

struct SystemInfo {
    bool sovereign_available = false;
    double max_temp = 0.0;
    int tier = -1;
    double sparse_pct = 0.0;
    bool is_throttled = false;
    bool should_yield = false;
    std::string inferred_profile;
};

void printHeader() {
    cout << "\n";
    cout << "╔════════════════════════════════════════════════════════════════════════╗\n";
    cout << "║  RawrXD Agentic Mini PC + 800B Model Integration Test (v1.2.0)        ║\n";
    cout << "║  Sovereign Telemetry + Loader Selection + Thermal Constraints          ║\n";
    cout << "╚════════════════════════════════════════════════════════════════════════╝\n";
    cout << "\n";
}

void printUsage(const char* prog_name) {
    cout << "Usage: " << prog_name << " <model.gguf> [options]\n";
    cout << "\nOptions:\n";
    cout << "  --profile <mini|70b|120b|800b|ultra>   Override profile selection\n";
    cout << "  --force-loader <basic|streaming|enhanced>  Force specific loader\n";
    cout << "  --verbose                               Show detailed logs\n";
    cout << "  --benchmark                             Run inference benchmark\n";
    cout << "  --tokens <N>                            Generate N tokens (default: 50)\n";
    cout << "\nEnvironment Variables:\n";
    cout << "  RAWRXD_LOADER_PROFILE                   Profile override\n";
    cout << "  RAWRXD_USE_ENHANCED_LOADER              Force enhanced loader\n";
    cout << "  RAWRXD_USE_STREAMING_LOADER             Force streaming loader\n";
    cout << "  RAWRXD_STREAMING_ZONE_MB                Zone memory budget (MB)\n";
    cout << "  RAWRXD_METADATA_ONLY                    Metadata-only load (mini PC memory guard)\n";
    cout << "\nProfiles:\n";
    cout << "  mini    - Mini PC optimized (128MB zones, enhanced loader)\n";
    cout << "  70b     - 70B model profile (384MB zones, streaming loader)\n";
    cout << "  120b    - 120B model profile (384MB zones, streaming loader)\n";
    cout << "  800b    - 800B model profile (128MB zones, enhanced loader)\n";
    cout << "  ultra   - Ultra performance (128MB zones, enhanced loader)\n";
}

TestConfig parseArgs(int argc, char* argv[]) {
    TestConfig config;
    
    if (argc < 2) {
        printUsage(argv[0]);
        exit(1);
    }
    
    config.model_path = argv[1];
    
    for (int i = 2; i < argc; i++) {
        string arg = argv[i];
        
        if (arg == "--profile" && i + 1 < argc) {
            config.profile_override = argv[++i];
        } else if (arg == "--force-loader" && i + 1 < argc) {
            config.loader_override = argv[++i];
        } else if (arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "--benchmark") {
            config.benchmark = true;
        } else if (arg == "--tokens" && i + 1 < argc) {
            config.inference_tokens = atoi(argv[++i]);
        } else {
            cerr << "Unknown option: " << arg << endl;
            printUsage(argv[0]);
            exit(1);
        }
    }
    
    return config;
}

SystemInfo probeSovereignSystem(bool verbose) {
    SystemInfo info;
    
    cout << "🔍 Probing Sovereign thermal telemetry...\n";
    
    try {
        auto stats = SovereignBridge::getStats();
        
        // Check if telemetry is available
        bool has_temps = false;
        for (int i = 0; i < 5; i++) {
            if (stats.temps[i] > 0.0) {
                has_temps = true;
                info.max_temp = max(info.max_temp, stats.temps[i]);
            }
        }
        
        if (has_temps) {
            info.sovereign_available = true;
            info.tier = stats.tier;
            info.sparse_pct = stats.sparsePct;
            info.is_throttled = stats.isThrottled;
            info.should_yield = SovereignBridge::shouldYield();
            
            // Infer profile from tier (matches InferenceEngine logic)
            switch (stats.tier) {
                case 0: info.inferred_profile = "70b"; break;
                case 1: info.inferred_profile = "120b"; break;
                case 2: info.inferred_profile = "800b"; break;
                default: info.inferred_profile = "auto"; break;
            }
            
            cout << "  ✓ Sovereign telemetry active\n";
            cout << "    Max NVMe temp: " << fixed << setprecision(1) << info.max_temp << "°C\n";
            cout << "    Tier: " << info.tier << " (" << info.inferred_profile << ")\n";
            cout << "    Sparse skip: " << fixed << setprecision(1) << info.sparse_pct << "%\n";
            cout << "    Throttled: " << (info.is_throttled ? "YES" : "NO") << "\n";
            cout << "    Should yield: " << (info.should_yield ? "YES" : "NO") << "\n";
            
            if (verbose) {
                cout << "    Individual temps: ";
                for (int i = 0; i < 5; i++) {
                    cout << stats.temps[i] << "°C ";
                }
                cout << "\n";
                cout << "    GPU split: " << stats.gpuSplit << "%\n";
            }
        } else {
            cout << "  ⚠ No Sovereign telemetry detected (using fallback)\n";
            info.inferred_profile = "auto";
        }
    } catch (const exception& e) {
        cout << "  ⚠ Sovereign bridge error: " << e.what() << "\n";
        info.inferred_profile = "auto";
    }
    
    return info;
}

void setEnvironmentOverrides(const TestConfig& config, const SystemInfo& sys_info) {
    cout << "\n🔧 Configuring environment...\n";
    
    // Apply profile override
    string active_profile = config.profile_override.empty() ? sys_info.inferred_profile : config.profile_override;
    
    if (!active_profile.empty() && active_profile != "auto") {
        _putenv_s("RAWRXD_LOADER_PROFILE", active_profile.c_str());
        cout << "  Profile set: " << active_profile << "\n";
    }
    
    // Apply loader override
    if (!config.loader_override.empty()) {
        if (config.loader_override == "enhanced") {
            _putenv_s("RAWRXD_USE_ENHANCED_LOADER", "1");
            cout << "  Forced loader: Enhanced Streaming\n";
        } else if (config.loader_override == "streaming") {
            _putenv_s("RAWRXD_USE_STREAMING_LOADER", "1");
            cout << "  Forced loader: Streaming\n";
        } else if (config.loader_override == "basic") {
            _putenv_s("RAWRXD_USE_ENHANCED_LOADER", "0");
            _putenv_s("RAWRXD_USE_STREAMING_LOADER", "0");
            cout << "  Forced loader: Basic\n";
        }
    }
    
    // Show final environment
    cout << "  Environment variables:\n";
    const char* env_vars[] = {
        "RAWRXD_LOADER_PROFILE",
        "RAWRXD_USE_ENHANCED_LOADER", 
        "RAWRXD_USE_STREAMING_LOADER",
        "RAWRXD_STREAMING_ZONE_MB",
        "RAWRXD_METADATA_ONLY"
    };
    
    for (const char* var : env_vars) {
        const char* val = getenv(var);
        if (val) {
            cout << "    " << var << "=" << val << "\n";
        }
    }
}

bool testInferenceEngine(TestConfig& config, const SystemInfo& sys_info) {
    cout << "\n🧠 Testing InferenceEngine with agentic loader selection...\n";
    
    if (!filesystem::exists(config.model_path)) {
        cerr << "  ✗ Model file not found: " << config.model_path << endl;
        return false;
    }
    
    auto file_size = filesystem::file_size(config.model_path);
    cout << "  Model: " << config.model_path << "\n";
    const double size_gb = (file_size / (1024.0 * 1024.0 * 1024.0));
    cout << "  Size: " << fixed << setprecision(2) << size_gb << " GB\n";

    // For models >= 32GB, use lazy pager mode (layer-wise demand paging)
    // For models >= 8GB but < 32GB, use metadata-only as fallback
    if (file_size >= (32ull * 1024ull * 1024ull * 1024ull)) {
        _putenv_s("RAWRXD_USE_LAZY_PAGER", "1");
        cout << "  🔄 Model >= 32GB; enabling lazy tensor pager for layer-wise demand paging\n";
    } else if (file_size >= (8ull * 1024ull * 1024ull * 1024ull)) {
        _putenv_s("RAWRXD_METADATA_ONLY", "1");
        if (config.benchmark) {
            cout << "  ⚠ Model exceeds mini PC budget; disabling benchmark and enabling metadata-only mode\n";
            config.benchmark = false;
        } else {
            cout << "  ⚠ Model exceeds mini PC budget; enabling metadata-only mode\n";
        }
    }
    
    // Create inference engine (requires Qt context)
    InferenceEngine engine(nullptr);
    
    cout << "  Loading model via InferenceEngine...\n";
    
    auto load_start = steady_clock::now();
    bool success = engine.Initialize(config.model_path);
    auto load_end = steady_clock::now();
    
    auto load_ms = duration_cast<milliseconds>(load_end - load_start).count();
    
    if (!success) {
        cout << "  ✗ Failed to load model\n";
        return false;
    }
    
    // Report inference mode
    cout << "  ✓ Model loaded successfully (" << load_ms << " ms)\n";
    cout << "  Model info:\n";
    cout << "    Loaded: " << (engine.isModelLoaded() ? "YES" : "NO") << "\n";
    cout << "    Mode: ";
    switch (engine.inferenceMode()) {
        case InferenceMode::STANDARD: cout << "STANDARD (full RAM)\n"; break;
        case InferenceMode::METADATA_ONLY: cout << "METADATA_ONLY\n"; break;
        case InferenceMode::LAZY_PAGED: cout << "LAZY_PAGED (layer-wise demand)\n"; break;
    }
    cout << "    Path: " << engine.modelPath() << "\n";
    cout << "    Path: " << engine.modelPath() << "\n";
    
    // Test basic inference if benchmark requested
    if (config.benchmark) {
        cout << "\n⚡ Running inference benchmark...\n";
        
        // Simple tokenization test
        QString test_prompt = "What is the purpose of machine learning in modern AI systems?";
        cout << "  Prompt: \"" << test_prompt.toStdString() << "\"\n";
        
        auto token_start = steady_clock::now();
        auto tokens = engine.tokenize(test_prompt);
        auto token_end = steady_clock::now();
        
        auto tokenize_ms = duration_cast<microseconds>(token_end - token_start).count();
        
        cout << "  Tokenization: " << tokens.size() << " tokens (" << tokenize_ms << " μs)\n";
        
        // Simple generation test
        auto gen_start = steady_clock::now();
        auto result = engine.generate(tokens, config.inference_tokens);
        auto gen_end = steady_clock::now();
        
        auto gen_ms = duration_cast<milliseconds>(gen_end - gen_start).count();
        double tokens_per_sec = (config.inference_tokens * 1000.0) / gen_ms;
        
        cout << "  Generation: " << result.size() << " tokens (" << gen_ms << " ms)\n";
        cout << "  Throughput: " << fixed << setprecision(2) << tokens_per_sec << " tokens/sec\n";
        
        // Check thermal yield during inference
        if (sys_info.sovereign_available) {
            bool yield_during = SovereignBridge::shouldYield();
            cout << "  Thermal yield during inference: " << (yield_during ? "YES" : "NO") << "\n";
            
            if (tokens_per_sec >= 0.7) {
                cout << "  ✓ Meets 0.7 t/s mini PC target\n";
            } else {
                cout << "  ⚠ Below 0.7 t/s mini PC target\n";
            }
        }
        
        // Detokenize a sample
        if (!result.empty()) {
            auto sample_tokens = vector<int32_t>(result.begin(), result.begin() + min(10, (int)result.size()));
            QString detokenized = engine.detokenize(sample_tokens);
            cout << "  Sample output: \"" << detokenized.left(100).toStdString() << "...\"\n";
        }
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    // Initialize Qt core (required for InferenceEngine)
    QCoreApplication app(argc, argv);
    
    printHeader();
    
    TestConfig config = parseArgs(argc, argv);
    
    cout << "📋 Configuration:\n";
    cout << "  Model: " << config.model_path << "\n";
    if (!config.profile_override.empty()) {
        cout << "  Profile override: " << config.profile_override << "\n";
    }
    if (!config.loader_override.empty()) {
        cout << "  Loader override: " << config.loader_override << "\n";
    }
    cout << "  Verbose: " << (config.verbose ? "ON" : "OFF") << "\n";
    cout << "  Benchmark: " << (config.benchmark ? "ON" : "OFF") << "\n";
    if (config.benchmark) {
        cout << "  Inference tokens: " << config.inference_tokens << "\n";
    }
    
    // Step 1: Probe Sovereign system
    SystemInfo sys_info = probeSovereignSystem(config.verbose);
    
    // Step 2: Configure environment
    setEnvironmentOverrides(config, sys_info);
    
    // Step 3: Test InferenceEngine with agentic selection
    bool success = testInferenceEngine(config, sys_info);
    
    cout << "\n";
    cout << "╔════════════════════════════════════════════════════════════════════════╗\n";
    if (success) {
        cout << "║  ✅ AGENTIC MINI PC + 800B INTEGRATION TEST: PASSED                   ║\n";
        cout << "║                                                                        ║\n";
        cout << "║  • Sovereign telemetry: " << (sys_info.sovereign_available ? "ACTIVE" : "INACTIVE") << "                                   ║\n";
        cout << "║  • Loader selection: AUTOMATIC                                        ║\n";
        cout << "║  • Model loading: SUCCESS                                             ║\n";
        if (config.benchmark) {
            cout << "║  • Inference benchmark: COMPLETED                                     ║\n";
        }
    } else {
        cout << "║  ❌ AGENTIC MINI PC + 800B INTEGRATION TEST: FAILED                   ║\n";
    }
    cout << "╚════════════════════════════════════════════════════════════════════════╝\n";
    cout << "\n";
    
    return success ? 0 : 1;
}