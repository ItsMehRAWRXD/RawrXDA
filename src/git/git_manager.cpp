#include "git/git_manager.h"
#include <process.h>
#include <sstream>

namespace RawrXD::Git {

GitManager::GitManager() = default;
GitManager::~GitManager() {
    shutdown();
}

bool GitManager::initialize(const std::string& repoPath) {
    m_repoPath = repoPath;
    return isRepository();
}

void GitManager::shutdown() {
    m_repoPath.clear();
}

bool GitManager::isRepository() const {
    std::string output, error;
    return executeGitCommand({"rev-parse", "--git-dir"}, output, error);
}

std::string GitManager::getRepositoryPath() const {
    return m_repoPath;
}

bool GitManager::initRepository(const std::string& path) {
    std::string output, error;
    bool result = executeGitCommand({"init", path}, output, error);
    if (result) {
        m_repoPath = path;
    }
    return result;
}

bool GitManager::cloneRepository(const std::string& url, const std::string& path,
                                  std::function<void(const std::string&)> progress) {
    std::string output, error;
    // Clone with progress callback would require libgit2 or similar
    bool result = executeGitCommand({"clone", url, path}, output, error);
    return result;
}

bool GitManager::openRepository(const std::string& path) {
    m_repoPath = path;
    return isRepository();
}

std::vector<FileStatus> GitManager::getStatus() {
    std::vector<FileStatus> statuses;
    std::string output, error;

    if (!executeGitCommand({"status", "--porcelain"}, output, error)) {
        return statuses;
    }

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.length() < 3) continue;

        FileStatus status;
        char indexStatus = line[0];
        char worktreeStatus = line[1];
        status.path = line.substr(3);

        // Parse status codes
        switch (indexStatus) {
            case 'M': status.headStatus = GitStatus::Modified; break;
            case 'A': status.headStatus = GitStatus::Added; break;
            case 'D': status.headStatus = GitStatus::Deleted; break;
            case 'R': status.headStatus = GitStatus::Renamed; break;
            case 'C': status.headStatus = GitStatus::Copied; break;
            default: status.headStatus = GitStatus::Unmodified; break;
        }

        switch (worktreeStatus) {
            case 'M': status.worktreeStatus = GitStatus::Modified; break;
            case 'A': status.worktreeStatus = GitStatus::Added; break;
            case 'D': status.worktreeStatus = GitStatus::Deleted; break;
            case '?': status.worktreeStatus = GitStatus::Untracked; break;
            case '!': status.worktreeStatus = GitStatus::Ignored; break;
            default: status.worktreeStatus = GitStatus::Unmodified; break;
        }

        status.status = status.worktreeStatus != GitStatus::Unmodified ?
            status.worktreeStatus : status.headStatus;

        statuses.push_back(status);
    }

    return statuses;
}

GitStatus GitManager::getFileStatus(const std::string& path) {
    auto statuses = getStatus();
    for (const auto& status : statuses) {
        if (status.path == path) {
            return status.status;
        }
    }
    return GitStatus::Unmodified;
}

bool GitManager::isIgnored(const std::string& path) {
    std::string output, error;
    if (!executeGitCommand({"check-ignore", path}, output, error)) {
        return false;
    }
    return !output.empty();
}

bool GitManager::stageFile(const std::string& path) {
    std::string output, error;
    return executeGitCommand({"add", path}, output, error);
}

bool GitManager::unstageFile(const std::string& path) {
    std::string output, error;
    return executeGitCommand({"reset", "HEAD", path}, output, error);
}

bool GitManager::stageAll() {
    std::string output, error;
    return executeGitCommand({"add", "."}, output, error);
}

bool GitManager::unstageAll() {
    std::string output, error;
    return executeGitCommand({"reset", "HEAD"}, output, error);
}

bool GitManager::stagePattern(const std::string& pattern) {
    std::string output, error;
    return executeGitCommand({"add", pattern}, output, error);
}

bool GitManager::commit(const std::string& message, bool amend) {
    std::string output, error;
    if (amend) {
        return executeGitCommand({"commit", "--amend", "-m", message}, output, error);
    }
    return executeGitCommand({"commit", "-m", message}, output, error);
}

bool GitManager::commit(const std::string& message, const std::vector<std::string>& files) {
    for (const auto& file : files) {
        if (!stageFile(file)) return false;
    }
    return commit(message, false);
}

std::optional<CommitInfo> GitManager::getLastCommit() {
    auto log = getLog(1);
    if (!log.empty()) {
        return log[0];
    }
    return std::nullopt;
}

std::vector<CommitInfo> GitManager::getLog(int maxCount) {
    std::vector<CommitInfo> commits;
    std::string output, error;

    std::string format = "%H|%h|%s|%an|%ae|%ad|%p";
    if (!executeGitCommand({"log", "-" + std::to_string(maxCount),
                           "--pretty=format:" + format}, output, error)) {
        return commits;
    }

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        std::istringstream lineStream(line);
        std::string token;
        CommitInfo commit;

        std::getline(lineStream, commit.hash, '|');
        std::getline(lineStream, commit.shortHash, '|');
        std::getline(lineStream, commit.message, '|');
        std::getline(lineStream, commit.author, '|');
        std::getline(lineStream, commit.email, '|');
        std::getline(lineStream, token, '|');
        // Parse date
        std::getline(lineStream, token, '|');

        // Parse parents
        std::istringstream parentStream(token);
        std::string parent;
        while (parentStream >> parent) {
            commit.parents.push_back(parent);
        }

        commits.push_back(commit);
    }

    return commits;
}

std::vector<CommitInfo> GitManager::getLog(const std::string& filePath, int maxCount) {
    std::vector<CommitInfo> commits;
    std::string output, error;

    std::string format = "%H|%h|%s|%an|%ae|%ad|%p";
    if (!executeGitCommand({"log", "-" + std::to_string(maxCount),
                           "--pretty=format:" + format, "--", filePath}, output, error)) {
        return commits;
    }

    // Parse similar to getLog()
    return commits;
}

std::vector<BranchInfo> GitManager::getBranches() {
    std::vector<BranchInfo> branches;
    std::string output, error;

    if (!executeGitCommand({"branch", "-vv"}, output, error)) {
        return branches;
    }

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.length() < 2) continue;

        BranchInfo branch;
        branch.isCurrent = (line[0] == '*');
        branch.name = line.substr(2);

        // Parse additional info from -vv output
        size_t pos = branch.name.find('[');
        if (pos != std::string::npos) {
            std::string upstream = branch.name.substr(pos + 1);
            size_t endPos = upstream.find(']');
            if (endPos != std::string::npos) {
                branch.upstream = upstream.substr(0, endPos);
            }
        }

        branches.push_back(branch);
    }

    return branches;
}

std::optional<BranchInfo> GitManager::getCurrentBranch() {
    auto branches = getBranches();
    for (const auto& branch : branches) {
        if (branch.isCurrent) {
            return branch;
        }
    }
    return std::nullopt;
}

bool GitManager::createBranch(const std::string& name, const std::string& startPoint) {
    std::string output, error;
    if (startPoint.empty()) {
        return executeGitCommand({"branch", name}, output, error);
    }
    return executeGitCommand({"branch", name, startPoint}, output, error);
}

bool GitManager::deleteBranch(const std::string& name, bool force) {
    std::string output, error;
    if (force) {
        return executeGitCommand({"branch", "-D", name}, output, error);
    }
    return executeGitCommand({"branch", "-d", name}, output, error);
}

bool GitManager::checkoutBranch(const std::string& name, bool create) {
    std::string output, error;
    if (create) {
        bool result = executeGitCommand({"checkout", "-b", name}, output, error);
        if (result) {
            notifyBranchChanged(name);
        }
        return result;
    }
    bool result = executeGitCommand({"checkout", name}, output, error);
    if (result) {
        notifyBranchChanged(name);
    }
    return result;
}

bool GitManager::renameBranch(const std::string& oldName, const std::string& newName) {
    std::string output, error;
    return executeGitCommand({"branch", "-m", oldName, newName}, output, error);
}

bool GitManager::mergeBranch(const std::string& branchName, bool noFastForward) {
    std::string output, error;
    if (noFastForward) {
        return executeGitCommand({"merge", "--no-ff", branchName}, output, error);
    }
    return executeGitCommand({"merge", branchName}, output, error);
}

std::vector<std::string> GitManager::getRemotes() {
    std::vector<std::string> remotes;
    std::string output, error;

    if (!executeGitCommand({"remote"}, output, error)) {
        return remotes;
    }

    std::istringstream stream(output);
    std::string remote;
    while (stream >> remote) {
        remotes.push_back(remote);
    }

    return remotes;
}

bool GitManager::addRemote(const std::string& name, const std::string& url) {
    std::string output, error;
    return executeGitCommand({"remote", "add", name, url}, output, error);
}

bool GitManager::removeRemote(const std::string& name) {
    std::string output, error;
    return executeGitCommand({"remote", "remove", name}, output, error);
}

bool GitManager::renameRemote(const std::string& oldName, const std::string& newName) {
    std::string output, error;
    return executeGitCommand({"remote", "rename", oldName, newName}, output, error);
}

bool GitManager::setRemoteUrl(const std::string& name, const std::string& url) {
    std::string output, error;
    return executeGitCommand({"remote", "set-url", name, url}, output, error);
}

std::string GitManager::getRemoteUrl(const std::string& name) {
    std::string output, error;
    if (!executeGitCommand({"remote", "get-url", name}, output, error)) {
        return "";
    }
    // Remove trailing newline
    if (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }
    return output;
}

bool GitManager::push(const std::string& remote, const std::string& branch,
                       bool force, bool forceWithLease) {
    std::string output, error;
    std::vector<std::string> args = {"push"};

    if (force) args.push_back("--force");
    if (forceWithLease) args.push_back("--force-with-lease");
    if (!remote.empty()) args.push_back(remote);
    if (!branch.empty()) args.push_back(branch);

    return executeGitCommand(args, output, error);
}

bool GitManager::pull(const std::string& remote, const std::string& branch, bool rebase) {
    std::string output, error;
    std::vector<std::string> args = {"pull"};

    if (rebase) args.push_back("--rebase");
    if (!remote.empty()) args.push_back(remote);
    if (!branch.empty()) args.push_back(branch);

    return executeGitCommand(args, output, error);
}

bool GitManager::fetch(const std::string& remote,
                       std::function<void(const std::string&)> progress) {
    std::string output, error;
    std::vector<std::string> args = {"fetch"};
    if (!remote.empty()) args.push_back(remote);

    return executeGitCommand(args, output, error);
}

bool GitManager::stash(const std::string& message) {
    std::string output, error;
    if (message.empty()) {
        return executeGitCommand({"stash"}, output, error);
    }
    return executeGitCommand({"stash", "save", message}, output, error);
}

bool GitManager::stashPop(int index) {
    std::string output, error;
    return executeGitCommand({"stash", "pop", "stash@{" + std::to_string(index) + "}"}, output, error);
}

bool GitManager::stashApply(int index) {
    std::string output, error;
    return executeGitCommand({"stash", "apply", "stash@{" + std::to_string(index) + "}"}, output, error);
}

bool GitManager::stashDrop(int index) {
    std::string output, error;
    return executeGitCommand({"stash", "drop", "stash@{" + std::to_string(index) + "}"}, output, error);
}

bool GitManager::stashClear() {
    std::string output, error;
    return executeGitCommand({"stash", "clear"}, output, error);
}

std::vector<StashInfo> GitManager::getStashes() {
    std::vector<StashInfo> stashes;
    std::string output, error;

    if (!executeGitCommand({"stash", "list", "--format=%gd|%s"}, output, error)) {
        return stashes;
    }

    std::istringstream stream(output);
    std::string line;
    int index = 0;

    while (std::getline(stream, line)) {
        size_t pos = line.find('|');
        if (pos != std::string::npos) {
            StashInfo stash;
            stash.index = index++;
            stash.message = line.substr(pos + 1);
            stashes.push_back(stash);
        }
    }

    return stashes;
}

std::string GitManager::getConfig(const std::string& key) {
    std::string output, error;
    if (!executeGitCommand({"config", key}, output, error)) {
        return "";
    }
    // Remove trailing newline
    if (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }
    return output;
}

bool GitManager::setConfig(const std::string& key, const std::string& value, bool global) {
    std::string output, error;
    if (global) {
        return executeGitCommand({"config", "--global", key, value}, output, error);
    }
    return executeGitCommand({"config", key, value}, output, error);
}

void GitManager::onStatusChanged(StatusChangeCallback callback) {
    m_statusCallback = callback;
}

void GitManager::onBranchChanged(BranchChangeCallback callback) {
    m_branchCallback = callback;
}

bool GitManager::executeGitCommand(const std::vector<std::string>& args,
                                    std::string& output,
                                    std::string& error) {
    std::string command = "git";
    for (const auto& arg : args) {
        command += " \"" + arg + "\"";
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdoutRead, stdoutWrite;
    HANDLE stderrRead, stderrWrite;

    CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0);
    CreatePipe(&stderrRead, &stderrWrite, &sa, 0);

    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = stdoutWrite;
    si.hStdError = stderrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {0};

    BOOL success = CreateProcessA(nullptr, const_cast<char*>(command.c_str()),
                                   nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                   nullptr, m_repoPath.empty() ? nullptr : m_repoPath.c_str(),
                                   &si, &pi);

    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);

    if (!success) {
        CloseHandle(stdoutRead);
        CloseHandle(stderrRead);
        return false;
    }

    // Read output
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(stdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    // Read error
    while (ReadFile(stderrRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        error += buffer;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(stdoutRead);
    CloseHandle(stderrRead);

    return exitCode == 0;
}

void GitManager::notifyStatusChanged() {
    if (m_statusCallback) {
        m_statusCallback();
    }
}

void GitManager::notifyBranchChanged(const std::string& branch) {
    if (m_branchCallback) {
        m_branchCallback(branch);
    }
}

// Global instance
GitManager& getGitManager() {
    static GitManager manager;
    return manager;
}

} // namespace RawrXD::Git
