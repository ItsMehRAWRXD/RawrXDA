// ============================================================================
// Win32IDE_Git.cpp — Real Git integration for Win32IDE
// ============================================================================
// Diff view, blame, stage/commit from IDE, branch indicator
// Shell to git CLI or libgit2 for repository operations
// Provides VS Code-style Git UX in native Win32 IDE
// ============================================================================

#include "Win32IDE_Git.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace RawrXD {
namespace IDE {

class GitRepository::GitRepositoryImpl {
private:
    std::string m_repoPath;
    std::string m_gitPath;      // Path to git.exe
    std::string m_currentBranch;
    std::mutex m_mutex;
    bool m_initialized = false;

public:
    GitRepositoryImpl() = default;
    ~GitRepositoryImpl() = default;

    bool initialize(const std::string& repoPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_repoPath = repoPath;
        
        if (!findGitExecutable()) {
            fprintf(stderr, "[GitRepository] Git not found in PATH\n");
            return false;
        }
        
        if (!isGitRepository()) {
            fprintf(stderr, "[GitRepository] Not a Git repository: %s\n", repoPath.c_str());
            return false;
        }
        
        m_currentBranch = getCurrentBranch();
        m_initialized = true;
        
        fprintf(stderr, "[GitRepository] Initialized for: %s\n", repoPath.c_str());
        fprintf(stderr, "[GitRepository] Current branch: %s\n", m_currentBranch.c_str());
        
        return true;
    }

    std::string getBranchName() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
        return m_currentBranch;
    }

    std::vector<GitFileStatus> getStatus() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized) {
            return {};
        }
        
        std::vector<GitFileStatus> files;
        std::string output = executeGitCommand("status --porcelain");
        std::istringstream stream(output);
        std::string line;
        
        while (std::getline(stream, line)) {
            if (line.size() < 4) {
                continue;
            }
            
            GitFileStatus file;
            char indexCode = line[0];
            char workCode = line[1];
            
            file.indexStatus = parseStatusCode(indexCode);
            file.status = parseStatusCode(workCode);
            file.filePath = line.substr(3);
            
            files.push_back(file);
        }
        
        return files;
    }

    bool stageFile(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return false;
        std::string cmd = "add \"" + filePath + "\"";
        executeGitCommand(cmd);
        fprintf(stderr, "[GitRepository] Staged: %s\n", filePath.c_str());
        return true;
    }

    bool unstageFile(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return false;
        std::string cmd = "reset HEAD \"" + filePath + "\"";
        executeGitCommand(cmd);
        fprintf(stderr, "[GitRepository] Unstaged: %s\n", filePath.c_str());
        return true;
    }

    bool commit(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return false;
        
        std::string escapedMsg = message;
        size_t pos = 0;
        while ((pos = escapedMsg.find("\"", pos)) != std::string::npos) {
            escapedMsg.replace(pos, 1, "\\\"");
            pos += 2;
        }
        
        std::string cmd = "commit -m \"" + escapedMsg + "\"";
        std::string output = executeGitCommand(cmd);
        
        fprintf(stderr, "[GitRepository] Committed: %s\n", message.c_str());
        fprintf(stderr, "%s\n", output.c_str());
        
        return true;
    }

    std::string getDiff(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return "";
        std::string cmd = "diff \"" + filePath + "\"";
        return executeGitCommand(cmd);
    }

    std::string getBlame(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return "";
        std::string cmd = "blame \"" + filePath + "\"";
        return executeGitCommand(cmd);
    }

    std::string getLog(int maxCount = 50) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return "";
        char buf[64];
        snprintf(buf, sizeof(buf), "log --oneline --max-count=%d", maxCount);
        return executeGitCommand(buf);
    }

    bool createBranch(const std::string& branchName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return false;
        std::string cmd = "branch \"" + branchName + "\"";
        executeGitCommand(cmd);
        fprintf(stderr, "[GitRepository] Created branch: %s\n", branchName.c_str());
        return true;
    }

    bool switchBranch(const std::string& branchName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return false;
        std::string cmd = "checkout \"" + branchName + "\"";
        executeGitCommand(cmd);
        m_currentBranch = getCurrentBranch();
        fprintf(stderr, "[GitRepository] Switched to branch: %s\n", branchName.c_str());
        return true;
    }

private:
    bool findGitExecutable() {
#ifdef _WIN32
        char pathBuf[MAX_PATH];
        if (SearchPathA(NULL, "git.exe", NULL, MAX_PATH, pathBuf, NULL) > 0) {
            m_gitPath = pathBuf;
            return true;
        }
        
        const char* commonPaths[] = {
            "C:\\Program Files\\Git\\bin\\git.exe",
            "C:\\Program Files (x86)\\Git\\bin\\git.exe",
            "C:\\Git\\bin\\git.exe"
        };
        
        for (const char* path : commonPaths) {
            if (fs::exists(path)) {
                m_gitPath = path;
                return true;
            }
        }
#else
        m_gitPath = "git";
        return true;
#endif
        return false;
    }

    bool isGitRepository() {
        fs::path gitDir = fs::path(m_repoPath) / ".git";
        return fs::exists(gitDir);
    }

    std::string getCurrentBranch() {
        std::string output = executeGitCommand("branch --show-current");
        if (!output.empty() && output.back() == '\n') {
            output.pop_back();
        }
        return output.empty() ? "main" : output;
    }

    std::string executeGitCommand(const std::string& args) {
#ifdef _WIN32
        std::string cmdLine = "\"" + m_gitPath + "\" " + args;
        fprintf(stderr, "[GitRepository] Executing: %s\n", cmdLine.c_str());
        
        HANDLE hReadPipe, hWritePipe;
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return "";
        
        STARTUPINFOA si = {sizeof(si)};
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        
        PROCESS_INFORMATION pi = {};
        char cmdBuf[4096];
        strncpy_s(cmdBuf, cmdLine.c_str(), _TRUNCATE);
        
        if (!CreateProcessA(NULL, cmdBuf, NULL, NULL, TRUE, 0, NULL, m_repoPath.c_str(), &si, &pi)) {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return "";
        }
        
        CloseHandle(hWritePipe);
        
        std::string output;
        char buf[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            output.append(buf);
        }
        
        CloseHandle(hReadPipe);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        return output;
#else
        return "";
#endif
    }

    GitStatus parseStatusCode(char code) {
        switch (code) {
            case ' ': return GitStatus::Unmodified;
            case 'M': return GitStatus::Modified;
            case 'A': return GitStatus::Added;
            case 'D': return GitStatus::Deleted;
            case 'R': return GitStatus::Renamed;
            case 'C': return GitStatus::Copied;
            case '?': return GitStatus::Untracked;
            case '!': return GitStatus::Ignored;
            default: return GitStatus::Unmodified;
        }
    }
};

GitRepository::GitRepository() : m_impl(std::make_unique<GitRepositoryImpl>()) {}
GitRepository::~GitRepository() = default;
bool GitRepository::initialize(const std::string& repoPath) { return m_impl->initialize(repoPath); }
std::string GitRepository::getBranchName() const { return m_impl->getBranchName(); }
std::vector<GitFileStatus> GitRepository::getStatus() { return m_impl->getStatus(); }
bool GitRepository::stageFile(const std::string& filePath) { return m_impl->stageFile(filePath); }
bool GitRepository::unstageFile(const std::string& filePath) { return m_impl->unstageFile(filePath); }
bool GitRepository::commit(const std::string& message) { return m_impl->commit(message); }
std::string GitRepository::getDiff(const std::string& filePath) { return m_impl->getDiff(filePath); }
std::string GitRepository::getBlame(const std::string& filePath) { return m_impl->getBlame(filePath); }
std::string GitRepository::getLog(int maxCount) { return m_impl->getLog(maxCount); }
bool GitRepository::createBranch(const std::string& branchName) { return m_impl->createBranch(branchName); }
bool GitRepository::switchBranch(const std::string& branchName) { return m_impl->switchBranch(branchName); }

} // namespace IDE
} // namespace RawrXD

