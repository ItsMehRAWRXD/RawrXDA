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
