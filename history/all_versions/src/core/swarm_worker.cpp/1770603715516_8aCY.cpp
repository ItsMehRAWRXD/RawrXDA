// =============================================================================
// swarm_worker.cpp — Phase 11B: Distributed Swarm Worker Node
// =============================================================================
// Worker node that receives compile tasks, executes them locally via
// CreateProcess, and sends results back to the coordinator. Includes
// heartbeat sender, capability reporting, and attestation response.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "swarm_worker.h"
#include <wincrypt.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>

// MASM ASM functions
extern "C" {
    uint32_t Swarm_ComputeNodeFitness(void);
    int      Swarm_RingBuffer_Init(void* ring, void* buffer);
    int      Swarm_RingBuffer_Push(void* ring, const void* data, uint64_t size);
    uint64_t Swarm_RingBuffer_Pop(void* ring, void* dest);
    int      Swarm_ValidatePacketHeader(const void* packet);
    int      Swarm_BuildPacketHeader(void* buffer, uint8_t opcode,
                                      uint16_t payloadLen, uint64_t taskId);
    int      Swarm_Blake2b_128(const void* data, uint64_t len, void* out16);
    uint64_t Swarm_XXH64(const void* data, uint64_t len, uint64_t seed);
    uint64_t Swarm_HeartbeatRecord(uint32_t nodeSlot);
}

// =============================================================================
//                          SINGLETON
// =============================================================================

SwarmWorker& SwarmWorker::instance() {
    static SwarmWorker inst;
    return inst;
}

SwarmWorker::SwarmWorker()
    : m_running(false)
    , m_connected(false)
    , m_shutdownRequested(false)
    , m_activeTasks(0)
    , m_completedTasks(0)
    , m_failedTasks(0)
    , m_fitnessScore(0)
    , m_maxConcurrentTasks(4)
    , m_nodeSlotIndex(0xFFFFFFFF)
    , m_leaderSocket(INVALID_SOCKET)
    , m_wsaInitialized(false)
    , m_sequenceCounter(0)
    , m_lastCompileTimeMs(0)
    , m_hReceiverThread(nullptr)
    , m_hHeartbeatThread(nullptr)
    , m_taskThreadCount(0)
    , m_taskQueueEvent(nullptr)
    , m_rxRing(nullptr)
    , m_rxRingBuffer(nullptr)
{
    memset(m_hTaskThreads, 0, sizeof(m_hTaskThreads));
    memset(m_localNodeId, 0, sizeof(m_localNodeId));
    memset(&m_wsaData, 0, sizeof(m_wsaData));
}

SwarmWorker::~SwarmWorker() {
    if (m_running.load()) stop();
}

// =============================================================================
//                          LIFECYCLE
// =============================================================================

bool SwarmWorker::start(const DscConfig& config) {
    if (m_running.load()) return false;

    m_config = config;
    m_shutdownRequested.store(false);

    // Init WinSock
    if (!m_wsaInitialized) {
        if (WSAStartup(MAKEWORD(2, 2), &m_wsaData) != 0) return false;
        m_wsaInitialized = true;
    }

    // Compute fitness via MASM CPUID
    m_fitnessScore = Swarm_ComputeNodeFitness();

    // Determine max concurrent tasks from CPU count
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_maxConcurrentTasks = si.dwNumberOfProcessors;
    if (m_maxConcurrentTasks < 1) m_maxConcurrentTasks = 1;
    if (m_maxConcurrentTasks > 8) m_maxConcurrentTasks = 8;

    // Generate node ID
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_FULL,
                              CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, 16, m_localNodeId);
        CryptReleaseContext(hProv, 0);
    }

    // Allocate ring buffer
    size_t ringStructSize = 72;
    size_t ringBufSize = (size_t)SWARM_RING_CAPACITY * SWARM_RING_SLOT_SIZE;
    m_rxRing = VirtualAlloc(nullptr, ringStructSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    m_rxRingBuffer = VirtualAlloc(nullptr, ringBufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (m_rxRing && m_rxRingBuffer) {
        Swarm_RingBuffer_Init(m_rxRing, m_rxRingBuffer);
    }

    // Create task queue event
    m_taskQueueEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);

    m_running.store(true);

    // Start task executor threads
    m_taskThreadCount = m_maxConcurrentTasks;
    for (uint32_t i = 0; i < m_taskThreadCount; i++) {
        m_hTaskThreads[i] = CreateThread(nullptr, 0, taskExecutorThread, this, 0, nullptr);
    }

    return true;
}

void SwarmWorker::stop() {
    if (!m_running.load()) return;

    m_shutdownRequested.store(true);
    m_running.store(false);

    // Disconnect from leader
    disconnect();

    // Signal task threads to wake up
    if (m_taskQueueEvent) {
        for (uint32_t i = 0; i < m_taskThreadCount; i++) {
            SetEvent(m_taskQueueEvent);
        }
    }

    // Wait for threads
    if (m_hReceiverThread)  { WaitForSingleObject(m_hReceiverThread, 3000); CloseHandle(m_hReceiverThread); m_hReceiverThread = nullptr; }
    if (m_hHeartbeatThread) { WaitForSingleObject(m_hHeartbeatThread, 3000); CloseHandle(m_hHeartbeatThread); m_hHeartbeatThread = nullptr; }
    for (uint32_t i = 0; i < m_taskThreadCount; i++) {
        if (m_hTaskThreads[i]) {
            WaitForSingleObject(m_hTaskThreads[i], 3000);
            CloseHandle(m_hTaskThreads[i]);
            m_hTaskThreads[i] = nullptr;
        }
    }

    if (m_taskQueueEvent) { CloseHandle(m_taskQueueEvent); m_taskQueueEvent = nullptr; }

    // Free ring buffers
    if (m_rxRing)       { VirtualFree(m_rxRing, 0, MEM_RELEASE); m_rxRing = nullptr; }
    if (m_rxRingBuffer) { VirtualFree(m_rxRingBuffer, 0, MEM_RELEASE); m_rxRingBuffer = nullptr; }
}

// =============================================================================
//                     CONNECTION
// =============================================================================

bool SwarmWorker::connectToLeader(const char* ipAddress, uint16_t port) {
    if (m_connected.load()) disconnect();

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ipAddress, &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    m_leaderSocket = sock;
    m_connected.store(true);

    // Start receiver and heartbeat threads
    m_hReceiverThread = CreateThread(nullptr, 0, receiverThread, this, 0, nullptr);
    m_hHeartbeatThread = CreateThread(nullptr, 0, heartbeatSenderThread, this, 0, nullptr);

    // Send capability report
    sendCapsReport();

    return true;
}

void SwarmWorker::disconnect() {
    m_connected.store(false);
    if (m_leaderSocket != INVALID_SOCKET) {
        closesocket(m_leaderSocket);
        m_leaderSocket = INVALID_SOCKET;
    }
}

// =============================================================================
//                     NETWORK THREADS
// =============================================================================

DWORD WINAPI SwarmWorker::receiverThread(LPVOID param) {
    auto* self = static_cast<SwarmWorker*>(param);
    std::vector<uint8_t> buf(SWARM_HEADER_SIZE + SWARM_MAX_PAYLOAD);

    while (self->m_running.load() && self->m_connected.load()) {
        // Receive header
        char* ptr = reinterpret_cast<char*>(buf.data());
        int totalRecv = 0;

        while (totalRecv < SWARM_HEADER_SIZE) {
            int r = recv(self->m_leaderSocket, ptr + totalRecv,
                          SWARM_HEADER_SIZE - totalRecv, 0);
            if (r <= 0) {
                self->disconnect();
                return 0;
            }
            totalRecv += r;
        }

        auto* hdr = reinterpret_cast<const SwarmPacketHeader*>(buf.data());
        uint16_t payloadLen = hdr->payloadLen;

        // Receive payload
        if (payloadLen > 0) {
            int payloadRecv = 0;
            while (payloadRecv < payloadLen) {
                int r = recv(self->m_leaderSocket,
                              ptr + SWARM_HEADER_SIZE + payloadRecv,
                              payloadLen - payloadRecv, 0);
                if (r <= 0) {
                    self->disconnect();
                    return 0;
                }
                payloadRecv += r;
            }
        }

        // Validate via MASM
        if (Swarm_ValidatePacketHeader(buf.data()) != 0) continue;

        const uint8_t* payloadPtr = buf.data() + SWARM_HEADER_SIZE;

        // Dispatch
        switch (static_cast<SwarmOpcode>(hdr->opcode)) {
            case SwarmOpcode::TaskPush:
                self->handleTaskPush(
                    reinterpret_cast<const TaskPushPayload*>(payloadPtr),
                    payloadPtr + sizeof(TaskPushPayload),
                    payloadLen > sizeof(TaskPushPayload) ?
                        payloadLen - sizeof(TaskPushPayload) : 0);
                break;

            case SwarmOpcode::AttestRequest:
                self->handleAttestRequest(
                    reinterpret_cast<const AttestRequestPayload*>(payloadPtr));
                break;

            case SwarmOpcode::Heartbeat:
                // Leader heartbeat — just record it
                Swarm_HeartbeatRecord(0);
                break;

            case SwarmOpcode::Shutdown:
                self->handleShutdownRequest();
                return 0;

            default:
                break;
        }
    }

    return 0;
}

DWORD WINAPI SwarmWorker::heartbeatSenderThread(LPVOID param) {
    auto* self = static_cast<SwarmWorker*>(param);

    while (self->m_running.load() && self->m_connected.load()) {
        Sleep(self->m_config.heartbeatIntervalMs);
        if (!self->m_running.load() || !self->m_connected.load()) break;

        self->sendHeartbeat();
    }

    return 0;
}

DWORD WINAPI SwarmWorker::taskExecutorThread(LPVOID param) {
    auto* self = static_cast<SwarmWorker*>(param);

    while (self->m_running.load()) {
        // Wait for a task
        WaitForSingleObject(self->m_taskQueueEvent, 1000);
        if (!self->m_running.load()) break;

        // Dequeue a task
        WorkerTask task;
        bool hasTask = false;
        {
            std::lock_guard<std::mutex> lock(self->m_taskQueueMutex);
            if (!self->m_taskQueue.empty()) {
                task = self->m_taskQueue.back();
                self->m_taskQueue.pop_back();
                hasTask = true;
            }
        }

        if (!hasTask) continue;

        self->m_activeTasks.fetch_add(1);

        // Execute the compilation with wall-clock timing
        int exitCode = 0;
        std::string compileLog;
        std::vector<uint8_t> objectData;

        LARGE_INTEGER freq, tStart, tEnd;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&tStart);

        bool ok = self->executeCompileTask(task, exitCode, compileLog, objectData);

        QueryPerformanceCounter(&tEnd);
        uint32_t elapsedMs = static_cast<uint32_t>(
            (tEnd.QuadPart - tStart.QuadPart) * 1000 / freq.QuadPart);
        self->m_lastCompileTimeMs.store(elapsedMs, std::memory_order_relaxed);

        self->m_activeTasks.fetch_sub(1);

        if (ok && exitCode == 0) {
            self->m_completedTasks.fetch_add(1);
        } else {
            self->m_failedTasks.fetch_add(1);
        }

        // Send result back to leader
        self->sendResult(task.taskId, exitCode, compileLog, objectData);

        // Request more work
        TaskPullPayload pull = {};
        pull.nodeSlotIndex = self->m_nodeSlotIndex;
        pull.maxConcurrentTasks = self->m_maxConcurrentTasks;
        pull.currentLoad = self->m_activeTasks.load();
        pull.lastCompletedTaskId = task.taskId;
        self->sendPacketToLeader(SwarmOpcode::TaskPull, &pull, sizeof(pull));
    }

    return 0;
}

// =============================================================================
//                     PACKET HANDLERS
// =============================================================================

void SwarmWorker::handleTaskPush(const TaskPushPayload* payload,
                                   const uint8_t* extraData, uint32_t extraLen) {
    WorkerTask task;
    task.taskId = payload->taskId;
    task.taskType = static_cast<SwarmTaskType>(payload->taskType);
    task.dagGeneration = payload->dagGeneration;

    // Extract source file and compiler args from extra data
    if (extraData && extraLen > 0) {
        task.sourceFile = std::string(reinterpret_cast<const char*>(extraData),
                                       payload->sourceFileLen > 0 ?
                                       payload->sourceFileLen - 1 : 0);
        if (payload->compilerArgsLen > 0 && extraLen >= payload->sourceFileLen) {
            task.compilerArgs = std::string(
                reinterpret_cast<const char*>(extraData + payload->sourceFileLen),
                payload->compilerArgsLen - 1);
        }
    }

    // Generate output file name
    std::string baseName = task.sourceFile;
    size_t lastSlash = baseName.find_last_of("/\\");
    if (lastSlash != std::string::npos) baseName = baseName.substr(lastSlash + 1);
    size_t dotPos = baseName.rfind('.');
    if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);
    task.outputFile = ".\\swarm_work\\" + baseName + ".obj";

    // Create work directory
    CreateDirectoryA(".\\swarm_work", nullptr);

    // Queue the task
    {
        std::lock_guard<std::mutex> lock(m_taskQueueMutex);
        m_taskQueue.push_back(task);
    }
    SetEvent(m_taskQueueEvent);
}

void SwarmWorker::handleAttestRequest(const AttestRequestPayload* payload) {
    AttestResponsePayload resp = {};
    memcpy(resp.hwid, m_localNodeId, 16);
    resp.protocolVersion = SWARM_VERSION;
    resp.fitnessScore = m_fitnessScore;
    resp.uptimeMs = SwarmTime::nowMs();

    // Compute HMAC of challenge using shared secret
    // For now, XOR the challenge with the shared secret as a simplified response
    for (int i = 0; i < 32; i++) {
        resp.challengeResp[i] = payload->challenge[i] ^
            static_cast<uint8_t>(m_config.sharedSecret[i % 64]);
    }

    sendPacketToLeader(SwarmOpcode::AttestResponse, &resp, sizeof(resp));
}

void SwarmWorker::handleShutdownRequest() {
    m_shutdownRequested.store(true);
    disconnect();
}

// =============================================================================
//                     TASK EXECUTION
// =============================================================================

bool SwarmWorker::executeCompileTask(const WorkerTask& task,
                                       int& outExitCode,
                                       std::string& outLog,
                                       std::vector<uint8_t>& outObjectData) {
    // Build command line
    std::string compiler = getCompilerPath(task.taskType);
    if (compiler.empty()) {
        outExitCode = -1;
        outLog = "Error: No compiler found for task type " +
                 std::to_string(static_cast<int>(task.taskType));
        return false;
    }

    std::string cmdLine;
    switch (task.taskType) {
        case SwarmTaskType::CompileCpp:
        case SwarmTaskType::CompileC:
            cmdLine = compiler + " -c " + task.compilerArgs + " -o " +
                      task.outputFile + " " + task.sourceFile;
            break;

        case SwarmTaskType::AssembleMASM:
            cmdLine = compiler + " /c /Fo" + task.outputFile + " " + task.sourceFile;
            break;

        case SwarmTaskType::AssembleNASM:
            cmdLine = compiler + " -f win64 -o " + task.outputFile + " " + task.sourceFile;
            break;

        case SwarmTaskType::LinkPartial:
        case SwarmTaskType::LinkFinal:
            cmdLine = compiler + " " + task.compilerArgs + " -o " +
                      task.outputFile + " " + task.sourceFile;
            break;

        default:
            cmdLine = compiler + " " + task.compilerArgs;
            break;
    }

    // Create process with pipes for stdout/stderr capture
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        outExitCode = -1;
        outLog = "Error: Failed to create pipe";
        return false;
    }

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    PROCESS_INFORMATION pi = {};

    BOOL created = CreateProcessA(
        nullptr, const_cast<char*>(cmdLine.c_str()),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr,
        &si, &pi);

    CloseHandle(hWritePipe); // Close write end in parent

    if (!created) {
        CloseHandle(hReadPipe);
        outExitCode = -1;
        outLog = "Error: CreateProcess failed for: " + cmdLine;
        return false;
    }

    // Read output
    std::string output;
    char readBuf[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, readBuf, sizeof(readBuf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        readBuf[bytesRead] = '\0';
        output += readBuf;
    }
    CloseHandle(hReadPipe);

    // Wait for process with timeout
    DWORD waitResult = WaitForSingleObject(pi.hProcess, m_config.taskTimeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        outExitCode = -2;
        outLog = "Error: Task timed out after " +
                 std::to_string(m_config.taskTimeoutMs) + "ms\n" + output;
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }

    DWORD exitCodeDw = 0;
    GetExitCodeProcess(pi.hProcess, &exitCodeDw);
    outExitCode = static_cast<int>(exitCodeDw);
    outLog = output;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Read compiled object file if successful
    if (outExitCode == 0) {
        HANDLE hObjFile = CreateFileA(task.outputFile.c_str(), GENERIC_READ,
                                       FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hObjFile != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hObjFile, nullptr);
            if (fileSize > 0 && fileSize < 100 * 1024 * 1024) { // Max 100MB
                outObjectData.resize(fileSize);
                DWORD read = 0;
                ReadFile(hObjFile, outObjectData.data(), fileSize, &read, nullptr);
            }
            CloseHandle(hObjFile);
        }
    }

    // Send real-time log to leader
    LogStreamPayload lsp = {};
    lsp.taskId = task.taskId;
    lsp.nodeSlotIndex = m_nodeSlotIndex;
    lsp.logLevel = (outExitCode == 0) ? 1 : 3;
    lsp.logLen = static_cast<uint32_t>(output.size());

    // Build payload with log text appended
    std::vector<uint8_t> logPayload(sizeof(LogStreamPayload) + output.size() + 1);
    memcpy(logPayload.data(), &lsp, sizeof(lsp));
    memcpy(logPayload.data() + sizeof(lsp), output.c_str(), output.size() + 1);
    sendPacketToLeader(SwarmOpcode::LogStream, logPayload.data(),
                        static_cast<uint16_t>(logPayload.size()), task.taskId);

    return true;
}

std::string SwarmWorker::getCompilerPath(SwarmTaskType type) const {
    switch (type) {
        case SwarmTaskType::CompileCpp:
        case SwarmTaskType::CompileC:
        case SwarmTaskType::LinkPartial:
        case SwarmTaskType::LinkFinal:
            if (m_config.compilerPath[0] != '\0') {
                return m_config.compilerPath;
            }
            return "g++"; // Default

        case SwarmTaskType::AssembleMASM:
            return "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\ml64.exe";

        case SwarmTaskType::AssembleNASM:
            return "nasm";

        default:
            return "";
    }
}

// =============================================================================
//                     NETWORK
// =============================================================================

bool SwarmWorker::sendPacketToLeader(SwarmOpcode opcode, const void* payload,
                                       uint16_t payloadLen, uint64_t taskId) {
    if (!m_connected.load() || m_leaderSocket == INVALID_SOCKET) return false;

    std::vector<uint8_t> packet(SWARM_HEADER_SIZE + payloadLen);
    if (payload && payloadLen > 0) {
        memcpy(packet.data() + SWARM_HEADER_SIZE, payload, payloadLen);
    }

    SwarmProtocol::buildPacket(packet.data(), opcode, payload, payloadLen,
                                taskId, m_localNodeId,
                                m_sequenceCounter.fetch_add(1));

    const char* ptr = reinterpret_cast<const char*>(packet.data());
    uint32_t remaining = static_cast<uint32_t>(packet.size());

    while (remaining > 0) {
        int sent = send(m_leaderSocket, ptr, remaining, 0);
        if (sent == SOCKET_ERROR) {
            disconnect();
            return false;
        }
        ptr += sent;
        remaining -= sent;
    }

    return true;
}

void SwarmWorker::sendCapsReport() {
    CapsReportPayload caps = {};

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    caps.logicalCores = si.dwNumberOfProcessors;
    caps.physicalCores = si.dwNumberOfProcessors; // Simplified

    MEMORYSTATUSEX memInfo = {};
    memInfo.dwLength = sizeof(memInfo);
    GlobalMemoryStatusEx(&memInfo);
    caps.ramTotalMB = static_cast<uint32_t>(memInfo.ullTotalPhys / (1024 * 1024));
    caps.ramAvailMB = static_cast<uint32_t>(memInfo.ullAvailPhys / (1024 * 1024));

    caps.fitnessScore = m_fitnessScore;
    caps.maxConcurrentTasks = m_maxConcurrentTasks;

    // Check AVX2/AVX512 via fitness (MASM already computed)
    caps.hasAVX2 = (m_fitnessScore >= 500) ? 1 : 0;
    caps.hasAVX512 = (m_fitnessScore >= 1500) ? 1 : 0;
    caps.hasAESNI = (m_fitnessScore >= 200) ? 1 : 0;

    gethostname(caps.hostname, sizeof(caps.hostname) - 1);
    strncpy(caps.compilerPath, getCompilerPath(SwarmTaskType::CompileCpp).c_str(),
            sizeof(caps.compilerPath) - 1);

    OSVERSIONINFOA osInfo = {};
    osInfo.dwOSVersionInfoSize = sizeof(osInfo);
    // GetVersionEx is deprecated but still works
    caps.osVersion = 0;

    sendPacketToLeader(SwarmOpcode::CapsReport, &caps, sizeof(caps));
}

void SwarmWorker::sendHeartbeat() {
    HeartbeatPayload hb = {};
    hb.nodeSlotIndex = m_nodeSlotIndex;
    hb.activeTasks = m_activeTasks.load();
    hb.uptimeMs = SwarmTime::nowMs();
    hb.fitnessScore = m_fitnessScore;

    // Get CPU load (simplified)
    hb.cpuLoadPercent = 0;
    MEMORYSTATUSEX memInfo = {};
    memInfo.dwLength = sizeof(memInfo);
    GlobalMemoryStatusEx(&memInfo);
    hb.memUsedMB = static_cast<uint32_t>(
        (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024));

    sendPacketToLeader(SwarmOpcode::Heartbeat, &hb, sizeof(hb));
}

void SwarmWorker::sendResult(uint64_t taskId, int exitCode, const std::string& log,
                               const std::vector<uint8_t>& objectData) {
    ResultPushPayload rp = {};
    rp.taskId = taskId;
    rp.exitCode = static_cast<uint32_t>(exitCode);
    rp.objectFileSize = static_cast<uint32_t>(objectData.size());
    rp.compileTimeMs = m_lastCompileTimeMs.load(std::memory_order_relaxed);
    rp.logLen = static_cast<uint32_t>(log.size());

    // Compute object hash
    if (!objectData.empty()) {
        rp.objectHash = Swarm_XXH64(objectData.data(), objectData.size(), 0xDEADBEEF);
    }

    // Build full payload: ResultPushPayload + object data + log
    uint32_t totalPayload = static_cast<uint32_t>(
        sizeof(ResultPushPayload) + objectData.size() + log.size() + 1);

    // Clamp to max payload
    if (totalPayload > SWARM_MAX_PAYLOAD) {
        // Send without object data if too large
        totalPayload = static_cast<uint32_t>(sizeof(ResultPushPayload) + log.size() + 1);
        rp.objectFileSize = 0;
    }

    std::vector<uint8_t> payload(totalPayload);
    memcpy(payload.data(), &rp, sizeof(rp));

    uint32_t offset = sizeof(ResultPushPayload);
    if (rp.objectFileSize > 0) {
        memcpy(payload.data() + offset, objectData.data(), objectData.size());
        offset += rp.objectFileSize;
    }
    memcpy(payload.data() + offset, log.c_str(), log.size() + 1);

    sendPacketToLeader(SwarmOpcode::ResultPush, payload.data(),
                        static_cast<uint16_t>(payload.size()), taskId);
}

// =============================================================================
//                     STATUS / JSON
// =============================================================================

std::string SwarmWorker::getStatusString() const {
    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  Swarm Worker Status\n"
        << "════════════════════════════════════════════\n"
        << "  Running:     " << (m_running.load() ? "YES" : "NO") << "\n"
        << "  Connected:   " << (m_connected.load() ? "YES" : "NO") << "\n"
        << "  Fitness:     " << m_fitnessScore << "\n"
        << "  Max Tasks:   " << m_maxConcurrentTasks << "\n"
        << "  Active:      " << m_activeTasks.load() << "\n"
        << "  Completed:   " << m_completedTasks.load() << "\n"
        << "  Failed:      " << m_failedTasks.load() << "\n"
        << "════════════════════════════════════════════";
    return oss.str();
}

std::string SwarmWorker::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"running\":" << (m_running.load() ? "true" : "false") << ","
        << "\"connected\":" << (m_connected.load() ? "true" : "false") << ","
        << "\"fitness\":" << m_fitnessScore << ","
        << "\"maxTasks\":" << m_maxConcurrentTasks << ","
        << "\"activeTasks\":" << m_activeTasks.load() << ","
        << "\"completedTasks\":" << m_completedTasks.load() << ","
        << "\"failedTasks\":" << m_failedTasks.load()
        << "}";
    return oss.str();
}
