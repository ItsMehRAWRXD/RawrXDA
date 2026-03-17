#include "multi_engine_system.h"
#include "gui.h"
#include "api_server.h"
#include "telemetry.h"
#include "settings.h"
#include "overclock_governor.h"
#include "overclock_vendor.h"
#include "native_agent.hpp"
#include "cpu_inference_engine.h"
#include "win32app/VSIXInstaller.hpp"
#include "modules/native_memory.hpp" // Added
#include "modules/ExtensionLoader.hpp" // Added
#include "reverse_engineering/RawrCodex.hpp"
#include "reverse_engineering/RawrDumpBin.hpp"
#include "reverse_engineering/RawrCompiler.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <conio.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>
#include <vector>

namespace fs = std::filesystem;

// ===== Non-Qt analysis helpers (CLI parity with Qt Tools menu) =====
struct CliSecurityIssue {
    std::string severity;
    std::string type;
    int line;
    std::string description;
};

struct CliOptimization {
    std::string type;
    int line;
    std::string description;
};

// Lightweight security scan
static std::vector<CliSecurityIssue> scanCodeSecurity(const std::string& code) {
    std::vector<CliSecurityIssue> issues;
    std::istringstream iss(code);
    std::string line;
    int lineNum = 0;
    std::regex sqlRegex(R"(\bexec\b|\bSELECT\b.*\+|\bINSERT\b.*\+|sprintf\s*\()", std::regex::icase);
    std::regex cmdRegex(R"(\bsystem\s*\(|\bpopen\s*\(|eval\s*\()", std::regex::icase);
    std::regex bufferRegex(R"(\bgets\s*\(|\bstrcpy\s*\(|\bsprintf\s*\()", std::regex::icase);
    while (std::getline(iss, line)) {
        ++lineNum;
        if (std::regex_search(line, sqlRegex)) {
            issues.push_back({"high", "sql_injection_risk", lineNum, "Potential SQL injection or unsafe sprintf"});
        }
        if (std::regex_search(line, cmdRegex)) {
            issues.push_back({"critical", "command_injection", lineNum, "Possible command injection via system/popen/eval"});
        }
        if (std::regex_search(line, bufferRegex)) {
            issues.push_back({"high", "buffer_overflow", lineNum, "Unsafe buffer function (gets/strcpy/sprintf)"});
        }
    }
    return issues;
}

// Lightweight optimization hint scanner
static std::vector<CliOptimization> scanOptimizations(const std::string& code) {
    std::vector<CliOptimization> opts;
    std::istringstream iss(code);
    std::string line;
    int lineNum = 0;
    std::regex loopRegex(R"(for\s*\([^;]*;[^;]*\.size\(\))", std::regex::icase);
    std::regex allocRegex(R"(new\s+\w+\[|malloc\s*\()", std::regex::icase);
    while (std::getline(iss, line)) {
        ++lineNum;
        if (std::regex_search(line, loopRegex)) {
            opts.push_back({"loop", lineNum, "Consider caching container size outside loop"});
        }
        if (std::regex_search(line, allocRegex)) {
            opts.push_back({"memory", lineNum, "Consider using smart pointers or pre-allocation"});
        }
    }
    return opts;
}

// Simple test stub generator
static std::string generateTestStub(const std::string& funcName, const std::string& language) {
    if (language == "cpp" || language == "c++") {
        return "TEST(" + funcName + "Test, BasicCase) {\n"
               "    // Arrange\n"
               "    // Act\n"
               "    // Assert\n"
               "}\n";
    } else if (language == "python") {
        return "def test_" + funcName + "():\n"
               "    # Arrange\n"
               "    # Act\n"
               "    # Assert\n"
               "    pass\n";
    }
    return "// Test stub for " + funcName + "\n";
}

// Extract function names from code
static std::vector<std::string> extractFunctions(const std::string& code, const std::string& language) {
    std::vector<std::string> funcs;
    std::regex funcRegex;
    if (language == "cpp" || language == "c++") {
        funcRegex = std::regex(R"(\w+\s+(\w+)\s*\([^)]*\)\s*\{)");
    } else if (language == "python") {
        funcRegex = std::regex(R"(def\s+(\w+)\s*\()");
    } else {
        funcRegex = std::regex(R"(function\s+(\w+)\s*\()");
    }
    auto begin = std::sregex_iterator(code.begin(), code.end(), funcRegex);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        funcs.push_back((*it)[1].str());
    }
    return funcs;
}

// Read file helper
static std::string readFileContents(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Detect language from extension
static std::string detectLanguage(const std::string& path) {
    if (path.ends_with(".cpp") || path.ends_with(".h") || path.ends_with(".hpp") || path.ends_with(".c")) return "cpp";
    if (path.ends_with(".py")) return "python";
    if (path.ends_with(".js") || path.ends_with(".ts")) return "javascript";
    return "cpp";
}

using namespace std::chrono_literals;

int main(int argc, char** argv) {
    AppState state;
    std::cout << "========================================================\n";
    std::cout << "   RawrXD CLI - Autonomous Inference Stack v2.0\n";
    std::cout << "   [MAX MODE] [DEEP RESEARCH] [800B MULTI-DRIVE]\n";
    std::cout << "========================================================\n";
    std::cout << "Initialize: [ENGINE: Native] [AGENT: Active] [RE: Loaded]\n";
    std::cout << "Type '!help' for commands.\n\n";

    // Load settings
    Settings::LoadCompute(state);
    Settings::LoadOverclock(state);

    // Initialize telemetry
    telemetry::Initialize();

    // Start API server
    APIServer api(state);
    api.Start(11434);

    // Start governor if requested
    OverclockGovernor governor;
    std::atomic<bool> governor_running{false};
    if (state.enable_overclock_governor) {
        governor.Start(state);
        governor_running = true;
    }

    // Initialize Inference Engine (CPU by default, upgrades to Titan/Multi as needed)
    static CPUInference::CPUInferenceEngine engine;
    static RawrXD::NativeAgent agent(&engine);
    
    // Extensions and RE Tools
    static RawrXD::ExtensionLoader extLoader;
    extLoader.Scan();
    extLoader.LoadNativeModules();
    
    static RawrXD::ReverseEngineering::RawrCodex codex;
    static RawrXD::ReverseEngineering::RawrDumpBin dumpbin;
    static RawrXD::ReverseEngineering::RawrCompiler compiler;
    static RawrXD::MultiEngineSystem multiEngine;

    std::string lastFilePath;
    bool running = true;
    telemetry::TelemetrySnapshot snap{};

    // MAIN INTERACTIVE LOOP
    while (running) {
        std::cout << "RawrXD (" << (engine.IsModelLoaded() ? "Online" : "Idle") << ")> ";
        std::string line;
        
        // Handle background updates while waiting for input
        while (!_kbhit()) {
            telemetry::Poll(snap);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::getline(std::cin, line);
        if (line.empty()) continue;

        // Command Parser
        if (line[0] == '!' || line[0] == '/') {
            std::stringstream ss(line);
            std::string cmd;
            ss >> cmd;
            if (cmd[0] == '/') cmd[0] = '!';

            if (cmd == "!help") {
                std::cout << "\n--- AI & ENGINE COMMANDS ---\n";
                std::cout << "  !load <path>          : Load GGUF model into memory\n";
                std::cout << "  !engine 800b          : Load 800B distributed model (5ndrive setup)\n";
                std::cout << "  !engine status        : Display drive distribution & engine health\n";
                std::cout << "  !think <on/off>       : Toggle Deep Thinking (CoT Reasoning)\n";
                std::cout << "  !research <on/off>    : Toggle Deep Research (Workspace Scanning)\n";
                std::cout << "  !maxmode <on/off>     : Toggle Max Mode (Full CPU Core Scaling)\n";
                std::cout << "  !norefusal <on/off>   : Toggle Unrestricted/No-Safety Mode\n";
                std::cout << "  !context <4k-1M>      : Set AI context window limit\n";
                std::cout << "\n--- REVERSE ENGINEERING ---\n";
                std::cout << "  !dumpbin <file>       : Deep PE/COFF analysis & security audit\n";
                std::cout << "  !disasm <file>        : X64 disassembly of native binary\n";
                std::cout << "  !compile <file>       : Compile C/C++/ASM with optimizations\n";
                std::cout << "\n--- SYSTEM ---\n";
                std::cout << "  !status               : Show CPU/GPU metrics & OC state\n";
                std::cout << "  !plan <task>          : Use Agent to build complex tech plans\n";
                std::cout << "  !quit                 : Exit RawrXD CLI\n\n";
            }
            else if (cmd == "!load") {
                std::string path; std::getline(ss, path);
                if (!path.empty()) {
                    path.erase(0, path.find_first_not_of(" \t"));
                    if (engine.LoadModel(path)) std::cout << "[SUCCESS] Model ready.\n";
                    else std::cout << "[ERROR] Loading failed.\n";
                }
            }
            else if (cmd == "!engine") {
                std::string sub; ss >> sub;
                if (sub == "800b") {
                    std::cout << "[System] Initializing 5-Drive High-Speed Array...\n";
                    if (multiEngine.Load800BModel("e:\\models\\deepseek-800b")) {
                        std::cout << "[SUCCESS] 800B Distributed Model is NOW ONLINE.\n";
                        agent.SetContextLimit(1048576); // 1M Tokens for 800B
                    }
                } else if (sub == "status") {
                    auto drives = multiEngine.GetDriveInfo();
                    std::cout << "--- 5ndrive Array Status ---\n";
                    for(auto& d : drives) std::cout << " Drive: " << d.drivePath << " | Online: " << (d.isActive?"Yes":"No") << " | Free: " << (d.availableSpace/1024/1024/1024) << "GB\n";
                }
            }
            else if (cmd == "!think") {
                std::string val; ss >> val;
                bool on = (val == "on");
                agent.SetDeepThink(on);
                std::cout << "Deep Thinking: " << (on ? "ENABLED" : "DISABLED") << "\n";
            }
            else if (cmd == "!research") {
                std::string val; ss >> val;
                bool on = (val == "on");
                agent.SetDeepResearch(on);
                std::cout << "Deep Research: " << (on ? "ENGAGED" : "OFFLINE") << "\n";
            }
            else if (cmd == "!maxmode") {
                std::string val; ss >> val;
                bool on = (val == "on");
                agent.SetMaxMode(on);
                std::cout << "MAX MODE: " << (on ? "ENGAGED (Full Compute)" : "STANDARD") << "\n";
            }
            else if (cmd == "!norefusal") {
                std::string val; ss >> val;
                bool on = (val == "on");
                agent.SetNoRefusal(on);
                std::cout << "Unrestricted Mode: " << (on ? "ON" : "OFF") << "\n";
            }
            else if (cmd == "!context") {
                std::string arg; ss >> arg;
                size_t sz = 4096;
                if (arg == "32k") sz = 32768;
                else if (arg == "1m") sz = 1048576;
                else try { sz = std::stoull(arg); } catch(...) {}
                agent.SetContextLimit(sz);
                std::cout << "Context: " << sz << " tokens.\n";
            }
            else if (cmd == "!dumpbin") {
                std::string path; std::getline(ss, path);
                if(!path.empty()) {
                    path.erase(0, path.find_first_not_of(" \t"));
                    std::cout << dumpbin.DumpAll(path) << "\n";
                }
            }
            else if (cmd == "!disasm") {
                std::string path; std::getline(ss, path);
                if(!path.empty()) {
                    path.erase(0, path.find_first_not_of(" \t"));
                    std::cout << dumpbin.DumpDisassembly(path, 0x140000000, 100) << "\n";
                }
            }
            else if (cmd == "!compile") {
                std::string path; std::getline(ss, path);
                if(!path.empty()) {
                    path.erase(0, path.find_first_not_of(" \t"));
                    auto res = compiler.CompileSource(path);
                    if(res.success) std::cout << "[SUCCESS] Object created: " << res.objectFile << "\n";
                }
            }
            else if (cmd == "!status") {
                std::cout << "--- SYSTEM METRICS ---\n";
                std::cout << "CPU Temp: " << snap.cpuTempC << "C\n";
                std::cout << "GPU Temp: " << snap.gpuTempC << "C\n";
                std::cout << "Threads: " << (agent.IsMaxMode() ? "MAX" : "AUTO") << "\n";
            }
            else if (cmd == "!plan") {
                std::string task; std::getline(ss, task);
                agent.Plan(task);
            }
            else if (cmd == "!quit") {
                running = false;
            }
            else {
                std::cout << "Unknown command. Try !help.\n";
            }
        } else {
            // DIRECT AGENT CHAT
            agent.Ask(line);
        }
    }

    if (governor_running) governor.Stop();
    api.Stop();
    telemetry::Shutdown();
    std::cout << "Closing RawrXD CLI. Engines offline.\n";
    return 0;
}
