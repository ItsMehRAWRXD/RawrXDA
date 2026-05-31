#include "self_code.hpp"
#include "process_utils.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

bool SelfCode::editSource(const std::string& filePath,
                          const std::string& oldSnippet,
                          const std::string& newSnippet) {
    std::string content = fileutil::readAll(filePath);
    if (content.empty()) {
        m_lastError = "Cannot read " + filePath;
        return false;
    }

    if (content.find(oldSnippet) == std::string::npos) {
        m_lastError = "Old snippet not found in " + filePath;
        return false;
    }

    if (!replaceInFile(filePath, oldSnippet, newSnippet))
        return false;

    return true;
}

bool SelfCode::addInclude(const std::string& hppFile,
                          const std::string& includeLine) {
    if (includeLine.rfind("#include", 0) != 0)
        return false;

    std::string content = fileutil::readAll(hppFile);
    if (content.empty()) {
        m_lastError = "Cannot read " + hppFile;
        return false;
    }

    if (content.find(includeLine) != std::string::npos)
        return true;

    // Find last #include
    size_t lastInclude = content.rfind("#include");
    if (lastInclude == std::string::npos) {
        return insertAfterIncludeGuard(hppFile, includeLine);
    }
    size_t insertPos = content.find('\n', lastInclude);
    if (insertPos == std::string::npos)
        insertPos = content.size();
    else
        insertPos += 1;

    std::string newContent = content.substr(0, insertPos)
                           + includeLine + "\n"
                           + content.substr(insertPos);
    return fileutil::writeAll(hppFile, newContent);
}

bool SelfCode::regenerateMOC(const std::string& /*header*/) {
    // MOC is a Qt concept — no longer needed in non-Qt build
    return true;
}

bool SelfCode::rebuildTarget(const std::string& target,
                             const std::string& config) {
    ProcResult pr = proc::run("cmake",
        {"--build", "build", "--config", config, "--target", target},
        120000);

    if (!pr.ok()) {
        m_lastError = "cmake build failed: " + pr.stderrStr;
        return false;
    }

    std::filesystem::path exe = std::filesystem::current_path() / "build" / "bin" / config / "RawrXD-Shell.exe";
    std::error_code ec;
    if (!std::filesystem::exists(exe, ec) || std::filesystem::file_size(exe, ec) == 0) {
        m_lastError = "Binary not produced or zero size";
        return false;
    }
    return true;
}

bool SelfCode::replaceInFile(const std::string& path,
                             const std::string& oldText,
                             const std::string& newText) {
    std::string content = fileutil::readAll(path);
    if (content.empty()) {
        m_lastError = "Cannot read " + path;
        return false;
    }

    size_t idx = content.find(oldText);
    if (idx == std::string::npos) {
        m_lastError = "Old text not found (exact match required)";
        return false;
    }
    content.replace(idx, oldText.length(), newText);
    if (!fileutil::writeAll(path, content)) {
        m_lastError = "Failed to write " + path;
        return false;
    }
    return true;
}

bool SelfCode::insertAfterIncludeGuard(const std::string& hpp,
                                       const std::string& includeLine) {
    std::string content = fileutil::readAll(hpp);
    if (content.empty()) {
        m_lastError = "Cannot read " + hpp;
        return false;
    }

    size_t pos = 0;
    if (content.rfind("#pragma once", 0) == 0) {
        pos = content.find('\n');
        if (pos != std::string::npos) pos += 1;
    } else {
        size_t ifndef = content.find("#ifndef");
        if (ifndef != std::string::npos) {
            size_t define = content.find("#define", ifndef);
            if (define != std::string::npos) {
                pos = content.find('\n', define);
                if (pos != std::string::npos) pos += 1;
            }
        }
    }
    if (pos == std::string::npos) pos = 0;

    std::string newContent = content.substr(0, pos)
                           + includeLine + "\n"
                           + content.substr(pos);
    return fileutil::writeAll(hpp, newContent);
}

bool SelfCode::runProcess(const std::string& program,
                          const std::vector<std::string>& args) {
    ProcResult pr = proc::run(program, args, 120000);
    if (pr.timedOut) {
        m_lastError = program + " timed out";
        return false;
    }
    if (pr.exitCode != 0) {
        m_lastError = program + " failed: " + pr.stderrStr;
        return false;
    }
    return true;
}
