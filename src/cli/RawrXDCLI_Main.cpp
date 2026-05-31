#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "inference/MLInferenceEngine.hpp"
#include "sovereign/SovereignCoreWrapper.hpp"
#include "prometheus/prometheus_engine.h"
#include "prometheus/prometheus_800b_config.h"
#include "prometheus/prometheus_weight_loader.h"

// ExternC declarations to call MASM sovereign core
extern "C" {
    void Sovereign_Pipeline_Cycle();
    void CoordinateAgents();
    void HealSymbolResolution();
    void ValidateDMAAlignment();
    void RawrXD_Trigger_Chat();
    void ObserveTokenStream();

    extern QWORD g_CycleCounter;
    extern QWORD g_SovereignStatus;
    extern QWORD g_SymbolHealCount;
    extern DWORD g_ActiveAgentCount;
}

using json = nlohmann::json;

namespace RawrXD::CLI {

/**
 * RawrXD CLI with Real ML Inference + Prometheus 800B MoE Engine
 *
 * Pipeline:
 * 1. Initialize sovereign core + ML engine
 * 2. Read user prompt from stdin
 * 3. Trigger inference (HTTP to RawrEngine)
 * 4. Run sovereign autonomous cycle
 * 5. Output structured telemetry as JSON
 */
class RawrXDCLI {
public:
    RawrXDCLI() = default;

    int run(int argc, char* argv[]) {
        // Parse command-line flags
        bool usePrometheus = false;
        std::string modelPath;
        std::string userPrompt;
        bool benchmarkMode = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--prometheus" || arg == "--prometheus-800b") {
                usePrometheus = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    modelPath = argv[++i];
                }
            } else if (arg == "--prompt" && i + 1 < argc) {
                userPrompt = argv[++i];
            } else if (arg == "--benchmark") {
                benchmarkMode = true;
            } else if (arg == "--help" || arg == "-h") {
                printHelp();
                return 0;
            }
        }

        if (usePrometheus) {
            return runPrometheus(modelPath, userPrompt, benchmarkMode);
        }

        return runLegacy(userPrompt);
    }

private:
    void printHelp() {
        std::cout << "RawrXD CLI — Real Inference + Autonomous Core\n"
                  << "Version: 1.0 (ml64 / curl / libcurl / Prometheus)\n\n"
                  << "Usage:\n"
                  << "  RawrXDCLI [options]\n\n"
                  << "Options:\n"
                  << "  --prometheus [model.gguf]  Use Prometheus 800B MoE engine\n"
                  << "  --prompt \"text\"            User prompt (default: demo)\n"
                  << "  --benchmark                Run memory/latency estimation\n"
                  << "  --help, -h                 Show this help\n\n"
                  << "Examples:\n"
                  << "  RawrXDCLI --prometheus model.gguf --prompt \"Hello\"\n"
                  << "  RawrXDCLI --prometheus --benchmark\n";
    }

    int runPrometheus(const std::string& modelPath, const std::string& prompt, bool benchmark) {
        std::cout << "=== Prometheus 800B MoE Engine ===\n" << std::endl;

        // Load 800B config
        auto config = Prometheus::get800BConfig();
        std::cout << "[Config] Hidden: " << config.hiddenDim
                  << " | Layers: " << config.numLayers
                  << " | Experts: " << config.numExperts
                  << " | Active: " << config.expertsPerToken << std::endl;

        // Memory estimation
        auto memEst = Prometheus::MemoryEstimator::estimate(config);
        std::cout << "\n[Memory Estimate]\n"
                  << "  Model weights:  " << (memEst.modelWeightsBytes / 1e9) << " GB\n"
                  << "  KV cache:      " << (memEst.kvCacheBytes / 1e9) << " GB\n"
                  << "  Activations:   " << (memEst.activationBytes / 1e9) << " GB\n"
                  << "  Total:         " << (memEst.totalBytes / 1e9) << " GB\n"
                  << "  Recommended:   " << (memEst.recommendedVRAMBytes / 1e9) << " GB\n";

        if (benchmark) {
            // GPU benchmark (H100 profile)
            auto gpuEst = Prometheus::LatencyEstimator::estimate(config, 800.0, 67.0, 1024, 256);
            std::cout << "\n[GPU Benchmark — H100]\n"
                      << "  Prefill:       " << gpuEst.prefillMs << " ms\n"
                      << "  Per token:     " << gpuEst.tokenMs << " ms\n"
                      << "  TPS:           " << gpuEst.tps << "\n";

            // CPU benchmark (AVX-512, 16 threads)
            auto cpuEst = Prometheus::LatencyEstimator::estimateCPU(config, 16, 1024, 256);
            std::cout << "\n[CPU Benchmark — AVX-512, 16 threads]\n"
                      << "  Prefill:       " << cpuEst.prefillMs << " ms\n"
                      << "  Per token:     " << cpuEst.tokenMs << " ms\n"
                      << "  TPS:           " << cpuEst.tps << "\n";

            // Output JSON telemetry
            json bench;
            bench["config"]["hiddenDim"] = config.hiddenDim;
            bench["config"]["numLayers"] = config.numLayers;
            bench["config"]["numExperts"] = config.numExperts;
            bench["memory"]["modelGB"] = memEst.modelWeightsBytes / 1e9;
            bench["memory"]["kvCacheGB"] = memEst.kvCacheBytes / 1e9;
            bench["memory"]["totalGB"] = memEst.totalBytes / 1e9;
            bench["gpu"]["prefillMs"] = gpuEst.prefillMs;
            bench["gpu"]["tokenMs"] = gpuEst.tokenMs;
            bench["gpu"]["tps"] = gpuEst.tps;
            bench["cpu"]["prefillMs"] = cpuEst.prefillMs;
            bench["cpu"]["tokenMs"] = cpuEst.tokenMs;
            bench["cpu"]["tps"] = cpuEst.tps;

            std::ofstream f("d:\\rawrxd\\prometheus_benchmark.json");
            f << bench.dump(4);
            f.close();
            std::cout << "\n✓ Benchmark written to: d:\\rawrxd\\prometheus_benchmark.json\n";
            return 0;
        }

        // Load model weights if path provided
        if (!modelPath.empty()) {
            std::cout << "\n[Loading] " << modelPath << std::endl;
            std::vector<Prometheus::TensorDesc> tensors;
            auto loadResult = Prometheus::GGUFLoader::load(modelPath, tensors, &config);

            if (!loadResult.success) {
                std::cerr << "ERROR: " << loadResult.error << std::endl;
                return 1;
            }

            std::cout << "✓ Loaded " << loadResult.tensorsLoaded
                      << " tensors (" << (loadResult.bytesLoaded / 1e6) << " MB) in "
                      << loadResult.loadTimeMs << " ms\n";
        } else {
            std::cout << "\n[Info] No model path provided — using random initialization\n";
        }

        // Run inference
        std::string userPrompt = prompt;
        if (userPrompt.empty()) {
            userPrompt = "Explain the architecture of a 800B parameter Mixture-of-Experts model.";
        }

        std::cout << "\nPrompt: " << userPrompt << "\n\n";
        std::cout << "Generating...\n";

        // Create Prometheus engine
        auto engine = Prometheus::PrometheusEngine::create(modelPath, config);
        if (!engine) {
            std::cerr << "ERROR: Failed to create PrometheusEngine" << std::endl;
            return 1;
        }

        // Build message
        Prometheus::Message msg;
        msg.role = "user";
        Prometheus::ContentPart part;
        part.type = Prometheus::ContentPart::Type::Text;
        part.text = userPrompt;
        msg.content.push_back(part);

        // Run inference
        Prometheus::GenerationConfig genConfig;
        genConfig.maxTokens = 256;
        genConfig.temperature = 0.7f;
        genConfig.streaming = true;

        auto result = engine->generate({msg}, genConfig);

        std::cout << "\n────────────────────────────────────────\n";
        std::cout << result.text << "\n";
        std::cout << "────────────────────────────────────────\n";
        std::cout << "Tokens: " << result.totalTokens
                  << " | Prompt: " << result.promptTokens
                  << " | Completion: " << result.completionTokens
                  << " | TPS: " << result.tokensPerSecond
                  << " | Time: " << result.totalTime.count() << " ms\n";

        // Output JSON telemetry
        json out;
        out["success"] = result.finished;
        out["text"] = result.text;
        out["tokens"]["total"] = result.totalTokens;
        out["tokens"]["prompt"] = result.promptTokens;
        out["tokens"]["completion"] = result.completionTokens;
        out["tokens"]["reasoning"] = result.reasoningTokens;
        out["performance"]["tps"] = result.tokensPerSecond;
        out["performance"]["totalMs"] = result.totalTime.count();
        out["performance"]["firstTokenMs"] = result.firstTokenLatency.count();
        out["finishReason"] = result.finishReason;

        std::ofstream f("d:\\rawrxd\\prometheus_output.json");
        f << out.dump(4);
        f.close();
        std::cout << "\n✓ Output written to: d:\\rawrxd\\prometheus_output.json\n";

        return 0;
    }

    int runLegacy(const std::string& prompt) {
        std::cout << "=== RawrXD CLI — Real Inference + Autonomous Core ===" << std::endl;
        std::cout << "Version: 1.0 (ml64 / curl / libcurl)" << std::endl;

        // Initialize ML engine
        std::cout << "\n[1/4] Initializing ML Inference Engine..." << std::endl;
        auto& mlEngine = RawrXD::ML::MLInferenceEngine::getInstance();
        if (!mlEngine.initialize()) {
            std::cerr << "ERROR: Failed to connect to RawrEngine (localhost:23959)" << std::endl;
            std::cerr << "Is RawrEngine running? Start it with: RawrEngine.exe" << std::endl;
            outputErrorTelemetry("ML Engine init failed");
            return 1;
        }
        std::cout << "✓ ML engine connected to RawrEngine" << std::endl;

        // Initialize sovereign core
        std::cout << "\n[2/4] Initializing Sovereign Autonomous Core..." << std::endl;
        auto& sovCore = RawrXD::Sovereign::SovereignCore::getInstance();
        try {
            sovCore.initialize(1);  // 1 agent
            std::cout << "✓ Sovereign core initialized" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "ERROR: " << e.what() << std::endl;
            outputErrorTelemetry("Sovereign core init failed");
            return 1;
        }

        // Read user prompt
        std::cout << "\n[3/4] Enter your prompt (or press Enter for demo):" << std::endl;
        std::string userPrompt;
        std::getline(std::cin, userPrompt);

        if (userPrompt.empty()) {
            userPrompt = "Explain x86-64 assembly MASM stack frames and .ENDPROLOG";
        }

        std::cout << "\nPrompt: " << userPrompt << std::endl;

        // Run inference with token callback for live output
        std::cout << "\n[4/4] Running inference + sovereign cycle..." << std::endl;
        std::cout << "────────────────────────────────────────" << std::endl;

        size_t tokenCount = 0;
        auto tokenCallback = [&](const std::string& token) {
            std::cout << token << std::flush;
            tokenCount++;
        };

        auto result = mlEngine.query(userPrompt, tokenCallback, 512);

        std::cout << "\n────────────────────────────────────────" << std::endl;

        // Trigger autonomous cycle
        std::cout << "\nTriggering sovereign autonomous cycle..." << std::endl;
        try {
            auto cycleStats = sovCore.runCycle();
            std::cout << "✓ Cycle " << cycleStats.cycleCount << " complete"
                      << " | Status: " << (int)cycleStats.status
                      << " | Heals: " << cycleStats.healCount << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "WARNING: Cycle error: " << e.what() << std::endl;
        }

        // Build output telemetry
        json output;
        output["success"] = result.success;
        output["response"] = result.response;
        output["tokenCount"] = result.tokenCount;
        output["latencyMs"] = result.latencyMs;

        json telemetry = json::parse(mlEngine.telemetryToJSON());
        output["telemetry"] = telemetry;

        // Add sovereign stats
        json sovStats;
        try {
            auto stats = sovCore.getStats();
            sovStats["cycleCount"] = stats.cycleCount;
            sovStats["healCount"] = stats.healCount;
            sovStats["status"] = (int)stats.status;
            output["sovereignStats"] = sovStats;
        } catch (...) {
            // Ignore
        }

        // Output JSON to stdout and file
        std::string jsonOutput = output.dump(4);
        std::cout << "\n" << jsonOutput << std::endl;

        // Write telemetry to file
        std::ofstream telemetryFile("d:\\rawrxd\\telemetry_latest.json");
        telemetryFile << jsonOutput;
        telemetryFile.close();

        std::cout << "\n✓ Telemetry written to: d:\\rawrxd\\telemetry_latest.json" << std::endl;

        sovCore.shutdown();
        mlEngine.shutdown();

        return 0;
    }

private:
    void outputErrorTelemetry(const std::string& error) {
        json errorJson;
        errorJson["success"] = false;
        errorJson["error"] = error;
        errorJson["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

        std::cout << errorJson.dump(4) << std::endl;

        std::ofstream file("d:\\rawrxd\\telemetry_error.json");
        file << errorJson.dump(4);
        file.close();
    }
};

}

int main(int argc, char* argv[]) {
    try {
        RawrXD::CLI::RawrXDCLI cli;
        return cli.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 2;
    } catch (...) {
        std::cerr << "FATAL: Unknown error" << std::endl;
        return 3;
    }
}
