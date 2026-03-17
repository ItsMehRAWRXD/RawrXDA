#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

/**
 * @file win32_agent_tools.h
 * @brief Full Win32 API bridge for autonomous agentic operations
 * 
 * Provides unrestricted Win32 access for:
 * - Process management
 * - Memory inspection
 * - File system operations
 * - Registry access
 * - IPC mechanisms
 * - Debug APIs
 * - Hardware control
 * 
 * No sandboxing - genuine autonomous capability
 */

namespace rawrxd {
namespace agent {

//=============================================================================
// PROCESS MANAGEMENT
//=============================================================================

class ProcessManager {
public:
    struct ProcessInfo {
        DWORD pid;
        std::wstring name;
        std::wstring path;
        size_t memory_mb;
        float cpu_percent;
    };
    
    // Create process with full control
    static HANDLE CreateAgenticProcess(
        const std::wstring& executable,
        const std::wstring& arguments,
        const std::wstring& working_dir = L"",
        bool inherit_handles = false
    );
    
    // Enumerate running processes
    static std::vector<ProcessInfo> EnumerateProcesses();
    
    // Read process memory
    static bool ReadProcessMemory(
        HANDLE process,
        void* address,
        void* buffer,
        size_t size
    );
    
    // Write process memory
    static bool WriteProcessMemory(
        HANDLE process,
        void* address,
        const void* buffer,
        size_t size
    );
    
    // Terminate process
    static bool TerminateProcess(HANDLE process, UINT exit_code = 0);
    
    // Get process by name
    static HANDLE FindProcessByName(const std::wstring& name);
    
    // Inject DLL into process
    static bool InjectDLL(HANDLE process, const std::wstring& dll_path);
};

//=============================================================================
// FILE SYSTEM OPERATIONS
//=============================================================================

class FileSystemTools {
public:
    // Read file with memory mapping
    static std::vector<uint8_t> ReadFileMapped(const std::wstring& path);
    
    // Write file atomically
    static bool WriteFileAtomic(
        const std::wstring& path,
        const void* data,
        size_t size
    );
    
    // Create directory recursively
    static bool CreateDirectoryRecursive(const std::wstring& path);
    
    // Delete directory recursively
    static bool DeleteDirectoryRecursive(const std::wstring& path);
    
    // Monitor directory changes
    static HANDLE MonitorDirectory(
        const std::wstring& path,
        std::function<void(const std::wstring&, DWORD)> callback
    );
    
    // Get file attributes
    static bool GetFileInfo(
        const std::wstring& path,
        WIN32_FILE_ATTRIBUTE_DATA& info
    );
    
    // Fast file copy
    static bool CopyFileFast(
        const std::wstring& source,
        const std::wstring& dest,
        bool overwrite = false
    );
};

//=============================================================================
// REGISTRY OPERATIONS
//=============================================================================

class RegistryTools {
public:
    enum class RootKey {
        HKEY_CURRENT_USER,
        HKEY_LOCAL_MACHINE,
        HKEY_CLASSES_ROOT,
        HKEY_USERS
    };
    
    // Read registry value
    static std::wstring ReadString(
        RootKey root,
        const std::wstring& subkey,
        const std::wstring& value_name
    );
    
    static DWORD ReadDWORD(
        RootKey root,
        const std::wstring& subkey,
        const std::wstring& value_name
    );
    
    // Write registry value
    static bool WriteString(
        RootKey root,
        const std::wstring& subkey,
        const std::wstring& value_name,
        const std::wstring& value
    );
    
    static bool WriteDWORD(
        RootKey root,
        const std::wstring& subkey,
        const std::wstring& value_name,
        DWORD value
    );
    
    // Delete registry key
    static bool DeleteKey(
        RootKey root,
        const std::wstring& subkey
    );
    
    // Enumerate subkeys
    static std::vector<std::wstring> EnumerateSubkeys(
        RootKey root,
        const std::wstring& subkey
    );
};

//=============================================================================
// MEMORY MANAGEMENT
//=============================================================================

class MemoryTools {
public:
    // Allocate virtual memory
    static void* AllocateVirtual(
        size_t size,
        DWORD protect = PAGE_READWRITE
    );
    
    // Free virtual memory
    static bool FreeVirtual(void* address);
    
    // Change memory protection
    static bool ProtectMemory(
        void* address,
        size_t size,
        DWORD new_protect,
        DWORD* old_protect = nullptr
    );
    
    // Lock memory pages
    static bool LockMemory(void* address, size_t size);
    
    // Unlock memory pages
    static bool UnlockMemory(void* address, size_t size);
    
    // Get system memory info
    static MEMORYSTATUSEX GetMemoryStatus();
    
    // Get process memory info
    static PROCESS_MEMORY_COUNTERS GetProcessMemory(HANDLE process = GetCurrentProcess());
};

//=============================================================================
// IPC MECHANISMS
//=============================================================================

class IPCTools {
public:
    // Create named pipe server
    static HANDLE CreateNamedPipeServer(
        const std::wstring& pipe_name,
        size_t buffer_size = 4096
    );
    
    // Connect to named pipe
    static HANDLE ConnectNamedPipe(const std::wstring& pipe_name);
    
    // Read from pipe
    static std::vector<uint8_t> ReadPipe(HANDLE pipe, size_t max_size = 65536);
    
    // Write to pipe
    static bool WritePipe(HANDLE pipe, const void* data, size_t size);
    
    // Create shared memory
    static HANDLE CreateSharedMemory(
        const std::wstring& name,
        size_t size
    );
    
    // Open shared memory
    static HANDLE OpenSharedMemory(const std::wstring& name);
    
    // Map shared memory
    static void* MapSharedMemory(HANDLE mapping);
    
    // Unmap shared memory
    static bool UnmapSharedMemory(void* address);
};

//=============================================================================
// THREADING & SYNCHRONIZATION
//=============================================================================

class ThreadingTools {
public:
    // Create thread with affinity
    static HANDLE CreateThreadWithAffinity(
        std::function<DWORD()> thread_func,
        DWORD_PTR affinity_mask
    );
    
    // Set thread priority
    static bool SetThreadPriority(HANDLE thread, int priority);
    
    // Create mutex
    static HANDLE CreateMutexEx(
        const std::wstring& name = L"",
        bool initial_owner = false
    );
    
    // Create event
    static HANDLE CreateEventEx(
        const std::wstring& name = L"",
        bool manual_reset = false,
        bool initial_state = false
    );
    
    // Create semaphore
    static HANDLE CreateSemaphoreEx(
        const std::wstring& name = L"",
        LONG initial_count = 0,
        LONG maximum_count = 1
    );
    
    // Wait for multiple objects
    static DWORD WaitForMultiple(
        const std::vector<HANDLE>& handles,
        bool wait_all = false,
        DWORD timeout_ms = INFINITE
    );
};

//=============================================================================
// SYSTEM INFORMATION
//=============================================================================

class SystemInfo {
public:
    struct CPUInfo {
        std::wstring vendor;
        std::wstring brand;
        int cores;
        int logical_processors;
        std::vector<uint64_t> features;
    };
    
    struct GPUInfo {
        std::wstring name;
        size_t memory_mb;
        std::wstring driver_version;
    };
    
    // Get CPU information
    static CPUInfo GetCPUInfo();
    
    // Get GPU information
    static std::vector<GPUInfo> GetGPUInfo();
    
    // Get system uptime
    static uint64_t GetSystemUptimeMS();
    
    // Get current user
    static std::wstring GetCurrentUser();
    
    // Get computer name
    static std::wstring GetComputerName();
    
    // Get OS version
    static std::wstring GetOSVersion();
    
    // Check if running as admin
    static bool IsElevated();
};

//=============================================================================
// DEBUG & DIAGNOSTICS
//=============================================================================

class DebugTools {
public:
    // Attach debugger to process
    static bool AttachDebugger(DWORD pid);
    
    // Detach debugger
    static bool DetachDebugger();
    
    // Set breakpoint
    static bool SetBreakpoint(HANDLE process, void* address);
    
    // Read debug registers
    static CONTEXT GetThreadContext(HANDLE thread);
    
    // Write debug registers
    static bool SetThreadContext(HANDLE thread, const CONTEXT& context);
    
    // Generate minidump
    static bool CreateMiniDump(
        HANDLE process,
        const std::wstring& dump_path,
        DWORD dump_type = 0
    );
    
    // Walk stack
    static std::vector<void*> WalkStack(HANDLE thread);
};

//=============================================================================
// AGENT TOOL ROUTER
//=============================================================================

/**
 * @class AgentToolRouter
 * @brief Routes agent actions to appropriate Win32 APIs
 */
class AgentToolRouter {
public:
    enum class ToolCategory {
        PROCESS,
        FILE_SYSTEM,
        REGISTRY,
        MEMORY,
        IPC,
        THREADING,
        SYSTEM_INFO,
        DEBUG
    };
    
    struct ToolResult {
        bool success;
        std::wstring message;
        std::vector<uint8_t> data;
    };
    
    AgentToolRouter();
    ~AgentToolRouter();
    
    // Execute tool by name
    ToolResult executeTool(
        const std::string& tool_name,
        const std::vector<std::string>& args
    );
    
    // Register custom tool
    void registerCustomTool(
        const std::string& name,
        ToolCategory category,
        std::function<ToolResult(const std::vector<std::string>&)> handler
    );
    
    // Get available tools
    std::vector<std::string> getAvailableTools(ToolCategory category) const;
    
    // Enable/disable tool categories
    void enableCategory(ToolCategory category, bool enable);
    
    // Check if tool is allowed
    bool isToolAllowed(const std::string& tool_name) const;
    
private:
    struct ToolDefinition {
        std::string name;
        ToolCategory category;
        std::function<ToolResult(const std::vector<std::string>&)> handler;
        bool enabled;
    };
    
    std::map<std::string, ToolDefinition> tools_;
    std::map<ToolCategory, bool> category_enabled_;
    
    void registerBuiltinTools();
};

} // namespace agent
} // namespace rawrxd
