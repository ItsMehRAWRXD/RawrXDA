// RawrXD Agentic Engine - Pure Win32/C++ Implementation
// Replaces: agentic_engine.cpp and all Qt-dependent agentic components
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
#include <thread>
#include <mutex>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <algorithm>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

// ============================================================================
// STRUCTURES AND TYPES
// ============================================================================

// Model metadata
struct ModelInfo {
    std::wstring name;
    std::wstring path;
    std::wstring architecture;
    uint64_t parameters;
    uint32_t context_length;
    bool loaded;
    HANDLE hFile;
    HANDLE hMapping;
    void* pData;
    uint64_t fileSize;
};

// Inference request/response
struct InferenceRequest {
    std::wstring prompt;
    std::wstring context;
    float temperature;
    float top_p;
    int max_tokens;
    int request_id;
    std::chrono::steady_clock::time_point start_time;
};

struct InferenceResponse {
    std::wstring text;
    int request_id;
    bool success;
    std::wstring error;
    int tokens_generated;
    std::chrono::milliseconds duration;
};

// Callback type for streaming responses
typedef std::function<void(const std::wstring& token)> StreamCallback;
typedef std::function<void(const InferenceResponse& response)> CompletionCallback;

// Agent task
struct AgentTask {
    std::wstring description;
    std::wstring context;
    std::map<std::wstring, std::wstring> parameters;
    int priority;
    bool completed;
    std::wstring result;
};

// Memory entry
struct MemoryEntry {
    std::wstring content;
    std::wstring type;  // "conversation", "file", "task", "error"
    std::chrono::system_clock::time_point timestamp;
    float relevance_score;
    std::map<std::wstring, std::wstring> metadata;
};

// File operation record
struct FileOperation {
    std::wstring operation;  // "read", "write", "create", "delete", "move"
    std::wstring path;
    std::wstring content;
    bool success;
    std::chrono::system_clock::time_point timestamp;
    std::wstring error;
};

// ============================================================================
// AGENTIC ENGINE CLASS
// ============================================================================

class AgenticEngine {
private:
    // Core state
    std::unique_ptr<ModelInfo> m_model;
    std::vector<MemoryEntry> m_memory;
    std::vector<AgentTask> m_taskQueue;
    std::vector<FileOperation> m_fileHistory;
    
    // Threading
    std::thread m_inferenceThread;
    std::thread m_memoryThread;
    std::mutex m_requestMutex;
    std::condition_variable m_requestCV;
    std::queue<InferenceRequest> m_requestQueue;
    std::atomic<bool> m_running;
    std::atomic<int> m_nextRequestId;
    
    // Callbacks
    StreamCallback m_streamCallback;
    CompletionCallback m_completionCallback;
    
    // Configuration
    std::wstring m_workspaceRoot;
    std::wstring m_instructionFile;
    std::wstring m_instructions;
    bool m_autoSaveMemory;
    size_t m_maxMemoryEntries;
    
    // Statistics
    std::atomic<int> m_totalRequests;
    std::atomic<int> m_successfulRequests;
    std::atomic<int> m_totalTokens;
    std::chrono::steady_clock::time_point m_startTime;

public:
    AgenticEngine() : 
        m_running(false),
        m_nextRequestId(1),
        m_autoSaveMemory(true),
        m_maxMemoryEntries(10000),
        m_totalRequests(0),
        m_successfulRequests(0),
        m_totalTokens(0),
        m_startTime(std::chrono::steady_clock::now()) {
    }
    
    ~AgenticEngine() {
        Stop();
    }
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    bool Initialize(const std::wstring& workspaceRoot = L"") {
        m_workspaceRoot = workspaceRoot.empty() ? GetCurrentDir() : workspaceRoot;
        m_running = true;
        
        // Start worker threads
        m_inferenceThread = std::thread(&AgenticEngine::InferenceWorker, this);
        m_memoryThread = std::thread(&AgenticEngine::MemoryWorker, this);
        
        // Load default instructions
        LoadInstructions(L"");
        
        // Load persistent memory
        LoadMemoryFromDisk();
        
        return true;
    }
    
    void Stop() {
        m_running = false;
        m_requestCV.notify_all();
        
        if (m_inferenceThread.joinable()) {
            m_inferenceThread.join();
        }
        if (m_memoryThread.joinable()) {
            m_memoryThread.join();
        }
        
        if (m_autoSaveMemory) {
            SaveMemoryToDisk();
        }
        
        UnloadModel();
    }
    
    // ========================================================================
    // MODEL MANAGEMENT
    // ========================================================================
    
    bool LoadModel(const std::wstring& path) {
        if (m_model) {
            UnloadModel();
        }
        
        m_model = std::make_unique<ModelInfo>();
        m_model->path = path;
        
        // Extract name from path
        size_t lastSlash = path.find_last_of(L"\\/");
        m_model->name = (lastSlash != std::wstring::npos) ? 
                        path.substr(lastSlash + 1) : path;
        
        // Open and memory-map the file
        m_model->hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (m_model->hFile == INVALID_HANDLE_VALUE) {
            m_model.reset();
            return false;
        }
        
        LARGE_INTEGER fileSize;
        GetFileSizeEx(m_model->hFile, &fileSize);
        m_model->fileSize = fileSize.QuadPart;
        
        m_model->hMapping = CreateFileMappingW(m_model->hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!m_model->hMapping) {
            CloseHandle(m_model->hFile);
            m_model.reset();
            return false;
        }
        
        m_model->pData = MapViewOfFile(m_model->hMapping, FILE_MAP_READ, 0, 0, 0);
        if (!m_model->pData) {
            CloseHandle(m_model->hMapping);
            CloseHandle(m_model->hFile);
            m_model.reset();
            return false;
        }
        
        // Basic GGUF validation
        if (m_model->fileSize < 16) {
            UnloadModel();
            return false;
        }
        
        const uint32_t* magic = (const uint32_t*)m_model->pData;
        if (*magic != 0x46554747) { // "GGUF"
            UnloadModel();
            return false;
        }
        
        m_model->loaded = true;
        
        // Add to memory
        MemoryEntry entry;
        entry.type = L"system";
        entry.content = L"Model loaded: " + m_model->name;
        entry.timestamp = std::chrono::system_clock::now();
        entry.relevance_score = 1.0f;
        AddMemoryEntry(entry);
        
        return true;
    }
    
    void UnloadModel() {
        if (!m_model) return;
        
        if (m_model->pData) UnmapViewOfFile(m_model->pData);
        if (m_model->hMapping) CloseHandle(m_model->hMapping);
        if (m_model->hFile != INVALID_HANDLE_VALUE) CloseHandle(m_model->hFile);
        
        m_model.reset();
    }
    
    bool IsModelLoaded() const {
        return m_model && m_model->loaded;
    }
    
    std::wstring GetModelName() const {
        return m_model ? m_model->name : L"";
    }
    
    // ========================================================================
    // INFERENCE
    // ========================================================================
    
    int SubmitRequest(const std::wstring& prompt, const std::wstring& context = L"",
                     float temperature = 0.8f, float top_p = 0.95f, int max_tokens = 512) {
        if (!IsModelLoaded()) return -1;
        
        InferenceRequest request;
        request.prompt = prompt;
        request.context = context;
        request.temperature = temperature;
        request.top_p = top_p;
        request.max_tokens = max_tokens;
        request.request_id = m_nextRequestId++;
        request.start_time = std::chrono::steady_clock::now();
        
        {
            std::lock_guard<std::mutex> lock(m_requestMutex);
            m_requestQueue.push(request);
        }
        m_requestCV.notify_one();
        
        m_totalRequests++;
        return request.request_id;
    }
    
    void SetStreamCallback(StreamCallback callback) {
        m_streamCallback = callback;
    }
    
    void SetCompletionCallback(CompletionCallback callback) {
        m_completionCallback = callback;
    }
    
    // ========================================================================
    // AGENT TASKS
    // ========================================================================
    
    void AddTask(const std::wstring& description, const std::wstring& context = L"", int priority = 0) {
        AgentTask task;
        task.description = description;
        task.context = context;
        task.priority = priority;
        task.completed = false;
        
        m_taskQueue.push_back(task);
        
        // Sort by priority (higher first)
        std::sort(m_taskQueue.begin(), m_taskQueue.end(),
                 [](const AgentTask& a, const AgentTask& b) {
                     return a.priority > b.priority;
                 });
    }
    
    std::vector<AgentTask> GetPendingTasks() const {
        std::vector<AgentTask> pending;
        for (const auto& task : m_taskQueue) {
            if (!task.completed) {
                pending.push_back(task);
            }
        }
        return pending;
    }
    
    void CompleteTask(size_t index, const std::wstring& result) {
        if (index < m_taskQueue.size()) {
            m_taskQueue[index].completed = true;
            m_taskQueue[index].result = result;
            
            // Add to memory
            MemoryEntry entry;
            entry.type = L"task";
            entry.content = L"Completed: " + m_taskQueue[index].description + L" → " + result;
            entry.timestamp = std::chrono::system_clock::now();
            entry.relevance_score = 0.8f;
            AddMemoryEntry(entry);
        }
    }
    
    // ========================================================================
    // MEMORY MANAGEMENT
    // ========================================================================
    
    void AddMemoryEntry(const MemoryEntry& entry) {
        m_memory.push_back(entry);
        
        // Prune if too many entries
        if (m_memory.size() > m_maxMemoryEntries) {
            // Remove oldest low-relevance entries
            std::sort(m_memory.begin(), m_memory.end(),
                     [](const MemoryEntry& a, const MemoryEntry& b) {
                         return a.relevance_score > b.relevance_score;
                     });
            m_memory.resize(m_maxMemoryEntries * 0.8); // Keep 80%
        }
    }
    
    std::vector<MemoryEntry> SearchMemory(const std::wstring& query, size_t maxResults = 10) const {
        std::vector<std::pair<MemoryEntry, float>> scored;
        
        // Simple keyword matching (replace with semantic search later)
        std::wstring lowerQuery = ToLower(query);
        
        for (const auto& entry : m_memory) {
            std::wstring lowerContent = ToLower(entry.content);
            float score = 0.0f;
            
            // Basic relevance scoring
            if (lowerContent.find(lowerQuery) != std::wstring::npos) {
                score += 1.0f;
            }
            
            // Boost recent entries
            auto now = std::chrono::system_clock::now();
            auto hours = std::chrono::duration_cast<std::chrono::hours>(now - entry.timestamp).count();
            if (hours < 24) score += 0.5f;
            else if (hours < 168) score += 0.2f; // 1 week
            
            // Use entry's intrinsic relevance
            score *= entry.relevance_score;
            
            if (score > 0.1f) {
                scored.push_back({entry, score});
            }
        }
        
        // Sort by score
        std::sort(scored.begin(), scored.end(),
                 [](const auto& a, const auto& b) {
                     return a.second > b.second;
                 });
        
        std::vector<MemoryEntry> result;
        for (size_t i = 0; i < std::min(maxResults, scored.size()); i++) {
            result.push_back(scored[i].first);
        }
        
        return result;
    }
    
    void SaveMemoryToDisk() {
        std::wstring path = m_workspaceRoot + L"\\rawrxd_memory.dat";
        std::wofstream file(path, std::ios::binary);
        if (!file) return;
        
        file << m_memory.size() << L"\n";
        for (const auto& entry : m_memory) {
            file << entry.type << L"|" << entry.content << L"|" 
                 << entry.relevance_score << L"|"
                 << std::chrono::duration_cast<std::chrono::seconds>(
                        entry.timestamp.time_since_epoch()).count() << L"\n";
        }
    }
    
    void LoadMemoryFromDisk() {
        std::wstring path = m_workspaceRoot + L"\\rawrxd_memory.dat";
        std::wifstream file(path);
        if (!file) return;
        
        size_t count;
        file >> count;
        if (count > 100000) return; // Sanity check
        
        file.ignore(); // Skip newline
        m_memory.reserve(count);
        
        for (size_t i = 0; i < count; i++) {
            std::wstring line;
            if (!std::getline(file, line)) break;
            
            // Parse: type|content|relevance|timestamp
            auto parts = Split(line, L'|');
            if (parts.size() >= 4) {
                MemoryEntry entry;
                entry.type = parts[0];
                entry.content = parts[1];
                entry.relevance_score = std::wcstof(parts[2].c_str(), nullptr);
                
                auto seconds = std::wcstoll(parts[3].c_str(), nullptr, 10);
                entry.timestamp = std::chrono::system_clock::from_time_t(seconds);
                
                m_memory.push_back(entry);
            }
        }
    }
    
    // ========================================================================
    // FILE OPERATIONS
    // ========================================================================
    
    bool ReadTextFile(const std::wstring& path, std::wstring& content) {
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            RecordFileOperation(L"read", path, L"", false, L"File not found");
            return false;
        }
        
        LARGE_INTEGER fileSize;
        GetFileSizeEx(hFile, &fileSize);
        
        if (fileSize.QuadPart > 10 * 1024 * 1024) { // 10MB limit
            CloseHandle(hFile);
            RecordFileOperation(L"read", path, L"", false, L"File too large");
            return false;
        }
        
        std::vector<char> buffer(static_cast<size_t>(fileSize.QuadPart));
        DWORD bytesRead;
        bool success = ::ReadFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, NULL);
        CloseHandle(hFile);
        
        if (success) {
            // Convert to wide string (assume UTF-8)
            int wideSize = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, NULL, 0);
            std::vector<wchar_t> wideBuffer(wideSize);
            MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, wideBuffer.data(), wideSize);
            content = std::wstring(wideBuffer.data(), wideSize);
            
            RecordFileOperation(L"read", path, content.substr(0, 100), true, L"");
            return true;
        }
        
        RecordFileOperation(L"read", path, L"", false, L"Read failed");
        return false;
    }
    
    bool WriteTextFile(const std::wstring& path, const std::wstring& content) {
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0,
                                  NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            RecordFileOperation(L"write", path, content.substr(0, 100), false, L"Cannot create file");
            return false;
        }
        
        // Convert to UTF-8
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, NULL, 0, NULL, NULL);
        std::vector<char> utf8Buffer(utf8Size);
        WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, utf8Buffer.data(), utf8Size, NULL, NULL);
        
        DWORD bytesWritten;
        bool success = ::WriteFile(hFile, utf8Buffer.data(), utf8Size - 1, &bytesWritten, NULL); // -1 to exclude null terminator
        CloseHandle(hFile);
        
        RecordFileOperation(L"write", path, content.substr(0, 100), success, success ? L"" : L"Write failed");
        return success;
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    void LoadInstructions(const std::wstring& filePath) {
        if (!filePath.empty()) {
            DWORD attr = GetFileAttributesW(filePath.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES) {
                m_instructionFile = filePath;
                ReadTextFile(filePath, m_instructions);
            }
        } else {
            // Default instructions
            m_instructions = 
                L"You are RawrXD, an advanced agentic AI assistant.\n"
                L"- Be helpful, accurate, and efficient\n"
                L"- Prioritize code quality and best practices\n"
                L"- Use Win32 APIs instead of Qt when possible\n"
                L"- Provide clear explanations for complex operations\n"
                L"- Handle errors gracefully\n";
        }
        
        MemoryEntry entry;
        entry.type = L"system";
        entry.content = L"Instructions loaded: " + std::to_wstring(m_instructions.length()) + L" characters";
        entry.timestamp = std::chrono::system_clock::now();
        entry.relevance_score = 1.0f;
        AddMemoryEntry(entry);
    }
    
    void SetWorkspaceRoot(const std::wstring& path) {
        m_workspaceRoot = path;
    }
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    struct Statistics {
        int totalRequests;
        int successfulRequests;
        int totalTokens;
        std::chrono::milliseconds uptime;
        size_t memoryEntries;
        size_t pendingTasks;
        std::wstring modelName;
    };
    
    Statistics GetStatistics() const {
        Statistics stats;
        stats.totalRequests = m_totalRequests;
        stats.successfulRequests = m_successfulRequests;
        stats.totalTokens = m_totalTokens;
        stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_startTime);
        stats.memoryEntries = m_memory.size();
        stats.pendingTasks = GetPendingTasks().size();
        stats.modelName = GetModelName();
        return stats;
    }

private:
    // ========================================================================
    // WORKER THREADS
    // ========================================================================
    
    void InferenceWorker() {
        while (m_running) {
            InferenceRequest request;
            bool hasRequest = false;
            
            {
                std::unique_lock<std::mutex> lock(m_requestMutex);
                m_requestCV.wait(lock, [this] { return !m_requestQueue.empty() || !m_running; });
                
                if (!m_running) break;
                
                if (!m_requestQueue.empty()) {
                    request = m_requestQueue.front();
                    m_requestQueue.pop();
                    hasRequest = true;
                }
            }
            
            if (hasRequest) {
                ProcessInferenceRequest(request);
            }
        }
    }
    
    void ProcessInferenceRequest(const InferenceRequest& request) {
        InferenceResponse response;
        response.request_id = request.request_id;
        response.success = false;
        
        auto start = std::chrono::steady_clock::now();
        
        if (!IsModelLoaded()) {
            response.error = L"No model loaded";
        } else {
            // TODO: Implement actual inference
            // For now, generate a placeholder response
            response.text = L"[RawrXD Response] I understand you want to: " + request.prompt + 
                           L"\n\nThis is a placeholder response. Implement actual GGUF inference here.";
            response.success = true;
            response.tokens_generated = 20;
            m_successfulRequests++;
            m_totalTokens += response.tokens_generated;
        }
        
        auto end = std::chrono::steady_clock::now();
        response.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Add to memory
        MemoryEntry entry;
        entry.type = L"conversation";
        entry.content = L"Q: " + request.prompt + L"\nA: " + response.text;
        entry.timestamp = std::chrono::system_clock::now();
        entry.relevance_score = 0.9f;
        AddMemoryEntry(entry);
        
        // Invoke callbacks
        if (response.success && m_streamCallback) {
            // Simulate streaming by sending words
            auto words = Split(response.text, L' ');
            for (const auto& word : words) {
                m_streamCallback(word + L" ");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        
        if (m_completionCallback) {
            m_completionCallback(response);
        }
    }
    
    void MemoryWorker() {
        while (m_running) {
            std::this_thread::sleep_for(std::chrono::minutes(5));
            
            if (m_autoSaveMemory) {
                SaveMemoryToDisk();
            }
            
            // Periodic cleanup
            if (m_memory.size() > m_maxMemoryEntries * 1.2) {
                // More aggressive cleanup
                std::sort(m_memory.begin(), m_memory.end(),
                         [](const MemoryEntry& a, const MemoryEntry& b) {
                             return a.relevance_score > b.relevance_score;
                         });
                m_memory.resize(m_maxMemoryEntries);
            }
        }
    }
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    std::wstring GetCurrentDir() const {
        wchar_t buffer[MAX_PATH];
        ::GetCurrentDirectoryW(MAX_PATH, buffer);
        return std::wstring(buffer);
    }
    
    static std::wstring ToLower(const std::wstring& str) {
        std::wstring result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::towlower);
        return result;
    }
    
    static std::vector<std::wstring> Split(const std::wstring& str, wchar_t delim) {
        std::vector<std::wstring> result;
        std::wstringstream ss(str);
        std::wstring item;
        
        while (std::getline(ss, item, delim)) {
            result.push_back(item);
        }
        
        return result;
    }
    
    void RecordFileOperation(const std::wstring& operation, const std::wstring& path,
                            const std::wstring& content, bool success, const std::wstring& error) {
        FileOperation op;
        op.operation = operation;
        op.path = path;
        op.content = content.substr(0, 200); // Truncate
        op.success = success;
        op.error = error;
        op.timestamp = std::chrono::system_clock::now();
        
        m_fileHistory.push_back(op);
        
        // Keep only last 1000 operations
        if (m_fileHistory.size() > 1000) {
            m_fileHistory.erase(m_fileHistory.begin(), m_fileHistory.begin() + 100);
        }
    }
};

// ============================================================================
// C INTERFACE FOR DLL EXPORT
// ============================================================================

extern "C" {
    
__declspec(dllexport) AgenticEngine* CreateAgenticEngine() {
    return new AgenticEngine();
}

__declspec(dllexport) void DestroyAgenticEngine(AgenticEngine* engine) {
    delete engine;
}

__declspec(dllexport) BOOL AgenticEngine_Initialize(AgenticEngine* engine, const wchar_t* workspaceRoot) {
    if (!engine) return FALSE;
    std::wstring root = workspaceRoot ? workspaceRoot : L"";
    return engine->Initialize(root) ? TRUE : FALSE;
}

__declspec(dllexport) void AgenticEngine_Stop(AgenticEngine* engine) {
    if (engine) engine->Stop();
}

__declspec(dllexport) BOOL AgenticEngine_LoadModel(AgenticEngine* engine, const wchar_t* path) {
    if (!engine || !path) return FALSE;
    return engine->LoadModel(path) ? TRUE : FALSE;
}

__declspec(dllexport) void AgenticEngine_UnloadModel(AgenticEngine* engine) {
    if (engine) engine->UnloadModel();
}

__declspec(dllexport) BOOL AgenticEngine_IsModelLoaded(AgenticEngine* engine) {
    return (engine && engine->IsModelLoaded()) ? TRUE : FALSE;
}

__declspec(dllexport) int AgenticEngine_SubmitRequest(AgenticEngine* engine, 
    const wchar_t* prompt, const wchar_t* context, 
    float temperature, float top_p, int max_tokens) {
    if (!engine || !prompt) return -1;
    
    std::wstring promptStr = prompt;
    std::wstring contextStr = context ? context : L"";
    
    return engine->SubmitRequest(promptStr, contextStr, temperature, top_p, max_tokens);
}

__declspec(dllexport) void AgenticEngine_AddTask(AgenticEngine* engine, 
    const wchar_t* description, const wchar_t* context, int priority) {
    if (!engine || !description) return;
    
    std::wstring descStr = description;
    std::wstring ctxStr = context ? context : L"";
    
    engine->AddTask(descStr, ctxStr, priority);
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
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}