// ============================================================================
// native_ide_tools.h — Unified Native IDE Tools for RawrXD
// Connects existing tool_registry, github_mcp_bridge, and agentic_executor
// into a cohesive IDE-integrated tool system. Zero external dependencies.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace RawrXD {
namespace NativeIDE {

// ============================================================================
// Tool Result Structure
// ============================================================================
struct ToolResult {
    bool success = false;
    int32_t statusCode = 0;
    std::string output;
    std::string error;
    std::map<std::string, std::string> metadata;
    double durationMs = 0.0;
};

// ============================================================================
// Tool Categories (bitmask for filtering)
// ============================================================================
enum class ToolCategory : uint64_t {
    None = 0,
    File = 1ULL << 0,
    Code = 1ULL << 1,
    Terminal = 1ULL << 2,
    Git = 1ULL << 3,
    GitHub = 1ULL << 4,
    Project = 1ULL << 5,
    Web = 1ULL << 6,
    Database = 1ULL << 7,
    Test = 1ULL << 8,
    Doc = 1ULL << 9,
    Security = 1ULL << 10,
    Performance = 1ULL << 11,
    Deployment = 1ULL << 12,
    Config = 1ULL << 13,
    Debug = 1ULL << 14,
    Memory = 1ULL << 15,
    User = 1ULL << 16,
    Math = 1ULL << 17,
    String = 1ULL << 18,
    Array = 1ULL << 19,
    Data = 1ULL << 20,
    System = 1ULL << 21,
    Network = 1ULL << 22,
    Archive = 1ULL << 23,
    Image = 1ULL << 24,
    Audio = 1ULL << 25,
    Video = 1ULL << 26,
    Crypto = 1ULL << 27,
    Compression = 1ULL << 28,
    Validation = 1ULL << 29,
    Conversion = 1ULL << 30,
    Misc = 1ULL << 31,
    Search = 1ULL << 32,
    Refactor = 1ULL << 33,
    All = 0xFFFFFFFFFFFFFFFFULL
};

// ============================================================================
// Tool Definition
// ============================================================================
struct ToolDefinition {
    std::string name;
    std::string description;
    ToolCategory category = ToolCategory::None;
    std::vector<std::string> parameters;
    std::function<ToolResult(const std::map<std::string, std::string>&)> execute;
    bool enabled = true;
    uint32_t rateLimit = 10; // calls per second
    uint8_t permissionLevel = 0; // 0=read, 1=write, 2=execute, 3=admin
};

// ============================================================================
// Tool Registry — 100+ Tools for IDE Integration
// ============================================================================
class ToolRegistry {
public:
    static ToolRegistry& Instance();
    
    // Registration
    void RegisterTool(const ToolDefinition& tool);
    void UnregisterTool(const std::string& name);
    bool HasTool(const std::string& name) const;
    
    // Execution
    ToolResult ExecuteTool(const std::string& name, 
                          const std::map<std::string, std::string>& params);
    ToolResult ExecuteToolJSON(const std::string& name, 
                               const std::string& jsonParams);
    
    // Discovery
    std::vector<std::string> ListTools(ToolCategory filter = ToolCategory::All) const;
    std::string GetToolDescription(const std::string& name) const;
    std::vector<std::string> GetToolParameters(const std::string& name) const;
    
    // Toggle
    void EnableTool(const std::string& name);
    void DisableTool(const std::string& name);
    bool IsToolEnabled(const std::string& name) const;
    
    // Rate limiting
    void SetRateLimit(const std::string& name, uint32_t callsPerSecond);
    bool CheckRateLimit(const std::string& name) const;
    
    // Permission levels
    void SetPermissionLevel(const std::string& name, uint8_t level);
    uint8_t GetPermissionLevel(const std::string& name) const;
    
    // Audit logging
    void EnableAuditLog(bool enable);
    std::string GetAuditLog() const;
    void ClearAuditLog();
    
    // Initialize all built-in tools
    void InitializeBuiltInTools();
    
private:
    ToolRegistry();
    ~ToolRegistry();
    
    std::map<std::string, ToolDefinition> tools_;
    mutable std::mutex mutex_;
    
    // Rate limiting state
    mutable std::map<std::string, uint32_t> callCounts_;
    mutable std::map<std::string, uint64_t> lastCallTimes_;
    
    // Audit log
    bool auditEnabled_ = false;
    std::string auditLog_;
    
    // Helper for JSON parsing
    static std::map<std::string, std::string> ParseJSON(const std::string& json);
    static std::string ToJSON(const ToolResult& result);
};

// ============================================================================
// File Explorer — Native File System Browser
// ============================================================================
struct FileEntry {
    std::string path;
    std::string name;
    bool isDirectory = false;
    bool isSymlink = false;
    uint64_t size = 0;
    uint64_t modifiedTime = 0;
    uint32_t depth = 0;
    bool isExpanded = false;
    bool isSelected = false;
};

class FileExplorer {
public:
    FileExplorer(const std::string& rootPath);
    ~FileExplorer();
    
    // Navigation
    void NavigateTo(const std::string& path);
    void GoUp();
    void Refresh();
    
    // File operations
    bool CreateFile(const std::string& name);
    bool CreateFolder(const std::string& name);
    bool Delete(const std::string& path);
    bool Rename(const std::string& oldPath, const std::string& newName);
    bool Copy(const std::string& src, const std::string& dst);
    bool Move(const std::string& src, const std::string& dst);
    
    // Search
    std::vector<FileEntry> Search(const std::string& query, bool recursive = true);
    
    // Access
    const std::vector<FileEntry>& GetEntries() const { return entries_; }
    const std::string& GetCurrentPath() const { return currentPath_; }
    const std::string& GetRootPath() const { return rootPath_; }
    
    // Selection
    void SetSelectedIndex(int32_t index) { selectedIndex_ = index; }
    int32_t GetSelectedIndex() const { return selectedIndex_; }
    const FileEntry* GetSelectedEntry() const;
    
    // Configuration
    void SetShowHidden(bool show) { showHidden_ = show; Refresh(); }
    bool GetShowHidden() const { return showHidden_; }
    
private:
    std::string rootPath_;
    std::string currentPath_;
    std::vector<FileEntry> entries_;
    int32_t selectedIndex_ = 0;
    bool showHidden_ = false;
    
    void LoadDirectory(const std::string& path);
};

// ============================================================================
// Code Navigator — Symbol Resolution & Cross-Reference
// ============================================================================
struct Symbol {
    std::string name;
    std::string file;
    uint32_t line = 0;
    uint32_t column = 0;
    uint32_t kind = 0; // function, variable, type, macro, etc.
    std::string signature;
    std::string documentation;
};

struct Reference {
    std::string file;
    uint32_t line = 0;
    uint32_t column = 0;
};

class CodeNavigator {
public:
    CodeNavigator();
    ~CodeNavigator();
    
    // Indexing
    void IndexFile(const std::string& path);
    void IndexDirectory(const std::string& path, bool recursive = true);
    void ClearIndex();
    
    // Symbol lookup
    Symbol* FindDefinition(const std::string& name, const std::string& contextFile = "", uint32_t contextLine = 0);
    std::vector<Reference> FindReferences(const std::string& name, const std::string& contextFile = "");
    Symbol* FindSymbol(const std::string& name);
    
    // Navigation
    std::string GetHoverInfo(const std::string& file, uint32_t line, uint32_t column);
    std::vector<std::string> GetCompletions(const std::string& file, uint32_t line, uint32_t column);
    
    // Cross-file
    void ResolveImports(const std::string& file);
    void BuildCallGraph();
    
    // Access
    const std::vector<Symbol>& GetSymbols() const { return symbols_; }
    const std::vector<std::string>& GetIndexedFiles() const { return indexedFiles_; }
    
private:
    std::vector<Symbol> symbols_;
    std::vector<Reference> references_;
    std::vector<std::string> indexedFiles_;
    
    void ParseFileForSymbols(const std::string& path);
};

// ============================================================================
// Autopilot System — Autonomous Task Execution
// ============================================================================
enum class TaskState {
    Idle,
    Analyzing,
    Planning,
    Executing,
    WaitingUser,
    Completed,
    Failed
};

struct Task {
    std::string description;
    std::vector<std::string> steps;
    size_t currentStep = 0;
    TaskState state = TaskState::Idle;
    std::string errorMessage;
};

class Autopilot {
public:
    Autopilot();
    ~Autopilot();
    
    // Task management
    void AddTask(const std::string& description);
    void CancelTask();
    Task* GetCurrentTask();
    
    // Execution
    void Step();
    void Run(const std::string& goal);
    void Pause();
    void Resume();
    
    // Planning
    void CreatePlan(const std::string& goal);
    void ExecuteNextStep();
    
    // User interaction
    bool RequestApproval(const std::string& action);
    void Log(const std::string& message);
    
    // Configuration
    void SetAutonomyLevel(int level); // 0=manual, 1=semi-auto, 2=full-auto
    int GetAutonomyLevel() const { return autonomyLevel_; }
    
    void SetRequireApprovalForFileWrite(bool require);
    void SetRequireApprovalForTerminal(bool require);
    void SetRequireApprovalForGit(bool require);
    
    // Callbacks
    using TaskCallback = std::function<void(Task*)>;
    void SetOnTaskStart(TaskCallback cb) { onTaskStart_ = cb; }
    void SetOnTaskProgress(TaskCallback cb) { onTaskProgress_ = cb; }
    void SetOnTaskComplete(TaskCallback cb) { onTaskComplete_ = cb; }
    void SetOnUserInputRequired(std::function<void(Task*, const std::string&)> cb) { onUserInputRequired_ = cb; }
    
private:
    std::vector<Task> tasks_;
    Task* currentTask_ = nullptr;
    int autonomyLevel_ = 1;
    
    bool requireApprovalFileWrite_ = true;
    bool requireApprovalTerminal_ = true;
    bool requireApprovalGit_ = true;
    
    std::string logBuffer_;
    
    TaskCallback onTaskStart_;
    TaskCallback onTaskProgress_;
    TaskCallback onTaskComplete_;
    std::function<void(Task*, const std::string&)> onUserInputRequired_;
    
    void AnalyzeTask(Task* task);
    void PlanTask(Task* task);
    void ExecuteStep(Task* task);
};

// ============================================================================
// Memory System — Persistent Knowledge Storage
// ============================================================================
class MemorySystem {
public:
    static MemorySystem& Instance();
    
    // Store
    void Remember(const std::string& key, const std::string& value);
    void RememberFile(const std::string& key, const std::string& filePath);
    
    // Recall
    std::string Recall(const std::string& key);
    std::vector<std::string> RecallPattern(const std::string& pattern);
    bool HasKey(const std::string& key);
    
    // Management
    void Forget(const std::string& key);
    void Clear();
    void SaveToFile(const std::string& filePath);
    void LoadFromFile(const std::string& filePath);
    
    // Access
    const std::map<std::string, std::string>& GetAll() const { return memory_; }
    size_t Count() const { return memory_.size(); }
    
private:
    MemorySystem() = default;
    std::map<std::string, std::string> memory_;
    std::map<std::string, uint64_t> timestamps_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Platform Abstraction — Zero External Dependencies
// ============================================================================
namespace Platform {

// Memory
void* Alloc(size_t size);
void* Realloc(void* ptr, size_t size);
void Free(void* ptr);

// Strings
size_t StrLen(const char* str);
char* StrDup(const char* str);
int StrCmp(const char* a, const char* b);
int StrNCmp(const char* a, const char* b, size_t n);
char* StrStr(const char* haystack, const char* needle);
char* StrChr(const char* str, char c);
void MemCpy(void* dst, const void* src, size_t n);
void MemMove(void* dst, const void* src, size_t n);
void MemSet(void* dst, int value, size_t n);

// File system
bool FileExists(const char* path);
bool DirExists(const char* path);
bool CreateDir(const char* path);
bool DeleteFile(const char* path);
bool DeleteDir(const char* path);
bool CopyFile(const char* src, const char* dst);
bool MoveFile(const char* src, const char* dst);
char* ReadFile(const char* path, size_t* outSize);
bool WriteFile(const char* path, const char* data, size_t size);
char* GetCwd(char* buffer, size_t size);
bool SetCwd(const char* path);
char* GetExePath();

// Process execution
int Execute(const char* command, char** output);

// Time
uint64_t GetTimeMicroseconds();
uint64_t GetTimeMilliseconds();
void SleepMs(uint32_t ms);

// Environment
char* GetEnv(const char* name);
bool SetEnv(const char* name, const char* value);

} // namespace Platform

} // namespace NativeIDE
} // namespace RawrXD

// ============================================================================
// Extended Tools Registration
// ============================================================================
void RegisterExtendedTools();

// ============================================================================
// Convenience Macros for Tool Registration
// ============================================================================
#define REGISTER_NATIVE_TOOL(name, category, desc, func) \
    RawrXD::NativeIDE::ToolRegistry::Instance().RegisterTool( \
        RawrXD::NativeIDE::ToolDefinition{name, desc, category, {}, func})

#define REGISTER_NATIVE_TOOL_WITH_PARAMS(name, category, desc, params, func) \
    RawrXD::NativeIDE::ToolRegistry::Instance().RegisterTool( \
        RawrXD::NativeIDE::ToolDefinition{name, desc, category, params, func})
