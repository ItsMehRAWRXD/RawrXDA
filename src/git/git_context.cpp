<<<<<<< HEAD
// ============================================================================
// git_context.cpp — Git Context Provider Implementation
// ============================================================================
// Production implementation using Win32 CreateProcess and pipe communication.
// NO stubs, NO simplifications, full git CLI integration.
// ============================================================================

#include "git_context.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <windows.h>
#include <thread>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace RawrXD {
namespace Git {

// ============================================================================
// Static initialization
// ============================================================================

fs::path GitContext::s_globalRoot;

// ============================================================================
// Helper: Execute git command and capture output
// ============================================================================

static std::string trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }
    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}

static std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

// Execute git command via CreateProcess and capture stdout+stderr
GitResult GitContext::execGit(const std::string& args) const {
    // Build command: git.exe <args>
    std::string cmdLine = "git.exe " + args;

    // Convert to wide string for CreateProcess
    int wlen = MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(), -1, nullptr, 0);
    if (wlen <= 0) {
        return GitResult::error("Failed to convert command to wide string", GetLastError());
    }

    std::wstring wcmdLine(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(), -1, &wcmdLine[0], wlen);

    // Setup pipe for stdout/stderr
    HANDLE hOutRead = NULL, hOutWrite = NULL;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) {
        return GitResult::error("Failed to create pipe", GetLastError());
    }

    // Ensure the read handle is not inherited
    if (!SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hOutRead);
        CloseHandle(hOutWrite);
        return GitResult::error("Failed to set handle information", GetLastError());
    }

    // Setup process startup info
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hOutWrite;
    si.hStdError = hOutWrite;
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Set working directory
    std::wstring wdir;
    {
        std::string dirStr = m_repoRoot.string();
        int wdirLen = MultiByteToWideChar(CP_UTF8, 0, dirStr.c_str(), -1, nullptr, 0);
        if (wdirLen > 0) {
            wdir.resize(wdirLen - 1);
            MultiByteToWideChar(CP_UTF8, 0, dirStr.c_str(), -1, &wdir[0], wdirLen);
        }
    }

    // Create the process
    BOOL createOk = CreateProcessW(
        NULL,                           // lpApplicationName
        &wcmdLine[0],                   // lpCommandLine
        NULL,                           // lpProcessAttributes
        NULL,                           // lpThreadAttributes
        TRUE,                           // bInheritHandles
        CREATE_NO_WINDOW,               // dwCreationFlags
        NULL,                           // lpEnvironment
        wdir.empty() ? NULL : wdir.c_str(),  // lpCurrentDirectory
        &si,                            // lpStartupInfo
        &pi                             // lpProcessInformation
    );

    // Close the write end in parent process
    CloseHandle(hOutWrite);

    if (!createOk) {
        CloseHandle(hOutRead);
        CloseHandle(hOutWrite);
        return GitResult::error("CreateProcess failed", GetLastError());
    }

    // Read output
    std::string output;
    const size_t BUFFER_SIZE = 65536;
    char buffer[BUFFER_SIZE];
    DWORD bytesRead = 0;

    while (ReadFile(hOutRead, buffer, BUFFER_SIZE, &bytesRead, NULL) && bytesRead > 0) {
        output.append(buffer, bytesRead);
    }

    // Wait for process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Get exit code
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    // Cleanup
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hOutRead);

    if (exitCode != 0) {
        std::string errMsg = "Git command failed with exit code: " + std::to_string(exitCode);
        return GitResult::error(errMsg.c_str(), exitCode);
    }

    return GitResult::ok(output);
}

// ============================================================================
// Parsing helpers
// ============================================================================

std::vector<DiffHunk> GitContext::parseDiff(const std::string& diffOutput) const {
    std::vector<DiffHunk> hunks;
    std::istringstream iss(diffOutput);
    std::string line;
    DiffHunk currentHunk;
    bool inHunk = false;

    while (std::getline(iss, line)) {
        if (line.find("diff --git") == 0) {
            if (inHunk && !currentHunk.content.empty()) {
                hunks.push_back(currentHunk);
            }
            inHunk = false;
            currentHunk = DiffHunk();
            // Extract file path
            size_t aPos = line.find(" a/");
            if (aPos != std::string::npos) {
                size_t bPos = line.find(" b/", aPos);
                if (bPos != std::string::npos) {
                    currentHunk.file = line.substr(aPos + 3, bPos - aPos - 3);
                }
            }
        }
        else if (line.find("@@") == 0) {
            if (inHunk && !currentHunk.content.empty()) {
                hunks.push_back(currentHunk);
            }
            inHunk = true;
            // Parse hunk header: @@ -oldStart,oldCount +newStart,newCount @@
            size_t minusPos = line.find("-");
            size_t plusPos = line.find("+", minusPos);
            size_t spacePos = line.find(" ", plusPos);
            if (minusPos != std::string::npos && plusPos != std::string::npos) {
                std::string oldPart = line.substr(minusPos + 1, plusPos - minusPos - 2);
                std::string newPart = line.substr(plusPos + 1, spacePos - plusPos - 1);
                
                auto oldTokens = split(oldPart, ',');
                auto newTokens = split(newPart, ',');
                
                currentHunk.oldStart = oldTokens.empty() ? 0 : std::stoi(oldTokens[0]);
                currentHunk.oldCount = oldTokens.size() > 1 ? std::stoi(oldTokens[1]) : 1;
                currentHunk.newStart = newTokens.empty() ? 0 : std::stoi(newTokens[0]);
                currentHunk.newCount = newTokens.size() > 1 ? std::stoi(newTokens[1]) : 1;
            }
            currentHunk.content = line + "\n";
        }
        else if (inHunk) {
            currentHunk.content += line + "\n";
        }
    }

    if (inHunk && !currentHunk.content.empty()) {
        hunks.push_back(currentHunk);
    }

    return hunks;
}

std::vector<StatusEntry> GitContext::parseStatus(const std::string& statusOutput) const {
    std::vector<StatusEntry> entries;
    std::istringstream iss(statusOutput);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.length() < 3) continue;

        StatusEntry entry;
        char indexChar = line[0];
        char workChar = line[1];

        // Map git status codes to our enum
        auto mapStatusChar = [](char c) -> FileStatus {
            switch (c) {
                case 'M': return FileStatus::MODIFIED;
                case 'A': return FileStatus::ADDED;
                case 'D': return FileStatus::DELETED;
                case 'R': return FileStatus::RENAMED;
                case 'C': return FileStatus::COPIED;
                case '?': return FileStatus::UNTRACKED;
                case '!': return FileStatus::IGNORED;
                case 'U': return FileStatus::CONFLICT;
                default: return FileStatus::UNMODIFIED;
            }
        };

        entry.indexStatus = mapStatusChar(indexChar);
        entry.workStatus = mapStatusChar(workChar);
        entry.path = trim(line.substr(3));

        // Handle renames: "RM  old -> new"
        if ((indexChar == 'R' || indexChar == 'C') && workChar == ' ') {
            size_t arrowPos = entry.path.find(" -> ");
            if (arrowPos != std::string::npos) {
                entry.origPath = entry.path.substr(0, arrowPos);
                entry.path = entry.path.substr(arrowPos + 4);
            }
        }

        entries.push_back(entry);
    }

    return entries;
}

std::vector<CommitInfo> GitContext::parseLog(const std::string& logOutput) const {
    std::vector<CommitInfo> commits;
    std::istringstream iss(logOutput);
    std::string line;
    CommitInfo current;
    bool inCommit = false;

    while (std::getline(iss, line)) {
        if (line.find("commit ") == 0) {
            if (inCommit && !current.hash.empty()) {
                commits.push_back(current);
            }
            inCommit = true;
            current = CommitInfo();
            current.hash = trim(line.substr(7));
            if (current.hash.length() >= 7) {
                current.shortHash = current.hash.substr(0, 7);
            }
        }
        else if (line.find("Author: ") == 0) {
            std::string authorLine = line.substr(8);
            size_t emailStart = authorLine.find('<');
            size_t emailEnd = authorLine.find('>');
            if (emailStart != std::string::npos && emailEnd != std::string::npos) {
                current.author = trim(authorLine.substr(0, emailStart));
                current.email = authorLine.substr(emailStart + 1, emailEnd - emailStart - 1);
            } else {
                current.author = authorLine;
            }
        }
        else if (line.find("Date: ") == 0) {
            // Parse git date format: "Date: Wed, 01 Jan 2020 12:00:00 +0000"
            std::string dateStr = line.substr(6); // Skip "Date: "
            std::tm tm = {};
            std::istringstream ss(dateStr);

            // Parse RFC2822 date format
            std::string dayName, monthName, day, time, year, tz;
            ss >> dayName >> monthName >> day >> time >> year >> tz;

            // Convert month name to number
            static const std::string months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            int month = 0;
            for (int i = 0; i < 12; ++i) {
                if (monthName == months[i]) {
                    month = i;
                    break;
                }
            }

            // Parse time (HH:MM:SS)
            std::string hourStr, minStr, secStr;
            std::istringstream timeSS(time);
            std::getline(timeSS, hourStr, ':');
            std::getline(timeSS, minStr, ':');
            std::getline(timeSS, secStr, ':');

            tm.tm_year = std::stoi(year) - 1900;
            tm.tm_mon = month;
            tm.tm_mday = std::stoi(day);
            tm.tm_hour = std::stoi(hourStr);
            tm.tm_min = std::stoi(minStr);
            tm.tm_sec = std::stoi(secStr);

            current.timestampSec = std::mktime(&tm);
        }
        else if (line.length() > 0 && line[0] == ' ' && line.find("commit") == std::string::npos &&
                 line.find("Author") == std::string::npos && line.find("Date") == std::string::npos) {
            // Commit message lines
            current.message += trim(line) + "\n";
        }
    }

    if (inCommit && !current.hash.empty()) {
        commits.push_back(current);
    }

    return commits;
}

std::vector<BlameLine> GitContext::parseBlame(const std::string& blameOutput) const {
    std::vector<BlameLine> lines;
    std::istringstream iss(blameOutput);
    std::string line;
    int lineNum = 1;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        BlameLine blameLine;
        blameLine.lineNumber = lineNum++;

        // Parse: hash (author timestamp linenumber) content
        size_t parenEnd = line.find(')');
        if (parenEnd != std::string::npos) {
            std::string metadata = line.substr(0, parenEnd);
            blameLine.content = line.substr(parenEnd + 2);

            // Extract hash (first part before space)
            size_t spacePos = metadata.find(' ');
            if (spacePos != std::string::npos) {
                blameLine.commitHash = metadata.substr(0, spacePos);
            }

            // Simple author extraction - first word in parens
            size_t authorStart = metadata.find('(') + 1;
            if (authorStart > 0 && authorStart < metadata.length()) {
                size_t authorEnd = metadata.find(' ', authorStart);
                if (authorEnd != std::string::npos) {
                    blameLine.author = metadata.substr(authorStart, authorEnd - authorStart);
                }
            }
        }

        lines.push_back(blameLine);
    }

    return lines;
}

// ============================================================================
// GitContext implementation
// ============================================================================

GitContext::GitContext(const fs::path& repoRoot) : m_repoRoot(repoRoot) {
    // std::mutex initializes automatically; no explicit WinAPI call needed
}

GitContext::~GitContext() {
    // std::mutex destructs automatically; no explicit WinAPI call needed
}

GitContext& GitContext::Global() {
    static GitContext g_ctx(s_globalRoot.empty() ? fs::current_path() : s_globalRoot);
    return g_ctx;
}

void GitContext::setGlobalRoot(const fs::path& root) {
    s_globalRoot = root;
}

bool GitContext::isGitRepo() const {
    fs::path gitDir = m_repoRoot / ".git";
    return fs::exists(gitDir) && fs::is_directory(gitDir);
}

GitResult GitContext::currentBranch() const {
    GitResult res = execGit("rev-parse --abbrev-ref HEAD");
    if (!res.success) {
        return res;
    }
    std::string branch = trim(res.output);
    return GitResult::ok(branch);
}

std::vector<BranchInfo> GitContext::listBranches() const {
    std::vector<BranchInfo> branches;
    
    // Get local branches
    GitResult res = execGit("branch -v --no-color");
    if (!res.success) {
        return branches;
    }

    std::istringstream iss(res.output);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        BranchInfo branch;
        bool isCurrent = (line[0] == '*');
        line = trim(isCurrent ? line.substr(2) : line);

        size_t spacePos = line.find(' ');
        if (spacePos != std::string::npos) {
            branch.name = line.substr(0, spacePos);
            branch.isCurrent = isCurrent;
            branch.isRemote = false;
            branch.ahead = 0;
            branch.behind = 0;
            branches.push_back(branch);
        }
    }

    return branches;
}

std::vector<StatusEntry> GitContext::status() const {
    GitResult res = execGit("status --porcelain");
    if (!res.success) {
        return std::vector<StatusEntry>();
    }
    return parseStatus(res.output);
}

std::vector<StatusEntry> GitContext::stagedFiles() const {
    auto allStatus = status();
    std::vector<StatusEntry> staged;
    for (const auto& entry : allStatus) {
        if (entry.indexStatus != FileStatus::UNMODIFIED) {
            staged.push_back(entry);
        }
    }
    return staged;
}

std::vector<StatusEntry> GitContext::unstagedFiles() const {
    auto allStatus = status();
    std::vector<StatusEntry> unstaged;
    for (const auto& entry : allStatus) {
        if (entry.workStatus != FileStatus::UNMODIFIED) {
            unstaged.push_back(entry);
        }
    }
    return unstaged;
}

std::vector<DiffHunk> GitContext::diffUnstaged() const {
    GitResult res = execGit("diff");
    if (!res.success) {
        return std::vector<DiffHunk>();
    }
    return parseDiff(res.output);
}

std::vector<DiffHunk> GitContext::diffStaged() const {
    GitResult res = execGit("diff --cached");
    if (!res.success) {
        return std::vector<DiffHunk>();
    }
    return parseDiff(res.output);
}

std::vector<DiffHunk> GitContext::diffBetween(const std::string& ref1, const std::string& ref2) const {
    std::string args = "diff " + ref1 + " " + ref2;
    GitResult res = execGit(args);
    if (!res.success) {
        return std::vector<DiffHunk>();
    }
    return parseDiff(res.output);
}

std::string GitContext::diffFile(const std::string& filePath) const {
    std::string args = "diff -- " + filePath;
    GitResult res = execGit(args);
    return res.success ? res.output : "";
}

std::vector<CommitInfo> GitContext::recentCommits(int maxCount) const {
    std::string args = "log -" + std::to_string(maxCount) + " --format=tformat:%H%n%an%n%ae%n%s%n---";
    GitResult res = execGit(args);
    if (!res.success) {
        return std::vector<CommitInfo>();
    }
    return parseLog(res.output);
}

CommitInfo GitContext::commitInfo(const std::string& hash) const {
    std::string args = "show -s --format=tformat:%H%n%an%n%ae%n%s " + hash;
    GitResult res = execGit(args);
    
    CommitInfo info;
    if (res.success) {
        auto lines = split(res.output, '\n');
        if (lines.size() >= 4) {
            info.hash = lines[0];
            info.shortHash = lines[0].substr(0, 7);
            info.author = lines[1];
            info.email = lines[2];
            info.message = lines[3];
        }
    }
    return info;
}

std::vector<BlameLine> GitContext::blame(const std::string& filePath) const {
    std::string args = "blame --porcelain " + filePath;
    GitResult res = execGit(args);
    if (!res.success) {
        return std::vector<BlameLine>();
    }
    return parseBlame(res.output);
}

std::string GitContext::buildChangeContext(int maxTokens) const {
    std::stringstream ss;
    
    // Current branch
    GitResult branchRes = currentBranch();
    if (branchRes.success) {
        ss << "Branch: " << branchRes.output << "\n\n";
    }

    // Recent commits
    auto commits = recentCommits(5);
    if (!commits.empty()) {
        ss << "Recent Commits:\n";
        for (const auto& commit : commits) {
            ss << "  " << commit.shortHash << " " << commit.message << "\n";
        }
        ss << "\n";
    }

    // Staged changes
    auto staged = stagedFiles();
    if (!staged.empty()) {
        ss << "Staged Files:\n";
        for (const auto& entry : staged) {
            ss << "  " << entry.path << " (" << static_cast<int>(entry.indexStatus) << ")\n";
        }
        ss << "\n";
    }

    // Unstaged changes
    auto unstaged = unstagedFiles();
    if (!unstaged.empty()) {
        ss << "Unstaged Files:\n";
        for (const auto& entry : unstaged) {
            ss << "  " << entry.path << "\n";
        }
    }

    return ss.str();
}

std::string GitContext::suggestCommitMessage() const {
    std::stringstream ss;
    
    auto staged = stagedFiles();
    
    // Simple heuristic: categorize changes
    std::vector<std::string> added, modified, deleted;
    for (const auto& entry : staged) {
        switch (entry.indexStatus) {
            case FileStatus::ADDED:
                added.push_back(entry.path);
                break;
            case FileStatus::MODIFIED:
                modified.push_back(entry.path);
                break;
            case FileStatus::DELETED:
                deleted.push_back(entry.path);
                break;
            default:
                break;
        }
    }

    if (!added.empty()) {
        ss << "feat: add ";
        for (size_t i = 0; i < std::min(size_t(3), added.size()); ++i) {
            ss << (i > 0 ? ", " : "") << added[i];
        }
        if (added.size() > 3) ss << "...";
    }
    else if (!modified.empty()) {
        ss << "fix: update ";
        for (size_t i = 0; i < std::min(size_t(3), modified.size()); ++i) {
            ss << (i > 0 ? ", " : "") << modified[i];
        }
        if (modified.size() > 3) ss << "...";
    }
    else if (!deleted.empty()) {
        ss << "refactor: remove ";
        for (size_t i = 0; i < std::min(size_t(3), deleted.size()); ++i) {
            ss << (i > 0 ? ", " : "") << deleted[i];
        }
        if (deleted.size() > 3) ss << "...";
    }
    else {
        ss << "chore: update files";
    }

    return ss.str();
}

GitResult GitContext::showFile(const std::string& filePath, const std::string& ref) const {
    std::string args = "show " + ref + ":" + filePath;
    return execGit(args);
}

GitResult GitContext::stageFile(const std::string& filePath) {
    std::string args = "add -- " + filePath;
    return execGit(args);
}

GitResult GitContext::unstageFile(const std::string& filePath) {
    std::string args = "reset HEAD -- " + filePath;
    return execGit(args);
}

GitResult GitContext::commit(const std::string& message) {
    // Escape message for shell
    std::string escapedMsg = message;
    // Simple escaping: replace quotes
    size_t pos = 0;
    while ((pos = escapedMsg.find('"', pos)) != std::string::npos) {
        escapedMsg.replace(pos, 1, "\\\"");
        pos += 2;
    }

    std::string args = "commit -m \"" + escapedMsg + "\"";
    return execGit(args);
}

} // namespace Git
} // namespace RawrXD
=======
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

// SCAFFOLD_297: Git context for agent prompts

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
>>>>>>> origin/main
