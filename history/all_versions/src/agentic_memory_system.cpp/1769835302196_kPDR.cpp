#include "agentic_memory_system.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>

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
    // Cleanup handled by unique_ptr
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

