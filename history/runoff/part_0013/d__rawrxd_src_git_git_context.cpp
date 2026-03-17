// ============================================================================
// git_context.cpp — Git Context Provider Implementation
// ============================================================================
// Git CLI subprocess integration for branch, status, diff, log, blame.
// Provides structured context for agentic prompts and commit message
// suggestion.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "git/git_context.h"

#include <sstream>
#include <regex>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Git {

// ============================================================================
// Static
// ============================================================================

fs::path GitContext::s_globalRoot;

GitContext& GitContext::Global() {
    static GitContext instance(s_globalRoot);
    return instance;
}

void GitContext::setGlobalRoot(const fs::path& root) {
    s_globalRoot = root;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

GitContext::GitContext(const fs::path& repoRoot)
    : m_repoRoot(repoRoot) {}

GitContext::~GitContext() = default;

// ============================================================================
// Repository Info
// ============================================================================

bool GitContext::isGitRepo() const {
    return fs::exists(m_repoRoot / ".git");
}

// ============================================================================
// Execute git command
// ============================================================================

GitResult GitContext::execGit(const std::string& args) const {
    std::string command = "git -C \"" + m_repoRoot.string() + "\" " + args + " 2>&1";

#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif

    if (!pipe) {
        return GitResult::error("Failed to execute git command", -1);
    }

    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }

#ifdef _WIN32
    int exitCode = _pclose(pipe);
#else
    int exitCode = pclose(pipe);
#endif

    if (exitCode != 0) {
        // Non-zero exit but may still have useful output
        GitResult r;
        r.success   = false;
        r.detail    = "Git command returned non-zero";
        r.errorCode = exitCode;
        r.output    = std::move(output);
        return r;
    }

    return GitResult::ok(std::move(output));
}

// ============================================================================
// Branch Operations
// ============================================================================

GitResult GitContext::currentBranch() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto result = execGit("rev-parse --abbrev-ref HEAD");
    if (result.success) {
        // Trim newline
        while (!result.output.empty() &&
               (result.output.back() == '\n' || result.output.back() == '\r')) {
            result.output.pop_back();
        }
    }
    return result;
}

std::vector<BranchInfo> GitContext::listBranches() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto result = execGit("branch -a --format='%(refname:short)|%(HEAD)|%(upstream:short)|%(upstream:track,nobracket)'");
    if (!result.success) return {};

    std::vector<BranchInfo> branches;
    std::istringstream stream(result.output);
    std::string line;

    while (std::getline(stream, line)) {
        // Remove surrounding quotes if present
        if (!line.empty() && line.front() == '\'') line = line.substr(1);
        if (!line.empty() && line.back() == '\'') line.pop_back();

        BranchInfo info;
        size_t pos = 0;

        // Parse pipe-delimited fields
        auto nextField = [&]() -> std::string {
            size_t pipePos = line.find('|', pos);
            std::string field;
            if (pipePos != std::string::npos) {
                field = line.substr(pos, pipePos - pos);
                pos = pipePos + 1;
            } else {
                field = line.substr(pos);
                pos = line.size();
            }
            return field;
        };

        info.name      = nextField();
        info.isCurrent = (nextField() == "*");
        info.upstream  = nextField();
        std::string track = nextField();

        info.isRemote = (info.name.find("remotes/") == 0 ||
                         info.name.find("origin/") == 0);

        // Parse ahead/behind from track info
        info.ahead  = 0;
        info.behind = 0;
        static const std::regex aheadRe(R"(ahead (\d+))");
        static const std::regex behindRe(R"(behind (\d+))");
        std::smatch m;
        if (std::regex_search(track, m, aheadRe)) info.ahead = std::stoi(m[1].str());
        if (std::regex_search(track, m, behindRe)) info.behind = std::stoi(m[1].str());

        if (!info.name.empty()) {
            branches.push_back(std::move(info));
        }
    }

    return branches;
}

// ============================================================================
// Status
// ============================================================================

std::vector<StatusEntry> GitContext::parseStatus(const std::string& statusOutput) const {
    std::vector<StatusEntry> entries;
    std::istringstream stream(statusOutput);
    std::string line;

    auto charToStatus = [](char c) -> FileStatus {
        switch (c) {
            case 'M': return FileStatus::MODIFIED;
            case 'A': return FileStatus::ADDED;
            case 'D': return FileStatus::DELETED;
            case 'R': return FileStatus::RENAMED;
            case 'C': return FileStatus::COPIED;
            case '?': return FileStatus::UNTRACKED;
            case '!': return FileStatus::IGNORED;
            case 'U': return FileStatus::CONFLICT;
            default:  return FileStatus::UNMODIFIED;
        }
    };

    while (std::getline(stream, line)) {
        if (line.size() < 4) continue;

        StatusEntry entry;
        entry.indexStatus = charToStatus(line[0]);
        entry.workStatus  = charToStatus(line[1]);
        // Skip the space at position 2
        entry.path = line.substr(3);

        // Handle renames: "R  old -> new"
        size_t arrowPos = entry.path.find(" -> ");
        if (arrowPos != std::string::npos) {
            entry.origPath = entry.path.substr(0, arrowPos);
            entry.path = entry.path.substr(arrowPos + 4);
        }

        entries.push_back(std::move(entry));
    }

    return entries;
}

std::vector<StatusEntry> GitContext::status() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto result = execGit("status --porcelain=v1");
    if (!result.success) return {};
    return parseStatus(result.output);
}

std::vector<StatusEntry> GitContext::stagedFiles() const {
    auto all = status();
    std::vector<StatusEntry> staged;
    for (const auto& e : all) {
        if (e.indexStatus != FileStatus::UNMODIFIED &&
            e.indexStatus != FileStatus::UNTRACKED) {
            staged.push_back(e);
        }
    }
    return staged;
}

std::vector<StatusEntry> GitContext::unstagedFiles() const {
    auto all = status();
    std::vector<StatusEntry> unstaged;
    for (const auto& e : all) {
        if (e.workStatus != FileStatus::UNMODIFIED) {
            unstaged.push_back(e);
        }
    }
    return unstaged;
}

// ============================================================================
// Diff
// ============================================================================

std::vector<DiffHunk> GitContext::parseDiff(const std::string& diffOutput) const {
    std::vector<DiffHunk> hunks;

    static const std::regex hunkHeader(
        R"(@@ -(\d+),?(\d*) \+(\d+),?(\d*) @@)",
        std::regex::optimize
    );
    static const std::regex fileHeader(
        R"(\+\+\+ b/(.+))",
        std::regex::optimize
    );

    std::istringstream stream(diffOutput);
    std::string line;
    std::string currentFile;
    DiffHunk* currentHunk = nullptr;

    while (std::getline(stream, line)) {
        std::smatch match;

        if (std::regex_search(line, match, fileHeader)) {
            currentFile = match[1].str();
            continue;
        }

        if (std::regex_search(line, match, hunkHeader)) {
            hunks.push_back({});
            currentHunk = &hunks.back();
            currentHunk->file     = currentFile;
            currentHunk->oldStart = std::stoi(match[1].str());
            currentHunk->oldCount = match[2].length() > 0 ? std::stoi(match[2].str()) : 1;
            currentHunk->newStart = std::stoi(match[3].str());
            currentHunk->newCount = match[4].length() > 0 ? std::stoi(match[4].str()) : 1;
            continue;
        }

        if (currentHunk && !line.empty() &&
            (line[0] == '+' || line[0] == '-' || line[0] == ' ')) {
            currentHunk->content += line + "\n";
        }
    }

    return hunks;
}

std::vector<DiffHunk> GitContext::diffUnstaged() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto result = execGit("diff");
    if (!result.success) return {};
    return parseDiff(result.output);
}

std::vector<DiffHunk> GitContext::diffStaged() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto result = execGit("diff --staged");
    if (!result.success) return {};
    return parseDiff(result.output);
}

std::vector<DiffHunk> GitContext::diffBetween(const std::string& ref1,
                                              const std::string& ref2) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto result = execGit("diff " + ref1 + " " + ref2);
    if (!result.success) return {};
    return parseDiff(result.output);
}

std::string GitContext::diffFile(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto result = execGit("diff -- \"" + filePath + "\"");
    return result.output;
}

// ============================================================================
// Log
// ============================================================================

std::vector<CommitInfo> GitContext::parseLog(const std::string& logOutput) const {
    std::vector<CommitInfo> commits;

    // Format: hash|shorthash|author|email|message|timestamp
    std::istringstream stream(logOutput);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        CommitInfo info;
        size_t pos = 0;

        auto nextField = [&]() -> std::string {
            size_t pipePos = line.find('|', pos);
            std::string field;
            if (pipePos != std::string::npos) {
                field = line.substr(pos, pipePos - pos);
                pos = pipePos + 1;
            } else {
                field = line.substr(pos);
                pos = line.size();
            }
            return field;
        };

        info.hash         = nextField();
        info.shortHash    = nextField();
        info.author       = nextField();
        info.email        = nextField();
        info.message      = nextField();
        std::string ts    = nextField();

        if (!ts.empty()) {
            info.timestampSec = std::stoull(ts);
        }

        if (!info.hash.empty()) {
            commits.push_back(std::move(info));
        }
    }

    return commits;
}

std::vector<CommitInfo> GitContext::recentCommits(int maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string fmt = "log -" + std::to_string(maxCount) +
                      " --format='%H|%h|%an|%ae|%s|%ct'";
    auto result = execGit(fmt);
    if (!result.success) return {};
    return parseLog(result.output);
}

CommitInfo GitContext::commitInfo(const std::string& hash) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto result = execGit("log -1 --format='%H|%h|%an|%ae|%s|%ct' " + hash);
    if (!result.success) return {};
    auto commits = parseLog(result.output);
    return commits.empty() ? CommitInfo{} : commits[0];
}

// ============================================================================
// Blame
// ============================================================================

std::vector<BlameLine> GitContext::parseBlame(const std::string& blameOutput) const {
    std::vector<BlameLine> lines;

    static const std::regex blameLineRe(
        R"(^([0-9a-f]+)\s.*\((.+?)\s+(\d{4}-\d{2}-\d{2})\s+\d+\)\s(.*)$)",
        std::regex::optimize
    );

    std::istringstream stream(blameOutput);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;
        std::smatch match;
        if (std::regex_search(line, match, blameLineRe)) {
            BlameLine bl;
            bl.commitHash = match[1].str();
            bl.author     = match[2].str();
            bl.timestamp  = 0; // Could parse date but keeping simple
            bl.lineNumber = lineNum;
            bl.content    = match[4].str();
            lines.push_back(std::move(bl));
        }
    }

    return lines;
}

std::vector<BlameLine> GitContext::blame(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto result = execGit("blame \"" + filePath + "\"");
    if (!result.success) return {};
    return parseBlame(result.output);
}

// ============================================================================
// Context Building
// ============================================================================

std::string GitContext::buildChangeContext(int maxTokens) const {
    std::ostringstream oss;

    // Current branch
    auto branch = currentBranch();
    if (branch.success) {
        oss << "Branch: " << branch.output << "\n";
    }

    // Staged changes
    auto staged = diffStaged();
    if (!staged.empty()) {
        oss << "\n// Staged changes:\n";
        for (const auto& hunk : staged) {
            std::string entry = "// " + hunk.file +
                                " @@ " + std::to_string(hunk.newStart) + " @@\n" +
                                hunk.content;
            oss << entry;
        }
    }

    // Unstaged changes
    auto unstaged = diffUnstaged();
    if (!unstaged.empty()) {
        oss << "\n// Unstaged changes:\n";
        for (const auto& hunk : unstaged) {
            std::string entry = "// " + hunk.file +
                                " @@ " + std::to_string(hunk.newStart) + " @@\n" +
                                hunk.content;
            oss << entry;
        }
    }

    // Truncate to token budget (rough: 4 chars/token)
    std::string result = oss.str();
    size_t maxChars = static_cast<size_t>(maxTokens) * 4;
    if (result.size() > maxChars) {
        result = result.substr(0, maxChars) + "\n// ... (truncated)\n";
    }

    return result;
}

std::string GitContext::suggestCommitMessage() const {
    auto staged = diffStaged();
    if (staged.empty()) return "";

    // Build a summary of changes
    std::ostringstream oss;

    std::unordered_map<std::string, int> fileCounts;
    int totalAdded = 0, totalRemoved = 0;

    for (const auto& hunk : staged) {
        fileCounts[hunk.file]++;
        // Count +/- lines
        std::istringstream stream(hunk.content);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line[0] == '+') totalAdded++;
            if (!line.empty() && line[0] == '-') totalRemoved++;
        }
    }

    // Heuristic commit message
    if (fileCounts.size() == 1) {
        auto it = fileCounts.begin();
        fs::path p(it->first);
        std::string stem = p.stem().string();

        if (totalAdded > 0 && totalRemoved == 0) {
            oss << "Add " << stem;
        } else if (totalRemoved > 0 && totalAdded == 0) {
            oss << "Remove code from " << stem;
        } else {
            oss << "Update " << stem;
        }
    } else {
        if (totalAdded > totalRemoved * 3) {
            oss << "Add new files and features";
        } else if (totalRemoved > totalAdded * 3) {
            oss << "Clean up and remove unused code";
        } else {
            oss << "Update " << fileCounts.size() << " files";
        }
    }

    oss << " (+" << totalAdded << "/-" << totalRemoved << ")";
    return oss.str();
}

// ============================================================================
// File at revision
// ============================================================================

GitResult GitContext::showFile(const std::string& filePath,
                               const std::string& ref) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return execGit("show " + ref + ":\"" + filePath + "\"");
}

// ============================================================================
// Staging
// ============================================================================

GitResult GitContext::stageFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return execGit("add \"" + filePath + "\"");
}

GitResult GitContext::unstageFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return execGit("reset HEAD \"" + filePath + "\"");
}

// ============================================================================
// Commit
// ============================================================================

GitResult GitContext::commit(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Escape quotes in message
    std::string escaped = message;
    size_t pos = 0;
    while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\"");
        pos += 2;
    }
    return execGit("commit -m \"" + escaped + "\"");
}

} // namespace Git
} // namespace RawrXD
