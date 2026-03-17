#include "swarm_orchestrator.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
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

    currentSwarmId = swarmId;
    activeNodes.push_back(self);

    // TODO: Implement full join protocol with authentication
    // For now, just add self to active nodes
    return true;
}

bool SwarmOrchestrator::leaveSwarm() {
    std::lock_guard<std::mutex> lock(swarmMutex);
    currentSwarmId.clear();
    activeNodes.clear();
    pendingResults.clear();
    return true;
}

bool SwarmOrchestrator::distributeTask(const Task& task, std::vector<NodeInfo>& targets) {
    std::lock_guard<std::mutex> lock(swarmMutex);
    if (activeNodes.empty()) return false;

    // Simple round-robin distribution
    static size_t nextNode = 0;
    for (const auto& node : activeNodes) {
        if (nextNode >= targets.size()) nextNode = 0;
        targets.push_back(node);
        nextNode++;
    }

    // TODO: Implement actual task distribution over network
    // For now, simulate by queuing results
    std::thread([task]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate processing
        TaskResult result;
        result.taskId = task.id;
        result.success = true;
        result.data = task.data; // Echo back
        std::lock_guard<std::mutex> lock(swarmMutex);
        pendingResults[task.id].push_back(result);
    }).detach();

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

bool SwarmOrchestrator::heartbeat() {
    std::lock_guard<std::mutex> lock(swarmMutex);
    if (activeNodes.empty()) return false;

    // TODO: Send heartbeat to all nodes
    // For now, just check if we have nodes
    return true;
}

bool SwarmOrchestrator::syncModel(const std::string& modelPath, const std::vector<NodeInfo>& nodes) {
    std::lock_guard<std::mutex> lock(swarmMutex);
    if (nodes.empty()) return false;

    // TODO: Implement model synchronization protocol
    // For now, assume success
    return true;
}

} // namespace RawrXD::Swarm