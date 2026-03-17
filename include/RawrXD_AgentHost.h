#ifndef RAWRXD_AGENT_HOST_H
#define RAWRXD_AGENT_HOST_H

#include <string>
#include <stdint.h>

namespace RawrXD {

struct AgentTask {
    std::string id;
    std::string payload;
    int priority;
};

// Interface for the Multi-Agent Coordinator
class IAgentHost {
public:
    virtual ~IAgentHost() = default;
    virtual void EnqueueTask(const std::string& task) = 0;
    virtual void Run() = 0;
};

} // namespace RawrXD

#endif // RAWRXD_AGENT_HOST_H
