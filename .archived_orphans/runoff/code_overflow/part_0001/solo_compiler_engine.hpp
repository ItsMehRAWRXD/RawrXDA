#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <memory>
#include <cstdint>
#include <cstdio>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Compiler {

// ============================================================================
// Compiler Types
// ============================================================================

enum class TargetArchitecture { X86_64, X86_32, ARM64, RISCV64, WASM, Auto };
enum class CompilationStage { 
    Idle, Initializing, Preprocessing, Lexing, Parsing, 
    SemanticAnalysis, IRGeneration, Optimization, CodeGeneration, 
    Assembly, Linking, Complete, Failed 
};

struct CompilationOptions {
    std::string inputFile;
    std::string outputFile;
    TargetArchitecture target = TargetArchitecture::Auto;
    int optimizationLevel = 2;  // 0-3
    bool debugSymbols = false;
    bool verbose = false;
    bool warningsAsErrors = false;
    std::vector<std::string> includePaths;
    std::vector<std::string> libraryPaths;
    std::vector<std::string> libraries;
    std::vector<std::string> defines;
    std::vector<std::string> extraFlags;
};

struct CompilationError {
    int line = 0;
    int column = 0;
    std::string file;
    std::string message;
    bool isWarning = false;
};

struct CompilationMetrics {
    int64_t totalTimeMs = 0;
    int64_t preprocessTimeMs = 0;
    int64_t compileTimeMs = 0;
    int64_t linkTimeMs = 0;
    int64_t inputSizeBytes = 0;
    int64_t outputSizeBytes = 0;
    int errorCount = 0;
    int warningCount = 0;
    CompilationStage lastStage = CompilationStage::Idle;
};

using ProgressCallback = std::function<void(CompilationStage, int)>;

// ============================================================================
// SoloCompilerEngine — System compiler wrapper
// ============================================================================

/**
 * @class SoloCompilerEngine
 * @brief Wraps system compilers (MSVC cl.exe, clang, gcc) to provide
 *        a unified compilation interface for the IDE.
 *
 * Delegates actual compilation to the platform's native toolchain
 * and captures output, errors, and metrics in real-time.
 */
class SoloCompilerEngine {
public:
    SoloCompilerEngine() = default;
    explicit SoloCompilerEngine(const CompilationOptions& opts) : options_(opts) {}
    ~SoloCompilerEngine() = default;

    void setProgressCallback(ProgressCallback cb) { progressCb_ = std::move(cb); }

    /**
     * @brief Compile a source file to output
     * @return true on success, false on failure
     */
    bool compile(const std::string& sourceFile, const std::string& outputFile) {
        auto startTime = std::chrono::steady_clock::now();
        metrics_ = {};
        errors_.clear();

        emitProgress(CompilationStage::Initializing, 0);

        // Determine file size for metrics
        FILE* f = fopen(sourceFile.c_str(), "rb");
        if (!f) {
            addError(0, 0, sourceFile, "Cannot open source file");
            metrics_.lastStage = CompilationStage::Failed;
            return false;
        }
        fseek(f, 0, SEEK_END);
        metrics_.inputSizeBytes = ftell(f);
        fclose(f);

        // Detect language from file extension
        std::string ext;
        auto dotPos = sourceFile.rfind('.');
        if (dotPos != std::string::npos) {
            ext = sourceFile.substr(dotPos);
        }

        // Build compiler command line
        std::string cmd = buildCommandLine(sourceFile, outputFile, ext);
        if (cmd.empty()) {
            addError(0, 0, sourceFile, "No suitable compiler found for this file type");
            metrics_.lastStage = CompilationStage::Failed;
            return false;
        }

        emitProgress(CompilationStage::Preprocessing, 10);

        // Execute compiler process and capture output
        emitProgress(CompilationStage::CodeGeneration, 30);
        
        std::string compilerOutput;
        int exitCode = executeProcess(cmd, compilerOutput);

        emitProgress(CompilationStage::Linking, 80);

        // Parse compiler output for errors and warnings
        parseCompilerOutput(compilerOutput, sourceFile);

        auto endTime = std::chrono::steady_clock::now();
        metrics_.totalTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        metrics_.compileTimeMs = metrics_.totalTimeMs;

        // Check output file exists
        FILE* outF = fopen(outputFile.c_str(), "rb");
        if (outF) {
            fseek(outF, 0, SEEK_END);
            metrics_.outputSizeBytes = ftell(outF);
            fclose(outF);
        }

        bool success = (exitCode == 0) && (metrics_.errorCount == 0);
        metrics_.lastStage = success ? CompilationStage::Complete : CompilationStage::Failed;
        emitProgress(success ? CompilationStage::Complete : CompilationStage::Failed, 100);

        return success;
    }

    const CompilationMetrics& getMetrics() const { return metrics_; }
    const std::vector<CompilationError>& getErrors() const { return errors_; }

private:
    CompilationOptions options_;
    CompilationMetrics metrics_;
    std::vector<CompilationError> errors_;
    ProgressCallback progressCb_;

    void emitProgress(CompilationStage stage, int percent) {
        if (progressCb_) progressCb_(stage, percent);
    }

    void addError(int line, int col, const std::string& file, const std::string& msg, bool warning = false) {
        CompilationError err;
        err.line = line;
        err.column = col;
        err.file = file;
        err.message = msg;
        err.isWarning = warning;
        errors_.push_back(err);
        if (warning) metrics_.warningCount++;
        else metrics_.errorCount++;
    }

    std::string buildCommandLine(const std::string& source, const std::string& output,
                                  const std::string& ext) {
        std::string cmd;
        
        bool isCpp = (ext == ".cpp" || ext == ".cxx" || ext == ".cc");
        bool isC = (ext == ".c");
        bool isAsm = (ext == ".asm" || ext == ".s");

#ifdef _WIN32
        // MSVC cl.exe for C/C++
        if (isCpp || isC) {
            cmd = "cl.exe /nologo /EHsc";
            if (isCpp) cmd += " /std:c++20";
            if (options_.debugSymbols) cmd += " /Zi";
            switch (options_.optimizationLevel) {
                case 0: cmd += " /Od"; break;
                case 1: cmd += " /O1"; break;
                case 2: cmd += " /O2"; break;
                case 3: cmd += " /Ox"; break;
            }
            for (const auto& inc : options_.includePaths) cmd += " /I\"" + inc + "\"";
            for (const auto& def : options_.defines) cmd += " /D" + def;
            for (const auto& flag : options_.extraFlags) cmd += " " + flag;
            cmd += " /Fe\"" + output + "\" \"" + source + "\"";
            for (const auto& lib : options_.libraries) cmd += " " + lib;
        } else if (isAsm) {
            cmd = "ml64.exe /nologo /c /Fo\"" + output + "\" \"" + source + "\"";
        }
#else
        // GCC/Clang for POSIX
        if (isCpp) {
            cmd = "c++ -std=c++20";
        } else if (isC) {
            cmd = "cc";
        }
        if (!cmd.empty()) {
            if (options_.debugSymbols) cmd += " -g";
            cmd += " -O" + std::to_string(std::min(options_.optimizationLevel, 3));
            for (const auto& inc : options_.includePaths) cmd += " -I" + inc;
            for (const auto& def : options_.defines) cmd += " -D" + def;
            for (const auto& flag : options_.extraFlags) cmd += " " + flag;
            cmd += " -o \"" + output + "\" \"" + source + "\"";
            for (const auto& lp : options_.libraryPaths) cmd += " -L" + lp;
            for (const auto& lib : options_.libraries) cmd += " -l" + lib;
        }
        if (isAsm) {
            cmd = "nasm -f elf64 -o \"" + output + "\" \"" + source + "\"";
        }
#endif
        return cmd;
    }

    int executeProcess(const std::string& cmd, std::string& output) {
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hReadPipe, hWritePipe;
        CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;

        PROCESS_INFORMATION pi = {};
        std::string cmdMutable = cmd;
        BOOL created = CreateProcessA(NULL, cmdMutable.data(), NULL, NULL, TRUE,
                                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        CloseHandle(hWritePipe);

        if (!created) {
            CloseHandle(hReadPipe);
            output = "Failed to launch compiler process";
            return -1;
        }

        // Read output
        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }
        CloseHandle(hReadPipe);

        WaitForSingleObject(pi.hProcess, 60000); // 60s timeout
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        return static_cast<int>(exitCode);
#else
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            output = "Failed to launch compiler process";
            return -1;
        }
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            output += buffer;
        }
        return pclose(pipe);
#endif
    }

    void parseCompilerOutput(const std::string& output, const std::string& defaultFile) {
        // Parse MSVC-style and GCC/Clang-style error messages
        // MSVC: file.cpp(line): error C1234: message
        // GCC:  file.cpp:line:col: error: message
        
        std::string line;
        for (size_t i = 0; i < output.size(); ) {
            size_t eol = output.find('\n', i);
            if (eol == std::string::npos) eol = output.size();
            line = output.substr(i, eol - i);
            i = eol + 1;

            // Trim carriage return
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            bool isWarning = false;
            
            // MSVC format: filename(line) : error/warning Cxxxx: message
            auto parenPos = line.find('(');
            auto colonError = line.find(": error");
            auto colonWarning = line.find(": warning");
            
            if (parenPos != std::string::npos && (colonError != std::string::npos || colonWarning != std::string::npos)) {
                isWarning = (colonWarning != std::string::npos && 
                            (colonError == std::string::npos || colonWarning < colonError));
                std::string file = line.substr(0, parenPos);
                auto closeP = line.find(')', parenPos);
                int errLine = 0;
                if (closeP != std::string::npos) {
                    try { errLine = std::stoi(line.substr(parenPos + 1, closeP - parenPos - 1)); }
                    catch (...) {}
                }
                size_t msgStart = isWarning ? colonWarning + 2 : colonError + 2;
                std::string msg = (msgStart < line.size()) ? line.substr(msgStart) : line;
                addError(errLine, 0, file, msg, isWarning);
                continue;
            }
            
            // GCC/Clang format: file:line:col: error/warning: message
            auto firstColon = line.find(':');
            if (firstColon != std::string::npos && firstColon > 0) {
                auto secondColon = line.find(':', firstColon + 1);
                if (secondColon != std::string::npos) {
                    std::string file = line.substr(0, firstColon);
                    int errLine = 0, errCol = 0;
                    try { errLine = std::stoi(line.substr(firstColon + 1)); } catch (...) {}
                    auto thirdColon = line.find(':', secondColon + 1);
                    if (thirdColon != std::string::npos) {
                        try { errCol = std::stoi(line.substr(secondColon + 1)); } catch (...) {}
                        std::string rest = line.substr(thirdColon + 1);
                        while (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
                        isWarning = (rest.find("warning") == 0);
                        if (rest.find("error") == 0 || isWarning) {
                            auto msgColon = rest.find(':');
                            std::string msg = (msgColon != std::string::npos) ? rest.substr(msgColon + 1) : rest;
                            while (!msg.empty() && msg[0] == ' ') msg.erase(0, 1);
                            addError(errLine, errCol, file, msg, isWarning);
                        }
                    }
                }
            }
        }
    }
};

// ============================================================================
// CompilerFactory — creates configured SoloCompilerEngine instances
// ============================================================================

class CompilerFactory {
public:
    static std::unique_ptr<SoloCompilerEngine> createSoloCompiler(const CompilationOptions& opts) {
        return std::make_unique<SoloCompilerEngine>(opts);
    }

    static CompilationOptions createDefaultOptions() {
        CompilationOptions opts;
        opts.target = TargetArchitecture::X86_64;
        opts.optimizationLevel = 2;
        opts.debugSymbols = false;
        opts.verbose = false;
        return opts;
    }
};

} // namespace Compiler
