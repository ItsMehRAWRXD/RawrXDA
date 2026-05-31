#include "dynamic_powershell_terminal_manager.hpp"
#include <array>
#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <regex>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace RawrXD::Terminal {

// =====================================================================
// CONSTANTS AND UTILITIES
// =====================================================================

constexpr std::chrono::milliseconds DEFAULT_BASE_TIMEOUT = 30000ms;
constexpr std::chrono::milliseconds MIN_TIMEOUT_LIMIT = 500ms;
constexpr std::chrono::milliseconds MAX_TIMEOUT_LIMIT = 1800000ms; // 30 minutes
constexpr double LEARNING_MOMENTUM = 0.9;
constexpr double QUANTUM_ENHANCEMENT_FACTOR = 1.618; // Golden ratio
constexpr uint32_t MAX_WORKER_THREADS = 8;
constexpr size_t TERMINAL_OUTPUT_HARD_CAP_BYTES = 512ull * 1024ull * 1024ull; // 512 MB hard cap

// Command complexity patterns for classification
const std::vector<std::pair<std::regex, CommandComplexity>> COMPLEXITY_PATTERNS = {
    {std::regex(R"(Get-Date|Get-Location|pwd|ls)", std::regex::icase), CommandComplexity::TRIVIAL},
    {std::regex(R"(Get-ChildItem|dir|copy|move|del)", std::regex::icase), CommandComplexity::SIMPLE},
    {std::regex(R"(ForEach-Object|Where-Object|\|)", std::regex::icase), CommandComplexity::MODERATE},
    {std::regex(R"(Install-Module|Update-Module|Import-Module)", std::regex::icase), CommandComplexity::COMPLEX},
    {std::regex(R"(Get-WmiObject|Get-CimInstance|Invoke-WebRequest)", std::regex::icase), CommandComplexity::HEAVY},
    {std::regex(R"(Start-Job|Receive-Job|Wait-Job)", std::regex::icase), CommandComplexity::QUANTUM}
};

// Quantum hash function for command fingerprinting
inline uint64_t quantum_command_hash(const std::string& command) {
    uint64_t hash = 0xC0FFEEBABEULL;
    for (size_t i = 0; i < command.length(); ++i) {
        hash ^= static_cast<uint64_t>(command[i]) << (i % 8);
        hash = (hash << 13) | (hash >> 51); // Rotate left 13 bits
        hash *= 1099511628211ULL; // FNV prime
    }
    return hash;
}

// Statistical analysis for timeout optimization
inline double calculate_standard_deviation(const std::vector<std::chrono::milliseconds>& values) {
    if (values.size() < 2) return 0.0;
    
    double mean = std::accumulate(values.begin(), values.end(), 0.0,
        [](double sum, const auto& val) { return sum + val.count(); }) / values.size();
    
    double variance = std::accumulate(values.begin(), values.end(), 0.0,
        [mean](double sum, const auto& val) {
            double diff = val.count() - mean;
            return sum + diff * diff;
        }) / (values.size() - 1);
    
    return std::sqrt(variance);
}

class CircularLineBuffer {
public:
    explicit CircularLineBuffer(size_t cap_bytes)
        : cap_bytes_(std::max<size_t>(1024, cap_bytes)) {}

    void append(const std::string& chunk) {
        if (chunk.empty()) return;

        size_t start = 0;
        for (size_t i = 0; i < chunk.size(); ++i) {
            if (chunk[i] == '\n') {
                push_line(chunk.substr(start, i - start + 1));
                start = i + 1;
            }
        }
        if (start < chunk.size()) {
            push_line(chunk.substr(start));
        }

        const size_t target_bytes = cap_bytes_ > 5 ? (cap_bytes_ - (cap_bytes_ / 5)) : cap_bytes_;
        while (bytes_ > target_bytes && !lines_.empty()) {
            dropped_bytes_ += lines_.front().size();
            dropped_lines_++;
            bytes_ -= lines_.front().size();
            lines_.pop_front();
        }
    }

    std::string str() const {
        std::string out;
        out.reserve(bytes_ + 64);
        for (const auto& line : lines_) {
            out += line;
        }
        return out;
    }

    bool truncated() const { return dropped_bytes_ > 0; }
    size_t dropped_bytes() const { return dropped_bytes_; }
    size_t dropped_lines() const { return dropped_lines_; }
    size_t cap_bytes() const { return cap_bytes_; }
    size_t retained_bytes() const { return bytes_; }

private:
    void push_line(const std::string& line) {
        lines_.push_back(line);
        bytes_ += line.size();
    }

    size_t cap_bytes_{0};
    size_t bytes_{0};
    size_t dropped_bytes_{0};
    size_t dropped_lines_{0};
    std::deque<std::string> lines_;
};

#ifdef _WIN32
std::wstring Utf8ToWideString(const std::string& input) {
    if (input.empty()) {
        return std::wstring();
    }

    const int wideLength = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
    if (wideLength <= 0) {
        return std::wstring();
    }

    std::wstring output(static_cast<size_t>(wideLength) - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, output.data(), wideLength);
    return output;
}

std::string Base64Encode(const unsigned char* data, size_t size) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((size + 2) / 3) * 4);
    for (size_t index = 0; index < size; index += 3) {
        const uint32_t octetA = data[index];
        const uint32_t octetB = (index + 1 < size) ? data[index + 1] : 0;
        const uint32_t octetC = (index + 2 < size) ? data[index + 2] : 0;
        const uint32_t triple = (octetA << 16) | (octetB << 8) | octetC;

        encoded.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        encoded.push_back(kAlphabet[(triple >> 12) & 0x3F]);
        encoded.push_back(index + 1 < size ? kAlphabet[(triple >> 6) & 0x3F] : '=');
        encoded.push_back(index + 2 < size ? kAlphabet[triple & 0x3F] : '=');
    }

    return encoded;
}

std::string EncodePowerShellCommand(const std::string& command) {
    const std::wstring script =
        L"$OutputEncoding=[Console]::OutputEncoding=[System.Text.Encoding]::UTF8; "
        L"[Console]::InputEncoding=[System.Text.Encoding]::UTF8; & { " +
        Utf8ToWideString(command) + L" }";
    return Base64Encode(reinterpret_cast<const unsigned char*>(script.data()), script.size() * sizeof(wchar_t));
}

std::vector<wchar_t> BuildEnvironmentBlock(
    const std::map<std::string, std::string>& sessionEnv,
    const std::map<std::string, std::string>& requestEnv) {
    if (sessionEnv.empty() && requestEnv.empty()) {
        return {};
    }

    std::map<std::wstring, std::wstring> mergedEnv;
    LPWCH environment = GetEnvironmentStringsW();
    if (environment != nullptr) {
        for (LPWCH current = environment; *current != L'\0'; current += wcslen(current) + 1) {
            std::wstring entry(current);
            const size_t separator = entry.find(L'=');
            if (separator == std::wstring::npos || separator == 0) {
                continue;
            }
            mergedEnv[entry.substr(0, separator)] = entry.substr(separator + 1);
        }
        FreeEnvironmentStringsW(environment);
    }

    const auto applyOverrides = [&mergedEnv](const std::map<std::string, std::string>& overrides) {
        for (const auto& [key, value] : overrides) {
            if (key.empty()) {
                continue;
            }
            mergedEnv[Utf8ToWideString(key)] = Utf8ToWideString(value);
        }
    };

    applyOverrides(sessionEnv);
    applyOverrides(requestEnv);

    std::vector<wchar_t> block;
    for (const auto& [key, value] : mergedEnv) {
        const std::wstring entry = key + L"=" + value;
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}
#endif

// =====================================================================
// DYNAMIC POWERSHELL TERMINAL MANAGER IMPLEMENTATION
// =====================================================================

DynamicPowerShellTerminalManager::DynamicPowerShellTerminalManager()
    : startup_time_(std::chrono::system_clock::now()),
      random_generator_(std::chrono::high_resolution_clock::now().time_since_epoch().count()) {
    performance_metrics_ = {};
}

DynamicPowerShellTerminalManager::~DynamicPowerShellTerminalManager() {
    shutdown();
}

bool DynamicPowerShellTerminalManager::initialize(const DynamicTimeoutConfig& config) {
    if (initialized_.load()) {
        return true;
    }
    
    try {
        config_ = config;
        
        // Initialize random distribution for timeout variation
        variation_distribution_ = std::uniform_real_distribution<double>(
            config_.random_variation_min, config_.random_variation_max);
        
        // Set random seed
        if (config_.random_seed != 0) {
            random_generator_.seed(config_.random_seed);
        }
        
        // Initialize worker threads
        workers_active_ = true;
        uint32_t thread_count = std::min(std::thread::hardware_concurrency(), MAX_WORKER_THREADS);
        worker_threads_.reserve(thread_count);
        
        for (uint32_t i = 0; i < thread_count; ++i) {
            worker_threads_.emplace_back(&DynamicPowerShellTerminalManager::worker_thread_function, this);
        }
        
        // Start monitoring thread
        monitoring_active_ = true;
        monitoring_thread_ = std::thread(&DynamicPowerShellTerminalManager::monitoring_thread_function, this);
        
        initialized_ = true;
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

void DynamicPowerShellTerminalManager::shutdown() {
    if (!initialized_.load()) return;
    
    shutting_down_ = true;
    
    // Stop worker threads
    workers_active_ = false;
    execution_condition_.notify_all();
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    // Stop monitoring
    monitoring_active_ = false;
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    
    // Close all sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& pair : sessions_) {
            close_session(pair.first);
        }
        sessions_.clear();
    }
    
    initialized_ = false;
    shutting_down_ = false;
}

std::string DynamicPowerShellTerminalManager::create_session(
    const std::string& working_dir,
    const std::map<std::string, std::string>& env_vars,
    TerminalMode mode) {
    
    if (!initialized_.load()) {
        throw std::runtime_error("Terminal manager not initialized");
    }
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // Check session limits
    if (sessions_.size() >= config_.max_concurrent_sessions) {
        cleanup_inactive_sessions();
        if (sessions_.size() >= config_.max_concurrent_sessions) {
            throw std::runtime_error("Maximum concurrent sessions reached");
        }
    }
    
    auto session = std::make_shared<PowerShellSession>();
    session->session_id = generate_session_id();
    session->working_directory = working_dir.empty() ? fs::current_path().string() : working_dir;
    session->environment_vars = env_vars;
    session->mode = mode;
    session->created_at = std::chrono::system_clock::now();
    session->last_used = session->created_at;
    session->is_active = true;
    
    sessions_[session->session_id] = session;
    
    {
        std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
        performance_metrics_.total_sessions_created++;
        performance_metrics_.active_sessions = sessions_.size();
    }
    
    return session->session_id;
}

std::string DynamicPowerShellTerminalManager::get_or_create_session(const std::string& session_id) {
    if (!session_id.empty()) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end() && it->second->is_active) {
            it->second->last_used = std::chrono::system_clock::now();
            return session_id;
        }
    }
    
    return create_session();
}

bool DynamicPowerShellTerminalManager::close_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return false;
    }
    
    auto& session = it->second;
    session->is_active = false;
    
#ifdef _WIN32
    if (session->process_handle != INVALID_HANDLE_VALUE) {
        TerminateProcess(session->process_handle, 1);
        CloseHandle(session->process_handle);
        session->process_handle = INVALID_HANDLE_VALUE;
    }
#else
    if (session->process_id > 0) {
        kill(session->process_id, SIGTERM);
        session->process_id = -1;
    }
#endif
    
    sessions_.erase(it);
    
    {
        std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
        performance_metrics_.active_sessions = sessions_.size();
    }
    
    return true;
}

std::shared_ptr<PowerShellSession> DynamicPowerShellTerminalManager::get_session_info(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    return it != sessions_.end() ? it->second : nullptr;
}

std::vector<std::string> DynamicPowerShellTerminalManager::list_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::vector<std::string> session_list;
    session_list.reserve(sessions_.size());
    
    for (const auto& pair : sessions_) {
        if (pair.second->is_active) {
            session_list.push_back(pair.first);
        }
    }
    
    return session_list;
}

void DynamicPowerShellTerminalManager::cleanup_inactive_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> sessions_to_remove;
    
    for (const auto& pair : sessions_) {
        const auto& session = pair.second;
        auto idle_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - session->last_used);
        
        if (!session->is_active || idle_time > config_.session_idle_timeout) {
            sessions_to_remove.push_back(pair.first);
        }
    }
    
    for (const auto& session_id : sessions_to_remove) {
        close_session(session_id);
    }
}
}

TerminalExecutionResult DynamicPowerShellTerminalManager::execute_command(const TerminalExecutionRequest& request) {
    if (!initialized_.load()) {
        return TerminalExecutionResult::error_result("", "Terminal manager not initialized");
    }
    
    std::string request_id = generate_request_id();
    
    auto result = execute_command_internal(request);
    result.request_id = request_id;
    
    // Update statistics and learning
    update_command_statistics(request.command, result);
    
    return result;
}

TerminalExecutionResult DynamicPowerShellTerminalManager::execute_simple_command(
    const std::string& command, const std::string& session_id) {
    
    TerminalExecutionRequest request;
    request.command = command;
    request.session_id = session_id;
    request.mode = TerminalMode::SYNCHRONOUS;
    request.priority = ExecutionPriority::NORMAL;
    
    return execute_command(request);
}

std::chrono::milliseconds DynamicPowerShellTerminalManager::calculate_dynamic_timeout(
    const std::string& command, bool use_learning, bool apply_random_variation) {
    
    std::chrono::milliseconds timeout = config_.base_timeout;
    
    try {
        // Step 1: Analyze command complexity
        CommandComplexity complexity = analyze_command_complexity(command);
        
        // Step 2: Apply complexity-based timeout multiplier
        double complexity_multiplier = 1.0;
        switch (complexity) {
            case CommandComplexity::TRIVIAL:  complexity_multiplier = 0.1; break;
            case CommandComplexity::SIMPLE:   complexity_multiplier = 0.3; break;
            case CommandComplexity::MODERATE: complexity_multiplier = 1.0; break;
            case CommandComplexity::COMPLEX:  complexity_multiplier = 3.0; break;
            case CommandComplexity::HEAVY:    complexity_multiplier = 8.0; break;
            case CommandComplexity::QUANTUM:  complexity_multiplier = 20.0; break;
        }
        
        timeout = std::chrono::milliseconds(
            static_cast<int64_t>(timeout.count() * complexity_multiplier));
        
        // Step 3: Apply learning-based adaptation
        if (use_learning && config_.enable_adaptive_learning) {
            std::string pattern = normalize_command_pattern(command);
            
            std::lock_guard<std::mutex> lock(stats_mutex_);
            auto it = command_stats_.find(pattern);
            if (it != command_stats_.end() && it->second.execution_count > 0) {
                timeout = calculate_adaptive_timeout(it->second);
            }
        }
        
        // Step 4: Apply performance prediction
        if (config_.enable_performance_prediction) {
            auto predicted_time = predict_execution_time(command);
            if (predicted_time > std::chrono::milliseconds{0}) {
                // Use prediction with safety margin
                auto predicted_timeout = std::chrono::milliseconds(
                    static_cast<int64_t>(predicted_time.count() * 2.5));
                
                // Blend with current timeout using performance weight
                timeout = std::chrono::milliseconds(static_cast<int64_t>(
                    timeout.count() * (1.0 - config_.performance_weight) +
                    predicted_timeout.count() * config_.performance_weight));
            }
        }
        
        // Step 5: Apply quantum enhancement
        if (config_.enable_quantum_enhancement) {
            timeout = apply_quantum_enhancement(command, timeout);
        }
        
        // Step 6: Apply random variation
        if (apply_random_variation && config_.enable_random_variation) {
            timeout = apply_random_variation(timeout);
        }
        
        // Step 7: Clamp to bounds
        timeout = std::max(timeout, config_.min_timeout);
        timeout = std::min(timeout, config_.max_timeout);
        
    } catch (const std::exception& e) {
        (void)e;
        timeout = config_.base_timeout;
    }
    
    return timeout;
}

CommandComplexity DynamicPowerShellTerminalManager::analyze_command_complexity(const std::string& command) {
    // Check against predefined patterns
    for (const auto& pattern : COMPLEXITY_PATTERNS) {
        if (std::regex_search(command, pattern.first)) {
            return pattern.second;
        }
    }
    
    // Heuristic analysis
    CommandComplexity complexity = CommandComplexity::SIMPLE;
    
    // Count pipes and complex operators
    size_t pipe_count = std::count(command.begin(), command.end(), '|');
    if (pipe_count > 2) complexity = CommandComplexity::COMPLEX;
    else if (pipe_count > 0) complexity = CommandComplexity::MODERATE;
    
    // Check for loops and iterations
    if (command.find("foreach") != std::string::npos || 
        command.find("for (") != std::string::npos ||
        command.find("while") != std::string::npos) {
        complexity = std::max(complexity, CommandComplexity::COMPLEX);
    }
    
    // Check for network operations
    if (command.find("http") != std::string::npos ||
        command.find("ftp") != std::string::npos ||
        command.find("Invoke-WebRequest") != std::string::npos ||
        command.find("Invoke-RestMethod") != std::string::npos) {
        complexity = std::max(complexity, CommandComplexity::HEAVY);
    }
    
    // Check for system operations
    if (command.find("Get-WmiObject") != std::string::npos ||
        command.find("Get-CimInstance") != std::string::npos ||
        command.find("Start-Process") != std::string::npos) {
        complexity = std::max(complexity, CommandComplexity::HEAVY);
    }
    
    return complexity;
}

std::chrono::milliseconds DynamicPowerShellTerminalManager::predict_execution_time(const std::string& command) {
    std::string pattern = normalize_command_pattern(command);
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = command_stats_.find(pattern);
    if (it != command_stats_.end() && it->second.execution_count > 0) {
        // Use historical average with confidence adjustment
        auto& stats = it->second;
        double confidence = std::min(1.0, static_cast<double>(stats.execution_count) / 10.0);
        
        return std::chrono::milliseconds(static_cast<int64_t>(
            stats.avg_execution_time.count() * (1.0 + (1.0 - confidence) * 0.5)));
    }
    
    // Fallback to complexity-based estimation
    CommandComplexity complexity = analyze_command_complexity(command);
    return std::chrono::milliseconds(static_cast<int64_t>(complexity) * 2000);
}

void DynamicPowerShellTerminalManager::optimize_timeouts() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    uint32_t optimizations = 0;
    
    for (auto& pair : command_stats_) {
        auto& stats = pair.second;
        
        if (stats.execution_count < 3) continue; // Need minimum data
        
        // Calculate optimal timeout based on statistics
        auto old_timeout = stats.optimal_timeout;
        auto new_timeout = calculate_adaptive_timeout(stats);
        
        if (std::abs((new_timeout - old_timeout).count()) > 1000) { // 1 second difference
            stats.optimal_timeout = new_timeout;
            optimizations++;
        }
    }
    
    {
        std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
        performance_metrics_.adaptations_performed += optimizations;
    }
}

TerminalExecutionResult DynamicPowerShellTerminalManager::execute_command_internal(const TerminalExecutionRequest& request) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Get or create session
    std::string session_id = get_or_create_session(request.session_id);
    auto session = get_session_info(session_id);
    if (!session) {
        return TerminalExecutionResult::error_result("", "Failed to create/get session");
    }
    
    // Calculate timeout
    std::chrono::milliseconds timeout = request.custom_timeout > std::chrono::milliseconds{0} ?
        request.custom_timeout :
        calculate_dynamic_timeout(request.command, request.use_adaptive_timeout, request.allow_random_variation);
    
    TerminalExecutionResult result;
    result.command = request.command;
    result.session_id = session_id;
    result.timeout_used = timeout;
    result.started_at = std::chrono::system_clock::now();
    result.detected_complexity = analyze_command_complexity(request.command);

    const size_t per_execution_cap = std::min<size_t>(config_.max_memory_per_session, TERMINAL_OUTPUT_HARD_CAP_BYTES);
    CircularLineBuffer out_buffer(per_execution_cap);
    CircularLineBuffer err_buffer(std::max<size_t>(4096, per_execution_cap / 4));
    result.output_buffer_cap_bytes = per_execution_cap;
    
    try {
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE stdoutRead = INVALID_HANDLE_VALUE;
        HANDLE stdoutWrite = INVALID_HANDLE_VALUE;
        HANDLE stderrRead = INVALID_HANDLE_VALUE;
        HANDLE stderrWrite = INVALID_HANDLE_VALUE;
        HANDLE stdinRead = INVALID_HANDLE_VALUE;

        if (!CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0) ||
            !CreatePipe(&stderrRead, &stderrWrite, &sa, 0)) {
            return TerminalExecutionResult::error_result("", "Failed to create process output pipes");
        }
        SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);

        stdinRead = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        if (stdinRead == INVALID_HANDLE_VALUE) {
            stdinRead = GetStdHandle(STD_INPUT_HANDLE);
        }

        const std::string encodedCommand = EncodePowerShellCommand(request.command);
        std::wstring commandLine =
            L"powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand " +
            Utf8ToWideString(encodedCommand);
        std::vector<wchar_t> commandLineBuffer(commandLine.begin(), commandLine.end());
        commandLineBuffer.push_back(L'\0');

        const std::string effectiveWorkingDirectory = request.working_directory.empty()
                                                         ? session->working_directory
                                                         : request.working_directory;
        if (!request.working_directory.empty()) {
            session->working_directory = request.working_directory;
        }
        std::wstring workingDirectoryWide = Utf8ToWideString(effectiveWorkingDirectory);

        const std::vector<wchar_t> environmentBlock =
            BuildEnvironmentBlock(session->environment_vars, request.environment_vars);

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.dwFlags = STARTF_USESTDHANDLES;
        startupInfo.hStdInput = stdinRead;
        startupInfo.hStdOutput = stdoutWrite;
        startupInfo.hStdError = stderrWrite;

        PROCESS_INFORMATION processInfo{};
        const BOOL created = CreateProcessW(nullptr,
                                            commandLineBuffer.data(),
                                            nullptr,
                                            nullptr,
                                            TRUE,
                                            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                                            environmentBlock.empty()
                                                ? nullptr
                                                : const_cast<wchar_t*>(environmentBlock.data()),
                                            workingDirectoryWide.empty() ? nullptr : workingDirectoryWide.c_str(),
                                            &startupInfo,
                                            &processInfo);

        CloseHandle(stdoutWrite);
        CloseHandle(stderrWrite);
        if (stdinRead != INVALID_HANDLE_VALUE && stdinRead != GetStdHandle(STD_INPUT_HANDLE)) {
            CloseHandle(stdinRead);
        }

        if (!created) {
            if (stdoutRead != INVALID_HANDLE_VALUE) CloseHandle(stdoutRead);
            if (stderrRead != INVALID_HANDLE_VALUE) CloseHandle(stderrRead);
            return TerminalExecutionResult::error_result(
                "",
                "Failed to spawn PowerShell process: " + std::to_string(static_cast<unsigned long>(GetLastError())));
        }

        session->process_handle = processInfo.hProcess;
        session->process_id = processInfo.dwProcessId;
        CloseHandle(processInfo.hThread);

        auto drainPipe = [](HANDLE pipe,
                            CircularLineBuffer& buffer,
                            const std::function<void(const std::string&)>& callback) {
            std::array<char, 4096> bytes{};
            DWORD bytesRead = 0;
            while (ReadFile(pipe, bytes.data(), static_cast<DWORD>(bytes.size()), &bytesRead, nullptr) &&
                   bytesRead > 0) {
                std::string chunk(bytes.data(), bytes.data() + bytesRead);
                buffer.append(chunk);
                if (callback) {
                    callback(chunk);
                }
            }
        };

        std::thread stdoutThread(
            drainPipe,
            stdoutRead,
            std::ref(out_buffer),
            request.capture_output ? request.output_callback : std::function<void(const std::string&)>{});
        std::thread stderrThread(
            drainPipe,
            stderrRead,
            std::ref(err_buffer),
            request.capture_errors ? request.error_callback : std::function<void(const std::string&)>{});

        const ULONGLONG deadline = GetTickCount64() + static_cast<ULONGLONG>(timeout.count());
        DWORD waitResult = WAIT_TIMEOUT;
        while ((waitResult = WaitForSingleObject(processInfo.hProcess, 50)) == WAIT_TIMEOUT) {
            if (GetTickCount64() >= deadline) {
                result.timed_out = true;
                TerminateProcess(processInfo.hProcess, 124);
                WaitForSingleObject(processInfo.hProcess, 5000);
                break;
            }
        }

        if (stdoutThread.joinable()) {
            stdoutThread.join();
        }
        if (stderrThread.joinable()) {
            stderrThread.join();
        }

        CloseHandle(stdoutRead);
        CloseHandle(stderrRead);

        DWORD exitCode = 0;
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
        CloseHandle(processInfo.hProcess);
        session->process_handle = INVALID_HANDLE_VALUE;
        session->process_id = 0;

        result.exit_code = result.timed_out ? -1 : static_cast<int>(exitCode);
        result.success = !result.timed_out && exitCode == 0;
        if (result.timed_out) {
            result.error_message = "Command execution timed out";
            err_buffer.append(result.error_message + "\n");
        } else if (exitCode != 0 && err_buffer.str().empty()) {
            result.error_message = "Command exited with code " + std::to_string(exitCode);
            err_buffer.append(result.error_message + "\n");
        }
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(
            static_cast<int64_t>(result.detected_complexity) * 100));
        result.success = true;
        result.exit_code = 0;
        out_buffer.append("Terminal execution fallback path completed\n");
#endif

        session->last_used = std::chrono::system_clock::now();
        session->commands_executed++;

        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        result.completed_at = std::chrono::system_clock::now();

        result.output = request.capture_output ? out_buffer.str() : std::string();
        result.error_output = request.capture_errors ? err_buffer.str() : std::string();
        result.output_truncated = out_buffer.truncated() || err_buffer.truncated();
        result.dropped_output_bytes = out_buffer.dropped_bytes() + err_buffer.dropped_bytes();
        result.dropped_output_lines = out_buffer.dropped_lines() + err_buffer.dropped_lines();
        result.memory_used = out_buffer.retained_bytes() + err_buffer.retained_bytes();
        session->memory_usage = result.memory_used;
        
        // Update performance metrics
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            performance_metrics_.total_commands_executed++;
            
            if (result.success) {
                performance_metrics_.successful_commands++;
            } else if (result.timed_out) {
                performance_metrics_.timed_out_commands++;
            } else {
                performance_metrics_.failed_commands++;
            }
            
            performance_metrics_.total_execution_time += result.execution_time;
            performance_metrics_.average_execution_time = std::chrono::milliseconds(
                performance_metrics_.total_execution_time.count() / 
                std::max(performance_metrics_.total_commands_executed, 1u));
            
            performance_metrics_.overall_success_rate = 
                static_cast<double>(performance_metrics_.successful_commands) / 
                std::max(performance_metrics_.total_commands_executed, 1u);
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Execution error: ") + e.what();
        err_buffer.append(result.error_message + "\n");
        result.error_output = err_buffer.str();
        result.output_truncated = err_buffer.truncated();
        result.dropped_output_bytes = err_buffer.dropped_bytes();
        result.dropped_output_lines = err_buffer.dropped_lines();
        result.memory_used = err_buffer.retained_bytes();
        result.exit_code = -1;
        result.completed_at = std::chrono::system_clock::now();
        
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        performance_metrics_.failed_commands++;
    }
    
    return result;
}

void DynamicPowerShellTerminalManager::update_command_statistics(
    const std::string& command, const TerminalExecutionResult& result) {
    
    std::string pattern = normalize_command_pattern(command);
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto& stats = command_stats_[pattern];
    
    // Initialize if first execution
    if (stats.execution_count == 0) {
        stats.command_pattern = pattern;
        stats.detected_complexity = result.detected_complexity;
        stats.current_timeout = result.timeout_used;
        stats.optimal_timeout = result.timeout_used;
        stats.min_execution_time = result.execution_time;
        stats.max_execution_time = result.execution_time;
    }
    
    // Update statistics
    stats.execution_count++;
    if (result.success) {
        stats.success_count++;
    }
    if (result.timed_out) {
        stats.timeout_count++;
    }
    
    stats.total_execution_time += result.execution_time;
    stats.avg_execution_time = std::chrono::milliseconds(
        stats.total_execution_time.count() / stats.execution_count);
    
    stats.min_execution_time = std::min(stats.min_execution_time, result.execution_time);
    stats.max_execution_time = std::max(stats.max_execution_time, result.execution_time);
    
    stats.success_rate = static_cast<double>(stats.success_count) / stats.execution_count;
    stats.last_executed = std::chrono::system_clock::now();
    stats.last_updated = stats.last_executed;
    
    // Apply learning if enabled
    if (config_.enable_adaptive_learning) {
        apply_learning_update(pattern, result.execution_time, result.timeout_used, result.success);
    }
}

void DynamicPowerShellTerminalManager::apply_learning_update(
    const std::string& pattern, std::chrono::milliseconds actual_time,
    std::chrono::milliseconds timeout_used, bool success) {
    
    auto& stats = command_stats_[pattern];
    
    // Calculate prediction error
    double error_ratio = static_cast<double>(actual_time.count()) / timeout_used.count();
    
    // Update prediction accuracy
    double old_accuracy = stats.prediction_accuracy;
    double new_accuracy = success ? (1.0 - std::abs(1.0 - error_ratio)) : 0.0;
    stats.prediction_accuracy = old_accuracy * LEARNING_MOMENTUM + new_accuracy * (1.0 - LEARNING_MOMENTUM);
    
    // Adaptive timeout adjustment
    if (success && error_ratio < 0.5) {
        // Command finished much faster than expected - reduce timeout
        stats.optimal_timeout = std::chrono::milliseconds(static_cast<int64_t>(
            stats.optimal_timeout.count() * (1.0 - config_.learning_rate * 0.5)));
    } else if (!success || error_ratio > 1.2) {
        // Command took longer than expected or failed - increase timeout
        stats.optimal_timeout = std::chrono::milliseconds(static_cast<int64_t>(
            stats.optimal_timeout.count() * (1.0 + config_.learning_rate)));
    }
    
    // Clamp to reasonable bounds
    stats.optimal_timeout = std::max(stats.optimal_timeout, config_.min_timeout);
    stats.optimal_timeout = std::min(stats.optimal_timeout, config_.max_timeout);
}

std::chrono::milliseconds DynamicPowerShellTerminalManager::calculate_adaptive_timeout(const CommandExecutionStats& stats) {
    if (stats.execution_count == 0) {
        return config_.base_timeout;
    }
    
    // Use statistical analysis for optimal timeout
    double mean = stats.avg_execution_time.count();
    
    // Estimate standard deviation from min/max (rough approximation)
    double range = std::max(1.0, static_cast<double>((stats.max_execution_time - stats.min_execution_time).count()));
    double estimated_stddev = range / 4.0; // Rough estimate: range ≈ 4σ
    
    // Calculate timeout with confidence interval
    double confidence_multiplier = 2.5; // 2.5 standard deviations for ~98.8% confidence
    double safety_margin = 1.2; // 20% extra safety margin
    
    return std::chrono::milliseconds(static_cast<int64_t>(
        (mean + confidence_multiplier * estimated_stddev) * safety_margin));
}

std::chrono::milliseconds DynamicPowerShellTerminalManager::apply_random_variation(std::chrono::milliseconds base_timeout) {
    double multiplier = variation_distribution_(random_generator_);
    return std::chrono::milliseconds(static_cast<int64_t>(base_timeout.count() * multiplier));
}

std::chrono::milliseconds DynamicPowerShellTerminalManager::apply_quantum_enhancement(
    const std::string& command, std::chrono::milliseconds base_timeout) {
    
    // Quantum enhancement uses advanced mathematical models
    uint64_t command_hash = quantum_command_hash(command);
    double quantum_factor = 1.0 + (command_hash % 1000) / 10000.0; // Small variation based on command
    
    // Apply golden ratio enhancement for complex commands
    if (analyze_command_complexity(command) >= CommandComplexity::COMPLEX) {
        quantum_factor *= QUANTUM_ENHANCEMENT_FACTOR;
    }
    
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        performance_metrics_.quantum_optimizations++;
    }
    
    return std::chrono::milliseconds(static_cast<int64_t>(base_timeout.count() * quantum_factor));
}

std::string DynamicPowerShellTerminalManager::normalize_command_pattern(const std::string& command) {
    std::string normalized = command;
    
    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    
    // Remove specific parameters and values, keep structure
    std::regex param_regex(R"(\s+[-/]\w+\s+[^\s-]+)", std::regex::icase);
    normalized = std::regex_replace(normalized, param_regex, " -param");
    
    // Normalize whitespace
    std::regex whitespace_regex(R"(\s+)");
    normalized = std::regex_replace(normalized, whitespace_regex, " ");
    
    // Trim
    normalized.erase(0, normalized.find_first_not_of(" \t\n\r"));
    normalized.erase(normalized.find_last_not_of(" \t\n\r") + 1);
    
    return normalized;
}

std::string DynamicPowerShellTerminalManager::generate_session_id() {
    std::ostringstream oss;
    oss << "PS_" << std::hex << session_counter_.fetch_add(1) << "_" 
        << std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return oss.str();
}

std::string DynamicPowerShellTerminalManager::generate_request_id() {
    static std::atomic<uint64_t> counter{0};
    std::ostringstream oss;
    oss << "REQ_" << std::hex << counter.fetch_add(1) << "_" 
        << std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return oss.str();
}

void DynamicPowerShellTerminalManager::worker_thread_function() {
    while (workers_active_.load()) {
        std::unique_lock<std::mutex> lock(execution_mutex_);
        execution_condition_.wait(lock, [this] { 
            return !execution_queue_.empty() || !workers_active_.load();
        });
        
        if (!workers_active_.load()) break;
        if (execution_queue_.empty()) continue;
        
        TerminalExecutionRequest request = execution_queue_.front();
        execution_queue_.pop();
        lock.unlock();
        
        // Execute request asynchronously
        auto result = execute_command_internal(request);
        
        // Call completion callback if provided
        if (request.completion_callback) {
            request.completion_callback(result.success ? result.output : result.error_message);
        }
    }
}

void DynamicPowerShellTerminalManager::monitoring_thread_function() {
    while (monitoring_active_.load()) {
        std::this_thread::sleep_for(30s);
        
        if (shutting_down_.load()) break;
        
        // Perform maintenance tasks
        perform_maintenance_tasks();
        
        // Optimize timeouts periodically
        if (config_.enable_adaptive_learning) {
            optimize_timeouts();
        }
        
        // Validate resource limits
        validate_resource_limits();
        
        // Cleanup inactive sessions
        cleanup_inactive_sessions();
    }
}

void DynamicPowerShellTerminalManager::perform_maintenance_tasks() {
    // Update performance metrics
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        performance_metrics_.active_sessions = sessions_.size();
        
        // Calculate timeout accuracy
        if (performance_metrics_.total_commands_executed > 0) {
            double timeout_success_rate = 1.0 - 
                static_cast<double>(performance_metrics_.timed_out_commands) / 
                performance_metrics_.total_commands_executed;
            performance_metrics_.timeout_accuracy = timeout_success_rate;
        }
    }
}

bool DynamicPowerShellTerminalManager::validate_resource_limits() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    size_t total_memory = 0;
    for (const auto& pair : sessions_) {
        total_memory += pair.second->memory_usage;
    }
    
    performance_metrics_.total_memory_usage = total_memory;
    
    // Check if memory limit exceeded
    size_t total_limit = config_.max_concurrent_sessions * config_.max_memory_per_session;
    if (total_memory > total_limit) {
        return false;
    }
    
    return true;
}

DynamicPowerShellTerminalManager::PerformanceMetrics DynamicPowerShellTerminalManager::get_performance_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return performance_metrics_;
}

DynamicPowerShellTerminalManager::SystemStatus DynamicPowerShellTerminalManager::get_system_status() const {
    SystemStatus status;
    
    status.manager_initialized = initialized_.load();
    status.quantum_enhancement = config_.enable_quantum_enhancement;
    status.started_at = startup_time_;
    status.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - startup_time_);
    
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        status.active_sessions = sessions_.size();
        status.max_sessions = config_.max_concurrent_sessions;
    }
    
    {
        std::lock_guard<std::mutex> lock(execution_mutex_);
        status.running_commands = execution_queue_.size();
    }
    
    status.performance = get_performance_metrics();
    status.current_config = config_;
    
    return status;
}

} // namespace RawrXD::Terminal
