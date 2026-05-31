#include "swarm_orchestrator.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace RawrXD::Swarm {

static std::mutex swarmMutex;
static std::vector<NodeInfo> activeNodes;
static std::string currentSwarmId;
static std::unordered_map<std::string, std::vector<TaskResult>> pendingResults;
static bool initialized = false;

static bool initWinsock() {
    if (initialized) return true;
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "[Swarm] WSAStartup failed: " << result << std::endl;
        return false;
    }
    initialized = true;
    return true;
}

bool SwarmOrchestrator::discoverNodes(const std::string& multicastAddr, uint16_t port, std::vector<NodeInfo>& nodes) {
    std::lock_guard<std::mutex> lock(swarmMutex);
    if (!initWinsock()) return false;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "[Swarm] Socket creation failed" << std::endl;
        return false;
    }

    sockaddr_in localAddr = {};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(port);
    localAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    ip_mreq mreq = {};
    mreq.imr_multiaddr.s_addr = inet_addr(multicastAddr.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    // Send discovery beacon
    sockaddr_in multicastAddrStruct = {};
    multicastAddrStruct.sin_family = AF_INET;
    multicastAddrStruct.sin_port = htons(port);
    multicastAddrStruct.sin_addr.s_addr = inet_addr(multicastAddr.c_str());

    const char* beacon = "RAWRXD_SWARM_DISCOVER";
    sendto(sock, beacon, strlen(beacon), 0, (sockaddr*)&multicastAddrStruct, sizeof(multicastAddrStruct));

    // Listen for responses with timeout
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    timeval timeout = {2, 0}; // 2 seconds

    if (select(0, &readfds, nullptr, nullptr, &timeout) > 0) {
        char buffer[1024];
        sockaddr_in senderAddr;
        int senderAddrSize = sizeof(senderAddr);
        int bytesReceived = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&senderAddr, &senderAddrSize);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            if (strstr(buffer, "RAWRXD_SWARM_RESPONSE")) {
                NodeInfo node;
                node.address = inet_ntoa(senderAddr.sin_addr);
                node.port = ntohs(senderAddr.sin_port);
                node.id = std::string(buffer).substr(strlen("RAWRXD_SWARM_RESPONSE "));
                nodes.push_back(node);
            }
        }
    }

    closesocket(sock);
    return !nodes.empty();
}

bool SwarmOrchestrator::joinSwarm(const std::string& swarmId, const NodeInfo& self) {
    std::lock_guard<std::mutex> lock(swarmMutex);
    if (!initWinsock()) return false;

    // Implement full join protocol with authentication
    // Step 1: Generate authentication token (HMAC-SHA256 of swarmId + nodeId + timestamp)
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::string authData = swarmId + ":" + self.id + ":" + std::to_string(timestamp);
    
    // Simple hash-based token (production would use HMAC-SHA256)
    uint64_t tokenHash = 5381;
    for (char c : authData) {
        tokenHash = ((tokenHash << 5) + tokenHash) + c;
    }
    std::string authToken = std::to_string(tokenHash);

    // Step 2: Connect to a known bootstrap node (if any active nodes exist)
    if (!activeNodes.empty()) {
        const NodeInfo& bootstrapNode = activeNodes[0];
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return false;

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(bootstrapNode.port);
        inet_pton(AF_INET, bootstrapNode.address.c_str(), &addr.sin_addr);

        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(sock);
            return false;
        }

        // Send JOIN request with authentication
        std::string joinMsg = "JOIN|" + swarmId + "|" + self.id + "|" + self.address + "|" + 
                             std::to_string(self.port) + "|" + authToken;
        if (send(sock, joinMsg.c_str(), (int)joinMsg.size(), 0) == SOCKET_ERROR) {
            closesocket(sock);
            return false;
        }

        // Receive JOIN ACK
        char buffer[256];
        int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
        closesocket(sock);

        if (bytesReceived <= 0) return false;
        buffer[bytesReceived] = '\0';
        if (strstr(buffer, "ACK") == nullptr) return false;
    }

    // Step 3: Add self to active nodes
    currentSwarmId = swarmId;
    activeNodes.push_back(self);
    return true;
}

bool SwarmOrchestrator::leaveSwarm() {
    std::lock_guard<std::mutex> lock(swarmMutex);
    
    // Stop heartbeat monitoring thread
    heartbeatRunning = false;
    if (heartbeatThread != nullptr) {
        if (heartbeatThread->joinable()) {
            heartbeatThread->join();
        }
        delete heartbeatThread;
        heartbeatThread = nullptr;
    }
    
    currentSwarmId.clear();
    activeNodes.clear();
    pendingResults.clear();
    return true;
}

bool SwarmOrchestrator::distributeTask(const Task& task, std::vector<NodeInfo>& targets) {
    std::lock_guard<std::mutex> lock(swarmMutex);
    if (activeNodes.empty()) return false;

    // Simple round-robin distribution with network transmission
    static size_t nextNode = 0;
    targets.clear();

    // Implement actual task distribution over network
    for (size_t i = 0; i < activeNodes.size(); ++i) {
        const auto& node = activeNodes[(nextNode + i) % activeNodes.size()];
        targets.push_back(node);

        // Create network socket and send task to node
        std::thread([task, node]() {
            SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sock == INVALID_SOCKET) return;

            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(node.port);
            inet_pton(AF_INET, node.address.c_str(), &addr.sin_addr);

            if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
                closesocket(sock);
                return;
            }

            // Build and send TASK message: TASK|taskId|type|dataLength|data
            std::string taskHeader = "TASK|" + task.id + "|" + task.type + "|" + 
                                    std::to_string(task.data.size()) + "|";
            if (send(sock, taskHeader.c_str(), (int)taskHeader.size(), 0) == SOCKET_ERROR) {
                closesocket(sock);
                return;
            }

            // Send task data payload
            if (!task.data.empty()) {
                if (send(sock, (const char*)task.data.data(), (int)task.data.size(), 0) == SOCKET_ERROR) {
                    closesocket(sock);
                    return;
                }
            }

            // Wait for RESULT acknowledgment
            char buffer[512];
            int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);

            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                std::lock_guard<std::mutex> lock(swarmMutex);
                
                // Parse result: RESULT|taskId|success|dataLength|data
                if (strstr(buffer, "RESULT") != nullptr) {
                    TaskResult result;
                    result.taskId = task.id;
                    result.success = true;
                    
                    // Extract result data if present
                    std::string resultStr(buffer);
                    size_t dataStart = resultStr.find_last_of('|') + 1;
                    if (dataStart < resultStr.size()) {
                        std::string dataHex = resultStr.substr(dataStart);
                        result.data.assign(dataHex.begin(), dataHex.end());
                    }
                    
                    pendingResults[task.id].push_back(result);
                }
            }

            closesocket(sock);
        }).detach();
    }

    nextNode = (nextNode + activeNodes.size()) % activeNodes.size();
    return true;
}

bool SwarmOrchestrator::collectResults(const std::string& taskId, std::vector<TaskResult>& results) {
    std::lock_guard<std::mutex> lock(swarmMutex);
    auto it = pendingResults.find(taskId);
    if (it != pendingResults.end()) {
        results = std::move(it->second);
        pendingResults.erase(it);
        return true;
    }
    return false;
}

static bool heartbeatRunning = false;
static std::thread* heartbeatThread = nullptr;

static void heartbeatThreadProc() {
    while (heartbeatRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Heartbeat every 5 seconds
        
        std::vector<NodeInfo> nodesToCheck;
        {
            std::lock_guard<std::mutex> lock(swarmMutex);
            nodesToCheck = activeNodes;
        }

        // Send heartbeat to all nodes
        for (const auto& node : nodesToCheck) {
            std::thread([node]() {
                SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                if (sock == INVALID_SOCKET) return;

                sockaddr_in addr = {};
                addr.sin_family = AF_INET;
                addr.sin_port = htons(node.port);
                inet_pton(AF_INET, node.address.c_str(), &addr.sin_addr);

                // Send HEARTBEAT packet
                std::string hbMsg = "HEARTBEAT|" + std::to_string(
                    std::chrono::system_clock::now().time_since_epoch().count());
                sendto(sock, hbMsg.c_str(), (int)hbMsg.size(), 0, (sockaddr*)&addr, sizeof(addr));

                closesocket(sock);
            }).detach();
        }
    }
}

bool SwarmOrchestrator::heartbeat() {
    std::lock_guard<std::mutex> lock(swarmMutex);
    if (activeNodes.empty()) return false;

    // Start heartbeat thread if not already running
    if (!heartbeatRunning) {
        heartbeatRunning = true;
        if (heartbeatThread == nullptr) {
            heartbeatThread = new std::thread(heartbeatThreadProc);
        }
    }

    return true;
}

bool SwarmOrchestrator::syncModel(const std::string& modelPath, const std::vector<NodeInfo>& nodes) {
    std::lock_guard<std::mutex> lock(swarmMutex);
    if (nodes.empty()) return false;

    // Implement model synchronization protocol
    // Step 1: Calculate model file hash (for integrity verification)
    std::ifstream modelFile(modelPath, std::ios::binary);
    if (!modelFile.is_open()) return false;

    // Calculate simple checksum
    uint64_t checksum = 0;
    char buffer[4096];
    while (modelFile.read(buffer, sizeof(buffer))) {
        for (int i = 0; i < modelFile.gcount(); ++i) {
            checksum = ((checksum << 5) + checksum) + (uint8_t)buffer[i];
        }
    }
    modelFile.close();

    // Get file size
    std::ifstream file(modelPath, std::ios::binary | std::ios::ate);
    uint64_t fileSize = file.tellg();
    file.close();

    // Step 2: Broadcast model sync announcement to all nodes
    std::string announcement = "SYNC_ANNOUNCE|" + modelPath + "|" + 
                              std::to_string(fileSize) + "|" + std::to_string(checksum);

    for (const auto& node : nodes) {
        std::thread([node, announcement, modelPath, fileSize, checksum]() {
            SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (sock == INVALID_SOCKET) return;

            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(node.port);
            inet_pton(AF_INET, node.address.c_str(), &addr.sin_addr);

            // Send announcement
            sendto(sock, announcement.c_str(), (int)announcement.size(), 0, 
                   (sockaddr*)&addr, sizeof(addr));

            // Step 3: Wait for ACK, then stream model file in chunks
            char ackBuffer[256];
            sockaddr_in remoteAddr;
            int remoteAddrSize = sizeof(remoteAddr);
            
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);
            timeval timeout = {5, 0}; // 5 second timeout

            if (select(0, &readfds, nullptr, nullptr, &timeout) > 0) {
                int bytesReceived = recvfrom(sock, ackBuffer, sizeof(ackBuffer) - 1, 0,
                                            (sockaddr*)&remoteAddr, &remoteAddrSize);
                if (bytesReceived > 0 && strstr(ackBuffer, "SYNC_ACK")) {
                    // Stream model file
                    std::ifstream srcFile(modelPath, std::ios::binary);
                    if (srcFile.is_open()) {
                        const size_t CHUNK_SIZE = 65536; // 64KB chunks
                        std::vector<char> chunk(CHUNK_SIZE);
                        size_t bytesRead = 0;
                        
                        while (srcFile.read(chunk.data(), CHUNK_SIZE) || srcFile.gcount() > 0) {
                            size_t toSend = srcFile.gcount();
                            std::string chunkHeader = "SYNC_DATA|" + std::to_string(toSend) + "|";
                            
                            sendto(sock, chunkHeader.c_str(), (int)chunkHeader.size(), 0,
                                   (sockaddr*)&addr, sizeof(addr));
                            sendto(sock, chunk.data(), (int)toSend, 0,
                                   (sockaddr*)&addr, sizeof(addr));
                            
                            bytesRead += toSend;
                        }
                        
                        // Send completion marker
                        std::string completion = "SYNC_COMPLETE|" + std::to_string(checksum);
                        sendto(sock, completion.c_str(), (int)completion.size(), 0,
                               (sockaddr*)&addr, sizeof(addr));
                        
                        srcFile.close();
                    }
                }
            }

            closesocket(sock);
        }).detach();
    }

    return true;
}

} // namespace RawrXD::Swarm
