// RawrXD Executor - Pure Win32/C++ Implementation
// Replaces: agentic_executor.cpp and all Qt-dependent task execution
// Zero Qt dependencies - just Win32 API + STL

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <sstream>

// ============================================================================
// EXECUTION REQUEST/RESPONSE STRUCTURES
// ============================================================================

enum class TaskStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED
};

struct ExecutionRequest {
    int request_id;
    std::wstring description;
    std::wstring code;
    std::wstring command;
    std::wstring working_directory;
    int priority;
    std::chrono::steady_clock::time_point created_at;
    bool is_training;
};

struct ExecutionResult {
    int request_id;
    TaskStatus status;
    std::wstring output;
    std::wstring error_message;
    int exit_code;
    std::chrono::milliseconds duration;
    int tokens_generated;
    bool success;
};

struct TrainingProgress {
    int epoch;
    int total_epochs;
    float loss;
    float accuracy;
    float learning_rate;
    std::chrono::steady_clock::time_point timestamp;
};

// ============================================================================
// EVENT SYSTEM
// ============================================================================

template<typename... Args>
class EventSystem {
private:
    std::vector<std::function<void(Args...)>> m_handlers;
    std::mutex m_mutex;
    
public:
    void Connect(std::function<void(Args...)> handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers.push_back(handler);
    }
    
    void Emit(Args... args) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& handler : m_handlers) {
            try {
                handler(args...);
            } catch (...) {
                OutputDebugStringW(L"[Executor] Handler exception\n");
            }
        }
    }
    
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers.clear();
    }
};

// ============================================================================
// EXECUTOR ENGINE
// ============================================================================

class ExecutorEngine {
private:
    // State
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_running;
    std::wstring m_workspaceRoot;
    std::wstring m_currentWorkingDirectory;
    
    // Request queue
    std::queue<ExecutionRequest> m_requestQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCV;
    
    // Results cache
    std::map<int, ExecutionResult> m_results;
    std::mutex m_resultsMutex;
    
    // Worker threads
    std::thread m_executionThread;
    std::thread m_trainingThread;
    
    // Statistics
    std::atomic<int> m_nextRequestId;
    std::atomic<int> m_completedTasks;
    std::atomic<int> m_failedTasks;
    std::atomic<int> m_totalTokensGenerated;
    std::chrono::steady_clock::time_point m_sessionStart;
    
    // Training state
    TrainingProgress m_currentTrainingProgress;
    std::atomic<bool> m_trainingActive;
    
    // Memory management
    std::vector<std::wstring> m_memorySpace;
    std::map<std::wstring, std::wstring> m_contextVariables;
    
public:
    // Events
    EventSystem<int, std::wstring> ProgressUpdated;      // request_id, message
    EventSystem<int, ExecutionResult> TaskCompleted;     // request_id, result
    EventSystem<int, std::wstring> TaskFailed;           // request_id, error
    EventSystem<TrainingProgress> TrainingProgress;      // progress
    EventSystem<std::wstring> TrainingCompleted;         // message
    EventSystem<std::wstring> LogMessage;                // message
    
    ExecutorEngine()
        : m_initialized(false),
          m_running(false),
          m_nextRequestId(1),
          m_completedTasks(0),
          m_failedTasks(0),
          m_totalTokensGenerated(0),
          m_sessionStart(std::chrono::steady_clock::now()),
          m_trainingActive(false) {
        
        InitializePaths();
        LogInfo(L"Executor Engine initialized");
    }
    
    ~ExecutorEngine() {
        Shutdown();
    }
    
    // ========================================================================
    // INITIALIZATION & LIFECYCLE
    // ========================================================================
    
    bool Initialize() {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        if (m_initialized.load()) {
            LogWarning(L"Executor already initialized");
            return true;
        }
        
        m_running = true;
        m_executionThread = std::thread(&ExecutorEngine::ExecutionWorker, this);
        m_trainingThread = std::thread(&ExecutorEngine::TrainingWorker, this);
        
        m_initialized = true;
        LogInfo(L"Executor Engine initialization complete");
        
        return true;
    }
    
    void Shutdown() {
        if (!m_running.load()) return;
        
        LogInfo(L"Shutting down Executor Engine");
        
        m_running = false;
        m_queueCV.notify_all();
        
        if (m_executionThread.joinable()) {
            m_executionThread.join();
        }
        if (m_trainingThread.joinable()) {
            m_trainingThread.join();
        }
        
        ProgressUpdated.Clear();
        TaskCompleted.Clear();
        TaskFailed.Clear();
        TrainingProgress.Clear();
        TrainingCompleted.Clear();
        LogMessage.Clear();
        
        m_initialized = false;
    }
    
    // ========================================================================
    // TASK EXECUTION
    // ========================================================================
    
    int SubmitRequest(const ExecutionRequest& request) {
        if (!m_initialized.load()) {
            LogError(L"Executor not initialized");
            return -1;
        }
        
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            
            ExecutionRequest req = request;
            req.request_id = m_nextRequestId++;
            req.created_at = std::chrono::steady_clock::now();
            
            m_requestQueue.push(req);
            
            LogInfo(L"Request " + std::to_wstring(req.request_id) + L" submitted: " + req.description);
        }
        
        m_queueCV.notify_one();
        return request.request_id;
    }
    
    ExecutionResult ExecuteCommand(const std::wstring& command, const std::wstring& workDir = L"") {
        auto startTime = std::chrono::steady_clock::now();
        ExecutionResult result;
        result.success = false;
        result.exit_code = -1;
        result.tokens_generated = 0;
        
        LogInfo(L"Executing command: " + command);
        
        // Create process
        STARTUPINFOW si = {};
        PROCESS_INFORMATION pi = {};
        
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        
        // Create pipes for stdout/stderr
        HANDLE hReadPipe, hWritePipe;
        if (!CreatePipe(&hReadPipe, &hWritePipe, NULL, 0)) {
            result.error_message = L"Failed to create pipe";
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime);
            return result;
        }
        
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        
        const wchar_t* workingDir = workDir.empty() ? m_currentWorkingDirectory.c_str() : workDir.c_str();
        
        if (!CreateProcessW(NULL, (LPWSTR)command.c_str(), NULL, NULL, TRUE,
                           0, NULL, workingDir, &si, &pi)) {
            result.error_message = L"Failed to create process";
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime);
            return result;
        }
        
        CloseHandle(hWritePipe);
        
        // Read output
        std::wstring output;
        std::vector<char> buffer(4096);
        DWORD bytesRead;
        
        while (ReadFile(hReadPipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, NULL) && bytesRead > 0) {
            int wideSize = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, NULL, 0);
            std::vector<wchar_t> wideBuffer(wideSize);
            MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, wideBuffer.data(), wideSize);
            output.append(wideBuffer.data(), wideSize);
        }
        
        CloseHandle(hReadPipe);
        
        // Wait for process
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        result.success = (exitCode == 0);
        result.exit_code = static_cast<int>(exitCode);
        result.output = output;
        
        auto endTime = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        if (result.success) {
            m_completedTasks++;
            LogInfo(L"Command completed successfully in " + std::to_wstring(result.duration.count()) + L" ms");
        } else {
            m_failedTasks++;
            result.error_message = L"Command failed with exit code " + std::to_wstring(exitCode);
        }
        
        return result;
    }
    
    // ========================================================================
    // CODE EXECUTION
    // ========================================================================
    
    ExecutionResult ExecuteCode(const std::wstring& code, const std::wstring& language = L"cpp") {
        auto startTime = std::chrono::steady_clock::now();
        ExecutionResult result;
        result.success = false;
        
        // Create temporary file
        wchar_t tempPath[MAX_PATH];
        wchar_t tempFileName[MAX_PATH];
        
        GetTempPathW(MAX_PATH, tempPath);
        
        std::wstring extension = (language == L"cpp") ? L".cpp" : (language == L"py") ? L".py" : L".txt";
        
        if (!GetTempFileNameW(tempPath, L"RXD", 0, tempFileName)) {
            result.error_message = L"Failed to create temp file";
            return result;
        }
        
        // Write code to file
        HANDLE hFile = CreateFileW(tempFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            result.error_message = L"Failed to create code file";
            return result;
        }
        
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, code.c_str(), -1, NULL, 0, NULL, NULL);
        std::vector<char> utf8Buffer(utf8Size);
        WideCharToMultiByte(CP_UTF8, 0, code.c_str(), -1, utf8Buffer.data(), utf8Size, NULL, NULL);
        
        DWORD bytesWritten;
        WriteFile(hFile, utf8Buffer.data(), utf8Size - 1, &bytesWritten, NULL);
        CloseHandle(hFile);
        
        // Execute based on language
        std::wstring command;
        if (language == L"cpp") {
            command = L"cl.exe " + std::wstring(tempFileName) + L" /Fe:" + std::wstring(tempFileName) + L".exe";
        } else if (language == L"py") {
            command = L"python.exe " + std::wstring(tempFileName);
        }
        
        if (!command.empty()) {
            result = ExecuteCommand(command);
        } else {
            result.error_message = L"Unsupported language: " + language;
        }
        
        // Cleanup
        DeleteFileW(tempFileName);
        
        auto endTime = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        return result;
    }
    
    // ========================================================================
    // TRAINING SYSTEM
    // ========================================================================
    
    int StartTraining(int epochs, float learning_rate) {
        if (m_trainingActive.exchange(true)) {
            LogWarning(L"Training already in progress");
            return -1;
        }
        
        int trainingId = m_nextRequestId++;
        
        LogInfo(L"Starting training with " + std::to_wstring(epochs) + L" epochs");
        
        // Initialize training progress
        m_currentTrainingProgress.epoch = 0;
        m_currentTrainingProgress.total_epochs = epochs;
        m_currentTrainingProgress.learning_rate = learning_rate;
        m_currentTrainingProgress.loss = 1.0f;
        m_currentTrainingProgress.accuracy = 0.0f;
        m_currentTrainingProgress.timestamp = std::chrono::steady_clock::now();
        
        m_queueCV.notify_one();
        
        return trainingId;
    }
    
    void CancelTraining() {
        if (m_trainingActive.load()) {
            m_trainingActive = false;
            LogInfo(L"Training cancelled");
        }
    }
    
    TrainingProgress GetTrainingProgress() const {
        return m_currentTrainingProgress;
    }
    
    // ========================================================================
    // MEMORY MANAGEMENT
    // ========================================================================
    
    void AddMemory(const std::wstring& entry) {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        m_memorySpace.push_back(entry);
        
        // Keep only last 10000 entries
        if (m_memorySpace.size() > 10000) {
            m_memorySpace.erase(m_memorySpace.begin());
        }
    }
    
    std::vector<std::wstring> SearchMemory(const std::wstring& query, size_t maxResults = 10) const {
        std::vector<std::wstring> results;
        
        size_t count = 0;
        for (const auto& entry : m_memorySpace) {
            if (entry.find(query) != std::wstring::npos) {
                results.push_back(entry);
                if (++count >= maxResults) break;
            }
        }
        
        return results;
    }
    
    void ClearMemory() {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_memorySpace.clear();
        m_contextVariables.clear();
        LogInfo(L"Memory cleared");
    }
    
    void SetContextVariable(const std::wstring& key, const std::wstring& value) {
        m_contextVariables[key] = value;
    }
    
    std::wstring GetContextVariable(const std::wstring& key) const {
        auto it = m_contextVariables.find(key);
        return (it != m_contextVariables.end()) ? it->second : L"";
    }
    
    // ========================================================================
    // STATUS & STATISTICS
    // ========================================================================
    
    struct Statistics {
        int completed_tasks;
        int failed_tasks;
        int pending_tasks;
        int total_tokens_generated;
        std::chrono::milliseconds uptime;
        size_t memory_entries;
        bool training_active;
    };
    
    Statistics GetStatistics() const {
        Statistics stats;
        stats.completed_tasks = m_completedTasks.load();
        stats.failed_tasks = m_failedTasks.load();
        stats.total_tokens_generated = m_totalTokensGenerated.load();
        stats.training_active = m_trainingActive.load();
        
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            stats.pending_tasks = static_cast<int>(m_requestQueue.size());
            stats.memory_entries = m_memorySpace.size();
        }
        
        auto now = std::chrono::steady_clock::now();
        stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_sessionStart);
        
        return stats;
    }
    
    std::wstring GetStatusReport() const {
        auto stats = GetStatistics();
        
        std::wstring report = L"Executor Engine Status:\n";
        report += L"  Initialized: " + std::wstring(m_initialized.load() ? L"Yes" : L"No") + L"\n";
        report += L"  Running: " + std::wstring(m_running.load() ? L"Yes" : L"No") + L"\n";
        report += L"  Completed Tasks: " + std::to_wstring(stats.completed_tasks) + L"\n";
        report += L"  Failed Tasks: " + std::to_wstring(stats.failed_tasks) + L"\n";
        report += L"  Pending Tasks: " + std::to_wstring(stats.pending_tasks) + L"\n";
        report += L"  Total Tokens: " + std::to_wstring(stats.total_tokens_generated) + L"\n";
        report += L"  Memory Entries: " + std::to_wstring(stats.memory_entries) + L"\n";
        report += L"  Training Active: " + std::wstring(stats.training_active ? L"Yes" : L"No") + L"\n";
        report += L"  Uptime: " + std::to_wstring(stats.uptime.count()) + L" ms\n";
        
        return report;
    }
    
private:
    // ========================================================================
    // WORKER THREADS
    // ========================================================================
    
    void ExecutionWorker() {
        while (m_running.load()) {
            ExecutionRequest request;
            bool hasRequest = false;
            
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_queueCV.wait(lock, [this] { return !m_requestQueue.empty() || !m_running.load(); });
                
                if (!m_running.load()) break;
                
                if (!m_requestQueue.empty()) {
                    request = m_requestQueue.front();
                    m_requestQueue.pop();
                    hasRequest = true;
                }
            }
            
            if (hasRequest) {
                ExecutionResult result;
                result.request_id = request.request_id;
                result.status = TaskStatus::RUNNING;
                
                if (!request.command.empty()) {
                    result = ExecuteCommand(request.command, request.working_directory);
                } else if (!request.code.empty()) {
                    result = ExecuteCode(request.code, L"cpp");
                } else {
                    result.success = false;
                    result.error_message = L"No command or code provided";
                }
                
                result.status = result.success ? TaskStatus::COMPLETED : TaskStatus::FAILED;
                
                {
                    std::lock_guard<std::mutex> lock(m_resultsMutex);
                    m_results[request.request_id] = result;
                }
                
                if (result.success) {
                    TaskCompleted.Emit(request.request_id, result);
                } else {
                    TaskFailed.Emit(request.request_id, result.error_message);
                }
            }
        }
    }
    
    void TrainingWorker() {
        while (m_running.load()) {
            if (m_trainingActive.load()) {
                // Simulate training loop
                int epochs = m_currentTrainingProgress.total_epochs;
                
                for (int epoch = 0; epoch < epochs && m_trainingActive.load(); ++epoch) {
                    m_currentTrainingProgress.epoch = epoch + 1;
                    
                    // Simulate loss decrease
                    m_currentTrainingProgress.loss = 1.0f / (1.0f + (epoch + 1));
                    m_currentTrainingProgress.accuracy = (epoch + 1) / (float)epochs;
                    m_currentTrainingProgress.timestamp = std::chrono::steady_clock::now();
                    
                    TrainingProgress.Emit(m_currentTrainingProgress);
                    
                    // Simulate epoch time
                    Sleep(100);
                }
                
                if (m_trainingActive.load()) {
                    m_trainingActive = false;
                    TrainingCompleted.Emit(L"Training completed successfully");
                }
            } else {
                Sleep(100);
            }
        }
    }
    
    void InitializePaths() {
        wchar_t buffer[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, buffer);
        m_workspaceRoot = buffer;
        m_currentWorkingDirectory = buffer;
    }
    
    void LogInfo(const std::wstring& message) {
        OutputDebugStringW((L"[Executor] INFO: " + message + L"\n").c_str());
        LogMessage.Emit(message);
    }
    
    void LogWarning(const std::wstring& message) {
        OutputDebugStringW((L"[Executor] WARNING: " + message + L"\n").c_str());
    }
    
    void LogError(const std::wstring& message) {
        OutputDebugStringW((L"[Executor] ERROR: " + message + L"\n").c_str());
        LogMessage.Emit(L"ERROR: " + message);
    }
};

// ============================================================================
// C INTERFACE FOR DLL EXPORT
// ============================================================================

extern "C" {

__declspec(dllexport) ExecutorEngine* CreateExecutor() {
    return new ExecutorEngine();
}

__declspec(dllexport) void DestroyExecutor(ExecutorEngine* engine) {
    delete engine;
}

__declspec(dllexport) BOOL Executor_Initialize(ExecutorEngine* engine) {
    return engine ? (engine->Initialize() ? TRUE : FALSE) : FALSE;
}

__declspec(dllexport) void Executor_Shutdown(ExecutorEngine* engine) {
    if (engine) engine->Shutdown();
}

__declspec(dllexport) int Executor_SubmitRequest(ExecutorEngine* engine,
    const wchar_t* description, const wchar_t* command, int priority) {
    if (!engine || !description || !command) return -1;
    
    ExecutionRequest req;
    req.description = description;
    req.command = command;
    req.priority = priority;
    req.is_training = FALSE;
    
    return engine->SubmitRequest(req);
}

__declspec(dllexport) int Executor_StartTraining(ExecutorEngine* engine, int epochs, float learning_rate) {
    return engine ? engine->StartTraining(epochs, learning_rate) : -1;
}

__declspec(dllexport) void Executor_CancelTraining(ExecutorEngine* engine) {
    if (engine) engine->CancelTraining();
}

__declspec(dllexport) const wchar_t* Executor_GetStatus(ExecutorEngine* engine) {
    if (!engine) return L"Engine not available";
    
    static std::wstring status;
    status = engine->GetStatusReport();
    return status.c_str();
}

} // extern "C"

// ============================================================================
// DLL ENTRY POINT
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringW(L"[RawrXD_Executor] DLL loaded\n");
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringW(L"[RawrXD_Executor] DLL unloaded\n");
        break;
    }
    return TRUE;
}