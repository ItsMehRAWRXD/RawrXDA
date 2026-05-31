#include "agentic/ToolRegistry.h"

namespace RawrXD { namespace Agent {

AgentToolRegistry::AgentToolRegistry() {}

AgentToolRegistry& AgentToolRegistry::Instance() {
    static AgentToolRegistry instance;
    return instance;
}

ToolExecResult AgentToolRegistry::Dispatch(const std::string&, const json&) {
    return ToolExecResult::error("stub");
}

}} // namespace RawrXD::Agent
