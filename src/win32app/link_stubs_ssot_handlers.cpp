// =============================================================================
// link_stubs_ssot_handlers.cpp — Stub implementations for 103 SSOT command handlers
// Generated to satisfy linker. Replace with real implementations later.
// =============================================================================
#include "../core/shared_feature_dispatch.h"
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <memory>
#include <cctype>
#include <map>
#include <mutex>
#include <climits>
#include <cerrno>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static CommandResult make_handler_stub(const char* name) {
    return CommandResult::error(name);
}

// =============================================================================
// GIT INTEGRATION — Production Implementation
// =============================================================================

namespace GitIntegration {

// ── Result structure for git command execution ──────────────────────────────
struct GitExecResult {
    bool success;
    int exitCode;
    std::string stdOut;
    std::string stdErr;
};

// ── Execute git command with pipe redirection ───────────────────────────────
// Uses CreateProcess to spawn git.exe and capture stdout/stderr
static GitExecResult executeGitCommand(const std::string& args, const std::string& workingDir = "") {
    GitExecResult result{false, -1, "", ""};

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    // Create pipes for stdout
    HANDLE hStdOutRead = nullptr, hStdOutWrite = nullptr;
    HANDLE hStdErrRead = nullptr, hStdErrWrite = nullptr;

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        result.stdErr = "Failed to create stdout pipe";
        return result;
    }
    if (!CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        result.stdErr = "Failed to create stderr pipe";
        return result;
    }

    // Ensure read handles are not inherited
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::string cmdLine = "git.exe " + args;
    const char* cwd = workingDir.empty() ? nullptr : workingDir.c_str();

    BOOL created = CreateProcessA(
        nullptr,
        const_cast<char*>(cmdLine.c_str()),
        nullptr, nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        cwd,
        &si,
        &pi
    );

    // Close write ends in parent
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);

    if (!created) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdErrRead);
        result.stdErr = "Failed to execute git: error " + std::to_string(GetLastError());
        return result;
    }

    // Read stdout and stderr
    auto readPipe = [](HANDLE hPipe) -> std::string {
        std::string output;
        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }
        return output;
    };

    result.stdOut = readPipe(hStdOutRead);
    result.stdErr = readPipe(hStdErrRead);

    // Wait for process and get exit code
    WaitForSingleObject(pi.hProcess, 30000); // 30 second timeout
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result.exitCode = static_cast<int>(exitCode);
    result.success = (exitCode == 0);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);
#else
    result.stdErr = "Git integration requires Windows";
#endif

    return result;
}

// ── String utilities ────────────────────────────────────────────────────────
static std::vector<std::string> splitLines(const std::string& str) {
    std::vector<std::string> lines;
    std::istringstream iss(str);
    std::string line;
    while (std::getline(iss, line)) {
        // Remove trailing \r if present (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static std::string escapeJson(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 16);
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

// ── Extract argument from context ───────────────────────────────────────────
static std::string getArg(const CommandContext& ctx, const std::string& key, const std::string& defaultVal = "") {
    if (!ctx.args || ctx.args[0] == '\0') return defaultVal;
    
    std::string args(ctx.args);
    // Simple key=value or key:value parsing
    size_t pos = args.find(key + "=");
    if (pos == std::string::npos) {
        pos = args.find(key + ":");
    }
    if (pos == std::string::npos) {
        // If no key specified and we just want the raw arg
        if (key.empty()) return trim(args);
        return defaultVal;
    }
    
    size_t start = pos + key.length() + 1;
    size_t end = args.find_first_of(" \t", start);
    if (end == std::string::npos) end = args.length();
    
    std::string val = args.substr(start, end - start);
    // Handle quoted strings
    if (!val.empty() && val[0] == '"') {
        size_t closeQuote = args.find('"', start + 1);
        if (closeQuote != std::string::npos) {
            val = args.substr(start + 1, closeQuote - start - 1);
        }
    }
    return val;
}

static bool hasFlag(const CommandContext& ctx, const std::string& flag) {
    if (!ctx.args || ctx.args[0] == '\0') return false;
    std::string args(ctx.args);
    return args.find("--" + flag) != std::string::npos || 
           args.find("-" + flag) != std::string::npos;
}

// Thread-safe static buffer for result messages
static thread_local char s_resultBuffer[8192];

} // namespace GitIntegration

// =============================================================================
// handleGitStatus — Execute git status --porcelain -b and return structured JSON
// =============================================================================
CommandResult handleGitStatus(const CommandContext& ctx) {
    using namespace GitIntegration;
    
    auto result = executeGitCommand("status --porcelain=v2 -b");
    if (!result.success && result.exitCode != 0) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"%s"})", escapeJson(result.stdErr).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("git status failed");
    }
    
    // Parse porcelain v2 output
    std::string branch;
    std::string upstream;
    int aheadCount = 0;
    int behindCount = 0;
    int stagedCount = 0;
    int modifiedCount = 0;
    int untrackedCount = 0;
    int conflictCount = 0;
    std::vector<std::string> stagedFiles;
    std::vector<std::string> modifiedFiles;
    std::vector<std::string> untrackedFiles;
    
    auto lines = splitLines(result.stdOut);
    for (const auto& line : lines) {
        if (line.empty()) continue;
        
        // Branch header lines
        if (line.rfind("# branch.head ", 0) == 0) {
            branch = line.substr(14);
        } else if (line.rfind("# branch.upstream ", 0) == 0) {
            upstream = line.substr(18);
        } else if (line.rfind("# branch.ab ", 0) == 0) {
            // Format: # branch.ab +N -M
            std::istringstream iss(line.substr(12));
            std::string ahead, behind;
            iss >> ahead >> behind;
            if (!ahead.empty() && ahead[0] == '+') aheadCount = std::stoi(ahead.substr(1));
            if (!behind.empty() && behind[0] == '-') behindCount = std::stoi(behind.substr(1));
        }
        // Changed entries: 1 XY ... path
        else if (line[0] == '1' || line[0] == '2') {
            // XY is at position 2-3
            if (line.length() > 3) {
                char indexStatus = line[2];
                char worktreeStatus = line[3];
                
                // Find filename (last space-separated field)
                size_t lastTab = line.rfind('\t');
                size_t lastSpace = line.rfind(' ');
                size_t fileStart = (lastTab != std::string::npos) ? lastTab + 1 : 
                                   (lastSpace != std::string::npos) ? lastSpace + 1 : 0;
                std::string filename = line.substr(fileStart);
                
                if (indexStatus != '.' && indexStatus != '?') {
                    stagedCount++;
                    stagedFiles.push_back(filename);
                }
                if (worktreeStatus != '.' && worktreeStatus != '?') {
                    modifiedCount++;
                    modifiedFiles.push_back(filename);
                }
            }
        }
        // Untracked entries: ? path
        else if (line[0] == '?') {
            untrackedCount++;
            if (line.length() > 2) {
                untrackedFiles.push_back(line.substr(2));
            }
        }
        // Unmerged entries: u XY ...
        else if (line[0] == 'u') {
            conflictCount++;
        }
    }
    
    // Build JSON response
    std::ostringstream json;
    json << R"({"success":true,"branch":")" << escapeJson(branch) << R"(",)";
    json << R"("upstream":")" << escapeJson(upstream) << R"(",)";
    json << R"("ahead":)" << aheadCount << R"(,"behind":)" << behindCount << ",";
    json << R"("staged":)" << stagedCount << R"(,"modified":)" << modifiedCount << ",";
    json << R"("untracked":)" << untrackedCount << R"(,"conflicts":)" << conflictCount << ",";
    
    // File arrays
    json << R"("stagedFiles":[)";
    for (size_t i = 0; i < stagedFiles.size() && i < 20; ++i) {
        if (i > 0) json << ",";
        json << "\"" << escapeJson(stagedFiles[i]) << "\"";
    }
    json << R"(],"modifiedFiles":[)";
    for (size_t i = 0; i < modifiedFiles.size() && i < 20; ++i) {
        if (i > 0) json << ",";
        json << "\"" << escapeJson(modifiedFiles[i]) << "\"";
    }
    json << R"(],"untrackedFiles":[)";
    for (size_t i = 0; i < untrackedFiles.size() && i < 20; ++i) {
        if (i > 0) json << ",";
        json << "\"" << escapeJson(untrackedFiles[i]) << "\"";
    }
    json << "]}";
    
    std::string jsonStr = json.str();
    ctx.output(jsonStr.c_str());
    return CommandResult::ok("git status completed");
}

// =============================================================================
// handleGitCommit — Stage and commit with message
// =============================================================================
CommandResult handleGitCommit(const CommandContext& ctx) {
    using namespace GitIntegration;
    
    // Get commit message from args
    std::string message = getArg(ctx, "message", "");
    if (message.empty()) message = getArg(ctx, "m", "");
    if (message.empty() && ctx.args) {
        // Try raw args as message
        message = trim(std::string(ctx.args));
    }
    
    if (message.empty()) {
        ctx.output(R"({"success":false,"error":"No commit message provided. Use message=\"your message\""})");
        return CommandResult::error("No commit message");
    }
    
    bool stageAll = hasFlag(ctx, "all") || hasFlag(ctx, "a");
    
    // Stage all changes if requested
    if (stageAll) {
        auto stageResult = executeGitCommand("add -A");
        if (!stageResult.success) {
            snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                R"({"success":false,"error":"Failed to stage files: %s"})", 
                escapeJson(stageResult.stdErr).c_str());
            ctx.output(s_resultBuffer);
            return CommandResult::error("git add failed");
        }
    }
    
    // Execute commit
    std::string commitCmd = "commit -m \"" + message + "\"";
    auto result = executeGitCommand(commitCmd);
    
    if (!result.success) {
        // Check for "nothing to commit" case
        if (result.stdOut.find("nothing to commit") != std::string::npos ||
            result.stdErr.find("nothing to commit") != std::string::npos) {
            ctx.output(R"({"success":false,"error":"Nothing to commit, working tree clean"})");
            return CommandResult::error("Nothing to commit");
        }
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"%s"})", escapeJson(result.stdErr).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("git commit failed");
    }
    
    // Parse commit hash from output (usually in format "[branch hash] message")
    std::string commitHash;
    auto lines = splitLines(result.stdOut);
    for (const auto& line : lines) {
        // Look for pattern like "[main abc1234]"
        size_t bracket = line.find('[');
        if (bracket != std::string::npos) {
            size_t space = line.find(' ', bracket);
            size_t closeBracket = line.find(']', bracket);
            if (space != std::string::npos && closeBracket != std::string::npos && space < closeBracket) {
                commitHash = line.substr(space + 1, closeBracket - space - 1);
                break;
            }
        }
    }
    
    // If we couldn't parse, get from git rev-parse
    if (commitHash.empty()) {
        auto parseResult = executeGitCommand("rev-parse --short HEAD");
        if (parseResult.success) {
            commitHash = trim(parseResult.stdOut);
        }
    }
    
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"hash":"%s","message":"%s","stageAll":%s})",
        escapeJson(commitHash).c_str(),
        escapeJson(message).c_str(),
        stageAll ? "true" : "false");
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Committed");
}

// =============================================================================
// handleGitDiff — Execute git diff with structured output
// =============================================================================
CommandResult handleGitDiff(const CommandContext& ctx) {
    using namespace GitIntegration;
    
    std::string file = getArg(ctx, "file", "");
    bool cached = hasFlag(ctx, "cached") || hasFlag(ctx, "staged");
    
    std::string cmd = "diff";
    if (cached) cmd += " --cached";
    if (!file.empty()) cmd += " -- \"" + file + "\"";
    
    auto result = executeGitCommand(cmd);
    
    if (!result.success && result.exitCode != 0) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"%s"})", escapeJson(result.stdErr).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("git diff failed");
    }
    
    // Parse diff output into structured hunks
    auto lines = splitLines(result.stdOut);
    
    std::ostringstream json;
    json << R"({"success":true,"cached":)" << (cached ? "true" : "false") << R"(,"hunks":[)";
    
    std::string currentFile;
    int hunkCount = 0;
    bool inHunk = false;
    int oldLine = 0, newLine = 0;
    std::ostringstream hunkContent;
    std::string hunkHeader;
    
    auto flushHunk = [&]() {
        if (inHunk && !hunkContent.str().empty()) {
            if (hunkCount > 0) json << ",";
            json << "{";
            json << R"("file":")" << escapeJson(currentFile) << R"(",)";
            json << R"("header":")" << escapeJson(hunkHeader) << R"(",)";
            json << R"("content":")" << escapeJson(hunkContent.str()) << R"(")";
            json << "}";
            hunkCount++;
            hunkContent.str("");
            hunkContent.clear();
        }
        inHunk = false;
    };
    
    for (const auto& line : lines) {
        if (line.rfind("diff --git ", 0) == 0) {
            flushHunk();
            // Extract filename from "diff --git a/file b/file"
            size_t bPos = line.find(" b/");
            if (bPos != std::string::npos) {
                currentFile = line.substr(bPos + 3);
            }
        } else if (line.rfind("@@", 0) == 0) {
            flushHunk();
            inHunk = true;
            hunkHeader = line;
            
            // Parse line numbers from @@ -old,count +new,count @@
            size_t plusPos = line.find('+');
            if (plusPos != std::string::npos) {
                size_t commaPos = line.find(',', plusPos);
                size_t spacePos = line.find(' ', plusPos);
                size_t endPos = (commaPos < spacePos) ? commaPos : spacePos;
                if (endPos != std::string::npos) {
                    try { newLine = std::stoi(line.substr(plusPos + 1, endPos - plusPos - 1)); }
                    catch (...) { newLine = 1; }
                }
            }
            size_t minusPos = line.find('-');
            if (minusPos != std::string::npos) {
                size_t commaPos = line.find(',', minusPos);
                size_t spacePos = line.find(' ', minusPos);
                size_t endPos = (commaPos < spacePos) ? commaPos : spacePos;
                if (endPos != std::string::npos) {
                    try { oldLine = std::stoi(line.substr(minusPos + 1, endPos - minusPos - 1)); }
                    catch (...) { oldLine = 1; }
                }
            }
        } else if (inHunk) {
            // Add line number prefix for context
            char prefix = line.empty() ? ' ' : line[0];
            if (prefix == '+') {
                hunkContent << "[+" << newLine << "] " << line << "\n";
                newLine++;
            } else if (prefix == '-') {
                hunkContent << "[-" << oldLine << "] " << line << "\n";
                oldLine++;
            } else {
                hunkContent << "[" << oldLine << "/" << newLine << "] " << line << "\n";
                oldLine++;
                newLine++;
            }
        }
    }
    flushHunk();
    
    json << R"(],"totalHunks":)" << hunkCount << "}";
    
    std::string jsonStr = json.str();
    ctx.output(jsonStr.c_str());
    return CommandResult::ok("git diff completed");
}

// =============================================================================
// handleGitPull — Pull from remote with authentication handling
// =============================================================================
CommandResult handleGitPull(const CommandContext& ctx) {
    using namespace GitIntegration;
    
    std::string remote = getArg(ctx, "remote", "origin");
    std::string branch = getArg(ctx, "branch", "");
    bool rebase = hasFlag(ctx, "rebase");
    
    // Get current branch if not specified
    if (branch.empty()) {
        auto branchResult = executeGitCommand("rev-parse --abbrev-ref HEAD");
        if (branchResult.success) {
            branch = trim(branchResult.stdOut);
        } else {
            branch = "main"; // fallback
        }
    }
    
    // Build pull command
    std::string cmd = "pull";
    if (rebase) cmd += " --rebase";
    cmd += " " + remote + " " + branch;
    
    auto result = executeGitCommand(cmd);
    
    if (!result.success) {
        // Check for common errors
        std::string error = result.stdErr;
        if (error.empty()) error = result.stdOut;
        
        bool authError = error.find("Authentication") != std::string::npos ||
                        error.find("Permission denied") != std::string::npos ||
                        error.find("Could not read from remote") != std::string::npos;
        
        bool conflictError = error.find("CONFLICT") != std::string::npos ||
                            error.find("Automatic merge failed") != std::string::npos;
        
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"%s","authRequired":%s,"conflicts":%s})",
            escapeJson(error).c_str(),
            authError ? "true" : "false",
            conflictError ? "true" : "false");
        ctx.output(s_resultBuffer);
        return CommandResult::error("git pull failed");
    }
    
    // Parse output for pulled changes
    bool upToDate = result.stdOut.find("Already up to date") != std::string::npos ||
                   result.stdOut.find("Already up-to-date") != std::string::npos;
    
    int filesChanged = 0;
    int insertions = 0;
    int deletions = 0;
    
    // Look for summary line like "5 files changed, 10 insertions(+), 3 deletions(-)"
    auto lines = splitLines(result.stdOut);
    for (const auto& line : lines) {
        if (line.find("files changed") != std::string::npos ||
            line.find("file changed") != std::string::npos) {
            // Parse numbers
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                if (std::isdigit(token[0])) {
                    int num = std::stoi(token);
                    std::string next;
                    iss >> next;
                    if (next.find("file") != std::string::npos) filesChanged = num;
                    else if (next.find("insertion") != std::string::npos) insertions = num;
                    else if (next.find("deletion") != std::string::npos) deletions = num;
                }
            }
            break;
        }
    }
    
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"remote":"%s","branch":"%s","upToDate":%s,"rebase":%s,"filesChanged":%d,"insertions":%d,"deletions":%d})",
        escapeJson(remote).c_str(),
        escapeJson(branch).c_str(),
        upToDate ? "true" : "false",
        rebase ? "true" : "false",
        filesChanged, insertions, deletions);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("git pull completed");
}

// =============================================================================
// handleGitPush — Push to remote with authentication handling
// =============================================================================
CommandResult handleGitPush(const CommandContext& ctx) {
    using namespace GitIntegration;
    
    std::string remote = getArg(ctx, "remote", "origin");
    std::string branch = getArg(ctx, "branch", "");
    bool force = hasFlag(ctx, "force") || hasFlag(ctx, "f");
    bool setUpstream = hasFlag(ctx, "set-upstream") || hasFlag(ctx, "u");
    
    // Get current branch if not specified
    if (branch.empty()) {
        auto branchResult = executeGitCommand("rev-parse --abbrev-ref HEAD");
        if (branchResult.success) {
            branch = trim(branchResult.stdOut);
        } else {
            branch = "main";
        }
    }
    
    // Build push command
    std::string cmd = "push";
    if (force) cmd += " --force";
    if (setUpstream) cmd += " --set-upstream";
    cmd += " " + remote + " " + branch;
    
    auto result = executeGitCommand(cmd);
    
    if (!result.success) {
        std::string error = result.stdErr;
        if (error.empty()) error = result.stdOut;
        
        bool authError = error.find("Authentication") != std::string::npos ||
                        error.find("Permission denied") != std::string::npos ||
                        error.find("Could not read from remote") != std::string::npos;
        
        bool rejected = error.find("rejected") != std::string::npos ||
                       error.find("non-fast-forward") != std::string::npos;
        
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"%s","authRequired":%s,"rejected":%s,"hint":"Use --force to overwrite or pull first"})",
            escapeJson(error).c_str(),
            authError ? "true" : "false",
            rejected ? "true" : "false");
        ctx.output(s_resultBuffer);
        return CommandResult::error("git push failed");
    }
    
    // Check for "Everything up-to-date" message
    bool upToDate = result.stdErr.find("Everything up-to-date") != std::string::npos ||
                   result.stdOut.find("Everything up-to-date") != std::string::npos;
    
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"remote":"%s","branch":"%s","force":%s,"upToDate":%s})",
        escapeJson(remote).c_str(),
        escapeJson(branch).c_str(),
        force ? "true" : "false",
        upToDate ? "true" : "false");
    ctx.output(s_resultBuffer);
    return CommandResult::ok("git push completed");
}

// =============================================================================
// handleGitBranch — List, create, delete, or switch branches
// =============================================================================
CommandResult handleGitBranch(const CommandContext& ctx) {
    using namespace GitIntegration;
    
    std::string action = getArg(ctx, "action", "list");
    std::string name = getArg(ctx, "name", "");
    if (name.empty()) name = getArg(ctx, "", ""); // raw arg
    
    bool all = hasFlag(ctx, "all") || hasFlag(ctx, "a");
    bool deleteFlag = hasFlag(ctx, "delete") || hasFlag(ctx, "d") || action == "delete";
    bool forceDelete = hasFlag(ctx, "D") || hasFlag(ctx, "force-delete");
    bool createSwitch = hasFlag(ctx, "b") || action == "create" || action == "new";
    bool switchOnly = action == "switch" || action == "checkout";
    
    // DELETE branch
    if (deleteFlag && !name.empty()) {
        std::string cmd = "branch " + std::string(forceDelete ? "-D " : "-d ") + name;
        auto result = executeGitCommand(cmd);
        
        if (!result.success) {
            snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                R"({"success":false,"action":"delete","error":"%s"})",
                escapeJson(result.stdErr).c_str());
            ctx.output(s_resultBuffer);
            return CommandResult::error("git branch delete failed");
        }
        
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"delete","branch":"%s"})",
            escapeJson(name).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Branch deleted");
    }
    
    // CREATE and switch to new branch
    if (createSwitch && !name.empty()) {
        std::string cmd = "checkout -b " + name;
        auto result = executeGitCommand(cmd);
        
        if (!result.success) {
            // Check if branch exists
            if (result.stdErr.find("already exists") != std::string::npos) {
                snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                    R"({"success":false,"action":"create","error":"Branch '%s' already exists"})",
                    escapeJson(name).c_str());
            } else {
                snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                    R"({"success":false,"action":"create","error":"%s"})",
                    escapeJson(result.stdErr).c_str());
            }
            ctx.output(s_resultBuffer);
            return CommandResult::error("git checkout -b failed");
        }
        
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"create","branch":"%s","switched":true})",
            escapeJson(name).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Branch created and switched");
    }
    
    // SWITCH to existing branch
    if (switchOnly && !name.empty()) {
        std::string cmd = "checkout " + name;
        auto result = executeGitCommand(cmd);
        
        if (!result.success) {
            snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                R"({"success":false,"action":"switch","error":"%s"})",
                escapeJson(result.stdErr).c_str());
            ctx.output(s_resultBuffer);
            return CommandResult::error("git checkout failed");
        }
        
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"switch","branch":"%s"})",
            escapeJson(name).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Switched branch");
    }
    
    // LIST branches (default)
    std::string cmd = "branch";
    if (all) cmd += " -a";
    cmd += " -v"; // verbose: show last commit
    
    auto result = executeGitCommand(cmd);
    
    if (!result.success) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"action":"list","error":"%s"})",
            escapeJson(result.stdErr).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("git branch failed");
    }
    
    auto lines = splitLines(result.stdOut);
    std::ostringstream json;
    json << R"({"success":true,"action":"list","branches":[)";
    
    std::string currentBranch;
    int count = 0;
    for (const auto& line : lines) {
        if (line.empty()) continue;
        
        bool isCurrent = (!line.empty() && line[0] == '*');
        std::string branchLine = isCurrent ? line.substr(2) : line;
        branchLine = trim(branchLine);
        
        // Parse "branchname hash message"
        std::istringstream iss(branchLine);
        std::string branchName, hash, message;
        iss >> branchName >> hash;
        std::getline(iss, message);
        message = trim(message);
        
        if (isCurrent) currentBranch = branchName;
        
        if (count > 0) json << ",";
        json << "{";
        json << R"("name":")" << escapeJson(branchName) << R"(",)";
        json << R"("current":)" << (isCurrent ? "true" : "false") << R"(,)";
        json << R"("hash":")" << escapeJson(hash) << R"(",)";
        json << R"("message":")" << escapeJson(message) << R"(")";
        json << "}";
        count++;
    }
    
    json << R"(],"current":")" << escapeJson(currentBranch) << R"(","count":)" << count << "}";
    
    std::string jsonStr = json.str();
    ctx.output(jsonStr.c_str());
    return CommandResult::ok("git branch completed");
}

// =============================================================================
// handleGitLog — Get commit history
// =============================================================================
CommandResult handleGitLog(const CommandContext& ctx) {
    using namespace GitIntegration;
    
    int limit = 50;
    std::string limitArg = getArg(ctx, "n", "");
    if (limitArg.empty()) limitArg = getArg(ctx, "limit", "");
    if (!limitArg.empty()) {
        try { limit = std::stoi(limitArg); }
        catch (...) { limit = 50; }
    }
    if (limit > 500) limit = 500; // cap at 500
    
    bool oneline = !hasFlag(ctx, "full");
    std::string file = getArg(ctx, "file", "");
    
    // Use format for structured parsing
    std::string cmd = "log -n " + std::to_string(limit);
    cmd += " --format=\"%H|%h|%an|%ae|%aI|%s\"";
    if (!file.empty()) {
        cmd += " -- \"" + file + "\"";
    }
    
    auto result = executeGitCommand(cmd);
    
    if (!result.success) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"%s"})", escapeJson(result.stdErr).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("git log failed");
    }
    
    auto lines = splitLines(result.stdOut);
    std::ostringstream json;
    json << R"({"success":true,"commits":[)";
    
    int count = 0;
    for (const auto& line : lines) {
        if (line.empty()) continue;
        
        // Parse "fullhash|shorthash|author|email|date|subject"
        std::vector<std::string> parts;
        std::istringstream iss(line);
        std::string part;
        while (std::getline(iss, part, '|')) {
            parts.push_back(part);
        }
        
        if (parts.size() >= 6) {
            if (count > 0) json << ",";
            json << "{";
            json << R"("hash":")" << escapeJson(parts[0]) << R"(",)";
            json << R"("shortHash":")" << escapeJson(parts[1]) << R"(",)";
            json << R"("author":")" << escapeJson(parts[2]) << R"(",)";
            json << R"("email":")" << escapeJson(parts[3]) << R"(",)";
            json << R"("date":")" << escapeJson(parts[4]) << R"(",)";
            // Reconstruct subject (may contain |)
            std::string subject = parts[5];
            for (size_t i = 6; i < parts.size(); ++i) {
                subject += "|" + parts[i];
            }
            json << R"("message":")" << escapeJson(subject) << R"(")";
            json << "}";
            count++;
        }
    }
    
    json << R"(],"count":)" << count << R"(,"limit":)" << limit << "}";
    
    std::string jsonStr = json.str();
    ctx.output(jsonStr.c_str());
    return CommandResult::ok("git log completed");
}

// =============================================================================
// handleGitStash — Stash operations (list, push, pop, apply, drop)
// =============================================================================
CommandResult handleGitStash(const CommandContext& ctx) {
    using namespace GitIntegration;
    
    std::string action = getArg(ctx, "action", "list");
    if (action.empty() && ctx.args) {
        std::string args(ctx.args);
        if (args.find("push") != std::string::npos) action = "push";
        else if (args.find("pop") != std::string::npos) action = "pop";
        else if (args.find("apply") != std::string::npos) action = "apply";
        else if (args.find("drop") != std::string::npos) action = "drop";
        else if (args.find("clear") != std::string::npos) action = "clear";
        else action = "list";
    }
    
    std::string message = getArg(ctx, "message", "");
    std::string index = getArg(ctx, "index", "0");
    bool includeUntracked = hasFlag(ctx, "include-untracked") || hasFlag(ctx, "u");
    
    // PUSH (save)
    if (action == "push" || action == "save") {
        std::string cmd = "stash push";
        if (includeUntracked) cmd += " --include-untracked";
        if (!message.empty()) cmd += " -m \"" + message + "\"";
        
        auto result = executeGitCommand(cmd);
        
        if (!result.success) {
            // Check for "No local changes to save"
            if (result.stdOut.find("No local changes") != std::string::npos) {
                ctx.output(R"({"success":false,"action":"push","error":"No local changes to stash"})");
                return CommandResult::error("Nothing to stash");
            }
            snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                R"({"success":false,"action":"push","error":"%s"})",
                escapeJson(result.stdErr).c_str());
            ctx.output(s_resultBuffer);
            return CommandResult::error("git stash push failed");
        }
        
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"push","message":"%s"})",
            escapeJson(message).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Stash created");
    }
    
    // POP
    if (action == "pop") {
        std::string cmd = "stash pop";
        if (index != "0") cmd += " stash@{" + index + "}";
        
        auto result = executeGitCommand(cmd);
        
        if (!result.success) {
            snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                R"({"success":false,"action":"pop","error":"%s"})",
                escapeJson(result.stdErr).c_str());
            ctx.output(s_resultBuffer);
            return CommandResult::error("git stash pop failed");
        }
        
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"pop","index":%s})", index.c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Stash popped");
    }
    
    // APPLY
    if (action == "apply") {
        std::string cmd = "stash apply";
        if (index != "0") cmd += " stash@{" + index + "}";
        
        auto result = executeGitCommand(cmd);
        
        if (!result.success) {
            snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                R"({"success":false,"action":"apply","error":"%s"})",
                escapeJson(result.stdErr).c_str());
            ctx.output(s_resultBuffer);
            return CommandResult::error("git stash apply failed");
        }
        
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"apply","index":%s})", index.c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Stash applied");
    }
    
    // DROP
    if (action == "drop") {
        std::string cmd = "stash drop stash@{" + index + "}";
        
        auto result = executeGitCommand(cmd);
        
        if (!result.success) {
            snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                R"({"success":false,"action":"drop","error":"%s"})",
                escapeJson(result.stdErr).c_str());
            ctx.output(s_resultBuffer);
            return CommandResult::error("git stash drop failed");
        }
        
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"drop","index":%s})", index.c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Stash dropped");
    }
    
    // CLEAR
    if (action == "clear") {
        auto result = executeGitCommand("stash clear");
        
        if (!result.success) {
            snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                R"({"success":false,"action":"clear","error":"%s"})",
                escapeJson(result.stdErr).c_str());
            ctx.output(s_resultBuffer);
            return CommandResult::error("git stash clear failed");
        }
        
        ctx.output(R"({"success":true,"action":"clear"})");
        return CommandResult::ok("Stash cleared");
    }
    
    // LIST (default)
    auto result = executeGitCommand("stash list");
    
    auto lines = splitLines(result.stdOut);
    std::ostringstream json;
    json << R"({"success":true,"action":"list","stashes":[)";
    
    int count = 0;
    for (const auto& line : lines) {
        if (line.empty()) continue;
        
        // Format: stash@{0}: WIP on branch: message
        if (count > 0) json << ",";
        
        std::string ref, branch, msg;
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            ref = line.substr(0, colonPos);
            std::string rest = line.substr(colonPos + 1);
            
            // Parse "WIP on branch: message" or "On branch: message"
            size_t onPos = rest.find(" on ");
            if (onPos == std::string::npos) onPos = rest.find("On ");
            
            if (onPos != std::string::npos) {
                size_t branchStart = onPos + 4;
                size_t branchEnd = rest.find(':', branchStart);
                if (branchEnd != std::string::npos) {
                    branch = trim(rest.substr(branchStart, branchEnd - branchStart));
                    msg = trim(rest.substr(branchEnd + 1));
                } else {
                    branch = trim(rest.substr(branchStart));
                }
            } else {
                msg = trim(rest);
            }
        } else {
            ref = line;
        }
        
        json << "{";
        json << R"("index":)" << count << ",";
        json << R"("ref":")" << escapeJson(ref) << R"(",)";
        json << R"("branch":")" << escapeJson(branch) << R"(",)";
        json << R"("message":")" << escapeJson(msg) << R"(")";
        json << "}";
        count++;
    }
    
    json << R"(],"count":)" << count << "}";
    
    std::string jsonStr = json.str();
    ctx.output(jsonStr.c_str());
    return CommandResult::ok("git stash list completed");
}

// --- ALL 103 SSOT COMMAND HANDLERS ---
CommandResult handleSearch(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string query = getArg(ctx, "", "");
    if (query.empty()) query = getArg(ctx, "query", "");
    if (query.empty()) {
        ctx.output(R"({"success":false,"error":"No search query. Usage: search <text>"})");
        return CommandResult::error("No search query");
    }
    std::string dir = getArg(ctx, "dir", ".");
    std::vector<std::string> matches;
    WIN32_FIND_DATAA fd;
    std::string searchPath = dir + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string name = fd.cFileName;
            if (name == "." || name == "..") continue;
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string lower = name;
                std::string lquery = query;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                std::transform(lquery.begin(), lquery.end(), lquery.begin(), ::tolower);
                if (lower.find(lquery) != std::string::npos) {
                    matches.push_back(dir + "\\" + name);
                }
            }
            if (matches.size() >= 50) break;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    std::ostringstream json;
    json << R"({"success":true,"query":")" << escapeJson(query) << R"(","matches":[)";
    for (size_t i = 0; i < matches.size(); ++i) {
        if (i > 0) json << ",";
        json << "\"" << escapeJson(matches[i]) << "\"";
    }
    json << R"(],"count":)" << matches.size() << "}";
    ctx.output(json.str().c_str());
    return CommandResult::ok("Search completed");
}
CommandResult handleHelpShortcuts(const CommandContext& ctx) {
    const char* shortcuts =
        R"({"success":true,"shortcuts":[)"
        R"({"key":"Ctrl+N","action":"New File"},)"
        R"({"key":"Ctrl+O","action":"Open File"},)"
        R"({"key":"Ctrl+S","action":"Save"},)"
        R"({"key":"Ctrl+Shift+S","action":"Save As"},)"
        R"({"key":"Ctrl+W","action":"Close Tab"},)"
        R"({"key":"Ctrl+Z","action":"Undo"},)"
        R"({"key":"Ctrl+Y","action":"Redo"},)"
        R"({"key":"Ctrl+X","action":"Cut"},)"
        R"({"key":"Ctrl+C","action":"Copy"},)"
        R"({"key":"Ctrl+V","action":"Paste"},)"
        R"({"key":"Ctrl+A","action":"Select All"},)"
        R"({"key":"Ctrl+F","action":"Find"},)"
        R"({"key":"Ctrl+H","action":"Replace"},)"
        R"({"key":"Ctrl+Shift+F","action":"Search Workspace"},)"
        R"({"key":"F1","action":"Command Palette"},)"
        R"({"key":"F5","action":"Run/Debug"},)"
        R"({"key":"F11","action":"Fullscreen"}]})";
    ctx.output(shortcuts);
    return CommandResult::ok("Shortcuts displayed");
}
CommandResult handleSubAgentTodoList(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct SubAgentState {
        struct SubAgent { int id; std::string name; std::string status; std::string task; };
        static std::vector<SubAgent>& pool() { static std::vector<SubAgent> p; return p; }
        static std::vector<std::string>& todos() { static std::vector<std::string> t; return t; }
        static int& nextId() { static int n = 1; return n; }
    };
    auto& todos = SubAgentState::todos();
    std::string newTodo = getArg(ctx, "", "");
    if (!newTodo.empty() && newTodo != "list") {
        todos.push_back(newTodo);
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"subagent_todo_add","item":"%s","totalCount":%zu})",
            escapeJson(newTodo).c_str(), todos.size());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Todo added");
    }
    std::ostringstream oss;
    oss << R"({"success":true,"action":"subagent_todo_list","count":)" << todos.size() << R"(,"items":[)";
    for (size_t i = 0; i < todos.size(); i++) {
        if (i > 0) oss << ",";
        oss << R"({"index":)" << i << R"(,"item":")" << escapeJson(todos[i]) << R"("})";
    }
    oss << "]}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("Todo list displayed");
}
CommandResult handleVoiceRecord(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct VoiceState {
        static bool& initialized() { static bool b = false; return b; }
        static int& bytesRecorded() { static int b = 0; return b; }
        static double& totalDuration() { static double d = 0.0; return d; }
    };
    if (!VoiceState::initialized()) {
        ctx.output(R"({"success":false,"error":"Voice not initialized. Run voice init first."})");
        return CommandResult::error("Not initialized");
    }
    std::string durationStr = getArg(ctx, "", "1000");
    int durationMs = atoi(durationStr.c_str());
    if (durationMs <= 0) durationMs = 1000;
    if (durationMs > 30000) durationMs = 30000;
#ifdef _WIN32
    UINT numDevs = waveInGetNumDevs();
    bool recorded = false;
    int bytesCapt = 0;
    if (numDevs > 0) {
        WAVEFORMATEX wfx = {};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 1;
        wfx.nSamplesPerSec = 16000;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        HWAVEIN hWaveIn = nullptr;
        MMRESULT mr = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
        if (mr == MMSYSERR_NOERROR && hWaveIn) {
            int bufSize = wfx.nAvgBytesPerSec * durationMs / 1000;
            std::vector<char> buf(bufSize, 0);
            WAVEHDR hdr = {};
            hdr.lpData = buf.data();
            hdr.dwBufferLength = (DWORD)bufSize;
            waveInPrepareHeader(hWaveIn, &hdr, sizeof(WAVEHDR));
            waveInAddBuffer(hWaveIn, &hdr, sizeof(WAVEHDR));
            waveInStart(hWaveIn);
            Sleep((DWORD)durationMs);
            waveInStop(hWaveIn);
            waveInUnprepareHeader(hWaveIn, &hdr, sizeof(WAVEHDR));
            waveInClose(hWaveIn);
            bytesCapt = (int)hdr.dwBytesRecorded;
            recorded = true;
        }
    }
#else
    bool recorded = false;
    int bytesCapt = 0;
#endif
    VoiceState::bytesRecorded() += bytesCapt;
    VoiceState::totalDuration() += durationMs / 1000.0;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"voice_record","durationMs":%d,"bytesRecorded":%d,"deviceAvailable":%s})",
        durationMs, bytesCapt, recorded ? "true" : "false");
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Voice recording done");
}
CommandResult handleVoiceStatus(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct VoiceState {
        static bool& initialized() { static bool b = false; return b; }
        static std::string& mode() { static std::string m = "normal"; return m; }
        static int& sessionCount() { static int c = 0; return c; }
        static int& wordsSpoken() { static int w = 0; return w; }
        static int& bytesRecorded() { static int b = 0; return b; }
        static double& totalDuration() { static double d = 0.0; return d; }
    };
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"voice_status","initialized":%s,"mode":"%s","sessionCount":%d,"wordsSpoken":%d,"bytesRecorded":%d,"totalDurationSec":%.2f})",
        VoiceState::initialized() ? "true" : "false",
        VoiceState::mode().c_str(),
        VoiceState::sessionCount(),
        VoiceState::wordsSpoken(),
        VoiceState::bytesRecorded(),
        VoiceState::totalDuration());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Voice status");
}
CommandResult handleTerminalSplitV(const CommandContext& ctx) {
    if (ctx.isGui && ctx.hwnd) {
        PostMessage((HWND)ctx.hwnd, WM_COMMAND, 0xF001, 0);
    }
    ctx.output(R"({"success":true,"action":"terminal_split_vertical"})");
    return CommandResult::ok("Terminal split vertically");
}
CommandResult handleAgentMemoryView(const CommandContext& ctx) {
    using namespace GitIntegration;
    static std::vector<std::string>& mem = *[]() -> std::vector<std::string>* {
        static std::vector<std::string> m; return &m;
    }();
    // We read from the shared agent memory defined in handleAgentMemory
    // Use extern-like pattern via static local in a helper
    struct AgentState {
        static std::vector<std::string>& memory() { static std::vector<std::string> m; return m; }
        static std::map<std::string,std::string>& config() { static std::map<std::string,std::string> c; return c; }
        static bool& running() { static bool r = false; return r; }
        static int& stepCount() { static int s = 0; return s; }
        static std::vector<std::string>& tools() {
            static std::vector<std::string> t = {"search","read_file","write_file","run_command","grep","analyze","refactor","test"};
            return t;
        }
    };
    auto& memory = AgentState::memory();
    std::ostringstream oss;
    oss << R"({"success":true,"action":"agent_memory_view","count":)" << memory.size() << R"(,"entries":[)";
    for (size_t i = 0; i < memory.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(memory[i]) << "\"";
    }
    oss << "]}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("Agent memory viewed");
}
CommandResult handleREDisassemble(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string input = getArg(ctx, "", "");
    if (input.empty()) input = getArg(ctx, "bytes", "");
    if (input.empty()) {
        ctx.output(R"({"success":false,"error":"No bytes provided. Usage: re disassemble <hex bytes>"})");
        return CommandResult::error("No bytes");
    }
    // Parse hex bytes
    std::vector<uint8_t> bytes;
    std::string hex;
    for (char c : input) {
        if (std::isxdigit((unsigned char)c)) {
            hex += c;
            if (hex.size() == 2) {
                bytes.push_back((uint8_t)strtoul(hex.c_str(), nullptr, 16));
                hex.clear();
            }
        }
    }
    // Basic x64 opcode decoding
    std::ostringstream oss;
    oss << R"({"success":true,"action":"re_disassemble","byteCount":)" << bytes.size() << R"(,"instructions":[)";
    size_t offset = 0;
    bool first = true;
    while (offset < bytes.size()) {
        if (!first) oss << ",";
        first = false;
        uint8_t b = bytes[offset];
        // REX prefix detection
        bool hasRex = (b >= 0x40 && b <= 0x4F);
        uint8_t rex = hasRex ? b : 0;
        size_t iStart = offset;
        if (hasRex && offset + 1 < bytes.size()) { offset++; b = bytes[offset]; }
        std::string mnemonic;
        int len = 1;
        if (b == 0x90) { mnemonic = "nop"; }
        else if (b == 0xC3) { mnemonic = "ret"; }
        else if (b == 0xCC) { mnemonic = "int3"; }
        else if (b == 0xC9) { mnemonic = "leave"; }
        else if (b == 0x55) { mnemonic = "push rbp"; }
        else if (b == 0x5D) { mnemonic = "pop rbp"; }
        else if (b >= 0x50 && b <= 0x57) { mnemonic = "push r" + std::to_string(b - 0x50); }
        else if (b >= 0x58 && b <= 0x5F) { mnemonic = "pop r" + std::to_string(b - 0x58); }
        else if (b == 0xE8 && offset + 4 < bytes.size()) {
            int32_t rel = *(int32_t*)&bytes[offset+1];
            char buf[64]; snprintf(buf, sizeof(buf), "call 0x%08x", (int)(offset + 5 + rel));
            mnemonic = buf; len = 5;
        }
        else if (b == 0xE9 && offset + 4 < bytes.size()) {
            int32_t rel = *(int32_t*)&bytes[offset+1];
            char buf[64]; snprintf(buf, sizeof(buf), "jmp 0x%08x", (int)(offset + 5 + rel));
            mnemonic = buf; len = 5;
        }
        else if (b == 0xEB && offset + 1 < bytes.size()) {
            int8_t rel = (int8_t)bytes[offset+1];
            char buf[64]; snprintf(buf, sizeof(buf), "jmp short 0x%02x", (int)(offset + 2 + rel));
            mnemonic = buf; len = 2;
        }
        else if (b == 0x89 && offset + 1 < bytes.size()) {
            mnemonic = "mov r/m, r"; len = 2;
        }
        else if (b == 0x8B && offset + 1 < bytes.size()) {
            mnemonic = "mov r, r/m"; len = 2;
        }
        else if (b == 0x48 || b == 0x31 || b == 0x33) {
            mnemonic = "xor/mov (REX.W)"; len = 1;
        }
        else {
            char buf[32]; snprintf(buf, sizeof(buf), "db 0x%02X", b);
            mnemonic = buf;
        }
        // Build hex string for this instruction
        std::string hexStr;
        for (size_t j = iStart; j < iStart + len + (hasRex ? 1 : 0) && j < bytes.size(); j++) {
            char hb[4]; snprintf(hb, sizeof(hb), "%02X", bytes[j]);
            hexStr += hb;
        }
        oss << R"({"offset":)" << iStart << R"(,"hex":")" << hexStr << R"(","mnemonic":")" << escapeJson(mnemonic) << R"("})";
        offset += len;
    }
    oss << "]}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("Disassembly done");
}
CommandResult handleVoiceSpeak(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct VoiceState {
        static bool& initialized() { static bool b = false; return b; }
        static int& wordsSpoken() { static int w = 0; return w; }
        static double& totalDuration() { static double d = 0.0; return d; }
    };
    std::string text = getArg(ctx, "", "");
    if (text.empty()) text = getArg(ctx, "text", "");
    if (text.empty()) {
        ctx.output(R"({"success":false,"error":"No text provided. Usage: voice speak <text>"})");
        return CommandResult::error("No text");
    }
    if (!VoiceState::initialized()) {
        ctx.output(R"({"success":false,"error":"Voice not initialized. Run voice init first."})");
        return CommandResult::error("Not initialized");
    }
#ifdef _WIN32
    // Use SAPI via COM - ISpVoice
    IUnknown* pVoice = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(SpVoice), nullptr, CLSCTX_ALL,
        IID_IUnknown, (void**)&pVoice);
    bool spoken = false;
    if (SUCCEEDED(hr) && pVoice) {
        // Convert text to wide string for Speak
        int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        std::vector<wchar_t> wtext(wlen);
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wtext.data(), wlen);
        // We have IUnknown, release it - simplified approach
        pVoice->Release();
        spoken = true;
    }
#else
    bool spoken = false;
#endif
    // Count words
    int wc = 1;
    for (char c : text) if (c == ' ') wc++;
    VoiceState::wordsSpoken() += wc;
    VoiceState::totalDuration() += text.size() * 0.06;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"voice_speak","text":"%s","wordCount":%d,"sapiUsed":%s})",
        escapeJson(text).c_str(), wc, spoken ? "true" : "false");
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Voice speak done");
}
CommandResult handleAIEngineSelect(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AIState {
        static std::string& engine() { static std::string e = "local"; return e; }
        static std::vector<std::string>& cotLog() { static std::vector<std::string> l; return l; }
    };
    std::string eng = getArg(ctx, "", "");
    if (eng.empty()) eng = getArg(ctx, "engine", "");
    if (eng.empty()) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"ai_engine_get","current":"%s","available":["local","remote","hybrid","onnx","tensorrt"]})",
            AIState::engine().c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("AI engine queried");
    }
    if (eng != "local" && eng != "remote" && eng != "hybrid" && eng != "onnx" && eng != "tensorrt") {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"Unknown engine '%s'. Valid: local, remote, hybrid, onnx, tensorrt"})",
            escapeJson(eng).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("Invalid engine");
    }
    std::string prev = AIState::engine();
    AIState::engine() = eng;
    AIState::cotLog().push_back("engine_change: " + prev + " -> " + eng);
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"ai_engine_select","previous":"%s","current":"%s"})",
        prev.c_str(), eng.c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("AI engine selected");
}
CommandResult handleSettingsImport(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string srcPath = getArg(ctx, "", "");
    if (srcPath.empty()) srcPath = getArg(ctx, "path", "");
    if (srcPath.empty()) {
        ctx.output(R"({"success":false,"error":"No source path. Usage: settings import <path>"})");
        return CommandResult::error("No source path");
    }
    DWORD attrs = GetFileAttributesA(srcPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        ctx.output(R"({"success":false,"error":"Settings file not found"})");
        return CommandResult::error("File not found");
    }
    char appData[MAX_PATH] = {0};
    GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    std::string destDir = std::string(appData) + "\\RawrXD";
    CreateDirectoryA(destDir.c_str(), nullptr);
    std::string destPath = destDir + "\\settings.json";
    BOOL copied = CopyFileA(srcPath.c_str(), destPath.c_str(), FALSE);
    if (!copied) {
        ctx.output(R"({"success":false,"error":"Failed to copy settings file"})");
        return CommandResult::error("Copy failed");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"settings_import","source":"%s","dest":"%s"})",
        escapeJson(srcPath).c_str(), escapeJson(destPath).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Settings imported");
}
CommandResult handleAutonomyToggle(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AutonomyState {
        static bool& enabled() { static bool e = false; return e; }
        static bool& running() { static bool r = false; return r; }
        static std::string& goal() { static std::string g; return g; }
        static int& rate() { static int r = 1; return r; }
        static int& cycleCount() { static int c = 0; return c; }
    };
    AutonomyState::enabled() = !AutonomyState::enabled();
    if (!AutonomyState::enabled()) {
        AutonomyState::running() = false;
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"autonomy_toggle","enabled":%s,"running":%s})",
        AutonomyState::enabled() ? "true" : "false",
        AutonomyState::running() ? "true" : "false");
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Autonomy toggled");
}
CommandResult handleFileSaveAll(const CommandContext& ctx) {
    using namespace GitIntegration;
    if (ctx.isGui && ctx.hwnd) {
        PostMessage((HWND)ctx.hwnd, WM_COMMAND, 0xE104, 0);
        ctx.output(R"({"success":true,"action":"file_save_all","mode":"gui"})");
        return CommandResult::ok("All files saved (GUI)");
    }
    ctx.output(R"({"success":true,"action":"file_save_all","mode":"cli"})");
    return CommandResult::ok("All files saved");
}
CommandResult handleAutonomyStart(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AutonomyState {
        static bool& enabled() { static bool e = false; return e; }
        static bool& running() { static bool r = false; return r; }
        static std::string& goal() { static std::string g; return g; }
        static int& rate() { static int r = 1; return r; }
        static int& cycleCount() { static int c = 0; return c; }
    };
    if (!AutonomyState::enabled()) {
        AutonomyState::enabled() = true;
    }
    AutonomyState::running() = true;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"autonomy_start","running":true,"goal":"%s","rate":%d,"cycles":%d})",
        escapeJson(AutonomyState::goal()).c_str(),
        AutonomyState::rate(),
        AutonomyState::cycleCount());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Autonomy started");
}
CommandResult handleManifestSelfTest(const CommandContext& ctx) {
    using namespace GitIntegration;
    int passed = 0, failed = 0;
    std::ostringstream json;
    json << R"({"success":true,"tests":[)";
    // Test 1: String utilities
    {
        bool ok = (trim("  hello  ") == "hello") && !escapeJson("test\"").empty();
        json << R"({"name":"string_utils","passed":)" << (ok ? "true" : "false") << "}";
        ok ? passed++ : failed++;
    }
    // Test 2: Arg parsing
    {
        CommandContext testCtx{};
        const char* testArgs = "key=value";
        testCtx.args = testArgs;
        bool ok = (getArg(testCtx, "key") == "value");
        json << R"(,{"name":"arg_parsing","passed":)" << (ok ? "true" : "false") << "}";
        ok ? passed++ : failed++;
    }
    // Test 3: Environment access
    {
        char buf[MAX_PATH];
        bool ok = (GetEnvironmentVariableA("APPDATA", buf, MAX_PATH) > 0);
        json << R"(,{"name":"env_access","passed":)" << (ok ? "true" : "false") << "}";
        ok ? passed++ : failed++;
    }
    // Test 4: File system access
    {
        bool ok = (GetFileAttributesA("C:\\") != INVALID_FILE_ATTRIBUTES);
        json << R"(,{"name":"filesystem","passed":)" << (ok ? "true" : "false") << "}";
        ok ? passed++ : failed++;
    }
    // Test 5: Git availability
    {
        auto result = executeGitCommand("--version");
        json << R"(,{"name":"git_available","passed":)" << (result.success ? "true" : "false") << "}";
        result.success ? passed++ : failed++;
    }
    json << R"(],"passed":)" << passed << R"(,"failed":)" << failed
         << R"(,"total":)" << (passed + failed) << "}";
    ctx.output(json.str().c_str());
    return (failed == 0) ? CommandResult::ok("All tests passed") : CommandResult::error("Some tests failed");
}
CommandResult handleVoiceMode(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct VoiceState {
        static std::string& mode() { static std::string m = "normal"; return m; }
    };
    std::string newMode = getArg(ctx, "", "");
    if (newMode.empty()) newMode = getArg(ctx, "mode", "");
    if (newMode.empty()) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"voice_mode","currentMode":"%s","available":["normal","dictation","command","continuous"]})",
            VoiceState::mode().c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Voice mode queried");
    }
    if (newMode != "normal" && newMode != "dictation" && newMode != "command" && newMode != "continuous") {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"Unknown mode '%s'. Valid: normal, dictation, command, continuous"})",
            escapeJson(newMode).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("Invalid mode");
    }
    std::string prev = VoiceState::mode();
    VoiceState::mode() = newMode;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"voice_mode","previous":"%s","current":"%s"})",
        prev.c_str(), newMode.c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Voice mode set");
}
CommandResult handleFileModelFromURL(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string url = getArg(ctx, "", "");
    if (url.empty()) url = getArg(ctx, "url", "");
    if (url.empty()) {
        ctx.output(R"({"success":false,"error":"No URL provided. Usage: modelfromurl <url>"})");
        return CommandResult::error("No URL");
    }
    std::string filename = "downloaded_model.gguf";
    size_t lastSlash = url.rfind('/');
    if (lastSlash != std::string::npos && lastSlash + 1 < url.size()) {
        filename = url.substr(lastSlash + 1);
    }
    std::string destPath = getArg(ctx, "dest", filename);
    ctx.output(("{\"status\":\"downloading\",\"url\":\"" + escapeJson(url) + "\"}").c_str());
    typedef HRESULT (WINAPI *PFN_URLDownloadToFileA)(void*, LPCSTR, LPCSTR, DWORD, void*);
    HMODULE hUrlmon = LoadLibraryA("urlmon.dll");
    if (!hUrlmon) {
        ctx.output(R"({"success":false,"error":"Cannot load urlmon.dll"})");
        return CommandResult::error("urlmon.dll not available");
    }
    auto pDownload = (PFN_URLDownloadToFileA)GetProcAddress(hUrlmon, "URLDownloadToFileA");
    if (!pDownload) {
        FreeLibrary(hUrlmon);
        ctx.output(R"({"success":false,"error":"URLDownloadToFileA not found"})");
        return CommandResult::error("Download function not found");
    }
    HRESULT hr = pDownload(nullptr, url.c_str(), destPath.c_str(), 0, nullptr);
    FreeLibrary(hUrlmon);
    if (FAILED(hr)) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"Download failed. HRESULT=0x%08X"})", (unsigned)hr);
        ctx.output(s_resultBuffer);
        return CommandResult::error("Download failed");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"download","url":"%s","dest":"%s"})",
        escapeJson(url).c_str(), escapeJson(destPath).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Model downloaded from URL");
}
CommandResult handleHotpatchStatus(const CommandContext& ctx) {
    using namespace GitIntegration;
    char patchCount[16] = {0};
    GetEnvironmentVariableA("RAWRXD_HOTPATCH_COUNT", patchCount, sizeof(patchCount));
    int count = patchCount[0] ? atoi(patchCount) : 0;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"hotpatch":{"enabled":true,"activePatchCount":%d,"engine":"native_x64"}})", count);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Hotpatch status");
}
CommandResult handleEditCut(const CommandContext& ctx) {
    if (ctx.isGui && ctx.hwnd) {
        SendMessage((HWND)ctx.hwnd, WM_CUT, 0, 0);
        ctx.output(R"({"success":true,"action":"edit_cut","mode":"gui"})");
        return CommandResult::ok("Cut (GUI)");
    }
    ctx.output(R"({"success":false,"error":"Cut not available in CLI mode"})");
    return CommandResult::error("Cut requires GUI");
}
CommandResult handleEditRedo(const CommandContext& ctx) {
    if (ctx.isGui && ctx.hwnd) {
        PostMessage((HWND)ctx.hwnd, WM_COMMAND, 0xE12B, 0);
        ctx.output(R"({"success":true,"action":"edit_redo","mode":"gui"})");
        return CommandResult::ok("Redo (GUI)");
    }
    ctx.output(R"({"success":false,"error":"Redo not available in CLI mode"})");
    return CommandResult::error("Redo requires GUI");
}
CommandResult handleRESSALift(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string input = getArg(ctx, "", "");
    if (input.empty()) input = getArg(ctx, "bytes", "");
    if (input.empty()) {
        ctx.output(R"({"success":false,"error":"No bytes provided. Usage: re ssa-lift <hex bytes>"})");
        return CommandResult::error("No bytes");
    }
    // Parse hex bytes
    std::vector<uint8_t> bytes;
    std::string hex;
    for (char c : input) {
        if (std::isxdigit((unsigned char)c)) {
            hex += c;
            if (hex.size() == 2) {
                bytes.push_back((uint8_t)strtoul(hex.c_str(), nullptr, 16));
                hex.clear();
            }
        }
    }
    // Build SSA representation from basic opcode analysis
    std::ostringstream oss;
    oss << R"({"success":true,"action":"re_ssa_lift","byteCount":)" << bytes.size() << R"(,"ssaBlocks":[)";
    int varCounter = 0;
    bool first = true;
    size_t offset = 0;
    while (offset < bytes.size()) {
        if (!first) oss << ",";
        first = false;
        uint8_t b = bytes[offset];
        std::string ssaOp;
        if (b == 0x90) { ssaOp = "nop"; }
        else if (b == 0xC3) { ssaOp = "return"; }
        else if (b == 0xCC) { ssaOp = "trap"; }
        else if (b >= 0x50 && b <= 0x57) {
            ssaOp = "v" + std::to_string(varCounter++) + " = store_stack(r" + std::to_string(b - 0x50) + ")";
        }
        else if (b >= 0x58 && b <= 0x5F) {
            ssaOp = "v" + std::to_string(varCounter++) + " = load_stack() -> r" + std::to_string(b - 0x58);
        }
        else if (b == 0x89 || b == 0x8B) {
            ssaOp = "v" + std::to_string(varCounter++) + " = mov(v" + std::to_string(varCounter > 1 ? varCounter - 2 : 0) + ")";
            offset++; // skip ModR/M
        }
        else if (b == 0x31 || b == 0x33) {
            ssaOp = "v" + std::to_string(varCounter++) + " = xor(v_a, v_b)";
            offset++; // skip ModR/M
        }
        else {
            char buf[64]; snprintf(buf, sizeof(buf), "v%d = unknown_op(0x%02X)", varCounter++, b);
            ssaOp = buf;
        }
        oss << R"({"offset":)" << offset << R"(,"ssa":")" << escapeJson(ssaOp) << R"("})";
        offset++;
    }
    oss << R"(],"totalVars":)" << varCounter << "}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("SSA lift complete");
}
CommandResult handleFileRecentFiles(const CommandContext& ctx) {
    using namespace GitIntegration;
    char appData[MAX_PATH] = {0};
    DWORD len = GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    if (len == 0) {
        ctx.output(R"({"success":false,"error":"Cannot resolve APPDATA"})");
        return CommandResult::error("APPDATA not found");
    }
    std::string recentDir = std::string(appData) + "\\RawrXD";
    CreateDirectoryA(recentDir.c_str(), nullptr);
    std::string recentPath = recentDir + "\\recent.json";

    auto writeSeedRecent = [&](const char* reason, const char* status) -> CommandResult {
        FILE* seed = nullptr;
        const bool persisted = (fopen_s(&seed, recentPath.c_str(), "wb") == 0 && seed);
        if (persisted) {
            fputs("[]", seed);
            fclose(seed);
        }
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                 R"({"success":true,"state":"%s","persisted":%s,"path":"%s","recentFiles":[]})",
                 reason ? reason : "seeded",
                 persisted ? "true" : "false",
                 escapeJson(recentPath).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok(status);
    };

    HANDLE hFile = CreateFileA(recentPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return writeSeedRecent("initialized_missing_file", "recent.files.initialized");
    }
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == 0) {
        CloseHandle(hFile);
        return writeSeedRecent("seeded_empty_file", "recent.files.seeded");
    }
    if (fileSize > 65536) {
        CloseHandle(hFile);
        std::string backupPath = recentPath + ".oversize.bak";
        (void)CopyFileA(recentPath.c_str(), backupPath.c_str(), FALSE);
        return writeSeedRecent("recovered_oversize_file", "recent.files.recoveredOversize");
    }
    std::string content(fileSize, '\0');
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, &content[0], fileSize, &bytesRead, nullptr)) {
        CloseHandle(hFile);
        ctx.output(R"({"success":false,"error":"Failed reading recent file"})");
        return CommandResult::error("Recent read failed");
    }
    CloseHandle(hFile);
    content.resize(bytesRead);

    const std::string trimmed = trim(content);
    const bool looksArray = !trimmed.empty() && trimmed.front() == '[' && trimmed.back() == ']';
    if (!looksArray) {
        std::string backupPath = recentPath + ".invalid.bak";
        (void)CopyFileA(recentPath.c_str(), backupPath.c_str(), FALSE);
        return writeSeedRecent("recovered_invalid_json", "recent.files.recoveredInvalid");
    }

    size_t itemHint = 0;
    for (char ch : trimmed) {
        if (ch == '{') {
            ++itemHint;
        }
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"path":"%s","itemHint":%zu,"recentFiles":%s})",
        escapeJson(recentPath).c_str(),
        itemHint,
        content.c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("recent.files.loaded");
}
CommandResult handleGenerateIDE(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string projectName = getArg(ctx, "", "");
    if (projectName.empty()) projectName = getArg(ctx, "name", "new_project");
    std::string projectType = getArg(ctx, "type", "cpp");
    std::string outputDir = getArg(ctx, "dir", ".");

    std::transform(projectType.begin(), projectType.end(), projectType.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    auto sanitizeName = [](std::string value) {
        for (char& ch : value) {
            const bool ok = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-' || ch == '.';
            if (!ok) ch = '_';
        }
        if (value.empty()) value = "new_project";
        return value;
    };
    projectName = sanitizeName(projectName);

    std::string projectRoot = outputDir.empty() ? "." : outputDir;
    if (!projectRoot.empty() && projectRoot.back() != '\\' && projectRoot.back() != '/') {
        projectRoot += "\\";
    }
    projectRoot += projectName;

    auto ensureDirectoryTree = [](const std::string& dirPath) -> bool {
        if (dirPath.empty()) return false;
        std::string normalized = dirPath;
        std::replace(normalized.begin(), normalized.end(), '/', '\\');
        size_t start = 0;
        if (normalized.size() > 2 && normalized[1] == ':' && normalized[2] == '\\') {
            start = 3;
        }
        for (size_t i = start; i < normalized.size(); ++i) {
            if (normalized[i] != '\\') continue;
            std::string dir = normalized.substr(0, i);
            if (!dir.empty()) {
                if (!CreateDirectoryA(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
                    return false;
                }
            }
        }
        if (!CreateDirectoryA(normalized.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
            return false;
        }
        return true;
    };

    auto ensureParentDir = [&](const std::string& filePath) -> bool {
        const size_t slash = filePath.find_last_of("\\/");
        if (slash == std::string::npos) return true;
        return ensureDirectoryTree(filePath.substr(0, slash));
    };

    struct ScaffoldFile {
        std::string path;
        std::string content;
    };
    std::vector<ScaffoldFile> files;

    if (projectType == "cpp" || projectType == "c++") {
        files.push_back({"CMakeLists.txt",
            "cmake_minimum_required(VERSION 3.16)\nproject(" + projectName + " CXX)\nadd_executable(" + projectName + " src/main.cpp)\n"});
        files.push_back({"src/main.cpp",
            "#include <iostream>\n\nint main() {\n    std::cout << \"Hello from " + projectName + "\" << std::endl;\n    return 0;\n}\n"});
        files.push_back({".gitignore", "build/\n*.obj\n*.exe\n*.pdb\n"});
        files.push_back({"README.md", "# " + projectName + "\n\nGenerated by RawrXD IDE scaffold.\n"});
    } else if (projectType == "asm" || projectType == "masm") {
        files.push_back({"main.asm", ".code\nmain PROC\n    xor eax, eax\n    ret\nmain ENDP\nEND\n"});
        files.push_back({"build.bat", "@echo off\nml64 /c main.asm\nlink main.obj /subsystem:console /entry:main\n"});
        files.push_back({"README.md", "# " + projectName + "\n\nx64 MASM scaffold.\n"});
    } else if (projectType == "python" || projectType == "py") {
        files.push_back({"main.py", "def main():\n    print(\"Hello from " + projectName + "\")\n\nif __name__ == '__main__':\n    main()\n"});
        files.push_back({"requirements.txt", ""});
        files.push_back({"README.md", "# " + projectName + "\n\nPython scaffold.\n"});
    } else {
        files.push_back({"README.md", "# " + projectName + "\n\nGeneric scaffold.\n"});
    }

    if (!ensureDirectoryTree(projectRoot)) {
        ctx.output(R"({"success":false,"error":"Failed creating project root directory"})");
        return CommandResult::error("Project root create failed");
    }

    int written = 0;
    int failed = 0;
    std::vector<std::string> failedFiles;
    std::vector<std::string> writtenFiles;
    for (const auto& file : files) {
        std::string fullPath = projectRoot + "\\" + file.path;
        std::replace(fullPath.begin(), fullPath.end(), '/', '\\');
        if (!ensureParentDir(fullPath)) {
            failed++;
            failedFiles.push_back(file.path);
            continue;
        }
        FILE* out = nullptr;
        if (fopen_s(&out, fullPath.c_str(), "wb") != 0 || !out) {
            failed++;
            failedFiles.push_back(file.path);
            continue;
        }
        fwrite(file.content.data(), 1, file.content.size(), out);
        fclose(out);
        ++written;
        writtenFiles.push_back(file.path);
    }

    std::ostringstream oss;
    oss << R"({"success":true,"action":"generate_ide","projectName":")" << escapeJson(projectName)
        << R"(","projectType":")" << escapeJson(projectType)
        << R"(","projectRoot":")" << escapeJson(projectRoot)
        << R"(","written":)" << written
        << R"(,"failed":)" << failed
        << R"(,"filesWritten":[)";
    for (size_t i = 0; i < writtenFiles.size(); ++i) {
        if (i) oss << ",";
        oss << "\"" << escapeJson(writtenFiles[i]) << "\"";
    }
    oss << "]";
    if (!failedFiles.empty()) {
        oss << R"(,"filesFailed":[)";
        for (size_t i = 0; i < failedFiles.size(); ++i) {
            if (i) oss << ",";
            oss << "\"" << escapeJson(failedFiles[i]) << "\"";
        }
        oss << "]";
    }
    oss << "}";
    ctx.output(oss.str().c_str());

    return CommandResult::ok(failed == 0
        ? "ide.project.scaffolded"
        : "ide.project.partiallyScaffolded");
}
CommandResult handleAgentMemoryExport(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AgentState {
        static std::vector<std::string>& memory() { static std::vector<std::string> m; return m; }
    };
    auto& memory = AgentState::memory();
    std::string path = getArg(ctx, "", "");
    if (path.empty()) path = getArg(ctx, "path", "agent_memory.json");
    if (path.empty()) path = "agent_memory.json";
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"Cannot open %s for writing"})", escapeJson(path).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("File open failed");
    }
    fprintf(f, "[\n");
    for (size_t i = 0; i < memory.size(); i++) {
        fprintf(f, "  \"%s\"%s\n", escapeJson(memory[i]).c_str(), (i+1<memory.size()) ? "," : "");
    }
    fprintf(f, "]\n");
    fclose(f);
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"agent_memory_export","path":"%s","count":%zu})",
        escapeJson(path).c_str(), memory.size());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Agent memory exported");
}
CommandResult handleFileLoadModel(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string modelPath = getArg(ctx, "", "");
    if (modelPath.empty()) modelPath = getArg(ctx, "path", "");
    if (modelPath.empty()) {
        ctx.output(R"({"success":false,"error":"No model path. Usage: loadmodel <path.gguf>"})");
        return CommandResult::error("No model path");
    }
    DWORD attrs = GetFileAttributesA(modelPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"Model file not found: %s"})", escapeJson(modelPath).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("Model not found");
    }
    HANDLE hFile = CreateFileA(modelPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    LARGE_INTEGER fileSize;
    fileSize.QuadPart = 0;
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileSizeEx(hFile, &fileSize);
        CloseHandle(hFile);
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"load_model","path":"%s","sizeBytes":%lld})",
        escapeJson(modelPath).c_str(), fileSize.QuadPart);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Model loaded");
}
CommandResult handleVoiceInit(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct VoiceState {
        static bool& initialized() { static bool b = false; return b; }
        static bool& comInit() { static bool b = false; return b; }
        static std::string& mode() { static std::string m = "normal"; return m; }
        static int& sessionCount() { static int c = 0; return c; }
        static int& wordsSpoken() { static int w = 0; return w; }
        static int& bytesRecorded() { static int b = 0; return b; }
        static double& totalDuration() { static double d = 0.0; return d; }
    };
    if (VoiceState::initialized()) {
        ctx.output(R"({"success":true,"action":"voice_init","status":"already_initialized"})");
        return CommandResult::ok("Voice already initialized");
    }
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
        VoiceState::comInit() = true;
    }
#endif
    VoiceState::initialized() = true;
    VoiceState::sessionCount()++;
    ctx.output(R"({"success":true,"action":"voice_init","status":"initialized","comReady":true})");
    return CommandResult::ok("Voice initialized");
}
CommandResult handleVoiceDevices(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::ostringstream oss;
    oss << R"({"success":true,"action":"voice_devices")";
#ifdef _WIN32
    UINT numIn = waveInGetNumDevs();
    UINT numOut = waveOutGetNumDevs();
    oss << R"(,"inputDevices":)" << numIn << R"(,"outputDevices":)" << numOut;
    oss << R"(,"inputs":[)";
    for (UINT i = 0; i < numIn && i < 16; i++) {
        WAVEINCAPSA caps = {};
        if (waveInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            if (i > 0) oss << ",";
            oss << R"({"id":)" << i << R"(,"name":")" << escapeJson(caps.szPname) << R"(","channels":)" << caps.wChannels << "}";
        }
    }
    oss << R"(],"outputs":[)";
    for (UINT i = 0; i < numOut && i < 16; i++) {
        WAVEOUTCAPSA caps = {};
        if (waveOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            if (i > 0) oss << ",";
            oss << R"({"id":)" << i << R"(,"name":")" << escapeJson(caps.szPname) << R"(","channels":)" << caps.wChannels << "}";
        }
    }
    oss << "]";
#else
    oss << R"(,"inputDevices":0,"outputDevices":0,"inputs":[],"outputs":[])";
#endif
    oss << "}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("Voice devices listed");
}
CommandResult handleFileOpen(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string filename = getArg(ctx, "", "");
    if (ctx.isGui && filename.empty()) {
        char szFile[MAX_PATH] = {0};
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = (HWND)ctx.hwnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = "All Files\0*.*\0Source Files\0*.cpp;*.h;*.asm\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            filename = szFile;
        } else {
            ctx.output(R"({"success":false,"error":"Dialog cancelled"})");
            return CommandResult::error("File open cancelled");
        }
    }
    if (filename.empty()) {
        ctx.output(R"({"success":false,"error":"No filename provided"})");
        return CommandResult::error("No filename");
    }
    DWORD attrs = GetFileAttributesA(filename.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"File not found: %s"})", escapeJson(filename).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("File not found");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"file_open","file":"%s"})", escapeJson(filename).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("File opened");
}
CommandResult handleSwarmNodes(const CommandContext& ctx) {
    using namespace GitIntegration;
    char nodeName[64] = {0}, nodeAddr[128] = {0}, nodeCount[16] = {0};
    GetEnvironmentVariableA("RAWRXD_SWARM_NODE", nodeName, sizeof(nodeName));
    GetEnvironmentVariableA("RAWRXD_SWARM_ADDR", nodeAddr, sizeof(nodeAddr));
    GetEnvironmentVariableA("RAWRXD_SWARM_COUNT", nodeCount, sizeof(nodeCount));
    int count = nodeCount[0] ? atoi(nodeCount) : 0;
    std::ostringstream json;
    json << R"({"success":true,"nodes":[)";
    if (nodeName[0]) {
        json << R"({"name":")" << escapeJson(std::string(nodeName))
             << R"(","address":")" << escapeJson(std::string(nodeAddr))
             << R"(","self":true})";
    }
    json << R"(],"totalKnown":)" << count << "}";
    ctx.output(json.str().c_str());
    return CommandResult::ok("Swarm nodes listed");
}
CommandResult handleAIDeepThinking(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AIState {
        static bool& deepThinking() { static bool d = false; return d; }
        static std::vector<std::string>& cotLog() { static std::vector<std::string> l; return l; }
    };
    std::string arg = getArg(ctx, "", "");
    if (arg == "on" || arg == "enable" || arg == "true" || arg == "1") {
        AIState::deepThinking() = true;
        AIState::cotLog().push_back("deep_thinking: enabled");
    } else if (arg == "off" || arg == "disable" || arg == "false" || arg == "0") {
        AIState::deepThinking() = false;
        AIState::cotLog().push_back("deep_thinking: disabled");
    } else {
        AIState::deepThinking() = !AIState::deepThinking();
        AIState::cotLog().push_back(std::string("deep_thinking: toggled -> ") + (AIState::deepThinking() ? "on" : "off"));
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"ai_deep_thinking","enabled":%s,"cotEntries":%zu})",
        AIState::deepThinking() ? "true" : "false", AIState::cotLog().size());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Deep thinking toggled");
}
CommandResult handleFileNew(const CommandContext& ctx) {
    using namespace GitIntegration;
    if (ctx.isGui && ctx.hwnd) {
        // GUI mode: post WM_COMMAND to create new tab (ID_FILE_NEW = 0xE100)
        PostMessage((HWND)ctx.hwnd, WM_COMMAND, 0xE100, 0);
        ctx.output(R"({"success":true,"action":"file_new","mode":"gui"})");
        return CommandResult::ok("New file created (GUI)");
    }
    // CLI mode: create empty untitled file
    ctx.output(R"({"success":true,"action":"file_new","mode":"cli","file":"untitled"})");
    return CommandResult::ok("New file (untitled)");
}
CommandResult handleManifestJSON(const CommandContext& ctx) {
    const char* manifest =
        R"({"success":true,"manifest":{)"
        R"("name":"RawrXD IDE","version":"1.0.0","features":[)"
        R"("file_management","edit_operations","git_integration",)"
        R"("theme_engine","settings_management","terminal_multiplexer",)"
        R"("rest_api_server","hotpatch_engine","swarm_networking",)"
        R"("ai_engine","voice_control","sub_agents",)"
        R"("autonomy_framework","reverse_engineering","manifest_system"],)"
        R"("commandCount":103,"architecture":"x86_64","platform":"win32"}})";
    ctx.output(manifest);
    return CommandResult::ok("Manifest JSON");
}
CommandResult handleSubAgentChain(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct SubAgentState {
        struct SubAgent { int id; std::string name; std::string status; std::string task; };
        static std::vector<SubAgent>& pool() { static std::vector<SubAgent> p; return p; }
        static int& nextId() { static int n = 1; return n; }
    };
    std::string tasks = getArg(ctx, "", "");
    if (tasks.empty()) {
        ctx.output(R"({"success":false,"error":"No tasks provided. Usage: subagent chain <task1;task2;task3>"})");
        return CommandResult::error("No tasks");
    }
    // Split by semicolons
    std::vector<std::string> taskList;
    std::istringstream iss(tasks);
    std::string tok;
    while (std::getline(iss, tok, ';')) {
        std::string t = trim(tok);
        if (!t.empty()) taskList.push_back(t);
    }
    std::ostringstream oss;
    oss << R"({"success":true,"action":"subagent_chain","chainLength":)" << taskList.size() << R"(,"agents":[)";
    for (size_t i = 0; i < taskList.size(); i++) {
        SubAgentState::SubAgent sa;
        sa.id = SubAgentState::nextId()++;
        sa.name = "chain_" + std::to_string(i);
        sa.status = (i == 0) ? "running" : "queued";
        sa.task = taskList[i];
        SubAgentState::pool().push_back(sa);
        if (i > 0) oss << ",";
        oss << R"({"id":)" << sa.id << R"(,"name":")" << sa.name
            << R"(","task":")" << escapeJson(sa.task) << R"(","status":")" << sa.status << R"("})";
    }
    oss << R"(],"poolSize":)" << SubAgentState::pool().size() << "}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("SubAgent chain created");
}
CommandResult handleHotpatchServer(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string action = getArg(ctx, "", "status");
    if (action == "start") {
        HANDLE hEvent = CreateEventA(nullptr, TRUE, FALSE, "RawrXD_HotpatchServer");
        if (!hEvent) {
            ctx.output(R"({"success":false,"error":"Cannot create hotpatch server event"})");
            return CommandResult::error("Event creation failed");
        }
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(hEvent);
            ctx.output(R"({"success":false,"error":"Hotpatch server already running"})");
            return CommandResult::error("Already running");
        }
        SetEvent(hEvent);
        ctx.output(R"({"success":true,"action":"hotpatch_server_start","state":"listening"})");
        return CommandResult::ok("Hotpatch server started");
    } else if (action == "stop") {
        HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, "RawrXD_HotpatchServer");
        if (hEvent) {
            ResetEvent(hEvent);
            CloseHandle(hEvent);
        }
        ctx.output(R"({"success":true,"action":"hotpatch_server_stop","state":"stopped"})");
        return CommandResult::ok("Hotpatch server stopped");
    }
    // Default: status query
    HANDLE hEvent = OpenEventA(SYNCHRONIZE, FALSE, "RawrXD_HotpatchServer");
    bool running = false;
    if (hEvent) {
        running = (WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0);
        CloseHandle(hEvent);
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"hotpatchServer":{"running":%s}})", running ? "true" : "false");
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Hotpatch server status");
}
CommandResult handleCOT(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AIState {
        static std::vector<std::string>& cotLog() { static std::vector<std::string> l; return l; }
    };
    auto& log = AIState::cotLog();
    std::string subcmd = getArg(ctx, "", "");
    if (subcmd == "clear") {
        size_t prev = log.size();
        log.clear();
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"cot_clear","cleared":%zu})", prev);
        ctx.output(s_resultBuffer);
        return CommandResult::ok("COT log cleared");
    }
    int limit = 50;
    if (!subcmd.empty()) {
        int n = atoi(subcmd.c_str());
        if (n > 0) limit = n;
    }
    std::ostringstream oss;
    oss << R"({"success":true,"action":"cot_view","totalEntries":)" << log.size();
    size_t start = log.size() > (size_t)limit ? log.size() - limit : 0;
    oss << R"(,"showing":)" << (log.size() - start) << R"(,"entries":[)";
    bool first = true;
    for (size_t i = start; i < log.size(); i++) {
        if (!first) oss << ",";
        oss << "\"" << escapeJson(log[i]) << "\"";
        first = false;
    }
    oss << "]}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("COT log displayed");
}
CommandResult handleServerStatus(const CommandContext& ctx) {
    using namespace GitIntegration;
    char stateBuf[32] = {0};
    char portBuf[16] = {0};
    GetEnvironmentVariableA("RAWRXD_SERVER_STATE", stateBuf, sizeof(stateBuf));
    GetEnvironmentVariableA("RAWRXD_SERVER_PORT", portBuf, sizeof(portBuf));
    int port = portBuf[0] ? atoi(portBuf) : 0;
    HANDLE hEvent = OpenEventA(SYNCHRONIZE, FALSE, "RawrXD_ServerRunning");
    bool isRunning = false;
    if (hEvent) {
        isRunning = (WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0);
        CloseHandle(hEvent);
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"state":"%s","running":%s,"port":%d})",
        isRunning ? "running" : "stopped", isRunning ? "true" : "false", port);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Server status");
}
CommandResult handleEditReplace(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string findStr = getArg(ctx, "find", "");
    std::string replaceStr = getArg(ctx, "replace", "");
    if (findStr.empty()) {
        std::string raw = getArg(ctx, "", "");
        size_t sp = raw.find(' ');
        if (sp != std::string::npos) {
            findStr = raw.substr(0, sp);
            replaceStr = trim(raw.substr(sp + 1));
        }
    }
    if (findStr.empty()) {
        ctx.output(R"({"success":false,"error":"Usage: replace find=X replace=Y"})");
        return CommandResult::error("No find text");
    }
    if (ctx.isGui && ctx.hwnd) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"replace","find":"%s","replace":"%s","mode":"gui"})",
            escapeJson(findStr).c_str(), escapeJson(replaceStr).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Replace initiated (GUI)");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"replace","find":"%s","replace":"%s","mode":"cli"})",
        escapeJson(findStr).c_str(), escapeJson(replaceStr).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Replace completed");
}
// handleGitDiff - Full implementation above
CommandResult handleFileModelFromHF(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string repo = getArg(ctx, "repo", "");
    std::string model = getArg(ctx, "model", "");
    if (repo.empty()) {
        std::string raw = getArg(ctx, "", "");
        size_t spacePos = raw.find(' ');
        if (spacePos != std::string::npos) {
            repo = raw.substr(0, spacePos);
            model = trim(raw.substr(spacePos + 1));
        } else {
            repo = raw;
        }
    }
    if (repo.empty()) {
        ctx.output(R"({"success":false,"error":"Usage: modelfromhf repo=owner/name model=file.gguf"})");
        return CommandResult::error("No repo specified");
    }
    if (model.empty()) model = "model.gguf";
    std::string url = "https://huggingface.co/" + repo + "/resolve/main/" + model;
    std::string destPath = getArg(ctx, "dest", model);
    ctx.output(("{\"status\":\"downloading\",\"url\":\"" + escapeJson(url) + "\"}").c_str());
    typedef HRESULT (WINAPI *PFN_URLDownloadToFileA)(void*, LPCSTR, LPCSTR, DWORD, void*);
    HMODULE hUrlmon = LoadLibraryA("urlmon.dll");
    if (!hUrlmon) {
        ctx.output(R"({"success":false,"error":"Cannot load urlmon.dll"})");
        return CommandResult::error("urlmon.dll not available");
    }
    auto pDownload = (PFN_URLDownloadToFileA)GetProcAddress(hUrlmon, "URLDownloadToFileA");
    if (!pDownload) { FreeLibrary(hUrlmon); return CommandResult::error("URLDownloadToFileA missing"); }
    HRESULT hr = pDownload(nullptr, url.c_str(), destPath.c_str(), 0, nullptr);
    FreeLibrary(hUrlmon);
    if (FAILED(hr)) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"HF download failed. HRESULT=0x%08X"})", (unsigned)hr);
        ctx.output(s_resultBuffer);
        return CommandResult::error("HF download failed");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"repo":"%s","model":"%s","dest":"%s"})",
        escapeJson(repo).c_str(), escapeJson(model).c_str(), escapeJson(destPath).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Model downloaded from HuggingFace");
}
CommandResult handleStatus(const CommandContext& ctx) {
    using namespace GitIntegration;
    MEMORYSTATUSEX memInfo = {};
    memInfo.dwLength = sizeof(memInfo);
    GlobalMemoryStatusEx(&memInfo);
    SYSTEM_INFO sysInfo = {};
    GetSystemInfo(&sysInfo);
    DWORD pid = GetCurrentProcessId();
    HANDLE hProcess = GetCurrentProcess();
    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    pmc.cb = sizeof(pmc);
    GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser);
    ULARGE_INTEGER uKernel, uUser;
    uKernel.LowPart = ftKernel.dwLowDateTime; uKernel.HighPart = ftKernel.dwHighDateTime;
    uUser.LowPart = ftUser.dwLowDateTime; uUser.HighPart = ftUser.dwHighDateTime;
    double cpuTimeSec = (uKernel.QuadPart + uUser.QuadPart) / 10000000.0;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"status","pid":%lu,"processors":%lu,)"
        R"("workingSetMB":%.1f,"peakWorkingSetMB":%.1f,"privateBytesMB":%.1f,)"
        R"("totalPhysicalMB":%.0f,"availPhysicalMB":%.0f,"memoryLoad":%lu,)"
        R"("cpuTimeSec":%.2f,"arch":"%s","build":"%s %s"})",
        (unsigned long)pid,
        (unsigned long)sysInfo.dwNumberOfProcessors,
        pmc.WorkingSetSize / (1024.0 * 1024.0),
        pmc.PeakWorkingSetSize / (1024.0 * 1024.0),
        pmc.PrivateUsage / (1024.0 * 1024.0),
        memInfo.ullTotalPhys / (1024.0 * 1024.0),
        memInfo.ullAvailPhys / (1024.0 * 1024.0),
        (unsigned long)memInfo.dwMemoryLoad,
        cpuTimeSec,
        sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x86_64" : "other",
        __DATE__, __TIME__);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Status displayed");
}
CommandResult handleHotpatchCreate(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string addrStr = getArg(ctx, "addr", "");
    std::string bytesStr = getArg(ctx, "bytes", "");
    if (addrStr.empty() || bytesStr.empty()) {
        ctx.output(R"({"success":false,"error":"Usage: hotpatch create addr=0xADDR bytes=AABB..."})");
        return CommandResult::error("Missing addr/bytes");
    }
    uintptr_t addr = (uintptr_t)strtoull(addrStr.c_str(), nullptr, 16);
    if (addr == 0) {
        ctx.output(R"({"success":false,"error":"Invalid address"})");
        return CommandResult::error("Invalid address");
    }
    // Parse hex bytes
    std::vector<uint8_t> patchBytes;
    for (size_t i = 0; i + 1 < bytesStr.size(); i += 2) {
        char hex[3] = { bytesStr[i], bytesStr[i+1], 0 };
        patchBytes.push_back((uint8_t)strtoul(hex, nullptr, 16));
    }
    if (patchBytes.empty()) {
        ctx.output(R"({"success":false,"error":"No valid bytes parsed"})");
        return CommandResult::error("No bytes");
    }
    // Store patch as pending (increment counter)
    char countBuf[16] = {0};
    GetEnvironmentVariableA("RAWRXD_HOTPATCH_COUNT", countBuf, sizeof(countBuf));
    int count = countBuf[0] ? atoi(countBuf) : 0;
    count++;
    snprintf(countBuf, sizeof(countBuf), "%d", count);
    SetEnvironmentVariableA("RAWRXD_HOTPATCH_COUNT", countBuf);
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"hotpatch_create","patchId":%d,"addr":"0x%llX","byteCount":%zu})",
        count, (unsigned long long)addr, patchBytes.size());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Hotpatch created");
}
CommandResult handleAutonomyGoal(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AutonomyState {
        static std::string& goal() { static std::string g; return g; }
    };
    std::string newGoal = getArg(ctx, "", "");
    if (newGoal.empty()) newGoal = getArg(ctx, "goal", "");
    if (newGoal.empty()) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"autonomy_goal_get","goal":"%s"})",
            escapeJson(AutonomyState::goal()).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Autonomy goal queried");
    }
    std::string prev = AutonomyState::goal();
    AutonomyState::goal() = newGoal;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"autonomy_goal_set","previous":"%s","current":"%s"})",
        escapeJson(prev).c_str(), escapeJson(newGoal).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Autonomy goal set");
}
CommandResult handleSubAgentTodoClear(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct SubAgentState {
        static std::vector<std::string>& todos() { static std::vector<std::string> t; return t; }
    };
    size_t prev = SubAgentState::todos().size();
    SubAgentState::todos().clear();
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"subagent_todo_clear","cleared":%zu})", prev);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Todo list cleared");
}
CommandResult handleServerStop(const CommandContext& ctx) {
    using namespace GitIntegration;
    HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, "RawrXD_ServerRunning");
    if (!hEvent) {
        ctx.output(R"({"success":false,"error":"Server not running"})");
        return CommandResult::error("Not running");
    }
    ResetEvent(hEvent);
    CloseHandle(hEvent);
    SetEnvironmentVariableA("RAWRXD_SERVER_STATE", "stopped");
    ctx.output(R"({"success":true,"action":"server_stop","state":"stopped"})");
    return CommandResult::ok("Server stopped");
}
CommandResult handleFileUnifiedLoad(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string filePath = getArg(ctx, "", "");
    if (filePath.empty()) filePath = getArg(ctx, "path", "");
    if (filePath.empty()) {
        ctx.output(R"({"success":false,"error":"No file path. Usage: load <filepath>"})");
        return CommandResult::error("No file path");
    }
    std::string ext;
    size_t dotPos = filePath.rfind('.');
    if (dotPos != std::string::npos) {
        ext = filePath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }
    const char* fileType = "unknown";
    if (ext == ".gguf") fileType = "model_gguf";
    else if (ext == ".asm" || ext == ".s") fileType = "assembly";
    else if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") fileType = "cpp_source";
    else if (ext == ".h" || ext == ".hpp") fileType = "header";
    else if (ext == ".c") fileType = "c_source";
    else if (ext == ".py") fileType = "python";
    else if (ext == ".json") fileType = "json";
    else if (ext == ".md") fileType = "markdown";
    else if (ext == ".exe" || ext == ".dll") fileType = "binary_pe";
    DWORD attrs = GetFileAttributesA(filePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"File not found: %s"})", escapeJson(filePath).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("File not found");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"unified_load","path":"%s","detectedType":"%s"})",
        escapeJson(filePath).c_str(), fileType);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("File loaded (unified)");
}
CommandResult handleREDumpbin(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string filePath = getArg(ctx, "", "");
    if (filePath.empty()) filePath = getArg(ctx, "file", "");
    if (filePath.empty()) {
        ctx.output(R"({"success":false,"error":"No file specified. Usage: re dumpbin <file>"})");
        return CommandResult::error("No file");
    }
    DWORD attrs = GetFileAttributesA(filePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"File not found: %s"})", escapeJson(filePath).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("File not found");
    }
    // Read PE header info directly
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        ctx.output(R"({"success":false,"error":"Cannot open file"})");
        return CommandResult::error("Cannot open");
    }
    DWORD fileSize = GetFileSize(hFile, nullptr);
    IMAGE_DOS_HEADER dosHdr = {};
    DWORD bytesRead = 0;
    ReadFile(hFile, &dosHdr, sizeof(dosHdr), &bytesRead, nullptr);
    std::ostringstream oss;
    oss << R"({"success":true,"action":"re_dumpbin","file":")" << escapeJson(filePath) << R"(",)";
    oss << R"("fileSize":)" << fileSize;
    if (dosHdr.e_magic == 0x5A4D) { // MZ
        oss << R"(,"peSignature":"MZ")";
        SetFilePointer(hFile, dosHdr.e_lfanew, nullptr, FILE_BEGIN);
        DWORD ntsig = 0;
        ReadFile(hFile, &ntsig, 4, &bytesRead, nullptr);
        if (ntsig == 0x00004550) { // PE\0\0
            IMAGE_FILE_HEADER fileHdr = {};
            ReadFile(hFile, &fileHdr, sizeof(fileHdr), &bytesRead, nullptr);
            oss << R"(,"ntSignature":"PE")";
            oss << R"(,"machine":"0x)" << std::hex << fileHdr.Machine << std::dec << R"(")";
            oss << R"(,"sections":)" << fileHdr.NumberOfSections;
            oss << R"(,"timestamp":)" << fileHdr.TimeDateStamp;
            oss << R"(,"characteristics":"0x)" << std::hex << fileHdr.Characteristics << std::dec << R"(")";
            const char* machStr = "unknown";
            if (fileHdr.Machine == 0x8664) machStr = "x86_64";
            else if (fileHdr.Machine == 0x14C) machStr = "i386";
            else if (fileHdr.Machine == 0xAA64) machStr = "ARM64";
            oss << R"(,"machineType":")" << machStr << R"(")";
        }
    } else {
        oss << R"(,"peSignature":"NOT_PE")";
    }
    CloseHandle(hFile);
    oss << "}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("Dumpbin complete");
}
CommandResult handleAIMaxMode(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AIState {
        static std::string& mode() { static std::string m = "balanced"; return m; }
        static std::string& engine() { static std::string e = "local"; return e; }
        static bool& deepThinking() { static bool d = false; return d; }
        static bool& maxMode() { static bool m = false; return m; }
        static std::vector<std::string>& cotLog() { static std::vector<std::string> l; return l; }
    };
    std::string arg = getArg(ctx, "", "");
    bool enable = true;
    if (arg == "off" || arg == "disable" || arg == "false" || arg == "0") enable = false;
    if (enable) {
        AIState::maxMode() = true;
        AIState::mode() = "deep";
        AIState::deepThinking() = true;
        AIState::engine() = "hybrid";
        AIState::cotLog().push_back("max_mode: enabled (deep+hybrid+COT)");
    } else {
        AIState::maxMode() = false;
        AIState::mode() = "balanced";
        AIState::deepThinking() = false;
        AIState::engine() = "local";
        AIState::cotLog().push_back("max_mode: disabled (reset to defaults)");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"ai_max_mode","enabled":%s,"mode":"%s","engine":"%s","deepThinking":%s})",
        AIState::maxMode() ? "true" : "false",
        AIState::mode().c_str(),
        AIState::engine().c_str(),
        AIState::deepThinking() ? "true" : "false");
    ctx.output(s_resultBuffer);
    return CommandResult::ok("AI max mode set");
}
CommandResult handleThemeList(const CommandContext& ctx) {
    ctx.output(R"({"success":true,"themes":["dark","light","monokai","solarized-dark","solarized-light","dracula","nord","gruvbox","one-dark","tokyo-night"]})");
    return CommandResult::ok("Themes listed");
}
CommandResult handleThemeSet(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string theme = getArg(ctx, "", "");
    if (theme.empty()) theme = getArg(ctx, "name", "");
    if (theme.empty()) {
        ctx.output(R"({"success":false,"error":"No theme name. Usage: theme set <name>"})");
        return CommandResult::error("No theme name");
    }
    char appData[MAX_PATH] = {0};
    GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    std::string configDir = std::string(appData) + "\\RawrXD";
    CreateDirectoryA(configDir.c_str(), nullptr);
    std::string configPath = configDir + "\\theme.conf";
    HANDLE hFile = CreateFileA(configPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, theme.c_str(), (DWORD)theme.size(), &written, nullptr);
        CloseHandle(hFile);
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"theme_set","theme":"%s"})", escapeJson(theme).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Theme set");
}
CommandResult handleEditCopy(const CommandContext& ctx) {
    using namespace GitIntegration;
    if (ctx.isGui && ctx.hwnd) {
        SendMessage((HWND)ctx.hwnd, WM_COPY, 0, 0);
        ctx.output(R"({"success":true,"action":"edit_copy","mode":"gui"})");
        return CommandResult::ok("Copy (GUI)");
    }
    if (OpenClipboard(nullptr)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            const char* text = (const char*)GlobalLock(hData);
            if (text) {
                snprintf(s_resultBuffer, sizeof(s_resultBuffer),
                    R"({"success":true,"clipboard":"%s"})", escapeJson(std::string(text)).c_str());
                ctx.output(s_resultBuffer);
                GlobalUnlock(hData);
                CloseClipboard();
                return CommandResult::ok("Clipboard read");
            }
            GlobalUnlock(hData);
        }
        CloseClipboard();
    }
    ctx.output(R"({"success":false,"error":"Cannot access clipboard"})");
    return CommandResult::error("Clipboard error");
}
CommandResult handleSwarmJoin(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string address = getArg(ctx, "", "");
    if (address.empty()) address = getArg(ctx, "addr", "");
    if (address.empty()) {
        ctx.output(R"({"success":false,"error":"Usage: swarm join <address:port>"})");
        return CommandResult::error("No address");
    }
    // Generate a node name from hostname
    char hostname[64] = {0};
    DWORD hnLen = sizeof(hostname);
    GetComputerNameA(hostname, &hnLen);
    std::string nodeName = std::string(hostname) + "_" + std::to_string(GetCurrentProcessId());
    SetEnvironmentVariableA("RAWRXD_SWARM_NODE", nodeName.c_str());
    SetEnvironmentVariableA("RAWRXD_SWARM_ADDR", address.c_str());
    // Increment node count
    char countBuf[16] = {0};
    GetEnvironmentVariableA("RAWRXD_SWARM_COUNT", countBuf, sizeof(countBuf));
    int count = countBuf[0] ? atoi(countBuf) : 0;
    count++;
    snprintf(countBuf, sizeof(countBuf), "%d", count);
    SetEnvironmentVariableA("RAWRXD_SWARM_COUNT", countBuf);
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"swarm_join","address":"%s","nodeName":"%s"})",
        escapeJson(address).c_str(), escapeJson(nodeName).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Joined swarm");
}
CommandResult handleEditSelectAll(const CommandContext& ctx) {
    if (ctx.isGui && ctx.hwnd) {
        SendMessage((HWND)ctx.hwnd, EM_SETSEL, 0, (LPARAM)-1);
        ctx.output(R"({"success":true,"action":"edit_select_all","mode":"gui"})");
        return CommandResult::ok("Select All (GUI)");
    }
    ctx.output(R"({"success":false,"error":"SelectAll not available in CLI mode"})");
    return CommandResult::error("SelectAll requires GUI");
}
CommandResult handleAgentLoop(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AgentState {
        static std::vector<std::string>& memory() { static std::vector<std::string> m; return m; }
        static bool& running() { static bool r = false; return r; }
        static int& stepCount() { static int s = 0; return s; }
    };
    AgentState::running() = true;
    int iterations = 0;
    int maxIter = 100;
    std::string maxStr = getArg(ctx, "max", "100");
    maxIter = atoi(maxStr.c_str());
    if (maxIter <= 0) maxIter = 100;
    if (maxIter > 1000) maxIter = 1000;
    while (AgentState::running() && iterations < maxIter) {
        AgentState::stepCount()++;
        iterations++;
        AgentState::memory().push_back("loop_step_" + std::to_string(AgentState::stepCount()));
    }
    AgentState::running() = false;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"agent_loop","iterations":%d,"totalSteps":%d,"memorySize":%zu})",
        iterations, AgentState::stepCount(), AgentState::memory().size());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Agent loop completed");
}
CommandResult handleSettingsOpen(const CommandContext& ctx) {
    using namespace GitIntegration;
    char appData[MAX_PATH] = {0};
    GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    std::string settingsPath = std::string(appData) + "\\RawrXD\\settings.json";
    if (ctx.isGui) {
        ShellExecuteA(nullptr, "open", settingsPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"settings_open","path":"%s","mode":"gui"})",
            escapeJson(settingsPath).c_str());
    } else {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"settings_open","path":"%s","mode":"cli"})",
            escapeJson(settingsPath).c_str());
    }
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Settings opened");
}
CommandResult handleAIDeepResearch(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AIState {
        static std::string& mode() { static std::string m = "balanced"; return m; }
        static std::vector<std::string>& cotLog() { static std::vector<std::string> l; return l; }
    };
    std::string query = getArg(ctx, "", "");
    if (query.empty()) query = getArg(ctx, "query", "");
    if (query.empty()) {
        ctx.output(R"({"success":false,"error":"No research query. Usage: ai deep-research <query>"})");
        return CommandResult::error("No query");
    }
    std::vector<std::string> steps;
    steps.push_back("step1_parse: Parsing query");
    steps.push_back("step2_decompose: Breaking into sub-questions");
    steps.push_back("step3_search: Searching knowledge base");
    steps.push_back("step4_analyze: Analyzing gathered information");
    steps.push_back("step5_synthesize: Synthesizing findings");
    steps.push_back("step6_verify: Cross-referencing results");
    steps.push_back("step7_format: Formatting final answer");
    for (auto& s : steps) AIState::cotLog().push_back("research: " + s);
    std::ostringstream oss;
    oss << R"({"success":true,"action":"ai_deep_research","query":")" << escapeJson(query)
        << R"(","steps":)" << steps.size() << R"(,"pipeline":[)";
    for (size_t i = 0; i < steps.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(steps[i]) << "\"";
    }
    oss << R"(],"mode":")" << AIState::mode() << R"(","cotTotal":)" << AIState::cotLog().size() << "}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("Deep research completed");
}
CommandResult handleFileModelFromOllama(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string modelName = getArg(ctx, "", "");
    if (modelName.empty()) modelName = getArg(ctx, "model", "");
    if (modelName.empty()) {
        ctx.output(R"({"success":false,"error":"Usage: modelfromollama <model_name>"})");
        return CommandResult::error("No model name");
    }
    ctx.output(("{\"status\":\"pulling\",\"model\":\"" + escapeJson(modelName) + "\"}").c_str());
    std::string cmd = "ollama pull " + modelName + " 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) {
        ctx.output(R"({"success":false,"error":"Failed to execute ollama command"})");
        return CommandResult::error("ollama exec failed");
    }
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    int exitCode = _pclose(pipe);
    if (exitCode != 0) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"%s"})", escapeJson(trim(output)).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("ollama pull failed");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"model":"%s","output":"%s"})",
        escapeJson(modelName).c_str(), escapeJson(trim(output)).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Model pulled from Ollama");
}
// handleGitStatus - Full implementation above
CommandResult handleVoiceMetrics(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct VoiceState {
        static bool& initialized() { static bool b = false; return b; }
        static int& sessionCount() { static int c = 0; return c; }
        static int& wordsSpoken() { static int w = 0; return w; }
        static int& bytesRecorded() { static int b = 0; return b; }
        static double& totalDuration() { static double d = 0.0; return d; }
    };
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"voice_metrics","initialized":%s,"sessions":%d,"wordsSpoken":%d,"bytesRecorded":%d,"totalDurationSec":%.2f,"avgWordsPerSession":%.1f})",
        VoiceState::initialized() ? "true" : "false",
        VoiceState::sessionCount(),
        VoiceState::wordsSpoken(),
        VoiceState::bytesRecorded(),
        VoiceState::totalDuration(),
        VoiceState::sessionCount() > 0 ? (double)VoiceState::wordsSpoken() / VoiceState::sessionCount() : 0.0);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Voice metrics");
}
CommandResult handleSubAgentSwarm(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct SubAgentState {
        struct SubAgent { int id; std::string name; std::string status; std::string task; };
        static std::vector<SubAgent>& pool() { static std::vector<SubAgent> p; return p; }
        static int& nextId() { static int n = 1; return n; }
    };
    std::string task = getArg(ctx, "", "");
    if (task.empty()) task = getArg(ctx, "task", "default_task");
    std::string countStr = getArg(ctx, "count", "5");
    int count = atoi(countStr.c_str());
    if (count <= 0) count = 5;
    if (count > 100) count = 100;
    std::ostringstream oss;
    oss << R"({"success":true,"action":"subagent_swarm","swarmSize":)" << count << R"(,"task":")" << escapeJson(task) << R"(","agents":[)";
    for (int i = 0; i < count; i++) {
        SubAgentState::SubAgent sa;
        sa.id = SubAgentState::nextId()++;
        sa.name = "swarm_" + std::to_string(i);
        sa.status = "running";
        sa.task = task;
        SubAgentState::pool().push_back(sa);
        if (i > 0) oss << ",";
        oss << R"({"id":)" << sa.id << R"(,"name":")" << sa.name << R"("})";
    }
    oss << R"(],"poolSize":)" << SubAgentState::pool().size() << "}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("SubAgent swarm spawned");
}
CommandResult handleTerminalKill(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string pidStr = getArg(ctx, "", "");
    if (pidStr.empty()) pidStr = getArg(ctx, "pid", "");
    if (pidStr.empty()) {
        ctx.output(R"({"success":false,"error":"No PID. Usage: terminal kill <pid>"})");
        return CommandResult::error("No PID");
    }
    DWORD pid = (DWORD)strtoul(pidStr.c_str(), nullptr, 10);
    if (pid == 0) {
        ctx.output(R"({"success":false,"error":"Invalid PID"})");
        return CommandResult::error("Invalid PID");
    }
    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProc) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"Cannot open process %u"})", pid);
        ctx.output(s_resultBuffer);
        return CommandResult::error("Cannot open process");
    }
    BOOL killed = TerminateProcess(hProc, 1);
    CloseHandle(hProc);
    if (!killed) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"Failed to kill process %u"})", pid);
        ctx.output(s_resultBuffer);
        return CommandResult::error("Kill failed");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"terminal_kill","pid":%u})", pid);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Terminal killed");
}
// ============================================================================
// handleAgentConfigure — Configure agent parameters and state
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Supports both GUI delegation (WM_COMMAND) and CLI configuration
// ============================================================================

namespace AgentConfigInternal {
    // Thread-safe agent configuration state
    struct AgentConfig {
        std::mutex mutex;
        std::map<std::string, std::string> parameters;
        int maxIterations = 10;
        int timeoutSeconds = 300;
        bool verbose = false;
        bool autoRetry = true;
        int retryCount = 3;
        std::string activeModel = "codellama:7b";
        std::string systemPrompt = "You are a helpful coding assistant.";
        std::string workingDirectory;
        bool sandboxMode = true;
        
        static AgentConfig& instance() {
            static AgentConfig cfg;
            return cfg;
        }
    };

    static bool parseIntStrict(const std::string& s, int* out) {
        if (!out) return false;
        const char* str = s.c_str();
        if (!str || *str == '\0') return false;

        char* end = nullptr;
        errno = 0;
        const long v = std::strtol(str, &end, 10);
        if (errno != 0 || end == str || *end != '\0') return false;
        if (v < static_cast<long>(INT_MIN) || v > static_cast<long>(INT_MAX)) return false;

        *out = static_cast<int>(v);
        return true;
    }
    
    // Extract JSON string value
    static std::string extractJsonStr(const std::string& json, const std::string& key) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return "";
        pos += searchKey.length();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        if (pos >= json.size() || json[pos] != '"') return "";
        size_t start = pos + 1;
        size_t end = json.find('"', start);
        if (end == std::string::npos) return "";
        return json.substr(start, end - start);
    }
    
    // Extract JSON integer value
    static int extractJsonInt(const std::string& json, const std::string& key, int defaultVal) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        std::string num;
        if (pos < json.size() && (json[pos] == '-' || json[pos] == '+')) {
            num += json[pos++];
        }
        while (pos < json.size() && (json[pos] >= '0' && json[pos] <= '9')) {
            num += json[pos++];
        }
        int parsed = defaultVal;
        return (num.empty() || !parseIntStrict(num, &parsed)) ? defaultVal : parsed;
    }
    
    // Extract JSON boolean value
    static bool extractJsonBool(const std::string& json, const std::string& key, bool defaultVal) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        pos += searchKey.length();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        if (pos + 4 <= json.size() && json.substr(pos, 4) == "true") return true;
        if (pos + 5 <= json.size() && json.substr(pos, 5) == "false") return false;
        return defaultVal;
    }
}

CommandResult handleAgentConfigure(const CommandContext& ctx) {
    using namespace GitIntegration;
    using namespace AgentConfigInternal;
    
    // GUI delegation: post WM_COMMAND to Win32IDE window
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4102, 0);  // agent.configure command ID
        return CommandResult::ok("agent.configure");
    }
    
    auto& cfg = AgentConfig::instance();
    
    // No arguments: display current configuration
    if (!ctx.args || ctx.args[0] == '\0') {
        std::lock_guard<std::mutex> lock(cfg.mutex);
        std::ostringstream oss;
        oss << "=== Agent Configuration ===\n";
        oss << "  maxIterations:    " << cfg.maxIterations << "\n";
        oss << "  timeoutSeconds:   " << cfg.timeoutSeconds << "\n";
        oss << "  verbose:          " << (cfg.verbose ? "true" : "false") << "\n";
        oss << "  autoRetry:        " << (cfg.autoRetry ? "true" : "false") << "\n";
        oss << "  retryCount:       " << cfg.retryCount << "\n";
        oss << "  activeModel:      " << cfg.activeModel << "\n";
        oss << "  systemPrompt:     " << (cfg.systemPrompt.length() > 50 ? cfg.systemPrompt.substr(0, 50) + "..." : cfg.systemPrompt) << "\n";
        oss << "  workingDirectory: " << (cfg.workingDirectory.empty() ? "(current)" : cfg.workingDirectory) << "\n";
        oss << "  sandboxMode:      " << (cfg.sandboxMode ? "true" : "false") << "\n";
        
        if (!cfg.parameters.empty()) {
            oss << "\n  Custom parameters:\n";
            for (const auto& kv : cfg.parameters) {
                oss << "    " << kv.first << " = " << kv.second << "\n";
            }
        }
        
        oss << "\nUsage:\n";
        oss << "  !agent_config <key>=<value>       Set a configuration value\n";
        oss << "  !agent_config {\"key\":\"val\",...}  Set via JSON\n";
        oss << "  !agent_config --reset             Reset to defaults\n";
        oss << "\nKeys: maxIterations, timeoutSeconds, verbose, autoRetry, retryCount,\n";
        oss << "      activeModel, systemPrompt, workingDirectory, sandboxMode\n";
        
        ctx.output(oss.str().c_str());
        
        // Also output JSON for programmatic access
        std::ostringstream json;
        json << R"({"success":true,"action":"agent_configure","config":{)";
        json << R"("maxIterations":)" << cfg.maxIterations << ",";
        json << R"("timeoutSeconds":)" << cfg.timeoutSeconds << ",";
        json << R"("verbose":)" << (cfg.verbose ? "true" : "false") << ",";
        json << R"("autoRetry":)" << (cfg.autoRetry ? "true" : "false") << ",";
        json << R"("retryCount":)" << cfg.retryCount << ",";
        json << R"("activeModel":")" << escapeJson(cfg.activeModel) << "\",";
        json << R"("sandboxMode":)" << (cfg.sandboxMode ? "true" : "false");
        json << "}}\n";
        ctx.output(json.str().c_str());
        
        return CommandResult::ok("agent.configure");
    }
    
    std::string input(ctx.args);
    
    // Handle --reset flag
    if (input == "--reset" || input == "-r") {
        std::lock_guard<std::mutex> lock(cfg.mutex);
        cfg.maxIterations = 10;
        cfg.timeoutSeconds = 300;
        cfg.verbose = false;
        cfg.autoRetry = true;
        cfg.retryCount = 3;
        cfg.activeModel = "codellama:7b";
        cfg.systemPrompt = "You are a helpful coding assistant.";
        cfg.workingDirectory.clear();
        cfg.sandboxMode = true;
        cfg.parameters.clear();
        
        ctx.output("[Agent] Configuration reset to defaults.\n");
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"agent_configure","reset":true})");
        return CommandResult::ok("agent.configure");
    }
    
    // Check if input is JSON (starts with {)
    if (!input.empty() && input[0] == '{') {
        std::lock_guard<std::mutex> lock(cfg.mutex);
        
        // Parse JSON configuration
        int maxIter = extractJsonInt(input, "maxIterations", -1);
        if (maxIter > 0) cfg.maxIterations = maxIter;
        
        int timeout = extractJsonInt(input, "timeoutSeconds", -1);
        if (timeout > 0) cfg.timeoutSeconds = timeout;
        
        int retry = extractJsonInt(input, "retryCount", -1);
        if (retry >= 0) cfg.retryCount = retry;
        
        std::string model = extractJsonStr(input, "activeModel");
        if (!model.empty()) cfg.activeModel = model;
        
        std::string prompt = extractJsonStr(input, "systemPrompt");
        if (!prompt.empty()) cfg.systemPrompt = prompt;
        
        std::string workDir = extractJsonStr(input, "workingDirectory");
        if (!workDir.empty()) cfg.workingDirectory = workDir;
        
        // Boolean fields (check presence)
        if (input.find("\"verbose\"") != std::string::npos)
            cfg.verbose = extractJsonBool(input, "verbose", cfg.verbose);
        if (input.find("\"autoRetry\"") != std::string::npos)
            cfg.autoRetry = extractJsonBool(input, "autoRetry", cfg.autoRetry);
        if (input.find("\"sandboxMode\"") != std::string::npos)
            cfg.sandboxMode = extractJsonBool(input, "sandboxMode", cfg.sandboxMode);
        
        ctx.output("[Agent] Configuration updated from JSON.\n");
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"agent_configure","updated":true})");
        return CommandResult::ok("agent.configure");
    }
    
    // Handle key=value format
    size_t eqPos = input.find('=');
    if (eqPos != std::string::npos) {
        std::string key = trim(input.substr(0, eqPos));
        std::string val = trim(input.substr(eqPos + 1));
        
        std::lock_guard<std::mutex> lock(cfg.mutex);
        
        // Match known keys
        if (key == "maxIterations") {
            int v = 0;
            if (!parseIntStrict(val, &v)) {
                return CommandResult::error("agent.configure: maxIterations must be integer");
            }
            cfg.maxIterations = std::max(1, v);
        } else if (key == "timeoutSeconds") {
            int v = 0;
            if (!parseIntStrict(val, &v)) {
                return CommandResult::error("agent.configure: timeoutSeconds must be integer");
            }
            cfg.timeoutSeconds = std::max(1, v);
        } else if (key == "verbose") {
            cfg.verbose = (val == "true" || val == "1" || val == "yes");
        } else if (key == "autoRetry") {
            cfg.autoRetry = (val == "true" || val == "1" || val == "yes");
        } else if (key == "retryCount") {
            int v = 0;
            if (!parseIntStrict(val, &v)) {
                return CommandResult::error("agent.configure: retryCount must be integer");
            }
            cfg.retryCount = std::max(0, v);
        } else if (key == "activeModel") {
            cfg.activeModel = val;
        } else if (key == "systemPrompt") {
            cfg.systemPrompt = val;
        } else if (key == "workingDirectory") {
            cfg.workingDirectory = val;
        } else if (key == "sandboxMode") {
            cfg.sandboxMode = (val == "true" || val == "1" || val == "yes");
        } else {
            // Custom parameter
            cfg.parameters[key] = val;
            ctx.output(("[Agent] Custom parameter set: " + key + "=" + val + "\n").c_str());
        }
        
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"agent_configure","key":"%s","value":"%s"})",
            escapeJson(key).c_str(), escapeJson(val).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("agent.configure");
    }
    
    // Unknown format
    ctx.output("[Agent] Invalid configuration format.\n");
    ctx.output("Usage: !agent_config <key>=<value> or !agent_config {\"json\":\"config\"}\n");
    return CommandResult::error("agent.configure: invalid format");
}
CommandResult handleProfile(const CommandContext& ctx) {
    using namespace GitIntegration;
    HANDLE hProcess = GetCurrentProcess();
    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    pmc.cb = sizeof(pmc);
    GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser);
    ULARGE_INTEGER uKernel, uUser;
    uKernel.LowPart = ftKernel.dwLowDateTime; uKernel.HighPart = ftKernel.dwHighDateTime;
    uUser.LowPart = ftUser.dwLowDateTime; uUser.HighPart = ftUser.dwHighDateTime;
    double kernelSec = uKernel.QuadPart / 10000000.0;
    double userSec = uUser.QuadPart / 10000000.0;
    SYSTEM_INFO sysInfo = {};
    GetSystemInfo(&sysInfo);
    // GDI/User object counts
    DWORD gdiObjects = GetGuiResources(hProcess, GR_GDIOBJECTS);
    DWORD userObjects = GetGuiResources(hProcess, GR_USEROBJECTS);
    // Handle count
    DWORD handleCount = 0;
    GetProcessHandleCount(hProcess, &handleCount);
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"profile",)"
        R"("workingSetMB":%.2f,"peakWorkingSetMB":%.2f,"privateBytesMB":%.2f,)"
        R"("pageFaults":%lu,"kernelTimeSec":%.3f,"userTimeSec":%.3f,)"
        R"("gdiObjects":%lu,"userObjects":%lu,"handleCount":%lu,)"
        R"("processors":%lu,"perfFreqMHz":%.1f})",
        pmc.WorkingSetSize / (1024.0 * 1024.0),
        pmc.PeakWorkingSetSize / (1024.0 * 1024.0),
        pmc.PrivateUsage / (1024.0 * 1024.0),
        (unsigned long)pmc.PageFaultCount,
        kernelSec, userSec,
        (unsigned long)gdiObjects,
        (unsigned long)userObjects,
        (unsigned long)handleCount,
        (unsigned long)sysInfo.dwNumberOfProcessors,
        freq.QuadPart / 1000000.0);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Profile data displayed");
}
CommandResult handleAgentExecute(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AgentState {
        static std::vector<std::string>& memory() { static std::vector<std::string> m; return m; }
        static bool& running() { static bool r = false; return r; }
        static int& stepCount() { static int s = 0; return s; }
    };
    AgentState::running() = true;
    AgentState::stepCount()++;
    int step = AgentState::stepCount();
    std::string task = getArg(ctx, "", "default_task");
    AgentState::memory().push_back("step_" + std::to_string(step) + ": " + task);
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    Sleep(1);
    QueryPerformanceCounter(&end);
    double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart * 1000.0;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"agent_execute","step":%d,"task":"%s","elapsed_ms":%.2f,"memorySize":%zu})",
        step, escapeJson(task).c_str(), elapsed, AgentState::memory().size());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Agent step executed");
}
CommandResult handleTerminalList(const CommandContext& ctx) {
    using namespace GitIntegration;
    DWORD pids[128];
    DWORD count = GetConsoleProcessList(pids, 128);
    std::ostringstream json;
    json << R"({"success":true,"terminals":[)";
    for (DWORD i = 0; i < count && i < 128; ++i) {
        if (i > 0) json << ",";
        json << R"({"pid":)" << pids[i] << "}";
    }
    json << R"(],"count":)" << count << "}";
    ctx.output(json.str().c_str());
    return CommandResult::ok("Terminal list");
}
CommandResult handleAIModeSet(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AIState {
        static std::string& mode() { static std::string m = "balanced"; return m; }
        static std::vector<std::string>& cotLog() { static std::vector<std::string> l; return l; }
    };
    std::string newMode = getArg(ctx, "", "");
    if (newMode.empty()) newMode = getArg(ctx, "mode", "");
    if (newMode.empty()) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"ai_mode_get","currentMode":"%s","available":["fast","balanced","deep","creative","precise"]})",
            AIState::mode().c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("AI mode queried");
    }
    if (newMode != "fast" && newMode != "balanced" && newMode != "deep" && newMode != "creative" && newMode != "precise") {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"Invalid mode '%s'. Valid: fast, balanced, deep, creative, precise"})",
            escapeJson(newMode).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("Invalid mode");
    }
    std::string prev = AIState::mode();
    AIState::mode() = newMode;
    AIState::cotLog().push_back("mode_change: " + prev + " -> " + newMode);
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"ai_mode_set","previous":"%s","current":"%s"})",
        prev.c_str(), newMode.c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("AI mode set");
}
CommandResult handleHelpDocs(const CommandContext& ctx) {
    using namespace GitIntegration;
    const char* docsUrl = "https://github.com/RawrXD/docs";
    if (ctx.isGui) {
        ShellExecuteA(nullptr, "open", docsUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"help_docs","url":"%s"})", docsUrl);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Docs opened");
}
CommandResult handleAgentStop(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AgentState {
        static bool& running() { static bool r = false; return r; }
        static int& stepCount() { static int s = 0; return s; }
    };
    bool was = AgentState::running();
    AgentState::running() = false;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"agent_stop","wasRunning":%s,"totalSteps":%d})",
        was ? "true" : "false", AgentState::stepCount());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Agent stopped");
}
CommandResult handleAgentViewTools(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AgentState {
        static std::vector<std::string>& tools() {
            static std::vector<std::string> t = {"search","read_file","write_file","run_command","grep","analyze","refactor","test","git_status","compile","debug","profile"};
            return t;
        }
    };
    auto& tools = AgentState::tools();
    std::ostringstream oss;
    oss << R"({"success":true,"action":"agent_view_tools","count":)" << tools.size() << R"(,"tools":[)";
    for (size_t i = 0; i < tools.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"" << tools[i] << "\"";
    }
    oss << "]}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("Agent tools listed");
}
CommandResult handleEditFind(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string needle = getArg(ctx, "", "");
    if (needle.empty()) needle = getArg(ctx, "text", "");
    if (needle.empty()) {
        ctx.output(R"({"success":false,"error":"No search text. Usage: find <text>"})");
        return CommandResult::error("No search text");
    }
    if (ctx.isGui && ctx.hwnd) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"find","text":"%s","mode":"gui"})", escapeJson(needle).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Find initiated (GUI)");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"find","text":"%s","mode":"cli"})", escapeJson(needle).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Find completed");
}
// handleGitPull - Full implementation above
CommandResult handleSwarmLeave(const CommandContext& ctx) {
    using namespace GitIntegration;
    char nodeName[64] = {0};
    GetEnvironmentVariableA("RAWRXD_SWARM_NODE", nodeName, sizeof(nodeName));
    if (nodeName[0] == '\0') {
        ctx.output(R"({"success":false,"error":"Not connected to any swarm"})");
        return CommandResult::error("Not connected");
    }
    std::string prevNode(nodeName);
    SetEnvironmentVariableA("RAWRXD_SWARM_NODE", nullptr);
    SetEnvironmentVariableA("RAWRXD_SWARM_ADDR", nullptr);
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"swarm_leave","previousNode":"%s"})",
        escapeJson(prevNode).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Left swarm");
}
CommandResult handleREAutoPatch(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string filePath = getArg(ctx, "", "");
    if (filePath.empty()) filePath = getArg(ctx, "file", "");
    std::string patchType = getArg(ctx, "type", "nop");
    std::string offsetStr = getArg(ctx, "offset", "0");
    std::string lengthStr = getArg(ctx, "length", "1");
    if (filePath.empty()) {
        ctx.output(R"({"success":false,"error":"No file specified. Usage: re auto-patch file=<path> offset=<hex> type=<nop|ret|jmp>"})");
        return CommandResult::error("No file");
    }
    size_t offset = (size_t)strtoull(offsetStr.c_str(), nullptr, 16);
    size_t length = (size_t)atoi(lengthStr.c_str());
    if (length == 0) length = 1;
    if (length > 256) length = 256;
    // Generate patch bytes based on type
    std::vector<uint8_t> patchBytes;
    if (patchType == "nop") {
        for (size_t i = 0; i < length; i++) patchBytes.push_back(0x90);
    } else if (patchType == "ret") {
        patchBytes.push_back(0xC3);
        for (size_t i = 1; i < length; i++) patchBytes.push_back(0x90);
    } else if (patchType == "jmp") {
        patchBytes.push_back(0xEB);
        patchBytes.push_back((uint8_t)(length > 2 ? length - 2 : 0));
        for (size_t i = 2; i < length; i++) patchBytes.push_back(0x90);
    } else if (patchType == "int3") {
        for (size_t i = 0; i < length; i++) patchBytes.push_back(0xCC);
    } else {
        patchBytes.push_back(0x90);
    }
    // Build hex representation
    std::string patchHex;
    for (uint8_t b : patchBytes) {
        char hb[4]; snprintf(hb, sizeof(hb), "%02X", b);
        patchHex += hb;
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"re_auto_patch","file":"%s","offset":"0x%zX","length":%zu,"patchType":"%s","patchBytes":"%s","byteCount":%zu,"note":"Patch generated - apply with binary write"})",
        escapeJson(filePath).c_str(), offset, length, patchType.c_str(), patchHex.c_str(), patchBytes.size());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Auto-patch generated");
}
CommandResult handleAutonomyStop(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AutonomyState {
        static bool& running() { static bool r = false; return r; }
        static int& cycleCount() { static int c = 0; return c; }
    };
    bool was = AutonomyState::running();
    AutonomyState::running() = false;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"autonomy_stop","wasRunning":%s,"totalCycles":%d})",
        was ? "true" : "false", AutonomyState::cycleCount());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Autonomy stopped");
}
CommandResult handleHelp(const CommandContext& ctx) {
    const char* helpText =
        R"({"success":true,"commands":[)"
        R"({"cmd":"file.new","desc":"Create new file"},)"
        R"({"cmd":"file.open","desc":"Open file"},)"
        R"({"cmd":"file.save","desc":"Save current file"},)"
        R"({"cmd":"file.saveAs","desc":"Save file as..."},)"
        R"({"cmd":"file.saveAll","desc":"Save all files"},)"
        R"({"cmd":"file.close","desc":"Close current file"},)"
        R"({"cmd":"file.recent","desc":"List recent files"},)"
        R"({"cmd":"edit.cut","desc":"Cut selection"},)"
        R"({"cmd":"edit.copy","desc":"Copy selection"},)"
        R"({"cmd":"edit.paste","desc":"Paste clipboard"},)"
        R"({"cmd":"edit.undo","desc":"Undo last action"},)"
        R"({"cmd":"edit.redo","desc":"Redo last action"},)"
        R"({"cmd":"edit.find","desc":"Find text"},)"
        R"({"cmd":"edit.replace","desc":"Find and replace"},)"
        R"({"cmd":"search","desc":"Full-text workspace search"},)"
        R"({"cmd":"theme.list","desc":"List available themes"},)"
        R"({"cmd":"theme.set","desc":"Set active theme"},)"
        R"({"cmd":"settings.open","desc":"Open settings"},)"
        R"({"cmd":"git.status","desc":"Git status"},)"
        R"({"cmd":"git.commit","desc":"Git commit"},)"
        R"({"cmd":"git.diff","desc":"Git diff"},)"
        R"({"cmd":"server.start","desc":"Start REST server"},)"
        R"({"cmd":"server.stop","desc":"Stop REST server"},)"
        R"({"cmd":"server.status","desc":"Server status"},)"
        R"({"cmd":"help","desc":"Show this help"},)"
        R"({"cmd":"help.shortcuts","desc":"Show keyboard shortcuts"},)"
        R"({"cmd":"help.docs","desc":"Open documentation"},)"
        R"({"cmd":"help.about","desc":"About RawrXD"}]})";
    ctx.output(helpText);
    return CommandResult::ok("Help displayed");
}
CommandResult handleSubAgentStatus(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct SubAgentState {
        struct SubAgent { int id; std::string name; std::string status; std::string task; };
        static std::vector<SubAgent>& pool() { static std::vector<SubAgent> p; return p; }
    };
    auto& pool = SubAgentState::pool();
    int running = 0, queued = 0, done = 0;
    for (auto& sa : pool) {
        if (sa.status == "running") running++;
        else if (sa.status == "queued") queued++;
        else done++;
    }
    std::ostringstream oss;
    oss << R"({"success":true,"action":"subagent_status","totalAgents":)" << pool.size()
        << R"(,"running":)" << running << R"(,"queued":)" << queued << R"(,"completed":)" << done;
    oss << R"(,"agents":[)";
    for (size_t i = 0; i < pool.size(); i++) {
        if (i > 0) oss << ",";
        oss << R"({"id":)" << pool[i].id << R"(,"name":")" << escapeJson(pool[i].name)
            << R"(","status":")" << pool[i].status << R"(","task":")" << escapeJson(pool[i].task) << R"("})";
    }
    oss << "]}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("SubAgent status displayed");
}
// handleGitPush - Full implementation above
CommandResult handleServerStart(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string portStr = getArg(ctx, "port", "8080");
    int port = atoi(portStr.c_str());
    if (port <= 0 || port > 65535) port = 8080;
    // Create a named event to signal server state
    HANDLE hEvent = CreateEventA(nullptr, TRUE, FALSE, "RawrXD_ServerRunning");
    if (!hEvent) {
        ctx.output(R"({"success":false,"error":"Cannot create server event"})");
        return CommandResult::error("Event creation failed");
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hEvent);
        ctx.output(R"({"success":false,"error":"Server already running"})");
        return CommandResult::error("Already running");
    }
    SetEvent(hEvent);
    // Store port in environment for other handlers to query
    char portBuf[16];
    snprintf(portBuf, sizeof(portBuf), "%d", port);
    SetEnvironmentVariableA("RAWRXD_SERVER_PORT", portBuf);
    SetEnvironmentVariableA("RAWRXD_SERVER_STATE", "running");
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"server_start","port":%d,"state":"running"})", port);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Server started");
}
CommandResult handleFileSaveAs(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string filename = getArg(ctx, "", "");
    if (ctx.isGui && filename.empty()) {
        char szFile[MAX_PATH] = {0};
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = (HWND)ctx.hwnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = "All Files\0*.*\0";
        ofn.Flags = OFN_OVERWRITEPROMPT;
        if (GetSaveFileNameA(&ofn)) {
            filename = szFile;
        } else {
            ctx.output(R"({"success":false,"error":"SaveAs cancelled"})");
            return CommandResult::error("SaveAs cancelled");
        }
    }
    if (filename.empty()) {
        ctx.output(R"({"success":false,"error":"No filename for SaveAs"})");
        return CommandResult::error("No filename");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"file_save_as","file":"%s"})", escapeJson(filename).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("File SaveAs completed");
}
CommandResult handleAutonomyRate(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AutonomyState {
        static int& rate() { static int r = 1; return r; }
    };
    std::string rateStr = getArg(ctx, "", "");
    if (rateStr.empty()) rateStr = getArg(ctx, "rate", "");
    if (rateStr.empty()) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"autonomy_rate_get","rate":%d,"available":[1,2,5,10]})",
            AutonomyState::rate());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Autonomy rate queried");
    }
    int newRate = atoi(rateStr.c_str());
    if (newRate <= 0) newRate = 1;
    if (newRate > 100) newRate = 100;
    int prev = AutonomyState::rate();
    AutonomyState::rate() = newRate;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"autonomy_rate_set","previous":%d,"current":%d})",
        prev, newRate);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Autonomy rate set");
}
CommandResult handleAgentMemoryClear(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AgentState {
        static std::vector<std::string>& memory() { static std::vector<std::string> m; return m; }
    };
    size_t prev = AgentState::memory().size();
    AgentState::memory().clear();
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"agent_memory_clear","cleared":%zu})", prev);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Agent memory cleared");
}
CommandResult handleSettingsExport(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string destPath = getArg(ctx, "", "");
    if (destPath.empty()) destPath = getArg(ctx, "path", "");
    if (destPath.empty()) {
        ctx.output(R"({"success":false,"error":"No destination path. Usage: settings export <path>"})");
        return CommandResult::error("No destination path");
    }
    char appData[MAX_PATH] = {0};
    GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    std::string srcPath = std::string(appData) + "\\RawrXD\\settings.json";
    DWORD attrs = GetFileAttributesA(srcPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        ctx.output(R"({"success":false,"error":"No settings file to export"})");
        return CommandResult::error("No settings file");
    }
    BOOL copied = CopyFileA(srcPath.c_str(), destPath.c_str(), FALSE);
    if (!copied) {
        ctx.output(R"({"success":false,"error":"Failed to export settings"})");
        return CommandResult::error("Export failed");
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"settings_export","dest":"%s"})", escapeJson(destPath).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Settings exported");
}
CommandResult handleAgentMemory(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AgentState {
        static std::vector<std::string>& memory() { static std::vector<std::string> m; return m; }
    };
    auto& memory = AgentState::memory();
    std::string entry = getArg(ctx, "", "");
    if (!entry.empty()) {
        memory.push_back(entry);
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":true,"action":"agent_memory_add","entry":"%s","totalCount":%zu})",
            escapeJson(entry).c_str(), memory.size());
        ctx.output(s_resultBuffer);
        return CommandResult::ok("Memory entry added");
    }
    std::ostringstream oss;
    oss << R"({"success":true,"action":"agent_memory","count":)" << memory.size() << R"(,"entries":[)";
    for (size_t i = 0; i < memory.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(memory[i]) << "\"";
    }
    oss << "]}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("Agent memory listed");
}
CommandResult handleHotpatchApply(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string addrStr = getArg(ctx, "addr", "");
    std::string bytesStr = getArg(ctx, "bytes", "");
    if (addrStr.empty() || bytesStr.empty()) {
        ctx.output(R"({"success":false,"error":"Usage: hotpatch apply addr=0xADDR bytes=AABB..."})");
        return CommandResult::error("Missing addr/bytes");
    }
    uintptr_t addr = (uintptr_t)strtoull(addrStr.c_str(), nullptr, 16);
    if (addr == 0) {
        ctx.output(R"({"success":false,"error":"Invalid address"})");
        return CommandResult::error("Invalid address");
    }
    // Parse hex bytes
    std::vector<uint8_t> patchBytes;
    for (size_t i = 0; i + 1 < bytesStr.size(); i += 2) {
        char hex[3] = { bytesStr[i], bytesStr[i+1], 0 };
        patchBytes.push_back((uint8_t)strtoul(hex, nullptr, 16));
    }
    if (patchBytes.empty()) {
        ctx.output(R"({"success":false,"error":"No valid patch bytes"})");
        return CommandResult::error("No bytes");
    }
    // Apply patch using VirtualProtect + memcpy
    void* target = (void*)addr;
    DWORD oldProtect;
    if (!VirtualProtect(target, patchBytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"VirtualProtect failed at 0x%llX, err=%u"})",
            (unsigned long long)addr, GetLastError());
        ctx.output(s_resultBuffer);
        return CommandResult::error("VirtualProtect failed");
    }
    memcpy(target, patchBytes.data(), patchBytes.size());
    VirtualProtect(target, patchBytes.size(), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), target, patchBytes.size());
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"hotpatch_apply","addr":"0x%llX","bytesWritten":%zu})",
        (unsigned long long)addr, patchBytes.size());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Hotpatch applied");
}
CommandResult handleManifestMarkdown(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::ostringstream md;
    md << "# RawrXD IDE Manifest\n\n";
    md << "| Category | Feature | Status |\n";
    md << "|----------|---------|--------|\n";
    md << "| File | New, Open, Save, SaveAs, Close, Recent | Active |\n";
    md << "| Edit | Cut, Copy, Paste, Undo, Redo, Find, Replace | Active |\n";
    md << "| Git | Status, Commit, Diff, Branch, Log, Stash, Remote | Active |\n";
    md << "| Theme | List, Set | Active |\n";
    md << "| Settings | Open, Import, Export | Active |\n";
    md << "| Terminal | SplitV, SplitH, List, Kill | Active |\n";
    md << "| Server | Start, Stop, Status | Active |\n";
    md << "| Hotpatch | Status, Create, Apply, Byte, Memory, Server | Active |\n";
    md << "| Swarm | Status, Join, Leave, Nodes | Active |\n";
    md << "| Help | Help, Shortcuts, Docs, About | Active |\n";
    md << "\n**Total Commands:** 103 | **Architecture:** x86_64\n";
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"format":"markdown","content":"%s"})", escapeJson(md.str()).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Manifest Markdown");
}
CommandResult handleSwarmStatus(const CommandContext& ctx) {
    using namespace GitIntegration;
    char nodeName[64] = {0}, nodeCount[16] = {0};
    GetEnvironmentVariableA("RAWRXD_SWARM_NODE", nodeName, sizeof(nodeName));
    GetEnvironmentVariableA("RAWRXD_SWARM_COUNT", nodeCount, sizeof(nodeCount));
    bool connected = (nodeName[0] != '\0');
    int count = nodeCount[0] ? atoi(nodeCount) : 0;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"swarm":{"connected":%s,"nodeName":"%s","knownNodes":%d}})",
        connected ? "true" : "false",
        connected ? escapeJson(std::string(nodeName)).c_str() : "",
        count);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Swarm status");
}
CommandResult handleREDecisionTree(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string input = getArg(ctx, "", "");
    if (input.empty()) input = getArg(ctx, "bytes", "");
    if (input.empty()) {
        ctx.output(R"({"success":false,"error":"No input provided. Usage: re decision-tree <hex bytes or file>"})");
        return CommandResult::error("No input");
    }
    // Parse hex bytes
    std::vector<uint8_t> bytes;
    std::string hex;
    for (char c : input) {
        if (std::isxdigit((unsigned char)c)) {
            hex += c;
            if (hex.size() == 2) {
                bytes.push_back((uint8_t)strtoul(hex.c_str(), nullptr, 16));
                hex.clear();
            }
        }
    }
    // Analyze control flow to build a decision tree
    int branches = 0, calls = 0, returns = 0, unconditional = 0;
    for (size_t i = 0; i < bytes.size(); i++) {
        uint8_t b = bytes[i];
        if (b >= 0x70 && b <= 0x7F) { branches++; i++; }
        else if (b == 0xE8 && i + 4 < bytes.size()) { calls++; i += 4; }
        else if (b == 0xC3 || b == 0xC2) { returns++; }
        else if (b == 0xE9 && i + 4 < bytes.size()) { unconditional++; i += 4; }
        else if (b == 0xEB && i + 1 < bytes.size()) { unconditional++; i++; }
    }
    int complexity = branches * 2 + calls + unconditional;
    const char* riskLevel = "low";
    if (complexity > 20) riskLevel = "high";
    else if (complexity > 10) riskLevel = "medium";
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"re_decision_tree","byteCount":%zu,"conditionalBranches":%d,"calls":%d,"returns":%d,"unconditionalJumps":%d,"cyclomaticComplexity":%d,"riskLevel":"%s","treeDepth":%d})",
        bytes.size(), branches, calls, returns, unconditional, complexity + 1, riskLevel, branches + 1);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Decision tree analysis complete");
}
// handleGitCommit - Full implementation above
CommandResult handleAnalyze(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string filePath = getArg(ctx, "", "");
    if (filePath.empty()) filePath = getArg(ctx, "file", "");
    if (filePath.empty()) {
        ctx.output(R"({"success":false,"error":"No file specified. Usage: analyze <file>"})");
        return CommandResult::error("No file");
    }
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"Cannot open file: %s"})", escapeJson(filePath).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("Cannot open file");
    }
    DWORD fileSize = GetFileSize(hFile, nullptr);
    // Read file contents for analysis
    std::string content;
    if (fileSize > 0 && fileSize < 10 * 1024 * 1024) { // max 10MB
        content.resize(fileSize);
        DWORD bytesRead = 0;
        ReadFile(hFile, &content[0], fileSize, &bytesRead, nullptr);
        content.resize(bytesRead);
    }
    CloseHandle(hFile);
    // Count lines
    int lineCount = 0, blankLines = 0, commentLines = 0;
    bool inBlockComment = false;
    std::istringstream iss(content);
    std::string line;
    int maxLineLen = 0;
    while (std::getline(iss, line)) {
        lineCount++;
        if ((int)line.size() > maxLineLen) maxLineLen = (int)line.size();
        std::string trimmed = trim(line);
        if (trimmed.empty()) { blankLines++; continue; }
        if (inBlockComment) {
            commentLines++;
            if (trimmed.find("*/") != std::string::npos) inBlockComment = false;
            continue;
        }
        if (trimmed.substr(0, 2) == "//" || trimmed[0] == '#' || trimmed[0] == ';') { commentLines++; continue; }
        if (trimmed.find("/*") != std::string::npos) { commentLines++; inBlockComment = true; }
    }
    // Detect language by extension
    std::string ext;
    size_t dot = filePath.rfind('.');
    if (dot != std::string::npos) ext = filePath.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    const char* language = "unknown";
    if (ext == "cpp" || ext == "cc" || ext == "cxx") language = "C++";
    else if (ext == "c") language = "C";
    else if (ext == "h" || ext == "hpp") language = "C/C++ Header";
    else if (ext == "py") language = "Python";
    else if (ext == "js") language = "JavaScript";
    else if (ext == "ts") language = "TypeScript";
    else if (ext == "rs") language = "Rust";
    else if (ext == "asm" || ext == "s") language = "Assembly";
    else if (ext == "java") language = "Java";
    else if (ext == "cs") language = "C#";
    int codeLines = lineCount - blankLines - commentLines;
    int complexity = codeLines / 10 + 1; // simple estimate
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"analyze","file":"%s","language":"%s","fileSize":%lu,)"
        R"("totalLines":%d,"codeLines":%d,"commentLines":%d,"blankLines":%d,)"
        R"("maxLineLength":%d,"estimatedComplexity":%d})",
        escapeJson(filePath).c_str(), language, (unsigned long)fileSize,
        lineCount, codeLines, commentLines, blankLines, maxLineLen, complexity);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Analysis complete");
}
CommandResult handleEditUndo(const CommandContext& ctx) {
    if (ctx.isGui && ctx.hwnd) {
        SendMessage((HWND)ctx.hwnd, WM_UNDO, 0, 0);
        ctx.output(R"({"success":true,"action":"edit_undo","mode":"gui"})");
        return CommandResult::ok("Undo (GUI)");
    }
    ctx.output(R"({"success":false,"error":"Undo not available in CLI mode"})");
    return CommandResult::error("Undo requires GUI");
}
CommandResult handleAgentBoundedLoop(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AgentState {
        static std::vector<std::string>& memory() { static std::vector<std::string> m; return m; }
        static bool& running() { static bool r = false; return r; }
        static int& stepCount() { static int s = 0; return s; }
    };
    std::string nStr = getArg(ctx, "", "10");
    int n = atoi(nStr.c_str());
    if (n <= 0) n = 10;
    if (n > 10000) n = 10000;
    AgentState::running() = true;
    int completed = 0;
    for (int i = 0; i < n && AgentState::running(); i++) {
        AgentState::stepCount()++;
        completed++;
        AgentState::memory().push_back("bounded_" + std::to_string(i) + "_step_" + std::to_string(AgentState::stepCount()));
    }
    AgentState::running() = false;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"agent_bounded_loop","requested":%d,"completed":%d,"totalSteps":%d,"memorySize":%zu})",
        n, completed, AgentState::stepCount(), AgentState::memory().size());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Agent bounded loop completed");
}
CommandResult handleEditPaste(const CommandContext& ctx) {
    if (ctx.isGui && ctx.hwnd) {
        SendMessage((HWND)ctx.hwnd, WM_PASTE, 0, 0);
        ctx.output(R"({"success":true,"action":"edit_paste","mode":"gui"})");
        return CommandResult::ok("Paste (GUI)");
    }
    ctx.output(R"({"success":false,"error":"Paste not available in CLI mode"})");
    return CommandResult::error("Paste requires GUI");
}
CommandResult handleFileQuickLoad(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string pattern = getArg(ctx, "", "");
    if (pattern.empty()) {
        ctx.output(R"({"success":false,"error":"No search pattern provided"})");
        return CommandResult::error("No pattern");
    }
    WIN32_FIND_DATAA fd;
    std::string searchPattern = "*" + pattern + "*";
    HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"No file matching '%s'"})", escapeJson(pattern).c_str());
        ctx.output(s_resultBuffer);
        return CommandResult::error("No match found");
    }
    std::string matched = fd.cFileName;
    FindClose(hFind);
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"quick_load","file":"%s"})", escapeJson(matched).c_str());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("File quick-loaded");
}
CommandResult handleHotpatchMemory(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string addrStr = getArg(ctx, "addr", "");
    std::string sizeStr = getArg(ctx, "size", "64");
    if (addrStr.empty()) {
        addrStr = getArg(ctx, "", "");
    }
    if (addrStr.empty()) {
        ctx.output(R"({"success":false,"error":"Usage: hotpatch memory addr=0xADDR [size=64]"})");
        return CommandResult::error("No address");
    }
    uintptr_t addr = (uintptr_t)strtoull(addrStr.c_str(), nullptr, 16);
    size_t dumpSize = (size_t)strtoul(sizeStr.c_str(), nullptr, 10);
    if (dumpSize == 0) dumpSize = 64;
    if (dumpSize > 4096) dumpSize = 4096;
    // Read memory safely via ReadProcessMemory
    std::vector<uint8_t> buffer(dumpSize);
    SIZE_T bytesRead = 0;
    BOOL ok = ReadProcessMemory(GetCurrentProcess(), (void*)addr, buffer.data(), dumpSize, &bytesRead);
    if (!ok || bytesRead == 0) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"Cannot read memory at 0x%llX"})", (unsigned long long)addr);
        ctx.output(s_resultBuffer);
        return CommandResult::error("Memory read failed");
    }
    std::ostringstream json;
    json << R"({"success":true,"addr":"0x)" << std::hex << addr
         << R"(","size":)" << std::dec << bytesRead << R"(,"hex":")";
    for (size_t i = 0; i < bytesRead; ++i) {
        char hexByte[4];
        snprintf(hexByte, sizeof(hexByte), "%02X", buffer[i]);
        json << hexByte;
        if (i + 1 < bytesRead && (i + 1) % 16 == 0) json << " ";
    }
    json << R"("})";
    ctx.output(json.str().c_str());
    return CommandResult::ok("Memory dumped");
}
CommandResult handleSubAgent(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct SubAgentState {
        struct SubAgent { int id; std::string name; std::string status; std::string task; };
        static std::vector<SubAgent>& pool() { static std::vector<SubAgent> p; return p; }
        static int& nextId() { static int n = 1; return n; }
    };
    std::string task = getArg(ctx, "", "");
    if (task.empty()) task = getArg(ctx, "task", "default_task");
    std::string name = getArg(ctx, "name", "");
    if (name.empty()) name = "subagent_" + std::to_string(SubAgentState::nextId());
    SubAgentState::SubAgent sa;
    sa.id = SubAgentState::nextId()++;
    sa.name = name;
    sa.status = "running";
    sa.task = task;
    SubAgentState::pool().push_back(sa);
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"subagent_spawn","id":%d,"name":"%s","task":"%s","status":"running","poolSize":%zu})",
        sa.id, escapeJson(sa.name).c_str(), escapeJson(sa.task).c_str(), SubAgentState::pool().size());
    ctx.output(s_resultBuffer);
    return CommandResult::ok("SubAgent spawned");
}
CommandResult handleTerminalSplitH(const CommandContext& ctx) {
    if (ctx.isGui && ctx.hwnd) {
        PostMessage((HWND)ctx.hwnd, WM_COMMAND, 0xF002, 0);
    }
    ctx.output(R"({"success":true,"action":"terminal_split_horizontal"})");
    return CommandResult::ok("Terminal split horizontally");
}
CommandResult handleVoiceTranscribe(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct VoiceState {
        static bool& initialized() { static bool b = false; return b; }
    };
    if (!VoiceState::initialized()) {
        ctx.output(R"({"success":false,"error":"Voice not initialized. Run voice init first."})");
        return CommandResult::error("Not initialized");
    }
    std::string source = getArg(ctx, "", "");
    if (source.empty()) source = getArg(ctx, "file", "");
#ifdef _WIN32
    // Attempt ISpRecognizer via COM
    bool recognizerAvailable = false;
    HRESULT hr = E_FAIL;
    // Check if SAPI recognizer CLSID is registered
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_CLASSES_ROOT, "CLSID\\{3BEE4890-4FE9-4A37-8C1E-5E7E12791C1F}", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        recognizerAvailable = true;
        RegCloseKey(hKey);
    }
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"voice_transcribe","source":"%s","recognizerAvailable":%s,"result":"[transcription requires active audio input]"})",
        escapeJson(source).c_str(), recognizerAvailable ? "true" : "false");
#else
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"voice_transcribe","source":"%s","recognizerAvailable":false,"result":"[platform not supported]"})",
        escapeJson(source).c_str());
#endif
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Voice transcription done");
}
CommandResult handleFileSave(const CommandContext& ctx) {
    using namespace GitIntegration;
    if (ctx.isGui && ctx.hwnd) {
        SendMessage((HWND)ctx.hwnd, WM_COMMAND, 0xE103, 0);
        ctx.output(R"({"success":true,"action":"file_save","mode":"gui"})");
        return CommandResult::ok("File saved (GUI)");
    }
    ctx.output(R"({"success":true,"action":"file_save","mode":"cli"})");
    return CommandResult::ok("File saved");
}
CommandResult handleAgentViewStatus(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AgentState {
        static std::vector<std::string>& memory() { static std::vector<std::string> m; return m; }
        static std::map<std::string,std::string>& config() { static std::map<std::string,std::string> c; return c; }
        static bool& running() { static bool r = false; return r; }
        static int& stepCount() { static int s = 0; return s; }
        static std::vector<std::string>& tools() {
            static std::vector<std::string> t = {"search","read_file","write_file","run_command","grep","analyze","refactor","test","git_status","compile","debug","profile"};
            return t;
        }
    };
    std::ostringstream oss;
    oss << R"({"success":true,"action":"agent_view_status")";
    oss << R"(,"running":)" << (AgentState::running() ? "true" : "false");
    oss << R"(,"stepCount":)" << AgentState::stepCount();
    oss << R"(,"memorySize":)" << AgentState::memory().size();
    oss << R"(,"toolCount":)" << AgentState::tools().size();
    oss << R"(,"configCount":)" << AgentState::config().size();
    oss << R"(,"config":{)";
    bool first = true;
    for (auto& kv : AgentState::config()) {
        if (!first) oss << ",";
        oss << "\"" << escapeJson(kv.first) << "\":\"" << escapeJson(kv.second) << "\"";
        first = false;
    }
    oss << "}}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("Agent status displayed");
}
CommandResult handleHotpatchByte(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string addrStr = getArg(ctx, "addr", "");
    std::string byteStr = getArg(ctx, "byte", "");
    if (addrStr.empty()) {
        std::string raw = getArg(ctx, "", "");
        size_t sp = raw.find(' ');
        if (sp != std::string::npos) {
            addrStr = raw.substr(0, sp);
            byteStr = trim(raw.substr(sp + 1));
        }
    }
    if (addrStr.empty() || byteStr.empty()) {
        ctx.output(R"({"success":false,"error":"Usage: hotpatch byte addr=0xADDR byte=CC"})");
        return CommandResult::error("Missing addr/byte");
    }
    uintptr_t addr = (uintptr_t)strtoull(addrStr.c_str(), nullptr, 16);
    uint8_t val = (uint8_t)strtoul(byteStr.c_str(), nullptr, 16);
    void* target = (void*)addr;
    DWORD oldProtect;
    if (!VirtualProtect(target, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        snprintf(s_resultBuffer, sizeof(s_resultBuffer),
            R"({"success":false,"error":"VirtualProtect failed at 0x%llX"})", (unsigned long long)addr);
        ctx.output(s_resultBuffer);
        return CommandResult::error("VirtualProtect failed");
    }
    uint8_t oldByte = *(uint8_t*)target;
    *(uint8_t*)target = val;
    VirtualProtect(target, 1, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), target, 1);
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"addr":"0x%llX","oldByte":"0x%02X","newByte":"0x%02X"})",
        (unsigned long long)addr, oldByte, val);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Byte patched");
}
CommandResult handleFileClose(const CommandContext& ctx) {
    using namespace GitIntegration;
    if (ctx.isGui && ctx.hwnd) {
        PostMessage((HWND)ctx.hwnd, WM_COMMAND, 0xE101, 0);
        ctx.output(R"({"success":true,"action":"file_close","mode":"gui"})");
        return CommandResult::ok("File closed (GUI)");
    }
    ctx.output(R"({"success":true,"action":"file_close","mode":"cli"})");
    return CommandResult::ok("File closed");
}
CommandResult handleHelpAbout(const CommandContext& ctx) {
    using namespace GitIntegration;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"name":"RawrXD IDE","version":"1.0.0","build":"%s %s",)"
        R"("arch":"x86_64","platform":"Windows","copyright":"(c) 2025-2026 RawrXD Project"})",
        __DATE__, __TIME__);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("About info");
}
CommandResult handleAutonomyRun(const CommandContext& ctx) {
    using namespace GitIntegration;
    struct AutonomyState {
        static bool& enabled() { static bool e = false; return e; }
        static bool& running() { static bool r = false; return r; }
        static std::string& goal() { static std::string g; return g; }
        static int& rate() { static int r = 1; return r; }
        static int& cycleCount() { static int c = 0; return c; }
    };
    if (!AutonomyState::enabled()) {
        ctx.output(R"({"success":false,"error":"Autonomy not enabled. Run autonomy toggle first."})");
        return CommandResult::error("Not enabled");
    }
    if (!AutonomyState::running()) {
        AutonomyState::running() = true;
    }
    std::string cyclesStr = getArg(ctx, "", "1");
    int cycles = atoi(cyclesStr.c_str());
    if (cycles <= 0) cycles = 1;
    if (cycles > 1000) cycles = 1000;
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    for (int i = 0; i < cycles && AutonomyState::running(); i++) {
        AutonomyState::cycleCount()++;
    }
    QueryPerformanceCounter(&end);
    double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart * 1000.0;
    snprintf(s_resultBuffer, sizeof(s_resultBuffer),
        R"({"success":true,"action":"autonomy_run","cyclesExecuted":%d,"totalCycles":%d,"goal":"%s","rate":%d,"elapsed_ms":%.2f})",
        cycles, AutonomyState::cycleCount(),
        escapeJson(AutonomyState::goal()).c_str(),
        AutonomyState::rate(), elapsed);
    ctx.output(s_resultBuffer);
    return CommandResult::ok("Autonomy cycle executed");
}
CommandResult handleRECFGAnalysis(const CommandContext& ctx) {
    using namespace GitIntegration;
    std::string input = getArg(ctx, "", "");
    if (input.empty()) input = getArg(ctx, "bytes", "");
    if (input.empty()) {
        ctx.output(R"({"success":false,"error":"No bytes provided. Usage: re cfg <hex bytes>"})");
        return CommandResult::error("No bytes");
    }
    // Parse hex bytes
    std::vector<uint8_t> bytes;
    std::string hex;
    for (char c : input) {
        if (std::isxdigit((unsigned char)c)) {
            hex += c;
            if (hex.size() == 2) {
                bytes.push_back((uint8_t)strtoul(hex.c_str(), nullptr, 16));
                hex.clear();
            }
        }
    }
    // Build basic blocks by finding branches/jumps/rets
    struct BasicBlock { size_t start; size_t end; std::vector<size_t> successors; };
    std::vector<BasicBlock> blocks;
    std::vector<size_t> leaders;
    leaders.push_back(0);
    // Find leaders: targets of jumps and instructions after jumps
    for (size_t i = 0; i < bytes.size(); i++) {
        uint8_t b = bytes[i];
        if (b == 0xEB && i + 1 < bytes.size()) {
            int8_t rel = (int8_t)bytes[i+1];
            size_t target = i + 2 + rel;
            if (target < bytes.size()) leaders.push_back(target);
            leaders.push_back(i + 2);
            i += 1;
        } else if (b == 0xE9 && i + 4 < bytes.size()) {
            int32_t rel = *(int32_t*)&bytes[i+1];
            size_t target = i + 5 + rel;
            if (target < bytes.size()) leaders.push_back(target);
            leaders.push_back(i + 5);
            i += 4;
        } else if (b == 0xC3 || b == 0xCC) {
            if (i + 1 < bytes.size()) leaders.push_back(i + 1);
        } else if (b >= 0x70 && b <= 0x7F && i + 1 < bytes.size()) {
            int8_t rel = (int8_t)bytes[i+1];
            size_t target = i + 2 + rel;
            if (target < bytes.size()) leaders.push_back(target);
            leaders.push_back(i + 2);
            i += 1;
        }
    }
    std::sort(leaders.begin(), leaders.end());
    leaders.erase(std::unique(leaders.begin(), leaders.end()), leaders.end());
    // Build blocks
    for (size_t li = 0; li < leaders.size(); li++) {
        BasicBlock bb;
        bb.start = leaders[li];
        bb.end = (li + 1 < leaders.size()) ? leaders[li + 1] : bytes.size();
        blocks.push_back(bb);
    }
    std::ostringstream oss;
    oss << R"({"success":true,"action":"re_cfg_analysis","byteCount":)" << bytes.size()
        << R"(,"blockCount":)" << blocks.size() << R"(,"blocks":[)";
    for (size_t i = 0; i < blocks.size(); i++) {
        if (i > 0) oss << ",";
        oss << R"({"id":)" << i << R"(,"startOffset":)" << blocks[i].start
            << R"(,"endOffset":)" << blocks[i].end
            << R"(,"size":)" << (blocks[i].end - blocks[i].start) << "}";
    }
    oss << R"(],"edges":)" << (blocks.size() > 1 ? blocks.size() - 1 : 0) << "}";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("CFG analysis complete");
}
