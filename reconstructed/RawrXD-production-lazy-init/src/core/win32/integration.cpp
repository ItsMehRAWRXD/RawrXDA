#include "win32_integration.hpp"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <windows.h>
#include <Lmcons.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <winreg.h>
#include <winsvc.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace RawrXD {

// ============================================================
// Win32Registry Implementation
// ============================================================

Win32Registry& Win32Registry::instance() {
    static Win32Registry s_instance;
    return s_instance;
}

HKEY Win32Registry::getRootKeyHandle(RootKey key) {
    switch (key) {
        case KeyClassesRoot:
            return HKEY_CLASSES_ROOT;
        case KeyCurrentUser:
            return HKEY_CURRENT_USER;
        case KeyLocalMachine:
            return HKEY_LOCAL_MACHINE;
        case KeyUsers:
            return HKEY_USERS;
        case KeyPerformanceData:
            return HKEY_PERFORMANCE_DATA;
        case KeyCurrentConfig:
            return HKEY_CURRENT_CONFIG;
        default:
            return HKEY_CURRENT_USER;
    }
}

QString Win32Registry::readRegistryString(RootKey rootKey, const QString& subKey, const QString& valueName) {
    HKEY hKey = getRootKeyHandle(rootKey);
    HKEY hSubKey = nullptr;

    // Convert QString to wchar_t for Windows API
    std::wstring subKeyWide = subKey.toStdWString();
    std::wstring valueNameWide = valueName.toStdWString();

    LONG result = RegOpenKeyEx(hKey, subKeyWide.c_str(), 0, KEY_READ, &hSubKey);
    if (result != ERROR_SUCCESS) {
        qWarning() << "Failed to open registry key:" << subKey;
        return "";
    }

    wchar_t buffer[256] = {0};
    DWORD size = sizeof(buffer);
    result = RegQueryValueEx(hSubKey, valueNameWide.c_str(), nullptr, nullptr, (LPBYTE)buffer, &size);

    RegCloseKey(hSubKey);

    if (result != ERROR_SUCCESS) {
        qWarning() << "Failed to read registry value:" << valueName;
        return "";
    }

    return QString::fromWCharArray(buffer);
}

bool Win32Registry::writeRegistryString(RootKey rootKey, const QString& subKey, const QString& valueName, const QString& value) {
    HKEY hKey = getRootKeyHandle(rootKey);
    HKEY hSubKey = nullptr;

    std::wstring subKeyWide = subKey.toStdWString();
    std::wstring valueNameWide = valueName.toStdWString();
    std::wstring valueWide = value.toStdWString();

    DWORD disposition = 0;
    LONG result = RegCreateKeyEx(hKey, subKeyWide.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hSubKey, &disposition);
    if (result != ERROR_SUCCESS) {
        qWarning() << "Failed to create/open registry key:" << subKey;
        return false;
    }

    result = RegSetValueEx(hSubKey, valueNameWide.c_str(), 0, REG_SZ, (const BYTE*)valueWide.c_str(), (DWORD)(valueWide.length() + 1) * sizeof(wchar_t));

    RegCloseKey(hSubKey);

    if (result != ERROR_SUCCESS) {
        qWarning() << "Failed to write registry value:" << valueName;
        return false;
    }

    return true;
}

int Win32Registry::readRegistryInt(RootKey rootKey, const QString& subKey, const QString& valueName) {
    HKEY hKey = getRootKeyHandle(rootKey);
    HKEY hSubKey = nullptr;

    std::wstring subKeyWide = subKey.toStdWString();
    std::wstring valueNameWide = valueName.toStdWString();

    LONG result = RegOpenKeyEx(hKey, subKeyWide.c_str(), 0, KEY_READ, &hSubKey);
    if (result != ERROR_SUCCESS) {
        return 0;
    }

    DWORD value = 0;
    DWORD size = sizeof(DWORD);
    result = RegQueryValueEx(hSubKey, valueNameWide.c_str(), nullptr, nullptr, (LPBYTE)&value, &size);

    RegCloseKey(hSubKey);

    if (result != ERROR_SUCCESS) {
        return 0;
    }

    return (int)value;
}

bool Win32Registry::writeRegistryInt(RootKey rootKey, const QString& subKey, const QString& valueName, int value) {
    HKEY hKey = getRootKeyHandle(rootKey);
    HKEY hSubKey = nullptr;

    std::wstring subKeyWide = subKey.toStdWString();
    std::wstring valueNameWide = valueName.toStdWString();

    DWORD disposition = 0;
    LONG result = RegCreateKeyEx(hKey, subKeyWide.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hSubKey, &disposition);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    DWORD dwordValue = (DWORD)value;
    result = RegSetValueEx(hSubKey, valueNameWide.c_str(), 0, REG_DWORD, (const BYTE*)&dwordValue, sizeof(DWORD));

    RegCloseKey(hSubKey);

    return result == ERROR_SUCCESS;
}

bool Win32Registry::deleteRegistry(RootKey rootKey, const QString& subKey, const QString& valueName) {
    HKEY hKey = getRootKeyHandle(rootKey);
    HKEY hSubKey = nullptr;

    std::wstring subKeyWide = subKey.toStdWString();

    if (valueName.isEmpty()) {
        // Delete the entire key
        LONG result = RegDeleteKey(hKey, subKeyWide.c_str());
        return result == ERROR_SUCCESS;
    } else {
        // Delete a specific value
        LONG result = RegOpenKeyEx(hKey, subKeyWide.c_str(), 0, KEY_WRITE, &hSubKey);
        if (result != ERROR_SUCCESS) {
            return false;
        }

        std::wstring valueNameWide = valueName.toStdWString();
        result = RegDeleteValue(hSubKey, valueNameWide.c_str());

        RegCloseKey(hSubKey);
        return result == ERROR_SUCCESS;
    }
}

QStringList Win32Registry::listRegistryValues(RootKey rootKey, const QString& subKey) {
    QStringList values;
    HKEY hKey = getRootKeyHandle(rootKey);
    HKEY hSubKey = nullptr;

    std::wstring subKeyWide = subKey.toStdWString();
    LONG result = RegOpenKeyEx(hKey, subKeyWide.c_str(), 0, KEY_READ, &hSubKey);
    if (result != ERROR_SUCCESS) {
        return values;
    }

    DWORD index = 0;
    wchar_t valueName[256];
    DWORD valueNameSize = sizeof(valueName) / sizeof(valueName[0]);

    while (RegEnumValue(hSubKey, index, valueName, &valueNameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        values.append(QString::fromWCharArray(valueName));
        valueNameSize = sizeof(valueName) / sizeof(valueName[0]);
        index++;
    }

    RegCloseKey(hSubKey);
    return values;
}

// ============================================================
// Win32ProcessManager Implementation
// ============================================================

Win32ProcessManager& Win32ProcessManager::instance() {
    static Win32ProcessManager s_instance;
    return s_instance;
}

DWORD Win32ProcessManager::launchProcess(const QString& executable, const QStringList& arguments, const QString& workingDirectory) {
    std::wstring exePath = executable.toStdWString();
    std::wstring cmdLine = exePath;

    for (const QString& arg : arguments) {
        cmdLine += L" \"" + arg.toStdWString() + L"\"";
    }

    std::wstring workDir = workingDirectory.toStdWString();

    STARTUPINFO si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};

    if (!CreateProcess(exePath.c_str(), (LPWSTR)cmdLine.c_str(), nullptr, nullptr, FALSE,
                       0, nullptr, workingDirectory.isEmpty() ? nullptr : workDir.c_str(), &si, &pi)) {
        qWarning() << "Failed to launch process:" << executable;
        return 0;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return pi.dwProcessId;
}

bool Win32ProcessManager::terminateProcess(DWORD processId, bool forceKill) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (hProcess == nullptr) {
        qWarning() << "Failed to open process:" << processId;
        return false;
    }

    BOOL result = TerminateProcess(hProcess, forceKill ? 1 : 0);
    CloseHandle(hProcess);

    return result == TRUE;
}

Win32ProcessInfo Win32ProcessManager::getProcessInfo(DWORD processId) {
    Win32ProcessInfo info;
    info.processId = processId;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess == nullptr) {
        return info;
    }

    // Get process name
    wchar_t szProcessName[MAX_PATH] = L"<unknown>";
    DWORD cbNeeded = 0;

    if (GetModuleFileNameEx(hProcess, nullptr, szProcessName, sizeof(szProcessName) / sizeof(wchar_t))) {
        QFileInfo fileInfo(QString::fromWCharArray(szProcessName));
        info.processName = fileInfo.fileName();
        info.executablePath = fileInfo.filePath();
    }

    // Get memory usage
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        info.memoryUsage = pmc.WorkingSetSize;
    }

    // Get thread count
    DWORD threadCount = 0;
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te32;
        te32.dwSize = sizeof(THREADENTRY32);

        if (Thread32First(hThreadSnap, &te32)) {
            do {
                if (te32.th32OwnerProcessID == processId) {
                    threadCount++;
                }
            } while (Thread32Next(hThreadSnap, &te32));
        }
        CloseHandle(hThreadSnap);
    }
    info.threadCount = threadCount;

    // Check if process is still running
    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess, &exitCode)) {
        info.isRunning = (exitCode == STILL_ACTIVE);
    }

    CloseHandle(hProcess);
    return info;
}

QStringList Win32ProcessManager::listRunningProcesses() {
    QStringList processes;

    DWORD aProcesses[1024], cbNeeded, cProcesses;
    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
        return processes;
    }

    cProcesses = cbNeeded / sizeof(DWORD);

    for (DWORD i = 0; i < cProcesses; i++) {
        if (aProcesses[i] == 0) continue;

        Win32ProcessInfo info = getProcessInfo(aProcesses[i]);
        if (info.isRunning && !info.processName.isEmpty()) {
            processes.append(QString("%1 (PID: %2)").arg(info.processName).arg(info.processId));
        }
    }

    return processes;
}

bool Win32ProcessManager::isProcessRunning(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
    if (hProcess == nullptr) {
        return false;
    }

    DWORD exitCode = 0;
    bool isRunning = GetExitCodeProcess(hProcess, &exitCode) && (exitCode == STILL_ACTIVE);

    CloseHandle(hProcess);
    return isRunning;
}

qint64 Win32ProcessManager::getProcessMemoryUsage(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess == nullptr) {
        return 0;
    }

    PROCESS_MEMORY_COUNTERS pmc;
    if (!GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        CloseHandle(hProcess);
        return 0;
    }

    CloseHandle(hProcess);
    return pmc.WorkingSetSize;
}

// ============================================================
// Win32FileSystem Implementation
// ============================================================

Win32FileSystem& Win32FileSystem::instance() {
    static Win32FileSystem s_instance;
    return s_instance;
}

DWORD Win32FileSystem::getFileAttributes(const QString& filePath) {
    std::wstring path = filePath.toStdWString();
    return GetFileAttributes(path.c_str());
}

bool Win32FileSystem::setFileAttributes(const QString& filePath, DWORD attributes) {
    std::wstring path = filePath.toStdWString();
    return SetFileAttributes(path.c_str(), attributes) == TRUE;
}

QMap<QString, QString> Win32FileSystem::getFileTimestamps(const QString& filePath) {
    QMap<QString, QString> timestamps;

    std::wstring path = filePath.toStdWString();
    HANDLE hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return timestamps;
    }

    FILETIME ftCreated, ftAccessed, ftModified;
    if (GetFileTime(hFile, &ftCreated, &ftAccessed, &ftModified)) {
        // Convert FILETIME to string (simplified)
        timestamps["created"] = "File creation time";
        timestamps["accessed"] = "File access time";
        timestamps["modified"] = "File modification time";
    }

    CloseHandle(hFile);
    return timestamps;
}

qint64 Win32FileSystem::getFileSize(const QString& filePath) {
    std::wstring path = filePath.toStdWString();
    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &fad)) {
        LARGE_INTEGER size;
        size.LowPart = fad.nFileSizeLow;
        size.HighPart = fad.nFileSizeHigh;
        return size.QuadPart;
    }

    return 0;
}

qint64 Win32FileSystem::getAvailableDiskSpace(const QString& drivePath) {
    std::wstring path = drivePath.toStdWString();
    ULARGE_INTEGER lpFreeBytesAvailableToCaller, lpTotalNumberOfBytes;

    if (GetDiskFreeSpaceEx(path.c_str(), &lpFreeBytesAvailableToCaller, &lpTotalNumberOfBytes, nullptr)) {
        return lpFreeBytesAvailableToCaller.QuadPart;
    }

    return 0;
}

qint64 Win32FileSystem::getTotalDiskSpace(const QString& drivePath) {
    std::wstring path = drivePath.toStdWString();
    ULARGE_INTEGER lpFreeBytesAvailableToCaller, lpTotalNumberOfBytes;

    if (GetDiskFreeSpaceEx(path.c_str(), &lpFreeBytesAvailableToCaller, &lpTotalNumberOfBytes, nullptr)) {
        return lpTotalNumberOfBytes.QuadPart;
    }

    return 0;
}

bool Win32FileSystem::createSymbolicLink(const QString& linkPath, const QString& targetPath, bool isDirectory) {
    std::wstring link = linkPath.toStdWString();
    std::wstring target = targetPath.toStdWString();

    return CreateSymbolicLink(link.c_str(), target.c_str(), isDirectory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) == TRUE;
}

QString Win32FileSystem::getShortFileName(const QString& filePath) {
    std::wstring path = filePath.toStdWString();
    wchar_t shortPath[MAX_PATH];

    DWORD result = GetShortPathName(path.c_str(), shortPath, MAX_PATH);
    if (result == 0 || result > MAX_PATH) {
        return filePath;
    }

    return QString::fromWCharArray(shortPath);
}

// ============================================================
// Win32System Implementation
// ============================================================

Win32System& Win32System::instance() {
    static Win32System s_instance;
    return s_instance;
}

QString Win32System::getEnvironmentVariable(const QString& variableName) {
    std::wstring var = variableName.toStdWString();
    wchar_t buffer[32767];
    DWORD size = GetEnvironmentVariable(var.c_str(), buffer, 32767);

    if (size == 0 || size > 32767) {
        return "";
    }

    return QString::fromWCharArray(buffer);
}

bool Win32System::setEnvironmentVariable(const QString& variableName, const QString& value) {
    std::wstring var = variableName.toStdWString();
    std::wstring val = value.toStdWString();

    return SetEnvironmentVariable(var.c_str(), val.c_str()) == TRUE;
}

QMap<QString, QString> Win32System::getSystemInfo() {
    QMap<QString, QString> info;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    info["processors"] = QString::number(sysInfo.dwNumberOfProcessors);
    info["processor_type"] = QString::number(sysInfo.dwProcessorType);
    info["page_size"] = QString::number(sysInfo.dwPageSize);

    return info;
}

QMap<QString, QString> Win32System::getProcessorInfo() {
    QMap<QString, QString> info;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    info["count"] = QString::number(sysInfo.dwNumberOfProcessors);
    info["type"] = QString::number(sysInfo.dwProcessorType);
    info["architecture"] = "x64";  // Simplified

    return info;
}

QMap<QString, qint64> Win32System::getMemoryInfo() {
    QMap<QString, qint64> info;

    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);

    if (GlobalMemoryStatusEx(&statex)) {
        info["total"] = statex.ullTotalPhys;
        info["available"] = statex.ullAvailPhys;
        info["used"] = statex.ullTotalPhys - statex.ullAvailPhys;
        info["load"] = statex.dwMemoryLoad;
    }

    return info;
}

QString Win32System::getWindowsVersion() {
    // Simplified Windows version retrieval
    return "Windows 10 or Later";
}

qint64 Win32System::getSystemUptime() {
    return (qint64)(GetTickCount64() / 1000);  // Convert milliseconds to seconds
}

QString Win32System::getUserName() {
    wchar_t buffer[UNLEN + 1];
    DWORD size = UNLEN + 1;

    if (GetUserName(buffer, &size)) {
        return QString::fromWCharArray(buffer);
    }

    return "Unknown";
}

QString Win32System::getComputerName() {
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;

    if (GetComputerName(buffer, &size)) {
        return QString::fromWCharArray(buffer);
    }

    return "Unknown";
}

// ============================================================
// Win32ServiceManager Implementation
// ============================================================

Win32ServiceManager& Win32ServiceManager::instance() {
    static Win32ServiceManager s_instance;
    return s_instance;
}

bool Win32ServiceManager::startService(const QString& serviceName) {
    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCManager == nullptr) {
        return false;
    }

    std::wstring svcName = serviceName.toStdWString();
    SC_HANDLE hService = OpenService(hSCManager, svcName.c_str(), SERVICE_START);
    if (hService == nullptr) {
        CloseServiceHandle(hSCManager);
        return false;
    }

    BOOL result = StartService(hService, 0, nullptr);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return result == TRUE;
}

bool Win32ServiceManager::stopService(const QString& serviceName) {
    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCManager == nullptr) {
        return false;
    }

    std::wstring svcName = serviceName.toStdWString();
    SC_HANDLE hService = OpenService(hSCManager, svcName.c_str(), SERVICE_STOP);
    if (hService == nullptr) {
        CloseServiceHandle(hSCManager);
        return false;
    }

    SERVICE_STATUS svcStatus;
    BOOL result = ControlService(hService, SERVICE_CONTROL_STOP, &svcStatus);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return result == TRUE;
}

bool Win32ServiceManager::pauseService(const QString& serviceName) {
    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCManager == nullptr) {
        return false;
    }

    std::wstring svcName = serviceName.toStdWString();
    SC_HANDLE hService = OpenService(hSCManager, svcName.c_str(), SERVICE_PAUSE_CONTINUE);
    if (hService == nullptr) {
        CloseServiceHandle(hSCManager);
        return false;
    }

    SERVICE_STATUS svcStatus;
    BOOL result = ControlService(hService, SERVICE_CONTROL_PAUSE, &svcStatus);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return result == TRUE;
}

bool Win32ServiceManager::resumeService(const QString& serviceName) {
    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCManager == nullptr) {
        return false;
    }

    std::wstring svcName = serviceName.toStdWString();
    SC_HANDLE hService = OpenService(hSCManager, svcName.c_str(), SERVICE_PAUSE_CONTINUE);
    if (hService == nullptr) {
        CloseServiceHandle(hSCManager);
        return false;
    }

    SERVICE_STATUS svcStatus;
    BOOL result = ControlService(hService, SERVICE_CONTROL_CONTINUE, &svcStatus);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return result == TRUE;
}

Win32Service Win32ServiceManager::getServiceStatus(const QString& serviceName) {
    Win32Service service;
    service.serviceName = serviceName;

    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCManager == nullptr) {
        return service;
    }

    std::wstring svcName = serviceName.toStdWString();
    SC_HANDLE hService = OpenService(hSCManager, svcName.c_str(), SERVICE_QUERY_STATUS);
    if (hService == nullptr) {
        CloseServiceHandle(hSCManager);
        return service;
    }

    SERVICE_STATUS_PROCESS svcStatus;
    DWORD cbNeeded = 0;

    if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&svcStatus, sizeof(SERVICE_STATUS_PROCESS), &cbNeeded)) {
        service.isRunning = (svcStatus.dwCurrentState == SERVICE_RUNNING);
        switch (svcStatus.dwCurrentState) {
            case SERVICE_STOPPED:
                service.state = Win32Service::ServiceState::Stopped;
                break;
            case SERVICE_START_PENDING:
                service.state = Win32Service::ServiceState::StartPending;
                break;
            case SERVICE_STOP_PENDING:
                service.state = Win32Service::ServiceState::StopPending;
                break;
            case SERVICE_RUNNING:
                service.state = Win32Service::ServiceState::Running;
                break;
            case SERVICE_CONTINUE_PENDING:
                service.state = Win32Service::ServiceState::ContinuePending;
                break;
            case SERVICE_PAUSE_PENDING:
                service.state = Win32Service::ServiceState::PausePending;
                break;
            case SERVICE_PAUSED:
                service.state = Win32Service::ServiceState::Paused;
                break;
            default:
                service.state = Win32Service::ServiceState::Unknown;
        }
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return service;
}

QStringList Win32ServiceManager::listServices(bool filterRunning) {
    QStringList services;

    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (hSCManager == nullptr) {
        return services;
    }

    DWORD dwBytesNeeded = 0, dwServicesReturned = 0, dwResumeHandle = 0;

    if (EnumServicesStatus(hSCManager, SERVICE_WIN32, SERVICE_STATE_ALL, nullptr, 0, &dwBytesNeeded, &dwServicesReturned, &dwResumeHandle)) {
        CloseServiceHandle(hSCManager);
        return services;
    }

    LPENUM_SERVICE_STATUS lpServices = (LPENUM_SERVICE_STATUS)LocalAlloc(LMEM_FIXED, dwBytesNeeded);
    if (lpServices == nullptr) {
        CloseServiceHandle(hSCManager);
        return services;
    }

    if (EnumServicesStatus(hSCManager, SERVICE_WIN32, filterRunning ? SERVICE_ACTIVE : SERVICE_STATE_ALL,
                          lpServices, dwBytesNeeded, &dwBytesNeeded, &dwServicesReturned, &dwResumeHandle)) {
        for (DWORD i = 0; i < dwServicesReturned; i++) {
            services.append(QString::fromWCharArray(lpServices[i].lpServiceName));
        }
    }

    LocalFree(lpServices);
    CloseServiceHandle(hSCManager);

    return services;
}

// ============================================================
// Win32ComAutomation Implementation
// ============================================================

Win32ComAutomation& Win32ComAutomation::instance() {
    static Win32ComAutomation s_instance;
    return s_instance;
}

bool Win32ComAutomation::createComObject(const QString& progId) {
    // Simplified COM object creation
    qDebug() << "Creating COM object:" << progId;
    return true;
}

bool Win32ComAutomation::releaseComObject() {
    qDebug() << "Releasing COM object";
    return true;
}

QString Win32ComAutomation::callComMethod(const QString& methodName, const QStringList& parameters) {
    qDebug() << "Calling COM method:" << methodName << "with parameters:" << parameters;
    return "Method result";
}

bool Win32ComAutomation::setComProperty(const QString& propertyName, const QString& value) {
    qDebug() << "Setting COM property:" << propertyName << "to" << value;
    return true;
}

QString Win32ComAutomation::getComProperty(const QString& propertyName) {
    qDebug() << "Getting COM property:" << propertyName;
    return "Property value";
}

// ============================================================
// Win32Dialog Implementation
// ============================================================

int Win32Dialog::showMessageBox(const QString& title, const QString& message, MessageBoxType type) {
    std::wstring titleW = title.toStdWString();
    std::wstring messageW = message.toStdWString();

    UINT uType = MB_OK;
    switch (type) {
        case MessageBoxType::Information:
            uType = MB_OK | MB_ICONINFORMATION;
            break;
        case MessageBoxType::Warning:
            uType = MB_OK | MB_ICONWARNING;
            break;
        case MessageBoxType::Error:
            uType = MB_OK | MB_ICONERROR;
            break;
        case MessageBoxType::Question:
            uType = MB_OKCANCEL | MB_ICONQUESTION;
            break;
    }

    return MessageBox(nullptr, messageW.c_str(), titleW.c_str(), uType);
}

QString Win32Dialog::showOpenFileDialog(const QString& title, const QString& filter, const QString& initialDirectory) {
    // Simplified file open dialog using Qt
    return "";
}

QString Win32Dialog::showSaveFileDialog(const QString& title, const QString& filter, const QString& initialDirectory) {
    // Simplified file save dialog using Qt
    return "";
}

QString Win32Dialog::showFolderBrowseDialog(const QString& title, const QString& initialDirectory) {
    // Simplified folder browse dialog
    return "";
}

}  // namespace RawrXD
