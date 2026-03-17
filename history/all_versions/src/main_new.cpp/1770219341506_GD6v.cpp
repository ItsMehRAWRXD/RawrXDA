#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <csignal>
#include <iomanip>
#include <thread>
#include <chrono>

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
#include "ide_window.h"

// ANSI Color Codes
#define COLOR_RESET   "\033[0m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
#define COLOR_MAGENTA "\033[35m"

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

__declspec(noinline) bool SecurityCheck() {
    std::cout << "[SECURITY] Verifying License Key...\n";
    return false;
}

static void SignalHandlerFunc(int signal) {
    std::cout << "\n[ENGINE] Caught signal " << signal << ". Flushing memory...\n";
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

    std::cout << "[SYSTEM] Universal Generator Service Ready." << std::endl;

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
        std::cerr << COLOR_RED << "[FATAL] Failed to allocate default memory context\n" << COLOR_RESET;
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
         std::cout << "[SYSTEM] Starting Win32 IDE Environment...\n";
         IDEWindow ide;
         if (ide.Initialize(GetModuleHandle(NULL))) {
             // Message Pump
             MSG msg;
             while (GetMessage(&msg, NULL, 0, 0)) {
                 TranslateMessage(&msg);
                 DispatchMessage(&msg);
             }
         } else {
             std::cerr << "Failed to initialize IDE Window.\n";
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
            [](const std::string& s) { std::cout << s; },
            [](const std::string& s) { /* Input callback */ }
        );
        
        // Keep main thread alive until shell exits
        while (g_shell->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        delete agent;
        delete memory_manager;
    }

    std::cout << "\n[ENGINE] Shutting down...\n";
    if (ctx.memory) ctx.memory->Wipe();
    return 0;
}
