#include "gui.h"
#include "api_server.h"
#include "telemetry.h"
#include "settings.h"
#include "overclock_governor.h"
#include "overclock_vendor.h"
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
    // Install centralized exception handler first
    RawrXD::CentralizedExceptionHandler::instance().installHandler();
    RawrXD::CentralizedExceptionHandler::instance().enableAutomaticRecovery(true);
    
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
    
    // Log startup
    RawrXD::CentralizedExceptionHandler::instance().reportError("CLI started", "main", {});

    // Start governor if requested
    OverclockGovernor governor;
    std::atomic<bool> governor_running{false};
    if (state.enable_overclock_governor) {
        governor.Start(state);
        governor_running = true;
    }

    std::cout << "Commands: h=help, p=status, g=toggle governor, a=apply profile, r=reset offsets, +=inc offset, -=dec offset, s=save settings" << std::endl;
    std::cout << "          x=analyze file, t=generate tests, y=security scan, o=optimize scan, q=quit" << std::endl;

    std::string lastFilePath;  // Track last used file for convenience
    bool running = true;
    telemetry::TelemetrySnapshot snap{};
    while (running) {
        if (_kbhit()) {
            int c = _getch();
            switch (c) {
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
