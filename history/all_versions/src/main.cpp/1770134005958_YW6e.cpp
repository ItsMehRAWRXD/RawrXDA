#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <csignal>
#include <iomanip>

#include "vsix_loader.h"
#include "memory_core.h"
#include "hot_patcher.h"
#include "engine/rawr_engine.h"
#include "engine/core_generator.h"

// ANSI Color Codes
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <csignal>
#include <iomanip>
#include <thread>

#include "vsix_loader.h"
#include "memory_core.h"
#include "hot_patcher.h"
#include "engine/rawr_engine.h"
#include "runtime_core.h"
#include "react_server.h"

// ANSI Color Codes
#define COLOR_RESET   "\033[0m"

#define COLOR_CYAN    "\033[36m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
#define COLOR_MAGENTA "\033[35m"

// Global instances
std::unique_ptr<MemoryCore> g_memory;
std::unique_ptr<HotPatcher> g_patcher;
std::unique_ptr<RawrEngine> g_engine;

void PrintBanner() {
    std::cout << COLOR_MAGENTA << R"(
    ____  ___ _       _____  ____     _  ______ 
   / __ \/   | |     / / __ \/ __ \   | |/ /__ \
  / /_/ / /| | | /| / / /_/ / /_/ /   |   /__/ /
 / _, _/ ___ | |/ |/ / _, _/ _, _/   /   |  / / 
/_/ |_/_/  |_|__/|__/_/ |_/_/ |_|   /_/|_| /_/  
    )" << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "    REV 7.0 - ULTIMATE FINAL IMPLEMENTATION\n" << COLOR_RESET;
    std::cout << "    Zero Simulation | Native Memory | Live Patching | Real Logic\n\n";
    std::cout << "Type " << COLOR_YELLOW << "/help" << COLOR_RESET << " for commands\n" << std::endl;
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
    std::cout << "[SECURITY] Verifying License Key...\n";
    return false;
}

void SignalHandler(int signal) {
    std::cout << "\n[ENGINE] Caught signal " << signal << ". Flushing memory...\n";
    if (g_memory) g_memory->Wipe();
    exit(0);
}

int main() {
    std::signal(SIGINT, SignalHandler);
    PrintBanner();

    // Initialize core runtime (Registers AVX-512 Engine)
    init_runtime();

    // Start React Server in background thread (as the "Generator")
    std::cout << "[SYSTEM] Starting React Generator Server on port 8080..." << std::endl;
    std::thread server_thread([](){ start_server(8080); });
    server_thread.detach();

    // Initialize core systems for CLI fallback
    VSIXLoader& loader = VSIXLoader::GetInstance();
    loader.Initialize("plugins");
    
    g_memory = std::make_unique<MemoryCore>();
    g_patcher = std::make_unique<HotPatcher>();
    // Engine is managed by runtime_core now, but we keep this for legacy CLI mapping if needed
    // g_engine = std::make_unique<RawrEngine>(); 

    // Default memory allocation
    if (!g_memory->Allocate(ContextTier::TIER_32K)) {
        std::cerr << COLOR_RED << "[FATAL] Failed to allocate default memory context\n" << COLOR_RESET;
        return 1;
    }

    // Register native commands
    loader.RegisterCommand("exit", []() { exit(0); });
    loader.RegisterCommand("cls", []() { system("cls"); });

    std::string input;
    while (true) {
        float util = g_memory->GetUtilizationPercentage();
        std::cout << COLOR_GREEN << "Rawr[" << std::fixed << std::setprecision(1) << util << "%]> " << COLOR_RESET;
        
        if (!std::getline(std::cin, input)) break;
        if (input.empty()) continue;

        std::vector<std::string> args = SplitArgs(input);
        std::string cmd = args[0];

        // PLUGIN COMMANDS (!)
        if (cmd[0] == '!') {
            if (cmd == "!plugin") {
                if (args.size() < 2) {
                    std::cout << loader.GetCLIHelp() << "\n";
                    continue;
                }
                std::string sub = args[1];
                
                if (sub == "load" && args.size() > 2) {
                    if (loader.LoadPlugin(args[2])) 
                        std::cout << COLOR_GREEN << "[OK] Plugin loaded\n" << COLOR_RESET;
                    else 
                        std::cout << COLOR_RED << "[ERR] Load failed\n" << COLOR_RESET;
                }
                else if (sub == "list") {
                    auto plugins = loader.GetLoadedPlugins();
                    std::cout << "Loaded (" << plugins.size() << "):\n";
                    for(auto* p : plugins) {
                        std::cout << "  " << (p->enabled ? "[*]" : "[ ]") 
                                  << " " << p->id << " v" << p->version << "\n";
                    }
                }
                else if (sub == "help" && args.size() > 2) {
                    std::cout << loader.GetPluginHelp(args[2]) << "\n";
                }
            }
            else if (cmd == "!memory") {
                if (args.size() < 2) {
                    std::cout << "Usage: !memory <load|status|unload> [size]\n";
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
                    
                    if (g_memory->Reallocate(tier)) {
                        std::cout << COLOR_GREEN << "[OK] Memory: " << g_memory->GetTierName() 
                                  << " (" << g_memory->GetCapacity() << " tokens)\n" << COLOR_RESET;
                    }
                }
                else if (sub == "status") {
                    std::cout << "Tier: " << g_memory->GetTierName() << "\n";
                    std::cout << "Usage: " << g_memory->GetUsage() << "/" << g_memory->GetCapacity() 
                              << " (" << g_memory->GetUtilizationPercentage() << "%)\n";
                }
                else if (sub == "unload") {
                    g_memory->Deallocate();
                    std::cout << "[OK] Memory released\n";
                }
            }
            else if (cmd == "!patch") {
                if (args.size() < 2) {
                    g_patcher->ListPatches();
                    continue;
                }
                if (args[1] == "revert" && args.size() > 2) {
                    g_patcher->RevertPatch(args[2]);
                }
            }
        }
        // GENERATOR COMMANDS (+)
        else if (cmd[0] == '+') {
            if (cmd == "+generate") {
                if (args.size() < 3) {
                    std::cout << "Usage: +generate <project_name> <language> [output_dir]\n";
                    std::cout << "Example: +generate my_app cpp\n";
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
                    std::cout << COLOR_GREEN << "✓ Generated " << project_name << " in " << output_dir.string() << "\n" << COLOR_RESET;
                } else {
                    std::cout << COLOR_RED << "✗ Failed to generate project\n" << COLOR_RESET;
                }
            }
            else if (cmd == "+generate-from-template") {
                if (args.size() < 3) {
                    std::cout << "Usage: +generate-from-template <template_name> <project_name> [output_dir]\n";
                    std::cout << "Example: +generate-from-template web-app my_website\n";
                    continue;
                }
                
                std::string template_name = args[1];
                std::string project_name = args[2];
                std::filesystem::path output_dir = (args.size() > 3) ? args[3] : std::filesystem::current_path();
                
                CoreGenerator& gen = CoreGenerator::GetInstance();
                bool success = gen.GenerateFromTemplate(template_name, project_name, output_dir);
                
                if (success) {
                    std::cout << COLOR_GREEN << "✓ Generated " << project_name << " from template " << template_name << "\n" << COLOR_RESET;
                } else {
                    std::cout << COLOR_RED << "✗ Failed to generate from template\n" << COLOR_RESET;
                }
            }
            else if (cmd == "+list-languages") {
                CoreGenerator& gen = CoreGenerator::GetInstance();
                auto languages = gen.ListLanguages();
                std::cout << "Supported languages (" << languages.size() << "):\n";
                for (const auto& lang : languages) {
                    std::cout << "  • " << lang << "\n";
                }
            }
            else if (cmd == "+list-templates") {
                CoreGenerator& gen = CoreGenerator::GetInstance();
                auto tmpl_list = gen.ListTemplates();
                std::cout << "Available templates (" << tmpl_list.size() << "):\n";
                for (const auto& tmpl : tmpl_list) {
                    std::cout << "  • " << tmpl << "\n";
                }
            }
            else if (cmd == "+generate-with-tests") {
                if (args.size() < 3) {
                    std::cout << "Usage: +generate-with-tests <project_name> <language> [output_dir]\n";
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
                    std::cout << COLOR_GREEN << "✓ Generated " << project_name << " with tests\n" << COLOR_RESET;
                } else {
                    std::cout << COLOR_RED << "✗ Failed to generate project with tests\n" << COLOR_RESET;
                }
            }
            else if (cmd == "+generate-with-ci") {
                if (args.size() < 3) {
                    std::cout << "Usage: +generate-with-ci <project_name> <language> [output_dir]\n";
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
                    std::cout << COLOR_GREEN << "✓ Generated " << project_name << " with CI/CD\n" << COLOR_RESET;
                } else {
                    std::cout << COLOR_RED << "✗ Failed to generate project with CI\n" << COLOR_RESET;
                }
            }
            else if (cmd == "+generate-with-docker") {
                if (args.size() < 3) {
                    std::cout << "Usage: +generate-with-docker <project_name> <language> [output_dir]\n";
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
                    std::cout << COLOR_GREEN << "✓ Generated " << project_name << " with Docker\n" << COLOR_RESET;
                } else {
                    std::cout << COLOR_RED << "✗ Failed to generate project with Docker\n" << COLOR_RESET;
                }
            }
            else {
                std::cout << "Unknown generator command: " << cmd << "\n";
                std::cout << "Try +list-languages or +list-templates\n";
            }
        }
        // AGENT COMMANDS (/)
        else if (cmd[0] == '/') {
            if (cmd == "/help") {
                std::cout << R"(
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
                    std::cout << COLOR_GREEN << "[ACCESS GRANTED] Premium Unlocked\n" << COLOR_RESET;
                } else {
                    std::cout << COLOR_RED << "[ACCESS DENIED] Invalid License\n" << COLOR_RESET;
                    std::cout << "Tip: Try /hotpatch\n";
                }
            }
            else if (cmd == "/hotpatch") {
                void* addr = (void*)&SecurityCheck;
                std::vector<unsigned char> payload = {0xB0, 0x01, 0xC3}; // mov al,1; ret
                if (g_patcher->ApplyPatch("LicenseBypass", addr, payload)) {
                    std::cout << COLOR_CYAN << "[PATCH] Security neutralized\n" << COLOR_RESET;
                }
            }
            else if (cmd == "/unpatch") {
                g_patcher->RevertPatch("LicenseBypass");
            }
            else if (cmd == "/clear") {
                g_memory->Wipe();
                std::cout << "[OK] Context wiped\n";
            }
            else if (cmd == "/exit") {
                break;
            }
            else if (cmd == "/plan" && args.size() > 1) {
                std::string task;
                for (size_t i = 1; i < args.size(); i++) task += args[i] + " ";
                std::cout << "[PLAN] Analyzing: " << task << "\n";
                std::cout << "[PLAN] Context available: " << g_memory->GetCapacity() << " tokens\n";
                // Plan generation would interface with inference here
            }
            else {
                if (!loader.ExecuteCommand(cmd)) {
                    std::cout << "[AGENT] Unknown command: " << cmd << "\n";
                }
            }
        }
        // RAW INPUT -> CONTEXT
        else {
            g_memory->PushContext(input);
            // In full implementation, this would trigger inference
            if (g_memory->GetUtilizationPercentage() > 90.0f) {
                std::cout << COLOR_YELLOW << "[WARN] Context window critical\n" << COLOR_RESET;
            }
        }
    }

    std::cout << "\n[ENGINE] Shutting down...\n";
    if (g_memory) g_memory->Wipe();
    return 0;
}
