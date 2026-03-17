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
#include "native_agent.hpp" // Fully integrated Native Agent
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
    bool maxMode = false;
    bool deepThinking = false;
    bool deepResearch = false;
    bool noRefusal = false;
    bool researchMode = false;
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
    while (!input.empty() && (input.back() == '\r' || input.back() == ' ' || input.back() == '\t')) input.pop_back();
    while (!input.empty() && (input.front() == ' ' || input.front() == '\t')) input.erase(0, 1);

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
    if (input.rfind("/metrics", 0) == 0) {
        std::cout << "--- Performance Metrics ---\n";
        std::cout << "Commands Executed: " << g_Metrics.commandsExecuted << "\n";
        std::cout << "Inference Calls: " << g_Metrics.inferenceCalls << "\n";
        std::cout << "Max Mode: " << (g_Config.maxMode ? "ON" : "OFF") << "\n";
        std::cout << "Deep Thinking: " << (g_Config.deepThinking ? "ON" : "OFF") << "\n";
        std::cout << "Deep Research: " << (g_Config.researchMode ? "ON" : "OFF") << "\n";
        std::cout << "No Refusal: " << (g_Config.noRefusal ? "ON" : "OFF") << "\n";
        return;
    }

    // /config
    if (input.rfind("/config ", 0) == 0) {
        auto args = split(input.substr(8), ' ');
        if (args.size() >= 2 && args[0] == "set") {
            if (args[1] == "verbose") g_Config.verbose = (args.size() > 2 && args[2] == "true");
            else if (args[1] == "highlight") g_Config.syntaxHighlighting = (args.size() > 2 && args[2] == "true");
            else if (args[1] == "maxmode") g_Config.maxMode = (args.size() > 2 && args[2] == "true");
            else if (args[1] == "deepthink") g_Config.deepThinking = (args.size() > 2 && args[2] == "true");
            else if (args[1] == "deepresearch") g_Config.deepResearch = (args.size() > 2 && args[2] == "true");
            else if (args[1] == "norefusal") g_Config.noRefusal = (args.size() > 2 && args[2] == "true");
            else if (args[1] == "researchmode") g_Config.researchMode = (args.size() > 2 && args[2] == "true");
            std::cout << "Config updated.\n";
            
            // Apply to hub's engine config
            GenerationConfig genCfg;
            genCfg.maxMode = g_Config.maxMode;
            genCfg.deepThinking = g_Config.deepThinking;
            genCfg.deepResearch = g_Config.deepResearch;
            genCfg.noRefusal = g_Config.noRefusal;
            hub->updateAgentConfig(genCfg);
        } else if (args.size() >= 2 && args[0] == "get") {
             if (args[1] == "verbose") std::cout << "verbose: " << g_Config.verbose << "\n";
             else if (args[1] == "maxmode") std::cout << "maxmode: " << g_Config.maxMode << "\n";
             else if (args[1] == "deepthink") std::cout << "deepthink: " << g_Config.deepThinking << "\n";
             else if (args[1] == "deepresearch") std::cout << "deepresearch: " << g_Config.deepResearch << "\n";
             else if (args[1] == "norefusal") std::cout << "norefusal: " << g_Config.noRefusal << "\n";
             else if (args[1] == "researchmode") std::cout << "researchmode: " << g_Config.researchMode << "\n";
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

    // Agent processing with Advanced Features
    NativeAgent agent;
    // Apply Config
    agent.setMaxMode(g_Config.maxMode);
    agent.setDeepThinking(g_Config.deepThinking);
    agent.setDeepResearch(g_Config.deepResearch);
    agent.setNoRefusal(g_Config.noRefusal);

    bool agentProcessed = false;

    if (input.rfind("/bugreport ", 0) == 0) {
            std::string context = input.substr(11);
            std::cout << "[Agent] Analyzing for bugs: " << context << "...\n";
            // Apply advanced features to bug report
            std::string enhancedContext = context;
            if (g_Config.deepResearch) {
                enhancedContext = AdvancedFeatures::DeepResearch(context);
            }
            if (g_Config.deepThinking) {
                enhancedContext = AdvancedFeatures::ChainOfThought(enhancedContext);
            }
            if (g_Config.noRefusal) {
                enhancedContext = AdvancedFeatures::NoRefusal(enhancedContext);
            }
            AgentResult res = agent.BugReport(enhancedContext); 
            std::cout << "[Report] Found issues:\n";
            std::cout << res.content << "\n";
            agentProcessed = true;
    }
    else if (input.rfind("/suggest ", 0) == 0) {
        std::string context = input.substr(9);
        std::cout << "[Agent] Generating suggestions for: " << context << "...\n";
        // Apply advanced features to suggestions
        std::string enhancedContext = context;
        if (g_Config.deepResearch) {
            enhancedContext = AdvancedFeatures::DeepResearch(context);
        }
        if (g_Config.deepThinking) {
            enhancedContext = AdvancedFeatures::ChainOfThought(enhancedContext);
        }
        if (g_Config.noRefusal) {
            enhancedContext = AdvancedFeatures::NoRefusal(enhancedContext);
        }
        AgentResult res = agent.Suggest(enhancedContext);
        std::cout << "[Suggestions]:\n" << res.content << "\n";
        agentProcessed = true;
    }
    else if (input.rfind("/plan ", 0) == 0) {
            std::string description = input.substr(6);
            std::cout << "[Agent] creating plan for: " << description << "...\n";
            // Apply advanced features to planning
            std::string enhancedDescription = description;
            if (g_Config.deepResearch) {
                enhancedDescription = AdvancedFeatures::DeepResearch(description);
            }
            if (g_Config.deepThinking) {
                enhancedDescription = AdvancedFeatures::ChainOfThought(enhancedDescription);
            }
            if (g_Config.noRefusal) {
                enhancedDescription = AdvancedFeatures::NoRefusal(enhancedDescription);
            }
            AgentResult res = agent.Plan(enhancedDescription); 
            std::cout << "[Plan]:\n" << res.content << "\n";
            agentProcessed = true;
    }
    else if (input.rfind("/ask ", 0) == 0) {
            std::string question = input.substr(5);
            std::cout << "[Agent] " << question << "\n";
            // Apply advanced features to Q&A
            std::string enhancedQuestion = question;
            if (g_Config.deepResearch) {
                enhancedQuestion = AdvancedFeatures::DeepResearch(question);
            }
            if (g_Config.deepThinking) {
                enhancedQuestion = AdvancedFeatures::ChainOfThought(enhancedQuestion);
            }
            if (g_Config.noRefusal) {
                enhancedQuestion = AdvancedFeatures::NoRefusal(enhancedQuestion);
            }
            AgentResult res = agent.Ask(enhancedQuestion);
            std::cout << "[Answer]: " << res.content << "\n"; 
            agentProcessed = true;
    }
    else if (input.rfind("/edit ", 0) == 0) {
            std::string args = input.substr(6);
            // Splitting args into file and instructions would be better, but for now passing all
            std::cout << "[Agent] applying edit: " << args << "...\n";
            
            // Simple space split for file vs instructions
            std::string filename, instr;
            size_t spacePos = args.find(' ');
            if(spacePos != std::string::npos) {
                filename = args.substr(0, spacePos);
                instr = args.substr(spacePos+1);
            } else {
                filename = args; instr = "Fix this file.";
            }

            AgentResult res = agent.Edit(filename, instr);
            std::cout << "[Edited Code]:\n" << res.content << "\n";
            
            // Auto-correction simulation
            if (res.content.empty() || res.content.find("error") != std::string::npos) {
                std::cout << "[Agent] Detected potential error in output. Attempting AutoFix...\n";
                AgentResult fixRes = agent.AutoFix(res.content, "Output was empty or contained error keyword.");
                std::cout << "[AutoFix Result]:\n" << fixRes.content << "\n";
            }
            
            agentProcessed = true;
    }
    else if (input.rfind("/agent ", 0) == 0) {
            // General agent command
            std::string q = input.substr(7);
            AgentResult res = agent.Ask(q);
             std::cout << "[Agent]: " << res.content << "\n";
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
    

    // /config
    if (input.rfind("/config ", 0) == 0) {
        std::vector<std::string> parts = split(input.substr(8), ' ');
        if (parts.size() >= 2) {
             std::string key = parts[0];
             bool val = (parts[1] == "true" || parts[1] == "1" || parts[1] == "on");
             
             if (key == "maxmode") g_Config.maxMode = val;
             else if (key == "deepthink") g_Config.deepThinking = val;
             else if (key == "deepresearch") g_Config.deepResearch = val;
             else if (key == "norefusal") g_Config.noRefusal = val;
             else if (key == "researchmode") g_Config.researchMode = val;
             
             // Sync with Engine
             GenerationConfig genConf;
             genConf.maxMode = g_Config.maxMode;
             genConf.deepThinking = g_Config.deepThinking;
             genConf.deepResearch = g_Config.deepResearch;
             genConf.noRefusal = g_Config.noRefusal;
             
             if (hub->getAgent()) hub->getAgent()->updateConfig(genConf);
             std::cout << "Config updated: " << key << " = " << (val ? "ON" : "OFF") << "\n";
        }
        else {
             std::cout << "Usage: /config <key> <true/false>\n";
        }
        return;
    }

    // /plan
    if (input.rfind("/plan ", 0) == 0) {
        std::string goal = input.substr(6);
        if (hub->getAgent()) {
             std::cout << "[Agent] Planning: " << goal << "\n";
             std::string planJson = hub->getAgent()->planTask(goal);
             std::cout << "[Plan]:\n" << planJson << "\n";
             std::cout << "Execute this plan? (y/n): ";
             char c; std::cin >> c;
             if (c == 'y') {
                 std::cout << "[Agent] Executing...\n";
                 std::string report = hub->getAgent()->executePlan(planJson);
                 std::cout << report << "\n";
             }
        }
        return;
    }

    // /ask or /chat
    if (input.rfind("/ask ", 0) == 0 || input.rfind("/chat ", 0) == 0) {
        std::string msg = input.substr(5); // Roughly, check length
        std::cout << "RawrXD: " << hub->chat(msg) << "\n";
        return;
    }

    // /bugreport
    if (input.rfind("/bugreport ", 0) == 0) {
        if (hub->getAgent()) {
            std::string arg = input.substr(11); // filename or code
            std::string code = arg; 
            // Try to read file if arg is a path
            std::string fileContent;
            if (FileOps::readText(arg, fileContent).success) {
                code = fileContent;
                std::cout << "[Agent] Analyzing file: " << arg << "\n";
            }
            std::string report = hub->getAgent()->bugReport(code, "User Requested Audit");
            std::cout << "[Report]:\n" << report << "\n";
        }
        return;
    }
    
    // /suggestions
    if (input.rfind("/suggest ", 0) == 0) {
        if (hub->getAgent()) {
            std::string arg = input.substr(9);
            std::string code = arg;
            std::string fileContent;
            if (FileOps::readText(arg, fileContent).success) {
                code = fileContent;
            }
            std::cout << "[Agent] Generating suggestions...\n";
            std::cout << hub->getAgent()->codeSuggestions(code) << "\n";
        }
        return;
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
        std::cout << "          Keys: verbose, maxmode, deepthink, deepresearch, norefusal\n";
        std::cout << "Batch:    /batch <filepath>\n";
        std::cout << "\nCurrent Mode: ";
        if (g_Config.maxMode) std::cout << "[MAX] ";
        if (g_Config.deepThinking) std::cout << "[DEEP-THINK] ";
        if (g_Config.deepResearch) std::cout << "[RESEARCH] ";
        if (g_Config.noRefusal) std::cout << "[UNCENSORED] ";
        std::cout << "\n";
        
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
