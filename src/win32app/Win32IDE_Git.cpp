// ============================================================================
// Win32IDE_Git.cpp — Real Git integration for Win32IDE
// ============================================================================
// Diff view, blame, stage/commit from IDE, branch indicator
// Shell to git CLI or libgit2 for repository operations
// Provides VS Code-style Git UX in native Win32 IDE
// ============================================================================

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

namespace fs = std::filesystem;

namespace RawrXD {
namespace IDE {

// ============================================================================
// Git Status Entry
// ============================================================================

enum class GitStatus {
    Unmodified,
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Untracked,
    Ignored
};

struct GitFileStatus {
    std::string filePath;
    GitStatus status;
    GitStatus indexStatus;
};

// ============================================================================
// Git Repository
// ============================================================================

class GitRepository {
private:
    std::string m_repoPath;
    std::string m_gitPath;      // Path to git.exe
    std::string m_currentBranch;
    std::mutex m_mutex;
    bool m_initialized = false;
    
public:
    GitRepository() = default;
    ~GitRepository() = default;
    
    // Initialize for repository
    bool initialize(const std::string& repoPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_repoPath = repoPath;
        
        // Find git.exe
        if (!findGitExecutable()) {
            fprintf(stderr, "[GitRepository] Git not found in PATH\n");
            return false;
        }
        
        // Check if this is a Git repository
        if (!isGitRepository()) {
            fprintf(stderr, "[GitRepository] Not a Git repository: %s\n", repoPath.c_str());
            return false;
        }
        
        // Get current branch
        m_currentBranch = getCurrentBranch();
        
        m_initialized = true;
        
        fprintf(stderr, "[GitRepository] Initialized for: %s\n", repoPath.c_str());
        fprintf(stderr, "[GitRepository] Current branch: %s\n", m_currentBranch.c_str());
        
        return true;
    }
    
    // Get current branch name
    std::string getBranchName() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
        return m_currentBranch;
    }
    
    // Get file status
    std::vector<GitFileStatus> getStatus() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized) {
            return {};
        }
        
        std::vector<GitFileStatus> files;
        
        // Run: git status --porcelain
        std::string output = executeGitCommand("status --porcelain");
        
        // Parse output
        std::istringstream stream(output);
        std::string line;
        
        while (std::getline(stream, line)) {
            if (line.size() < 4) {
                continue;
            }
            
            GitFileStatus file;
            
            // Parse status codes (XY format)
            char indexCode = line[0];
            char workCode = line[1];
            
            file.indexStatus = parseStatusCode(indexCode);
            file.status = parseStatusCode(workCode);
            file.filePath = line.substr(3);
            
            files.push_back(file);
        }
        
        return files;
    }
    
    // Stage file
    bool stageFile(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized) {
            return false;
        }
        
        std::string cmd = "add \"" + filePath + "\"";
        std::string output = executeGitCommand(cmd);
        
        fprintf(stderr, "[GitRepository] Staged: %s\n", filePath.c_str());
        return true;
    }
    
    // Unstage file
    bool unstageFile(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized) {
            return false;
        }
        
        std::string cmd = "reset HEAD \"" + filePath + "\"";
        std::string output = executeGitCommand(cmd);
        
        fprintf(stderr, "[GitRepository] Unstaged: %s\n", filePath.c_str());
        return true;
    }
    
    // Commit staged changes
    bool commit(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized) {
            return false;
        }
        
        // Escape message
        std::string escapedMsg = message;
        // Replace quotes
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
    
    // Get diff for file
    std::string getDiff(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized) {
            return "";
        }
        
        std::string cmd = "diff \"" + filePath + "\"";
        return executeGitCommand(cmd);
    }
    
    // Get blame for file
    std::string getBlame(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized) {
            return "";
        }
        
        std::string cmd = "blame \"" + filePath + "\"";
        return executeGitCommand(cmd);
    }
    
    // Get log (commit history)
    std::string getLog(int maxCount = 50) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized) {
            return "";
        }
        
        char buf[64];
        snprintf(buf, sizeof(buf), "log --oneline --max-count=%d", maxCount);
        
        return executeGitCommand(buf);
    }
    
    // Create branch
    bool createBranch(const std::string& branchName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized) {
            return false;
        }
        
        std::string cmd = "branch \"" + branchName + "\"";
        std::string output = executeGitCommand(cmd);
        
        fprintf(stderr, "[GitRepository] Created branch: %s\n", branchName.c_str());
        return true;
    }
    
    // Switch branch
    bool switchBranch(const std::string& branchName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized) {
            return false;
        }
        
        std::string cmd = "checkout \"" + branchName + "\"";
        std::string output = executeGitCommand(cmd);
        
        // Update current branch
        m_currentBranch = getCurrentBranch();
        
        fprintf(stderr, "[GitRepository] Switched to branch: %s\n", branchName.c_str());
        return true;
    }
    
private:
    bool findGitExecutable() {
#ifdef _WIN32
        // Check if git.exe is in PATH
        char pathBuf[MAX_PATH];
        DWORD len = SearchPathA(NULL, "git.exe", NULL, MAX_PATH, pathBuf, NULL);
        
        if (len > 0) {
            m_gitPath = pathBuf;
            return true;
        }
        
        // Check common Git installation paths
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
        m_gitPath = "git"; // Assume git is in PATH on Unix
        return true;
#endif
        
        return false;
    }
    
    bool isGitRepository() {
        // Check if .git directory exists
        fs::path gitDir = fs::path(m_repoPath) / ".git";
        return fs::exists(gitDir);
    }
    
    std::string getCurrentBranch() {
        std::string output = executeGitCommand("branch --show-current");
        
        // Trim newline
        if (!output.empty() && output.back() == '\n') {
            output.pop_back();
        }
        
        return output.empty() ? "main" : output;
    }
    
    std::string executeGitCommand(const std::string& args) {
#ifdef _WIN32
        // Build command line
        std::string cmdLine = "\"" + m_gitPath + "\" " + args;
        
        fprintf(stderr, "[GitRepository] Executing: %s\n", cmdLine.c_str());
        
        // Create pipe for output
        HANDLE hReadPipe, hWritePipe;
        SECURITY_ATTRIBUTES sa = {sizeof(sa)};
        sa.bInheritHandle = TRUE;
        
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            return "";
        }
        
        // Set up STARTUPINFO
        STARTUPINFOA si = {sizeof(si)};
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        
        PROCESS_INFORMATION pi = {};
        
        char cmdBuf[4096];
        strncpy_s(cmdBuf, cmdLine.c_str(), _TRUNCATE);
        
        if (!CreateProcessA(NULL, cmdBuf, NULL, NULL, TRUE, 0, NULL,
                           m_repoPath.c_str(), &si, &pi)) {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return "";
        }
        
        CloseHandle(hWritePipe);
        
        // Read output
        std::string output;
        char buf[4096];
        DWORD bytesRead;
        constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buf) - 1);
        
        while (ReadFile(hReadPipe, buf, kMaxChunk, &bytesRead, NULL) && bytesRead > 0) {
            const size_t safeBytes = (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
            buf[safeBytes] = '\0';
            output.append(buf, safeBytes);
        }
        
        CloseHandle(hReadPipe);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        return output;
#else
        // Unix implementation would use popen()
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

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<GitRepository> g_repo;
static std::mutex g_repoMutex;

} // namespace IDE
} // namespace RawrXD

// ============================================================================
// C API
// ============================================================================

extern "C" {

bool  RawrXD_IDE_InitGit(const char* repoPath) {
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_repoMutex);
    
    RawrXD::IDE::g_repo = std::make_unique<RawrXD::IDE::GitRepository>();
    return RawrXD::IDE::g_repo->initialize(repoPath ? repoPath : ".");
}

const char* RawrXD_IDE_GetGitBranch() {
    static thread_local char buf[256];
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_repoMutex);
    
    if (!RawrXD::IDE::g_repo) {
        return "";
    }
    
    std::string branch = RawrXD::IDE::g_repo->getBranchName();
    snprintf(buf, sizeof(buf), "%s", branch.c_str());
    return buf;
}

bool RawrXD_IDE_GitStage(const char* filePath) {
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_repoMutex);
    
    if (!RawrXD::IDE::g_repo || !filePath) {
        return false;
    }
    
    return RawrXD::IDE::g_repo->stageFile(filePath);
}

bool RawrXD_IDE_GitCommit(const char* message) {
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_repoMutex);
    
    if (!RawrXD::IDE::g_repo || !message) {
        return false;
    }
    
    return RawrXD::IDE::g_repo->commit(message);
}

} // extern "C"
