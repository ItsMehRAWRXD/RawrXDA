#pragma once
#ifndef GIT_INTEGRATION_H
#define GIT_INTEGRATION_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>

namespace RawrXD {

// Git file status enum
enum class GitStatus {
    Unmodified,
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Unmerged,
    Untracked,
    Ignored
};

// Git file entry
struct GitFileEntry {
    std::string path;
    GitStatus status;
    std::string oldPath;  // For renames
};

// Git commit info
struct GitCommit {
    std::string hash;
    std::string author;
    std::string email;
    std::string message;
    std::time_t timestamp;
    std::vector<std::string> parents;
};

// Git branch info
struct GitBranch {
    std::string name;
    bool isHead;
    bool isRemote;
    std::string upstream;
};

// Git diff line
struct GitDiffLine {
    enum class Type { Context, Addition, Deletion, Header };
    Type type;
    int oldLineNo;
    int newLineNo;
    std::string content;
};

// Git diff hunk
struct GitDiffHunk {
    int oldStart;
    int oldLines;
    int newStart;
    int newLines;
    std::string header;
    std::vector<GitDiffLine> lines;
};

// Git diff for a file
struct GitFileDiff {
    std::string path;
    GitStatus status;
    std::vector<GitDiffHunk> hunks;
};

// Git integration interface
class GitIntegration {
public:
    virtual ~GitIntegration() = default;

    // Repository operations
    virtual bool isRepository(const std::string& path) = 0;
    virtual bool initRepository(const std::string& path) = 0;
    virtual std::optional<std::string> getRepositoryRoot(const std::string& path) = 0;

    // Status operations
    virtual std::vector<GitFileEntry> getStatus() = 0;
    virtual GitStatus getFileStatus(const std::string& path) = 0;
    virtual void refreshStatus() = 0;

    // Diff operations
    virtual std::optional<GitFileDiff> getDiff(const std::string& path) = 0;
    virtual std::optional<GitFileDiff> getStagedDiff(const std::string& path) = 0;
    virtual std::vector<GitFileDiff> getAllDiffs() = 0;

    // Staging operations
    virtual bool stageFile(const std::string& path) = 0;
    virtual bool unstageFile(const std::string& path) = 0;
    virtual bool stageAll() = 0;
    virtual bool unstageAll() = 0;

    // Commit operations
    virtual bool commit(const std::string& message, const std::string& author = "", const std::string& email = "") = 0;
    virtual std::vector<GitCommit> getCommitHistory(int limit = 100) = 0;
    virtual std::optional<GitCommit> getCommit(const std::string& hash) = 0;

    // Branch operations
    virtual std::vector<GitBranch> getBranches() = 0;
    virtual std::optional<GitBranch> getCurrentBranch() = 0;
    virtual bool checkoutBranch(const std::string& name) = 0;
    virtual bool createBranch(const std::string& name) = 0;
    virtual bool deleteBranch(const std::string& name) = 0;

    // Remote operations
    virtual bool fetch(const std::string& remote = "origin") = 0;
    virtual bool pull(const std::string& remote = "origin") = 0;
    virtual bool push(const std::string& remote = "origin") = 0;

    // Async callback types
    using StatusCallback = std::function<void(const std::vector<GitFileEntry>&)>;
    using DiffCallback = std::function<void(const GitFileDiff&)>;
    using CommitCallback = std::function<void(bool success, const std::string& error)>;

    // Async operations
    virtual void getStatusAsync(StatusCallback callback) = 0;
    virtual void commitAsync(const std::string& message, CommitCallback callback) = 0;
};

// Git CLI implementation (uses git.exe)
class GitCLI : public GitIntegration {
public:
    explicit GitCLI(const std::string& repoPath);
    ~GitCLI() override = default;

    bool isRepository(const std::string& path) override;
    bool initRepository(const std::string& path) override;
    std::optional<std::string> getRepositoryRoot(const std::string& path) override;

    std::vector<GitFileEntry> getStatus() override;
    GitStatus getFileStatus(const std::string& path) override;
    void refreshStatus() override;

    std::optional<GitFileDiff> getDiff(const std::string& path) override;
    std::optional<GitFileDiff> getStagedDiff(const std::string& path) override;
    std::vector<GitFileDiff> getAllDiffs() override;

    bool stageFile(const std::string& path) override;
    bool unstageFile(const std::string& path) override;
    bool stageAll() override;
    bool unstageAll() override;

    bool commit(const std::string& message, const std::string& author, const std::string& email) override;
    std::vector<GitCommit> getCommitHistory(int limit) override;
    std::optional<GitCommit> getCommit(const std::string& hash) override;

    std::vector<GitBranch> getBranches() override;
    std::optional<GitBranch> getCurrentBranch() override;
    bool checkoutBranch(const std::string& name) override;
    bool createBranch(const std::string& name) override;
    bool deleteBranch(const std::string& name) override;

    bool fetch(const std::string& remote) override;
    bool pull(const std::string& remote) override;
    bool push(const std::string& remote) override;

    void getStatusAsync(StatusCallback callback) override;
    void commitAsync(const std::string& message, CommitCallback callback) override;

private:
    std::string repoPath_;
    std::vector<GitFileEntry> cachedStatus_;
    std::time_t lastStatusRefresh_;

    std::string runGitCommand(const std::vector<std::string>& args);
    std::vector<std::string> splitLines(const std::string& output);
    GitStatus parseStatusCode(const std::string& code);
    GitDiffLine::Type parseDiffLineType(const std::string& line);
};

} // namespace RawrXD

#endif // GIT_INTEGRATION_H
