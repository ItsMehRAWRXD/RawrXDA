#include "self_code.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

namespace RawrXD {

bool SelfCode::editSource(const std::string& filePath, const std::string& oldSnippet, const std::string& newSnippet) {
    if (!replaceInFile(filePath, oldSnippet, newSnippet)) {
        m_lastError = "Could not find snippet in " + filePath;
        return false;
    }
    return true;
}

bool SelfCode::addInclude(const std::string& hppFile, const std::string& includeLine) {
    return insertAfterIncludeGuard(hppFile, includeLine);
}

bool SelfCode::rebuildTarget(const std::string& target, const std::string& config) {
    std::vector<std::string> args = {"--build", "build", "--target", target, "--config", config};
    return runProcess("cmake", args);
}

bool SelfCode::replaceInFile(const std::string& path, const std::string& oldText, const std::string& newText) {
    std::ifstream infile(path);
    if (!infile.is_open()) return false;
    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string content = buffer.str();
    infile.close();

    size_t pos = content.find(oldText);
    if (pos == std::string::npos) return false;

    content.replace(pos, oldText.length(), newText);

    std::ofstream outfile(path, std::ios::trunc);
    outfile << content;
    return true;
}

bool SelfCode::insertAfterIncludeGuard(const std::string& hpp, const std::string& includeLine) {
    std::ifstream infile(hpp);
    if (!infile.is_open()) return false;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(infile, line)) {
        lines.push_back(line);
    }
    infile.close();

    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find("#pragma once") != std::string::npos || lines[i].find("#ifndef") != std::string::npos) {
            lines.insert(lines.begin() + i + 1, includeLine);
            break;
        }
    }

    std::ofstream outfile(hpp, std::ios::trunc);
    for (const auto& l : lines) outfile << l << "\n";
    return true;
}

bool SelfCode::runProcess(const std::string& program, const std::vector<std::string>& args) {
    std::string cmd = program;
    for (const auto& arg : args) cmd += " " + arg;

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    return false;
}

} // namespace RawrXD
