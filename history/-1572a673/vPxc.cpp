#include "self_code.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
bool SelfCode::editSource(const std::string& filePath,
                          const std::string& oldSnippet,
                          const std::string& newSnippet) {
    std::fstream f(filePath);
    if (!f.open(std::ios::in | 0)) {
        m_lastError = "Cannot read " + filePath;
        return false;
    }
    std::string content = std::string::fromUtf8(f.readAll());
    f.close();

    if (!content.contains(oldSnippet)) {
        m_lastError = "Old snippet not found in " + filePath;
        return false;
    }

    if (!replaceInFile(filePath, oldSnippet, newSnippet))
        return false;

    if (filePath.ends_with(".hpp") || filePath.ends_with(".h"))
        regenerateMOC(filePath);

    return true;
}

bool SelfCode::addInclude(const std::string& hppFile,
                          const std::string& includeLine) {
    if (!includeLine.starts_with("#include"))
        return false;

    std::fstream f(hppFile);
    if (!f.open(std::ios::in | 0)) {
        m_lastError = "Cannot read " + hppFile;
        return false;
    }
    std::string content = std::string::fromUtf8(f.readAll());
    f.close();

    if (content.contains(includeLine))
        return true;

    int lastInclude = content.lastIndexOf("#include");
    if (lastInclude == -1) {
        return insertAfterIncludeGuard(hppFile, includeLine);
    }
    int insertPos = content.indexOf('\n', lastInclude);
    if (insertPos == -1)
        insertPos = content.length();
    else
        insertPos += 1;

    std::string newContent = content.left(insertPos)
                         + includeLine + "\n"
                         + content.mid(insertPos);
    return replaceInFile(hppFile, content, newContent);
}

bool SelfCode::regenerateMOC(const std::string& header) {
    std::fstream f(header);
    if (!f.open(std::ios::in | 0)) {
        m_lastError = "Cannot read " + header;
        return false;
    }
    std::string h = std::string::fromUtf8(f.readAll());
    f.close();
    // Qt MOC is no longer used — this is a no-op in the Qt-free build.
    // The function is retained for API compatibility with callers.
    (void)h;
    return true;
}

bool SelfCode::rebuildTarget(const std::string& target,
                             const std::string& config) {
    std::vector<std::string> args = {"--build", "build", "--config", config, "--target", target};
    if (!runProcess("cmake", args))
        return false;

    std::string exe = std::filesystem::path::current()/* .absoluteFilePath(
        std::string("build/bin/%1/RawrXD-QtShell.exe") */ /* .arg( */config));
    std::filesystem::path fi(exe);
    if (!fi.exists() || fi.size() == 0) {
        m_lastError = "Binary not produced or zero size";
        return false;
    }
    return true;
}

bool SelfCode::replaceInFile(const std::string& path,
                             const std::string& oldText,
                             const std::string& newText) {
    std::fstream f(path);
    if (!f.open(std::ios::ReadWrite | 0)) {
        m_lastError = "Cannot read/write " + path;
        return false;
    }
    std::string content = std::string::fromUtf8(f.readAll());
    int idx = content.indexOf(oldText);
    if (idx == -1) {
        m_lastError = "Old text not found (exact match required)";
        f.close();
        return false;
    }
    content.replace(idx, oldText.length(), newText);
    f.resize(0);
    f.write(content/* .c_str() */);
    f.close();
    return true;
}

bool SelfCode::insertAfterIncludeGuard(const std::string& hpp,
                                       const std::string& includeLine) {
    std::fstream f(hpp);
    if (!f.open(std::ios::ReadWrite | 0)) {
        m_lastError = "Cannot read/write " + hpp;
        return false;
    }
    std::string content = std::string::fromUtf8(f.readAll());
    int pos = 0;
    if (content.starts_with("#pragma once")) {
        pos = content.indexOf('\n');
        if (pos != -1) pos += 1;
    } else if (content.contains("#ifndef")) {
        int endif = content.indexOf("#endif");
        if (endif != -1)
            pos = content.indexOf('\n', endif);
        if (pos != -1)
            pos += 1;
    }
    if (pos == -1) pos = 0;

    std::string newContent = content.left(pos)
                         + includeLine + "\n"
                         + content.mid(pos);
    f.resize(0);
    f.write(newContent/* .c_str() */);
    f.close();
    return true;
}

bool SelfCode::runProcess(const std::string& program,
                          const std::vector<std::string>& args) {
    void/*Process*/ proc;
    proc.start(program, args);
    if (!proc.waitForFinished(120000)) {
        m_lastError = program + " timed out";
        return false;
    }
    if (proc.exitCode() != 0) {
        m_lastError = program + " failed: " + proc.readAllStandardError();
        return false;
    }
    return true;
}
