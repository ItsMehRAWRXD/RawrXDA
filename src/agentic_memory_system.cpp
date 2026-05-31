#include "agentic_memory_system.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>

namespace {
int64_t toUnixMillis(const std::chrono::system_clock::time_point& tp)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point fromUnixMillis(const nlohmann::json& value)
{
    if (!value.is_number_integer()) {
        return std::chrono::system_clock::now();
    }

    return std::chrono::system_clock::time_point(std::chrono::milliseconds(value.get<int64_t>()));
}

const char* memoryTypeToString(MemoryType type)
{
    switch (type) {
        case MemoryType::Episode: return "Episode";
        case MemoryType::Fact: return "Fact";
        case MemoryType::Procedure: return "Procedure";
        case MemoryType::Concept: return "Concept";
        case MemoryType::CodeSnippet: return "CodeSnippet";
        case MemoryType::UserPreference: return "UserPreference";
        case MemoryType::SystemConstraint: return "SystemConstraint";
        default: return "Episode";
    }
}

MemoryType memoryTypeFromString(const std::string& type)
{
    if (type == "Fact") return MemoryType::Fact;
    if (type == "Procedure") return MemoryType::Procedure;
    if (type == "Concept") return MemoryType::Concept;
    if (type == "CodeSnippet") return MemoryType::CodeSnippet;
    if (type == "UserPreference") return MemoryType::UserPreference;
    if (type == "SystemConstraint") return MemoryType::SystemConstraint;
    return MemoryType::Episode;
}
}

// Windows UUID support
#ifdef _WIN32
#include <rpc.h>
#pragma comment(lib, "Rpcrt4.lib")
#endif

AgenticMemorySystem::AgenticMemorySystem()
    : m_systemStartTime(std::chrono::system_clock::now())
{
}

AgenticMemorySystem::~AgenticMemorySystem()
{
    // Cleanup: clear all memory stores
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memories.clear();
}

std::string AgenticMemorySystem::generateUUID() {
#ifdef _WIN32
    UUID uuid;
    UuidCreate(&uuid);
    unsigned char* str;
    UuidToStringA(&uuid, &str);
    std::string s((char*)str);
    RpcStringFreeA(&str);
    return s;
#else
    // Fallback if not on Windows (though this workspace is Windows)
    return "uuid-fallback-" + std::to_string(std::rand());
#endif
}

std::string AgenticMemorySystem::storeMemory(
    MemoryType type,
    const std::string& content,
    const std::string& metadata)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string memoryId = generateUUID();

    auto memory = std::make_unique<MemoryEntry>();
    memory->id = memoryId;
    memory->type = type;
    memory->content = content;
    memory->metadata = metadata;
    memory->timestamp = std::chrono::system_clock::now();
    memory->relevanceScore = 1.0f;
    memory->accessCount = 0;
    memory->isPinned = false;

    m_memories[memoryId] = std::move(memory);
    m_totalStored++;

    return memoryId;
}

void AgenticMemorySystem::updateMemory(const std::string& memoryId, const std::string& content)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_memories.find(memoryId);
    if (it != m_memories.end()) {
        it->second->content = content;
        it->second->timestamp = std::chrono::system_clock::now(); // Update timestamp on edit
    }
}

void AgenticMemorySystem::deleteMemory(const std::string& memoryId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memories.erase(memoryId);
}

void AgenticMemorySystem::pinMemory(const std::string& memoryId, bool pinned)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_memories.find(memoryId);
    if (it != m_memories.end()) {
        it->second->isPinned = pinned;
    }
}

AgenticMemorySystem::MemoryEntry* AgenticMemorySystem::getMemory(const std::string& memoryId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_memories.find(memoryId);
    if (it != m_memories.end()) {
        it->second->accessCount++;
        m_totalRetrieved++;
        return it->second.get();
    }
    return nullptr;
}

std::vector<AgenticMemorySystem::MemoryEntry*> AgenticMemorySystem::getMemoriesByType(
    MemoryType type)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MemoryEntry*> results;

    for (auto& pair : m_memories) {
        if (pair.second->type == type) {
            results.push_back(pair.second.get());
        }
    }

    return results;
}

std::vector<AgenticMemorySystem::MemoryEntry*> AgenticMemorySystem::getMemoriesByContentSearch(const std::string& query)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MemoryEntry*> results;
    
    // Simple substring search for now (could be upgraded to semantic in future)
    for (auto& pair : m_memories) {
        if (pair.second->content.find(query) != std::string::npos) {
             results.push_back(pair.second.get());
        }
    }
    return results;
}

size_t AgenticMemorySystem::getMemoryCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_memories.size();
}

void AgenticMemorySystem::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memories.clear();
    m_totalStored = 0;
    m_totalRetrieved = 0;
}

nlohmann::json AgenticMemorySystem::exportState() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    nlohmann::json state = nlohmann::json::object();
    state["schemaVersion"] = 1;
    state["systemStartTimeMs"] = toUnixMillis(m_systemStartTime);
    state["totalStored"] = static_cast<uint64_t>(m_totalStored.load());
    state["totalRetrieved"] = static_cast<uint64_t>(m_totalRetrieved.load());
    state["memories"] = nlohmann::json::array();

    for (const auto& pair : m_memories) {
        const auto& memory = pair.second;
        nlohmann::json entry;
        entry["id"] = memory->id;
        entry["type"] = memoryTypeToString(memory->type);
        entry["content"] = memory->content;
        entry["metadata"] = memory->metadata;
        entry["timestampMs"] = toUnixMillis(memory->timestamp);
        entry["relevanceScore"] = memory->relevanceScore;
        entry["accessCount"] = memory->accessCount;
        entry["isPinned"] = memory->isPinned;
        state["memories"].push_back(std::move(entry));
    }

    return state;
}

bool AgenticMemorySystem::importState(const nlohmann::json& state)
{
    if (!state.is_object() || !state.contains("memories") || !state["memories"].is_array()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    std::map<std::string, std::unique_ptr<MemoryEntry>> restored;

    for (const auto& entry : state["memories"]) {
        if (!entry.is_object() || !entry.contains("id") || !entry["id"].is_string() ||
            !entry.contains("type") || !entry["type"].is_string() ||
            !entry.contains("content") || !entry["content"].is_string()) {
            return false;
        }

        auto memory = std::make_unique<MemoryEntry>();
        memory->id = entry["id"].get<std::string>();
        memory->type = memoryTypeFromString(entry["type"].get<std::string>());
        memory->content = entry["content"].get<std::string>();
        memory->metadata = entry.value("metadata", "");
        memory->timestamp = fromUnixMillis(entry.value("timestampMs", 0));
        memory->relevanceScore = entry.value("relevanceScore", 1.0f);
        memory->accessCount = entry.value("accessCount", 0);
        memory->isPinned = entry.value("isPinned", false);
        restored[memory->id] = std::move(memory);
    }

    m_memories = std::move(restored);
    m_systemStartTime = fromUnixMillis(state.value("systemStartTimeMs", 0));
    m_totalStored = state.value("totalStored", static_cast<uint64_t>(m_memories.size()));
    m_totalRetrieved = state.value("totalRetrieved", 0ULL);
    return true;
}

