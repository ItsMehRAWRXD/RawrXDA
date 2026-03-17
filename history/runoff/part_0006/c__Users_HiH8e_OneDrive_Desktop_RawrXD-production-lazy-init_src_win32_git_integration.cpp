#include "git_integration.h"
#include <sstream>
#include <algorithm>
#include <thread>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {

GitCLI::GitCLI(const std::string& repoPath)
    : repoPath_(repoPath), lastStatusRefresh_(0) {
}

std::string GitCLI::runGitCommand(const std::vector<std::string>& args) {
    std::ostringstream cmd;
    cmd << "git";
    for (const auto& arg : args) {
        cmd << " \"" << arg << "\"";
    }

#ifdef _WIN32
    HANDLE hStdoutRead, hStdoutWrite;
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        return "";
    }

    STARTUPINFOA si{sizeof(STARTUPINFOA)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    
    PROCESS_INFORMATION pi{};
    std::string cmdLine = cmd.str();
    
    if (!CreateProcessA(nullptr, const_cast<char*>(cmdLine.c_str()), nullptr, nullptr,
                        TRUE, CREATE_NO_WINDOW, nullptr, repoPath_.c_str(), &si, &pi)) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        return "";
    }

    CloseHandle(hStdoutWrite);
    
    std::string output;
    char buffer[4096];
    DWORD bytesRead;
    
    while (ReadFile(hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdoutRead);
    
    return output;
#else
    FILE* pipe = popen((cmd.str() + " 2>&1").c_str(), "r");
    if (!pipe) return "";
    
    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    
    pclose(pipe);
    return output;
#endif
}

std::vector<std::string> GitCLI::splitLines(const std::string& output) {
    std::vector<std::string> lines;
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

GitStatus GitCLI::parseStatusCode(const std::string& code) {
    if (code.empty()) return GitStatus::Unmodified;
    
    char x = code[0];
    if (x == 'M') return GitStatus::Modified;
    if (x == 'A') return GitStatus::Added;
    if (x == 'D') return GitStatus::Deleted;
    if (x == 'R') return GitStatus::Renamed;
    if (x == 'C') return GitStatus::Copied;
    if (x == 'U') return GitStatus::Unmerged;
    if (x == '?') return GitStatus::Untracked;
    if (x == '!') return GitStatus::Ignored;
    
    return GitStatus::Unmodified;
}

bool GitCLI::isRepository(const std::string& path) {
    auto result = runGitCommand({"-C", path, "rev-parse", "--git-dir"});
    return !result.empty();
}

bool GitCLI::initRepository(const std::string& path) {
    auto result = runGitCommand({"-C", path, "init"});
    return result.find("Initialized") != std::string::npos;
}

std::optional<std::string> GitCLI::getRepositoryRoot(const std::string& path) {
    auto result = runGitCommand({"-C", path, "rev-parse", "--show-toplevel"});
    if (result.empty()) return std::nullopt;
    
    // Remove trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

std::vector<GitFileEntry> GitCLI::getStatus() {
    auto output = runGitCommand({"status", "--porcelain"});
    auto lines = splitLines(output);
    
    std::vector<GitFileEntry> entries;
    for (const auto& line : lines) {
        if (line.length() < 4) continue;
        
        GitFileEntry entry;
        entry.status = parseStatusCode(line.substr(0, 2));
        entry.path = line.substr(3);
        
        // Handle renames
        if (entry.status == GitStatus::Renamed) {
            size_t arrow = entry.path.find(" -> ");
            if (arrow != std::string::npos) {
                entry.oldPath = entry.path.substr(0, arrow);
                entry.path = entry.path.substr(arrow + 4);
            }
        }
        
        entries.push_back(entry);
    }
    
    cachedStatus_ = entries;
    lastStatusRefresh_ = std::time(nullptr);
    return entries;
}

GitStatus GitCLI::getFileStatus(const std::string& path) {
    for (const auto& entry : cachedStatus_) {
        if (entry.path == path) {
            return entry.status;
        }
    }
    return GitStatus::Unmodified;
}

void GitCLI::refreshStatus() {
    getStatus();
}

bool GitCLI::stageFile(const std::string& path) {
    auto result = runGitCommand({"add", path});
    return result.find("fatal") == std::string::npos;
}

bool GitCLI::unstageFile(const std::string& path) {
    auto result = runGitCommand({"reset", "HEAD", path});
    return result.find("fatal") == std::string::npos;
}

bool GitCLI::stageAll() {
    auto result = runGitCommand({"add", "-A"});
    return result.find("fatal") == std::string::npos;
}

bool GitCLI::unstageAll() {
    auto result = runGitCommand({"reset", "HEAD"});
    return result.find("fatal") == std::string::npos;
}

bool GitCLI::commit(const std::string& message, const std::string& author, const std::string& email) {
    std::vector<std::string> args = {"commit", "-m", message};
    
    if (!author.empty() && !email.empty()) {
        args.push_back("--author");
        args.push_back(author + " <" + email + ">");
    }
    
    auto result = runGitCommand(args);
    return result.find("fatal") == std::string::npos;
}

std::vector<GitCommit> GitCLI::getCommitHistory(int limit) {
    std::vector<std::string> args = {"log", "--format=%H%n%an%n%ae%n%at%n%s%n%P%n---", "-n", std::to_string(limit)};
    auto output = runGitCommand(args);
    auto lines = splitLines(output);
    
    std::vector<GitCommit> commits;
    GitCommit current;
    int field = 0;
    
    for (const auto& line : lines) {
        if (line == "---") {
            commits.push_back(current);
            current = GitCommit{};
            field = 0;
            continue;
        }
        
        switch (field) {
            case 0: current.hash = line; break;
            case 1: current.author = line; break;
            case 2: current.email = line; break;
            case 3: current.timestamp = std::stol(line); break;
            case 4: current.message = line; break;
            case 5: {
                std::istringstream ss(line);
                std::string parent;
                while (ss >> parent) {
                    current.parents.push_back(parent);
                }
                break;
            }
        }
        field++;
    }
    
    return commits;
}

std::optional<GitCommit> GitCLI::getCommit(const std::string& hash) {
    auto commits = getCommitHistory(1);
    return commits.empty() ? std::nullopt : std::make_optional(commits[0]);
}

std::optional<GitFileDiff> GitCLI::getDiff(const std::string& path) {
    auto output = runGitCommand({"diff", path});
    if (output.empty()) return std::nullopt;
    
    GitFileDiff diff;
    diff.path = path;
    // Parse diff output (simplified)
    return diff;
}

std::optional<GitFileDiff> GitCLI::getStagedDiff(const std::string& path) {
    auto output = runGitCommand({"diff", "--cached", path});
    if (output.empty()) return std::nullopt;
    
    GitFileDiff diff;
    diff.path = path;
    return diff;
}

std::vector<GitFileDiff> GitCLI::getAllDiffs() {
    // Simplified implementation
    return {};
}

std::vector<GitBranch> GitCLI::getBranches() {
    auto output = runGitCommand({"branch", "-a"});
    auto lines = splitLines(output);
    
    std::vector<GitBranch> branches;
    for (const auto& line : lines) {
        GitBranch branch;
        branch.isHead = line[0] == '*';
        branch.name = branch.isHead ? line.substr(2) : line.substr(2);
        branch.isRemote = branch.name.find("remotes/") == 0;
        branches.push_back(branch);
    }
    
    return branches;
}

std::optional<GitBranch> GitCLI::getCurrentBranch() {
    auto output = runGitCommand({"branch", "--show-current"});
    if (output.empty()) return std::nullopt;
    
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    
    GitBranch branch;
    branch.name = output;
    branch.isHead = true;
    branch.isRemote = false;
    return branch;
}

bool GitCLI::checkoutBranch(const std::string& name) {
    auto result = runGitCommand({"checkout", name});
    return result.find("fatal") == std::string::npos;
}

bool GitCLI::createBranch(const std::string& name) {
    auto result = runGitCommand({"branch", name});
    return result.find("fatal") == std::string::npos;
}

bool GitCLI::deleteBranch(const std::string& name) {
    auto result = runGitCommand({"branch", "-d", name});
    return result.find("fatal") == std::string::npos;
}

bool GitCLI::fetch(const std::string& remote) {
    auto result = runGitCommand({"fetch", remote});
    return result.find("fatal") == std::string::npos;
}

bool GitCLI::pull(const std::string& remote) {
    auto result = runGitCommand({"pull", remote});
    return result.find("fatal") == std::string::npos;
}

bool GitCLI::push(const std::string& remote) {
    auto result = runGitCommand({"push", remote});
    return result.find("fatal") == std::string::npos;
}

void GitCLI::getStatusAsync(StatusCallback callback) {
    std::thread([this, callback]() {
        auto status = getStatus();
        callback(status);
    }).detach();
}

void GitCLI::commitAsync(const std::string& message, CommitCallback callback) {
    std::thread([this, message, callback]() {
        bool success = commit(message, "", "");
        callback(success, success ? "" : "Commit failed");
    }).detach();
}

} // namespace RawrXD
