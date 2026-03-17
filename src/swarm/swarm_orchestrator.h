#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD::Swarm {

struct NodeInfo {
    std::string id;
    std::string address;
    uint16_t port;
    uint64_t capabilities; // bitmask
};

struct Task {
    std::string id;
    std::string type; // e.g., "inference", "quantize"
    std::vector<uint8_t> data;
};

struct TaskResult {
    std::string taskId;
    bool success;
    std::vector<uint8_t> data;
};

class SwarmOrchestrator {
public:
    static bool discoverNodes(const std::string& multicastAddr, uint16_t port, std::vector<NodeInfo>& nodes);
    static bool joinSwarm(const std::string& swarmId, const NodeInfo& self);
    static bool leaveSwarm();
    static bool distributeTask(const Task& task, std::vector<NodeInfo>& targets);
    static bool collectResults(const std::string& taskId, std::vector<TaskResult>& results);
    static bool heartbeat();
    static bool syncModel(const std::string& modelPath, const std::vector<NodeInfo>& nodes);
};

} // namespace RawrXD::Swarm