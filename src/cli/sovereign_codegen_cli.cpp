// =============================================================================
// sovereign_codegen_cli.cpp — Standalone Sovereign Code Generator CLI
// Pure C++20 / Win32 — ZERO network, ZERO Qt, ZERO Electron
// =============================================================================
// Build:   cl /std:c++20 /O2 /MT /Fe:sovereign_codegen.exe sovereign_codegen_cli.cpp
//          link ... NativeInferenceClient.obj model_invoker.obj msvcrt.lib kernel32.lib
// Usage:   sovereign_codegen.exe "Create a thread-safe logger in Win32" --out=./generated
// =============================================================================

#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "NativeInferenceClient.h"
#include "model_invoker.hpp"

// ---------------------------------------------------------------------------
// CLI Arguments
// ---------------------------------------------------------------------------
struct CliArgs {
    std::wstring modelPath = L"models\\deep_thinker_v1.gguf";
    std::string  prompt;
    std::string  outDir = ".\\generated";
    int          cycleMultiplier = 3;
    float        confidenceThreshold = 0.85f;
    bool         verbose = false;
    bool         buildValidate = true;
    bool         help = false;
};

static void printUsage(const char* exe) {
    std::cout << "Sovereign Code Generator — RawrXD Standalone CLI\n"
              << "Usage: " << exe << " <prompt> [options]\n"
              << "Options:\n"
              << "  --model=<path>      Path to GGUF model (default: models\\deep_thinker_v1.gguf)\n"
              << "  --out=<dir>         Output directory (default: .\\generated)\n"
              << "  --cycles=N           Cycle multiplier for deep thinking (default: 3)\n"
              << "  --confidence=F       Minimum confidence threshold (default: 0.85)\n"
              << "  --no-build           Skip build validation step\n"
              << "  -v, --verbose        Enable verbose logging\n"
              << "  -h, --help           Show this help\n";
}

static CliArgs parseArgs(int argc, char* argv[]) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            args.help = true;
        } else if (arg.starts_with("--model=")) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, arg.c_str() + 8, -1, nullptr, 0);
            std::wstring wpath(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, arg.c_str() + 8, -1, wpath.data(), wlen);
            args.modelPath = wpath;
        } else if (arg.starts_with("--out=")) {
            args.outDir = arg.substr(6);
        } else if (arg.starts_with("--cycles=")) {
            args.cycleMultiplier = std::atoi(arg.c_str() + 9);
        } else if (arg.starts_with("--confidence=")) {
            args.confidenceThreshold = static_cast<float>(std::atof(arg.c_str() + 13));
        } else if (arg == "--no-build") {
            args.buildValidate = false;
        } else if (arg == "-v" || arg == "--verbose") {
            args.verbose = true;
        } else if (args.prompt.empty()) {
            args.prompt = arg;
        }
    }
    return args;
}

// ---------------------------------------------------------------------------
// File Manager — Write generated code blocks to disk
// ---------------------------------------------------------------------------
class FileManager {
public:
    static bool ensureDir(const std::string& dir) {
        return std::filesystem::create_directories(dir);
    }

    static bool writeFile(const std::string& path, const std::string& content) {
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) return false;
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        return ofs.good();
    }

    static std::vector<std::pair<std::string, std::string>> extractCodeBlocks(const std::string& markdown) {
        std::vector<std::pair<std::string, std::string>> blocks;
        size_t pos = 0;
        while ((pos = markdown.find("```", pos)) != std::string::npos) {
            size_t langEnd = markdown.find('\n', pos);
            if (langEnd == std::string::npos) break;
            std::string lang = markdown.substr(pos + 3, langEnd - pos - 3);
            size_t blockStart = langEnd + 1;
            size_t blockEnd = markdown.find("```", blockStart);
            if (blockEnd == std::string::npos) break;
            std::string code = markdown.substr(blockStart, blockEnd - blockStart);
            blocks.emplace_back(lang, code);
            pos = blockEnd + 3;
        }
        return blocks;
    }

    static bool writeGeneratedCode(const std::string& markdown, const std::string& outDir) {
        ensureDir(outDir);
        auto blocks = extractCodeBlocks(markdown);
        if (blocks.empty()) {
            // No code blocks — write entire response as raw text
            std::string path = outDir + "\\generated.txt";
            return writeFile(path, markdown);
        }
        int idx = 0;
        for (const auto& [lang, code] : blocks) {
            std::string ext = (lang == "cpp" || lang == "c++") ? ".cpp"
                            : (lang == "h" || lang == "hpp") ? ".hpp"
                            : (lang == "asm") ? ".asm"
                            : (lang == "c") ? ".c"
                            : ".txt";
            std::string path = outDir + "\\generated_" + std::to_string(idx++) + ext;
            if (!writeFile(path, code)) return false;
        }
        return true;
    }
};

// ---------------------------------------------------------------------------
// Build Validator — Silent build check via CreateProcess
// ---------------------------------------------------------------------------
class BuildValidator {
public:
    static bool validate(const std::string& outDir) {
        // Look for build.bat or CMakeLists.txt in output dir
        std::string buildScript = outDir + "\\build.bat";
        if (!std::filesystem::exists(buildScript)) {
            // Try to synthesize a minimal build.bat
            std::ofstream ofs(buildScript);
            if (ofs) {
                ofs << "@echo off\n"
                    << "cl /std:c++20 /W4 /EHsc /O2 *.cpp /Fe:generated.exe > build.log 2>&1\n"
                    << "if %ERRORLEVEL% NEQ 0 exit /b 1\n"
                    << "exit /b 0\n";
            }
        }

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        std::string cmd = "cmd.exe /c \"" + buildScript + "\"";

        BOOL created = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                                      CREATE_NO_WINDOW, nullptr, outDir.c_str(), &si, &pi);
        if (!created) return false;

        WaitForSingleObject(pi.hProcess, 60000); // 60s timeout
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }

    static std::string getBuildLog(const std::string& outDir) {
        std::string logPath = outDir + "\\build.log";
        std::ifstream ifs(logPath, std::ios::binary);
        if (!ifs) return "";
        return std::string((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
    }
};

// ---------------------------------------------------------------------------
// Sovereign Inference Engine Wrapper
// ---------------------------------------------------------------------------
class SovereignInferenceEngine {
public:
    explicit SovereignInferenceEngine(const std::wstring& modelPath) {
        if (!NativeInferenceClient_Initialize(modelPath.c_str())) {
            throw std::runtime_error("Failed to initialize sovereign inference engine");
        }
    }
    ~SovereignInferenceEngine() {
        NativeInferenceClient_Shutdown();
    }

    std::string generate(const std::string& prompt, int cycleMultiplier) {
        // Phase 1: Tokenize
        int32_t tokens[1024];
        int64_t tokenCount = ModelInvoker_PrepareContext(prompt.c_str(), tokens, 1024);
        if (tokenCount <= 0) return "";

        // Phase 2: Deep thinking iterations (cycle multiplier)
        char outBuf[32768] = {};
        for (int cycle = 0; cycle < cycleMultiplier; ++cycle) {
            int64_t written = ModelInvoker_Invoke(tokens, static_cast<size_t>(tokenCount),
                                                    outBuf, sizeof(outBuf));
            if (written <= 0) return "";
        }

        return std::string(outBuf);
    }

    float computeConfidence(const std::string& response, const std::string& prompt) {
        // Simple heuristic: keyword overlap + response length
        size_t overlap = 0;
        std::istringstream iss(prompt);
        std::string word;
        while (iss >> word) {
            if (response.find(word) != std::string::npos) ++overlap;
        }
        float keywordScore = (prompt.empty()) ? 0.0f
                           : static_cast<float>(overlap) / static_cast<float>(prompt.size() / 4 + 1);
        float lengthScore = std::min(1.0f, static_cast<float>(response.size()) / 2048.0f);
        return 0.6f * keywordScore + 0.4f * lengthScore;
    }

private:
    // Deleted copy/move
    SovereignInferenceEngine(const SovereignInferenceEngine&) = delete;
    SovereignInferenceEngine& operator=(const SovereignInferenceEngine&) = delete;
};

// ---------------------------------------------------------------------------
// Main — Generate-Validate-Fix Loop
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    CliArgs args = parseArgs(argc, argv);
    if (args.help || args.prompt.empty()) {
        printUsage(argv[0]);
        return args.help ? 0 : 1;
    }

    std::cout << "[Sovereign] Initializing inference engine...\n";
    std::unique_ptr<SovereignInferenceEngine> engine;
    try {
        engine = std::make_unique<SovereignInferenceEngine>(args.modelPath);
    } catch (const std::exception& e) {
        std::cerr << "[Sovereign] Engine init failed: " << e.what() << "\n";
        return 1;
    }

    std::cout << "[Sovereign] Model loaded. Generating code for: \""
              << args.prompt << "\"\n";

    // Generate-Validate-Fix loop
    int maxIterations = 5;
    for (int iteration = 0; iteration < maxIterations; ++iteration) {
        std::cout << "[Sovereign] Iteration " << (iteration + 1) << "/" << maxIterations << "\n";

        // Draft
        std::string response = engine->generate(args.prompt, args.cycleMultiplier);
        if (response.empty()) {
            std::cerr << "[Sovereign] Inference returned empty response.\n";
            continue;
        }

        if (args.verbose) {
            std::cout << "[Sovereign] Raw response:\n" << response << "\n---\n";
        }

        // Evaluate confidence
        float confidence = engine->computeConfidence(response, args.prompt);
        std::cout << "[Sovereign] Confidence: " << confidence << "\n";

        // Write files
        if (!FileManager::writeGeneratedCode(response, args.outDir)) {
            std::cerr << "[Sovereign] Failed to write generated code.\n";
            continue;
        }
        std::cout << "[Sovereign] Code written to: " << args.outDir << "\n";

        // Validate build
        if (args.buildValidate) {
            std::cout << "[Sovereign] Running build validation...\n";
            if (BuildValidator::validate(args.outDir)) {
                std::cout << "[Sovereign] Build validation PASSED.\n";
                if (confidence >= args.confidenceThreshold) {
                    std::cout << "[Sovereign] Confidence threshold met. Generation complete.\n";
                    return 0;
                }
            } else {
                std::string buildLog = BuildValidator::getBuildLog(args.outDir);
                std::cerr << "[Sovereign] Build validation FAILED.\n";
                if (args.verbose && !buildLog.empty()) {
                    std::cerr << "[Sovereign] Build log:\n" << buildLog << "\n";
                }
                // Feed build errors back into prompt for next iteration
                args.prompt += "\n[BUILD ERRORS]\n" + buildLog;
                continue;
            }
        } else {
            std::cout << "[Sovereign] Build validation skipped.\n";
            if (confidence >= args.confidenceThreshold) {
                std::cout << "[Sovereign] Confidence threshold met. Generation complete.\n";
                return 0;
            }
        }
    }

    std::cerr << "[Sovereign] Max iterations reached without meeting confidence threshold.\n";
    return 1;
}
