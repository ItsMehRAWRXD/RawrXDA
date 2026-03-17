#include "cpu_inference_engine.h"
#include "agentic_engine.h"
#include "api_server.h"
#include "telemetry.h"
#include "settings.h"
#include "overclock_governor.h"
#include "overclock_vendor.h"
#include "modules/vsix_loader.h"
#include "interactive_shell.h"
#include "modules/memory_manager.h"
#include "advanced_features.h"
#include "modules/react_generator.h"
#include "modules/engine_manager.h"
#include "modules/codex_ultimate.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <conio.h>
#include <string>
#include <filesystem>
#include <chrono>
#include <sstream>

using namespace std::chrono_literals;

struct AppState {
    std::string model_path;
    bool loaded_model = false;
    std::shared_ptr<RawrXD::CPUInferenceEngine> inference_engine;
    std::shared_ptr<AgenticEngine> agent_engine;
    std::unique_ptr<APIServer> api_server;
    std::unique_ptr<OverclockGovernor> governor;
    std::unique_ptr<VSIXLoader> vsix_loader;
    std::unique_ptr<MemoryManager> memory_manager;
    std::unique_ptr<InteractiveShell> shell;
    std::unique_ptr<EngineManager> engine_manager;
    std::unique_ptr<CodexUltimate> codex;
    
    // Configuration
    float temperature = 0.8f;
    float top_p = 0.9f;
    int max_tokens = 2048;
    bool enable_max_mode = false;
    bool enable_deep_thinking = false;
    bool enable_deep_research = false;
    bool enable_no_refusal = false;
    bool enable_autocorrect = false;
    bool enable_overclock_governor = false;
    size_t context_size = 4096;
    uint32_t target_cpu_temp_c = 85;
    uint32_t target_gpu_temp_c = 85;
};

void EnsureDirectories() {
    std::filesystem::create_directories("plugins");
    std::filesystem::create_directories("memory_modules");
    std::filesystem::create_directories("models");
    std::filesystem::create_directories("sessions");
    std::filesystem::create_directories("projects");
    std::filesystem::create_directories("models/800b");
    std::filesystem::create_directories("engines");
}

void PrintBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                    RawrXD AI Shell v6.0                      ║
║                                                              ║
║  Zero Qt • Zero Python • Pure Performance                   ║
║  Multi-Engine • 800B Support • Reverse Engineering          ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

int main(int argc, char** argv) {
    AppState state;
    EnsureDirectories();
    PrintBanner();
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) {
            state.model_path = argv[++i];
        }
        else if (arg == "--max-mode") {
            state.enable_max_mode = true;
        }
        else if (arg == "--deep-thinking") {
            state.enable_deep_thinking = true;
        }
        else if (arg == "--deep-research") {
            state.enable_deep_research = true;
        }
        else if (arg == "--no-refusal") {
            state.enable_no_refusal = true;
        }
        else if (arg == "--autocorrect") {
            state.enable_autocorrect = true;
        }
        else if (arg == "--context-size" && i + 1 < argc) {
            std::string size = argv[++i];
            if (size == "4k") state.context_size = 4096;
            else if (size == "32k") state.context_size = 32768;
            else if (size == "64k") state.context_size = 65536;
            else if (size == "128k") state.context_size = 131072;
            else if (size == "256k") state.context_size = 262144;
            else if (size == "512k") state.context_size = 524288;
            else if (size == "1m") state.context_size = 1048576;
        }
        else if (arg == "--governor") {
            state.enable_overclock_governor = true;
        }
        else if (arg == "--temp" && i + 1 < argc) {
            state.temperature = std::stof(argv[++i]);
        }
        else if (arg == "--top-p" && i + 1 < argc) {
            state.top_p = std::stof(argv[++i]);
        }
        else if (arg == "--max-tokens" && i + 1 < argc) {
            state.max_tokens = std::stoi(argv[++i]);
        }
        else if (arg == "--engine" && i + 1 < argc) {
            // Pre-select engine
            std::string engine_id = argv[++i];
            state.engine_manager = std::make_unique<EngineManager>();
            state.engine_manager->LoadEngine("engines/" + engine_id, engine_id);
            state.engine_manager->SwitchEngine(engine_id);
        }
        else if (arg == "--help") {
            std::cout << R"(
RawrXD CLI v6.0 - Ultimate AI Shell

Usage:
  RawrXD_CLI [options]

Options:
  --model <path>          Path to GGUF model file
  --max-mode              Enable max mode (32K+ context)
  --deep-thinking         Enable deep thinking mode
  --deep-research         Enable deep research mode
  --no-refusal            Enable no refusal mode
  --autocorrect           Enable auto-correction
  --context-size <size>   Set context size (4k/32k/64k/128k/256k/512k/1m)
  --governor              Enable overclock governor
  --temp <value>          Set temperature (default: 0.8)
  --top-p <value>         Set top-p (default: 0.9)
  --max-tokens <value>    Set max tokens (default: 2048)
  --engine <id>           Pre-select engine (800b-5drive, codex-ultimate, rawrxd-compiler)
  --help                  Show this help

ENGINE-SPECIFIC:
  800B Model Engine:
    !engine load800b <model>    - Load 800B model
    !engine setup5drive <dir>   - Setup 5-drive layout
    !engine verify              - Verify drive setup

  Codex Ultimate:
    !engine disasm <file>       - Disassemble binary
    !engine dumpbin <file>      - Dump PE/COFF info
    !engine analyze <file>      - Agentic analysis

  RawrXD Compiler:
    !engine compile <file>      - Compile with MASM64
    !engine optimize <file>     - Optimize code

INTERACTIVE COMMANDS:
  /plan <task>            - Create execution plan
  /react-server <name>    - Generate React project
  /bugreport <target>     - Analyze for bugs
  /suggest <code>         - Get code suggestions
  /hotpatch f old new     - Apply code hotpatch
  /analyze <target>       - Deep code analysis
  /optimize <target>      - Performance optimization
  /security <target>      - Security vulnerability scan

MODE TOGGLES:
  /maxmode <on|off>       - Toggle max mode
  /deepthinking <on|off>  - Toggle deep thinking
  /deepresearch <on|off>  - Toggle deep research
  /norefusal <on|off>     - Toggle no refusal
  /autocorrect <on|off>   - Toggle auto-correction

SHELL COMMANDS:
  /clear                  - Clear history
  /save <filename>        - Save conversation
  /load <filename>        - Load conversation
  /status                 - Show system status
  /help                   - Show this help
  /exit                   - Exit

For more help: https://github.com/ItsMehRAWRXD/RawrXD/wiki
)";
            return 0;
        }
    }
    
    // Initialize engine manager
    state.engine_manager = std::make_unique<EngineManager>();
    
    // Load default engines
    state.engine_manager->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive");
    state.engine_manager->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate");
    state.engine_manager->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler");
    
    // Initialize VSIX loader
    state.vsix_loader = std::make_unique<VSIXLoader>();
    state.vsix_loader->Initialize("plugins");
    
    // Initialize memory manager
    state.memory_manager = std::make_unique<MemoryManager>();
    state.memory_manager->SetContextSize(state.context_size);
    
    // Initialize inference engine
    state.inference_engine = std::make_shared<RawrXD::CPUInferenceEngine>();
    
    // Load model
    while (!state.loaded_model) {
        if (state.model_path.empty()) {
            std::cout << "Enter model path (GGUF/Blob): ";
            std::getline(std::cin, state.model_path);
            if (state.model_path.empty()) continue;
        }
        
        // Remove quotes
        if (!state.model_path.empty() && state.model_path.front() == '"') 
            state.model_path.erase(0, 1);
        if (!state.model_path.empty() && state.model_path.back() == '"') 
            state.model_path.pop_back();
        
        std::cout << "[Loading] " << state.model_path << "..." << std::endl;
        if (state.inference_engine->loadModel(state.model_path).has_value()) {
            state.loaded_model = true;
            std::cout << "[Success] Model loaded: " << state.model_path << std::endl;
        } else {
            std::cerr << "[Error] Failed to load model: " << state.model_path << std::endl;
            state.model_path.clear();
        }
    }
    
    // Initialize agentic engine
    state.agent_engine = std::make_shared<AgenticEngine>();
    state.agent_engine->setInferenceEngine(state.inference_engine.get());
    
    // Configure agent
    GenerationConfig config;
    config.temperature = state.temperature;
    config.top_p = state.top_p;
    config.max_tokens = state.max_tokens;
    config.maxMode = state.enable_max_mode;
    config.deepThinking = state.enable_deep_thinking;
    config.deepResearch = state.enable_deep_research;
    config.noRefusal = state.enable_no_refusal;
    config.autoCorrect = state.enable_autocorrect;
    state.agent_engine->updateConfig(config);
    
    // Initialize API server
    state.api_server = std::make_unique<APIServer>(state);
    state.api_server->Start(11434);
    std::cout << "[API] Server started on http://localhost:11434" << std::endl;
    
    // Initialize governor if requested
    if (state.enable_overclock_governor) {
        state.governor = std::make_unique<OverclockGovernor>();
        state.governor->Start(state);
        std::cout << "[Governor] Started" << std::endl;
    }
    
    // Initialize Codex Ultimate
    state.codex = std::make_unique<CodexUltimate>();
    
    // Initialize interactive shell
    ShellConfig shell_config;
    shell_config.cli_mode = true;
    shell_config.auto_save_history = true;
    shell_config.history_file = "shell_history.txt";
    shell_config.max_history_size = 1000;
    shell_config.enable_suggestions = true;
    shell_config.enable_autocomplete = true;
    
    state.shell = std::make_unique<InteractiveShell>(shell_config);
    state.shell->Start(state.agent_engine.get(), state.memory_manager.get(), state.vsix_loader.get(),
                      nullptr, // React generator not needed in CLI
                      [](const std::string& output) {
                          std::cout << output;
                      },
                      [](const std::string& input) {
                          // Input handling is done by shell
                      }
    );
    
    // Main loop
    std::cout << "\n=== RawrXD Interactive AI Shell v6.0 Ready ===\n";
    std::cout << "Model: " << state.model_path << "\n";
    std::cout << "Context: " << state.context_size / 1024 << "K\n";
    std::cout << "Engine: " << state.engine_manager->GetCurrentEngine() << "\n";
    std::cout << "Modes: " << (state.enable_max_mode ? "Max " : "") 
              << (state.enable_deep_thinking ? "Thinking " : "")
              << (state.enable_deep_research ? "Research " : "")
              << (state.enable_no_refusal ? "NoRefusal " : "")
              << (state.enable_autocorrect ? "AutoCorrect " : "") << "\n";
    std::cout << "Type /help for commands, /exit to quit\n\n";
    
    // Run shell
    while (state.shell->IsRunning()) {
        std::this_thread::sleep_for(100ms);
    }
    
    // Cleanup
    if (state.governor) {
        state.governor->Stop();
        std::cout << "[Governor] Stopped\n";
    }
    
    state.api_server->Stop();
    std::cout << "[API] Stopped\n";
    
    std::cout << "\n[Shutdown] RawrXD terminated gracefully.\n";
    return 0;
}
