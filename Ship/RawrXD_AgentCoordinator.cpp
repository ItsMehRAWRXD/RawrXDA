// RawrXD Agent Coordinator - Pure Win32/C++ Implementation
// Replaces: agentic_agent_coordinator.cpp - Multi-agent coordination
// Zero Qt dependencies - just Win32 API + STL

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>
#include <vector>
#include <wchar.h>
#include <windows.h>

// ============================================================================
// AGENT STRUCTURES
// ============================================================================

enum class AgentRole
{
    CODING,
    ANALYSIS,
    TESTING,
    PLANNING,
    EXECUTION
};

struct Agent
{
    int id;
    AgentRole role;
    std::wstring name;
    std::wstring description;
    bool enabled;
    float priority;
    int success_count;
    int failure_count;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_active;
};

struct Task
{
    int task_id;
    std::wstring description;
    int assigned_to_agent_id;
    std::chrono::steady_clock::time_point created_at;
    int priority;
    bool completed;
};

struct CoordinationResult
{
    bool success;
    std::wstring message;
    int completed_tasks;
    int failed_tasks;
    std::chrono::milliseconds duration;
};

// ============================================================================
// EVENT SYSTEM
// ============================================================================

template <typename... Args> class EventSystem
{
  private:
    std::vector<std::function<void(Args...)>> m_handlers;
    std::mutex m_mutex;

  public:
    void Connect(std::function<void(Args...)> handler)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers.push_back(handler);
    }

    void Emit(Args... args)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& handler : m_handlers)
        {
            try
            {
                handler(args...);
            }
            catch (...)
            {
                OutputDebugStringW(L"[AgentCoordinator] Handler exception\n");
            }
        }
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers.clear();
    }
};

// ============================================================================
// AGENT COORDINATOR
// ============================================================================

class AgentCoordinator
{
  private:
    // State
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_running;
    std::wstring m_workspaceRoot;

    // Agents
    std::map<int, Agent> m_agents;
    std::atomic<int> m_nextAgentId;
    mutable std::mutex m_agentsMutex;

    // Tasks
    std::queue<Task> m_taskQueue;
    std::vector<Task> m_completedTasks;
    mutable std::mutex m_tasksMutex;

    // Statistics
    std::atomic<int> m_totalTasksCompleted;
    std::atomic<int> m_totalTasksFailed;
    std::chrono::steady_clock::time_point m_sessionStart;

    // Worker thread - disabled for simpler compilation
    // std::thread m_coordinationThread;
    // std::condition_variable m_taskCV;

  public:
    // Events
    EventSystem<int, std::wstring> AgentRegistered;  // agent_id, name
    EventSystem<int> AgentTaskAssigned;              // agent_id
    EventSystem<int, bool> TaskCompleted;            // task_id, success
    EventSystem<std::wstring> CoordinationUpdate;    // message

    AgentCoordinator()
        : m_initialized(false), m_running(false), m_nextAgentId(1), m_totalTasksCompleted(0), m_totalTasksFailed(0),
          m_sessionStart(std::chrono::steady_clock::now())
    {

        InitializePaths();
        LogInfo(L"Agent Coordinator initialized");
    }

    ~AgentCoordinator() { Shutdown(); }

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    bool Initialize()
    {
        if (m_initialized.exchange(true))
        {
            LogWarning(L"Coordinator already initialized");
            return true;
        }

        m_running = true;
        // Threading disabled for now - simpler version
        // m_coordinationThread = std::thread(&AgentCoordinator::CoordinationWorker, this);

        LogInfo(L"Agent Coordinator initialization complete");

        return true;
    }

    void Shutdown()
    {
        if (!m_running.load())
            return;

        LogInfo(L"Shutting down Agent Coordinator");

        m_running = false;
        // m_taskCV.notify_all();

        // if (m_coordinationThread.joinable()) {
        //     m_coordinationThread.join();
        // }

        AgentRegistered.Clear();
        AgentTaskAssigned.Clear();
        TaskCompleted.Clear();
        CoordinationUpdate.Clear();

        m_initialized = false;
    }

    // ========================================================================
    // AGENT MANAGEMENT
    // ========================================================================

    int RegisterAgent(AgentRole role, const std::wstring& name, const std::wstring& description = L"")
    {
        std::lock_guard<std::mutex> lock(m_agentsMutex);

        int agentId = m_nextAgentId++;

        Agent agent;
        agent.id = agentId;
        agent.role = role;
        agent.name = name;
        agent.description = description;
        agent.enabled = true;
        agent.priority = 1.0f;
        agent.success_count = 0;
        agent.failure_count = 0;
        agent.created_at = std::chrono::system_clock::now();
        agent.last_active = std::chrono::system_clock::now();

        m_agents[agentId] = agent;

        std::wstring roleStr = RoleToString(role);
        LogInfo(L"Agent registered: ID=" + std::to_wstring(agentId) + L" Name=" + name + L" Role=" + roleStr);

        AgentRegistered.Emit(agentId, name);

        return agentId;
    }

    bool UnregisterAgent(int agentId)
    {
        std::lock_guard<std::mutex> lock(m_agentsMutex);

        auto it = m_agents.find(agentId);
        if (it == m_agents.end())
        {
            return false;
        }

        m_agents.erase(it);
        LogInfo(L"Agent unregistered: ID=" + std::to_wstring(agentId));

        return true;
    }

    int GetAgentCount() const
    {
        std::lock_guard<std::mutex> lock(m_agentsMutex);
        return static_cast<int>(m_agents.size());
    }

    std::vector<Agent> GetAgents() const
    {
        std::lock_guard<std::mutex> lock(m_agentsMutex);

        std::vector<Agent> result;
        for (const auto& pair : m_agents)
        {
            result.push_back(pair.second);
        }

        return result;
    }

    Agent GetAgent(int agentId) const
    {
        std::lock_guard<std::mutex> lock(m_agentsMutex);

        auto it = m_agents.find(agentId);
        if (it != m_agents.end())
        {
            return it->second;
        }

        Agent empty;
        empty.id = -1;
        return empty;
    }

    void UpdateAgentPriority(int agentId, float priority)
    {
        std::lock_guard<std::mutex> lock(m_agentsMutex);

        auto it = m_agents.find(agentId);
        if (it != m_agents.end())
        {
            it->second.priority = (priority < 0.0f) ? 0.0f : (priority > 10.0f) ? 10.0f : priority;
        }
    }

    void SetAgentEnabled(int agentId, bool enabled)
    {
        std::lock_guard<std::mutex> lock(m_agentsMutex);

        auto it = m_agents.find(agentId);
        if (it != m_agents.end())
        {
            it->second.enabled = enabled;
            LogInfo(std::wstring(enabled ? L"Enabled" : L"Disabled") + L" agent " + std::to_wstring(agentId));
        }
    }

    // ========================================================================
    // TASK MANAGEMENT
    // ========================================================================

    int SubmitTask(const std::wstring& description, int priority = 1)
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);

        static int nextTaskId = 1;
        Task task;
        task.task_id = nextTaskId++;
        task.description = description;
        task.created_at = std::chrono::steady_clock::now();
        task.priority = priority;
        task.assigned_to_agent_id = -1;
        task.completed = false;

        m_taskQueue.push(task);

        LogInfo(L"Task submitted: ID=" + std::to_wstring(task.task_id) + L" Priority=" + std::to_wstring(priority));

        // m_taskCV.notify_one();  // Disabled - no worker thread

        return task.task_id;
    }

    int GetPendingTaskCount() const
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        return static_cast<int>(m_taskQueue.size());
    }

    int GetCompletedTaskCount() const
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        return static_cast<int>(m_completedTasks.size());
    }

    /** Try to dequeue one task for pipeline; returns true if a task was copied into buffers. */
    bool TryDequeueTask(std::wstring& outDesc, int& outPriority)
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        if (m_taskQueue.empty())
            return false;
        Task t = m_taskQueue.front();
        m_taskQueue.pop();
        outDesc = t.description;
        outPriority = t.priority;
        return true;
    }

    // ========================================================================
    // COORDINATION & EXECUTION
    // ========================================================================

    CoordinationResult CoordinateAllAgents()
    {
        auto startTime = std::chrono::steady_clock::now();

        CoordinationResult result;
        result.success = true;
        result.completed_tasks = 0;
        result.failed_tasks = 0;

        // Get pending tasks and available agents
        std::vector<Task> tasks;
        std::vector<Agent> agents;

        {
            std::lock_guard<std::mutex> lock(m_tasksMutex);
            while (!m_taskQueue.empty())
            {
                tasks.push_back(m_taskQueue.front());
                m_taskQueue.pop();
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_agentsMutex);
            for (const auto& pair : m_agents)
            {
                if (pair.second.enabled)
                {
                    agents.push_back(pair.second);
                }
            }
        }

        // Sort agents by priority
        std::sort(agents.begin(), agents.end(), [](const Agent& a, const Agent& b) { return a.priority > b.priority; });

        // Assign tasks to agents
        size_t agentIndex = 0;
        for (auto& task : tasks)
        {
            if (!agents.empty())
            {
                task.assigned_to_agent_id = agents[agentIndex % agents.size()].id;
                agentIndex++;

                // Mark task as completed (simulated)
                task.completed = true;
                result.completed_tasks++;
                m_totalTasksCompleted++;

                TaskCompleted.Emit(task.task_id, true);

                {
                    std::lock_guard<std::mutex> lock(m_tasksMutex);
                    m_completedTasks.push_back(task);
                }
            }
            else
            {
                result.failed_tasks++;
                m_totalTasksFailed++;
                TaskCompleted.Emit(task.task_id, false);
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        result.message = L"Coordinated " + std::to_wstring(result.completed_tasks) + L" tasks to " +
                         std::to_wstring(agents.size()) + L" agents in " + std::to_wstring(result.duration.count()) +
                         L" ms";

        LogInfo(result.message);
        CoordinationUpdate.Emit(result.message);

        return result;
    }

    // ========================================================================
    // STATISTICS
    // ========================================================================

    struct Statistics
    {
        int total_agents;
        int active_agents;
        int pending_tasks;
        int completed_tasks;
        int total_completed;
        int total_failed;
        std::chrono::milliseconds uptime;
    };

    Statistics GetStatistics() const
    {
        Statistics stats;
        stats.total_completed = m_totalTasksCompleted.load();
        stats.total_failed = m_totalTasksFailed.load();

        {
            std::lock_guard<std::mutex> lock(m_agentsMutex);
            stats.total_agents = static_cast<int>(m_agents.size());
            stats.active_agents = 0;
            for (const auto& pair : m_agents)
            {
                if (pair.second.enabled)
                {
                    stats.active_agents++;
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_tasksMutex);
            stats.pending_tasks = static_cast<int>(m_taskQueue.size());
            stats.completed_tasks = static_cast<int>(m_completedTasks.size());
        }

        auto now = std::chrono::steady_clock::now();
        stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_sessionStart);

        return stats;
    }

    std::wstring GetStatusReport() const
    {
        auto stats = GetStatistics();

        std::wstring report = L"Agent Coordinator Status:\n";
        report += L"  Total Agents: " + std::to_wstring(stats.total_agents) + L"\n";
        report += L"  Active Agents: " + std::to_wstring(stats.active_agents) + L"\n";
        report += L"  Pending Tasks: " + std::to_wstring(stats.pending_tasks) + L"\n";
        report += L"  Completed Tasks: " + std::to_wstring(stats.completed_tasks) + L"\n";
        report += L"  Total Completed: " + std::to_wstring(stats.total_completed) + L"\n";
        report += L"  Total Failed: " + std::to_wstring(stats.total_failed) + L"\n";
        report += L"  Uptime: " + std::to_wstring(stats.uptime.count()) + L" ms\n";

        return report;
    }

  private:
    // ========================================================================
    // WORKER THREAD
    // ========================================================================

    void CoordinationWorker()
    {
        // Disabled - synchronous version used instead
        /*
        while (m_running.load()) {
            {
                std::unique_lock<std::mutex> lock(m_tasksMutex);
                m_taskCV.wait(lock, [this] { return !m_taskQueue.empty() || !m_running.load(); });

                if (!m_running.load()) break;
            }

            // Periodically coordinate
            if (GetPendingTaskCount() > 0 && GetAgentCount() > 0) {
                CoordinateAllAgents();
            }

            Sleep(100);
        }
        */
    }

    void InitializePaths()
    {
        wchar_t buffer[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, buffer);
        m_workspaceRoot = buffer;
    }

    std::wstring RoleToString(AgentRole role) const
    {
        switch (role)
        {
            case AgentRole::CODING:
                return L"Coding";
            case AgentRole::ANALYSIS:
                return L"Analysis";
            case AgentRole::TESTING:
                return L"Testing";
            case AgentRole::PLANNING:
                return L"Planning";
            case AgentRole::EXECUTION:
                return L"Execution";
            default:
                return L"Unknown";
        }
    }

    void LogInfo(const std::wstring& message)
    {
        OutputDebugStringW((L"[AgentCoordinator] INFO: " + message + L"\n").c_str());
    }

    void LogWarning(const std::wstring& message)
    {
        OutputDebugStringW((L"[AgentCoordinator] WARNING: " + message + L"\n").c_str());
    }

    void LogError(const std::wstring& message)
    {
        OutputDebugStringW((L"[AgentCoordinator] ERROR: " + message + L"\n").c_str());
    }
};

// ============================================================================
// C INTERFACE (see RawrXD_AgentCoordinator.h)
// ============================================================================
#include "RawrXD_AgentCoordinator.h"

static AgentCoordinator* ToCoord(AgentCoordinatorHandle h)
{
    return static_cast<AgentCoordinator*>(h);
}

extern "C"
{

    AgentCoordinatorHandle CreateAgentCoordinator(void)
    {
        return new AgentCoordinator();
    }

    void DestroyAgentCoordinator(AgentCoordinatorHandle coordinator)
    {
        delete ToCoord(coordinator);
    }

    BOOL AgentCoordinator_Initialize(AgentCoordinatorHandle coord)
    {
        return coord ? (ToCoord(coord)->Initialize() ? TRUE : FALSE) : FALSE;
    }

    void AgentCoordinator_Shutdown(AgentCoordinatorHandle coord)
    {
        if (coord)
            ToCoord(coord)->Shutdown();
    }

    __declspec(dllexport) int AgentCoordinator_RegisterAgent(AgentCoordinatorHandle coord, int role,
                                                             const wchar_t* name, const wchar_t* description)
    {
        if (!coord || !name)
            return -1;
        AgentRole agentRole = static_cast<AgentRole>(role);
        std::wstring desc = description ? description : L"";
        return ToCoord(coord)->RegisterAgent(agentRole, name, desc);
    }

    int AgentCoordinator_SubmitTask(AgentCoordinatorHandle coord, const wchar_t* description, int priority)
    {
        if (!coord || !description)
            return -1;
        return ToCoord(coord)->SubmitTask(description, priority);
    }

    __declspec(dllexport) int AgentCoordinator_GetAgentCount(AgentCoordinatorHandle coord)
    {
        return coord ? ToCoord(coord)->GetAgentCount() : 0;
    }

    int AgentCoordinator_GetPendingTaskCount(AgentCoordinatorHandle coord)
    {
        return coord ? ToCoord(coord)->GetPendingTaskCount() : 0;
    }

    __declspec(dllexport) const wchar_t* AgentCoordinator_GetStatus(AgentCoordinatorHandle coord)
    {
        if (!coord)
            return L"Coordinator not available";
        static std::wstring status;
        status = ToCoord(coord)->GetStatusReport();
        return status.c_str();
    }

    BOOL AgentCoordinator_TryDequeueTask(AgentCoordinatorHandle coord, wchar_t* descBuffer, int descBufferLen,
                                         int* outPriority)
    {
        if (!coord || !descBuffer || descBufferLen <= 0 || !outPriority)
            return FALSE;
        std::wstring desc;
        int pri = 1;
        if (!ToCoord(coord)->TryDequeueTask(desc, pri))
            return FALSE;
        *outPriority = pri;
        size_t n = desc.size();
        if (n >= (size_t)descBufferLen)
            n = (size_t)descBufferLen - 1;
        for (size_t i = 0; i < n; ++i)
            descBuffer[i] = desc[i];
        descBuffer[n] = L'\0';
        return TRUE;
    }

}  // extern "C"

// DllMain is not defined here. RawrXD_AgentCoordinator is linked into the Win32 IDE exe only.
// The exe gets a single DllMain from rawrxd_cot_dll_entry.obj when MASM objects are linked.
// Defining DllMain here caused LNK2005 duplicate symbol with rawrxd_cot_dll_entry.obj in build_real_lane.