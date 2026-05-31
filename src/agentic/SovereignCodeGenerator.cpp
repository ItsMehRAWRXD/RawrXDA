// ============================================================================
// SovereignCodeGenerator.cpp — Standalone CLI for Sovereign Code Generation
// Generate-Validate-Fix loop with compiler feedback
// Usage: SovereignCodeGenerator.exe "<prompt>" [output_file]
// ============================================================================
#include "SovereignInferenceClient.h"
#include "../agent/agentic_deep_thinking_engine.hpp"
#include "../agent/agentic_failure_detector.hpp"
#include "../agent/agentic_puppeteer.hpp"
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// Sovereign getThinkingLLM() — replaces socket-based version
// ---------------------------------------------------------------------------
static RawrXD::Agent::SovereignInferenceClient& getThinkingLLM() {
    static RawrXD::Agent::SovereignInferenceClient client;
    static bool initialized = false;
    if (!initialized) {
        RawrXD::Agent::SovereignModelConfig cfg;
        cfg.model_path = "models/phi3-mini-q4.gguf";
        cfg.context_size = 8192;
        cfg.n_batch = 512;
        cfg.n_gpu_layers = 99;
        cfg.temperature = 0.3f;
        cfg.max_tokens = 4096;
        cfg.enable_speculative = true;
        cfg.draft_tokens = 5;

        if (client.LoadModel(cfg.model_path)) {
            initialized = true;
            std::cerr << "[Sovereign] Model loaded: " << cfg.model_path << "\n";
        } else {
            std::cerr << "[Sovereign] WARN: Model load failed, running fallback mode (no inference available)\n";
            initialized = true; // Fallback mode active - no model loaded
        }
    }
    return client;
}

// ---------------------------------------------------------------------------
// FileManager: extract code blocks and write to disk
// ---------------------------------------------------------------------------
struct FileManager {
    static bool writeGeneratedCode(const std::string& markdown,
                                   const std::string& defaultPath = "generated_code.cpp") {
        std::vector<std::pair<std::string, std::string>> blocks;
        extractCodeBlocks(markdown, blocks);

        if (blocks.empty()) {
            // No code blocks found — write entire response as single file
            std::ofstream out(defaultPath);
            if (!out) return false;
            out << markdown;
            return true;
        }

        bool allOk = true;
        for (const auto& [lang, code] : blocks) {
            std::string path = guessPath(lang, defaultPath);
            std::ofstream out(path);
            if (out) {
                out << code;
                std::cerr << "[FileManager] Wrote: " << path << " ("
                          << code.size() << " bytes)\n";
            } else {
                std::cerr << "[FileManager] ERR: Cannot write " << path << "\n";
                allOk = false;
            }
        }
        return allOk;
    }

private:
    static void extractCodeBlocks(const std::string& md,
                                  std::vector<std::pair<std::string, std::string>>& out) {
        size_t pos = 0;
        while (true) {
            size_t start = md.find("```", pos);
            if (start == std::string::npos) break;
            start += 3;
            size_t nl = md.find('\n', start);
            std::string lang;
            if (nl != std::string::npos) {
                lang = md.substr(start, nl - start);
                start = nl + 1;
            }
            size_t end = md.find("```", start);
            if (end == std::string::npos) break;
            out.push_back({lang, md.substr(start, end - start)});
            pos = end + 3;
        }
    }

    static std::string guessPath(const std::string& lang,
                                 const std::string& fallback) {
        if (lang == "cpp" || lang == "c++" || lang == "cxx") return "generated.cpp";
        if (lang == "c") return "generated.c";
        if (lang == "asm" || lang == "masm") return "generated.asm";
        if (lang == "h" || lang == "hpp") return "generated.hpp";
        if (lang == "py") return "generated.py";
        if (lang == "js") return "generated.js";
        if (lang == "ts") return "generated.ts";
        return fallback;
    }
};

// ---------------------------------------------------------------------------
// Validation Sandbox: silent compile check
// ---------------------------------------------------------------------------
struct ValidationSandbox {
    static bool compileCheck(const std::string& sourcePath,
                             std::string& errorLog) {
        std::string ext = std::filesystem::path(sourcePath).extension().string();
        std::string cmd;
        if (ext == ".cpp" || ext == ".cxx" || ext == ".cc") {
            cmd = "cl.exe /nologo /c /W0 /EHsc \"" + sourcePath + "\" 2>&1";
        } else if (ext == ".c") {
            cmd = "cl.exe /nologo /c /W0 \"" + sourcePath + "\" 2>&1";
        } else if (ext == ".asm") {
            cmd = "ml64.exe /c \"" + sourcePath + "\" 2>&1";
        } else {
            errorLog = "Unknown extension: " + ext;
            return false;
        }

        // Run compiler silently
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
        HANDLE hRead, hWrite;
        CreatePipe(&hRead, &hWrite, &sa, 0);
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {sizeof(si)};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdError = hWrite;
        si.hStdOutput = hWrite;
        PROCESS_INFORMATION pi = {};

        BOOL ok = CreateProcessA(NULL, const_cast<char*>(cmd.c_str()),
                                 NULL, NULL, TRUE,
                                 CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (!ok) {
            errorLog = "Failed to launch compiler";
            CloseHandle(hWrite);
            CloseHandle(hRead);
            return false;
        }

        WaitForSingleObject(pi.hProcess, 15000); // 15s timeout
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hWrite);

        // Read error pipe
        char buf[4096];
        DWORD read = 0;
        std::string output;
        while (ReadFile(hRead, buf, sizeof(buf)-1, &read, NULL) && read > 0) {
            buf[read] = 0;
            output += buf;
        }
        CloseHandle(hRead);

        errorLog = output;
        return (exitCode == 0);
    }
};

// ---------------------------------------------------------------------------
// Generate-Validate-Fix loop
// ---------------------------------------------------------------------------
static std::string generateWithFixLoop(const std::string& prompt,
                                       int maxIterations = 3) {
    AgenticDeepThinkingEngine engine;
    AgenticDeepThinkingEngine::ThinkingContext ctx;
    ctx.problem = prompt;
    ctx.language = "C++";
    ctx.projectRoot = ".";
    ctx.deepResearch = true;
    ctx.cycleMultiplier = 3;
    ctx.allowSelfCorrection = true;
    ctx.maxIterations = maxIterations;

    std::string lastError;
    for (int attempt = 0; attempt < maxIterations; ++attempt) {
        if (attempt > 0 && !lastError.empty()) {
            ctx.problem = prompt + "\n\n[Compiler Error]\n" + lastError
                        + "\n\nPlease fix the above errors.";
        }

        auto result = engine.think(ctx);
        if (result.finalAnswer.empty()) {
            std::cerr << "[Gen] Attempt " << (attempt+1)
                      << ": Empty response\n";
            continue;
        }

        // Write code blocks to files
        if (!FileManager::writeGeneratedCode(result.finalAnswer)) {
            std::cerr << "[Gen] Attempt " << (attempt+1)
                      << ": File write failed\n";
            continue;
        }

        // Validate with compiler
        std::string errorLog;
        bool compiled = ValidationSandbox::compileCheck("generated.cpp", errorLog);
        if (compiled) {
            std::cerr << "[Gen] SUCCESS after " << (attempt+1) << " attempt(s)\n";
            return result.finalAnswer;
        }

        std::cerr << "[Gen] Attempt " << (attempt+1)
                  << ": Compile failed, retrying...\n";
        lastError = errorLog;

        // Feed error into failure detector for next iteration
        AgenticFailureDetector detector;
        auto failure = detector.detectFailure(errorLog, prompt);
        if (failure.confidence > 0.5f) {
            std::cerr << "[Gen] Detected issue: " << failure.description << "\n";
        }
    }

    return "// Generation failed after " + std::to_string(maxIterations)
         + " attempts. Last error:\n// " + lastError;
}

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0]
                  << " \"<prompt>\" [output_file]\n"
                     "Example:\n  " << argv[0]
                  << " \"Create a thread-safe logger in Win32\" generated.cpp\n";
        return 1;
    }

    std::string prompt = argv[1];
    std::string outputFile = (argc > 2) ? argv[2] : "generated_code.cpp";

    std::cerr << "========================================\n";
    std::cerr << "  RawrXD Sovereign Code Generator\n";
    std::cerr << "  Zero external dependencies\n";
    std::cerr << "========================================\n\n";

    // Initialize sovereign inference
    auto& llm = getThinkingLLM();
    if (!llm.IsLoaded()) {
        std::cerr << "[WARN] Running in fallback mode (no model loaded - output will be empty)\n";
    }

    auto t0 = std::chrono::high_resolution_clock::now();

    // Run Generate-Validate-Fix loop
    std::string generated = generateWithFixLoop(prompt);

    auto t1 = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    // Write final output
    std::ofstream out(outputFile);
    if (out.is_open()) {
        out << generated;
        out.close();
        std::cerr << "\n[OK] Output written to: " << outputFile << "\n";
    } else {
        std::cerr << "\n[ERR] Cannot open output file: " << outputFile << "\n";
        return 1;
    }

    std::cerr << "Total time: " << dur.count() << " ms\n";
    return 0;
}
