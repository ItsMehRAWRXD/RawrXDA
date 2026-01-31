// RawrXD Agent Pool - Pure Win32/C++ Implementation
// Efficient agent lifecycle and resource management

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <vector>
#include <map>
#include <queue>

// ============================================================================
// POOL STRUCTURES
// ============================================================================

struct PooledAgent {
    int agent_id;
    wchar_t name[128];
    int type;  // 0=Coding, 1=Analysis, 2=Testing, 3=Planning
    BOOL available;
    int tasks_completed;
    int tasks_failed;
    DWORD created_at;
    DWORD last_used_at;
};

struct PoolStats {
    int total_agents;
    int available_agents;
    int busy_agents;
    int total_tasks;
    int total_reuses;
};

// ============================================================================
// AGENT POOL MANAGER
// ============================================================================

class AgentPool {
private:
    CRITICAL_SECTION m_cs;
    std::vector<PooledAgent> m_agents;
    std::queue<int> m_availableAgentIds;
    std::map<int, int> m_agentUseCount;
    int m_nextAgentId;
    int m_maxPoolSize;
    DWORD m_createdAt;
    
public:
    AgentPool(int maxPoolSize = 100)
        : m_nextAgentId(1),
          m_maxPoolSize(maxPoolSize),
          m_createdAt(GetTickCount()) {
        InitializeCriticalSection(&m_cs);
    }
    
    ~AgentPool() {
        Shutdown();
        DeleteCriticalSection(&m_cs);
    }
    
    void Initialize() {
        EnterCriticalSection(&m_cs);
        // Pool starts empty, agents added on demand
        LeaveCriticalSection(&m_cs);
        OutputDebugStringW(L"[AgentPool] Initialized\n");
    }
    
    void Shutdown() {
        EnterCriticalSection(&m_cs);
        m_agents.clear();
        m_agentUseCount.clear();
        while (!m_availableAgentIds.empty()) m_availableAgentIds.pop();
        LeaveCriticalSection(&m_cs);
    }
    
    // Create and add agent to pool
    int CreateAgent(const wchar_t* name, int type) {
        if (!name || m_nextAgentId > m_maxPoolSize) return -1;
        
        EnterCriticalSection(&m_cs);
        
        PooledAgent agent;
        agent.agent_id = m_nextAgentId++;
        wcscpy_s(agent.name, 128, name);
        agent.type = type;
        agent.available = TRUE;
        agent.tasks_completed = 0;
        agent.tasks_failed = 0;
        agent.created_at = GetTickCount();
        agent.last_used_at = 0;
        
        m_agents.push_back(agent);
        int agentId = agent.agent_id;
        
        m_agentUseCount[agentId] = 0;
        m_availableAgentIds.push(agentId);
        
        LeaveCriticalSection(&m_cs);
        
        wchar_t buf[256];
        swprintf_s(buf, 256, L"[AgentPool] Agent created: ID=%d Name=%s Type=%d\n", agentId, name, type);
        OutputDebugStringW(buf);
        
        return agentId;
    }
    
    // Acquire agent from pool
    int AcquireAgent(int preferredType = -1) {
        EnterCriticalSection(&m_cs);
        
        int agentId = -1;
        
        // Try to find preferred type first
        if (preferredType >= 0) {
            for (auto& agent : m_agents) {
                if (agent.available && agent.type == preferredType) {
                    agent.available = FALSE;
                    agent.last_used_at = GetTickCount();
                    agentId = agent.agent_id;
                    break;
                }
            }
        }
        
        // Fall back to any available agent
        if (agentId < 0 && !m_availableAgentIds.empty()) {
            agentId = m_availableAgentIds.front();
            m_availableAgentIds.pop();
            
            for (auto& agent : m_agents) {
                if (agent.agent_id == agentId) {
                    agent.available = FALSE;
                    agent.last_used_at = GetTickCount();
                    break;
                }
            }
        }
        
        LeaveCriticalSection(&m_cs);
        return agentId;
    }
    
    // Release agent back to pool
    BOOL ReleaseAgent(int agentId, BOOL success) {
        EnterCriticalSection(&m_cs);
        
        BOOL found = FALSE;
        for (auto& agent : m_agents) {
            if (agent.agent_id == agentId) {
                agent.available = TRUE;
                if (success) {
                    agent.tasks_completed++;
                } else {
                    agent.tasks_failed++;
                }
                found = TRUE;
                m_availableAgentIds.push(agentId);
                break;
            }
        }
        
        LeaveCriticalSection(&m_cs);
        
        if (found) {
            m_agentUseCount[agentId]++;
        }
        
        return found;
    }
    
    // Get pool statistics
    PoolStats GetStats() {
        PoolStats stats;
        stats.total_agents = 0;
        stats.available_agents = 0;
        stats.busy_agents = 0;
        stats.total_tasks = 0;
        stats.total_reuses = 0;
        
        EnterCriticalSection(&m_cs);
        
        for (const auto& agent : m_agents) {
            stats.total_agents++;
            stats.total_tasks += (agent.tasks_completed + agent.tasks_failed);
            
            if (agent.available) {
                stats.available_agents++;
            } else {
                stats.busy_agents++;
            }
        }
        
        for (const auto& pair : m_agentUseCount) {
            stats.total_reuses += pair.second;
        }
        
        LeaveCriticalSection(&m_cs);
        return stats;
    }
    
    int GetAgentCount() {
        EnterCriticalSection(&m_cs);
        int count = (int)m_agents.size();
        LeaveCriticalSection(&m_cs);
        return count;
    }
    
    int GetAvailableCount() {
        EnterCriticalSection(&m_cs);
        int count = (int)m_availableAgentIds.size();
        LeaveCriticalSection(&m_cs);
        return count;
    }
    
    const wchar_t* GetStatus() {
        static wchar_t status[512];
        PoolStats stats = GetStats();
        
        swprintf_s(status, 512,
            L"AgentPool: %d agents (%d available, %d busy), %d total tasks",
            stats.total_agents, stats.available_agents, stats.busy_agents, stats.total_tasks);
        
        return status;
    }
};

// ============================================================================
// C INTERFACE
// ============================================================================

extern "C" {

__declspec(dllexport) AgentPool* __stdcall CreateAgentPool(int maxSize) {
    return new AgentPool(maxSize);
}

__declspec(dllexport) void __stdcall DestroyAgentPool(AgentPool* pool) {
    if (pool) delete pool;
}

__declspec(dllexport) void __stdcall AgentPool_Initialize(AgentPool* pool) {
    if (pool) pool->Initialize();
}

__declspec(dllexport) void __stdcall AgentPool_Shutdown(AgentPool* pool) {
    if (pool) pool->Shutdown();
}

__declspec(dllexport) int __stdcall AgentPool_CreateAgent(AgentPool* pool,
    const wchar_t* name, int type) {
    return pool ? pool->CreateAgent(name, type) : -1;
}

__declspec(dllexport) int __stdcall AgentPool_AcquireAgent(AgentPool* pool, int preferredType) {
    return pool ? pool->AcquireAgent(preferredType) : -1;
}

__declspec(dllexport) BOOL __stdcall AgentPool_ReleaseAgent(AgentPool* pool,
    int agentId, BOOL success) {
    return pool ? pool->ReleaseAgent(agentId, success) : FALSE;
}

__declspec(dllexport) int __stdcall AgentPool_GetAgentCount(AgentPool* pool) {
    return pool ? pool->GetAgentCount() : 0;
}

__declspec(dllexport) int __stdcall AgentPool_GetAvailableCount(AgentPool* pool) {
    return pool ? pool->GetAvailableCount() : 0;
}

__declspec(dllexport) const wchar_t* __stdcall AgentPool_GetStatus(AgentPool* pool) {
    return pool ? pool->GetStatus() : L"Not initialized";
}

} // extern "C"

// ============================================================================
// DLL ENTRY
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringW(L"[RawrXD_AgentPool] DLL loaded\n");
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringW(L"[RawrXD_AgentPool] DLL unloaded\n");
        break;
    }
    return TRUE;
}
