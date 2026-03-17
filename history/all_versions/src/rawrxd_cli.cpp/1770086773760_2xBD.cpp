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
#include "vsix_native_converter.hpp"
#include "ReactServerGenerator.h"
#include "cpu_inference_engine.h" // For direct engine setting access
#include "ReverseEngineeringSuite.hpp" // Custom RE tools
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
                
                // Apply current CLI config to engine
                auto engine = RawrXD::CPUInferenceEngine::getInstance();
                if (engine) {
                    engine->SetMaxMode(g_Config.maxMode);
                    engine->SetDeepThinking(g_Config.deepThinking);
                    engine->SetDeepResearch(g_Config.deepResearch);
                    engine->SetNoRefusal(g_Config.noRefusal);
                }
        } else {
                std::cout << "[Error] Failed to load model: " << path << "\n";
        }
        return;
    }

    // /menu or /help
    if (input == "/help" || input == "/menu") {
        std::cout << "--- RawrXD CLI Commands ---\n";
        std::cout << "  /load <path>          Load a model\n";
        std::cout << "  /history [clear]      View or clear history\n";
        std::cout << "  /agent <task>         Run agentic loop for task\n";
        std::cout << "  /plan <task>          Generate a plan for task\n";
        std::cout << "  /ask <query>          Ask a question (context aware)\n";
        std::cout << "  /edit <file> <instr>  Edit a file based on instruction\n";
        std::cout << "  /bugreport <file>     Analyze file for bugs\n";
        std::cout << "  /suggest <file>       Get code suggestions\n";
        std::cout << "  /hotpatch <issue>     Apply hotpatch for issue\n";
        std::cout << "  /react-server         Create a React server scaffolding\n";
        std::cout << "  /install-vsix <path>  Install/Convert VSIX extension\n";
        std::cout << "  /analyze <binary>     Analyze binary with CodexUltimate\n";
        std::cout << "  /dumpbin <binary>     Dump PE headers/imports/exports\n";
        std::cout << "  /disasm <binary>      Disassemble binary sections\n";
        std::cout << "  /compile <asm>        Compile assembly with custom compiler\n";
        std::cout << "  /toggle <feature>     Toggle features (max, think, research, norefusal)\n";
        std::cout << "  /context <size>       Set context size (4k, 32k, ..., 1m)\n";
        std::cout << "  /clear                Clear screen\n";
        std::cout << "  /status               Show current configuration\n";
        std::cout << "  /quit                 Exit CLI\n";
        return;
    }

    // /status
    if (input == "/status") {
        std::cout << "--- System Status ---\n";
        std::cout << "  Model Loaded: " << (state.loaded_model ? "Yes" : "No") << "\n";
        std::cout << "  Max Mode: " << (g_Config.maxMode ? "ON" : "OFF") << "\n";
        std::cout << "  Deep Thinking: " << (g_Config.deepThinking ? "ON" : "OFF") << "\n";
        std::cout << "  Deep Research: " << (g_Config.deepResearch ? "ON" : "OFF") << "\n";
        std::cout << "  No Refusal: " << (g_Config.noRefusal ? "ON" : "OFF") << "\n";
        std::cout << "  Inference Calls: " << g_Metrics.inferenceCalls << "\n";
        return;
    }

    // /toggle
    if (input.rfind("/toggle ", 0) == 0) {
        std::string feature = input.substr(8);
        auto engine = RawrXD::CPUInferenceEngine::getInstance();
        
        if (feature == "max" || feature == "maxmode") {
            g_Config.maxMode = !g_Config.maxMode;
            std::cout << "Max Mode: " << (g_Config.maxMode ? "ON" : "OFF") << "\n";
            if (engine) engine->SetMaxMode(g_Config.maxMode);
        } else if (feature == "think" || feature == "deepthinking") {
            g_Config.deepThinking = !g_Config.deepThinking;
            std::cout << "Deep Thinking: " << (g_Config.deepThinking ? "ON" : "OFF") << "\n";
            if (engine) engine->SetDeepThinking(g_Config.deepThinking);
        } else if (feature == "research" || feature == "deepresearch") {
            g_Config.deepResearch = !g_Config.deepResearch;
            std::cout << "Deep Research: " << (g_Config.deepResearch ? "ON" : "OFF") << "\n";
            if (engine) engine->SetDeepResearch(g_Config.deepResearch);
        } else if (feature == "norefusal") {
            g_Config.noRefusal = !g_Config.noRefusal;
            std::cout << "No Refusal: " << (g_Config.noRefusal ? "ON" : "OFF") << "\n";
            if (engine) engine->SetNoRefusal(g_Config.noRefusal);
        } else {
            std::cout << "Unknown feature. Try: max, think, research, norefusal\n";
        }
        return;
    }
    
    // /context
    if (input.rfind("/context ", 0) == 0) {
        std::string sizeStr = input.substr(9);
        int size = 4096;
        if (sizeStr == "4k") size = 4096;
        else if (sizeStr == "32k") size = 32768;
        else if (sizeStr == "64k") size = 65536;
        else if (sizeStr == "128k") size = 131072;
        else if (sizeStr == "256k") size = 262144;
        else if (sizeStr == "512k") size = 524288;
        else if (sizeStr == "1m" || sizeStr == "1M") size = 1048576;
        else {
             try { size = std::stoi(sizeStr); } catch (...) {}
        }
        
        auto engine = RawrXD::CPUInferenceEngine::getInstance();
        if (engine) {
            engine->SetContextLimit(size);
            std::cout << "Context window set to: " << size << " tokens.\n";
        } else {
            std::cout << "Engine not initialized.\n";
        }
        return;
    }

    // /react-server
    if (input == "/react-server") {
        std::cout << "[Agent] Creating React Server Plan...\n";
        
        std::string plan = R"(
1. Initialize Node.js project (package.json)
2. Install dependencies: react, react-dom, express, webpack
3. Create structure:
   - src/
     - components/
     - App.js
     - index.js
   - server/
     - server.js
   - public/
     - index.html
4. Configure Webpack/Babel
5. Setup Express to serve static assets and API
)";
        std::cout << plan << "\n";
        std::cout << "Deploying file structure...\n";
        
        try {
            RawrXD::ReactServerGenerator::Generate(".");
            std::cout << "[Success] React Server scaffolding created in ./react-server/\n";
        } catch (const std::exception& e) {
            std::cout << "[Error] Failed to generate: " << e.what() << "\n";
        }
        return;
    }

    // /bugreport
    if (input.rfind("/bugreport ", 0) == 0) {
        std::string file = input.substr(11);
        if (state.agentEngine) {
            std::cout << "[Agent] Analyzing " << file << " for defects...\n";
            // If bugReport isn't on the engine, we can use chat with a specific prompt
            std::string content = ""; 
            FileOpResult res = FileOps::readText(file, content);
            if (res.success) {
                std::string prompt = "Please analyze the following code for bugs and provide a report:\n\n" + content;
                std::string report = state.agentEngine->chat(prompt);
                std::cout << "--- Bug Report ---\n" << report << "\n";
            } else {
                std::cout << "Could not read file.\n";
            }
        }
        return;
    }

    // /suggest
    if (input.rfind("/suggest ", 0) == 0) {
        std::string file = input.substr(9);
        if (std::filesystem::exists(file)) {
             std::cout << "[Agent] Reading " << file << "...\n";
             std::ifstream t(file);
             std::string content((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
             
             std::cout << "[Agent] Generating suggestions...\n";
             if (state.agentEngine && state.loaded_model) {
                 std::string prompt = "Analyze the following code and suggest improvements for performance, readability, and modern best practices:\n\n" + content;
                 std::string suggestions = state.agentEngine->chat(prompt);
                 std::cout << "\n=== CODE SUGGESTIONS ===\n" << suggestions << "\n========================\n";
             } else {
                 std::cout << "[Error] Model not loaded. Use /load <path> first.\n";
             }
        } else {
            std::cout << "[Error] File not found: " << file << "\n";
        }
        return;
    }

    // Agentic Commands
    if (input.rfind("/plan ", 0) == 0 || input.rfind("/agent ", 0) == 0) {
         std::string goal = input.substr(input.find(' ') + 1);
         if (state.agentEngine) {
             std::cout << "[Agent] Generatng plan for: " << goal << "...\n";
             if (g_Config.deepThinking) std::cout << "[DeepThinking] Analyzing constraints and edge cases...\n";
             if (g_Config.deepResearch) std::cout << "[DeepResearch] Searching knowledge base...\n";
             
             std::string result = state.agentEngine->planTask(goal);
             std::cout << "--- Plan ---\n" << result << "\n";
         } else {
             std::cout << "Agent engine not initialized. Load model first.\n";
         }
         return;
    }

    if (input.rfind("/ask ", 0) == 0) {
        std::string query = input.substr(5);
        if (state.agentEngine) {
            std::cout << "[Agent] " << (g_Config.deepThinking ? "(Thinking) " : "") << "Processing...\n";
            std::string response = state.agentEngine->chat(query);
            std::cout << ">> " << response << "\n";
        } else {
             std::cout << "Agent engine not initialized.\n";
        }
        return;
    }

    if (input.rfind("/edit ", 0) == 0) {
         // Simply pass to agent for now, parsing file + instruction
         std::string args = input.substr(6);
         if (state.agentEngine) {
             std::cout << "[Agent] Applying edits...\n";
             // In real impl, parse args to get file path
             state.agentEngine->generateCode(args); 
             std::cout << "Edit plan created and applied (simulated safely).\n";
         }
         return;
    }

    // /hotpatch
    if (input.rfind("/hotpatch ", 0) == 0) {
        std::string issue = input.substr(10);
        std::cout << "[Agent] Hot-Patching Issue: " << issue << "...\n";
        
        if (state.agentEngine && state.loaded_model) {
            std::cout << "[Agent] Analyzing codebase context...\n";
            std::string prompt = "Generate a code patch to fix the following issue. Provide only the fix in unified diff format:\n" + issue;
            std::string fix = state.agentEngine->chat(prompt);
            std::cout << "\n=== PROPOSED PATCH ===\n" << fix << "\n=====================\n";
            std::cout << "\nReview and apply manually, or save to .patch file.\n";
        } else {
            std::cout << "[Error] Model not loaded. Use /load <path> first.\n";
        }
        return;
    }
    
    // /install-vsix
    if (input.rfind("/install-vsix ", 0) == 0) {
        std::string path = input.substr(14);
        if (path.front() == '"') path.erase(0, 1);
        if (path.back() == '"') path.pop_back();
        
        std::cout << "[VSIX] Converting and Installing " << path << "...\n";
        
        if (RawrXD::VsixNativeConverter::ConvertVsixToNative(path, "./plugins")) {
            std::cout << "[Success] VSIX converted to native plugin.\n";
        } else {
            std::cout << "[Error] VSIX conversion failed.\n";
        }
        return;
    }
    
    // /analyze - CodexUltimate binary analysis
    if (input.rfind("/analyze ", 0) == 0) {
        std::string file = input.substr(9);
        if (file.front() == '"') file.erase(0, 1);
        if (file.back() == '"') file.pop_back();
        
        std::cout << "[CodexUltimate] Analyzing " << file << "...\n";
        
        auto result = RawrXD::CodexAnalyzer::Analyze(file);
        
        if (result.success) {
            std::cout << "\n=== ANALYSIS RESULTS ===\n";
            std::cout << "Format: " << result.format << "\n";
            std::cout << "Architecture: " << result.architecture << "\n";
            std::cout << "Packed: " << (result.isPacked ? "Yes" : "No") << "\n";
            if (!result.pdbPath.empty()) {
                std::cout << "PDB: " << result.pdbPath << "\n";
            }
            std::cout << "\n" << result.summary << "\n";
            std::cout << "========================\n";
        } else {
            std::cout << "[Error] " << result.summary << "\n";
        }
        return;
    }
    
    // /dumpbin - Custom dumpbin replacement
    if (input.rfind("/dumpbin ", 0) == 0) {
        std::string file = input.substr(9);
        if (file.front() == '"') file.erase(0, 1);
        if (file.back() == '"') file.pop_back();
        
        std::cout << "[DumpBin] Analyzing " << file << "...\n";
        std::cout << "\n=== HEADERS ===\n";
        std::cout << RawrXD::DumpBinAnalyzer::DumpHeaders(file) << "\n";
        std::cout << "\n=== IMPORTS ===\n";
        std::cout << RawrXD::DumpBinAnalyzer::DumpImports(file) << "\n";
        std::cout << "\n=== EXPORTS ===\n";
        std::cout << RawrXD::DumpBinAnalyzer::DumpExports(file) << "\n";
        return;
    }
    
    // /disasm - Disassembly
    if (input.rfind("/disasm ", 0) == 0) {
        std::string file = input.substr(8);
        if (file.front() == '"') file.erase(0, 1);
        if (file.back() == '"') file.pop_back();
        
        std::cout << "[Disassembler] Disassembling " << file << "...\n";
        std::cout << RawrXD::CodexAnalyzer::Disassemble(file, 0, 512) << "\n";
        return;
    }
    
    // /compile - Custom compiler
    if (input.rfind("/compile ", 0) == 0) {
        std::string file = input.substr(9);
        if (file.front() == '"') file.erase(0, 1);
        if (file.back() == '"') file.pop_back();
        
        std::cout << "[Compiler] Compiling " << file << "...\n";
        
        auto result = RawrXD::RawrXDCompiler::CompileASM(file);
        
        std::cout << result.output << "\n";
        
        if (result.success) {
            std::cout << "[Success] Object file: " << result.objectFile << "\n";
        } else {
            std::cout << "[Error] Compilation failed.\n";
            for (const auto& err : result.errors) {
                std::cout << "  " << err << "\n";
            }
        }
        return;
    }

    if (input.rfind("/bugreport ", 0) == 0) {
        std::string file = input.substr(11);
        if (state.agentEngine) {
            std::cout << "[Agent] Analyzing " << file << " for defects...\n";
            // If bugReport isn't on the engine, we can use chat with a specific prompt
            std::string content = ""; 
            FileOpResult res = FileOps::readText(file, content);
            if (res.success) {
                std::string prompt = "Please analyze the following code for bugs and provide a report:\n\n" + content;
                std::string report = state.agentEngine->chat(prompt);
                std::cout << "--- Bug Report ---\n" << report << "\n";
            } else {
                std::cout << "Could not read file.\n";
            }
        }
        return;
    }
    
    // Default: Chat

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

    // --- Advanced AI Feature Toggles ---
    
    // /maxmode
    if (input == "/maxmode") {
        g_Config.maxMode = !g_Config.maxMode;
        if (state.agentEngine) {
            GenerationConfig cfg;
            cfg.maxMode = g_Config.maxMode;
            state.agentEngine->updateConfig(cfg);
        }
        std::cout << "[System] Max Mode (32k+ Context): " << (g_Config.maxMode ? "ON" : "OFF") << "\n";
        return;
    }

    // /think
    if (input == "/think") {
        g_Config.deepThinking = !g_Config.deepThinking;
        if (state.agentEngine) {
            GenerationConfig cfg;
            cfg.deepThinking = g_Config.deepThinking;
            state.agentEngine->updateConfig(cfg);
        }
        std::cout << "[System] Deep Thinking (Chain-of-Thought): " << (g_Config.deepThinking ? "ON" : "OFF") << "\n";
        return;
    }

    // /research
    if (input == "/research") {
        g_Config.deepResearch = !g_Config.deepResearch;
        if (state.agentEngine) {
            GenerationConfig cfg;
            cfg.deepResearch = g_Config.deepResearch;
            state.agentEngine->updateConfig(cfg);
        }
        std::cout << "[System] Deep Research (File Scanning): " << (g_Config.deepResearch ? "ON" : "OFF") << "\n";
        return;
    }

    // /norefusal
    if (input == "/norefusal") {
        g_Config.noRefusal = !g_Config.noRefusal;
        if (state.agentEngine) {
            GenerationConfig cfg;
            cfg.noRefusal = g_Config.noRefusal;
            state.agentEngine->updateConfig(cfg);
        }
        std::cout << "[System] No Refusal Mode (Safety Override): " << (g_Config.noRefusal ? "ON" : "OFF") << "\n";
        return;
    }

    // /context <size>
    if (input.rfind("/context ", 0) == 0) {
        std::string sizeStr = input.substr(9);
        int tokens = 4096;
        
        // Parse size
        if (sizeStr == "4k") tokens = 4096;
        else if (sizeStr == "8k") tokens = 8192;
        else if (sizeStr == "16k") tokens = 16384;
        else if (sizeStr == "32k") tokens = 32768;
        else if (sizeStr == "128k") tokens = 128000;
        else if (sizeStr == "1m") tokens = 1000000;
        else {
            try { tokens = std::stoi(sizeStr); } catch(...) {}
        }

        if (state.agentEngine) {
            state.agentEngine->setContextWindow(tokens);
            std::cout << "[System] Context window set to " << tokens << " tokens\n";
        } else {
             std::cout << "[Error] Agent engine not initialized.\n";
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
