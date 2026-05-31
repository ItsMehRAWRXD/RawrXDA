#include "GitMCPBridge.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <future>
#include <set>
#include <regex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace GitIntegrator {

GitMCPBridge::GitMCPBridge() {
}

// ============================================================================
// PR Review — fetches diff via git and performs structural analysis
// ============================================================================
ChangeReview GitMCPBridge::reviewPullRequest(uint32_t prNumber, const std::string& owner, const std::string& repo) {
    ChangeReview review;
    review.approved = false;
    review.confidence = 0.0f;

    // Fetch the PR branch
    std::string fetchOut;
    std::string fetchCmd = "git fetch origin pull/" + std::to_string(prNumber) + "/head:pr-" + std::to_string(prNumber);
    if (!executeGitCommand(fetchCmd, fetchOut)) {
        review.summary = "Failed to fetch PR #" + std::to_string(prNumber);
        return review;
    }

    // Get the actual diff
    std::string diff;
    std::string diffCmd = "git diff origin/main...pr-" + std::to_string(prNumber);
    if (!executeGitCommand(diffCmd, diff)) {
        review.summary = "Failed to obtain diff for PR #" + std::to_string(prNumber);
        return review;
    }

    // Structural analysis: count files, additions, deletions
    int additions = 0, deletions = 0;
    std::set<std::string> changedFiles;

    std::istringstream stream(diff);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.size() >= 6 && line.substr(0, 6) == "+++ b/") {
            changedFiles.insert(line.substr(6));
        }
        if (!line.empty() && line[0] == '+' && (line.size() < 4 || line.substr(0, 3) != "+++")) {
            ++additions;
        }
        if (!line.empty() && line[0] == '-' && (line.size() < 4 || line.substr(0, 3) != "---")) {
            ++deletions;
        }
    }

    // Risk heuristics
    bool touchesSensitive = false;
    for (const auto& f : changedFiles) {
        if (f.find("auth") != std::string::npos || f.find("crypto") != std::string::npos ||
            f.find("jwt") != std::string::npos || f.find("rbac") != std::string::npos) {
            touchesSensitive = true;
            review.issues.push_back("Sensitive file modified: " + f);
        }
    }

    if (additions + deletions > 1000) {
        review.issues.push_back("Large change set (" + std::to_string(additions + deletions) + " lines) — review carefully.");
    }

    review.confidence = touchesSensitive ? 0.55f : 0.85f;
    review.approved = review.issues.empty() && (additions + deletions < 500);

    std::ostringstream summary;
    summary << "PR #" << prNumber << ": " << changedFiles.size() << " file(s), +"
            << additions << "/-" << deletions;
    if (!review.issues.empty()) {
        summary << " | " << review.issues.size() << " issue(s) flagged";
    }
    review.summary = summary.str();

    return review;
}

// ============================================================================
// Smart commit message generation from diff
// ============================================================================
CommitProposal GitMCPBridge::proposeCommitFromDiff(const std::string& diff) {
    CommitProposal proposal;
    std::string context = parseDiffForContext(diff);

    // Detect context from diff content
    proposal.context = CommitContext::Feature;
    if (diff.find("fix") != std::string::npos || diff.find("bug") != std::string::npos ||
        diff.find("crash") != std::string::npos || diff.find("error") != std::string::npos) {
        proposal.context = CommitContext::Fix;
    } else if (diff.find("refactor") != std::string::npos || diff.find("rename") != std::string::npos) {
        proposal.context = CommitContext::Refactor;
    } else if (diff.find("perf") != std::string::npos || diff.find("optimize") != std::string::npos) {
        proposal.context = CommitContext::Performance;
    } else if (diff.find(".md") != std::string::npos || diff.find("README") != std::string::npos) {
        proposal.context = CommitContext::Documentation;
    }

    // Extract changed file names for scope
    std::string scope;
    std::istringstream stream(diff);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.size() >= 6 && line.substr(0, 6) == "+++ b/") {
            std::string fname = line.substr(6);
            auto slash = fname.find_last_of('/');
            if (slash != std::string::npos) {
                scope = fname.substr(slash + 1);
            } else {
                scope = fname;
            }
            break; // Use first changed file as scope
        }
    }

    const char* prefix = "feat";
    switch (proposal.context) {
        case CommitContext::Fix:           prefix = "fix"; break;
        case CommitContext::Refactor:      prefix = "refactor"; break;
        case CommitContext::Performance:   prefix = "perf"; break;
        case CommitContext::Documentation: prefix = "docs"; break;
        case CommitContext::Chore:         prefix = "chore"; break;
        default: break;
    }

    if (!scope.empty()) {
        proposal.conventionalHeader = std::string(prefix) + "(" + scope + "): update " + context;
    } else {
        proposal.conventionalHeader = std::string(prefix) + ": update " + context;
    }
    proposal.body = "Auto-generated from differential analysis.";
    proposal.commitMessage = proposal.conventionalHeader + "\n\n" + proposal.body;

    return proposal;
}

// ============================================================================
// Blame risk analysis — runs real git blame and parses author volatility
// ============================================================================
std::string GitMCPBridge::analyzeBlameRisk(const std::string& filePath, uint32_t startLine, uint32_t endLine) {
    std::string output;
    std::string cmd = "git blame -L " + std::to_string(startLine) + "," + std::to_string(endLine)
                    + " --porcelain -- " + filePath;

    if (!executeGitCommand(cmd, output)) {
        return "Error retrieving blame data for " + filePath;
    }

    // Parse porcelain blame to count unique authors
    std::set<std::string> authors;
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.size() > 7 && line.substr(0, 7) == "author ") {
            authors.insert(line.substr(7));
        }
    }

    std::ostringstream result;
    result << "Blame analysis for " << filePath << " (L" << startLine << "-L" << endLine << "): ";
    result << authors.size() << " unique author(s)";
    if (authors.size() >= 3) {
        result << " — HIGH volatility. Recent churn detected.";
    } else if (authors.size() == 2) {
        result << " — moderate volatility.";
    } else {
        result << " — low volatility (single owner).";
    }
    return result.str();
}

// ============================================================================
// Historical function state via git log -S (pickaxe search)
// ============================================================================
std::string GitMCPBridge::getHistoricalFunctionState(const std::string& symbol, const std::string& timeRange) {
    std::string output;
    std::string cmd = "git log -S \"" + symbol + "\" --oneline";
    if (!timeRange.empty()) {
        cmd += " --since=\"" + timeRange + "\"";
    }

    if (!executeGitCommand(cmd, output)) {
        return "Error: could not retrieve history for symbol '" + symbol + "'";
    }

    if (output.empty()) {
        return "No changes found for '" + symbol + "' in the specified range.";
    }

    // Count commits that touched this symbol
    int commitCount = 0;
    std::istringstream stream(output);
    std::string line;
    std::string firstCommit, lastCommit;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            ++commitCount;
            if (firstCommit.empty()) firstCommit = line;
            lastCommit = line;
        }
    }

    std::ostringstream result;
    result << "Symbol '" << symbol << "': " << commitCount << " commit(s) found.\n";
    result << "  First: " << firstCommit << "\n";
    if (commitCount > 1) {
        result << "  Latest: " << lastCommit;
    }
    return result.str();
}

// ============================================================================
// Real git command execution via CreateProcess + pipe capture
// ============================================================================
bool GitMCPBridge::executeGitCommand(const std::string& command, std::string& output) {
    output.clear();

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return false;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError  = hWritePipe;
    si.hStdInput  = nullptr;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};

    // Build full command line: "git <arguments>"
    // The command string already starts with "git ..." so we prepend cmd /c for shell resolution
    std::string fullCmd = command;

    // CreateProcessA needs a mutable buffer
    std::vector<char> cmdBuf(fullCmd.begin(), fullCmd.end());
    cmdBuf.push_back('\0');

    BOOL created = CreateProcessA(
        nullptr,
        cmdBuf.data(),
        nullptr, nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi
    );

    CloseHandle(hWritePipe);

    if (!created) {
        CloseHandle(hReadPipe);
        return false;
    }

    // Read stdout/stderr from the child
    constexpr DWORD kBufSize = 4096;
    char buf[kBufSize];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buf, kBufSize, &bytesRead, nullptr) && bytesRead > 0) {
        output.append(buf, bytesRead);
        // Safety cap: 1 MB max output
        if (output.size() > 1024 * 1024) break;
    }
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, 10000);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (exitCode == 0);
}

// ============================================================================
// Diff context extraction — identifies primary change category
// ============================================================================
std::string GitMCPBridge::parseDiffForContext(const std::string& diff) {
    if (diff.length() < 10) return "minimal change";

    int addCount = 0, delCount = 0;
    std::istringstream stream(diff);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line[0] == '+' && (line.size() < 4 || line.substr(0, 3) != "+++")) ++addCount;
        if (!line.empty() && line[0] == '-' && (line.size() < 4 || line.substr(0, 3) != "---")) ++delCount;
    }

    if (addCount > 0 && delCount == 0) return "additions only";
    if (delCount > 0 && addCount == 0) return "deletions only";
    if (addCount > delCount * 2)       return "major expansion";
    if (delCount > addCount * 2)       return "major reduction";
    return "mixed modifications";
}

} // namespace GitIntegrator
} // namespace RawrXD
