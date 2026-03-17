#include "gui.h"
#include "../include/ai_integration_hub.h" // Use Hub instead of direct engine
// #include "cpu_inference_engine.h"
#include "agentic_engine.h"
#include "api_server.h"
#include "telemetry.h"
#include "settings.h"
#include "overclock_governor.h"
#include "overclock_vendor.h"
#include "tools/file_ops.h"
#include "agent_hot_patcher.hpp"
#include "../include/AdvancedCodingAgent.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <conio.h>
#include <string>
#include <filesystem>
#include <sstream>
#include <vector>

using namespace RawrXD::Tools;
using namespace RawrXD::IDE;
using namespace std::chrono_literals;

// Helper: Split string
std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

// Helper: Interactive Chat Mode
void RunInteractiveChat(AppState& state, std::shared_ptr<AIIntegrationHub> hub) {
    if (!state.loaded_model) {
        std::cout << "[Info] Hub initialized (model status managed internally).\n";
    }

    std::cout << "\n=== RawrXD Interactive Chat (Hub Integration) ===\n";
    std::cout << "Type 'exit' to return to menu.\n";
    std::cout << "Commands: /load <path>, /audit <path>\n";
    std::cout << "File Ops: /file <ls|read|write|cp|mv|rm|mkdir> <args>\n";
    std::cout << "Patching: /patch <view|apply|clear>\n";
    std::cout << "Agent:    /agent <feature|test> <desc>\n";
    
    // Clear buffer
    while (_kbhit()) _getch();

    std::string input;
    while (true) {
        std::cout << "\n>> ";
        if (!std::getline(std::cin, input)) break;
        if (input == "exit" || input == "quit") break;
        
        // Hub handles commands like /audit. For /load, we can use Hub API.
        if (input.rfind("/load ", 0) == 0) {
            std::string path = input.substr(6);
            if (hub->loadModel(path)) {
                 std::cout << "[System] Model loaded: " << path << "\n";
                 state.loaded_model = true;
            } else {
                 std::cout << "[Error] Failed to load model: " << path << "\n";
            }
            continue;
        }

        // File Operations
        if (input.rfind("/file ", 0) == 0) {
            auto args = split(input.substr(6), ' ');
            if (args.empty()) { std::cout << "Usage: /file <op> <args>\n"; continue; }
            
            std::string op = args[0];
            if (op == "ls" && args.size() >= 2) {
                std::vector<std::string> files;
                FileOps::list(args[1], files, false);
                for (const auto& f : files) std::cout << f << "\n";
            }
            else if (op == "read" && args.size() >= 2) {
                std::string content;
                FileOpResult res = FileOps::readText(args[1], content);
                if (res.success) std::cout << content << "\n";
                else std::cout << "Error: " << res.message << "\n";
            }
            else if (op == "write" && args.size() >= 3) {
                // simple write, handles spaces badly if not quoted strings, but sufficient for simple test
                std::string content = input.substr(input.find(args[2])); 
                FileOpResult res = FileOps::writeText(args[1], content, true);
                std::cout << (res.success ? "Wrote file.\n" : "Write failed.\n");
            }
            else if (op == "rm" && args.size() >= 2) {
                 FileOpResult res = FileOps::remove(args[1]);
                 std::cout << (res.success ? "Deleted.\n" : "Delete failed.\n");
            }
            else {
                std::cout << "Unknown file op or missing args.\n";
            }
            continue;
        }
        
        // Agent HotPatching
        if (input.rfind("/patch ", 0) == 0) {
            std::string op = input.substr(7);
            if (op == "view") {
                auto patches = AgentHotPatcher::instance()->getStagedPatches();
                std::cout << "Staged Patches: " << patches.size() << "\n";
                for (size_t i = 0; i < patches.size(); i++) {
                    std::cout << "[" << i << "] " << patches[i].uri << "\n";
                }
            } else {
                std::cout << "Patch op " << op << " not fully impl.\n";
            }
            continue;
        }

        // Advanced Agent
        if (input.rfind("/agent ", 0) == 0) {
            std::string prompt = input.substr(7);
            std::cout << "[Agent] working on: " << prompt << "...\n";
            AdvancedCodingAgent agent;
            // Mocking context/lang for CLI
            AgentTaskResult res = agent.implementFeature(prompt, "No context", "cpp"); 
            std::cout << "[Result] Success: " << res.success << "\n";
            std::cout << res.result << "\n";
            continue;
        }

        if (input.empty()) continue;

        std::cout << "RawrXD: ";
        
        // Use Hub Chat (supports /audit)
        std::string response = hub->chat(input);
        std::cout << response << std::endl;
    }
    std::cout << "=== Exiting Chat ===\n";
}

int main(int argc, char** argv) {
    AppState state;
    
    // 0. Parse Args for Model
    if (argc > 1) {
        state.model_path = argv[1];
    }

    // Load settings
    Settings::LoadCompute(state);
    Settings::LoadOverclock(state);

    std::cout << "RawrXD CLI v2.0 - Hub Integration Build\n";
    
    // 1. Initialize Hub
    auto hub = std::make_shared<AIIntegrationHub>();
    hub->initialize(state.model_path);
    state.loaded_model = true; // Assume hub init works or handles it
    
    telemetry::Initialize();

    RunInteractiveChat(state, hub);
    
    telemetry::Shutdown();

    return 0;
}
