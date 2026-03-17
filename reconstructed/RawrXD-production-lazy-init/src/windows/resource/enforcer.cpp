/**
 * @file windows_resource_enforcer.cpp
 * @brief Windows Job Objects implementation for resource enforcement
 */

#include "windows_resource_enforcer.h"
#include <QTimer>
#include <QDebug>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <winnt.h>
#include <processthreadsapi.h>
#include <jobapi.h>
#include <jobapi2.h>
#include <psapi.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "psapi.lib")

// ============================================================================
// WindowsJobObject Implementation
// ============================================================================

WindowsJobObject::WindowsJobObject(const QString& jobName)
    : m_jobHandle(nullptr), m_jobName(jobName)
{
}

WindowsJobObject::~WindowsJobObject()
{
    close();
}

bool WindowsJobObject::create()
{
    // Create or open job object
    m_jobHandle = CreateJobObjectW(nullptr, (LPCWSTR)m_jobName.utf16());
    
    if (m_jobHandle == nullptr) {
        qWarning() << "[WindowsJobObject] Failed to create job:" << GetLastError();
        return false;
    }
    
    qInfo() << "[WindowsJobObject] Created job:" << m_jobName;
    return true;
}

bool WindowsJobObject::assignProcess(DWORD processId)
{
    HANDLE processHandle = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, processId);
    if (processHandle == nullptr) {
        qWarning() << "[WindowsJobObject] Failed to open process" << processId;
        return false;
    }
    
    bool result = assignProcessHandle(processHandle);
    CloseHandle(processHandle);
    return result;
}

bool WindowsJobObject::assignProcessHandle(HANDLE processHandle)
{
    if (!m_jobHandle) {
        qWarning() << "[WindowsJobObject] Job not created";
        return false;
    }
    
    if (!AssignProcessToJobObject(m_jobHandle, processHandle)) {
        qWarning() << "[WindowsJobObject] Failed to assign process to job:" << GetLastError();
        return false;
    }
    
    qInfo() << "[WindowsJobObject] Assigned process to job";
    return true;
}

void WindowsJobObject::close()
{
    if (m_jobHandle) {
        CloseHandle(m_jobHandle);
        m_jobHandle = nullptr;
    }
}

void WindowsJobObject::setResourceLimits(const ResourceLimits& limits)
{
    m_currentLimits = limits;
    
    if (configureBasicLimits(limits)) {
        qInfo() << "[WindowsJobObject] Basic limits configured";
    }
    
    if (configureExtendedLimits(limits)) {
        qInfo() << "[WindowsJobObject] Extended limits configured";
    }
    
    if (limits.enforceCpuLimit) {
        if (configureNotificationLimit(limits)) {
            qInfo() << "[WindowsJobObject] CPU notification limit configured";
        }
    }
}

void WindowsJobObject::setMemoryLimit(uint64_t limitMB)
{
    m_currentLimits.workingSetLimitMB = limitMB;
    setResourceLimits(m_currentLimits);
}

void WindowsJobObject::setCpuAffinity(int affinityMask)
{
    m_currentLimits.cpuAffinity = affinityMask;
    // Would require extending job object configuration
}

void WindowsJobObject::setPriorityClass(int priorityClass)
{
    m_currentLimits.priorityClass = priorityClass;
    // Would be applied at process level
}

WindowsJobObject::JobStats WindowsJobObject::getStatistics() const
{
    JobStats stats = {};
    
    if (!m_jobHandle) {
        return stats;
    }
    
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION basicInfo = {};
    if (QueryInformationJobObject(m_jobHandle, JobObjectBasicAccountingInformation,
                                  &basicInfo, sizeof(basicInfo), nullptr)) {
        stats.totalProcesses = basicInfo.TotalProcesses;
        stats.peakProcessCount = basicInfo.ThisPeriodTotalProcesses;
        stats.totalUserTime = basicInfo.TotalUserTime.QuadPart;
        stats.totalKernelTime = basicInfo.TotalKernelTime.QuadPart;
    }
    
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION extInfo = {};
    if (QueryInformationJobObject(m_jobHandle, JobObjectExtendedLimitInformation,
                                  &extInfo, sizeof(extInfo), nullptr)) {
        stats.peakWorkingSetBytes = extInfo.PeakProcessMemoryUsed.QuadPart;
    }
    
    return stats;
}

bool WindowsJobObject::isValid() const
{
    return m_jobHandle != nullptr;
}

bool WindowsJobObject::configureBasicLimits(const ResourceLimits& limits)
{
    if (!m_jobHandle) return false;
    
    JOBOBJECT_BASIC_LIMIT_INFORMATION basicInfo = {};
    basicInfo.LimitFlags = 0;
    
    if (limits.enforceMemoryLimit) {
        basicInfo.LimitFlags |= JOB_OBJECT_LIMIT_WORKINGSET;
        basicInfo.MinimumWorkingSetSize = 1024 * 1024;  // 1 MB minimum
        basicInfo.MaximumWorkingSetSize = limits.workingSetLimitMB * 1024 * 1024;
    }
    
    if (limits.maxProcessCount > 0) {
        basicInfo.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
        basicInfo.ActiveProcessLimit = limits.maxProcessCount;
    }
    
    if (limits.priorityClass > 0) {
        basicInfo.LimitFlags |= JOB_OBJECT_LIMIT_PRIORITY_CLASS;
        basicInfo.PriorityClass = limits.priorityClass;
    }
    
    basicInfo.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_TIME;
    basicInfo.PerProcessUserTimeLimit.QuadPart = 0; // No per-process time limit
    
    return SetInformationJobObject(m_jobHandle, JobObjectBasicLimitInformation,
                                   &basicInfo, sizeof(basicInfo));
}

bool WindowsJobObject::configureExtendedLimits(const ResourceLimits& limits)
{
    if (!m_jobHandle) return false;
    
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION extInfo = {};
    extInfo.BasicLimitInformation = {};
    extInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY;
    extInfo.JobMemoryLimit = limits.commitLimitMB * 1024 * 1024;
    
    // Optional: die on limit exceeded
    if (limits.terminateOnLimitExceeded) {
        extInfo.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    }
    
    return SetInformationJobObject(m_jobHandle, JobObjectExtendedLimitInformation,
                                   &extInfo, sizeof(extInfo));
}

bool WindowsJobObject::configureNotificationLimit(const ResourceLimits& limits)
{
    if (!m_jobHandle) return false;
    
    // Configure notification when CPU limit is approaching
    JOBOBJECT_NOTIFICATION_LIMIT_INFORMATION notifyInfo = {};
    notifyInfo.JobMemoryLimit = limits.commitLimitMB * 1024 * 1024 * 90 / 100; // 90% threshold
    
    return SetInformationJobObject(m_jobHandle, JobObjectNotificationLimitInformation,
                                   &notifyInfo, sizeof(notifyInfo));
}

// ============================================================================
// ProcessResourceManager Implementation
// ============================================================================

ProcessResourceManager::ProcessResourceManager(QObject* parent)
    : QObject(parent)
{
    // Start monitoring timer (runs every 5 seconds)
    m_monitoringTimer = new QTimer(this);
    connect(m_monitoringTimer, &QTimer::timeout, this, &ProcessResourceManager::onMonitoringTimer);
    m_monitoringTimer->start(5000);
    
    qInfo() << "[ProcessResourceManager] Initialized";
}

ProcessResourceManager::~ProcessResourceManager()
{
    if (m_monitoringTimer) {
        m_monitoringTimer->stop();
    }
    
    // Cleanup all processes
    QMutexLocker locker(&m_mutex);
    for (auto& entry : m_processes) {
        if (entry.jobObject) {
            entry.jobObject->close();
        }
    }
}

QString ProcessResourceManager::createSandboxedProcess(const QString& exePath, const QString& args,
                                                      const ResourceLimits& limits)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateResourceLimits(limits)) {
        m_lastError = "Invalid resource limits";
        return "";
    }
    
    // Create job object
    QString jobName = "JOB_" + QUuid::createUuid().toString(QUuid::WithoutBraces).mid(0, 16);
    auto jobObject = std::make_shared<WindowsJobObject>(jobName);
    
    if (!jobObject->create()) {
        m_lastError = "Failed to create job object";
        return "";
    }
    
    jobObject->setResourceLimits(limits);
    
    // Create process info structure
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    
    PROCESS_INFORMATION pi = {};
    
    // Build command line
    QString cmdLine = "\"" + exePath + "\"";
    if (!args.isEmpty()) {
        cmdLine += " " + args;
    }
    
    // Create the process
    if (!CreateProcessW((LPCWSTR)exePath.utf16(), (LPWSTR)cmdLine.utf16(),
                        nullptr, nullptr, FALSE, CREATE_SUSPENDED,
                        nullptr, nullptr, &si, &pi)) {
        m_lastError = QString("Failed to create process: %1").arg(GetLastError());
        return "";
    }
    
    // Assign to job object
    if (!jobObject->assignProcessHandle(pi.hProcess)) {
        m_lastError = "Failed to assign process to job";
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return "";
    }
    
    // Resume the process
    ResumeThread(pi.hThread);
    
    // Store process entry
    ProcessEntry entry;
    entry.jobId = jobName;
    entry.jobObject = jobObject;
    entry.processId = pi.dwProcessId;
    entry.creationTime = QDateTime::currentDateTime();
    entry.limits = limits;
    
    m_processes[jobName] = entry;
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    qInfo() << "[ProcessResourceManager] Created sandboxed process:" << jobName;
    return jobName;
}

bool ProcessResourceManager::terminateProcess(const QString& jobId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_processes.find(jobId);
    if (it == m_processes.end()) {
        m_lastError = "Process not found";
        return false;
    }
    
    if (it->jobObject) {
        // Close the job (terminates all associated processes)
        it->jobObject->close();
    }
    
    m_processes.erase(it);
    emit processTerminated(jobId);
    
    qInfo() << "[ProcessResourceManager] Terminated process:" << jobId;
    return true;
}

bool ProcessResourceManager::updateLimits(const QString& jobId, const ResourceLimits& limits)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_processes.find(jobId);
    if (it == m_processes.end()) {
        m_lastError = "Process not found";
        return false;
    }
    
    it->limits = limits;
    if (it->jobObject) {
        it->jobObject->setResourceLimits(limits);
    }
    
    return true;
}

QJsonObject ProcessResourceManager::getProcessStats(const QString& jobId) const
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_processes.find(jobId);
    if (it == m_processes.end()) {
        return QJsonObject();
    }
    
    QJsonObject stats;
    stats["jobId"] = jobId;
    stats["processId"] = static_cast<int>(it->processId);
    stats["creationTime"] = it->creationTime.toString(Qt::ISODate);
    
    if (it->jobObject) {
        auto jobStats = it->jobObject->getStatistics();
        stats["totalProcesses"] = static_cast<int>(jobStats.totalProcesses);
        stats["peakProcessCount"] = static_cast<int>(jobStats.peakProcessCount);
        stats["totalUserTime"] = static_cast<qint64>(jobStats.totalUserTime);
        stats["totalKernelTime"] = static_cast<qint64>(jobStats.totalKernelTime);
        stats["peakWorkingSetMB"] = static_cast<qint64>(jobStats.peakWorkingSetBytes / (1024 * 1024));
    }
    
    return stats;
}

QJsonArray ProcessResourceManager::getAllProcessStats() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonArray result;
    for (auto it = m_processes.begin(); it != m_processes.end(); ++it) {
        result.append(getProcessStats(it.key()));
    }
    return result;
}

bool ProcessResourceManager::validateResourceLimits(const ResourceLimits& limits) const
{
    if (limits.workingSetLimitMB < 64) {
        m_lastError = "Memory limit too low (minimum 64 MB)";
        return false;
    }
    
    if (limits.workingSetLimitMB > limits.commitLimitMB) {
        m_lastError = "Working set limit exceeds commit limit";
        return false;
    }
    
    return true;
}

void ProcessResourceManager::onMonitoringTimer()
{
    monitorProcesses();
}

void ProcessResourceManager::monitorProcesses()
{
    QMutexLocker locker(&m_mutex);
    
    for (auto it = m_processes.begin(); it != m_processes.end(); ) {
        if (it->jobObject && it->jobObject->isValid()) {
            auto stats = it->jobObject->getStatistics();
            
            // Check if process is still running
            HANDLE processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, it->processId);
            if (processHandle == nullptr) {
                qInfo() << "[ProcessResourceManager] Process" << it.key() << "no longer running";
                emit processTerminated(it.key());
                it = m_processes.erase(it);
                continue;
            }
            
            DWORD exitCode = 0;
            GetExitCodeProcess(processHandle, &exitCode);
            CloseHandle(processHandle);
            
            if (exitCode != STILL_ACTIVE) {
                qInfo() << "[ProcessResourceManager] Process" << it.key() << "terminated with code" << exitCode;
                emit processTerminated(it.key());
                it = m_processes.erase(it);
                continue;
            }
        }
        ++it;
    }
}
