#include "self_code.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <windows.h>

namespace fs = std::filesystem;

// Helper cleanup
static std::string readFileContent(const std::string& path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool writeFileContent(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << content;
    return true;
}

bool SelfCode::runProcess(const std::string& cmd, const std::vector<std::string>& args) {
    std::string commandLine = cmd;
    for (const auto& arg : args) {
        commandLine += " \"" + arg + "\"";
    }
    
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    
    char* cmdLine = _strdup(commandLine.c_str());
    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmdLine);
        return false;
    }
    free(cmdLine);
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (exitCode == 0);
}

// --------------------------------------------------------------------------

bool SelfCode::editSource(const std::string& filePath,
                          const std::string& oldSnippet,
                          const std::string& newSnippet) {
    std::string content = readFileContent(filePath);
    if (content.empty()) {
        m_lastError = "Cannot read " + filePath;
        return false;
    }

    size_t pos = content.find(oldSnippet);
    if (pos == std::string::npos) {
        m_lastError = "Old snippet not found in " + filePath;
        return false;
    }

    if (!replaceInFile(filePath, oldSnippet, newSnippet))
        return false;

    // No MOC regeneration needed as we are moving away from Qt
    
    return true;
}

bool SelfCode::addInclude(const std::string& hppFile,
                          const std::string& includeLine) {
    if (includeLine.find("#include") != 0)
        return false;

    std::string content = readFileContent(hppFile);
    if (content.empty()) {
        m_lastError = "Cannot read " + hppFile;
        return false;
    }

    if (content.find(includeLine) != std::string::npos)
        return true;

    size_t lastInclude = content.rfind("#include");
    if (lastInclude == std::string::npos) {
        return insertAfterIncludeGuard(hppFile, includeLine);
    }
    
    size_t insertPos = content.find('\n', lastInclude);
    if (insertPos == std::string::npos)
        insertPos = content.length();
    else
        insertPos += 1;

    std::string newContent = content.substr(0, insertPos) 
                           + includeLine + "\n" 
                           + content.substr(insertPos);
    
    return replaceInFile(hppFile, content, newContent);
}

bool SelfCode::regenerateMOC(const std::string& header) {
    // Deprecated / No-op
    return true;
}

bool SelfCode::rebuildTarget(const std::string& target,
                             const std::string& config) {
    // "cmake --build build --config <config> --target <target>"
    std::vector<std::string> args = {"--build", "build", "--config", config, "--target", target};
    if (!runProcess("cmake", args))
        return false;

    // Verify binary exists
    // Assuming structure: build/bin/<target>.exe or similar
    // The original code looked for RawrXD-QtShell.exe, but we are renaming/refactoring things.
    // Let's just check if build folder exists for now or trust cmake exit code.
    return true;
}

bool SelfCode::replaceInFile(const std::string& path,
                             const std::string& oldText,
                             const std::string& newText) {
    // Note: oldText and newText here are likely the *whole file content* 
    // or *snippets* depending on caller.
    // In editSource, it calls replaceInFile(path, oldSnippet, newSnippet).
    // In addInclude, it calls replaceInFile(path, oldContent, newContent).
    
    // The original logic for replaceInFile likely handled the actual writing.
    // If oldText == content (full replacement), we just write.
    // If oldText is a snippet, we finding-replace.
    
    std::string content = readFileContent(path);
    if (content == oldText) {
        // Full replacement
        return writeFileContent(path, newText);
    }
    
    // Snippet replacement
    size_t pos = content.find(oldText);
    if (pos != std::string::npos) {
        content.replace(pos, oldText.length(), newText);
        return writeFileContent(path, content);
    }
    
    m_lastError = "Could not locate text to replace";
    return false;
}

bool SelfCode::insertAfterIncludeGuard(const std::string& path, const std::string& code) {
    std::string content = readFileContent(path);
    // Look for #pragma once
    size_t pos = content.find("#pragma once");
    if (pos != std::string::npos) {
        size_t endLine = content.find('\n', pos);
        if (endLine == std::string::npos) endLine = content.length();
        else endLine += 1;
        
        std::string newContent = content.substr(0, endLine) + code + "\n" + content.substr(endLine);
        return writeFileContent(path, newContent);
    }
    // Fallback: prepend
    std::string newContent = code + "\n" + content;
    return writeFileContent(path, newContent);
}
