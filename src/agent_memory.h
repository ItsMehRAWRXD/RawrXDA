// ============================================================================
// agent_memory.h — Persistent Agent Memory System Header
// Provides cross-session context and memory for agentic workflows
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Agent {

struct MemoryEntry {
    std::string session_id;
    std::string key;
    nlohmann::json value;
    std::vector<std::string> tags;
    double importance;
    std::string timestamp;
};

class AgentMemory {
public:
    static AgentMemory& instance();

    // Store memory entry
    void store(const std::string& session_id, const std::string& key,
               const nlohmann::json& value,
               const std::vector<std::string>& tags = {},
               double importance = 1.0);

    // Retrieve memory entry
    nlohmann::json retrieve(const std::string& session_id, const std::string& key);

    // Search memory entries
    std::vector<MemoryEntry> search(const std::string& query, size_t limit = 10);

    // Session management
    void start_session(const std::string& session_id, const std::string& agent_type,
                      const nlohmann::json& context = nullptr);
    void end_session(const std::string& session_id);
    void clear_session(const std::string& session_id);

private:
    AgentMemory();
    ~AgentMemory();

    class Impl;
    std::unique_ptr<Impl> pimpl;
};

}} // namespace RawrXD::Agent