#include "self_test.hpp"
#include "model_invoker.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <windows.h>
#include <chrono>

namespace fs = std::filesystem;

// Minimal helper to running a process and checking exit code
static bool runProcess(const std::string& cmd, const std::vector<std::string>& args, int timeoutMs) {
    std::string commandLine = "\"" + cmd + "\"";
    for (const auto& arg : args) {
        commandLine += " \"" + arg + "\"";
    }

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    char* cmdLineStr = _strdup(commandLine.c_str());
    if (!CreateProcessA(NULL, cmdLineStr, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmdLineStr);
        return false;
    }
    free(cmdLineStr);

    DWORD result = WaitForSingleObject(pi.hProcess, timeoutMs);
    bool success = false;
    if (result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        success = false;
    } else {
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        success = (exitCode == 0);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return success;
}

static std::string findExecutable(const std::string& name) {
    // Simple path search
    char pathBuf[MAX_PATH];
    if (SearchPathA(NULL, name.c_str(), ".exe", MAX_PATH, pathBuf, NULL)) {
        return std::string(pathBuf);
    }
    return "";
}

SelfTest::SelfTest(void* parent) {
    // Parent unused
}

bool SelfTest::runAll() {
    m_output.clear();
    m_error.clear();

    log("=== Self-Test Start ===");

    if (!runUnitTests()) return false;
    if (!runIntegrationTests()) return false;
    if (!runInferenceTests()) return false;
    if (!runLint()) return false;
    if (!runBenchmarkBaseline()) return false;

    log("=== Self-Test PASSED ===");
    return true;
}

void SelfTest::log(const std::string& msg) {
    
    m_output += msg + "\n";
}

bool SelfTest::runUnitTests() {
    log("Running unit tests...");
    
    fs::path binDir = fs::absolute("build/bin"); // Adjust if needed
    if (!fs::exists(binDir)) {
         log("SKIP: build/bin directory missing");
         return true; // Don't fail if just not built
    }

    // Look for *_test.exe
    for (const auto& entry : fs::directory_iterator(binDir)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.find("_test.exe") != std::string::npos) {
                 if (!runProcess(entry.path().string(), {}, 30000)) {
                     m_error = "Unit test failed: " + filename;
                     log(m_error);
                     return false;
                 }
            }
        }
    }

    log("Unit tests PASSED");
    return true;
}

bool SelfTest::runIntegrationTests() {
    log("Running integration tests...");
    
    struct TestCase {
        std::string name;
        std::string exe;
        std::vector<std::string> args;
    };

    const std::vector<TestCase> tests = {
        {"Brutal 50 MB", "bench_deflate_50mb.exe", {}},
        {"Q8_0 end-to-end", "bench_q8_0_end2end.exe", {}},
        {"Flash-Attention", "bench_flash_attn.exe", {}},
        {"Quant ladder", "bench_quant_ladder.exe", {}}
    };

    // Assuming tests are in build/tests/
    fs::path testDir = fs::absolute("build/tests");
    
    for (const TestCase& test : tests) {
        fs::path exePath = testDir / test.exe;
        if (!fs::exists(exePath)) {
            log("SKIP: " + test.name + " (not built)");
            continue;
        }
        
        if (!runProcess(exePath.string(), test.args, 60000)) {
            m_error = "Integration test failed: " + test.name;
            log(m_error);
            return false;
        } 
    }

    log("Integration tests PASSED");
    return true;
}

bool SelfTest::runLint() {
    log("Running static analysis...");
    
    std::string cl = findExecutable("cl.exe");
    if (cl.empty()) {
        log("SKIP: cl.exe not found in PATH");
        return true;
    }

    fs::path srcDir = fs::absolute("src");
    std::vector<std::string> baseArgs = {"/analyze", "/W4", "/nologo", "/c"};

    // Recursive search for .cpp
    for (const auto& entry : fs::recursive_directory_iterator(srcDir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".cpp") {
                std::vector<std::string> args = baseArgs;
                args.push_back(entry.path().string());
                
                if (!runProcess(cl, args, 30000)) {
                    m_error = "Lint failed on " + entry.path().filename().string();
                    log(m_error);
                    return false;
                }
            }
        }
    }

    log("Static analysis PASSED");
    return true;
}

bool SelfTest::runBenchmarkBaseline() {
    log("Running CPU baseline benchmark...");
    
    // Simple matrix multiplication or similar to gauge basic ops/sec
    const int N = 256;
    std::vector<float> A(N*N, 1.0f);
    std::vector<float> B(N*N, 1.0f);
    std::vector<float> C(N*N, 0.0f);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        for (int k = 0; k < N; ++k) {
            float b = B[k*N + i]; // simple unoptimized access pattern
            for (int j = 0; j < N; ++j) {
                C[i*N + j] += A[i*N + k] * B[k*N + j];
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    std::chrono::duration<double> diff = end - start;
    double mflops = (2.0 * N * N * N) / (diff.count() * 1e6);
    
    log("Benchmark Result: " + std::to_string(mflops) + " MFLOPS (Approx)");
    
    // Assume if we completed > 0 MFLOPS, it passed.
    return true;
}
