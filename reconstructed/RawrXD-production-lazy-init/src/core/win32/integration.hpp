#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include <windows.h>
#include <memory>

namespace RawrXD {

// ============================================================
// Win32 Registry Integration
// ============================================================

class Win32Registry {
public:
    enum RootKey {
        KeyClassesRoot,
        KeyCurrentUser,
        KeyLocalMachine,
        KeyUsers,
        KeyPerformanceData,
        KeyCurrentConfig
    };

    static Win32Registry& instance();

    /**
     * Read a registry value
     * @param rootKey The root key (HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, etc.)
     * @param subKey The registry path (e.g., "Software\\RawrXD\\Settings")
     * @param valueName The value name to read
     * @return The value as QString, or empty if not found
     */
    QString readRegistryString(RootKey rootKey, const QString& subKey, const QString& valueName);

    /**
     * Write a registry value
     * @param rootKey The root key
     * @param subKey The registry path
     * @param valueName The value name
     * @param value The value to write
     * @return true if successful
     */
    bool writeRegistryString(RootKey rootKey, const QString& subKey, const QString& valueName, const QString& value);

    /**
     * Read an integer registry value
     * @return The value as int, or 0 if not found
     */
    int readRegistryInt(RootKey rootKey, const QString& subKey, const QString& valueName);

    /**
     * Write an integer registry value
     * @return true if successful
     */
    bool writeRegistryInt(RootKey rootKey, const QString& subKey, const QString& valueName, int value);

    /**
     * Delete a registry key or value
     * @param rootKey The root key
     * @param subKey The registry path
     * @param valueName If empty, deletes the entire key; otherwise deletes the value
     * @return true if successful
     */
    bool deleteRegistry(RootKey rootKey, const QString& subKey, const QString& valueName = "");

    /**
     * List all values in a registry key
     * @param rootKey The root key
     * @param subKey The registry path
     * @return List of value names
     */
    QStringList listRegistryValues(RootKey rootKey, const QString& subKey);

private:
    Win32Registry() = default;
    ~Win32Registry() = default;

    HKEY getRootKeyHandle(RootKey key);
};

// ============================================================
// Win32 Process Management
// ============================================================

class Win32ProcessInfo {
public:
    DWORD processId = 0;
    QString processName;
    QString executablePath;
    qint64 memoryUsage = 0;  // in bytes
    int threadCount = 0;
    bool isRunning = false;
};

class Win32ProcessManager {
public:
    static Win32ProcessManager& instance();

    /**
     * Launch a new process
     * @param executable Path to executable
     * @param arguments Command-line arguments
     * @param workingDirectory Working directory for the process
     * @return Process ID if successful, 0 if failed
     */
    DWORD launchProcess(const QString& executable, const QStringList& arguments = QStringList(), const QString& workingDirectory = "");

    /**
     * Terminate a process
     * @param processId Process ID to terminate
     * @param forceKill If true, forcefully kills the process
     * @return true if successful
     */
    bool terminateProcess(DWORD processId, bool forceKill = false);

    /**
     * Get information about a process
     * @param processId Process ID
     * @return Process information
     */
    Win32ProcessInfo getProcessInfo(DWORD processId);

    /**
     * List all running processes
     * @return List of process information
     */
    QStringList listRunningProcesses();

    /**
     * Check if a process is running
     * @param processId Process ID
     * @return true if the process is running
     */
    bool isProcessRunning(DWORD processId);

    /**
     * Get memory usage of a process
     * @param processId Process ID
     * @return Memory usage in bytes
     */
    qint64 getProcessMemoryUsage(DWORD processId);

private:
    Win32ProcessManager() = default;
    ~Win32ProcessManager() = default;
};

// ============================================================
// Win32 File System Integration
// ============================================================

class Win32FileSystem {
public:
    static Win32FileSystem& instance();

    /**
     * Get file attributes
     * @param filePath Path to file
     * @return File attributes (read-only, hidden, system, etc.)
     */
    DWORD getFileAttributes(const QString& filePath);

    /**
     * Set file attributes
     * @param filePath Path to file
     * @param attributes File attributes to set
     * @return true if successful
     */
    bool setFileAttributes(const QString& filePath, DWORD attributes);

    /**
     * Get file creation, access, and modification times
     * @param filePath Path to file
     * @return Map with "created", "accessed", "modified" timestamps
     */
    QMap<QString, QString> getFileTimestamps(const QString& filePath);

    /**
     * Get file size
     * @param filePath Path to file
     * @return File size in bytes
     */
    qint64 getFileSize(const QString& filePath);

    /**
     * Get available disk space
     * @param drivePath Drive letter or path (e.g., "C:\\")
     * @return Available space in bytes
     */
    qint64 getAvailableDiskSpace(const QString& drivePath);

    /**
     * Get total disk space
     * @param drivePath Drive letter or path
     * @return Total disk space in bytes
     */
    qint64 getTotalDiskSpace(const QString& drivePath);

    /**
     * Create a symbolic link
     * @param linkPath Path for the symbolic link
     * @param targetPath Path the link points to
     * @param isDirectory true if creating a directory link
     * @return true if successful
     */
    bool createSymbolicLink(const QString& linkPath, const QString& targetPath, bool isDirectory = false);

    /**
     * Get file short name (8.3 format)
     * @param filePath Path to file
     * @return Short filename
     */
    QString getShortFileName(const QString& filePath);

private:
    Win32FileSystem() = default;
    ~Win32FileSystem() = default;
};

// ============================================================
// Win32 Environment and System
// ============================================================

class Win32System {
public:
    static Win32System& instance();

    /**
     * Get environment variable
     * @param variableName Name of the environment variable
     * @return Value of the environment variable
     */
    QString getEnvironmentVariable(const QString& variableName);

    /**
     * Set environment variable
     * @param variableName Name of the environment variable
     * @param value Value to set
     * @return true if successful
     */
    bool setEnvironmentVariable(const QString& variableName, const QString& value);

    /**
     * Get system information (OS version, processor count, etc.)
     * @return Map with system information
     */
    QMap<QString, QString> getSystemInfo();

    /**
     * Get processor information
     * @return Map with processor details
     */
    QMap<QString, QString> getProcessorInfo();

    /**
     * Get memory information
     * @return Map with memory details (total, available, used)
     */
    QMap<QString, qint64> getMemoryInfo();

    /**
     * Get Windows version
     * @return Windows version string (e.g., "Windows 10 Build 19041")
     */
    QString getWindowsVersion();

    /**
     * Get total system uptime
     * @return Uptime in seconds
     */
    qint64 getSystemUptime();

    /**
     * Get user name
     * @return Current logged-in user name
     */
    QString getUserName();

    /**
     * Get computer name
     * @return Computer/hostname
     */
    QString getComputerName();

private:
    Win32System() = default;
    ~Win32System() = default;
};

// ============================================================
// Win32 Service Management
// ============================================================

class Win32Service {
public:
    enum class ServiceState {
        Stopped,
        StartPending,
        StopPending,
        Running,
        ContinuePending,
        PausePending,
        Paused,
        Unknown
    };

    QString serviceName;
    QString displayName;
    ServiceState state = ServiceState::Unknown;
    bool isRunning = false;
};

class Win32ServiceManager {
public:
    static Win32ServiceManager& instance();

    /**
     * Start a Windows service
     * @param serviceName Name of the service
     * @return true if successful
     */
    bool startService(const QString& serviceName);

    /**
     * Stop a Windows service
     * @param serviceName Name of the service
     * @return true if successful
     */
    bool stopService(const QString& serviceName);

    /**
     * Pause a Windows service
     * @param serviceName Name of the service
     * @return true if successful
     */
    bool pauseService(const QString& serviceName);

    /**
     * Resume a Windows service
     * @param serviceName Name of the service
     * @return true if successful
     */
    bool resumeService(const QString& serviceName);

    /**
     * Get service status
     * @param serviceName Name of the service
     * @return Service information
     */
    Win32Service getServiceStatus(const QString& serviceName);

    /**
     * List all services
     * @param filterRunning If true, only return running services
     * @return List of services
     */
    QStringList listServices(bool filterRunning = false);

private:
    Win32ServiceManager() = default;
    ~Win32ServiceManager() = default;
};

// ============================================================
// Win32 COM Automation
// ============================================================

class Win32ComAutomation {
public:
    static Win32ComAutomation& instance();

    /**
     * Create a COM object
     * @param progId Program ID (e.g., "Excel.Application")
     * @return true if successful
     */
    bool createComObject(const QString& progId);

    /**
     * Release COM object
     * @return true if successful
     */
    bool releaseComObject();

    /**
     * Call a method on the COM object
     * @param methodName Method name
     * @param parameters Method parameters
     * @return Method return value
     */
    QString callComMethod(const QString& methodName, const QStringList& parameters = QStringList());

    /**
     * Set a property on the COM object
     * @param propertyName Property name
     * @param value Property value
     * @return true if successful
     */
    bool setComProperty(const QString& propertyName, const QString& value);

    /**
     * Get a property from the COM object
     * @param propertyName Property name
     * @return Property value
     */
    QString getComProperty(const QString& propertyName);

private:
    Win32ComAutomation() = default;
    ~Win32ComAutomation() = default;
};

// ============================================================
// Win32 Dialog and Message Box
// ============================================================

class Win32Dialog {
public:
    enum class MessageBoxType {
        Information,
        Warning,
        Error,
        Question
    };

    /**
     * Show a message box
     * @param title Dialog title
     * @param message Dialog message
     * @param type Dialog type
     * @return User's response (1 for OK, 2 for Cancel, etc.)
     */
    static int showMessageBox(const QString& title, const QString& message, MessageBoxType type = MessageBoxType::Information);

    /**
     * Show file open dialog
     * @param title Dialog title
     * @param filter File filter (e.g., "*.txt|Text Files|*.*|All Files")
     * @param initialDirectory Initial directory to browse
     * @return Selected file path or empty if cancelled
     */
    static QString showOpenFileDialog(const QString& title, const QString& filter = "", const QString& initialDirectory = "");

    /**
     * Show file save dialog
     * @param title Dialog title
     * @param filter File filter
     * @param initialDirectory Initial directory
     * @return Selected file path or empty if cancelled
     */
    static QString showSaveFileDialog(const QString& title, const QString& filter = "", const QString& initialDirectory = "");

    /**
     * Show folder browse dialog
     * @param title Dialog title
     * @param initialDirectory Initial directory
     * @return Selected folder path or empty if cancelled
     */
    static QString showFolderBrowseDialog(const QString& title, const QString& initialDirectory = "");

private:
    Win32Dialog() = default;
};

}  // namespace RawrXD
