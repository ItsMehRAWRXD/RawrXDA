#pragma once

#include "native_ide.h"

class CompilerIntegration {
private:
    std::wstring m_toolchainPath;
    std::wstring m_gccPath;
    std::wstring m_clangPath;
    std::wstring m_makePath;
    std::wstring m_gdbPath;
    std::wstring m_currentCompiler;  // "gcc" or "clang"
    
public:
    explicit CompilerIntegration(const std::wstring& toolchainPath);
    ~CompilerIntegration();
    
    bool Initialize();
    bool IsAvailable() const;
    
    // Compiler operations
    CompileResult CompileFile(const std::wstring& sourceFile, 
                             const std::wstring& outputFile = L"",
                             const std::vector<std::wstring>& flags = {},
                             const std::wstring& compiler = L"");
    
    CompileResult BuildProject(const std::wstring& projectPath);
    CompileResult RunMake(const std::wstring& projectPath, const std::wstring& target = L"");
    
    // Compiler settings
    void SetCurrentCompiler(const std::wstring& compiler);
    std::wstring GetCurrentCompiler() const { return m_currentCompiler; }
    
    // Available compilers
    std::vector<std::wstring> GetAvailableCompilers() const;
    std::wstring GetCompilerVersion(const std::wstring& compiler) const;
    
    // Path utilities
    std::wstring GetCompilerPath(const std::wstring& compiler) const;
    std::wstring GetLinkerPath(const std::wstring& compiler) const;
    std::wstring GetDebuggerPath() const { return m_gdbPath; }
    
private:
    CompileResult ExecuteCommand(const std::wstring& command, const std::wstring& workingDir = L"");
    std::wstring BuildCompileCommand(const std::wstring& compiler,
                                    const std::wstring& sourceFile,
                                    const std::wstring& outputFile,
                                    const std::vector<std::wstring>& flags);
    
    std::wstring DetectProjectType(const std::wstring& projectPath);
    std::wstring FindProjectFile(const std::wstring& projectPath);
    
    bool VerifyCompilerExists(const std::wstring& path) const;
    void SetupEnvironmentVariables();
};