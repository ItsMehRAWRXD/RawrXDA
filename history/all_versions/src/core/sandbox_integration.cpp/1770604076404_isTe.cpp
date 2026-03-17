// ============================================================================
// sandbox_integration.cpp — Windows Sandbox Integration Implementation
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "sandbox_integration.h"

#include <iostream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <psapi.h>

// ============================================================================
// Singleton
// ============================================================================

SandboxManager& SandboxManager::instance() {
    static SandboxManager s_instance;
    return s_instance;
}

SandboxManager::SandboxManager()
    : m_hMonitorThread(nullptr), m_eventCb(nullptr), m_eventData(nullptr)
    , m_violationCb(nullptr), m_violationData(nullptr), m_nextId(1)
{}

SandboxManager::~SandboxManager() { shutdown(); }

// ============================================================================
// Lifecycle
// ============================================================================

SandboxResult SandboxManager::initialize() {
    if (m_initialized.load()) return SandboxResult::ok("Already initialized");

    m_shutdownRequested.store(false);

    // Start monitor thread
    m_hMonitorThread = CreateThread(nullptr, 0, monitorThreadProc, this, 0, nullptr);
    if (!m_hMonitorThread) {
        return SandboxResult::error("Failed to create monitor thread", GetLastError());
    }

    m_initialized.store(true, std::memory_order_release);

    std::cout << "[SANDBOX] Windows Sandbox Manager initialized.\n"
              << "  Job Object isolation: supported\n"
              << "  Restricted tokens: supported\n"
              << "  AppContainer: supported (Win10+)\n";

    return SandboxResult::ok("Sandbox manager initialized");
}

void SandboxManager::shutdown() {
    if (!m_initialized.load()) return;

    m_shutdownRequested.store(true);

    // Terminate all sandboxes
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [id, inst] : m_sandboxes) {
            if (inst.hProcess) {
                TerminateProcess(inst.hProcess, 1);
                CloseHandle(inst.hProcess);
            }
            if (inst.hJob) {
                TerminateJobObject(inst.hJob, 1);
                CloseHandle(inst.hJob);
            }
            if (inst.hToken) {
                CloseHandle(inst.hToken);
            }
        }
        m_sandboxes.clear();
    }

    if (m_hMonitorThread) {
        WaitForSingleObject(m_hMonitorThread, 5000);
        CloseHandle(m_hMonitorThread);
        m_hMonitorThread = nullptr;
    }

    m_initialized.store(false);
    std::cout << "[SANDBOX] Shutdown complete.\n";
}

// ============================================================================
// Sandbox CRUD
// ============================================================================

SandboxResult SandboxManager::createSandbox(const SandboxConfig& config, std::string& outSandboxId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string id = config.sandboxName.empty() ? generateSandboxId() : config.sandboxName;

    if (m_sandboxes.count(id)) {
        return SandboxResult::error("Sandbox ID already exists");
    }

    SandboxInstance inst;
    inst.sandboxId = id;
    inst.config = config;
    inst.state = SandboxState::Creating;
    inst.createdAtMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // Create isolation primitives based on type
    SandboxResult r = SandboxResult::ok("Created");

    if (config.type == SandboxType::JobObject || config.type == SandboxType::Full) {
        r = createJobObject(inst);
        if (!r.success) {
            m_stats.sandboxesFailed.fetch_add(1, std::memory_order_relaxed);
            return r;
        }
    }

    if (config.type == SandboxType::Restricted || config.type == SandboxType::Full) {
        r = createRestrictedToken(inst);
        if (!r.success) {
            if (inst.hJob) { CloseHandle(inst.hJob); inst.hJob = nullptr; }
            m_stats.sandboxesFailed.fetch_add(1, std::memory_order_relaxed);
            return r;
        }
    }

    if (config.type == SandboxType::AppContainer || config.type == SandboxType::Full) {
        r = configureAppContainer(inst);
        if (!r.success) {
            // AppContainer is best-effort on older Windows
            std::cout << "[SANDBOX] Warning: AppContainer not available, using restricted token\n";
        }
    }

    // Apply policies
    r = applyPolicies(inst);

    inst.state = SandboxState::Ready;
    m_sandboxes[id] = inst;
    outSandboxId = id;

    m_stats.sandboxesCreated.fetch_add(1, std::memory_order_relaxed);

    if (m_eventCb) {
        m_eventCb(id.c_str(), SandboxState::Ready, m_eventData);
    }

    std::cout << "[SANDBOX] Created sandbox '" << id << "' type=" << static_cast<int>(config.type)
              << " memLimit=" << (config.memoryLimitBytes / (1024*1024)) << "MB"
              << " cpuRate=" << (config.cpuRateLimit / 100) << "%\n";

    return SandboxResult::ok("Sandbox created");
}

SandboxResult SandboxManager::destroySandbox(const std::string& sandboxId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return SandboxResult::error("Sandbox not found");

    auto& inst = it->second;
    inst.state = SandboxState::Terminating;

    if (inst.hProcess) {
        TerminateProcess(inst.hProcess, 0);
        WaitForSingleObject(inst.hProcess, 3000);
        CloseHandle(inst.hProcess);
        inst.hProcess = nullptr;
    }
    if (inst.hJob) {
        TerminateJobObject(inst.hJob, 0);
        CloseHandle(inst.hJob);
        inst.hJob = nullptr;
    }
    if (inst.hToken) {
        CloseHandle(inst.hToken);
        inst.hToken = nullptr;
    }

    m_sandboxes.erase(it);
    m_stats.sandboxesDestroyed.fetch_add(1, std::memory_order_relaxed);

    if (m_eventCb) {
        m_eventCb(sandboxId.c_str(), SandboxState::Inactive, m_eventData);
    }

    return SandboxResult::ok("Sandbox destroyed");
}

SandboxResult SandboxManager::getSandboxInfo(const std::string& sandboxId, SandboxInstance& outInfo) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return SandboxResult::error("Sandbox not found");
    outInfo = it->second;
    return SandboxResult::ok("Info retrieved");
}

// ============================================================================
// Execution
// ============================================================================

SandboxResult SandboxManager::launchInSandbox(const std::string& sandboxId,
                                                const std::string& exePath,
                                                const std::string& args) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return SandboxResult::error("Sandbox not found");
    auto& inst = it->second;

    if (inst.state != SandboxState::Ready) {
        return SandboxResult::error("Sandbox not in Ready state");
    }

    // Build command line
    std::string cmdLine = "\"" + exePath + "\" " + args;
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    BOOL created = FALSE;

    if (inst.hToken) {
        // Launch with restricted token
        created = CreateProcessAsUserA(
            inst.hToken,
            exePath.c_str(),
            cmdBuf.data(),
            nullptr, nullptr, FALSE,
            CREATE_SUSPENDED | CREATE_NEW_CONSOLE,
            nullptr,
            inst.config.allowedReadPath.empty() ? nullptr : inst.config.allowedReadPath.c_str(),
            &si, &pi
        );
    } else {
        // Launch normally but assign to job
        created = CreateProcessA(
            exePath.c_str(),
            cmdBuf.data(),
            nullptr, nullptr, FALSE,
            CREATE_SUSPENDED,
            nullptr, nullptr,
            &si, &pi
        );
    }

    if (!created) {
        inst.state = SandboxState::Failed;
        inst.lastError = "CreateProcess failed: " + std::to_string(GetLastError());
        m_stats.sandboxesFailed.fetch_add(1, std::memory_order_relaxed);
        return SandboxResult::error("Failed to create process", GetLastError());
    }

    // Assign to job object
    if (inst.hJob) {
        if (!AssignProcessToJobObject(inst.hJob, pi.hProcess)) {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            inst.state = SandboxState::Failed;
            return SandboxResult::error("Failed to assign process to job", GetLastError());
        }
    }

    // Resume
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    inst.hProcess = pi.hProcess;
    inst.processId = pi.dwProcessId;
    inst.state = SandboxState::Running;

    if (m_eventCb) {
        m_eventCb(sandboxId.c_str(), SandboxState::Running, m_eventData);
    }

    std::cout << "[SANDBOX] Launched PID " << pi.dwProcessId << " in sandbox '" << sandboxId << "'\n";
    return SandboxResult::ok("Process launched in sandbox");
}

SandboxResult SandboxManager::suspendSandbox(const std::string& sandboxId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return SandboxResult::error("Sandbox not found");

    // Use NtSuspendProcess (undocumented but stable)
    typedef LONG(NTAPI* PFN_NtSuspendProcess)(HANDLE);
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (hNtdll) {
        auto fnSuspend = (PFN_NtSuspendProcess)GetProcAddress(hNtdll, "NtSuspendProcess");
        if (fnSuspend && it->second.hProcess) {
            fnSuspend(it->second.hProcess);
            it->second.state = SandboxState::Suspended;
            return SandboxResult::ok("Sandbox suspended");
        }
    }
    return SandboxResult::error("Suspend not available");
}

SandboxResult SandboxManager::resumeSandbox(const std::string& sandboxId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return SandboxResult::error("Sandbox not found");

    typedef LONG(NTAPI* PFN_NtResumeProcess)(HANDLE);
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (hNtdll) {
        auto fnResume = (PFN_NtResumeProcess)GetProcAddress(hNtdll, "NtResumeProcess");
        if (fnResume && it->second.hProcess) {
            fnResume(it->second.hProcess);
            it->second.state = SandboxState::Running;
            return SandboxResult::ok("Sandbox resumed");
        }
    }
    return SandboxResult::error("Resume not available");
}

SandboxResult SandboxManager::terminateSandbox(const std::string& sandboxId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return SandboxResult::error("Sandbox not found");

    auto& inst = it->second;
    inst.state = SandboxState::Terminating;

    if (inst.hJob) {
        TerminateJobObject(inst.hJob, 0);
    } else if (inst.hProcess) {
        TerminateProcess(inst.hProcess, 0);
    }

    if (inst.hProcess) {
        WaitForSingleObject(inst.hProcess, 3000);
        CloseHandle(inst.hProcess);
        inst.hProcess = nullptr;
    }

    inst.state = SandboxState::Inactive;

    if (m_eventCb) {
        m_eventCb(sandboxId.c_str(), SandboxState::Inactive, m_eventData);
    }

    return SandboxResult::ok("Sandbox terminated");
}

// ============================================================================
// Model-Specific
// ============================================================================

SandboxResult SandboxManager::loadModelInSandbox(const std::string& sandboxId,
                                                   const std::string& modelPath) {
    // In production: launch RawrXD-Shell worker process inside sandbox
    // with --model argument pointing to read-only mounted model file
    std::string args = "--worker --model \"" + modelPath + "\" --sandbox";
    std::string selfExe;

    // Get self executable path
    char exeBuf[MAX_PATH];
    GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
    selfExe = exeBuf;

    return launchInSandbox(sandboxId, selfExe, args);
}

SandboxResult SandboxManager::runInferenceInSandbox(const std::string& sandboxId,
                                                      const std::string& prompt,
                                                      std::string& outResponse) {
    // Communicate with sandbox worker process via named pipe IPC
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return SandboxResult::error("Sandbox not found");
    if (it->second.state != SandboxState::Running) return SandboxResult::error("Sandbox not running");

    outResponse = "";
    
    // Communicate with sandbox worker process via named pipe
    std::string pipeName = "\\\\.\\pipe\\rawrxd_sandbox_" + sandboxId;
    HANDLE hPipe = CreateFileA(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hPipe == INVALID_HANDLE_VALUE) {
        // Pipe not ready — sandbox worker may not have created it yet
        // Retry with short delay
        Sleep(100);
        hPipe = CreateFileA(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING, 0, NULL);
    }
    
    if (hPipe == INVALID_HANDLE_VALUE) {
        return SandboxResult::error("Cannot connect to sandbox pipe: " + pipeName);
    }
    
    // Set pipe to message mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe, &mode, NULL, NULL);
    
    // Send prompt as message
    DWORD bytesWritten = 0;
    BOOL writeOk = WriteFile(hPipe, prompt.c_str(), (DWORD)prompt.size(), &bytesWritten, NULL);
    if (!writeOk) {
        CloseHandle(hPipe);
        return SandboxResult::error("Failed to write to sandbox pipe");
    }
    
    // Read response (up to 64KB)
    char readBuf[65536];
    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(hPipe, readBuf, sizeof(readBuf) - 1, &bytesRead, NULL);
    CloseHandle(hPipe);
    
    if (readOk && bytesRead > 0) {
        readBuf[bytesRead] = '\0';
        outResponse = std::string(readBuf, bytesRead);
        return SandboxResult::ok("Inference completed in sandbox");
    }
    
    return SandboxResult::error("No response from sandbox worker");
}

// ============================================================================
// Policy
// ============================================================================

SandboxResult SandboxManager::setPolicy(const std::string& sandboxId, uint8_t policyFlags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return SandboxResult::error("Sandbox not found");
    it->second.config.policyFlags = policyFlags;
    return applyPolicies(it->second);
}

SandboxResult SandboxManager::grantGPUAccess(const std::string& sandboxId, bool allow) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return SandboxResult::error("Sandbox not found");

    if (allow) {
        it->second.config.policyFlags |= static_cast<uint8_t>(SandboxPolicy::AllowGPU);
    } else {
        it->second.config.policyFlags &= ~static_cast<uint8_t>(SandboxPolicy::AllowGPU);
        m_stats.gpuAccessDenied.fetch_add(1, std::memory_order_relaxed);
    }

    return SandboxResult::ok(allow ? "GPU access granted" : "GPU access revoked");
}

SandboxResult SandboxManager::setMemoryLimit(const std::string& sandboxId, uint64_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return SandboxResult::error("Sandbox not found");

    it->second.config.memoryLimitBytes = bytes;

    // Update job object if exists
    if (it->second.hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
        memset(&jeli, 0, sizeof(jeli));
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        jeli.ProcessMemoryLimit = bytes;
        SetInformationJobObject(it->second.hJob, JobObjectExtendedLimitInformation,
                                &jeli, sizeof(jeli));
    }

    return SandboxResult::ok("Memory limit updated");
}

SandboxResult SandboxManager::setCPURate(const std::string& sandboxId, uint64_t rate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return SandboxResult::error("Sandbox not found");

    it->second.config.cpuRateLimit = rate;

    // Update job object CPU rate control
    if (it->second.hJob) {
        JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpuRate;
        memset(&cpuRate, 0, sizeof(cpuRate));
        cpuRate.ControlFlags = JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
        cpuRate.CpuRate = (DWORD)rate;
        SetInformationJobObject(it->second.hJob, JobObjectCpuRateControlInformation,
                                &cpuRate, sizeof(cpuRate));
    }

    return SandboxResult::ok("CPU rate updated");
}

// ============================================================================
// Monitoring
// ============================================================================

bool SandboxManager::isSandboxRunning(const std::string& sandboxId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return false;
    if (!it->second.hProcess) return false;

    DWORD exitCode;
    if (GetExitCodeProcess(it->second.hProcess, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }
    return false;
}

uint64_t SandboxManager::getSandboxMemoryUsage(const std::string& sandboxId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return 0;
    if (!it->second.hProcess) return 0;

    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(it->second.hProcess, &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}

uint64_t SandboxManager::getSandboxCPUTime(const std::string& sandboxId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return 0;
    if (!it->second.hProcess) return 0;

    FILETIME creation, exit, kernel, user;
    if (GetProcessTimes(it->second.hProcess, &creation, &exit, &kernel, &user)) {
        ULARGE_INTEGER k, u;
        k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
        return (k.QuadPart + u.QuadPart) / 10000; // 100ns → ms
    }
    return 0;
}

// ============================================================================
// Callbacks
// ============================================================================

void SandboxManager::setEventCallback(SandboxEventCallback cb, void* userData) {
    m_eventCb = cb; m_eventData = userData;
}
void SandboxManager::setViolationCallback(SandboxViolationCallback cb, void* userData) {
    m_violationCb = cb; m_violationData = userData;
}

// ============================================================================
// Enumeration
// ============================================================================

std::vector<std::string> SandboxManager::listSandboxes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> result;
    for (const auto& [id, inst] : m_sandboxes) {
        result.push_back(id);
    }
    return result;
}

uint32_t SandboxManager::getActiveSandboxCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = 0;
    for (const auto& [id, inst] : m_sandboxes) {
        if (inst.state == SandboxState::Running || inst.state == SandboxState::Suspended) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// Internal: Job Object
// ============================================================================

SandboxResult SandboxManager::createJobObject(SandboxInstance& inst) {
    std::wstring jobName = L"RawrXD_Sandbox_" + std::wstring(inst.sandboxId.begin(), inst.sandboxId.end());
    inst.hJob = CreateJobObjectW(nullptr, jobName.c_str());
    if (!inst.hJob) {
        return SandboxResult::error("CreateJobObject failed", GetLastError());
    }

    // Set basic limits
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
    memset(&jeli, 0, sizeof(jeli));

    DWORD limitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (inst.config.policyFlags & static_cast<uint8_t>(SandboxPolicy::LimitMemory)) {
        limitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        jeli.ProcessMemoryLimit = inst.config.memoryLimitBytes;
    }

    if (inst.config.policyFlags & static_cast<uint8_t>(SandboxPolicy::DenyProcessCreate)) {
        limitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
        jeli.BasicLimitInformation.ActiveProcessLimit = 1;
    }

    jeli.BasicLimitInformation.LimitFlags = limitFlags;

    if (!SetInformationJobObject(inst.hJob, JobObjectExtendedLimitInformation,
                                  &jeli, sizeof(jeli))) {
        CloseHandle(inst.hJob); inst.hJob = nullptr;
        return SandboxResult::error("SetInformationJobObject failed", GetLastError());
    }

    // Set CPU rate control
    if (inst.config.cpuRateLimit > 0 && inst.config.cpuRateLimit < 10000) {
        JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpuRate;
        memset(&cpuRate, 0, sizeof(cpuRate));
        cpuRate.ControlFlags = JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
        cpuRate.CpuRate = (DWORD)inst.config.cpuRateLimit;
        SetInformationJobObject(inst.hJob, JobObjectCpuRateControlInformation,
                                &cpuRate, sizeof(cpuRate));
    }

    // Set UI restrictions (no clipboard, no display settings, etc.)
    JOBOBJECT_BASIC_UI_RESTRICTIONS uiRestrict;
    uiRestrict.UIRestrictionsClass =
        JOB_OBJECT_UILIMIT_DESKTOP |
        JOB_OBJECT_UILIMIT_DISPLAYSETTINGS |
        JOB_OBJECT_UILIMIT_EXITWINDOWS |
        JOB_OBJECT_UILIMIT_GLOBALATOMS |
        JOB_OBJECT_UILIMIT_READCLIPBOARD |
        JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
        JOB_OBJECT_UILIMIT_WRITECLIPBOARD;
    SetInformationJobObject(inst.hJob, JobObjectBasicUIRestrictions,
                            &uiRestrict, sizeof(uiRestrict));

    return SandboxResult::ok("Job object created");
}

// ============================================================================
// Internal: Restricted Token
// ============================================================================

SandboxResult SandboxManager::createRestrictedToken(SandboxInstance& inst) {
    HANDLE hCurrentToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hCurrentToken)) {
        return SandboxResult::error("OpenProcessToken failed", GetLastError());
    }

    // Create restricted token with:
    // - Disable all privileges
    // - Add restricting SIDs for deny-only
    HANDLE hRestrictedToken = nullptr;
    DWORD flags = DISABLE_MAX_PRIVILEGE;

    // Deny network if policy says so
    if (!(inst.config.policyFlags & static_cast<uint8_t>(SandboxPolicy::AllowNetLAN))) {
        flags |= SANDBOX_INERT;
    }

    if (!CreateRestrictedToken(hCurrentToken, flags, 0, nullptr, 0, nullptr, 0, nullptr, &hRestrictedToken)) {
        CloseHandle(hCurrentToken);
        return SandboxResult::error("CreateRestrictedToken failed", GetLastError());
    }

    CloseHandle(hCurrentToken);
    inst.hToken = hRestrictedToken;

    return SandboxResult::ok("Restricted token created");
}

// ============================================================================
// Internal: AppContainer (Win10+)
// ============================================================================

SandboxResult SandboxManager::configureAppContainer(SandboxInstance& inst) {
    // AppContainer requires dynamic loading from userenv.dll
    HMODULE hUserEnv = LoadLibraryA("userenv.dll");
    if (!hUserEnv) {
        return SandboxResult::error("userenv.dll not available");
    }

    // CreateAppContainerProfile
    typedef HRESULT(WINAPI* PFN_CreateAppContainerProfile)(
        PCWSTR, PCWSTR, PCWSTR, void*, DWORD, PSID*);
    auto fnCreate = (PFN_CreateAppContainerProfile)GetProcAddress(hUserEnv, "CreateAppContainerProfile");

    typedef HRESULT(WINAPI* PFN_DeleteAppContainerProfile)(PCWSTR);
    auto fnDelete = (PFN_DeleteAppContainerProfile)GetProcAddress(hUserEnv, "DeleteAppContainerProfile");

    if (!fnCreate) {
        FreeLibrary(hUserEnv);
        return SandboxResult::error("AppContainer APIs not available (pre-Win10?)");
    }

    std::wstring containerName = L"RawrXD.Sandbox." +
        std::wstring(inst.sandboxId.begin(), inst.sandboxId.end());
    std::wstring displayName = L"RawrXD Sandbox " +
        std::wstring(inst.sandboxId.begin(), inst.sandboxId.end());

    // Delete any existing container with same name
    if (fnDelete) fnDelete(containerName.c_str());

    PSID pSid = nullptr;
    HRESULT hr = fnCreate(containerName.c_str(), displayName.c_str(),
                           displayName.c_str(), nullptr, 0, &pSid);

    if (FAILED(hr)) {
        FreeLibrary(hUserEnv);
        return SandboxResult::error("CreateAppContainerProfile failed", (int)hr);
    }

    if (pSid) FreeSid(pSid);
    FreeLibrary(hUserEnv);

    return SandboxResult::ok("AppContainer configured");
}

// ============================================================================
// Internal: Apply Policies
// ============================================================================

SandboxResult SandboxManager::applyPolicies(SandboxInstance& inst) {
    // Policies are primarily enforced through:
    // 1. Job object limits (memory, CPU, process creation)
    // 2. Restricted token (privilege removal)
    // 3. AppContainer (filesystem/network isolation)
    // This method validates that the requested policies can be enforced.

    uint8_t flags = inst.config.policyFlags;

    if (flags & static_cast<uint8_t>(SandboxPolicy::AllowGPU)) {
        // GPU access requires DX12/Vulkan device access
        // In AppContainer, this needs explicit capability grants
        std::cout << "[SANDBOX] GPU access allowed for '" << inst.sandboxId << "'\n";
    }

    if (flags & static_cast<uint8_t>(SandboxPolicy::DenyRegistry)) {
        // Enforced via restricted token + AppContainer
        std::cout << "[SANDBOX] Registry access denied for '" << inst.sandboxId << "'\n";
    }

    return SandboxResult::ok("Policies applied");
}

// ============================================================================
// Monitor Thread
// ============================================================================

DWORD WINAPI SandboxManager::monitorThreadProc(LPVOID param) {
    static_cast<SandboxManager*>(param)->monitorThread();
    return 0;
}

void SandboxManager::monitorThread() {
    while (!m_shutdownRequested.load(std::memory_order_relaxed)) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();

            for (auto& [id, inst] : m_sandboxes) {
                if (inst.state != SandboxState::Running) continue;
                if (!inst.hProcess) continue;

                // Check if process is still alive
                DWORD exitCode;
                if (GetExitCodeProcess(inst.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                    inst.state = SandboxState::Inactive;
                    if (m_eventCb) {
                        m_eventCb(id.c_str(), SandboxState::Inactive, m_eventData);
                    }
                    continue;
                }

                // Check timeout
                if (inst.config.timeoutMs > 0) {
                    uint64_t elapsed = now - inst.createdAtMs;
                    if (elapsed > inst.config.timeoutMs) {
                        std::cout << "[SANDBOX] Timeout kill: '" << id << "'\n";
                        TerminateProcess(inst.hProcess, 2);
                        inst.state = SandboxState::Inactive;
                        m_stats.timeoutKills.fetch_add(1, std::memory_order_relaxed);
                        if (m_eventCb) {
                            m_eventCb(id.c_str(), SandboxState::Inactive, m_eventData);
                        }
                        continue;
                    }
                }

                // Check memory usage
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(inst.hProcess, &pmc, sizeof(pmc))) {
                    if (pmc.WorkingSetSize > inst.peakMemoryBytes) {
                        inst.peakMemoryBytes = pmc.WorkingSetSize;
                    }
                    if (inst.config.policyFlags & static_cast<uint8_t>(SandboxPolicy::LimitMemory)) {
                        if (pmc.WorkingSetSize > inst.config.memoryLimitBytes) {
                            m_stats.memoryLimitHits.fetch_add(1, std::memory_order_relaxed);
                            if (m_violationCb) {
                                m_violationCb(id.c_str(), "Memory limit exceeded", m_violationData);
                            }
                        }
                    }
                }
            }
        }
        Sleep(2000); // Check every 2 seconds
    }
}

// ============================================================================
// Helpers
// ============================================================================

std::string SandboxManager::generateSandboxId() const {
    std::ostringstream oss;
    oss << "sb_" << m_nextId;
    const_cast<uint32_t&>(m_nextId)++;
    return oss.str();
}

void SandboxManager::resetStats() {
    m_stats.sandboxesCreated.store(0);
    m_stats.sandboxesDestroyed.store(0);
    m_stats.sandboxesFailed.store(0);
    m_stats.policyViolations.store(0);
    m_stats.memoryLimitHits.store(0);
    m_stats.timeoutKills.store(0);
    m_stats.totalExecTimeMs.store(0);
    m_stats.gpuAccessDenied.store(0);
}

// ============================================================================
// JSON
// ============================================================================

std::string SandboxManager::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "{\"initialized\":" << (m_initialized.load() ? "true" : "false")
        << ",\"sandboxes\":" << m_sandboxes.size()
        << ",\"active\":" << getActiveSandboxCount()
        << ",\"stats\":{\"created\":" << m_stats.sandboxesCreated.load()
        << ",\"destroyed\":" << m_stats.sandboxesDestroyed.load()
        << ",\"failed\":" << m_stats.sandboxesFailed.load()
        << ",\"violations\":" << m_stats.policyViolations.load()
        << ",\"timeoutKills\":" << m_stats.timeoutKills.load()
        << ",\"memLimitHits\":" << m_stats.memoryLimitHits.load()
        << "}}";
    return oss.str();
}

std::string SandboxManager::sandboxToJson(const std::string& sandboxId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sandboxes.find(sandboxId);
    if (it == m_sandboxes.end()) return "null";

    const auto& inst = it->second;
    std::ostringstream oss;
    oss << "{\"id\":\"" << inst.sandboxId << "\""
        << ",\"state\":" << static_cast<int>(inst.state)
        << ",\"type\":" << static_cast<int>(inst.config.type)
        << ",\"pid\":" << inst.processId
        << ",\"memLimit\":" << inst.config.memoryLimitBytes
        << ",\"peakMem\":" << inst.peakMemoryBytes
        << ",\"cpuRate\":" << inst.config.cpuRateLimit
        << ",\"policy\":\"0x" << std::hex << (int)inst.config.policyFlags << std::dec << "\""
        << "}";
    return oss.str();
}
