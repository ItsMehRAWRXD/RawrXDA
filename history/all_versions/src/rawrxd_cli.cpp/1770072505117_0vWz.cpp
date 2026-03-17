#include "gui.h"
#include "../include/ai_integration_hub.h" // Real Hub integration
// #include "minimal_hub_stub.hpp" 
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
using namespace RawrXD; // Add this
using namespace std::chrono_literals;

// --- CLI State Management ---
struct CLIConfig {
    bool syntaxHighlighting = false; 
    bool verbose = false;
    std::string promptStyle = ">> ";
};

struct PerformanceMetrics {
    int commandsExecuted = 0;
    int inferenceCalls = 0;
    long long totalInferenceTimeMs = 0;
    long long lastCommandTimeMs = 0;
};

static CLIConfig g_Config;
static PerformanceMetrics g_Metrics;
static std::vector<std::string> g_CommandHistory;

void addToHistory(const std::string& cmd) {
    if (cmd.empty()) return;
    g_CommandHistory.push_back(cmd);
    if (g_CommandHistory.size() > 100) g_CommandHistory.erase(g_CommandHistory.begin());
}

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
// Helper: Process Command
void ProcessCommand(std::string input, AppState& state, std::shared_ptr<AIIntegrationHub> hub) {
    if (input.empty()) return;
    addToHistory(input);
    g_Metrics.commandsExecuted++;
    
    // /load
    if (input.rfind("/load ", 0) == 0) {
        std::string path = input.substr(6);
        if (hub->loadModel(path)) {
                std::cout << "[System] Model loaded: " << path << "\n";
                state.loaded_model = true;
        } else {
                std::cout << "[Error] Failed to load model: " << path << "\n";
        }
        return;
    }

    // /history
    if (input.rfind("/history", 0) == 0) {
        if (input.find("clear") != std::string::npos) {
            g_CommandHistory.clear();
            std::cout << "History cleared.\n";
        } else {
            std::cout << "--- Command History ---\n";
            for (const auto& h : g_CommandHistory) std::cout << h << "\n";
        }
        return;
    }

    // /metrics
    if (input == "/metrics") {
        std::cout << "--- Performance Metrics ---\n";
        std::cout << "Commands Executed: " << g_Metrics.commandsExecuted << "\n";
        std::cout << "Inference Calls:   " << g_Metrics.inferenceCalls << "\n";
        std::cout << "Total Inf Time:    " << g_Metrics.totalInferenceTimeMs << "ms\n";
        return;
    }

    // /config
    if (input.rfind("/config ", 0) == 0) {
        auto args = split(input.substr(8), ' ');
        if (args.size() >= 2 && args[0] == "set") {
            if (args[1] == "verbose") g_Config.verbose = (args.size() > 2 && args[2] == "true");
            else if (args[1] == "highlight") g_Config.syntaxHighlighting = (args.size() > 2 && args[2] == "true");
            std::cout << "Config updated.\n";
        } else if (args.size() >= 2 && args[0] == "get") {
             if (args[1] == "verbose") std::cout << "verbose: " << g_Config.verbose << "\n";
             else std::cout << "Unknown key.\n";
        }
        return;
    }

    // /batch
    if (input.rfind("/batch ", 0) == 0) {
        std::string path = input.substr(7);
        std::string content;
        FileOpResult res = FileOps::readText(path, content);
        if (res.success) {
            std::cout << "Executing batch: " << path << "\n";
            std::stringstream ss(content);
            std::string line;
            while(std::getline(ss, line)) {
                // Trim logic usually needed here
                if(!line.empty()) {
                    std::cout << "[Batch] >> " << line << "\n";
                    ProcessCommand(line, state, hub);
                }
            }
        } else {
            std::cout << "Failed to read batch file: " << path << "\n";
        }
        return;
    }

    // Agent processing
    AdvancedCodingAgent agent;
    bool agentProcessed = false;

    if (input.rfind("/bugreport ", 0) == 0) {
            std::string context = input.substr(11);
            std::cout << "[Agent] Analyzing for bugs: " << context << "...\n";
            AgentTaskResult res = agent.detectBugs(context, "cpp"); 
            std::cout << "[Report] Found " << res.suggestions.size() << " issues.\n";
            for(const auto& s : res.suggestions) std::cout << " - " << s << "\n";
            agentProcessed = true;
    }
    else if (input.rfind("/suggest ", 0) == 0) {
        std::string context = input.substr(9);
        std::cout << "[Agent] Generating suggestions for: " << context << "...\n";
        AgentTaskResult res = agent.optimizeForPerformance(context, "cpp");
        std::cout << "[Suggestions]:\n" << res.generatedCode << "\n";
        for(const auto& s : res.suggestions) std::cout << " * Tip: " << s << "\n";
        agentProcessed = true;
    }
    else if (input.rfind("/plan ", 0) == 0) {
            std::string description = input.substr(6);
            std::cout << "[Agent] creating plan for: " << description << "...\n";
            AgentTaskResult res = agent.implementFeature(description, "markdown", "", {}); 
            std::cout << "[Plan]:\n";
            for(const auto& step : res.steps) std::cout << " 1. " << step << "\n";
            agentProcessed = true;
    }
    else if (input.rfind("/ask ", 0) == 0) {
            std::string question = input.substr(5);
            std::cout << "[Agent] " << question << "\n";
            AgentTaskResult res = agent.implementFeature("Answer this question: " + question, "text", "", {});
            std::cout << "[Answer]: " << res.generatedCode << "\n"; 
            agentProcessed = true;
    }
    else if (input.rfind("/edit ", 0) == 0) {
            std::string args = input.substr(6);
            std::cout << "[Agent] applying edit: " << args << "...\n";
            AgentTaskResult res = agent.refactorCode(args, "cpp", "improve_names");
            std::cout << "[Edited Code]:\n" << res.generatedCode << "\n";
            agentProcessed = true;
    }
    
    if (agentProcessed) return;

    // /file
    if (input.rfind("/file ", 0) == 0) {
            auto args = split(input.substr(6), ' ');
            if (args.empty()) { std::cout << "Usage: /file <op> <args>\n"; return; }
            
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
            return;
    }
    
    // /patch
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
        return;
    }

    // /agent
    if (input.rfind("/agent ", 0) == 0) {
        std::string prompt = input.substr(7);
        std::cout << "[Agent] Deep Thinking: " << prompt << "...\n";
        // Use Real Hub -> Agentic Engine
        std::string result = hub->planTask(prompt); 
        std::cout << "[Result]:\n" << result << "\n";
        return;
    }

    std::cout << "RawrXD: ";
    std::string response = hub->chat(input);
    std::cout << response << std::endl;
}

// Helper: Interactive Chat Mode
void RunInteractiveChat(AppState& state, std::shared_ptr<AIIntegrationHub> hub) {
    if (!state.loaded_model) {
        std::cout << "[Info] Hub initialized (model status managed internally).\n";
    }

    std::cout << "\n=== RawrXD Interactive Chat (Hub Integration) ===\n";
    std::cout << "Type 'exit' to return to menu.\n";
    std::cout << "Commands: /load <path>, /audit <path>\n";
    std::cout << "Agent:    /agent <feature|test> <desc>\n";
    std::cout << "Tasks:    /plan, /ask, /edit, /bugreport, /suggest <context>\n";
    std::cout << "File Ops: /file <ls|read|write|cp|mv|rm|mkdir> <args>\n";
    std::cout << "Patching: /patch <view|apply|clear>\n";
    std::cout << "History:  /history <clear|show>\n";
    std::cout << "Metrics:  /metrics\n";
    std::cout << "Config:   /config <set|get> <key> [value]\n";
    std::cout << "Batch:    /batch <filepath>\n";
    
    // Clear buffer
    while (_kbhit()) _getch();

    std::string input;
    while (true) {
        std::cout << "\n>> ";
        if (!std::getline(std::cin, input)) break;
        if (input == "exit" || input == "quit") break;

        // Trim trailing whitespace and carriage returns
        while (!input.empty() && (input.back() == '\r' || input.back() == ' ' || input.back() == '\t')) {
            input.pop_back();
        }

        ProcessCommand(input, state, hub);
    }
    std::cout << "=== Exiting Chat ===\n";
}

// End of implementation

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
