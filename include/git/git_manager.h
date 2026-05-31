#pragma once
/**
 * @file git_manager.h
 * @brief Git integration for version control
 * Batch 5 - Item 61: Git manager
 */

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <future>

namespace RawrXD::Git {

enum class GitStatus {
    Unmodified,
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Untracked,
    Ignored,
    Conflicted
};

struct FileStatus {
    std::string path;
    GitStatus status;
    GitStatus headStatus;
    GitStatus worktreeStatus;
    int similarityScore;
};

struct CommitInfo {
    std::string hash;
    std::string shortHash;
    std::string message;
    std::string author;
    std::string email;
    std::chrono::system_clock::time_point date;
    std::vector<std::string> parents;
};

struct BranchInfo {
    std::string name;
    std::string remote;
    std::string upstream;
    bool isCurrent;
    bool isRemote;
    int ahead;
    int behind;
};

struct DiffHunk {
    int oldStart;
    int oldLines;
    int newStart;
    int newLines;
    std::vector<std::string> lines;
};

struct Diff {
    std::string oldPath;
    std::string newPath;
    GitStatus status;
    int similarity;
    std::vector<DiffHunk> hunks;
};

struct StashInfo {
    int index;
    std::string message;
    std::string hash;
};

struct BlameLine {
    std::string commitHash;
    std::string author;
    std::chrono::system_clock::time_point date;
    std::string line;
    bool isBoundary;
};

class GitManager {
public:
    GitManager();
    ~GitManager();

    // Initialization
    bool initialize(const std::string& repoPath);
    void shutdown();
    bool isRepository() const;
    std::string getRepositoryPath() const;

    // Repository operations
    bool initRepository(const std::string& path);
    bool cloneRepository(const std::string& url, const std::string& path,
                         std::function<void(const std::string&)> progress = nullptr);
    bool openRepository(const std::string& path);

    // Status
    std::vector<FileStatus> getStatus();
    GitStatus getFileStatus(const std::string& path);
    bool isIgnored(const std::string& path);

    // Staging
    bool stageFile(const std::string& path);
    bool unstageFile(const std::string& path);
    bool stageAll();
    bool unstageAll();
    bool stagePattern(const std::string& pattern);

    // Commit
    bool commit(const std::string& message, bool amend = false);
    bool commit(const std::string& message, const std::vector<std::string>& files);
    std::optional<CommitInfo> getLastCommit();
    std::vector<CommitInfo> getLog(int maxCount = 100);
    std::vector<CommitInfo> getLog(const std::string& filePath, int maxCount = 100);

    // Branches
    std::vector<BranchInfo> getBranches();
    std::optional<BranchInfo> getCurrentBranch();
    bool createBranch(const std::string& name, const std::string& startPoint = "");
    bool deleteBranch(const std::string& name, bool force = false);
    bool checkoutBranch(const std::string& name, bool create = false);
    bool renameBranch(const std::string& oldName, const std::string& newName);
    bool mergeBranch(const std::string& branchName, bool noFastForward = false);

    // Remotes
    std::vector<std::string> getRemotes();
    bool addRemote(const std::string& name, const std::string& url);
    bool removeRemote(const std::string& name);
    bool renameRemote(const std::string& oldName, const std::string& newName);
    bool setRemoteUrl(const std::string& name, const std::string& url);
    std::string getRemoteUrl(const std::string& name);

    // Push/Pull/Fetch
    bool push(const std::string& remote = "", const std::string& branch = "",
              bool force = false, bool forceWithLease = false);
    bool pull(const std::string& remote = "", const std::string& branch = "",
              bool rebase = false);
    bool fetch(const std::string& remote = "",
               std::function<void(const std::string&)> progress = nullptr);

    // Diff
    std::optional<Diff> getDiff(const std::string& path);
    std::vector<Diff> getStagedDiff();
    std::vector<Diff> getUnstagedDiff();
    std::optional<Diff> getCommitDiff(const std::string& commitHash);
    std::optional<Diff> getCommitDiff(const std::string& oldCommit, const std::string& newCommit);

    // Blame
    std::vector<BlameLine> blame(const std::string& path, const std::string& commit = "");

    // Stash
    bool stash(const std::string& message = "");
    bool stashPop(int index = 0);
    bool stashApply(int index = 0);
    bool stashDrop(int index = 0);
    bool stashClear();
    std::vector<StashInfo> getStashes();

    // Tags
    bool createTag(const std::string& name, const std::string& message = "",
                   const std::string& commit = "");
    bool deleteTag(const std::string& name);
    bool pushTag(const std::string& name);
    std::vector<std::string> getTags();

    // Configuration
    std::string getConfig(const std::string& key);
    bool setConfig(const std::string& key, const std::string& value, bool global = false);

    // Events
    using StatusChangeCallback = std::function<void()>;
    using BranchChangeCallback = std::function<void(const std::string& branch)>;
    void onStatusChanged(StatusChangeCallback callback);
    void onBranchChanged(BranchChangeCallback callback);

private:
    std::string m_repoPath;
    void* m_repository{nullptr};
    StatusChangeCallback m_statusCallback;
    BranchChangeCallback m_branchCallback;
    mutable std::mutex m_mutex;

    bool executeGitCommand(const std::vector<std::string>& args,
                            std::string& output,
                            std::string& error);
    void notifyStatusChanged();
    void notifyBranchChanged(const std::string& branch);
};

// Global instance
GitManager& getGitManager();

} // namespace RawrXD::Git
