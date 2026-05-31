/**
 * @file os_sandbox.cpp
 * @brief OS-Level Security Sandbox Implementation
 */

#include "os_sandbox.h"
#include <psapi.h>
#include <sddl.h>
#include <chrono>
#include <algorithm>
#include <filesystem>

namespace RawrXD::Extensions {

// ============================================================================
// Helpers
// ============================================================================

static uint64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

// ============================================================================
// OSSandbox Implementation
// ============================================================================

OSSandbox::OSSandbox(const OSSandboxConfig& config) : m_config(config) {}

OSSandbox::~OSSandbox() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [_, hJob] : m_jobs) {
        if (hJob) CloseHandle(hJob);
    }
    for (auto& [_, hTok] : m_tokens) {
        if (hTok) CloseHandle(hTok);
    }
}

bool OSSandbox::applyToProcess(int64_t extId, HANDLE hProcess) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove existing sandbox for this extension
    auto itJob = m_jobs.find(extId);
    if (itJob != m_jobs.end() && itJob->second) {
        CloseHandle(itJob->second);
        m_jobs.erase(itJob);
    }
    auto itTok = m_tokens.find(extId);
    if (itTok != m_tokens.end() && itTok->second) {
        CloseHandle(itTok->second);
        m_tokens.erase(itTok);
    }

    // Create job object with limits
    HANDLE hJob = CreateJobObjectA(nullptr, nullptr);
    if (!hJob) return false;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
    jeli.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_JOB_MEMORY |
        JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    jeli.JobMemoryLimit = m_config.maxMemoryBytes;
    jeli.BasicLimitInformation.ActiveProcessLimit = 4;

    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
                                   &jeli, sizeof(jeli))) {
        CloseHandle(hJob);
        return false;
    }

    if (!AssignProcessToJobObject(hJob, hProcess)) {
        CloseHandle(hJob);
        return false;
    }

    // Apply CPU affinity to approximate CPU limit
    if (m_config.maxCpuPercent > 0 && m_config.maxCpuPercent < 100) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        int cores = static_cast<int>(si.dwNumberOfProcessors);
        int allowed = std::max(1, static_cast<int>((cores * m_config.maxCpuPercent) / 100));
        DWORD_PTR mask = 0;
        for (int i = 0; i < allowed; ++i) mask |= (1ULL << i);
        SetProcessAffinityMask(hProcess, mask);
    }

    m_jobs[extId] = hJob;
    return true;
}

bool OSSandbox::removeFromProcess(int64_t extId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto itJob = m_jobs.find(extId);
    if (itJob != m_jobs.end() && itJob->second) {
        CloseHandle(itJob->second);
        m_jobs.erase(itJob);
    }
    auto itTok = m_tokens.find(extId);
    if (itTok != m_tokens.end() && itTok->second) {
        CloseHandle(itTok->second);
        m_tokens.erase(itTok);
    }
    return true;
}

bool OSSandbox::canReadFile(int64_t extId, const std::string& path) const {
    (void)extId;
    return isPathAllowed(path, m_config.allowedReadPaths);
}

bool OSSandbox::canWriteFile(int64_t extId, const std::string& path) const {
    (void)extId;
    return isPathAllowed(path, m_config.allowedWritePaths);
}

bool OSSandbox::canAccessNetwork(int64_t extId, const std::string& host, int port) const {
    (void)extId;
    if (m_config.blockNetwork) return false;
    for (const auto& h : m_config.allowedHosts) {
        if (h == "*" || toLower(h) == toLower(host)) return true;
    }
    for (const auto& p : m_config.allowedPorts) {
        if (p == 0 || p == port) return true;
    }
    return false;
}

bool OSSandbox::canAccessRegistry(int64_t extId) const {
    (void)extId;
    return !m_config.blockRegistry;
}

bool OSSandbox::canCreateProcess(int64_t extId) const {
    (void)extId;
    return !m_config.blockProcessCreation;
}

bool OSSandbox::enforceFileRead(int64_t extId, const std::string& path) {
    if (!canReadFile(extId, path)) {
        logViolation(extId, "fs.read", path);
        return false;
    }
    return true;
}

bool OSSandbox::enforceFileWrite(int64_t extId, const std::string& path) {
    if (!canWriteFile(extId, path)) {
        logViolation(extId, "fs.write", path);
        return false;
    }
    return true;
}

bool OSSandbox::enforceNetworkAccess(int64_t extId, const std::string& host, int port) {
    if (!canAccessNetwork(extId, host, port)) {
        logViolation(extId, "network", host + ":" + std::to_string(port));
        return false;
    }
    return true;
}

bool OSSandbox::setMemoryLimit(int64_t extId, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_jobs.find(extId);
    if (it == m_jobs.end() || !it->second) return false;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
    DWORD retLen = 0;
    if (!QueryInformationJobObject(it->second, JobObjectExtendedLimitInformation,
                                     &jeli, sizeof(jeli), &retLen))
        return false;

    jeli.JobMemoryLimit = bytes;
    jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
    return SetInformationJobObject(it->second, JobObjectExtendedLimitInformation,
                                  &jeli, sizeof(jeli)) != 0;
}

bool OSSandbox::setCpuLimit(int64_t extId, uint32_t percent) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // CPU limit applied via affinity at applyToProcess time
    // For dynamic changes, we'd need to track the process handle
    (void)extId;
    (void)percent;
    return true;
}

std::vector<OSSandbox::Violation> OSSandbox::getViolations(int64_t extId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Violation> result;
    for (const auto& v : m_violations) {
        if (v.extId == extId) result.push_back(v);
    }
    return result;
}

void OSSandbox::clearViolations(int64_t extId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_violations.erase(
        std::remove_if(m_violations.begin(), m_violations.end(),
            [extId](const Violation& v) { return v.extId == extId; }),
        m_violations.end());
}

// ============================================================================
// Internal
// ============================================================================

bool OSSandbox::isPathAllowed(const std::string& path,
                              const std::vector<std::string>& allowed) const {
    try {
        std::filesystem::path fp(path);
        std::filesystem::path canon = std::filesystem::weakly_canonical(fp);
        std::string canonStr = canon.string();
        for (const auto& dir : allowed) {
            std::filesystem::path dp(dir);
            std::filesystem::path dcanon = std::filesystem::weakly_canonical(dp);
            std::string dstr = dcanon.string();
            if (canonStr.find(dstr) == 0) return true;
        }
    } catch (...) {
        return false;
    }
    return false;
}

void OSSandbox::logViolation(int64_t extId, const std::string& op,
                             const std::string& resource) {
    Violation v;
    v.extId = extId;
    v.operation = op;
    v.resource = resource;
    v.timestamp = nowMs();
    m_violations.push_back(v);
    if (m_violations.size() > m_maxViolationLog) {
        m_violations.erase(m_violations.begin());
    }
}

} // namespace RawrXD::Extensions
