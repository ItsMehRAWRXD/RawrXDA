// ============================================================================
// git_context.h — Git Context Provider
// ============================================================================
// Provides git-aware context for agentic prompts: current branch, staged
// changes, recent commits, blame info, and commit message generation.
// Uses git CLI subprocess (no libgit2 dependency).
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>

namespace RawrXD {
namespace Git {

namespace fs = std::filesystem;

// ============================================================================
// Result type
// ============================================================================

struct GitResult {
    bool        success;
    const char* detail;
    int         errorCode;
    std::string output;

    static GitResult ok(std::string out) {
        GitResult r;
        r.success   = true;
        r.detail    = "OK";
        r.errorCode = 0;
        r.output    = std::move(out);
        return r;
    }

    static GitResult error(const char* msg, int code = -1) {
        GitResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Diff Hunk
// ============================================================================

struct DiffHunk {
    std::string file;
    int         oldStart;
    int         oldCount;
    int         newStart;
    int         newCount;
    std::string content;
};

// ============================================================================
// Commit Info
// ============================================================================

struct CommitInfo {
    std::string hash;           // Full SHA
    std::string shortHash;      // 7-char abbreviated
    std::string author;
    std::string email;
    std::string message;
    uint64_t    timestampSec;   // Unix epoch seconds
};

// ============================================================================
// Blame Line
// ============================================================================

struct BlameLine {
    std::string commitHash;
    std::string author;
    uint64_t    timestamp;
    int         lineNumber;
    std::string content;
};

// ============================================================================
// Branch Info
// ============================================================================

struct BranchInfo {
    std::string name;
    bool        isCurrent;
    bool        isRemote;
    std::string upstream;       // e.g. "origin/main"
    int         ahead;          // Commits ahead of upstream
    int         behind;         // Commits behind upstream
};

// ============================================================================
// File Status
// ============================================================================

enum class FileStatus : uint8_t {
    UNMODIFIED = 0,
    MODIFIED,
    ADDED,
    DELETED,
    RENAMED,
    COPIED,
    UNTRACKED,
    IGNORED,
    CONFLICT
};

struct StatusEntry {
    FileStatus  indexStatus;    // Staging area status
    FileStatus  workStatus;     // Working tree status
    std::string path;
    std::string origPath;       // For renames
};

// ============================================================================
// Git Context — main provider
// ============================================================================

class GitContext {
public:
    explicit GitContext(const fs::path& repoRoot);
    ~GitContext();

    // Singleton (for primary workspace)
    static GitContext& Global();
    static void setGlobalRoot(const fs::path& root);

    // ---- Repository Info ----

    bool        isGitRepo() const;
    std::string repoRoot() const { return m_repoRoot.string(); }

    // ---- Branch Operations ----

    GitResult   currentBranch() const;
    std::vector<BranchInfo> listBranches() const;

    // ---- Status ----

    std::vector<StatusEntry> status() const;
    std::vector<StatusEntry> stagedFiles() const;
    std::vector<StatusEntry> unstagedFiles() const;

    // ---- Diff ----

    std::vector<DiffHunk> diffUnstaged() const;
    std::vector<DiffHunk> diffStaged() const;
    std::vector<DiffHunk> diffBetween(const std::string& ref1,
                                       const std::string& ref2) const;
    std::string           diffFile(const std::string& filePath) const;

    // ---- Log ----

    std::vector<CommitInfo> recentCommits(int maxCount = 10) const;
    CommitInfo              commitInfo(const std::string& hash) const;

    // ---- Blame ----

    std::vector<BlameLine> blame(const std::string& filePath) const;

    // ---- Context Building (for prompts) ----

    // Build a context string with recent changes for agentic prompts
    std::string buildChangeContext(int maxTokens = 2000) const;

    // Build commit message suggestion from staged changes
    std::string suggestCommitMessage() const;

    // Get file content at a specific revision
    GitResult   showFile(const std::string& filePath,
                          const std::string& ref = "HEAD") const;

    // ---- Staging ----

    GitResult   stageFile(const std::string& filePath);
    GitResult   unstageFile(const std::string& filePath);

    // ---- Commit ----

    GitResult   commit(const std::string& message);

private:
    // Execute git command and capture stdout
    GitResult   execGit(const std::string& args) const;

    // Parse unified diff output
    std::vector<DiffHunk> parseDiff(const std::string& diffOutput) const;

    // Parse git status --porcelain=v1
    std::vector<StatusEntry> parseStatus(const std::string& statusOutput) const;

    // Parse git log --format output
    std::vector<CommitInfo> parseLog(const std::string& logOutput) const;

    // Parse git blame --porcelain output
    std::vector<BlameLine> parseBlame(const std::string& blameOutput) const;

    fs::path    m_repoRoot;
    mutable std::mutex m_mutex;

    static fs::path s_globalRoot;
};

} // namespace Git
} // namespace RawrXD
