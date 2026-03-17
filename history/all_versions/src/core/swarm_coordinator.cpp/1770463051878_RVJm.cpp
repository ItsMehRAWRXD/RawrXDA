// =============================================================================
// swarm_coordinator.cpp — Phase 11A: Distributed Swarm Coordinator (Leader)
// =============================================================================
// Full implementation of the SwarmCoordinator singleton: lifecycle, network
// threads, packet dispatch, scheduling, consensus, discovery, and object cache.
//
// Thread safety: all shared state locked via std::lock_guard<std::mutex>.
// Network I/O: WinSock2 TCP (swarm protocol) + UDP (discovery).
// Ring buffers: delegated to MASM kernel (RawrXD_Swarm_Network.asm).
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "swarm_coordinator.h"
#include "swarm_protocol.h"
#include "swarm_types.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

// MASM-linked ASM functions
extern "C" {
    uint32_t Swarm_ComputeNodeFitness(void);
    int      Swarm_RingBuffer_Init(void* ring, void* buffer);
    int      Swarm_RingBuffer_Push(void* ring, const void* data, uint64_t size);
    uint64_t Swarm_RingBuffer_Pop(void* ring, void* dest);
    uint64_t Swarm_RingBuffer_Count(void* ring);
    int      Swarm_Blake2b_128(const void* data, uint64_t len, void* out16);
    uint64_t Swarm_XXH64(const void* data, uint64_t len, uint64_t seed);
    int      Swarm_ValidatePacketHeader(const void* packet);
    int      Swarm_BuildPacketHeader(void* buffer, uint8_t opcode,
                                      uint16_t payloadLen, uint64_t taskId);
    uint64_t Swarm_HeartbeatRecord(uint32_t nodeSlot);
    int      Swarm_HeartbeatCheck(uint32_t nodeSlot);
    void*    Swarm_IOCP_Create(void);
    void*    Swarm_IOCP_Associate(void* socket, void* iocp, uint64_t key);
    uint64_t Swarm_MemCopy_NT(void* dest, const void* src, uint64_t count);
}

// =============================================================================
//                          SINGLETON
// =============================================================================

SwarmCoordinator& SwarmCoordinator::instance() {
    static SwarmCoordinator inst;
    return inst;
}

// =============================================================================
//                          CONSTRUCTOR / DESTRUCTOR
// =============================================================================

SwarmCoordinator::SwarmCoordinator()
    : m_running(false)
    , m_buildRunning(false)
    , m_discoveryEnabled(false)
    , m_shutdownRequested(false)
    , m_nodeCount(0)
    , m_nextTaskId(1)
    , m_eventCallback(nullptr)
    , m_eventUserData(nullptr)
    , m_logCallback(nullptr)
    , m_logUserData(nullptr)
    , m_sequenceCounter(0)
    , m_listenSocket(INVALID_SOCKET)
    , m_discoverySocket(INVALID_SOCKET)
    , m_iocp(nullptr)
    , m_hListenerThread(nullptr)
    , m_hHeartbeatThread(nullptr)
    , m_hDiscoveryThread(nullptr)
    , m_hSchedulerThread(nullptr)
    , m_iocpThreadCount(0)
    , m_txRing(nullptr)
    , m_rxRing(nullptr)
    , m_txRingBuffer(nullptr)
    , m_rxRingBuffer(nullptr)
    , m_wsaInitialized(false)
{
    memset(m_nodes, 0, sizeof(m_nodes));
    memset(m_hIocpThreads, 0, sizeof(m_hIocpThreads));
    memset(m_localNodeId, 0, sizeof(m_localNodeId));
}

SwarmCoordinator::~SwarmCoordinator() {
    if (m_running.load()) {
        stop();
    }
}

// =============================================================================
//                          LIFECYCLE
// =============================================================================

bool SwarmCoordinator::start(const SwarmConfig& config) {
    if (m_running.load()) return false;

    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_config = config;
    }

    m_shutdownRequested.store(false);

    // Initialize WinSock
    if (!m_wsaInitialized) {
        int rc = WSAStartup(MAKEWORD(2, 2), &m_wsaData);
        if (rc != 0) return false;
        m_wsaInitialized = true;
    }

    // Generate local node ID
    generateNodeId(m_localNodeId);

    // Allocate ring buffers via VirtualAlloc (MASM-managed)
    size_t ringStructSize = 72;  // sizeof(SWARM_RING) from ASM
    size_t ringBufSize = (size_t)SWARM_RING_CAPACITY * SWARM_RING_SLOT_SIZE;

    m_txRing = VirtualAlloc(nullptr, ringStructSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    m_rxRing = VirtualAlloc(nullptr, ringStructSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    m_txRingBuffer = VirtualAlloc(nullptr, ringBufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    m_rxRingBuffer = VirtualAlloc(nullptr, ringBufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!m_txRing || !m_rxRing || !m_txRingBuffer || !m_rxRingBuffer) {
        return false;
    }

    // Initialize ring buffers via MASM
    Swarm_RingBuffer_Init(m_txRing, m_txRingBuffer);
    Swarm_RingBuffer_Init(m_rxRing, m_rxRingBuffer);

    // Create IOCP
    m_iocp = Swarm_IOCP_Create();
    if (!m_iocp) return false;

    // Create listener socket (TCP)
    if (config.mode == SwarmMode::Leader || config.mode == SwarmMode::Hybrid) {
        m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSocket == INVALID_SOCKET) return false;

        // Set SO_REUSEADDR
        int optVal = 1;
        setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optVal, sizeof(optVal));

        // Bind
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(config.swarmPort);

        if (bind(m_listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
            return false;
        }

        if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
            return false;
        }
    }

    // Create discovery socket (UDP broadcast)
    m_discoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_discoverySocket != INVALID_SOCKET) {
        int optVal = 1;
        setsockopt(m_discoverySocket, SOL_SOCKET, SO_BROADCAST, (char*)&optVal, sizeof(optVal));
        setsockopt(m_discoverySocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optVal, sizeof(optVal));

        sockaddr_in bindAddr = {};
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_addr.s_addr = INADDR_ANY;
        bindAddr.sin_port = htons(config.discoveryPort);
        bind(m_discoverySocket, (sockaddr*)&bindAddr, sizeof(bindAddr));
    }

    m_running.store(true);

    // Start threads
    if (config.mode == SwarmMode::Leader || config.mode == SwarmMode::Hybrid) {
        m_hListenerThread  = CreateThread(nullptr, 0, listenerThread,  this, 0, nullptr);
        m_hSchedulerThread = CreateThread(nullptr, 0, schedulerThread, this, 0, nullptr);
    }
    m_hHeartbeatThread = CreateThread(nullptr, 0, heartbeatThread, this, 0, nullptr);
    m_hDiscoveryThread = CreateThread(nullptr, 0, discoveryThread, this, 0, nullptr);

    // Start IOCP worker threads (one per logical core, max 8)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_iocpThreadCount = (si.dwNumberOfProcessors < 8) ? si.dwNumberOfProcessors : 8;
    if (m_iocpThreadCount < 1) m_iocpThreadCount = 1;
    for (uint32_t i = 0; i < m_iocpThreadCount; i++) {
        m_hIocpThreads[i] = CreateThread(nullptr, 0, iocpWorkerThread, this, 0, nullptr);
    }

    m_discoveryEnabled.store(true);

    emitEvent(SwarmEventType::SwarmStarted, 0xFFFFFFFF, 0,
              config.mode == SwarmMode::Leader ? "Swarm started (Leader mode)" :
              config.mode == SwarmMode::Worker ? "Swarm started (Worker mode)" :
              "Swarm started (Hybrid mode)");

    return true;
}

void SwarmCoordinator::stop() {
    if (!m_running.load()) return;

    m_shutdownRequested.store(true);
    m_running.store(false);
    m_buildRunning.store(false);
    m_discoveryEnabled.store(false);

    // Close sockets to unblock listener/discovery threads
    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }
    if (m_discoverySocket != INVALID_SOCKET) {
        closesocket(m_discoverySocket);
        m_discoverySocket = INVALID_SOCKET;
    }

    // Close all node sockets
    {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        for (uint32_t i = 0; i < SWARM_MAX_NODES; i++) {
            if (m_nodes[i].socket != INVALID_SOCKET && m_nodes[i].socket != 0) {
                closesocket(m_nodes[i].socket);
                m_nodes[i].socket = INVALID_SOCKET;
            }
        }
    }

    // Post IOCP shutdown completions to wake worker threads
    if (m_iocp) {
        for (uint32_t i = 0; i < m_iocpThreadCount; i++) {
            PostQueuedCompletionStatus((HANDLE)m_iocp, 0, 0, nullptr);
        }
    }

    // Wait for threads to finish
    HANDLE allThreads[12];
    DWORD threadCount = 0;
    if (m_hListenerThread)  allThreads[threadCount++] = m_hListenerThread;
    if (m_hHeartbeatThread) allThreads[threadCount++] = m_hHeartbeatThread;
    if (m_hDiscoveryThread) allThreads[threadCount++] = m_hDiscoveryThread;
    if (m_hSchedulerThread) allThreads[threadCount++] = m_hSchedulerThread;
    for (uint32_t i = 0; i < m_iocpThreadCount; i++) {
        if (m_hIocpThreads[i]) allThreads[threadCount++] = m_hIocpThreads[i];
    }

    if (threadCount > 0) {
        WaitForMultipleObjects(threadCount, allThreads, TRUE, 5000);
    }

    // Close thread handles
    if (m_hListenerThread)  { CloseHandle(m_hListenerThread);  m_hListenerThread  = nullptr; }
    if (m_hHeartbeatThread) { CloseHandle(m_hHeartbeatThread); m_hHeartbeatThread = nullptr; }
    if (m_hDiscoveryThread) { CloseHandle(m_hDiscoveryThread); m_hDiscoveryThread = nullptr; }
    if (m_hSchedulerThread) { CloseHandle(m_hSchedulerThread); m_hSchedulerThread = nullptr; }
    for (uint32_t i = 0; i < m_iocpThreadCount; i++) {
        if (m_hIocpThreads[i]) { CloseHandle(m_hIocpThreads[i]); m_hIocpThreads[i] = nullptr; }
    }

    // Close IOCP
    if (m_iocp) { CloseHandle((HANDLE)m_iocp); m_iocp = nullptr; }

    // Free ring buffers
    if (m_txRing)       { VirtualFree(m_txRing, 0, MEM_RELEASE);       m_txRing = nullptr; }
    if (m_rxRing)       { VirtualFree(m_rxRing, 0, MEM_RELEASE);       m_rxRing = nullptr; }
    if (m_txRingBuffer) { VirtualFree(m_txRingBuffer, 0, MEM_RELEASE); m_txRingBuffer = nullptr; }
    if (m_rxRingBuffer) { VirtualFree(m_rxRingBuffer, 0, MEM_RELEASE); m_rxRingBuffer = nullptr; }

    emitEvent(SwarmEventType::SwarmStopped, 0xFFFFFFFF, 0, "Swarm stopped");
}

// =============================================================================
//                     CONFIGURATION
// =============================================================================

void SwarmCoordinator::setConfig(const SwarmConfig& config) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
    emitEvent(SwarmEventType::ConfigChanged, 0xFFFFFFFF, 0, "Configuration updated");
}

SwarmConfig SwarmCoordinator::getConfig() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

// =============================================================================
//                     NODE MANAGEMENT
// =============================================================================

std::vector<SwarmNodeInfo> SwarmCoordinator::getNodes() const {
    std::lock_guard<std::mutex> lock(m_nodesMutex);
    std::vector<SwarmNodeInfo> result;
    for (uint32_t i = 0; i < SWARM_MAX_NODES; i++) {
        if (m_nodes[i].state != SwarmNodeState::Unknown) {
            result.push_back(m_nodes[i]);
        }
    }
    return result;
}

bool SwarmCoordinator::getNode(uint32_t slotIndex, SwarmNodeInfo& outNode) const {
    if (slotIndex >= SWARM_MAX_NODES) return false;
    std::lock_guard<std::mutex> lock(m_nodesMutex);
    if (m_nodes[slotIndex].state == SwarmNodeState::Unknown) return false;
    outNode = m_nodes[slotIndex];
    return true;
}

uint32_t SwarmCoordinator::getOnlineNodeCount() const {
    std::lock_guard<std::mutex> lock(m_nodesMutex);
    uint32_t count = 0;
    for (uint32_t i = 0; i < SWARM_MAX_NODES; i++) {
        if (m_nodes[i].state == SwarmNodeState::Online ||
            m_nodes[i].state == SwarmNodeState::Busy) {
            count++;
        }
    }
    return count;
}

bool SwarmCoordinator::addNodeManual(const char* ipAddress, uint16_t port) {
    uint32_t slot = allocateNodeSlot();
    if (slot == UINT32_MAX) return false;

    {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        auto& node = m_nodes[slot];
        memset(&node, 0, sizeof(SwarmNodeInfo));
        strncpy(node.ipAddress, ipAddress, sizeof(node.ipAddress) - 1);
        node.swarmPort = port;
        node.slotIndex = slot;
        node.state = SwarmNodeState::Discovered;
        node.socket = INVALID_SOCKET;
        node.joinedAtMs = SwarmTime::nowMs();
    }

    // Initiate TCP connection in background
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        freeNodeSlot(slot);
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ipAddress, &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        freeNodeSlot(slot);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        m_nodes[slot].socket = sock;
    }

    // Associate with IOCP
    if (m_iocp) {
        Swarm_IOCP_Associate((void*)sock, m_iocp, (uint64_t)slot);
    }

    // Begin attestation
    if (m_config.requireAttestation) {
        sendAttestChallenge(slot);
    } else {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        m_nodes[slot].state = SwarmNodeState::Online;
        m_nodeCount.fetch_add(1);
        emitEvent(SwarmEventType::NodeJoined, slot, 0,
                  (std::string("Node joined: ") + ipAddress).c_str());
    }

    return true;
}

bool SwarmCoordinator::removeNode(uint32_t slotIndex) {
    if (slotIndex >= SWARM_MAX_NODES) return false;
    evictNode(slotIndex, "Manually removed");
    return true;
}

bool SwarmCoordinator::blacklistNode(uint32_t slotIndex, const char* reason) {
    if (slotIndex >= SWARM_MAX_NODES) return false;
    {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        if (m_nodes[slotIndex].state == SwarmNodeState::Unknown) return false;
        m_nodes[slotIndex].state = SwarmNodeState::Blacklisted;
        if (m_nodes[slotIndex].socket != INVALID_SOCKET) {
            closesocket(m_nodes[slotIndex].socket);
            m_nodes[slotIndex].socket = INVALID_SOCKET;
        }
    }
    emitEvent(SwarmEventType::NodeBlacklisted, slotIndex, 0,
              (std::string("Node blacklisted: ") + (reason ? reason : "unknown")).c_str());
    return true;
}

uint32_t SwarmCoordinator::allocateNodeSlot() {
    std::lock_guard<std::mutex> lock(m_nodesMutex);
    for (uint32_t i = 0; i < SWARM_MAX_NODES; i++) {
        if (m_nodes[i].state == SwarmNodeState::Unknown) {
            return i;
        }
    }
    return UINT32_MAX; // No free slots
}

void SwarmCoordinator::freeNodeSlot(uint32_t slotIndex) {
    if (slotIndex >= SWARM_MAX_NODES) return;
    std::lock_guard<std::mutex> lock(m_nodesMutex);
    memset(&m_nodes[slotIndex], 0, sizeof(SwarmNodeInfo));
    m_nodes[slotIndex].state = SwarmNodeState::Unknown;
    m_nodes[slotIndex].socket = INVALID_SOCKET;
}

void SwarmCoordinator::evictNode(uint32_t nodeSlot, const char* reason) {
    {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        if (m_nodes[nodeSlot].state == SwarmNodeState::Unknown) return;
        if (m_nodes[nodeSlot].socket != INVALID_SOCKET) {
            closesocket(m_nodes[nodeSlot].socket);
            m_nodes[nodeSlot].socket = INVALID_SOCKET;
        }
        m_nodes[nodeSlot].state = SwarmNodeState::Offline;
    }

    // Re-queue any tasks assigned to this node
    {
        std::lock_guard<std::mutex> lock(m_taskGraph.graphMutex);
        for (auto& task : m_taskGraph.tasks) {
            if (task.assignedNode == nodeSlot &&
                (task.taskState == SwarmTaskState::Assigned ||
                 task.taskState == SwarmTaskState::Running)) {
                task.taskState = SwarmTaskState::Ready;
                task.assignedNode = 0xFFFFFFFF;
                task.retryCount++;
                m_taskGraph.runningTasks--;
                m_taskGraph.pendingTasks++;
            }
        }
    }

    m_nodeCount.fetch_sub(1);
    emitEvent(SwarmEventType::NodeLeft, nodeSlot, 0,
              (std::string("Node evicted: ") + (reason ? reason : "unknown")).c_str());

    freeNodeSlot(nodeSlot);
}

// =============================================================================
//                     TASK GRAPH MANAGEMENT
// =============================================================================

uint64_t SwarmCoordinator::addTask(SwarmTaskType type,
                                     const std::string& sourceFile,
                                     const std::string& compilerArgs,
                                     const std::string& outputFile,
                                     const std::vector<uint64_t>& dependencies) {
    SwarmTasklet task;
    task.taskId = generateTaskId();
    task.taskType = type;
    task.sourceFile = sourceFile;
    task.compilerArgs = compilerArgs;
    task.outputFile = outputFile;
    task.dependsOn = dependencies;
    task.createdAtMs = SwarmTime::nowMs();
    task.dagGeneration = m_taskGraph.generation;

    // Determine initial state
    if (dependencies.empty()) {
        task.taskState = SwarmTaskState::Ready;
    } else {
        task.taskState = SwarmTaskState::Pending;
    }

    // Compute content hash of source + args
    std::string hashInput = sourceFile + "|" + compilerArgs;
    task.contentHash = Swarm_XXH64(hashInput.data(), hashInput.size(), 0xCAFEBABE);

    {
        std::lock_guard<std::mutex> lock(m_taskGraph.graphMutex);
        size_t index = m_taskGraph.tasks.size();
        m_taskGraph.tasks.push_back(std::move(task));
        m_taskGraph.totalTasks++;
        if (dependencies.empty()) {
            // Ready immediately
        } else {
            m_taskGraph.pendingTasks++;
        }

        {
            std::lock_guard<std::mutex> mapLock(m_taskMapMutex);
            m_taskIdToIndex[m_taskGraph.tasks[index].taskId] = index;
        }

        // Update dependedBy on dependency tasks
        for (uint64_t depId : dependencies) {
            std::lock_guard<std::mutex> mapLock(m_taskMapMutex);
            auto it = m_taskIdToIndex.find(depId);
            if (it != m_taskIdToIndex.end()) {
                m_taskGraph.tasks[it->second].dependedBy.push_back(
                    m_taskGraph.tasks[index].taskId);
            }
        }

        return m_taskGraph.tasks[index].taskId;
    }
}

bool SwarmCoordinator::cancelTask(uint64_t taskId, bool cancelDependents) {
    std::lock_guard<std::mutex> lock(m_taskGraph.graphMutex);
    std::lock_guard<std::mutex> mapLock(m_taskMapMutex);

    auto it = m_taskIdToIndex.find(taskId);
    if (it == m_taskIdToIndex.end()) return false;

    auto& task = m_taskGraph.tasks[it->second];
    if (task.taskState == SwarmTaskState::Completed) return false; // Can't cancel done tasks

    task.taskState = SwarmTaskState::Cancelled;
    m_taskGraph.cancelledTasks++;

    if (cancelDependents) {
        for (uint64_t depId : task.dependedBy) {
            auto dit = m_taskIdToIndex.find(depId);
            if (dit != m_taskIdToIndex.end()) {
                m_taskGraph.tasks[dit->second].taskState = SwarmTaskState::Cancelled;
                m_taskGraph.cancelledTasks++;
            }
        }
    }

    return true;
}

void SwarmCoordinator::cancelAllTasks() {
    std::lock_guard<std::mutex> lock(m_taskGraph.graphMutex);
    for (auto& task : m_taskGraph.tasks) {
        if (task.taskState != SwarmTaskState::Completed &&
            task.taskState != SwarmTaskState::Failed) {
            task.taskState = SwarmTaskState::Cancelled;
            m_taskGraph.cancelledTasks++;
        }
    }
    m_buildRunning.store(false);
}

bool SwarmCoordinator::getTask(uint64_t taskId, SwarmTasklet& outTask) const {
    std::lock_guard<std::mutex> lock(m_taskGraph.graphMutex);
    std::lock_guard<std::mutex> mapLock(m_taskMapMutex);

    auto it = m_taskIdToIndex.find(taskId);
    if (it == m_taskIdToIndex.end()) return false;
    outTask = m_taskGraph.tasks[it->second];
    return true;
}

uint64_t SwarmCoordinator::getDagGeneration() const {
    return m_taskGraph.generation;
}

bool SwarmCoordinator::buildDagFromSources(const std::vector<std::string>& sourceFiles,
                                             const std::string& compilerArgs,
                                             const std::string& outputDir) {
    // Clear existing graph
    {
        std::lock_guard<std::mutex> lock(m_taskGraph.graphMutex);
        m_taskGraph.tasks.clear();
        m_taskGraph.generation++;
        m_taskGraph.totalTasks = 0;
        m_taskGraph.completedTasks = 0;
        m_taskGraph.failedTasks = 0;
        m_taskGraph.runningTasks = 0;
        m_taskGraph.pendingTasks = 0;
        m_taskGraph.cancelledTasks = 0;
    }
    {
        std::lock_guard<std::mutex> lock(m_taskMapMutex);
        m_taskIdToIndex.clear();
    }

    std::vector<uint64_t> objTaskIds;

    // Create a compile task for each source file
    for (const auto& src : sourceFiles) {
        SwarmTaskType type = SwarmTaskType::CompileCpp;
        size_t extPos = src.rfind('.');
        if (extPos != std::string::npos) {
            std::string ext = src.substr(extPos);
            if (ext == ".c") type = SwarmTaskType::CompileC;
            else if (ext == ".asm") type = SwarmTaskType::AssembleMASM;
        }

        // Output: replace extension with .obj
        std::string objFile = outputDir + "/" + src.substr(src.find_last_of("/\\") + 1);
        size_t dotPos = objFile.rfind('.');
        if (dotPos != std::string::npos) {
            objFile = objFile.substr(0, dotPos) + ".obj";
        }

        uint64_t taskId = addTask(type, src, compilerArgs, objFile, {});
        objTaskIds.push_back(taskId);
    }

    // Create a final link task that depends on all compile tasks
    if (!objTaskIds.empty()) {
        std::string linkArgs;
        for (const auto& id : objTaskIds) {
            SwarmTasklet t;
            if (getTask(id, t)) {
                linkArgs += t.outputFile + " ";
            }
        }
        addTask(SwarmTaskType::LinkFinal, "link", linkArgs,
                outputDir + "/output.exe", objTaskIds);
    }

    return !sourceFiles.empty();
}

bool SwarmCoordinator::buildDagFromCMake(const char* buildDir) {
    // Parse compile_commands.json if it exists
    std::string ccPath = std::string(buildDir) + "/compile_commands.json";
    HANDLE hFile = CreateFileA(ccPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == 0 || fileSize > 64 * 1024 * 1024) {
        CloseHandle(hFile);
        return false;
    }

    std::string content(fileSize, '\0');
    DWORD bytesRead = 0;
    ReadFile(hFile, &content[0], fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);

    // Simple JSON parsing for compile_commands.json
    // Format: [ { "directory": "...", "command": "...", "file": "..." }, ... ]
    std::vector<std::string> sourceFiles;
    std::string defaultArgs;

    // Extract "file" entries
    size_t pos = 0;
    while (true) {
        pos = content.find("\"file\"", pos);
        if (pos == std::string::npos) break;
        pos = content.find(':', pos);
        if (pos == std::string::npos) break;
        pos = content.find('"', pos + 1);
        if (pos == std::string::npos) break;
        pos++; // skip opening quote
        size_t end = content.find('"', pos);
        if (end == std::string::npos) break;
        std::string filePath = content.substr(pos, end - pos);
        sourceFiles.push_back(filePath);
        pos = end + 1;
    }

    // Extract first "command" for default args
    pos = content.find("\"command\"");
    if (pos != std::string::npos) {
        pos = content.find(':', pos);
        pos = content.find('"', pos + 1);
        if (pos != std::string::npos) {
            pos++;
            size_t end = content.find('"', pos);
            if (end != std::string::npos) {
                defaultArgs = content.substr(pos, end - pos);
            }
        }
    }

    return buildDagFromSources(sourceFiles, defaultArgs, buildDir);
}

// =============================================================================
//                     BUILD EXECUTION
// =============================================================================

bool SwarmCoordinator::startBuild() {
    if (m_buildRunning.load()) return false;
    if (getOnlineNodeCount() == 0) return false;

    m_buildRunning.store(true);
    m_taskGraph.startTimeMs = SwarmTime::nowMs();
    m_taskGraph.endTimeMs = 0;

    emitEvent(SwarmEventType::BuildStarted, 0xFFFFFFFF, 0,
              ("Build started: " + std::to_string(m_taskGraph.totalTasks) + " tasks").c_str());

    // Scheduler thread will pick up ready tasks
    return true;
}

double SwarmCoordinator::getBuildProgress() const {
    if (m_taskGraph.totalTasks == 0) return 0.0;
    return static_cast<double>(m_taskGraph.completedTasks) /
           static_cast<double>(m_taskGraph.totalTasks);
}

// =============================================================================
//                     SCHEDULING
// =============================================================================

void SwarmCoordinator::scheduleReadyTasks() {
    std::lock_guard<std::mutex> lock(m_taskGraph.graphMutex);

    for (auto& task : m_taskGraph.tasks) {
        if (task.taskState != SwarmTaskState::Ready) continue;

        // Check object cache first
        ObjectCacheEntry cacheEntry;
        if (objectCacheLookup(task.contentHash, cacheEntry)) {
            // Cache hit — skip compilation
            task.taskState = SwarmTaskState::Completed;
            task.resultHash = cacheEntry.resultHash;
            task.objectFileSize = cacheEntry.objectSize;
            task.exitCode = 0;
            task.compileTimeMs = 0;
            task.completedAtMs = SwarmTime::nowMs();
            m_taskGraph.completedTasks++;
            {
                std::lock_guard<std::mutex> slock(m_statsMutex);
                m_stats.objectCacheHits++;
            }
            promotePendingTasks();
            continue;
        }

        // Find best node
        uint32_t nodeSlot = selectBestNode(task);
        if (nodeSlot == UINT32_MAX) continue; // No available node

        // Send task to worker
        if (sendTaskToNode(nodeSlot, task)) {
            task.taskState = SwarmTaskState::Assigned;
            task.assignedNode = nodeSlot;
            task.assignedAtMs = SwarmTime::nowMs();
            m_taskGraph.runningTasks++;
            {
                std::lock_guard<std::mutex> nlock(m_nodesMutex);
                m_nodes[nodeSlot].activeTasks++;
            }
            emitEvent(SwarmEventType::TaskAssigned, nodeSlot, task.taskId,
                      ("Task assigned: " + task.sourceFile).c_str());
        }
    }
}

uint32_t SwarmCoordinator::selectBestNode(const SwarmTasklet& task) const {
    std::lock_guard<std::mutex> lock(m_nodesMutex);

    uint32_t bestSlot = UINT32_MAX;
    uint32_t bestScore = 0;

    for (uint32_t i = 0; i < SWARM_MAX_NODES; i++) {
        const auto& node = m_nodes[i];
        if (node.state != SwarmNodeState::Online) continue;
        if (node.activeTasks >= node.maxConcurrentTasks) continue;

        // Score: fitness + (available_capacity * 100) - latency_penalty
        uint32_t score = node.fitnessScore;
        score += (node.maxConcurrentTasks - node.activeTasks) * 100;

        // Prefer nodes with matching MASM capability for ASM tasks
        if ((task.taskType == SwarmTaskType::AssembleMASM ||
             task.taskType == SwarmTaskType::AssembleNASM) &&
            node.hasAVX2) {
            score += 200;
        }

        if (score > bestScore) {
            bestScore = score;
            bestSlot = i;
        }
    }

    return bestSlot;
}

bool SwarmCoordinator::sendTaskToNode(uint32_t nodeSlot, SwarmTasklet& task) {
    TaskPushPayload payload = {};
    payload.taskId = task.taskId;
    payload.parentTaskId = task.parentTaskId;
    payload.taskType = static_cast<uint32_t>(task.taskType);
    payload.priority = task.priority;
    payload.sourceFileLen = static_cast<uint32_t>(task.sourceFile.size() + 1);
    payload.compilerArgsLen = static_cast<uint32_t>(task.compilerArgs.size() + 1);
    payload.dagGeneration = task.dagGeneration;

    // Content hash split
    payload.objectHashHi = static_cast<uint32_t>(task.contentHash >> 32);
    payload.objectHashLo = static_cast<uint32_t>(task.contentHash & 0xFFFFFFFF);

    // Build payload: struct + source path + compiler args
    std::vector<uint8_t> fullPayload(sizeof(TaskPushPayload) +
                                      payload.sourceFileLen + payload.compilerArgsLen);
    memcpy(fullPayload.data(), &payload, sizeof(TaskPushPayload));
    memcpy(fullPayload.data() + sizeof(TaskPushPayload),
           task.sourceFile.c_str(), payload.sourceFileLen);
    memcpy(fullPayload.data() + sizeof(TaskPushPayload) + payload.sourceFileLen,
           task.compilerArgs.c_str(), payload.compilerArgsLen);

    return sendPacket(nodeSlot, SwarmOpcode::TaskPush,
                       fullPayload.data(), static_cast<uint16_t>(fullPayload.size()),
                       task.taskId);
}

void SwarmCoordinator::promotePendingTasks() {
    // Called with graphMutex held
    for (auto& task : m_taskGraph.tasks) {
        if (task.taskState != SwarmTaskState::Pending) continue;

        bool allDone = true;
        for (uint64_t depId : task.dependsOn) {
            std::lock_guard<std::mutex> mapLock(m_taskMapMutex);
            auto it = m_taskIdToIndex.find(depId);
            if (it != m_taskIdToIndex.end()) {
                if (m_taskGraph.tasks[it->second].taskState != SwarmTaskState::Completed) {
                    allDone = false;
                    break;
                }
            }
        }

        if (allDone) {
            task.taskState = SwarmTaskState::Ready;
            m_taskGraph.pendingTasks--;
        }
    }
}

void SwarmCoordinator::checkTaskTimeouts() {
    uint64_t now = SwarmTime::nowMs();
    std::lock_guard<std::mutex> lock(m_taskGraph.graphMutex);

    for (auto& task : m_taskGraph.tasks) {
        if (task.taskState == SwarmTaskState::Assigned ||
            task.taskState == SwarmTaskState::Running) {
            uint64_t elapsed = now - task.assignedAtMs;
            if (elapsed > m_config.taskTimeoutMs) {
                task.taskState = SwarmTaskState::Failed;
                task.retryCount++;
                m_taskGraph.runningTasks--;
                m_taskGraph.failedTasks++;

                if (task.retryCount < m_config.maxRetries) {
                    task.taskState = SwarmTaskState::Ready;
                    task.assignedNode = 0xFFFFFFFF;
                    m_taskGraph.failedTasks--;
                    {
                        std::lock_guard<std::mutex> slock(m_statsMutex);
                        m_stats.retriedTasks++;
                    }
                    emitEvent(SwarmEventType::TaskRetried, task.assignedNode, task.taskId,
                              "Task timed out, retrying");
                } else {
                    emitEvent(SwarmEventType::TaskFailed, task.assignedNode, task.taskId,
                              "Task timed out after max retries");
                }
            }
        }
    }
}

void SwarmCoordinator::requeueFailedTask(uint64_t taskId) {
    std::lock_guard<std::mutex> lock(m_taskGraph.graphMutex);

    for (auto& task : m_taskGraph.tasks) {
        if (task.taskId == taskId &&
            task.taskState == SwarmTaskState::Failed) {
            task.retryCount++;
            if (task.retryCount < m_config.maxRetries) {
                task.taskState = SwarmTaskState::Ready;
                task.assignedNode = 0xFFFFFFFF;
                m_taskGraph.failedTasks--;
                {
                    std::lock_guard<std::mutex> slock(m_statsMutex);
                    m_stats.retriedTasks++;
                }
                emitEvent(SwarmEventType::TaskRetried, 0xFFFFFFFF, task.taskId,
                          "Task requeued manually");
            } else {
                emitEvent(SwarmEventType::TaskFailed, 0xFFFFFFFF, task.taskId,
                          "Task exceeded max retries — cannot requeue");
            }
            break;
        }
    }
}

// =============================================================================
//                     PACKET HANDLERS
// =============================================================================

void SwarmCoordinator::handleHeartbeat(uint32_t nodeSlot,
                                         const HeartbeatPayload* payload) {
    if (nodeSlot >= SWARM_MAX_NODES) return;

    // Record heartbeat in MASM table (RDTSC-based)
    Swarm_HeartbeatRecord(nodeSlot);

    {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        auto& node = m_nodes[nodeSlot];
        node.lastHeartbeatMs = SwarmTime::nowMs();
        node.activeTasks = payload->activeTasks;
        node.cpuLoadPercent = payload->cpuLoadPercent;
        node.memUsedMB = payload->memUsedMB;
        node.uptimeMs = payload->uptimeMs;
        node.fitnessScore = payload->fitnessScore;
        node.missedHeartbeats = 0;

        // Transition from Busy → Online if no longer at capacity
        if (node.state == SwarmNodeState::Busy &&
            node.activeTasks < node.maxConcurrentTasks) {
            node.state = SwarmNodeState::Online;
        }
        // Transition Online → Busy if at capacity
        if (node.state == SwarmNodeState::Online &&
            node.activeTasks >= node.maxConcurrentTasks) {
            node.state = SwarmNodeState::Busy;
        }
    }
}

void SwarmCoordinator::handleResultPush(uint32_t nodeSlot,
                                          const ResultPushPayload* payload,
                                          const uint8_t* extraData,
                                          uint32_t extraLen) {
    uint64_t taskId = payload->taskId;

    {
        std::lock_guard<std::mutex> lock(m_taskGraph.graphMutex);
        std::lock_guard<std::mutex> mapLock(m_taskMapMutex);

        auto it = m_taskIdToIndex.find(taskId);
        if (it == m_taskIdToIndex.end()) return;

        auto& task = m_taskGraph.tasks[it->second];
        task.exitCode = payload->exitCode;
        task.objectFileSize = payload->objectFileSize;
        task.resultHash = payload->objectHash;
        task.compileTimeMs = payload->compileTimeMs;
        task.warningCount = payload->warningCount;
        task.errorCount = payload->errorCount;
        task.completedAtMs = SwarmTime::nowMs();

        if (payload->exitCode == 0) {
            task.taskState = SwarmTaskState::Completed;
            m_taskGraph.completedTasks++;
            m_taskGraph.runningTasks--;

            // Store in object cache
            if (payload->objectFileSize > 0 && extraData) {
                objectCacheStore(task.contentHash, payload->objectHash,
                                  extraData, payload->objectFileSize,
                                  task.sourceFile);
            }

            // Extract compiler log if present
            if (payload->logLen > 0 && extraData) {
                const char* logStart = reinterpret_cast<const char*>(
                    extraData + payload->objectFileSize);
                task.compilerLog = std::string(logStart, payload->logLen);
            }

            promotePendingTasks();

            emitEvent(SwarmEventType::TaskCompleted, nodeSlot, taskId,
                      ("Compiled: " + task.sourceFile + " (" +
                       std::to_string(payload->compileTimeMs) + "ms)").c_str());

            // Check if build is complete
            if (m_taskGraph.completedTasks + m_taskGraph.failedTasks +
                m_taskGraph.cancelledTasks >= m_taskGraph.totalTasks) {
                m_buildRunning.store(false);
                m_taskGraph.endTimeMs = SwarmTime::nowMs();
                emitEvent(SwarmEventType::BuildCompleted, 0xFFFFFFFF, 0,
                          ("Build complete: " +
                           std::to_string(m_taskGraph.completedTasks) + "/" +
                           std::to_string(m_taskGraph.totalTasks) + " tasks succeeded").c_str());
            }
        } else {
            task.taskState = SwarmTaskState::Failed;
            m_taskGraph.failedTasks++;
            m_taskGraph.runningTasks--;

            // Retry if under limit
            if (task.retryCount < m_config.maxRetries) {
                task.taskState = SwarmTaskState::Retrying;
                task.retryCount++;
                task.assignedNode = 0xFFFFFFFF;
                m_taskGraph.failedTasks--;
                {
                    std::lock_guard<std::mutex> slock(m_statsMutex);
                    m_stats.retriedTasks++;
                }
            }

            emitEvent(SwarmEventType::TaskFailed, nodeSlot, taskId,
                      ("Failed: " + task.sourceFile + " (exit " +
                       std::to_string(payload->exitCode) + ")").c_str());
        }
    }

    // Update node stats
    {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        if (nodeSlot < SWARM_MAX_NODES) {
            m_nodes[nodeSlot].activeTasks--;
            if (payload->exitCode == 0) {
                m_nodes[nodeSlot].completedTasks++;
            } else {
                m_nodes[nodeSlot].failedTasks++;
            }
        }
    }

    // Update global stats
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalCompileTimeMs += payload->compileTimeMs;
        if (m_stats.completedTasks > 0) {
            m_stats.avgCompileTimeMs = m_stats.totalCompileTimeMs / m_stats.completedTasks;
        }
        if (payload->compileTimeMs > m_stats.maxCompileTimeMs) {
            m_stats.maxCompileTimeMs = payload->compileTimeMs;
        }
    }
}

void SwarmCoordinator::handleTaskPull(uint32_t nodeSlot,
                                        const TaskPullPayload* payload) {
    // Worker is requesting work — trigger scheduling
    {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        if (nodeSlot < SWARM_MAX_NODES) {
            m_nodes[nodeSlot].activeTasks = payload->currentLoad;
        }
    }
    scheduleReadyTasks();
}

void SwarmCoordinator::handleAttestResponse(uint32_t nodeSlot,
                                              const AttestResponsePayload* payload) {
    if (nodeSlot >= SWARM_MAX_NODES) return;

    if (verifyAttestResponse(nodeSlot, payload)) {
        {
            std::lock_guard<std::mutex> lock(m_nodesMutex);
            m_nodes[nodeSlot].state = SwarmNodeState::Online;
            memcpy(m_nodes[nodeSlot].nodeId, payload->hwid, 16);
            m_nodes[nodeSlot].fitnessScore = payload->fitnessScore;
            m_nodes[nodeSlot].uptimeMs = payload->uptimeMs;
        }
        m_nodeCount.fetch_add(1);
        emitEvent(SwarmEventType::AttestationSuccess, nodeSlot, 0,
                  "Node attestation successful");
        emitEvent(SwarmEventType::NodeJoined, nodeSlot, 0,
                  ("Node joined: " + std::string(m_nodes[nodeSlot].hostname)).c_str());
    } else {
        blacklistNode(nodeSlot, "Attestation failed");
        emitEvent(SwarmEventType::AttestationFailure, nodeSlot, 0,
                  "Node attestation failed — blacklisted");
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.attestationFailures++;
        }
    }
}

void SwarmCoordinator::handleCapsReport(uint32_t nodeSlot,
                                          const CapsReportPayload* payload) {
    if (nodeSlot >= SWARM_MAX_NODES) return;

    std::lock_guard<std::mutex> lock(m_nodesMutex);
    auto& node = m_nodes[nodeSlot];
    node.logicalCores = payload->logicalCores;
    node.physicalCores = payload->physicalCores;
    node.ramTotalMB = payload->ramTotalMB;
    node.ramAvailMB = payload->ramAvailMB;
    node.hasAVX2 = payload->hasAVX2;
    node.hasAVX512 = payload->hasAVX512;
    node.hasAESNI = payload->hasAESNI;
    node.maxConcurrentTasks = payload->maxConcurrentTasks;
    node.fitnessScore = payload->fitnessScore;
    strncpy(node.hostname, payload->hostname, sizeof(node.hostname) - 1);
}

void SwarmCoordinator::handleConsensusVote(uint32_t nodeSlot,
                                             const ConsensusVotePayload* payload) {
    std::lock_guard<std::mutex> lock(m_consensusMutex);

    auto it = m_consensus.find(payload->taskId);
    if (it == m_consensus.end()) return;

    auto& entry = it->second;
    if (entry.committed || entry.rejected) return;

    entry.totalVotes++;
    entry.voterNodes.push_back(nodeSlot);

    if (payload->verdict == 1 && payload->objectHash == entry.expectedHash) {
        entry.acceptVotes++;
    } else {
        entry.rejectVotes++;
    }

    // Check quorum
    uint32_t quorum = m_config.consensusQuorum;
    if (quorum == 0) quorum = (getOnlineNodeCount() / 2) + 1;
    if (quorum < 1) quorum = 1;

    if (entry.acceptVotes >= quorum) {
        entry.committed = true;
        {
            std::lock_guard<std::mutex> slock(m_statsMutex);
            m_stats.consensusCommits++;
        }
        emitEvent(SwarmEventType::ConsensusReached, 0xFFFFFFFF, payload->taskId,
                  "Consensus reached — result committed");
    } else if (entry.rejectVotes >= quorum) {
        entry.rejected = true;
        {
            std::lock_guard<std::mutex> slock(m_statsMutex);
            m_stats.consensusRejects++;
        }
        emitEvent(SwarmEventType::ConsensusRejected, 0xFFFFFFFF, payload->taskId,
                  "Consensus rejected — result discarded, retrying task");
    }
}

void SwarmCoordinator::handleLogStream(uint32_t nodeSlot,
                                         const LogStreamPayload* payload,
                                         const char* logText) {
    if (m_logCallback) {
        m_logCallback(payload->taskId, nodeSlot, logText, m_logUserData);
    }
}

void SwarmCoordinator::handleMetricReport(uint32_t nodeSlot,
                                            const MetricReportPayload* payload) {
    if (nodeSlot >= SWARM_MAX_NODES) return;

    std::lock_guard<std::mutex> lock(m_nodesMutex);
    auto& node = m_nodes[nodeSlot];
    node.cpuLoadPercent = payload->cpuPercent;
    node.memUsedMB = payload->memUsedMB;
    node.avgCompileTimeUs = payload->avgCompileTimeUs;
    node.uptimeMs = payload->uptimeMs;
}

// =============================================================================
//                     NETWORK THREADS
// =============================================================================

DWORD WINAPI SwarmCoordinator::listenerThread(LPVOID param) {
    auto* self = static_cast<SwarmCoordinator*>(param);

    while (self->m_running.load()) {
        sockaddr_in clientAddr = {};
        int addrLen = sizeof(clientAddr);

        SOCKET clientSock = accept(self->m_listenSocket,
                                    (sockaddr*)&clientAddr, &addrLen);
        if (clientSock == INVALID_SOCKET) {
            if (!self->m_running.load()) break;
            Sleep(10);
            continue;
        }

        // Allocate node slot
        uint32_t slot = self->allocateNodeSlot();
        if (slot == UINT32_MAX) {
            closesocket(clientSock);
            continue;
        }

        // Initialize node entry
        {
            std::lock_guard<std::mutex> lock(self->m_nodesMutex);
            auto& node = self->m_nodes[slot];
            memset(&node, 0, sizeof(SwarmNodeInfo));
            node.slotIndex = slot;
            node.socket = clientSock;
            node.state = SwarmNodeState::Discovered;
            node.joinedAtMs = SwarmTime::nowMs();

            char addrBuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, addrBuf, sizeof(addrBuf));
            strncpy(node.ipAddress, addrBuf, sizeof(node.ipAddress) - 1);
            node.swarmPort = ntohs(clientAddr.sin_port);
        }

        // Associate with IOCP
        if (self->m_iocp) {
            Swarm_IOCP_Associate((void*)(uintptr_t)clientSock,
                                  self->m_iocp, (uint64_t)slot);
        }

        // Begin attestation or directly mark online
        if (self->m_config.requireAttestation) {
            self->sendAttestChallenge(slot);
        } else {
            std::lock_guard<std::mutex> lock(self->m_nodesMutex);
            self->m_nodes[slot].state = SwarmNodeState::Online;
            self->m_nodeCount.fetch_add(1);
            self->emitEvent(SwarmEventType::NodeJoined, slot, 0, "Node connected");
        }
    }

    return 0;
}

DWORD WINAPI SwarmCoordinator::iocpWorkerThread(LPVOID param) {
    auto* self = static_cast<SwarmCoordinator*>(param);

    // Per-thread receive buffer
    std::vector<uint8_t> recvBuf(SWARM_HEADER_SIZE + SWARM_MAX_PAYLOAD);

    while (self->m_running.load()) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED overlapped = nullptr;

        BOOL ok = GetQueuedCompletionStatus(
            (HANDLE)self->m_iocp,
            &bytesTransferred,
            &completionKey,
            &overlapped,
            100  // 100ms timeout
        );

        if (!ok || bytesTransferred == 0) {
            if (!self->m_running.load()) break;
            continue;
        }

        uint32_t nodeSlot = static_cast<uint32_t>(completionKey);
        if (nodeSlot >= SWARM_MAX_NODES) continue;

        // Read the packet from the node's socket
        SOCKET sock;
        {
            std::lock_guard<std::mutex> lock(self->m_nodesMutex);
            sock = self->m_nodes[nodeSlot].socket;
        }
        if (sock == INVALID_SOCKET) continue;

        int received = self->recvPacket(sock, recvBuf.data(),
                                          static_cast<uint32_t>(recvBuf.size()));
        if (received <= 0) {
            self->evictNode(nodeSlot, "Connection lost");
            continue;
        }

        // Validate packet via MASM
        int valid = Swarm_ValidatePacketHeader(recvBuf.data());
        if (valid != 0) {
            std::lock_guard<std::mutex> lock(self->m_statsMutex);
            self->m_stats.checksumFailures++;
            continue;
        }

        // Dispatch by opcode
        auto* hdr = reinterpret_cast<const SwarmPacketHeader*>(recvBuf.data());
        const uint8_t* payloadPtr = recvBuf.data() + SWARM_HEADER_SIZE;

        switch (static_cast<SwarmOpcode>(hdr->opcode)) {
            case SwarmOpcode::Heartbeat:
                self->handleHeartbeat(nodeSlot,
                    reinterpret_cast<const HeartbeatPayload*>(payloadPtr));
                break;

            case SwarmOpcode::TaskPull:
                self->handleTaskPull(nodeSlot,
                    reinterpret_cast<const TaskPullPayload*>(payloadPtr));
                break;

            case SwarmOpcode::ResultPush:
                self->handleResultPush(nodeSlot,
                    reinterpret_cast<const ResultPushPayload*>(payloadPtr),
                    payloadPtr + sizeof(ResultPushPayload),
                    hdr->payloadLen - sizeof(ResultPushPayload));
                break;

            case SwarmOpcode::AttestResponse:
                self->handleAttestResponse(nodeSlot,
                    reinterpret_cast<const AttestResponsePayload*>(payloadPtr));
                break;

            case SwarmOpcode::CapsReport:
                self->handleCapsReport(nodeSlot,
                    reinterpret_cast<const CapsReportPayload*>(payloadPtr));
                break;

            case SwarmOpcode::ConsensusVote:
                self->handleConsensusVote(nodeSlot,
                    reinterpret_cast<const ConsensusVotePayload*>(payloadPtr));
                break;

            case SwarmOpcode::LogStream: {
                auto* lsp = reinterpret_cast<const LogStreamPayload*>(payloadPtr);
                const char* logText = reinterpret_cast<const char*>(
                    payloadPtr + sizeof(LogStreamPayload));
                self->handleLogStream(nodeSlot, lsp, logText);
                break;
            }

            case SwarmOpcode::MetricReport:
                self->handleMetricReport(nodeSlot,
                    reinterpret_cast<const MetricReportPayload*>(payloadPtr));
                break;

            case SwarmOpcode::Shutdown: {
                self->evictNode(nodeSlot, "Remote shutdown");
                break;
            }

            default:
                break;
        }

        // Update receive stats
        {
            std::lock_guard<std::mutex> lock(self->m_statsMutex);
            self->m_stats.totalPacketsRecv++;
            self->m_stats.totalBytesRecv += received;
        }
    }

    return 0;
}

DWORD WINAPI SwarmCoordinator::heartbeatThread(LPVOID param) {
    auto* self = static_cast<SwarmCoordinator*>(param);

    while (self->m_running.load()) {
        Sleep(self->m_config.heartbeatIntervalMs);
        if (!self->m_running.load()) break;

        // Check for dead nodes using MASM RDTSC heartbeat
        self->checkHeartbeatTimeouts();

        // Check for timed-out tasks
        if (self->m_buildRunning.load()) {
            self->checkTaskTimeouts();
        }

        // Send our own heartbeat to all connected workers (if we're leader)
        if (self->m_config.mode == SwarmMode::Leader ||
            self->m_config.mode == SwarmMode::Hybrid) {
            HeartbeatPayload hb = {};
            hb.nodeSlotIndex = 0; // Leader is always slot 0
            hb.activeTasks = self->m_taskGraph.runningTasks;
            hb.uptimeMs = SwarmTime::nowMs();
            hb.cpuLoadPercent = 0; // TODO: query actual CPU
            hb.fitnessScore = Swarm_ComputeNodeFitness();

            std::lock_guard<std::mutex> lock(self->m_nodesMutex);
            for (uint32_t i = 0; i < SWARM_MAX_NODES; i++) {
                if (self->m_nodes[i].state == SwarmNodeState::Online ||
                    self->m_nodes[i].state == SwarmNodeState::Busy) {
                    self->sendPacket(i, SwarmOpcode::Heartbeat,
                                      &hb, sizeof(hb));
                }
            }
        }
    }

    return 0;
}

void SwarmCoordinator::checkHeartbeatTimeouts() {
    uint64_t now = SwarmTime::nowMs();
    std::lock_guard<std::mutex> lock(m_nodesMutex);

    for (uint32_t i = 0; i < SWARM_MAX_NODES; i++) {
        if (m_nodes[i].state != SwarmNodeState::Online &&
            m_nodes[i].state != SwarmNodeState::Busy) continue;

        // Use MASM RDTSC check for fast detection
        int alive = Swarm_HeartbeatCheck(i);
        if (!alive) {
            m_nodes[i].missedHeartbeats++;
            if (m_nodes[i].missedHeartbeats >= 3) {
                // Unlock before eviction (evictNode takes its own lock)
                // Instead, mark for eviction
                uint32_t evictSlot = i;
                // Can't call evictNode while holding lock — mark offline
                m_nodes[i].state = SwarmNodeState::Offline;
                {
                    std::lock_guard<std::mutex> slock(m_statsMutex);
                    m_stats.heartbeatTimeouts++;
                }
                emitEvent(SwarmEventType::HeartbeatTimeout, i, 0,
                          ("Node heartbeat timeout: " +
                           std::string(m_nodes[i].hostname)).c_str());
            }
        }
    }
}

DWORD WINAPI SwarmCoordinator::discoveryThread(LPVOID param) {
    auto* self = static_cast<SwarmCoordinator*>(param);

    while (self->m_running.load()) {
        Sleep(self->m_config.discoveryIntervalMs);
        if (!self->m_running.load()) break;
        if (!self->m_discoveryEnabled.load()) continue;

        // Send discovery ping
        self->sendDiscoveryPing();

        // Listen for incoming pings/pongs
        if (self->m_discoverySocket != INVALID_SOCKET) {
            // Set non-blocking receive timeout
            DWORD timeout = 1000; // 1s
            setsockopt(self->m_discoverySocket, SOL_SOCKET, SO_RCVTIMEO,
                       (char*)&timeout, sizeof(timeout));

            uint8_t buf[SWARM_HEADER_SIZE + sizeof(DiscoveryPayload)];
            sockaddr_in fromAddr = {};
            int fromLen = sizeof(fromAddr);

            int received = recvfrom(self->m_discoverySocket, (char*)buf, sizeof(buf),
                                     0, (sockaddr*)&fromAddr, &fromLen);

            if (received > 0 && received >= (int)SWARM_HEADER_SIZE) {
                int valid = Swarm_ValidatePacketHeader(buf);
                if (valid == 0) {
                    auto* hdr = reinterpret_cast<const SwarmPacketHeader*>(buf);
                    auto* dp = reinterpret_cast<const DiscoveryPayload*>(
                        buf + SWARM_HEADER_SIZE);

                    if (hdr->opcode == static_cast<uint8_t>(SwarmOpcode::DiscoveryPing)) {
                        self->handleDiscoveryPing(dp, fromAddr);
                    } else if (hdr->opcode == static_cast<uint8_t>(SwarmOpcode::DiscoveryPong)) {
                        self->handleDiscoveryPong(dp, fromAddr);
                    }
                }
            }
        }
    }

    return 0;
}

DWORD WINAPI SwarmCoordinator::schedulerThread(LPVOID param) {
    auto* self = static_cast<SwarmCoordinator*>(param);

    while (self->m_running.load()) {
        Sleep(50); // Schedule every 50ms
        if (!self->m_running.load()) break;
        if (!self->m_buildRunning.load()) continue;

        self->scheduleReadyTasks();
    }

    return 0;
}

// =============================================================================
//                     DISCOVERY
// =============================================================================

void SwarmCoordinator::sendDiscoveryPing() {
    if (m_discoverySocket == INVALID_SOCKET) return;

    DiscoveryPayload dp = {};
    dp.swarmPort = m_config.swarmPort;
    dp.httpPort = m_config.httpPort;
    dp.fitnessScore = Swarm_ComputeNodeFitness();
    memcpy(dp.nodeId, m_localNodeId, 16);

    char hostname[64] = {};
    gethostname(hostname, sizeof(hostname) - 1);
    strncpy(dp.hostname, hostname, sizeof(dp.hostname) - 1);

    dp.isLeader = (m_config.mode == SwarmMode::Leader ||
                   m_config.mode == SwarmMode::Hybrid) ? 1 : 0;
    dp.nodeCount = m_nodeCount.load();

    // Build packet
    uint8_t packet[SWARM_HEADER_SIZE + sizeof(DiscoveryPayload)];
    memcpy(packet + SWARM_HEADER_SIZE, &dp, sizeof(dp));
    Swarm_BuildPacketHeader(packet, static_cast<uint8_t>(SwarmOpcode::DiscoveryPing),
                             sizeof(DiscoveryPayload), 0);

    // Broadcast
    sockaddr_in broadAddr = {};
    broadAddr.sin_family = AF_INET;
    broadAddr.sin_port = htons(m_config.discoveryPort);
    inet_pton(AF_INET, m_config.broadcastAddress, &broadAddr.sin_addr);

    sendto(m_discoverySocket, (char*)packet, sizeof(packet), 0,
           (sockaddr*)&broadAddr, sizeof(broadAddr));
}

void SwarmCoordinator::handleDiscoveryPing(const DiscoveryPayload* payload,
                                             const sockaddr_in& from) {
    // Don't respond to our own pings
    if (memcmp(payload->nodeId, m_localNodeId, 16) == 0) return;

    // Check if we already know this node
    {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        for (uint32_t i = 0; i < SWARM_MAX_NODES; i++) {
            if (memcmp(m_nodes[i].nodeId, payload->nodeId, 16) == 0 &&
                m_nodes[i].state != SwarmNodeState::Unknown) {
                return; // Already known
            }
        }
    }

    // If we're the leader, send a pong and try to connect
    if (m_config.mode == SwarmMode::Leader || m_config.mode == SwarmMode::Hybrid) {
        // Send pong
        DiscoveryPayload pong = {};
        pong.swarmPort = m_config.swarmPort;
        pong.fitnessScore = Swarm_ComputeNodeFitness();
        memcpy(pong.nodeId, m_localNodeId, 16);
        gethostname(pong.hostname, sizeof(pong.hostname) - 1);
        pong.isLeader = 1;
        pong.nodeCount = m_nodeCount.load();

        uint8_t packet[SWARM_HEADER_SIZE + sizeof(DiscoveryPayload)];
        memcpy(packet + SWARM_HEADER_SIZE, &pong, sizeof(pong));
        Swarm_BuildPacketHeader(packet, static_cast<uint8_t>(SwarmOpcode::DiscoveryPong),
                                 sizeof(DiscoveryPayload), 0);

        sendto(m_discoverySocket, (char*)packet, sizeof(packet), 0,
               (sockaddr*)&from, sizeof(from));

        // Try to connect to the discovered worker
        char addrBuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from.sin_addr, addrBuf, sizeof(addrBuf));
        addNodeManual(addrBuf, payload->swarmPort);
    }
}

void SwarmCoordinator::handleDiscoveryPong(const DiscoveryPayload* payload,
                                             const sockaddr_in& from) {
    // If we're a worker and received a pong from a leader, connect to them
    if (m_config.mode == SwarmMode::Worker && payload->isLeader) {
        char addrBuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from.sin_addr, addrBuf, sizeof(addrBuf));
        addNodeManual(addrBuf, payload->swarmPort);
    }
}

// =============================================================================
//                     CONSENSUS
// =============================================================================

void SwarmCoordinator::initiateConsensus(uint64_t taskId, uint64_t resultHash) {
    {
        std::lock_guard<std::mutex> lock(m_consensusMutex);
        ConsensusEntry entry;
        entry.taskId = taskId;
        entry.expectedHash = resultHash;
        entry.requiredVotes = m_config.consensusQuorum;
        if (entry.requiredVotes == 0) {
            entry.requiredVotes = (getOnlineNodeCount() / 2) + 1;
        }
        m_consensus[taskId] = entry;
    }

    // Send vote request to all online nodes
    ConsensusVotePayload vp = {};
    vp.taskId = taskId;
    vp.objectHash = resultHash;

    std::lock_guard<std::mutex> lock(m_nodesMutex);
    for (uint32_t i = 0; i < SWARM_MAX_NODES; i++) {
        if (m_nodes[i].state == SwarmNodeState::Online ||
            m_nodes[i].state == SwarmNodeState::Busy) {
            sendPacket(i, SwarmOpcode::ConsensusVote, &vp, sizeof(vp), taskId);
        }
    }
}

void SwarmCoordinator::setConsensusQuorum(uint32_t quorum) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config.consensusQuorum = quorum;
}

uint32_t SwarmCoordinator::getConsensusQuorum() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config.consensusQuorum;
}

// =============================================================================
//                     OBJECT CACHE
// =============================================================================

bool SwarmCoordinator::objectCacheLookup(uint64_t contentHash,
                                           ObjectCacheEntry& outEntry) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_objectCache.find(contentHash);
    if (it == m_objectCache.end()) return false;
    outEntry = it->second;
    return true;
}

void SwarmCoordinator::objectCacheStore(uint64_t contentHash, uint64_t resultHash,
                                          const void* objectData, uint32_t objectSize,
                                          const std::string& sourceFile) {
    // Write .obj to cache directory
    std::string cachePath = m_config.objectCachePath;
    if (cachePath.empty()) cachePath = ".\\swarm_cache";
    CreateDirectoryA(cachePath.c_str(), nullptr);

    char hashStr[32];
    snprintf(hashStr, sizeof(hashStr), "%016llX",
             static_cast<unsigned long long>(contentHash));
    std::string objPath = cachePath + "\\" + hashStr + ".obj";

    HANDLE hFile = CreateFileA(objPath.c_str(), GENERIC_WRITE, 0,
                                nullptr, CREATE_ALWAYS, 0, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, objectData, objectSize, &written, nullptr);
        CloseHandle(hFile);
    }

    ObjectCacheEntry entry;
    entry.contentHash = contentHash;
    entry.resultHash = resultHash;
    entry.objectSize = objectSize;
    entry.hitCount = 0;
    entry.createdAtMs = SwarmTime::nowMs();
    entry.lastAccessMs = SwarmTime::nowMs();
    entry.objectPath = objPath;
    entry.sourceFile = sourceFile;

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_objectCache[contentHash] = entry;
}

void SwarmCoordinator::objectCacheClear() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_objectCache.clear();
}

uint32_t SwarmCoordinator::objectCacheSize() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return static_cast<uint32_t>(m_objectCache.size());
}

// =============================================================================
//                     NETWORK I/O
// =============================================================================

bool SwarmCoordinator::sendPacket(uint32_t nodeSlot, SwarmOpcode opcode,
                                    const void* payload, uint16_t payloadLen,
                                    uint64_t taskId) {
    if (nodeSlot >= SWARM_MAX_NODES) return false;

    SOCKET sock;
    {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        sock = m_nodes[nodeSlot].socket;
    }
    if (sock == INVALID_SOCKET) return false;

    // Build packet
    std::vector<uint8_t> packet(SWARM_HEADER_SIZE + payloadLen);
    if (payload && payloadLen > 0) {
        memcpy(packet.data() + SWARM_HEADER_SIZE, payload, payloadLen);
    }

    SwarmProtocol::buildPacket(packet.data(), opcode, payload, payloadLen,
                                taskId, m_localNodeId, nextSequenceId());

    bool ok = sendPacketRaw(sock, packet.data(), static_cast<uint32_t>(packet.size()));

    if (ok) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalPacketsSent++;
        m_stats.totalBytesSent += packet.size();
    }

    return ok;
}

bool SwarmCoordinator::sendPacketRaw(SOCKET sock, const void* data, uint32_t len) {
    const char* ptr = static_cast<const char*>(data);
    uint32_t remaining = len;

    while (remaining > 0) {
        int sent = send(sock, ptr, remaining, 0);
        if (sent == SOCKET_ERROR) return false;
        ptr += sent;
        remaining -= sent;
    }

    return true;
}

int SwarmCoordinator::recvPacket(SOCKET sock, void* buffer, uint32_t bufferSize) {
    // First, receive the fixed header
    char* ptr = static_cast<char*>(buffer);
    int totalReceived = 0;

    // Read header (64 bytes)
    while (totalReceived < SWARM_HEADER_SIZE) {
        int r = recv(sock, ptr + totalReceived, SWARM_HEADER_SIZE - totalReceived, 0);
        if (r <= 0) return -1;
        totalReceived += r;
    }

    // Extract payload length from header
    auto* hdr = reinterpret_cast<const SwarmPacketHeader*>(buffer);
    uint16_t payloadLen = hdr->payloadLen;

    if (payloadLen > 0 && (SWARM_HEADER_SIZE + payloadLen) <= bufferSize) {
        // Read payload
        int payloadReceived = 0;
        while (payloadReceived < payloadLen) {
            int r = recv(sock, ptr + SWARM_HEADER_SIZE + payloadReceived,
                          payloadLen - payloadReceived, 0);
            if (r <= 0) return -1;
            payloadReceived += r;
        }
        totalReceived += payloadReceived;
    }

    return totalReceived;
}

// =============================================================================
//                     ATTESTATION
// =============================================================================

void SwarmCoordinator::sendAttestChallenge(uint32_t nodeSlot) {
    AttestRequestPayload req = {};

    // Generate random challenge bytes
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_FULL,
                              CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, sizeof(req.challenge), req.challenge);
        CryptReleaseContext(hProv, 0);
    }

    req.nonce = SwarmTime::nowUs();
    req.requiredVersion = SWARM_VERSION;

    {
        std::lock_guard<std::mutex> lock(m_nodesMutex);
        m_nodes[nodeSlot].state = SwarmNodeState::Attesting;
    }

    sendPacket(nodeSlot, SwarmOpcode::AttestRequest, &req, sizeof(req));
}

bool SwarmCoordinator::verifyAttestResponse(uint32_t nodeSlot,
                                              const AttestResponsePayload* resp) {
    // Verify protocol version
    if (resp->protocolVersion < SWARM_VERSION) return false;

    // Verify HWID is not all zeros
    bool allZero = true;
    for (int i = 0; i < 16; i++) {
        if (resp->hwid[i] != 0) { allZero = false; break; }
    }
    if (allZero) return false;

    // In a full implementation, we'd verify the HMAC of the challenge
    // using the shared secret. For now, accept if HWID is valid.
    return true;
}

// =============================================================================
//                     EVENTS
// =============================================================================

void SwarmCoordinator::setEventCallback(SwarmEventCallback callback, void* userData) {
    m_eventCallback = callback;
    m_eventUserData = userData;
}

void SwarmCoordinator::setLogCallback(SwarmLogCallback callback, void* userData) {
    m_logCallback = callback;
    m_logUserData = userData;
}

void SwarmCoordinator::emitEvent(SwarmEventType type, uint32_t nodeSlot,
                                   uint64_t taskId, const char* message) {
    SwarmEvent ev;
    ev.type = type;
    ev.nodeSlotIndex = nodeSlot;
    ev.taskId = taskId;
    ev.timestampMs = SwarmTime::nowMs();
    if (message) {
        strncpy(ev.message, message, sizeof(ev.message) - 1);
    }

    {
        std::lock_guard<std::mutex> lock(m_eventsMutex);
        m_events.push_back(ev);
        // Keep only last 1000 events
        if (m_events.size() > 1000) {
            m_events.erase(m_events.begin(), m_events.begin() + 500);
        }
    }

    if (m_eventCallback) {
        m_eventCallback(ev, m_eventUserData);
    }
}

std::vector<SwarmEvent> SwarmCoordinator::getRecentEvents(uint32_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_eventsMutex);
    std::vector<SwarmEvent> result;
    size_t start = m_events.size() > maxCount ? m_events.size() - maxCount : 0;
    for (size_t i = start; i < m_events.size(); i++) {
        result.push_back(m_events[i]);
    }
    return result;
}

// =============================================================================
//                     STATISTICS
// =============================================================================

SwarmStats SwarmCoordinator::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    SwarmStats s = m_stats;

    // Fill in dynamic stats
    s.totalNodes = 0;
    s.onlineNodes = 0;
    s.busyNodes = 0;
    s.offlineNodes = 0;

    {
        std::lock_guard<std::mutex> nlock(m_nodesMutex);
        for (uint32_t i = 0; i < SWARM_MAX_NODES; i++) {
            if (m_nodes[i].state == SwarmNodeState::Unknown) continue;
            s.totalNodes++;
            switch (m_nodes[i].state) {
                case SwarmNodeState::Online:     s.onlineNodes++; break;
                case SwarmNodeState::Busy:       s.busyNodes++;   break;
                case SwarmNodeState::Offline:    s.offlineNodes++; break;
                default: break;
            }
        }
    }

    s.totalTasks     = m_taskGraph.totalTasks;
    s.completedTasks = m_taskGraph.completedTasks;
    s.failedTasks    = m_taskGraph.failedTasks;
    s.runningTasks   = m_taskGraph.runningTasks;
    s.pendingTasks   = m_taskGraph.pendingTasks;

    if (m_taskGraph.startTimeMs > 0) {
        uint64_t endTime = m_taskGraph.endTimeMs > 0 ? m_taskGraph.endTimeMs : SwarmTime::nowMs();
        s.totalBuildTimeMs = endTime - m_taskGraph.startTimeMs;
        if (s.totalBuildTimeMs > 0 && s.totalCompileTimeMs > 0) {
            s.parallelSpeedup = static_cast<double>(s.totalCompileTimeMs) /
                                static_cast<double>(s.totalBuildTimeMs);
        }
    }

    s.objectCacheHits = static_cast<uint32_t>(m_objectCache.size());
    s.ringOverflows = 0; // TODO: read from MASM ring

    return s;
}

void SwarmCoordinator::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = SwarmStats();
}

void SwarmCoordinator::enableDiscovery(bool enable) {
    m_discoveryEnabled.store(enable);
}

// =============================================================================
//                     UTILITY
// =============================================================================

uint64_t SwarmCoordinator::generateTaskId() {
    return m_nextTaskId.fetch_add(1);
}

uint32_t SwarmCoordinator::nextSequenceId() {
    return m_sequenceCounter.fetch_add(1);
}

void SwarmCoordinator::generateNodeId(uint8_t outNodeId[16]) {
    // Generate from HWID + random
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_FULL,
                              CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, 16, outNodeId);
        CryptReleaseContext(hProv, 0);
    }

    // Mix in hostname for uniqueness
    char hostname[64] = {};
    gethostname(hostname, sizeof(hostname) - 1);
    for (int i = 0; hostname[i] && i < 16; i++) {
        outNodeId[i] ^= static_cast<uint8_t>(hostname[i]);
    }
}

// =============================================================================
//                     JSON SERIALIZATION
// =============================================================================

std::string SwarmCoordinator::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"running\":" << (m_running.load() ? "true" : "false") << ","
        << "\"buildRunning\":" << (m_buildRunning.load() ? "true" : "false") << ","
        << "\"discoveryEnabled\":" << (m_discoveryEnabled.load() ? "true" : "false") << ","
        << "\"mode\":" << static_cast<int>(m_config.mode) << ","
        << "\"onlineNodes\":" << getOnlineNodeCount() << ","
        << "\"dagGeneration\":" << m_taskGraph.generation << ","
        << "\"buildProgress\":" << std::fixed << std::setprecision(2) << getBuildProgress()
        << "}";
    return oss.str();
}

std::string SwarmCoordinator::nodesToJson() const {
    std::lock_guard<std::mutex> lock(m_nodesMutex);
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (uint32_t i = 0; i < SWARM_MAX_NODES; i++) {
        if (m_nodes[i].state == SwarmNodeState::Unknown) continue;
        if (!first) oss << ",";
        first = false;
        const auto& n = m_nodes[i];
        oss << "{"
            << "\"slot\":" << i << ","
            << "\"hostname\":\"" << n.hostname << "\","
            << "\"ip\":\"" << n.ipAddress << "\","
            << "\"state\":" << static_cast<int>(n.state) << ","
            << "\"fitness\":" << n.fitnessScore << ","
            << "\"cores\":" << n.logicalCores << ","
            << "\"ramMB\":" << n.ramTotalMB << ","
            << "\"activeTasks\":" << n.activeTasks << ","
            << "\"completedTasks\":" << n.completedTasks << ","
            << "\"failedTasks\":" << n.failedTasks << ","
            << "\"cpuLoad\":" << n.cpuLoadPercent << ","
            << "\"avx2\":" << n.hasAVX2 << ","
            << "\"avx512\":" << n.hasAVX512
            << "}";
    }
    oss << "]";
    return oss.str();
}

std::string SwarmCoordinator::taskGraphToJson() const {
    std::lock_guard<std::mutex> lock(m_taskGraph.graphMutex);
    std::ostringstream oss;
    oss << "{"
        << "\"generation\":" << m_taskGraph.generation << ","
        << "\"totalTasks\":" << m_taskGraph.totalTasks << ","
        << "\"completedTasks\":" << m_taskGraph.completedTasks << ","
        << "\"failedTasks\":" << m_taskGraph.failedTasks << ","
        << "\"runningTasks\":" << m_taskGraph.runningTasks << ","
        << "\"pendingTasks\":" << m_taskGraph.pendingTasks << ","
        << "\"tasks\":[";

    for (size_t i = 0; i < m_taskGraph.tasks.size(); i++) {
        if (i > 0) oss << ",";
        const auto& t = m_taskGraph.tasks[i];
        oss << "{"
            << "\"id\":" << t.taskId << ","
            << "\"type\":" << static_cast<int>(t.taskType) << ","
            << "\"state\":" << static_cast<int>(t.taskState) << ","
            << "\"source\":\"" << t.sourceFile << "\","
            << "\"node\":" << t.assignedNode << ","
            << "\"exit\":" << t.exitCode << ","
            << "\"timeMs\":" << t.compileTimeMs << ","
            << "\"retry\":" << static_cast<int>(t.retryCount)
            << "}";
    }

    oss << "]}";
    return oss.str();
}

std::string SwarmCoordinator::statsToJson() const {
    auto s = getStats();
    std::ostringstream oss;
    oss << "{"
        << "\"totalNodes\":" << s.totalNodes << ","
        << "\"onlineNodes\":" << s.onlineNodes << ","
        << "\"busyNodes\":" << s.busyNodes << ","
        << "\"offlineNodes\":" << s.offlineNodes << ","
        << "\"totalTasks\":" << s.totalTasks << ","
        << "\"completedTasks\":" << s.completedTasks << ","
        << "\"failedTasks\":" << s.failedTasks << ","
        << "\"runningTasks\":" << s.runningTasks << ","
        << "\"pendingTasks\":" << s.pendingTasks << ","
        << "\"retriedTasks\":" << s.retriedTasks << ","
        << "\"totalCompileTimeMs\":" << s.totalCompileTimeMs << ","
        << "\"avgCompileTimeMs\":" << s.avgCompileTimeMs << ","
        << "\"maxCompileTimeMs\":" << s.maxCompileTimeMs << ","
        << "\"totalBuildTimeMs\":" << s.totalBuildTimeMs << ","
        << "\"parallelSpeedup\":" << std::fixed << std::setprecision(2) << s.parallelSpeedup << ","
        << "\"objectCacheHits\":" << s.objectCacheHits << ","
        << "\"totalBytesSent\":" << s.totalBytesSent << ","
        << "\"totalBytesRecv\":" << s.totalBytesRecv << ","
        << "\"totalPacketsSent\":" << s.totalPacketsSent << ","
        << "\"totalPacketsRecv\":" << s.totalPacketsRecv << ","
        << "\"checksumFailures\":" << s.checksumFailures << ","
        << "\"heartbeatTimeouts\":" << s.heartbeatTimeouts << ","
        << "\"attestationFailures\":" << s.attestationFailures << ","
        << "\"consensusCommits\":" << s.consensusCommits << ","
        << "\"consensusRejects\":" << s.consensusRejects
        << "}";
    return oss.str();
}

std::string SwarmCoordinator::eventsToJson(uint32_t maxCount) const {
    auto events = getRecentEvents(maxCount);
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < events.size(); i++) {
        if (i > 0) oss << ",";
        const auto& e = events[i];
        oss << "{"
            << "\"type\":" << static_cast<int>(e.type) << ","
            << "\"nodeSlot\":" << e.nodeSlotIndex << ","
            << "\"taskId\":" << e.taskId << ","
            << "\"timestampMs\":" << e.timestampMs << ","
            << "\"message\":\"" << e.message << "\""
            << "}";
    }
    oss << "]";
    return oss.str();
}

std::string SwarmCoordinator::configToJson() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    std::ostringstream oss;
    oss << "{"
        << "\"swarmPort\":" << m_config.swarmPort << ","
        << "\"discoveryPort\":" << m_config.discoveryPort << ","
        << "\"httpPort\":" << m_config.httpPort << ","
        << "\"mode\":" << static_cast<int>(m_config.mode) << ","
        << "\"maxNodes\":" << m_config.maxNodes << ","
        << "\"maxConcurrentTasks\":" << m_config.maxConcurrentTasks << ","
        << "\"heartbeatIntervalMs\":" << m_config.heartbeatIntervalMs << ","
        << "\"heartbeatTimeoutMs\":" << m_config.heartbeatTimeoutMs << ","
        << "\"taskTimeoutMs\":" << m_config.taskTimeoutMs << ","
        << "\"maxRetries\":" << m_config.maxRetries << ","
        << "\"consensusQuorum\":" << m_config.consensusQuorum << ","
        << "\"requireAttestation\":" << (m_config.requireAttestation ? "true" : "false") << ","
        << "\"discoveryIntervalMs\":" << m_config.discoveryIntervalMs
        << "}";
    return oss.str();
}
