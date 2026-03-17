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
    std::cout << "RawrXD CLI - Non-Qt IDE mode\n";
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

    // Initialize Inference Engine and Agent
    std::cout << "Initializing Native Inference Engine..." << std::endl;
    static CPUInference::CPUInferenceEngine engine;
    
    // Register Native Memory Plugin (Max Mode Support)
    auto memPlugin = std::make_shared<RawrXD::Modules::NativeMemoryModule>();
    engine.RegisterMemoryPlugin(memPlugin);
    
    static RawrXD::NativeAgent agent(&engine);
    static RawrXD::ExtensionLoader extLoader;
    extLoader.Scan();
    extLoader.LoadNativeModules();
    
    // Initialize Reverse Engineering Suite
    static RawrXD::ReverseEngineering::RawrCodex codex;
    static RawrXD::ReverseEngineering::RawrDumpBin dumpbin;
    static RawrXD::ReverseEngineering::RawrCompiler compiler;

    // Initialize Multi-Engine System for 800B Model Support
    static RawrXD::MultiEngineSystem multiEngine;
    
    std::cout << "Commands: h=help, p=status, g=toggle governor, a=apply profile, r=reset offsets, +=inc offset, -=dec offset, s=save settings" << std::endl;
    std::cout << "          x=analyze file, t=generate tests, y=security scan, o=optimize scan, q=quit" << std::endl;
    std::cout << "          /=enter command mode (load, agent, compile, disasm, dumpbin, etc.)" << std::endl;

    std::string lastFilePath;  // Track last used file for convenience
    bool running = true;
    telemetry::TelemetrySnapshot snap{};
    while (running) {
        if (_kbhit()) {
            int c = _getch();
            switch (c) {
            case '/': // Command Mode
            case 'c': {
                std::cout << "\nRawrXD> ";
                std::string cmd;
                std::getline(std::cin, cmd);
                
                std::stringstream ss(cmd);
                std::string action;
                ss >> action;
                
                if (action == "/load") {
                    std::string path;
                    std::getline(ss, path);
                    if (!path.empty()) path = path.substr(1); // remove leading space
                    if (engine.LoadModel(path)) {
                        std::cout << "Model loaded successfully: " << path << std::endl;
                    } else {
                        std::cout << "Failed to load model." << std::endl;
                    }
                }
                else if (action == "/agent") {
                    std::string query;
                    std::getline(ss, query);
                    if (!query.empty()) agent.Ask(query.substr(1));
                }
                else if (action == "/plan") {
                    std::string task;
                    std::getline(ss, task);
                    if (!task.empty()) agent.Plan(task.substr(1));
                }
                else if (action == "/bugreport") {
                    std::cout << "Target File: ";
                    std::string f;
                    std::getline(std::cin, f);
                    if (!f.empty()) agent.BugReport(f);
                }
                else if (action == "/edit") {
                     std::cout << "Target File: ";
                     std::string f;
                     std::getline(std::cin, f);
                     std::cout << "Instructions: ";
                     std::string i;
                     std::getline(std::cin, i);
                     if (!f.empty()) agent.Edit(f, i);
                }
                else if (action == "/suggest") {
                    std::cout << "Target File: ";
                    std::string f;
                    std::getline(std::cin, f);
                    agent.Suggest(f);
                }
                else if (action == "/edit") {
                    std::cout << "Target File: ";
                    std::string f;
                    std::getline(std::cin, f);
                    std::cout << "Instructions: ";
                    std::string i;
                    std::getline(std::cin, i);
                    agent.Edit(f, i);
                }
                else if (action == "/patch") {
                     std::cout << "Target File: ";
                     std::string f;
                     std::getline(std::cin, f);
                     agent.HotPatch(f);
                }
                else if (action == "/react_server") {
                    agent.CreateReactServerPlan();
                }
                else if (action == "/context") {
                    std::string arg;
                    std::getline(ss, arg);
                    
                    // Parse k/m suffixes
                    size_t size = 2048;
                    size_t multiplier = 1;
                    if (arg.find("k") != std::string::npos || arg.find("K") != std::string::npos) multiplier = 1024;
                    else if (arg.find("m") != std::string::npos || arg.find("M") != std::string::npos) multiplier = 1024 * 1024;
                    
                    // clean string
                    std::string numStr = "";
                    for(char c : arg) if(isdigit(c)) numStr += c;
                    
                    if(!numStr.empty()) {
                        size = std::stoi(numStr) * multiplier;
                        engine.SetContextLimit(size);
                        // Trigger realloc
                        engine.ClearCache(); 
                        std::cout << "Context set to " << size << " tokens (Cache cleared)" << std::endl;
                    }
                }
                else if (action == "/max") {
                    static bool maxMode = false;
                    maxMode = !maxMode;
                    engine.SetMaxMode(maxMode);
                    agent.SetMaxMode(maxMode);
                    std::cout << "Max Mode: " << (maxMode ? "ON" : "OFF") << std::endl;
                }
                else if (action == "/think") {
                    static bool think = false;
                    think = !think;
                    engine.SetDeepThinking(think);
                    agent.SetDeepThink(think);
                    std::cout << "Deep Thinking: " << (think ? "ON" : "OFF") << std::endl;
                }
                else if (action == "/research") {
                    static bool res = false;
                    res = !res;
                    engine.SetDeepResearch(res);
                    agent.SetDeepResearch(res);
                    std::cout << "Deep Research: " << (res ? "ON" : "OFF") << std::endl;
                }
                else if (action == "/norefusal") {
                    static bool nr = false;
                    nr = !nr;
                    agent.SetNoRefusal(nr);
                    std::cout << "No Refusal: " << (nr ? "ON" : "OFF") << std::endl;
                }
                else if (action == "/autocorrect") {
                    static bool ac = false;
                    ac = !ac;
                    agent.SetAutoCorrect(ac);
                    std::cout << "Auto-Correct (Hallucination Fix): " << (ac ? "ON" : "OFF") << std::endl;
                }
                else if (action == "/install") {
                    std::string path;
                    std::getline(ss, path);
                    if (!path.empty()) {
                        path = path.substr(1); // remove leading space
                        if (RawrXD::VSIXInstaller::Install(path)) {
                            extLoader.Scan(); // Rescan after install
                        }
                    }
                }
                else if (action == "!help" || action == "/exthelp") {
                     std::string name;
                     std::getline(ss, name);
                     if (!name.empty()) name = name.substr(1);
                     if (name.empty()) {
                         auto exts = extLoader.GetExtensions();
                         std::cout << "Installed Extensions:\n";
                         for(auto& e : exts) std::cout << " - " << e.name << (e.isNative ? " (Native)" : "") << "\n";
                     } else {
                         std::cout << extLoader.GetHelp(name) << std::endl;
                     }
                }
                else if (action == "/disasm") {
                    std::string path;
                    std::getline(ss, path);
                    if (!path.empty()) path = path.substr(1);
                    if (path.empty()) {
                        std::cout << "Usage: /disasm <binary> [address] [count]\n";
                    } else {
                        uint64_t addr = 0x1000;
                        size_t count = 50;
                        ss >> std::hex >> addr >> std::dec >> count;
                        
                        if (codex.LoadBinary(path)) {
                            std::cout << dumpbin.DumpDisassembly(path, addr, count);
                        } else {
                            std::cout << "Failed to load binary: " << path << "\n";
                        }
                    }
                }
                else if (action == "/dumpbin") {
                    std::string path, mode;
                    std::getline(ss, path);
                    if (!path.empty()) path = path.substr(1);
                    size_t spacePos = path.find(' ');
                    if (spacePos != std::string::npos) {
                        mode = path.substr(spacePos + 1);
                        path = path.substr(0, spacePos);
                    } else {
                        mode = "all";
                    }
                    
                    if (path.empty()) {
                        std::cout << "Usage: /dumpbin <binary> [headers|imports|exports|strings|vulns|all]\n";
                    } else {
                        if (mode == "headers") std::cout << dumpbin.DumpHeaders(path);
                        else if (mode == "imports") std::cout << dumpbin.DumpImports(path);
                        else if (mode == "exports") std::cout << dumpbin.DumpExports(path);
                        else if (mode == "strings") std::cout << dumpbin.DumpStrings(path);
                        else if (mode == "vulns") std::cout << dumpbin.DumpVulnerabilities(path);
                        else std::cout << dumpbin.DumpAll(path);
                    }
                }
                else if (action == "/compile") {
                    std::string srcPath;
                    std::getline(ss, srcPath);
                    if (!srcPath.empty()) srcPath = srcPath.substr(1);
                    
                    if (srcPath.empty()) {
                        std::cout << "Usage: /compile <source_file>\n";
                    } else {
                        RawrXD::ReverseEngineering::CompilerOptions opts;
                        opts.optimizationLevel = 2;
                        opts.targetArch = "x64";
                        compiler.SetOptions(opts);
                        
                        std::cout << "Compiling " << srcPath << "...\n";
                        auto result = compiler.CompileSource(srcPath);
                        
                        if (result.success) {
                            std::cout << "✓ Compilation successful\n";
                            std::cout << "  Object file: " << result.objectFile << "\n";
                            std::cout << "  Time: " << result.compileTimeMs << "ms\n";
                        } else {
                            std::cout << "✗ Compilation failed\n";
                            for (const auto& err : result.errors) {
                                std::cout << "  Error: " << err << "\n";
                            }
                        }
                        
                        if (!result.warnings.empty()) {
                            std::cout << "  Warnings:\n";
                            for (const auto& warn : result.warnings) {
                                std::cout << "    " << warn << "\n";
                            }
                        }
                    }
                }
                else if (action == "/analyze_binary") {
                    std::string path;
                    std::getline(ss, path);
                    if (!path.empty()) path = path.substr(1);
                    
                    if (path.empty()) {
                        std::cout << "Usage: /analyze_binary <binary>\n";
                    } else if (codex.LoadBinary(path)) {
                        std::cout << "\n=== Binary Analysis: " << path << " ===\n\n";
                        
                        auto sections = codex.GetSections();
                        std::cout << "Sections: " << sections.size() << "\n";
                        for (const auto& sec : sections) {
                            std::cout << "  " << sec.name << " (VA: 0x" << std::hex << sec.virtualAddress << ")\n";
                        }
                        
                        auto imports = codex.GetImports();
                        std::cout << "\nImports: " << std::dec << imports.size() << "\n";
                        
                        auto exports = codex.GetExports();
                        std::cout << "Exports: " << exports.size() << "\n";
                        
                        auto vulns = codex.DetectVulnerabilities();
                        std::cout << "\nVulnerabilities: " << vulns.size() << "\n";
                        for (const auto& vuln : vulns) {
                            std::cout << "  [" << vuln.severity << "] " << vuln.description << "\n";
                        }
                        
                        auto strings = codex.ExtractStrings(6);
                        std::cout << "\nStrings (>= 6 chars): " << strings.size() << "\n";
                    } else {
                        std::cout << "Failed to load binary\n";
                    }
                }
                else if (action == "/compare") {
                    std::string path1, path2;
                    ss >> path1 >> path2;
                    
                    if (path1.empty() || path2.empty()) {
                        std::cout << "Usage: /compare <binary1> <binary2>\n";
                    } else {
                        std::cout << dumpbin.CompareBinaries(path1, path2);
                    }
                }
                else if (action == "/multiengine") {
                    std::string subcmd;
                    std::getline(ss, subcmd);
                    if (!subcmd.empty()) subcmd = subcmd.substr(1);
                    
                    if (subcmd == "load800b") {
                        std::cout << "Enter base path for 800B model parts: ";
                        std::string path;
                        std::getline(std::cin, path);
                        if (multiEngine.Load800BModel(path)) {
                            std::cout << "800B model loaded successfully across 8 engines" << std::endl;
                        } else {
                            std::cout << "Failed to load 800B model" << std::endl;
                        }
                    }
                    else if (subcmd == "distribute") {
                        std::cout << "Enter model name to distribute: ";
                        std::string modelName;
                        std::getline(std::cin, modelName);
                        if (multiEngine.DistributeModel(modelName)) {
                            std::cout << "Model distributed across 5 drives" << std::endl;
                        }
                    }
                    else if (subcmd == "status") {
                        auto drives = multiEngine.GetDriveInfo();
                        std::cout << "=== Multi-Engine System Status ===" << std::endl;
                        for (size_t i = 0; i < drives.size(); ++i) {
                            std::cout << "Drive " << i << ": " << drives[i].drivePath << " (" << (drives[i].availableSpace / (1024*1024*1024)) << " GB)" << std::endl;
                        }
                    }
                    else {
                        std::cout << "Multi-engine commands: load800b, distribute, status" << std::endl;
                    }
                }
                break;
            }
            case 'h':
                std::cout << "h=help, p=status, g=toggle governor, a=apply profile, r=reset, +=inc, -=dec, s=save" << std::endl;
                std::cout << "x=analyze file, t=generate tests, y=security scan, o=optimize scan, q=quit" << std::endl;
                break;
            case 'p':
                telemetry::Poll(snap);
                std::cout << "CPU temp: " << (snap.cpuTempValid ? std::to_string(snap.cpuTempC) + "C" : "n/a") << "\n";
                std::cout << "GPU temp: " << (snap.gpuTempValid ? std::to_string(snap.gpuTempC) + "C" : "n/a") << "\n";
                std::cout << "Governor status: " << state.governor_status << " applied_offset: " << state.applied_core_offset_mhz << "\n";
                break;
            case 'g':
                if (governor_running) { governor.Stop(); governor_running = false; state.governor_status = "stopped"; std::cout << "Governor stopped\n"; }
                else { governor.Start(state); governor_running = true; state.governor_status = "running"; std::cout << "Governor started\n"; }
                break;
            case 'a':
                // Apply profile - set all core target if configured
                if (state.target_all_core_mhz > 0) {
                    overclock_vendor::ApplyCpuTargetAllCoreMhz(state.target_all_core_mhz);
                    std::cout << "Applied all-core target: " << state.target_all_core_mhz << " MHz" << std::endl;
                } else {
                    std::cout << "No all-core target configured" << std::endl;
                }
                break;
            case 'r':
                overclock_vendor::ApplyCpuOffsetMhz(0);
                overclock_vendor::ApplyGpuClockOffsetMhz(0);
                state.applied_core_offset_mhz = 0;
                state.applied_gpu_offset_mhz = 0;
                std::cout << "Offsets reset" << std::endl;
                break;
            case '+':
            case '=':
                state.applied_core_offset_mhz += state.boost_step_mhz;
                overclock_vendor::ApplyCpuOffsetMhz(state.applied_core_offset_mhz);
                std::cout << "Increased offset to " << state.applied_core_offset_mhz << " MHz" << std::endl;
                break;
            case '-':
                state.applied_core_offset_mhz = std::max(0, state.applied_core_offset_mhz - (int)state.boost_step_mhz);
                overclock_vendor::ApplyCpuOffsetMhz(state.applied_core_offset_mhz);
                std::cout << "Decreased offset to " << state.applied_core_offset_mhz << " MHz" << std::endl;
                break;
            case 's':
                Settings::SaveCompute(state);
                Settings::SaveOverclock(state);
                std::cout << "Settings saved" << std::endl;
                break;
            case 'x': { // Analyze file
                std::cout << "Enter file path to analyze: ";
                std::string filePath;
                std::getline(std::cin, filePath);
                if (filePath.empty() && !lastFilePath.empty()) filePath = lastFilePath;
                if (filePath.empty()) { std::cout << "No file specified" << std::endl; break; }
                lastFilePath = filePath;
                std::string code = readFileContents(filePath);
                if (code.empty()) { std::cout << "Could not read file: " << filePath << std::endl; break; }
                std::string lang = detectLanguage(filePath);
                auto funcs = extractFunctions(code, lang);
                auto secIssues = scanCodeSecurity(code);
                auto opts = scanOptimizations(code);
                std::cout << "\n=== Analysis for " << filePath << " ===" << std::endl;
                std::cout << "Language: " << lang << std::endl;
                std::cout << "Functions found: " << funcs.size() << std::endl;
                for (const auto& fn : funcs) std::cout << "  - " << fn << std::endl;
                std::cout << "Security issues: " << secIssues.size() << std::endl;
                std::cout << "Optimization hints: " << opts.size() << std::endl;
                break;
            }
            case 't': { // Generate tests
                std::cout << "Enter file path for test generation: ";
                std::string filePath;
                std::getline(std::cin, filePath);
                if (filePath.empty() && !lastFilePath.empty()) filePath = lastFilePath;
                if (filePath.empty()) { std::cout << "No file specified" << std::endl; break; }
                lastFilePath = filePath;
                std::string code = readFileContents(filePath);
                if (code.empty()) { std::cout << "Could not read file" << std::endl; break; }
                std::string lang = detectLanguage(filePath);
                auto funcs = extractFunctions(code, lang);
                std::cout << "\n=== Generated Test Stubs ===" << std::endl;
                for (const auto& fn : funcs) {
                    std::cout << generateTestStub(fn, lang) << std::endl;
                }
                break;
            }
            case 'y': { // Security scan
                std::cout << "Enter file path for security scan: ";
                std::string filePath;
                std::getline(std::cin, filePath);
                if (filePath.empty() && !lastFilePath.empty()) filePath = lastFilePath;
                if (filePath.empty()) { std::cout << "No file specified" << std::endl; break; }
                lastFilePath = filePath;
                std::string code = readFileContents(filePath);
                if (code.empty()) { std::cout << "Could not read file" << std::endl; break; }
                auto issues = scanCodeSecurity(code);
                std::cout << "\n=== Security Scan Results ===" << std::endl;
                if (issues.empty()) {
                    std::cout << "No security issues detected." << std::endl;
                } else {
                    for (const auto& iss : issues) {
                        std::cout << "[" << iss.severity << "] Line " << iss.line << ": " << iss.type << " - " << iss.description << std::endl;
                    }
                }
                break;
            }
            case 'o': { // Optimize scan
                std::cout << "Enter file path for optimization scan: ";
                std::string filePath;
                std::getline(std::cin, filePath);
                if (filePath.empty() && !lastFilePath.empty()) filePath = lastFilePath;
                if (filePath.empty()) { std::cout << "No file specified" << std::endl; break; }
                lastFilePath = filePath;
                std::string code = readFileContents(filePath);
                if (code.empty()) { std::cout << "Could not read file" << std::endl; break; }
                auto opts = scanOptimizations(code);
                std::cout << "\n=== Optimization Hints ===" << std::endl;
                if (opts.empty()) {
                    std::cout << "No obvious optimizations detected." << std::endl;
                } else {
                    for (const auto& opt : opts) {
                        std::cout << "[" << opt.type << "] Line " << opt.line << ": " << opt.description << std::endl;
                    }
                }
                break;
            }
            // [FEATURE] Interactive Agent Mode
            case 'i': { 
                std::cout << "\n=== Interactive Agent Mode (Full Agentic) ===\n";
                
                // Initialize Native Inference Stack
                static std::unique_ptr<RawrXD::CPUInference::CPUInferenceEngine> s_inferenceEngine;
                static std::unique_ptr<AgenticEngine> s_agentEngine;
                
                if (!s_inferenceEngine) {
                     std::cout << "Initializing CPU Inference Engine...\n";
                     s_inferenceEngine = std::make_unique<RawrXD::CPUInference::CPUInferenceEngine>();
                     s_agentEngine = std::make_unique<AgenticEngine>();
                     s_agentEngine->setInferenceEngine(s_inferenceEngine.get());
                     std::cout << "Engine Ready.\n";
                }
                
                std::cout << "Commands:\n";
                std::cout << "  /load <path>   Load GGUF model\n";
                std::cout << "  /max           Toggle Max Context Mode\n";
                std::cout << "  /think         Toggle Deep Thinking\n";
                std::cout << "  /research      Toggle Deep Research\n";
                std::cout << "  /norefusal     Toggle No Refusal\n";
                std::cout << "  /context <sz>  Set Context (4k, 32k, 1m)\n";
                std::cout << "  /react-server  Generate React Server\n";
                std::cout << "  /vsix <path>   Install VSIX Native\n";
                std::cout << "  exit           Return to menu\n";

                std::string input;
                bool agentLoop = true;
                
                // State
                AgenticEngine::GenerationConfig pConfig;

                while(agentLoop) {
                    std::cout << "\nRawrXD (" << (s_inferenceEngine->IsModelLoaded() ? "Ready" : "No Model") << ")> ";
                    std::getline(std::cin, input);
                    if (input == "exit" || input == "quit") { agentLoop = false; break; }
                    if (input.empty()) continue;
                    
                    // Command Handling
                    if (input.rfind("/load ", 0) == 0) {
                        std::string path = input.substr(6);
                        if(s_inferenceEngine->LoadModel(path)) {
                            std::cout << "Model loaded successfully: " << path << "\n";
                        } else {
                            std::cout << "Failed to load model.\n";
                        }
                        continue;
                    }
                    else if (input == "/max") {
                        pConfig.maxMode = !pConfig.maxMode;
                        std::cout << "Max Mode: " << (pConfig.maxMode ? "ON" : "OFF") << "\n";
                        s_agentEngine->updateConfig(pConfig);
                        continue;
                    }
                    else if (input == "/think") {
                        pConfig.deepThinking = !pConfig.deepThinking;
                        std::cout << "Deep Thinking: " << (pConfig.deepThinking ? "ON" : "OFF") << "\n";
                        s_agentEngine->updateConfig(pConfig);
                        continue;
                    }
                    else if (input == "/research") {
                        pConfig.deepResearch = !pConfig.deepResearch;
                        std::cout << "Deep Research: " << (pConfig.deepResearch ? "ON" : "OFF") << "\n";
                        s_agentEngine->updateConfig(pConfig);
                        continue;
                    }
                    else if (input == "/norefusal") {
                        pConfig.noRefusal = !pConfig.noRefusal;
                        std::cout << "No Refusal: " << (pConfig.noRefusal ? "ON" : "OFF") << "\n";
                        s_agentEngine->updateConfig(pConfig);
                        continue;
                    }
                    else if (input.rfind("/context ", 0) == 0) {
                        std::string arg = input.substr(9);
                        if (arg == "32k") s_inferenceEngine->SetContextLimit(32768);
                        else if (arg == "1m") s_inferenceEngine->SetContextLimit(1048576);
                        else try { s_inferenceEngine->SetContextLimit(std::stoi(arg)); } catch(...) {}
                        std::cout << "Context limit set to " << s_inferenceEngine->GetContextLimit() << "\n";
                        continue;
                    }
                    else if (input.rfind("/vsix ", 0) == 0) {
                        std::string path = input.substr(6);
                        RawrXD::VsixNativeConverter::ConvertVsixToNative(path, "extensions/");
                        continue;
                    }
                    else if (input == "/react-server") {
                        s_agentEngine->planTask("Generate comprehensive React Server setup"); // Trigger generic agent flow
                        continue;
                    }
                    
                    // Chat / Agent
                    if (!s_inferenceEngine->IsModelLoaded()) {
                        std::cout << "Please /load a model first.\n";
                        continue;
                    }

                    // For standard chat vs agent commands
                    std::string response = s_agentEngine->chat(input);
                    // Output is handled via callback in AgenticEngine::chat usually, 
                    // but our implementation returns string too.
                    // The NativeAgent prints to callback or stdout.
                    // Let's print final result here just in case NativeAgent didn't stream to stdout directly (it defaults to cout if no callback).
                    // In agentic_engine.cpp I wired it to capture into 'response' string.
                    std::cout << "\n[Response]: " << response << "\n";
                }
                break;
            }
            case 'q':
                running = false; break;
            default:
                std::cout << "Unknown command: " << (char)c << std::endl;
            }
        }

        // Poll telemetry and update state
        telemetry::Poll(snap);
        if (snap.cpuTempValid) state.current_cpu_temp_c = (uint32_t)std::lround(snap.cpuTempC);
        if (snap.gpuTempValid) state.current_gpu_hotspot_c = (uint32_t)std::lround(snap.gpuTempC);
        std::this_thread::sleep_for(200ms);
    }

    if (governor_running) governor.Stop();
    api.Stop();
    telemetry::Shutdown();
    std::cout << "Exiting RawrXD CLI" << std::endl;
    return 0;
}
