// Agentic Core - Integration Layer Implementation
// Provides a unified agentic automation interface
// Delegates to real OS APIs for file, terminal, and search operations

#include "agentic_core.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <atomic>
#include <mutex>
#include <future>
#include <filesystem>

#include "logging/logger.h"
static Logger s_logger("agentic_core");

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace AgenticCore {

// ── Real file operation dispatcher ─────────────────────────────────────────
static TaskResult executeFileOp(const std::string& instruction, const std::string& workspaceRoot) {
    TaskResult result;
    // Parse instruction: "read <path>", "write <path> <content>", "list <path>", "delete <path>"
    std::istringstream iss(instruction);
    std::string verb, path;
    iss >> verb >> path;

    // Resolve path relative to workspace if not absolute
    std::filesystem::path resolved = path;
    if (resolved.is_relative() && !workspaceRoot.empty()) {
        resolved = std::filesystem::path(workspaceRoot) / resolved;
    }

    if (verb == "read" || verb == "Read" || verb == "READ") {
        std::ifstream file(resolved, std::ios::binary);
        if (!file) {
            result.success = false;
            result.errorMessage = "Cannot open file: " + resolved.string();
            return result;
        }
        std::ostringstream content;
        content << file.rdbuf();
        result.output = content.str();
        result.success = true;
    }
    else if (verb == "write" || verb == "Write" || verb == "WRITE") {
        // Rest of instruction after path is content
        std::string content;
        std::getline(iss, content);
        if (!content.empty() && content[0] == ' ') content.erase(0, 1);

        // Ensure parent directory exists
        auto parent = resolved.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
        }

        std::ofstream file(resolved, std::ios::binary);
        if (!file) {
            result.success = false;
            result.errorMessage = "Cannot write file: " + resolved.string();
            return result;
        }
        file << content;
        file.close();
        result.output = "Written " + std::to_string(content.size()) + " bytes to " + resolved.string();
        result.success = true;
    }
    else if (verb == "list" || verb == "List" || verb == "LIST" || verb == "ls") {
        std::error_code ec;
        if (!std::filesystem::exists(resolved, ec)) {
            result.success = false;
            result.errorMessage = "Path does not exist: " + resolved.string();
            return result;
        }
        std::ostringstream listing;
        for (const auto& entry : std::filesystem::directory_iterator(resolved, ec)) {
            if (ec) break;
            listing << (entry.is_directory() ? "[DIR]  " : "[FILE] ")
                     << entry.path().filename().string();
            if (entry.is_regular_file()) {
                listing << " (" << entry.file_size(ec) << " bytes)";
            }
            listing << "\n";
        }
        result.output = listing.str();
        result.success = !ec;
        if (ec) result.errorMessage = ec.message();
    }
    else if (verb == "delete" || verb == "Delete" || verb == "DELETE" || verb == "rm") {
        std::error_code ec;
        bool removed = std::filesystem::remove(resolved, ec);
        result.success = removed && !ec;
        result.output = removed ? "Deleted: " + resolved.string()
                                : "Not found: " + resolved.string();
        if (ec) result.errorMessage = ec.message();
    }
    else if (verb == "exists" || verb == "Exists") {
        std::error_code ec;
        bool ex = std::filesystem::exists(resolved, ec);
        result.success = true;
        result.output = ex ? "true" : "false";
    }
    else {
        // Fallback: treat whole instruction as a path to read
        std::ifstream file(resolved.empty() ? std::filesystem::path(instruction) : resolved, std::ios::binary);
        if (file) {
            std::ostringstream content;
            content << file.rdbuf();
            result.output = content.str();
            result.success = true;
        } else {
            result.success = false;
            result.errorMessage = "Unrecognized file operation: " + verb;
        }
    }
    return result;
}

// ── Real terminal command dispatcher ───────────────────────────────────────
static TaskResult executeTerminalCmd(const std::string& instruction, const std::string& workspaceRoot) {
    TaskResult result;
    if (instruction.empty()) {
        result.success = false;
        result.errorMessage = "Empty command";
        return result;
    }

#ifdef _WIN32
    // Build full command with workspace as working directory
    std::string fullCmd = "cmd.exe /c \"cd /d \"" + workspaceRoot + "\" && " + instruction + "\"";
    
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    
    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        result.success = false;
        result.errorMessage = "Failed to create pipe: " + std::to_string(GetLastError());
        return result;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    PROCESS_INFORMATION pi{};
    BOOL created = CreateProcessA(
        nullptr, const_cast<char*>(fullCmd.c_str()),
        nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, workspaceRoot.empty() ? nullptr : workspaceRoot.c_str(),
        &si, &pi);

    CloseHandle(hWritePipe);

    if (!created) {
        CloseHandle(hReadPipe);
        result.success = false;
        result.errorMessage = "CreateProcess failed: " + std::to_string(GetLastError());
        return result;
    }

    // Read output with bounded wait (30s)
    std::ostringstream output;
    char buf[4096];
    DWORD bytesRead;
    
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 30000);
    // Drain remaining pipe content
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        output << buf;
        if (output.str().size() > 256 * 1024) break; // 256KB output cap
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    result.output = output.str();
    result.success = (waitResult == WAIT_OBJECT_0 && exitCode == 0);
    if (waitResult == WAIT_TIMEOUT) {
        result.errorMessage = "Command timed out (30s)";
    } else if (exitCode != 0) {
        result.errorMessage = "Exit code: " + std::to_string(exitCode);
    }
#else
    // POSIX: popen
    std::string cmd = "cd " + workspaceRoot + " && " + instruction + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        result.success = false;
        result.errorMessage = "popen failed";
        return result;
    }
    std::ostringstream output;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        output << buf;
        if (output.str().size() > 256 * 1024) break;
    }
    int status = pclose(pipe);
    result.output = output.str();
    result.success = (status == 0);
    if (status != 0) result.errorMessage = "Exit code: " + std::to_string(status);
#endif
    return result;
}

// ── Real search dispatcher ─────────────────────────────────────────────────
static TaskResult executeSearch(const std::string& instruction, const std::string& workspaceRoot) {
    TaskResult result;
    if (instruction.empty()) {
        result.success = false;
        result.errorMessage = "Empty search query";
        return result;
    }

    std::string searchRoot = workspaceRoot.empty() ? "." : workspaceRoot;
    std::ostringstream output;
    size_t matchCount = 0;
    const size_t maxMatches = 200;

    // Recursive file content search
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(searchRoot, 
            std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (ec) continue;
        if (!entry.is_regular_file()) continue;

        // Skip binary/large files and common non-searchable dirs
        auto ext = entry.path().extension().string();
        auto pathStr = entry.path().string();
        if (pathStr.find("\\build\\") != std::string::npos ||
            pathStr.find("\\.git\\") != std::string::npos ||
            pathStr.find("\\node_modules\\") != std::string::npos) continue;
        if (ext == ".obj" || ext == ".exe" || ext == ".dll" || ext == ".lib" ||
            ext == ".pdb" || ext == ".png" || ext == ".jpg" || ext == ".zip") continue;
        if (entry.file_size(ec) > 2 * 1024 * 1024) continue; // Skip >2MB

        std::ifstream file(entry.path());
        if (!file) continue;

        std::string line;
        int lineNum = 0;
        while (std::getline(file, line)) {
            lineNum++;
            if (line.find(instruction) != std::string::npos) {
                auto relPath = std::filesystem::relative(entry.path(), searchRoot, ec);
                output << relPath.string() << ":" << lineNum << ": "
                       << line.substr(0, 200) << "\n";
                matchCount++;
                if (matchCount >= maxMatches) goto done;
            }
        }
    }
done:
    result.output = output.str();
    result.success = true;
    if (matchCount >= maxMatches) {
        result.output += "\n... (truncated at " + std::to_string(maxMatches) + " matches)\n";
    }
    if (matchCount == 0) {
        result.output = "No matches found for: " + instruction;
    }
    return result;
}

class AgenticCoreImpl : public IAgenticCore {
public:
    AgenticCoreImpl() = default;
    ~AgenticCoreImpl() override { shutdown(); }
    
    bool initialize(const CoreConfig& config) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = config;

        // Validate workspace root exists
        if (!config.workspaceRoot.empty()) {
            std::error_code ec;
            if (!std::filesystem::exists(config.workspaceRoot, ec)) {
                s_logger.error( "[AgenticCore] WARNING: workspace root does not exist: "
                          << config.workspaceRoot << std::endl;
            }
        }

        m_ready = true;
        s_logger.info("[AgenticCore] Initialized with workspace: ");
        if (!config.modelPath.empty()) {
            s_logger.info("[AgenticCore] Model: ");
        }
        m_taskCount = 0;
        m_totalLatencyMs = 0.0;
        return true;
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_ready) {
            m_ready = false;
            s_logger.info("[AgenticCore] Shutdown (processed ");
        }
    }
    
    bool isReady() const override { return m_ready.load(); }
    
    TaskResult executeTask(const std::string& instruction, TaskType type) override {
        auto start = std::chrono::high_resolution_clock::now();
        TaskResult result;
        
        if (!m_ready) {
            result.success = false;
            result.errorMessage = "Agentic core not initialized";
            return result;
        }
        
        if (m_cancelled) {
            result.success = false;
            result.errorMessage = "Task cancelled before execution";
            return result;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cancelled = false;
        
        s_logger.info("[AgenticCore] Executing ");
        
        // Route to real handler based on task type
        switch (type) {
        case TaskType::FileOperation:
            result = executeFileOp(instruction, m_config.workspaceRoot);
            break;
        case TaskType::TerminalCommand:
            result = executeTerminalCmd(instruction, m_config.workspaceRoot);
            break;
        case TaskType::Search:
            result = executeSearch(instruction, m_config.workspaceRoot);
            break;
        default:
            // General task: delegate to terminal as a command
            result = executeTerminalCmd(instruction, m_config.workspaceRoot);
            break;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        result.latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        m_taskCount.fetch_add(1, std::memory_order_relaxed);
        m_totalLatencyMs += result.latencyMs;
        
        s_logger.info("[AgenticCore] Task ");
        return result;
    }
    
    TaskResult executeTaskAsync(const std::string& instruction, 
                                ProgressCallback onProgress) override {
        if (onProgress) {
            onProgress("Starting task...", 0.0f);
        }
        
        // Launch on a real async thread
        auto future = std::async(std::launch::async, [this, instruction, onProgress]() {
            if (onProgress) onProgress("Executing...", 0.25f);
            auto result = executeTask(instruction);
            if (onProgress) {
                onProgress(result.success ? "Complete" : "Failed", 1.0f);
            }
            return result;
        });
        
        // Wait for result (with timeout)
        auto status = future.wait_for(std::chrono::seconds(60));
        if (status == std::future_status::timeout) {
            m_cancelled = true;
            TaskResult timeout;
            timeout.success = false;
            timeout.errorMessage = "Async task timed out (60s)";
            if (onProgress) onProgress("Timeout", 1.0f);
            return timeout;
        }
        return future.get();
    }
    
    void cancelCurrentTask() override {
        m_cancelled = true;
        s_logger.info("[AgenticCore] Task cancellation requested");
    }
    
    std::string getStatus() const override {
        if (!m_ready) return "not_initialized";
        if (m_cancelled) return "cancelled";
        return "ready (tasks=" + std::to_string(m_taskCount.load()) + ")";
    }
    
private:
    static const char* taskTypeName(TaskType t) {
        switch (t) {
        case TaskType::FileOperation:  return "FileOp";
        case TaskType::TerminalCommand: return "Terminal";
        case TaskType::Search:          return "Search";
        default:                        return "General";
        }
    }

    CoreConfig m_config;
    std::atomic<bool> m_ready{false};
    std::atomic<bool> m_cancelled{false};
    std::atomic<uint64_t> m_taskCount{0};
    double m_totalLatencyMs = 0.0;
    mutable std::mutex m_mutex;
};

std::unique_ptr<IAgenticCore> createAgenticCore() {
    return std::make_unique<AgenticCoreImpl>();
}

} // namespace AgenticCore

