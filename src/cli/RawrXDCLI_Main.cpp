#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "inference/MLInferenceEngine.hpp"
#include "sovereign/SovereignCoreWrapper.hpp"

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
 * RawrXD CLI with Real ML Inference
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
