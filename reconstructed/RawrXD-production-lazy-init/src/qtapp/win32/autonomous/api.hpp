#pragma once
/**
 * @file win32_autonomous_api.hpp
 * @brief Full Win32 API access for autonomous agent workloads
 * 
 * Provides comprehensive Win32 API wrappers enabling:
 * - Process creation and management (CreateProcessW, CreateProcessAsUserW)
 * - Thread operations (CreateThread, TerminateThread, GetThreadPriority)
 * - Module/DLL loading (LoadLibraryW, GetProcAddress, FreeLibrary)
 * - File operations (CreateFileW, ReadFile, WriteFile, DeleteFileW)
 * - Registry access (RegOpenKeyExW, RegQueryValueExW, RegSetValueExW)
 * - Window/UI automation (FindWindowW, PostMessageW, SendMessageW)
 * - System information queries (GetSystemInfo, GetProcessorCount)
 * - Named pipes for inter-process communication
 * - Event/Mutex/Semaphore creation and signaling
 * - Job objects for process group management
 * 
 * @author RawrXD Team
 * @date 2026-01-12
 */

#include <QString>
#include <QStringList>
#include <QProcess>
#include <QMap>
#include <windows.h>
#include <memory>
#include <functional>

// Forward declarations
struct ProcessHandle;
struct ThreadHandle;
struct ModuleHandle;
struct JobObjectHandle;

/**
 * @struct ProcessInfo
 * @brief Information about a running process
 */
struct ProcessInfo {
    DWORD processId = 0;
    QString commandLine;
    QString workingDirectory;
    QString username;
    DWORD priority = NORMAL_PRIORITY_CLASS;
    HANDLE jobObject = NULL;
    bool isRunning = false;
    DWORD exitCode = 0;
};

/**
 * @struct ProcessExecutionResult
 * @brief Result of process execution
 */
struct ProcessExecutionResult {
    bool success = false;
    DWORD processId = 0;
    int exitCode = 0;
    QString stdOutput;
    QString stdError;
    QString errorMessage;
    double executionTimeMs = 0.0;
    HANDLE processHandle = NULL;
    bool waitForCompletion = true;
};

/**
 * @class Win32AutonomousAPI
 * @brief Complete Win32 API wrapper for autonomous agent operations
 * 
 * All methods are thread-safe. Provides both synchronous and asynchronous
 * process execution with full output capture, environment control, and
 * inter-process communication.
 */
class Win32AutonomousAPI {
public:
    Win32AutonomousAPI();
    ~Win32AutonomousAPI();
    
    /**
     * @brief Get singleton instance
     */
    static Win32AutonomousAPI& instance();
    
    // =====================================================================
    // PROCESS CREATION & MANAGEMENT
    // =====================================================================
    
    /**
     * @brief Create and execute a process with full control
     * 
     * @param executable Path to executable
     * @param arguments Command line arguments
     * @param workingDirectory Working directory (empty = current)
     * @param waitForCompletion If true, wait for process to finish
     * @param captureOutput If true, capture stdout/stderr
     * @param environmentVars Environment variables (empty = inherit parent)
     * @param priority Process priority class (IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, etc.)
     * @param createWindow If true, create visible window; if false, hide window
     * @param runAsAdmin If true, attempt to run as administrator
     * @return Process execution result with stdout/stderr
     */
    ProcessExecutionResult createProcess(
        const QString& executable,
        const QStringList& arguments = QStringList(),
        const QString& workingDirectory = QString(),
        bool waitForCompletion = true,
        bool captureOutput = true,
        const QMap<QString, QString>& environmentVars = QMap<QString, QString>(),
        DWORD priority = NORMAL_PRIORITY_CLASS,
        bool createWindow = false,
        bool runAsAdmin = false
    );
    
    /**
     * @brief Create process as specific user
     * 
     * Requires elevated privileges.
     * 
     * @param username Username (domain\\username or just username)
     * @param password Password
     * @param executable Path to executable
     * @param arguments Command line arguments
     * @param workingDirectory Working directory
     * @return Process execution result
     */
    ProcessExecutionResult createProcessAsUser(
        const QString& username,
        const QString& password,
        const QString& executable,
        const QStringList& arguments = QStringList(),
        const QString& workingDirectory = QString()
    );
    
    /**
     * @brief Terminate a running process
     * 
     * @param processId Process ID
     * @param waitMs Maximum milliseconds to wait (0 = immediate termination)
     * @return true if successfully terminated
     */
    bool terminateProcess(DWORD processId, int waitMs = 5000);
    
    /**
     * @brief Kill entire process tree (parent + all children)
     * 
     * @param processId Root process ID
     * @param waitMs Maximum milliseconds to wait
     * @return true if all processes terminated
     */
    bool killProcessTree(DWORD processId, int waitMs = 5000);
    
    /**
     * @brief Get information about running process
     * 
     * @param processId Process ID
     * @return Process information
     */
    ProcessInfo getProcessInfo(DWORD processId);
    
    /**
     * @brief Get list of all process IDs
     * 
     * @return QList of process IDs
     */
    QList<DWORD> getAllProcessIds();
    
    /**
     * @brief Find process ID by executable name
     * 
     * @param executableName Name of executable (e.g., "notepad.exe")
     * @param instance Which instance (0 = first, 1 = second, etc.)
     * @return Process ID or 0 if not found
     */
    DWORD findProcessByName(const QString& executableName, int instance = 0);
    
    /**
     * @brief Wait for process to finish with timeout
     * 
     * @param processId Process ID
     * @param timeoutMs Timeout in milliseconds (INFINITE for no timeout)
     * @return true if process finished, false if timeout
     */
    bool waitForProcess(DWORD processId, DWORD timeoutMs = INFINITE);
    
    /**
     * @brief Get exit code of process
     * 
     * @param processId Process ID
     * @return Exit code or STILL_ACTIVE if still running
     */
    DWORD getProcessExitCode(DWORD processId);
    
    /**
     * @brief Create job object for process group management
     * 
     * Job objects allow grouping processes and applying limits
     * 
     * @param jobName Name of job object (empty = unnamed)
     * @return Job object handle
     */
    HANDLE createJobObject(const QString& jobName = QString());
    
    /**
     * @brief Assign process to job object
     * 
     * @param jobHandle Job object handle
     * @param processHandle Process handle
     * @return true if successful
     */
    bool assignProcessToJob(HANDLE jobHandle, HANDLE processHandle);
    
    // =====================================================================
    // THREAD OPERATIONS
    // =====================================================================
    
    /**
     * @brief Create remote thread in process
     * 
     * Creates a thread that runs in another process's address space.
     * Useful for code injection and process manipulation.
     * 
     * @param processHandle Process handle
     * @param threadFunction Address of function to execute
     * @param threadParameter Parameter to pass to thread
     * @param creationFlags Thread creation flags (0, CREATE_SUSPENDED, etc.)
     * @return Thread ID or 0 if failed
     */
    DWORD createRemoteThread(
        HANDLE processHandle,
        LPTHREAD_START_ROUTINE threadFunction,
        LPVOID threadParameter,
        DWORD creationFlags = 0
    );
    
    /**
     * @brief Suspend thread execution
     * 
     * @param threadId Thread ID
     * @return true if successful
     */
    bool suspendThread(DWORD threadId);
    
    /**
     * @brief Resume thread execution
     * 
     * @param threadId Thread ID
     * @return true if successful
     */
    bool resumeThread(DWORD threadId);
    
    /**
     * @brief Terminate thread
     * 
     * @param threadId Thread ID
     * @param exitCode Thread exit code
     * @return true if successful
     */
    bool terminateThread(DWORD threadId, DWORD exitCode);
    
    /**
     * @brief Get thread priority
     * 
     * @param threadId Thread ID
     * @return Priority value or 0 if failed
     */
    int getThreadPriority(DWORD threadId);
    
    /**
     * @brief Set thread priority
     * 
     * @param threadId Thread ID
     * @param priority Priority value (THREAD_PRIORITY_IDLE, THREAD_PRIORITY_NORMAL, etc.)
     * @return true if successful
     */
    bool setThreadPriority(DWORD threadId, int priority);
    
    // =====================================================================
    // MODULE/DLL LOADING
    // =====================================================================
    
    /**
     * @brief Load dynamic library into process
     * 
     * @param dllPath Path to DLL file
     * @param searchFlags Search flags (LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR, etc.)
     * @return Module handle or NULL if failed
     */
    HMODULE loadLibrary(const QString& dllPath, DWORD searchFlags = 0);
    
    /**
     * @brief Load library into another process (remote injection)
     * 
     * Uses CreateRemoteThread to inject DLL into target process
     * 
     * @param processHandle Target process handle
     * @param dllPath Path to DLL file
     * @return true if injection successful
     */
    bool injectDll(HANDLE processHandle, const QString& dllPath);
    
    /**
     * @brief Get address of function in module
     * 
     * @param module Module handle from loadLibrary()
     * @param functionName Function name (ANSI or Unicode)
     * @return Function address or NULL if not found
     */
    FARPROC getProcAddress(HMODULE module, const QString& functionName);
    
    /**
     * @brief Unload dynamic library
     * 
     * @param module Module handle
     * @return true if successful
     */
    bool freeLibrary(HMODULE module);
    
    /**
     * @brief Get base address of module in another process
     * 
     * @param processId Process ID
     * @param moduleName Module name (e.g., "kernel32.dll")
     * @return Base address or 0 if not found
     */
    DWORD64 getRemoteModuleBase(DWORD processId, const QString& moduleName);
    
    // =====================================================================
    // FILE OPERATIONS
    // =====================================================================
    
    /**
     * @brief Create or open file
     * 
     * @param filePath File path
     * @param desiredAccess Access flags (GENERIC_READ, GENERIC_WRITE, etc.)
     * @param creationDisposition What to do if file exists (CREATE_NEW, OPEN_EXISTING, etc.)
     * @param attributes File attributes (FILE_ATTRIBUTE_NORMAL, FILE_FLAG_DELETE_ON_CLOSE, etc.)
     * @return File handle or INVALID_HANDLE_VALUE if failed
     */
    HANDLE createFile(
        const QString& filePath,
        DWORD desiredAccess = GENERIC_READ | GENERIC_WRITE,
        DWORD creationDisposition = OPEN_ALWAYS,
        DWORD attributes = FILE_ATTRIBUTE_NORMAL
    );
    
    /**
     * @brief Read data from file
     * 
     * @param fileHandle File handle from createFile()
     * @param maxBytes Maximum bytes to read
     * @return File contents as QString
     */
    QString readFile(HANDLE fileHandle, int maxBytes = 1000000);
    
    /**
     * @brief Write data to file
     * 
     * @param fileHandle File handle from createFile()
     * @param data Data to write
     * @return true if successful
     */
    bool writeFile(HANDLE fileHandle, const QString& data);
    
    /**
     * @brief Delete file
     * 
     * @param filePath File path
     * @return true if successful
     */
    bool deleteFile(const QString& filePath);
    
    /**
     * @brief Get file size
     * 
     * @param filePath File path
     * @return File size in bytes or -1 if error
     */
    qint64 getFileSize(const QString& filePath);
    
    /**
     * @brief Copy file with optional overwrite
     * 
     * @param sourceFile Source file path
     * @param targetFile Target file path
     * @param failIfExists If true, fail if target exists
     * @return true if successful
     */
    bool copyFile(const QString& sourceFile, const QString& targetFile, bool failIfExists = false);
    
    /**
     * @brief Move/rename file
     * 
     * @param oldPath Old file path
     * @param newPath New file path
     * @return true if successful
     */
    bool moveFile(const QString& oldPath, const QString& newPath);
    
    /**
     * @brief Close file handle
     * 
     * @param fileHandle File handle
     * @return true if successful
     */
    bool closeFile(HANDLE fileHandle);
    
    // =====================================================================
    // REGISTRY OPERATIONS
    // =====================================================================
    
    /**
     * @brief Open registry key
     * 
     * @param rootKey Root key (HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER, etc.)
     * @param subKeyPath Sub-key path
     * @param samDesired Access flags (KEY_READ, KEY_WRITE, etc.)
     * @return Registry key handle or NULL if failed
     */
    HKEY openRegistryKey(
        HKEY rootKey,
        const QString& subKeyPath,
        REGSAM samDesired = KEY_READ
    );
    
    /**
     * @brief Read registry value
     * 
     * @param keyHandle Key handle from openRegistryKey()
     * @param valueName Value name
     * @return Registry value as QString (empty if not found or error)
     */
    QString queryRegistryValue(HKEY keyHandle, const QString& valueName);
    
    /**
     * @brief Write registry value
     * 
     * @param keyHandle Key handle from openRegistryKey()
     * @param valueName Value name
     * @param value Value to write (can be string or DWORD)
     * @return true if successful
     */
    bool setRegistryValue(HKEY keyHandle, const QString& valueName, const QString& value);
    
    /**
     * @brief Delete registry value
     * 
     * @param keyHandle Key handle
     * @param valueName Value name
     * @return true if successful
     */
    bool deleteRegistryValue(HKEY keyHandle, const QString& valueName);
    
    /**
     * @brief Close registry key
     * 
     * @param keyHandle Key handle
     * @return true if successful
     */
    bool closeRegistryKey(HKEY keyHandle);
    
    // =====================================================================
    // WINDOW/UI AUTOMATION
    // =====================================================================
    
    /**
     * @brief Find window by name
     * 
     * @param windowName Window title/name
     * @param className Class name (empty to search all)
     * @return Window handle or NULL if not found
     */
    HWND findWindow(const QString& windowName, const QString& className = QString());
    
    /**
     * @brief Send message to window
     * 
     * @param hwnd Window handle
     * @param msg Message ID
     * @param wParam WPARAM
     * @param lParam LPARAM
     * @return Result from window message handler
     */
    LRESULT sendMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    /**
     * @brief Post message to window (asynchronous)
     * 
     * @param hwnd Window handle
     * @param msg Message ID
     * @param wParam WPARAM
     * @param lParam LPARAM
     * @return true if successful
     */
    bool postMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    /**
     * @brief Simulate keyboard input
     * 
     * @param keyCode Virtual key code (VK_A, VK_RETURN, etc.)
     * @param modifiers Modifier keys (Ctrl, Alt, Shift)
     * @return true if successful
     */
    bool simulateKeyPress(int keyCode, int modifiers = 0);
    
    /**
     * @brief Simulate mouse click
     * 
     * @param x X coordinate
     * @param y Y coordinate
     * @param button Mouse button (1=left, 2=right, 3=middle)
     * @return true if successful
     */
    bool simulateMouseClick(int x, int y, int button = 1);
    
    // =====================================================================
    // SYSTEM INFORMATION
    // =====================================================================
    
    /**
     * @brief Get number of CPU cores
     * 
     * @return Number of logical processors
     */
    int getProcessorCount();
    
    /**
     * @brief Get total system memory in bytes
     * 
     * @return Total physical memory
     */
    qint64 getTotalSystemMemory();
    
    /**
     * @brief Get available system memory in bytes
     * 
     * @return Available physical memory
     */
    qint64 getAvailableSystemMemory();
    
    /**
     * @brief Get OS version string
     * 
     * @return OS version (e.g., "Windows 10 Pro Build 19042")
     */
    QString getOSVersion();
    
    /**
     * @brief Get username of current process
     * 
     * @return Current username
     */
    QString getCurrentUsername();
    
    /**
     * @brief Check if running with administrator privileges
     * 
     * @return true if admin, false otherwise
     */
    bool isRunningAsAdmin();
    
    // =====================================================================
    // INTER-PROCESS COMMUNICATION
    // =====================================================================
    
    /**
     * @brief Create named pipe for IPC
     * 
     * @param pipeName Pipe name (e.g., "\\\\.\\pipe\\myapp")
     * @param bufferSize Buffer size in bytes
     * @param openMode Open mode (PIPE_ACCESS_INBOUND, PIPE_ACCESS_OUTBOUND, etc.)
     * @return Pipe handle or INVALID_HANDLE_VALUE if failed
     */
    HANDLE createNamedPipe(
        const QString& pipeName,
        int bufferSize = 4096,
        DWORD openMode = PIPE_ACCESS_DUPLEX
    );
    
    /**
     * @brief Connect to named pipe
     * 
     * @param pipeName Pipe name
     * @param timeoutMs Connection timeout in milliseconds
     * @return Pipe handle or INVALID_HANDLE_VALUE if failed
     */
    HANDLE connectToNamedPipe(const QString& pipeName, int timeoutMs = 5000);
    
    /**
     * @brief Write to pipe
     * 
     * @param pipeHandle Pipe handle
     * @param data Data to write
     * @return true if successful
     */
    bool writeToPipe(HANDLE pipeHandle, const QString& data);
    
    /**
     * @brief Read from pipe
     * 
     * @param pipeHandle Pipe handle
     * @param maxBytes Maximum bytes to read
     * @return Data read from pipe
     */
    QString readFromPipe(HANDLE pipeHandle, int maxBytes = 4096);
    
    /**
     * @brief Close pipe handle
     * 
     * @param pipeHandle Pipe handle
     * @return true if successful
     */
    bool closePipe(HANDLE pipeHandle);
    
    // =====================================================================
    // EVENT/MUTEX/SEMAPHORE
    // =====================================================================
    
    /**
     * @brief Create named event object
     * 
     * @param eventName Event name (empty = unnamed)
     * @param manualReset true for manual reset, false for auto-reset
     * @param initialState Initial state (signaled or unsignaled)
     * @return Event handle or NULL if failed
     */
    HANDLE createEvent(
        const QString& eventName = QString(),
        bool manualReset = true,
        bool initialState = false
    );
    
    /**
     * @brief Set event (signal)
     * 
     * @param eventHandle Event handle
     * @return true if successful
     */
    bool setEvent(HANDLE eventHandle);
    
    /**
     * @brief Reset event (unsignal)
     * 
     * @param eventHandle Event handle
     * @return true if successful
     */
    bool resetEvent(HANDLE eventHandle);
    
    /**
     * @brief Wait for event with timeout
     * 
     * @param eventHandle Event handle
     * @param timeoutMs Timeout in milliseconds (INFINITE for no timeout)
     * @return true if event signaled, false if timeout
     */
    bool waitForEvent(HANDLE eventHandle, DWORD timeoutMs = INFINITE);
    
    /**
     * @brief Create mutex object
     * 
     * @param mutexName Mutex name (empty = unnamed)
     * @return Mutex handle or NULL if failed
     */
    HANDLE createMutex(const QString& mutexName = QString());
    
    /**
     * @brief Acquire mutex with timeout
     * 
     * @param mutexHandle Mutex handle
     * @param timeoutMs Timeout in milliseconds
     * @return true if acquired
     */
    bool acquireMutex(HANDLE mutexHandle, DWORD timeoutMs = INFINITE);
    
    /**
     * @brief Release mutex
     * 
     * @param mutexHandle Mutex handle
     * @return true if successful
     */
    bool releaseMutex(HANDLE mutexHandle);
    
    /**
     * @brief Create semaphore
     * 
     * @param semaphoreName Semaphore name (empty = unnamed)
     * @param initialCount Initial count
     * @param maxCount Maximum count
     * @return Semaphore handle or NULL if failed
     */
    HANDLE createSemaphore(
        const QString& semaphoreName = QString(),
        long initialCount = 1,
        long maxCount = 1
    );
    
    /**
     * @brief Close object handle
     * 
     * @param handle Handle to close
     * @return true if successful
     */
    bool closeHandle(HANDLE handle);
    
private:
    // Helper methods
    QString getErrorMessage(DWORD errorCode);
    QString wideToQt(const wchar_t* wideStr);
    std::wstring qtToWide(const QString& str);
    HANDLE getCurrentProcessHandle();
    
    QMap<DWORD, ProcessInfo> m_processes;
    static Win32AutonomousAPI* s_instance;
};
