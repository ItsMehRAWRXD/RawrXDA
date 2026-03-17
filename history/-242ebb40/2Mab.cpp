#include "compiler_integration.h"
#include <process.h>
#include <io.h>

CompilerIntegration::CompilerIntegration(const std::wstring& toolchainPath)
    : m_toolchainPath(toolchainPath), m_currentCompiler(L"gcc") {
}

CompilerIntegration::~CompilerIntegration() = default;

bool CompilerIntegration::Initialize() {
    // Build paths to compilers
    m_gccPath = m_toolchainPath + L"\\gcc\\bin\\gcc.exe";
    m_clangPath = m_toolchainPath + L"\\clang\\bin\\clang.exe";
    m_makePath = m_toolchainPath + L"\\make\\bin\\make.exe";
    m_gdbPath = m_toolchainPath + L"\\gdb\\bin\\gdb.exe";
    
    // Check if at least one compiler is available
    bool gccAvailable = VerifyCompilerExists(m_gccPath);
    bool clangAvailable = VerifyCompilerExists(m_clangPath);
    
    if (!gccAvailable && !clangAvailable) {
        return false;
    }
    
    // Set default compiler to the first available one
    if (gccAvailable) {
        m_currentCompiler = L"gcc";
    } else {
        m_currentCompiler = L"clang";
    }
    
    // Setup environment variables for the toolchain
    SetupEnvironmentVariables();
    
    return true;
}

bool CompilerIntegration::IsAvailable() const {
    return VerifyCompilerExists(GetCompilerPath(m_currentCompiler));
}

CompileResult CompilerIntegration::CompileFile(const std::wstring& sourceFile,
                                              const std::wstring& outputFile,
                                              const std::vector<std::wstring>& flags,
                                              const std::wstring& compiler) {
    
    std::wstring compilerToUse = compiler.empty() ? m_currentCompiler : compiler;
    std::wstring compilerPath = GetCompilerPath(compilerToUse);
    
    if (!VerifyCompilerExists(compilerPath)) {
        CompileResult result;
        result.exit_code = 1;
        result.errors = "Compiler not found: " + IDEUtils::WStringToString(compilerPath);
        return result;
    }
    
    // Determine output file if not specified
    std::wstring output = outputFile;
    if (output.empty()) {
        output = sourceFile;
        size_t pos = output.find_last_of(L'.');
        if (pos != std::wstring::npos) {
            output = output.substr(0, pos) + L".exe";
        } else {
            output += L".exe";
        }
    }
    
    // Build compile command
    std::wstring command = BuildCompileCommand(compilerToUse, sourceFile, output, flags);
    
    // Get working directory from source file path
    std::wstring workingDir = IDEUtils::GetDirectoryFromPath(sourceFile);
    
    return ExecuteCommand(command, workingDir);
}

CompileResult CompilerIntegration::BuildProject(const std::wstring& projectPath) {
    std::wstring projectType = DetectProjectType(projectPath);
    
    if (projectType == L"makefile") {
        return RunMake(projectPath);
    } else if (projectType == L"cmake") {
        // TODO: Implement CMake build support
        CompileResult result;
        result.exit_code = 1;
        result.errors = "CMake projects not yet supported";
        return result;
    } else {
        // Try to find and compile individual source files
        CompileResult result;
        result.exit_code = 1;
        result.errors = "Unknown project type or no build system found";
        return result;
    }
}

CompileResult CompilerIntegration::RunMake(const std::wstring& projectPath, const std::wstring& target) {
    if (!VerifyCompilerExists(m_makePath)) {
        CompileResult result;
        result.exit_code = 1;
        result.errors = "Make not found";
        return result;
    }
    
    std::wstring command = L"\"" + m_makePath + L"\"";
    
    // Add target if specified
    if (!target.empty()) {
        command += L" " + target;
    }
    
    // Add parallel build flag
    command += L" -j" + std::to_wstring(std::thread::hardware_concurrency());
    
    return ExecuteCommand(command, projectPath);
}

void CompilerIntegration::SetCurrentCompiler(const std::wstring& compiler) {
    if (VerifyCompilerExists(GetCompilerPath(compiler))) {
        m_currentCompiler = compiler;
    }
}

std::vector<std::wstring> CompilerIntegration::GetAvailableCompilers() const {
    std::vector<std::wstring> compilers;
    
    if (VerifyCompilerExists(m_gccPath)) {
        compilers.push_back(L"gcc");
    }
    
    if (VerifyCompilerExists(m_clangPath)) {
        compilers.push_back(L"clang");
    }
    
    return compilers;
}

std::wstring CompilerIntegration::GetCompilerVersion(const std::wstring& compiler) const {
    std::wstring compilerPath = GetCompilerPath(compiler);
    if (!VerifyCompilerExists(compilerPath)) {
        return L"Not available";
    }
    
    std::wstring command = L"\"" + compilerPath + L"\" --version";
    CompileResult result = const_cast<CompilerIntegration*>(this)->ExecuteCommand(command);
    
    if (result.exit_code == 0 && !result.output.empty()) {
        // Extract version from first line of output
        std::string output = result.output;
        size_t pos = output.find('\n');
        if (pos != std::string::npos) {
            output = output.substr(0, pos);
        }
        return IDEUtils::StringToWString(output);
    }
    
    return L"Unknown";
}

std::wstring CompilerIntegration::GetCompilerPath(const std::wstring& compiler) const {
    if (compiler == L"gcc") {
        return m_gccPath;
    } else if (compiler == L"clang") {
        return m_clangPath;
    }
    
    return L"";
}

std::wstring CompilerIntegration::GetLinkerPath(const std::wstring& compiler) const {
    // For now, use the same executable as the compiler
    return GetCompilerPath(compiler);
}

CompileResult CompilerIntegration::ExecuteCommand(const std::wstring& command, const std::wstring& workingDir) {
    CompileResult result = {};
    auto startTime = std::chrono::high_resolution_clock::now();
    
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    
    HANDLE hStdoutRead, hStdoutWrite;
    HANDLE hStderrRead, hStderrWrite;
    
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0) ||
        !CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
        result.exit_code = 1;
        result.errors = "Failed to create pipes for process communication";
        return result;
    }
    
    // Ensure pipe handles are not inherited by child process
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.dwFlags = STARTF_USESTDHANDLES;
    
    PROCESS_INFORMATION pi = {};
    
    // Set working directory if specified
    const wchar_t* workDir = workingDir.empty() ? nullptr : workingDir.c_str();
    
    BOOL created = CreateProcessW(
        nullptr,
        const_cast<LPWSTR>(command.c_str()),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workDir,
        &si,
        &pi
    );
    
    if (created) {
        // Close write handles so pipes don't hang
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrWrite);
        
        // Read output from pipes
        std::thread stdoutThread([&result, hStdoutRead]() {
            char buffer[4096];
            DWORD bytesRead;
            
            while (ReadFile(hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\\0';
                result.output.append(buffer);
            }
        });
        
        std::thread stderrThread([&result, hStderrRead]() {
            char buffer[4096];
            DWORD bytesRead;
            
            while (ReadFile(hStderrRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\\0';
                result.errors.append(buffer);
            }
        });
        
        // Wait for process to complete
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        // Get exit code
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exit_code = static_cast<int>(exitCode);
        
        // Wait for output threads to complete
        stdoutThread.join();
        stderrThread.join();
        
        // Cleanup process handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        result.exit_code = 1;
        result.errors = "Failed to create process: " + IDEUtils::WStringToString(command);
        
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrWrite);
    }
    
    // Cleanup pipe handles
    CloseHandle(hStdoutRead);
    CloseHandle(hStderrRead);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.compile_time = std::chrono::duration<double>(endTime - startTime).count();
    
    return result;
}

std::wstring CompilerIntegration::BuildCompileCommand(const std::wstring& compiler,
                                                     const std::wstring& sourceFile,
                                                     const std::wstring& outputFile,
                                                     const std::vector<std::wstring>& flags) {
    
    std::wstring compilerPath = GetCompilerPath(compiler);
    std::wstring command = L"\"" + compilerPath + L"\"";
    
    // Add static linking flags for portability
    command += L" -static -static-libgcc -static-libstdc++";
    
    // Add optimization and warning flags
    command += L" -O2 -Wall -Wextra";
    
    // Add user-specified flags
    for (const auto& flag : flags) {
        command += L" " + flag;
    }
    
    // Add source file
    command += L" \"" + sourceFile + L"\"";
    
    // Add output file
    command += L" -o \"" + outputFile + L"\"";
    
    return command;
}

std::wstring CompilerIntegration::DetectProjectType(const std::wstring& projectPath) {
    // Check for Makefile
    std::vector<std::wstring> makefileNames = {L"Makefile", L"makefile", L"GNUmakefile"};
    for (const auto& name : makefileNames) {
        std::wstring makefilePath = IDEUtils::JoinPath(projectPath, name);
        if (IDEUtils::PathExists(makefilePath)) {
            return L"makefile";
        }
    }
    
    // Check for CMakeLists.txt
    std::wstring cmakeFile = IDEUtils::JoinPath(projectPath, L"CMakeLists.txt");
    if (IDEUtils::PathExists(cmakeFile)) {
        return L"cmake";
    }
    
    // Check for Visual Studio project files
    WIN32_FIND_DATAW findData;
    std::wstring searchPath = projectPath + L"\\*.vcxproj";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        FindClose(hFind);
        return L"msbuild";
    }
    
    return L"unknown";
}

std::wstring CompilerIntegration::FindProjectFile(const std::wstring& projectPath) {
    std::wstring projectType = DetectProjectType(projectPath);
    
    if (projectType == L"makefile") {
        std::vector<std::wstring> makefileNames = {L"Makefile", L"makefile", L"GNUmakefile"};
        for (const auto& name : makefileNames) {
            std::wstring makefilePath = IDEUtils::JoinPath(projectPath, name);
            if (IDEUtils::PathExists(makefilePath)) {
                return makefilePath;
            }
        }
    } else if (projectType == L"cmake") {
        return IDEUtils::JoinPath(projectPath, L"CMakeLists.txt");
    }
    
    return L"";
}

bool CompilerIntegration::VerifyCompilerExists(const std::wstring& path) const {
    return !path.empty() && IDEUtils::PathExists(path);
}

void CompilerIntegration::SetupEnvironmentVariables() {
    // Add toolchain bin directories to PATH
    std::wstring currentPath;
    DWORD pathSize = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    if (pathSize > 0) {
        std::vector<wchar_t> pathBuffer(pathSize);
        GetEnvironmentVariableW(L"PATH", pathBuffer.data(), pathSize);
        currentPath = pathBuffer.data();
    }
    
    // Add our toolchain directories
    std::wstring newPath = m_toolchainPath + L"\\gcc\\bin;" +
                          m_toolchainPath + L"\\clang\\bin;" +
                          m_toolchainPath + L"\\make\\bin;" +
                          m_toolchainPath + L"\\gdb\\bin;";
    
    if (!currentPath.empty()) {
        newPath += currentPath;
    }
    
    SetEnvironmentVariableW(L"PATH", newPath.c_str());
    
    // Set other useful environment variables
    SetEnvironmentVariableW(L"CC", GetCompilerPath(L"gcc").c_str());
    SetEnvironmentVariableW(L"CXX", GetCompilerPath(L"gcc").c_str());
}