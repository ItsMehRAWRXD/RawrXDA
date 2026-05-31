/**
 * @file process_broker.cpp
 * @brief MASM64 Extension Process Broker — C++ implementation
 *
 * Spawns extensions in separate processes, manages named-pipe IPC,
 * enforces resource quotas, and monitors process health.
 */

#include "process_broker.h"
#include <psapi.h>
#include <processthreadsapi.h>
#include <jobapi2.h>
#include <chrono>
#include <algorithm>

namespace RawrXD::Extensions {

// ============================================================================
// Helpers
// ============================================================================

static uint32_t crc32(const uint8_t* data, size_t len) {
    static const uint32_t table[256] = {
        0x00000000,0x77073096,0xee0e612c,0x990951ba,0x076dc419,0x706af48f,
        0xe963a535,0x9e6495a3,0x0edb8832,0x79dcb8a4,0xe0d5e91e,0x97d2d988,
        0x09b64c2b,0x7eb17cbd,0xe7b82d07,0x90bf1d91,0x1db71064,0x6ab020f2,
        0xf3b97148,0x84be41de,0x1adad47d,0x6ddde4eb,0xf4d4b551,0x83d385c7,
        0x136c9856,0x646ba8c0,0xfd62f97a,0x8a65c9ec,0x14015c4f,0x63066cd9,
        0xfa0f3d63,0x8d080df5,0x3b6e20c8,0x4c69105e,0xd56041e4,0xa2677172,
        0x3c03e4d1,0x4b04d447,0xd20d85fd,0xa50ab56b,0x35b5a8fa,0x42b2986c,
        0xdbbbc9d6,0xacbcf940,0x32d86ce3,0x45df5c75,0xdcd60dcf,0xabd13d59,
        0x26d930ac,0x51de003a,0xc8d75180,0xbfd06116,0x21b4f4b5,0x56b3c423,
        0xcfba9599,0xb8bda50f,0x2802b89e,0x5f058808,0xc60cd9b2,0xb10be924,
        0x2f6f7c87,0x58684c11,0xc1611dab,0xb6662d3d,0x76dc4190,0x01db7106,
        0x98d220bc,0xefd5102a,0x71b18589,0x06b6b51f,0x9fbfe4a5,0xe8b8d433,
        0x7807c9a2,0x0f00f934,0x9609a88e,0xe10e9818,0x7f6a0dbb,0x086d3d2d,
        0x91646c97,0xe6635c01,0x6b6b51f4,0x1c6c6162,0x856530d8,0xf262004e,
        0x6c0695ed,0x1b01a57b,0x8208f4c1,0xf50fc457,0x65b0d9c6,0x12b7e950,
        0x8bbeb8ea,0xfcb9887c,0x62dd1ddf,0x15da2d49,0x8cd37cf3,0xfbd44c65,
        0x4db26158,0x3ab551ce,0xa3bc0074,0xd4bb30e2,0x4adfa541,0x3dd895d7,
        0xa4d1c46d,0xd3d6f4fb,0x4369e96a,0x346ed9fc,0xad678846,0xda60b8d0,
        0x44042d73,0x33031de5,0xaa0a4c5f,0xdd0d7cc9,0x5005713c,0x270241aa,
        0xbe0b1010,0xc90c2086,0x5768b525,0x206f85b3,0xb966d409,0xce61e49f,
        0x5edef90e,0x29d9c998,0xb0d09822,0xc7d7a8b4,0x59b33d17,0x2eb40d81,
        0xb7bd5c3b,0xc0ba6cad,0xedb88320,0x9abfb3b6,0x03b6e20c,0x74b1d29a,
        0xead54739,0x9dd277af,0x04db2615,0x73dc1683,0xe3630b12,0x94643b84,
        0x0d6d6a3e,0x7a6a5aa8,0xe40ecf0b,0x9309ff9d,0x0a00ae27,0x7d079eb1,
        0xf00f9344,0x8708a3d2,0x1e01f268,0x6906c2fe,0xf762575d,0x806567cb,
        0x196c3671,0x6e6b06e7,0xfed41b76,0x89d32be0,0x10da7a5a,0x67dd4acc,
        0xf9b9df6f,0x8ebeeff9,0x17b7be43,0x60b08ed5,0xd6d6a3e8,0xa1d1937e,
        0x38d8c2c4,0x4fdff252,0xd1bb67f1,0xa6bc5767,0x3fb506dd,0x48b2364b,
        0xd80d2bda,0xaf0a1b4c,0x36034af6,0x41047a60,0xdf60efc3,0xa867df55,
        0x316e8eef,0x4669be79,0xcb61b38c,0xbc66831a,0x256fd2a0,0x5268e236,
        0xcc0c7795,0xbb0b4703,0x220216b9,0x5505262f,0xc5ba3bbe,0xb2bd0b28,
        0x2bb45a92,0x5cb36a04,0xc2d7ffa7,0xb5d0cf31,0x2cd99e8b,0x5bdeae1d,
        0x9b64c2b0,0xec63f226,0x756aa39c,0x026d930a,0x9c0906a9,0xeb0e363f,
        0x72076785,0x05005713,0x95bf4a82,0xe2b87a14,0x7bb12bae,0x0cb61b38,
        0x92d28e9b,0xe5d5be0d,0x7cdcefb7,0x0bdbdf21,0x86d3d2d4,0xf1d4e242,
        0x68ddb3f8,0x1fda836e,0x81be16cd,0xf6b9265b,0x6fb077e1,0x18b74777,
        0x88085ae6,0xff0f6a70,0x66063bca,0x11010b5c,0x8f659eff,0xf862ae69,
        0x616bffd3,0x166ccf45,0xa00ae278,0xd70dd2ee,0x4e048354,0x3903b3c2,
        0xa7672661,0xd06016f7,0x4969474d,0x3e6e77db,0xaed16a4a,0xd9d65adc,
        0x40df0b66,0x37d83bf0,0xa9bcae53,0xdebb9ec5,0x47b2cf7f,0x30b5ffe9,
        0xbdbdf21c,0xcabac28a,0x53b39330,0x24b4a3a6,0xbad03605,0xcdd70693,
        0x54de5729,0x23d967bf,0xb3667a2e,0xc4614ab8,0x5d681b02,0x2a6f2b94,
        0xb40bbe37,0xc30c8ea1,0x5a05df1b,0x2d02ef8d
    };
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

static std::string buildCommandLine(const std::string& exe,
                                    const std::vector<std::string>& args) {
    std::string cmd = "\"" + exe + "\"";
    for (const auto& a : args) {
        cmd += " \"" + a + "\"";
    }
    return cmd;
}

static uint64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static bool connectNamedPipeWithTimeout(HANDLE hPipe, uint32_t timeoutMs)
{
    if (hPipe == INVALID_HANDLE_VALUE) return false;

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) return false;

    BOOL ok = ConnectNamedPipe(hPipe, &ov);
    if (!ok)
    {
        DWORD err = GetLastError();
        if (err == ERROR_PIPE_CONNECTED)
        {
            CloseHandle(ov.hEvent);
            return true;
        }
        if (err != ERROR_IO_PENDING)
        {
            CloseHandle(ov.hEvent);
            return false;
        }
    }

    DWORD wait = WaitForSingleObject(ov.hEvent, timeoutMs);
    if (wait != WAIT_OBJECT_0)
    {
        CancelIoEx(hPipe, &ov);
        CloseHandle(ov.hEvent);
        return false;
    }

    DWORD transferred = 0;
    ok = GetOverlappedResult(hPipe, &ov, &transferred, FALSE);
    CloseHandle(ov.hEvent);
    return ok == TRUE;
}

// ============================================================================
// ProcessBroker Implementation
// ============================================================================

ProcessBroker::ProcessBroker(const BrokerConfig& config) : m_config(config) {}

ProcessBroker::~ProcessBroker() { shutdown(); }

bool ProcessBroker::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) return true;
    m_running = true;
    m_monitorThread = std::thread(&ProcessBroker::monitorLoop, this);
    return true;
}

void ProcessBroker::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running) return;
        m_running = false;
    }
    if (m_monitorThread.joinable()) m_monitorThread.join();

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [id, info] : m_processes) {
        if (info->hPipe != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(info->hPipe);
            CloseHandle(info->hPipe);
        }
        if (info->hProcess) {
            TerminateProcess(info->hProcess, 1);
            WaitForSingleObject(info->hProcess, m_config.killTimeoutMs);
            CloseHandle(info->hProcess);
        }
        if (info->hThread) CloseHandle(info->hThread);
    }
    m_processes.clear();
}

int64_t ProcessBroker::spawnExtension(const std::string& exePath,
                                      const std::string& workingDir,
                                      const std::vector<std::string>& args) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) return -1;
    if (m_processes.size() >= m_config.maxExtensions) return -1;

    int64_t extId = m_nextExtId++;
    std::string pipeName = m_config.pipePrefix + std::to_string(extId);

    // Create named pipe
    HANDLE hPipe = CreateNamedPipeA(
        pipeName.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1, 65536, 65536, m_config.pipeTimeoutMs, nullptr);
    if (hPipe == INVALID_HANDLE_VALUE) return -1;

    // Build command line with pipe name argument
    std::vector<std::string> extArgs = args;
    extArgs.push_back("--rawrxd-pipe=" + pipeName);

    std::string cmdLine = buildCommandLine(exePath, extArgs);
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    BOOL created = CreateProcessA(
        nullptr, cmdBuf.data(), nullptr, nullptr, FALSE,
        CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED,
        nullptr, workingDir.empty() ? nullptr : workingDir.c_str(),
        &si, &pi);

    if (!created) {
        CloseHandle(hPipe);
        return -1;
    }

    // Apply job-object limits (memory + CPU)
    HANDLE hJob = CreateJobObjectA(nullptr, nullptr);
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
        jeli.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_JOB_MEMORY |
            JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        jeli.JobMemoryLimit = m_config.maxMemoryPerExtension;
        jeli.BasicLimitInformation.ActiveProcessLimit = 4;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
                                &jeli, sizeof(jeli));
        AssignProcessToJobObject(hJob, pi.hProcess);
        // hJob intentionally leaked with process; closed on process death
    }

    ResumeThread(pi.hThread);

    // Wait for the extension process to connect to the server pipe.
    // Without a connection, IPC writes will fail and termination will be best-effort only.
    if (!connectNamedPipeWithTimeout(hPipe, m_config.pipeTimeoutMs))
    {
        // Connection failure: keep the process record, but mark the pipe invalid so callers fail closed.
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
    }

    auto info = std::make_unique<ExtProcessInfo>();
    info->extId = extId;
    info->hProcess = pi.hProcess;
    info->hThread = pi.hThread;
    info->processId = pi.dwProcessId;
    info->pipeName = pipeName;
    info->hPipe = hPipe;
    info->active = true;
    info->spawnTime = nowMs();
    info->lastHeartbeat = nowMs();

    m_processes[extId] = std::move(info);
    return extId;
}

bool ProcessBroker::terminateExtension(int64_t extId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(extId);
    if (it == m_processes.end()) return false;

    auto& info = it->second;
    // Send graceful shutdown message
    if (info->hPipe != INVALID_HANDLE_VALUE) {
        BrokerMessage msg;
        msg.type = static_cast<uint32_t>(BrokerMsgType::Shutdown);
        msg.payloadLen = 0;
        msg.crc32 = crc32(nullptr, 0);
        writeFrame(info->hPipe, msg);
    }

    // Give process time to exit gracefully
    if (WaitForSingleObject(info->hProcess, m_config.killTimeoutMs) == WAIT_OBJECT_0) {
        info->active = false;
        return true;
    }
    return false;
}

bool ProcessBroker::killExtension(int64_t extId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(extId);
    if (it == m_processes.end()) return false;

    auto& info = it->second;
    if (info->hProcess) {
        TerminateProcess(info->hProcess, 1);
        WaitForSingleObject(info->hProcess, 1000);
    }
    info->active = false;
    return true;
}

bool ProcessBroker::isExtensionAlive(int64_t extId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(extId);
    if (it == m_processes.end()) return false;
    if (!it->second->active) return false;
    DWORD code = 0;
    if (GetExitCodeProcess(it->second->hProcess, &code) && code == STILL_ACTIVE)
        return true;
    return false;
}

size_t ProcessBroker::getActiveCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& [_, info] : m_processes) {
        if (info->active) ++count;
    }
    return count;
}

std::vector<int64_t> ProcessBroker::listActive() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<int64_t> ids;
    for (const auto& [id, info] : m_processes) {
        if (info->active) ids.push_back(id);
    }
    return ids;
}

bool ProcessBroker::sendMessage(int64_t extId, const BrokerMessage& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(extId);
    if (it == m_processes.end() || !it->second->active) return false;
    return writeFrame(it->second->hPipe, msg);
}

bool ProcessBroker::recvMessage(int64_t extId, BrokerMessage& msg, uint32_t timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_processes.find(extId);
            if (it == m_processes.end() || !it->second->active) return false;
            if (readFrame(it->second->hPipe, msg, 100)) {
                // Validate CRC
                uint32_t computedCrc = crc32(msg.payload.data(), msg.payload.size());
                if (computedCrc != msg.crc32) {
                    OutputDebugStringA("[ProcessBroker] CRC mismatch\n");
                    return false;
                }
                // Validate magic
                if (msg.magic != 0x5242574D) {
                    OutputDebugStringA("[ProcessBroker] Invalid magic\n");
                    return false;
                }
                it->second->lastHeartbeat = nowMs();
                return true;
            }
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            OutputDebugStringA("[ProcessBroker] recvMessage timeout\n");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool ProcessBroker::setMemoryLimit(int64_t extId, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(extId);
    if (it == m_processes.end()) return false;
    // Job object already set at spawn; this is advisory for future spawns
    (void)bytes;
    return true;
}

bool ProcessBroker::setCpuLimit(int64_t extId, uint32_t percent) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(extId);
    if (it == m_processes.end() || !it->second->hProcess) return false;
    // Use hard affinity mask to approximate CPU limit
    DWORD_PTR mask = 0;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int cores = static_cast<int>(si.dwNumberOfProcessors);
    int allowed = std::max(1, static_cast<int>((cores * percent) / 100));
    for (int i = 0; i < allowed; ++i) mask |= (1ULL << i);
    SetProcessAffinityMask(it->second->hProcess, mask);
    return true;
}

size_t ProcessBroker::getPeakMemory(int64_t extId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(extId);
    if (it == m_processes.end()) return 0;
    return it->second->peakMemory;
}

void ProcessBroker::onExtensionDeath(const DeathCallback& cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deathCallback = cb;
}

// ============================================================================
// Pipe I/O
// ============================================================================

bool ProcessBroker::writeFrame(HANDLE hPipe, const BrokerMessage& msg) {
    if (hPipe == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    // Header
    struct Header {
        uint32_t magic;
        uint32_t type;
        uint32_t payloadLen;
        uint32_t crc32;
    } hdr = { msg.magic, msg.type, msg.payloadLen, msg.crc32 };
    if (!WriteFile(hPipe, &hdr, sizeof(hdr), &written, nullptr) || written != sizeof(hdr))
        return false;
    if (msg.payloadLen > 0 && !msg.payload.empty()) {
        if (!WriteFile(hPipe, msg.payload.data(), msg.payloadLen, &written, nullptr))
            return false;
    }
    FlushFileBuffers(hPipe);
    return true;
}

bool ProcessBroker::readFrame(HANDLE hPipe, BrokerMessage& msg, uint32_t timeoutMs) {
    if (hPipe == INVALID_HANDLE_VALUE) return false;
    DWORD totalAvail = 0;
    if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &totalAvail, nullptr))
        return false;
    if (totalAvail < sizeof(uint32_t) * 4) return false;

    struct Header {
        uint32_t magic;
        uint32_t type;
        uint32_t payloadLen;
        uint32_t crc32;
    } hdr = {};
    DWORD read = 0;
    if (!ReadFile(hPipe, &hdr, sizeof(hdr), &read, nullptr) || read != sizeof(hdr))
        return false;
    if (hdr.magic != 0x5242574D) return false;

    msg.magic = hdr.magic;
    msg.type = hdr.type;
    msg.payloadLen = hdr.payloadLen;
    msg.crc32 = hdr.crc32;
    msg.payload.clear();
    if (hdr.payloadLen > 0) {
        msg.payload.resize(hdr.payloadLen);
        DWORD totalRead = 0;
        while (totalRead < hdr.payloadLen) {
            DWORD chunk = 0;
            if (!ReadFile(hPipe, msg.payload.data() + totalRead,
                          hdr.payloadLen - totalRead, &chunk, nullptr))
                return false;
            totalRead += chunk;
        }
    }
    return true;
}

// ============================================================================
// Monitoring
// ============================================================================

void ProcessBroker::monitorLoop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::unique_lock<std::mutex> lock(m_mutex);
        const uint64_t now = nowMs();

        for (auto it = m_processes.begin(); it != m_processes.end(); ) {
            ExtProcessInfo* info = it->second.get();
            if (!info || !info->active) { ++it; continue; }

            DWORD exitCode = 0;
            const BOOL gotExitCode = GetExitCodeProcess(info->hProcess, &exitCode);
            if (gotExitCode && exitCode != STILL_ACTIVE) {
                info->active = false;

                const int64_t deadExtId = info->extId;
                const uint32_t deadExit = static_cast<uint32_t>(exitCode);
                const auto cb = m_deathCallback;

                // Cleanup handles while we still hold the lock.
                if (info->hPipe != INVALID_HANDLE_VALUE) {
                    DisconnectNamedPipe(info->hPipe);
                    CloseHandle(info->hPipe);
                    info->hPipe = INVALID_HANDLE_VALUE;
                }
                if (info->hProcess) { CloseHandle(info->hProcess); info->hProcess = nullptr; }
                if (info->hThread) { CloseHandle(info->hThread); info->hThread = nullptr; }

                it = m_processes.erase(it);

                // Run callback without holding broker mutex.
                if (cb) {
                    lock.unlock();
                    cb(deadExtId, deadExit);
                    lock.lock();
                }
                continue;
            }

            // Heartbeat timeout
            if ((now - info->lastHeartbeat) > (static_cast<uint64_t>(m_config.heartbeatIntervalMs) * 3ULL)) {
                TerminateProcess(info->hProcess, 2);
            }

            updatePeakMemory(*info);
            ++it;
        }
    }
}

void ProcessBroker::updatePeakMemory(ExtProcessInfo& info) {
    PROCESS_MEMORY_COUNTERS pmc = {};
    if (GetProcessMemoryInfo(info.hProcess, &pmc, sizeof(pmc))) {
        if (pmc.PeakWorkingSetSize > info.peakMemory)
            info.peakMemory = pmc.PeakWorkingSetSize;
    }
}

} // namespace RawrXD::Extensions
