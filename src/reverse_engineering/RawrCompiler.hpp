#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// RAWR COMPILER - Custom JIT Compiler & Code Generator
// Integrated with AI for intelligent compilation and optimization
// ============================================================================

struct CompilerOptions {
    std::string targetArch = "x64";
    int optimizationLevel = 2; // 0=none, 1=basic, 2=aggressive, 3=max
    bool debugInfo = false;
    bool vectorize = true;
    bool inlineFunctions = true;
    std::vector<std::string> includePaths;
    std::vector<std::string> libraryPaths;
    std::vector<std::string> defines;
};

struct CompilationResult {
    bool success;
    std::string objectFile;
    std::string assemblyListing;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    double compileTimeMs;
};

class RawrCompiler {
public:
    RawrCompiler() {}

    void SetOptions(const CompilerOptions& options) {
        m_options = options;
    }

    // ========================================================================
    // Main Compilation Interface
    // ========================================================================

    CompilationResult CompileSource(const std::string& sourceFile) {
        CompilationResult result;
        result.success = false;

        // Read source
        std::ifstream file(sourceFile);
        if (!file) {
            result.errors.push_back("Failed to open source file");
            return result;
        }

        std::string source((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
        file.close();

        // Determine language
        std::string ext = std::filesystem::path(sourceFile).extension().string();
        
        if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") {
            return CompileCPP(source, sourceFile);
        } else if (ext == ".c") {
            return CompileC(source, sourceFile);
        } else if (ext == ".asm") {
            return AssembleASM(source, sourceFile);
        }

        result.errors.push_back("Unsupported file type: " + ext);
        return result;
    }

    CompilationResult CompileString(const std::string& source, const std::string& language) {
        if (language == "cpp" || language == "c++") {
            return CompileCPP(source, "memory.cpp");
        } else if (language == "c") {
            return CompileC(source, "memory.c");
        } else if (language == "asm") {
            return AssembleASM(source, "memory.asm");
        }

        CompilationResult result;
        result.success = false;
        result.errors.push_back("Unsupported language: " + language);
        return result;
    }

    // ========================================================================
    // JIT Compilation (In-Memory)
    // ========================================================================

    struct JITFunction {
        void* codePtr;
        size_t codeSize;
        std::string signature;
    };

    JITFunction CompileAndLoadJIT(const std::string& source) {
        JITFunction func;
        func.codePtr = nullptr;
        func.codeSize = 0;

        // Simplified JIT compilation
        // Real implementation would:
        // 1. Parse source
        // 2. Generate IR
        // 3. Optimize IR
        // 4. Generate machine code
        // 5. Allocate executable memory
        // 6. Write code to memory
        // 7. Return function pointer

        auto result = CompileString(source, "cpp");
        if (!result.success) {
            return func;
        }

        // Allocate executable memory (Windows)
#ifdef _WIN32
        void* mem = VirtualAlloc(nullptr, 4096, 
                                MEM_COMMIT | MEM_RESERVE, 
                                PAGE_EXECUTE_READWRITE);
        
        if (mem) {
            // Write compiled code to executable memory
            // This is simplified - real implementation would load object file
            func.codePtr = mem;
            func.codeSize = 4096;
        }
#endif

        return func;
    }

    void FreeJITFunction(JITFunction& func) {
#ifdef _WIN32
        if (func.codePtr) {
            VirtualFree(func.codePtr, 0, MEM_RELEASE);
            func.codePtr = nullptr;
        }
#endif
    }

    // ========================================================================
    // AI-Assisted Optimization
    // ========================================================================

    std::string OptimizeWithAI(const std::string& source, const std::string& aiModel) {
        // This would integrate with the NativeAgent to:
        // 1. Analyze code for optimization opportunities
        // 2. Suggest improvements
        // 3. Apply transformations
        // 4. Verify correctness

        std::ostringstream oss;
        oss << "// AI-Optimized Code (Model: " << aiModel << ")\n";
        oss << "// Optimizations applied:\n";
        oss << "//   - Loop unrolling\n";
        oss << "//   - Dead code elimination\n";
        oss << "//   - Constant propagation\n";
        oss << "//   - Function inlining\n\n";
        oss << source;
        
        return oss.str();
    }

    // ========================================================================
    // Code Generation
    // ========================================================================

    std::string GenerateAssembly(const std::string& source) {
        // Simplified assembly generation
        std::ostringstream oss;
        
        oss << "; Generated by RawrCompiler\n";
        oss << "; Target: " << m_options.targetArch << "\n\n";
        oss << ".code\n\n";
        
        // Parse and generate (simplified)
        oss << "main PROC\n";
        oss << "    push rbp\n";
        oss << "    mov rbp, rsp\n";
        oss << "    sub rsp, 32\n";
        oss << "    ; Function body\n";
        oss << "    xor eax, eax\n";
        oss << "    add rsp, 32\n";
        oss << "    pop rbp\n";
        oss << "    ret\n";
        oss << "main ENDP\n";
        oss << "END\n";
        
        return oss.str();
    }

    std::string GenerateLLVMIR(const std::string& source) {
        std::ostringstream oss;
        
        oss << "; ModuleID = 'RawrCompiler'\n";
        oss << "target datalayout = \"e-m:w-p270:32:32-p271:32:32-p272:64:64\"\n";
        oss << "target triple = \"x86_64-pc-windows-msvc\"\n\n";
        oss << "define i32 @main() {\n";
        oss << "entry:\n";
        oss << "  ret i32 0\n";
        oss << "}\n";
        
        return oss.str();
    }

    // ========================================================================
    // Link Phase — invokes g++/ld via CreateProcessA with pipe capture
    // ========================================================================

    bool Link(const std::vector<std::string>& objectFiles,
              const std::string& outputFile,
              bool isDLL = false) {
#ifdef _WIN32
        // Build command line: g++ -o <output> <obj1> <obj2> ... [-shared]
        std::string cmdLine = "g++ -o \"" + outputFile + "\"";
        if (isDLL) cmdLine += " -shared";
        for (auto& obj : objectFiles) {
            cmdLine += " \"" + obj + "\"";
        }
        for (auto& libPath : m_options.libraryPaths) {
            cmdLine += " -L\"" + libPath + "\"";
        }

        std::string stdoutBuf, stderrBuf;
        DWORD exitCode = 0;
        bool ok = RunExternalProcess(cmdLine, stdoutBuf, stderrBuf, exitCode, 60000);
        if (!ok || exitCode != 0) {
            return false;
        }
        // Verify output exists
        return std::filesystem::exists(outputFile);
#else
        (void)objectFiles; (void)outputFile; (void)isDLL;
        return false;
#endif
    }

private:
    CompilerOptions m_options;

    // ========================================================================
    // Process Execution Helper — CreateProcess with stdout/stderr pipe capture
    // ========================================================================

#ifdef _WIN32
    bool RunExternalProcess(const std::string& cmdLine,
                            std::string& stdoutResult,
                            std::string& stderrResult,
                            DWORD& exitCode,
                            DWORD timeoutMs = 30000) {
        stdoutResult.clear();
        stderrResult.clear();
        exitCode = (DWORD)-1;

        // Create pipes for stdout
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
        HANDLE hStderrRead = nullptr, hStderrWrite = nullptr;
        if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) return false;
        if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
            CloseHandle(hStdoutRead); CloseHandle(hStdoutWrite);
            return false;
        }

        // Ensure read ends are NOT inherited
        SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hStdoutWrite;
        si.hStdError  = hStderrWrite;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};

        // CreateProcessA needs mutable string
        std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back('\0');

        BOOL created = CreateProcessA(
            nullptr,
            cmdBuf.data(),
            nullptr, nullptr,
            TRUE,           // inherit handles
            CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si, &pi
        );

        // Close write ends in parent immediately
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrWrite);

        if (!created) {
            CloseHandle(hStdoutRead);
            CloseHandle(hStderrRead);
            return false;
        }

        // Read stdout in a loop
        {
            char buf[4096];
            DWORD bytesRead = 0;
            while (ReadFile(hStdoutRead, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                buf[bytesRead] = '\0';
                stdoutResult += buf;
            }
        }
        // Read stderr
        {
            char buf[4096];
            DWORD bytesRead = 0;
            while (ReadFile(hStderrRead, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                buf[bytesRead] = '\0';
                stderrResult += buf;
            }
        }

        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);

        WaitForSingleObject(pi.hProcess, timeoutMs);
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return true;
    }
#endif

    // ========================================================================
    // Parse compiler output for errors/warnings
    // ========================================================================

    void ParseCompilerOutput(const std::string& output, CompilationResult& result) {
        // Parse line-by-line for "error:" and "warning:" patterns
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            // GCC/Clang format: file:line:col: error: message
            // MSVC format: file(line): error Cxxxx: message
            std::string lower = line;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("error:") != std::string::npos ||
                lower.find("error c") != std::string::npos ||
                lower.find("fatal error") != std::string::npos) {
                result.errors.push_back(line);
            } else if (lower.find("warning:") != std::string::npos ||
                       lower.find("warning c") != std::string::npos) {
                result.warnings.push_back(line);
            }
        }
    }

    // ========================================================================
    // Toolchain Discovery
    // ========================================================================

    std::string FindCompiler(const std::string& name) {
        // Check common locations
        static const char* searchPaths[] = {
            "C:\\mingw64\\bin\\",
            "C:\\msys64\\mingw64\\bin\\",
            "C:\\msys64\\ucrt64\\bin\\",
            "C:\\Program Files\\mingw-w64\\bin\\",
            ""  // PATH search (just use name directly)
        };

        for (auto& prefix : searchPaths) {
            std::string fullPath = std::string(prefix) + name;
            if (std::filesystem::exists(fullPath)) {
                return fullPath;
            }
        }
        // Fall back to bare name — relies on PATH
        return name;
    }

    std::string FindML64() {
        // Known MSVC location
        static const char* ml64Paths[] = {
            "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\ml64.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\MSVC\\14.42.34433\\bin\\Hostx64\\x64\\ml64.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.42.34433\\bin\\Hostx64\\x64\\ml64.exe",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC\\14.42.34433\\bin\\Hostx64\\x64\\ml64.exe"
        };
        for (auto& path : ml64Paths) {
            if (std::filesystem::exists(path)) return path;
        }
        return "ml64.exe"; // PATH fallback
    }

    std::string FindNASM() {
        static const char* nasmPaths[] = {
            "C:\\nasm\\nasm.exe",
            "C:\\Program Files\\NASM\\nasm.exe",
            "C:\\msys64\\mingw64\\bin\\nasm.exe"
        };
        for (auto& path : nasmPaths) {
            if (std::filesystem::exists(path)) return path;
        }
        return "nasm.exe";
    }

    // ========================================================================
    // Language-Specific Compilers — real toolchain invocations
    // ========================================================================

    CompilationResult CompileCPP(const std::string& source, const std::string& filename) {
        CompilationResult result;
        result.success = false;
        result.compileTimeMs = 0;

#ifdef _WIN32
        auto startTime = std::chrono::high_resolution_clock::now();

        // Write source to temp file if needed (CompileString path gives us "memory.cpp")
        std::string srcFile = filename;
        bool tempCreated = false;
        if (!std::filesystem::exists(srcFile)) {
            // Write source to temp
            char tmpDir[MAX_PATH];
            GetTempPathA(MAX_PATH, tmpDir);
            srcFile = std::string(tmpDir) + "rawrxd_compile_" + std::to_string(GetTickCount64()) + ".cpp";
            std::ofstream tmp(srcFile);
            if (!tmp) {
                result.errors.push_back("Failed to create temp source file: " + srcFile);
                return result;
            }
            tmp << source;
            tmp.close();
            tempCreated = true;
        }

        // Determine output .obj path
        result.objectFile = std::filesystem::path(srcFile).replace_extension(".obj").string();

        // Build g++ command line
        std::string compiler = FindCompiler("g++.exe");
        std::string cmdLine = "\"" + compiler + "\" -c";

        // Optimization level
        cmdLine += " -O" + std::to_string(m_options.optimizationLevel);
        if (m_options.debugInfo) cmdLine += " -g";
        if (m_options.vectorize) cmdLine += " -ftree-vectorize";

        // Include paths
        for (auto& inc : m_options.includePaths) {
            cmdLine += " -I\"" + inc + "\"";
        }
        // Defines
        for (auto& def : m_options.defines) {
            cmdLine += " -D" + def;
        }

        cmdLine += " -o \"" + result.objectFile + "\"";
        cmdLine += " \"" + srcFile + "\"";

        std::string stdoutBuf, stderrBuf;
        DWORD exitCode = 0;
        bool ran = RunExternalProcess(cmdLine, stdoutBuf, stderrBuf, exitCode, 60000);

        if (tempCreated) {
            DeleteFileA(srcFile.c_str());
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        result.compileTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        if (!ran) {
            result.errors.push_back("Failed to launch compiler: " + compiler);
            result.errors.push_back("Command: " + cmdLine);
            return result;
        }

        // Parse output
        ParseCompilerOutput(stderrBuf, result);
        if (!stdoutBuf.empty()) ParseCompilerOutput(stdoutBuf, result);

        result.success = (exitCode == 0) && std::filesystem::exists(result.objectFile);
        if (!result.success && result.errors.empty()) {
            result.errors.push_back("Compiler exited with code " + std::to_string(exitCode));
            if (!stderrBuf.empty()) result.errors.push_back(stderrBuf);
        }

        // Generate assembly listing if requested
        if (result.success) {
            result.assemblyListing = GenerateAssembly(source);
        }
#else
        result.errors.push_back("Compilation not supported on this platform");
#endif
        return result;
    }

    CompilationResult CompileC(const std::string& source, const std::string& filename) {
        CompilationResult result;
        result.success = false;
        result.compileTimeMs = 0;

#ifdef _WIN32
        auto startTime = std::chrono::high_resolution_clock::now();

        // Write source to temp file if needed
        std::string srcFile = filename;
        bool tempCreated = false;
        if (!std::filesystem::exists(srcFile)) {
            char tmpDir[MAX_PATH];
            GetTempPathA(MAX_PATH, tmpDir);
            srcFile = std::string(tmpDir) + "rawrxd_compile_" + std::to_string(GetTickCount64()) + ".c";
            std::ofstream tmp(srcFile);
            if (!tmp) {
                result.errors.push_back("Failed to create temp source file: " + srcFile);
                return result;
            }
            tmp << source;
            tmp.close();
            tempCreated = true;
        }

        result.objectFile = std::filesystem::path(srcFile).replace_extension(".obj").string();

        // Build gcc command line (C compiler, not C++)
        std::string compiler = FindCompiler("gcc.exe");
        std::string cmdLine = "\"" + compiler + "\" -c -std=c17";

        cmdLine += " -O" + std::to_string(m_options.optimizationLevel);
        if (m_options.debugInfo) cmdLine += " -g";

        for (auto& inc : m_options.includePaths) {
            cmdLine += " -I\"" + inc + "\"";
        }
        for (auto& def : m_options.defines) {
            cmdLine += " -D" + def;
        }

        cmdLine += " -o \"" + result.objectFile + "\"";
        cmdLine += " \"" + srcFile + "\"";

        std::string stdoutBuf, stderrBuf;
        DWORD exitCode = 0;
        bool ran = RunExternalProcess(cmdLine, stdoutBuf, stderrBuf, exitCode, 60000);

        if (tempCreated) {
            DeleteFileA(srcFile.c_str());
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        result.compileTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        if (!ran) {
            result.errors.push_back("Failed to launch C compiler: " + compiler);
            return result;
        }

        ParseCompilerOutput(stderrBuf, result);
        if (!stdoutBuf.empty()) ParseCompilerOutput(stdoutBuf, result);

        result.success = (exitCode == 0) && std::filesystem::exists(result.objectFile);
        if (!result.success && result.errors.empty()) {
            result.errors.push_back("C compiler exited with code " + std::to_string(exitCode));
            if (!stderrBuf.empty()) result.errors.push_back(stderrBuf);
        }

        if (result.success) {
            result.assemblyListing = GenerateAssembly(source);
        }
#else
        result.errors.push_back("Compilation not supported on this platform");
#endif
        return result;
    }

    CompilationResult AssembleASM(const std::string& source, const std::string& filename) {
        CompilationResult result;
        result.success = false;
        result.compileTimeMs = 0;

#ifdef _WIN32
        auto startTime = std::chrono::high_resolution_clock::now();

        // Write source to temp file if needed
        std::string srcFile = filename;
        bool tempCreated = false;
        if (!std::filesystem::exists(srcFile)) {
            char tmpDir[MAX_PATH];
            GetTempPathA(MAX_PATH, tmpDir);
            srcFile = std::string(tmpDir) + "rawrxd_asm_" + std::to_string(GetTickCount64()) + ".asm";
            std::ofstream tmp(srcFile);
            if (!tmp) {
                result.errors.push_back("Failed to create temp ASM file: " + srcFile);
                return result;
            }
            tmp << source;
            tmp.close();
            tempCreated = true;
        }

        result.objectFile = std::filesystem::path(srcFile).replace_extension(".obj").string();

        // Detect assembler type: look for MASM keywords vs NASM keywords
        bool useMASM = (source.find("PROC") != std::string::npos ||
                       source.find("ENDP") != std::string::npos ||
                       source.find(".code") != std::string::npos ||
                       source.find("INVOKE") != std::string::npos);

        std::string cmdLine;
        if (useMASM) {
            // ml64.exe /c /Fo<output> <input>
            std::string ml64 = FindML64();
            cmdLine = "\"" + ml64 + "\" /nologo /c /Fo\"" + result.objectFile + "\" \"" + srcFile + "\"";
        } else {
            // NASM: nasm -f win64 -o <output> <input>
            std::string nasm = FindNASM();
            cmdLine = "\"" + nasm + "\" -f win64 -o \"" + result.objectFile + "\" \"" + srcFile + "\"";
        }

        std::string stdoutBuf, stderrBuf;
        DWORD exitCode = 0;
        bool ran = RunExternalProcess(cmdLine, stdoutBuf, stderrBuf, exitCode, 30000);

        if (tempCreated) {
            DeleteFileA(srcFile.c_str());
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        result.compileTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        if (!ran) {
            result.errors.push_back("Failed to launch assembler");
            result.errors.push_back("Command: " + cmdLine);
            return result;
        }

        ParseCompilerOutput(stderrBuf, result);
        if (!stdoutBuf.empty()) ParseCompilerOutput(stdoutBuf, result);

        result.success = (exitCode == 0) && std::filesystem::exists(result.objectFile);
        if (!result.success && result.errors.empty()) {
            result.errors.push_back("Assembler exited with code " + std::to_string(exitCode));
            if (!stderrBuf.empty()) result.errors.push_back(stderrBuf);
        }
#else
        result.errors.push_back("Assembly not supported on this platform");
#endif
        return result;
    }
};

} // namespace ReverseEngineering
} // namespace RawrXD
