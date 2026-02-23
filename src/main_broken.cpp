#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <csignal>
#include <iomanip>
#include <thread>

#include "logging/logger.h"
static Logger s_logger("main_broken");

#include "vsix_loader.h"
#include "memory_core.h"
#include "hot_patcher.h"
#include "engine/rawr_engine.h"
#include "runtime_core.h"
#include "universal_generator_service.h"

#include "shared_context.h"
#include "interactive_shell.h"
#include "agentic_engine.h"
#include "modules/memory_manager.h"
#include "modules/react_generator.h"
#include "ide_window.h"

// ANSI Color Codes
#define COLOR_RESET   "\033[0m"

#define COLOR_CYAN    "\033[36m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
#define COLOR_MAGENTA "\033[35m"

void PrintBanner() {
    s_logger.info( COLOR_MAGENTA << R"(
    ____  ___ _       _____  ____     _  ______ 
   / __ \/   | |     / / __ \/ __ \   | |/ /__ \
  / /_/ / /| | | /| / / /_/ / /_/ /   |   /__/ /
 / _, _/ ___ | |/ |/ / _, _/ _, _/   /   |  / / 
/_/ |_/_/  |_|__/|__/_/ |_/_/ |_|   /_/|_| /_/  
    )" << COLOR_RESET << std::endl;
    s_logger.info( COLOR_CYAN << "    REV 7.0 - ULTIMATE FINAL IMPLEMENTATION\n" << COLOR_RESET;
    s_logger.info("    Zero Simulation | Native Memory | Live Patching | Real Logic\n\n");
    s_logger.info("Type ");
}

std::vector<std::string> SplitArgs(const std::string& input) {
    std::vector<std::string> args;
    std::stringstream ss(input);
    std::string item;
    while (ss >> item) {
        if (!item.empty()) args.push_back(item);
    }
    return args;
}

// Demo security function for hotpatching
__declspec(noinline) bool SecurityCheck() {
    s_logger.info("[SECURITY] Verifying License Key...\n");
    return false;
}

static void SignalHandlerFunc(int signal) {
    s_logger.info("\n[ENGINE] Caught signal ");
    if (GlobalContext::Get().memory) GlobalContext::Get().memory->Wipe();
    exit(0);
}

int main(int argc, char** argv) {
    std::signal(SIGINT, SignalHandlerFunc);
    PrintBanner();

    // Parse arguments
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(std::string(argv[i]));
    }

    // Initialize core runtime
    init_runtime();

    s_logger.info("[SYSTEM] Universal Generator Service Ready.");

    // Initialize Global Context
    GlobalContext& ctx = GlobalContext::Get();

    // Initialize core systems
    VSIXLoader& loader = VSIXLoader::GetInstance();
    loader.Initialize("plugins");
    ctx.vsix_loader = &loader;
    
    ctx.memory = std::make_unique<MemoryCore>();
    ctx.patcher = std::make_unique<HotPatcher>();
    ctx.agent_engine = std::make_unique<AgenticEngine>();
    ctx.agent_engine->initialize();

    // Default memory allocation
    if (!ctx.memory->Allocate(ContextTier::TIER_32K)) {
        s_logger.error( COLOR_RED << "[FATAL] Failed to allocate default memory context\n" << COLOR_RESET;
        return 1;
    }

    // Register native commands
    loader.RegisterCommand("exit", []() { exit(0); });
    loader.RegisterCommand("cls", []() { system("cls"); });

    // Parse command-line arguments
    bool gui_mode = true;
    for (const auto& arg : args) {
         if (arg == "--cli") {
            gui_mode = false;
         }
    }

    if (gui_mode) {
         s_logger.info("[SYSTEM] Starting Win32 IDE Environment...\n");
         IDEWindow ide;
         if (ide.Initialize(GetModuleHandle(NULL))) {
             // Message Pump
             MSG msg;
             while (GetMessage(&msg, NULL, 0, 0)) {
                 TranslateMessage(&msg);
                 DispatchMessage(&msg);
             }
         } else {
             s_logger.error( "Failed to initialize IDE Window.\n";
             return 1;
         }
    } else {
        // CLI Mode: Initialize Shell Configuration
        ShellConfig shellConfig;
        shellConfig.cli_mode = true;
        shellConfig.enable_colors = true;
        shellConfig.enable_autocomplete = true;
        shellConfig.auto_save_history = true;
        
        // Instantiate shell
        g_shell = std::make_unique<InteractiveShell>(shellConfig);
        
        // Dependencies (for shell backward compatibility)
        auto agent = new AgenticEngine();
        auto memory_manager = new MemoryManager();

        // Start Shell
        g_shell->Start(agent, memory_manager, &loader, nullptr, 
            [](const std::string& s) { s_logger.info( s; },
            [](const std::string& s) { /* Input callback */ }
        );
        
        // Keep main thread alive until shell exits
        while (g_shell->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        delete agent;
        delete memory_manager;
    }

    s_logger.info("\n[ENGINE] Shutting down...\n");
    if (ctx.memory) ctx.memory->Wipe();
    return 0;
}
    std::string input;
    while (true) {
        float util = ctx.memory->GetUtilizationPercentage();
        s_logger.info( COLOR_GREEN << "Rawr[" << std::fixed << std::setprecision(1) << util << "%]> " << COLOR_RESET;
        
        if (!std::getline(std::cin, input)) break;
        if (input.empty()) continue;

        std::vector<std::string> args = SplitArgs(input);
        std::string cmd = args[0];

        // PLUGIN COMMANDS (!)
        if (cmd[0] == '!') {
            if (cmd == "!plugin") {
                if (args.size() < 2) {
                    s_logger.info( loader.GetCLIHelp() << "\n";
                    continue;
                }
                std::string sub = args[1];
                
                if (sub == "load" && args.size() > 2) {
                    if (loader.LoadPlugin(args[2])) 
                        s_logger.info( COLOR_GREEN << "[OK] Plugin loaded\n" << COLOR_RESET;
                    else 
                        s_logger.info( COLOR_RED << "[ERR] Load failed\n" << COLOR_RESET;
                }
                else if (sub == "list") {
                    auto plugins = loader.GetLoadedPlugins();
                    s_logger.info("Loaded (");
                    for(auto* p : plugins) {
                        s_logger.info("  ");
                    }
                }
                else if (sub == "help" && args.size() > 2) {
                    s_logger.info( loader.GetPluginHelp(args[2]) << "\n";
                }
            }
            else if (cmd == "!memory") {
                if (args.size() < 2) {
                    s_logger.info("Usage: !memory <load|status|unload> [size]\n");
                    continue;
                }
                std::string sub = args[1];
                
                if (sub == "load" && args.size() > 2) {
                    ContextTier tier = ContextTier::TIER_4K;
                    std::string sz = args[2];
                    if (sz == "32k") tier = ContextTier::TIER_32K;
                    else if (sz == "64k") tier = ContextTier::TIER_64K;
                    else if (sz == "128k") tier = ContextTier::TIER_128K;
                    else if (sz == "256k") tier = ContextTier::TIER_256K;
                    else if (sz == "512k") tier = ContextTier::TIER_512K;
                    else if (sz == "1m") tier = ContextTier::TIER_1M;
                    
                    if (ctx.memory->Reallocate(tier)) {
                        s_logger.info( COLOR_GREEN << "[OK] Memory: " << ctx.memory->GetTierName() 
                                  << " (" << ctx.memory->GetCapacity() << " tokens)\n" << COLOR_RESET;
                    }
                }
                else if (sub == "status") {
                    s_logger.info("Tier: ");
                    s_logger.info("Usage: ");
                }
                else if (sub == "unload") {
                    ctx.memory->Deallocate();
                    s_logger.info("[OK] Memory released\n");
                }
            }
            else if (cmd == "!patch") {
                if (args.size() < 2) {
                    ctx.patcher->ListPatches();
                    continue;
                }
                if (args[1] == "revert" && args.size() > 2) {
                    ctx.patcher->RevertPatch(args[2]);
                }
            }
        }
        // GENERATOR COMMANDS (+)
        else if (cmd[0] == '+') {
            if (cmd == "+generate") {
                if (args.size() < 3) {
                    s_logger.info("Usage: +generate <project_name> <language> [output_dir]\n");
                    s_logger.info("Example: +generate my_app cpp\n");
                    continue;
                }
                
                std::string project_name = args[1];
                std::string language_str = args[2];
                std::filesystem::path output_dir = (args.size() > 3) ? args[3] : std::filesystem::current_path();
                
                // Map string to LanguageType
                LanguageType lang = LanguageType::CPP; // default
                
                // Convert to uppercase for comparison
                std::transform(language_str.begin(), language_str.end(), language_str.begin(), ::toupper);
                
                if (language_str == "C") lang = LanguageType::C;
                else if (language_str == "CPP" || language_str == "C++") lang = LanguageType::CPP;
                else if (language_str == "RUST") lang = LanguageType::RUST;
                else if (language_str == "GO") lang = LanguageType::GO;
                else if (language_str == "PYTHON") lang = LanguageType::PYTHON;
                else if (language_str == "JAVASCRIPT" || language_str == "JS") lang = LanguageType::JAVASCRIPT;
                else if (language_str == "TYPESCRIPT" || language_str == "TS") lang = LanguageType::TYPESCRIPT;
                else if (language_str == "REACT") lang = LanguageType::REACT;
                else if (language_str == "SWIFT") lang = LanguageType::SWIFT;
                else if (language_str == "CSHARP" || language_str == "C#") lang = LanguageType::CSHARP;
                else if (language_str == "UNITY") lang = LanguageType::UNITY;
                else if (language_str == "HASKELL") lang = LanguageType::HASKELL;
                else if (language_str == "ARDUINO") lang = LanguageType::ARDUINO;
                else if (language_str == "ASSEMBLY" || language_str == "ASM") lang = LanguageType::X64;
                
                CoreGenerator& gen = CoreGenerator::GetInstance();
                bool success = gen.Generate(project_name, lang, output_dir);
                
                if (success) {
                    s_logger.info( COLOR_GREEN << "✓ Generated " << project_name << " in " << output_dir.string() << "\n" << COLOR_RESET;
                } else {
                    s_logger.info( COLOR_RED << "✗ Failed to generate project\n" << COLOR_RESET;
                }
            }
            else if (cmd == "+generate-from-template") {
                if (args.size() < 3) {
                    s_logger.info("Usage: +generate-from-template <template_name> <project_name> [output_dir]\n");
                    s_logger.info("Example: +generate-from-template web-app my_website\n");
                    continue;
                }
                
                std::string template_name = args[1];
                std::string project_name = args[2];
                std::filesystem::path output_dir = (args.size() > 3) ? args[3] : std::filesystem::current_path();
                
                CoreGenerator& gen = CoreGenerator::GetInstance();
                bool success = gen.GenerateFromTemplate(template_name, project_name, output_dir);
                
                if (success) {
                    s_logger.info( COLOR_GREEN << "✓ Generated " << project_name << " from template " << template_name << "\n" << COLOR_RESET;
                } else {
                    s_logger.info( COLOR_RED << "✗ Failed to generate from template\n" << COLOR_RESET;
                }
            }
            else if (cmd == "+list-languages") {
                CoreGenerator& gen = CoreGenerator::GetInstance();
                auto languages = gen.ListLanguages();
                s_logger.info("Supported languages (");
                for (const auto& lang : languages) {
                    s_logger.info("  • ");
                }
            }
            else if (cmd == "+list-templates") {
                CoreGenerator& gen = CoreGenerator::GetInstance();
                auto tmpl_list = gen.ListTemplates();
                s_logger.info("Available templates (");
                for (const auto& tmpl : tmpl_list) {
                    s_logger.info("  • ");
                }
            }
            else if (cmd == "+generate-with-tests") {
                if (args.size() < 3) {
                    s_logger.info("Usage: +generate-with-tests <project_name> <language> [output_dir]\n");
                    continue;
                }
                
                std::string project_name = args[1];
                std::string language_str = args[2];
                std::filesystem::path output_dir = (args.size() > 3) ? args[3] : std::filesystem::current_path();
                
                LanguageType lang = LanguageType::CPP;
                std::transform(language_str.begin(), language_str.end(), language_str.begin(), ::toupper);
                
                if (language_str == "C") lang = LanguageType::C;
                else if (language_str == "CPP" || language_str == "C++") lang = LanguageType::CPP;
                else if (language_str == "PYTHON") lang = LanguageType::PYTHON;
                else if (language_str == "RUST") lang = LanguageType::RUST;
                
                CoreGenerator& gen = CoreGenerator::GetInstance();
                bool success = gen.GenerateWithTests(project_name, lang, output_dir);
                
                if (success) {
                    s_logger.info( COLOR_GREEN << "✓ Generated " << project_name << " with tests\n" << COLOR_RESET;
                } else {
                    s_logger.info( COLOR_RED << "✗ Failed to generate project with tests\n" << COLOR_RESET;
                }
            }
            else if (cmd == "+generate-with-ci") {
                if (args.size() < 3) {
                    s_logger.info("Usage: +generate-with-ci <project_name> <language> [output_dir]\n");
                    continue;
                }
                
                std::string project_name = args[1];
                std::string language_str = args[2];
                std::filesystem::path output_dir = (args.size() > 3) ? args[3] : std::filesystem::current_path();
                
                LanguageType lang = LanguageType::CPP;
                std::transform(language_str.begin(), language_str.end(), language_str.begin(), ::toupper);
                
                if (language_str == "C") lang = LanguageType::C;
                else if (language_str == "CPP" || language_str == "C++") lang = LanguageType::CPP;
                else if (language_str == "PYTHON") lang = LanguageType::PYTHON;
                else if (language_str == "RUST") lang = LanguageType::RUST;
                
                CoreGenerator& gen = CoreGenerator::GetInstance();
                bool success = gen.GenerateWithCI(project_name, lang, output_dir);
                
                if (success) {
                    s_logger.info( COLOR_GREEN << "✓ Generated " << project_name << " with CI/CD\n" << COLOR_RESET;
                } else {
                    s_logger.info( COLOR_RED << "✗ Failed to generate project with CI\n" << COLOR_RESET;
                }
            }
            else if (cmd == "+generate-with-docker") {
                if (args.size() < 3) {
                    s_logger.info("Usage: +generate-with-docker <project_name> <language> [output_dir]\n");
                    continue;
                }
                
                std::string project_name = args[1];
                std::string language_str = args[2];
                std::filesystem::path output_dir = (args.size() > 3) ? args[3] : std::filesystem::current_path();
                
                LanguageType lang = LanguageType::CPP;
                std::transform(language_str.begin(), language_str.end(), language_str.begin(), ::toupper);
                
                if (language_str == "C") lang = LanguageType::C;
                else if (language_str == "CPP" || language_str == "C++") lang = LanguageType::CPP;
                else if (language_str == "PYTHON") lang = LanguageType::PYTHON;
                else if (language_str == "RUST") lang = LanguageType::RUST;
                else if (language_str == "JAVASCRIPT" || language_str == "JS") lang = LanguageType::JAVASCRIPT;
                
                CoreGenerator& gen = CoreGenerator::GetInstance();
                bool success = gen.GenerateWithDocker(project_name, lang, output_dir);
                
                if (success) {
                    s_logger.info( COLOR_GREEN << "✓ Generated " << project_name << " with Docker\n" << COLOR_RESET;
                } else {
                    s_logger.info( COLOR_RED << "✗ Failed to generate project with Docker\n" << COLOR_RESET;
                }
            }
            else {
                s_logger.info("Unknown generator command: ");
                s_logger.info("Try +list-languages or +list-templates\n");
            }
        }
        // AGENT COMMANDS (/)
        else if (cmd[0] == '/') {
            if (cmd == "/help") {
                s_logger.info( R"(
=== RAWER ENGINE COMMANDS ===

MEMORY:
  !memory load <4k|32k|64k|128k|256k|512k|1m>  Hot-swap context window
  !memory status                               Show utilization
  !memory unload                               Free RAM

PLUGINS:
  !plugin load <path>     Load VSIX module
  !plugin list            Show loaded modules
  !plugin help <id>       Module documentation

PATCHING:
  /hack                   Test security (demo)
  /hotpatch               Bypass security (demo)
  /unpatch                Restore original code
  !patch list             Show active patches

AGENT:
  /plan <task>            Create execution plan
  /clear                  Wipe context memory
  /exit                   Shutdown engine

Any other input is added to context memory.
)";
            }
            else if (cmd == "/hack") {
                if (SecurityCheck()) {
                    s_logger.info( COLOR_GREEN << "[ACCESS GRANTED] Premium Unlocked\n" << COLOR_RESET;
                } else {
                    s_logger.info( COLOR_RED << "[ACCESS DENIED] Invalid License\n" << COLOR_RESET;
                    s_logger.info("Tip: Try /hotpatch\n");
                }
            }
            else if (cmd == "/hotpatch") {
                void* addr = (void*)&SecurityCheck;
                std::vector<unsigned char> payload = {0xB0, 0x01, 0xC3}; // mov al,1; ret
                if (g_patcher->ApplyPatch("LicenseBypass", addr, payload)) {
                    s_logger.info( COLOR_CYAN << "[PATCH] Security neutralized\n" << COLOR_RESET;
                }
            }
            else if (cmd == "/unpatch") {
                g_patcher->RevertPatch("LicenseBypass");
            }
            else if (cmd == "/clear") {
                g_memory->Wipe();
                s_logger.info("[OK] Context wiped\n");
            }
            else if (cmd == "/exit") {
                break;
            }
            else if (cmd == "/plan" && args.size() > 1) {
                std::string task;
                for (size_t i = 1; i < args.size(); i++) task += args[i] + " ";
                s_logger.info("[PLAN] Analyzing: ");
                s_logger.info("[PLAN] Context available: ");
                // Plan generation would interface with inference here
            }
            else {
                if (!loader.ExecuteCommand(cmd)) {
                    s_logger.info("[AGENT] Unknown command: ");
                }
            }
        }
        // RAW INPUT -> CONTEXT
        else {
            g_memory->PushContext(input);
            // In full implementation, this would trigger inference
            if (g_memory->GetUtilizationPercentage() > 90.0f) {
                s_logger.info( COLOR_YELLOW << "[WARN] Context window critical\n" << COLOR_RESET;
            }
        }
    }

    s_logger.info("\n[ENGINE] Shutting down...\n");
    if (g_memory) g_memory->Wipe();
    return 0;
}
*/
