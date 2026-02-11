// RawrXD Task Scheduler - Pure Win32/C++ Implementation  
// Task scheduling, priorities, and execution timing

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
#include <queue>

// ============================================================================
// SCHEDULER STRUCTURES
// ============================================================================

struct ScheduledTask {
    int task_id;
    wchar_t description[512];
    int priority;  // 0-10
    DWORD scheduled_time;
    DWORD execute_time;
    BOOL executed;
    int retry_count;
};

struct ScheduleStats {
    int pending_tasks;
    int executed_tasks;
    int failed_tasks;
    DWORD avg_execution_ms;
};

// ============================================================================
// TASK SCHEDULER
// ============================================================================

class TaskScheduler {
private:
    CRITICAL_SECTION m_cs;
    std::vector<ScheduledTask> m_tasks;
    std::queue<int> m_executeQueue;
    int m_nextTaskId;
    int m_executedCount;
    int m_failedCount;
    DWORD m_totalExecutionTime;
    
public:
    TaskScheduler()
        : m_nextTaskId(1),
          m_executedCount(0),
          m_failedCount(0),
          m_totalExecutionTime(0) {
        InitializeCriticalSection(&m_cs);
    }
    
    ~TaskScheduler() {
        Shutdown();
        DeleteCriticalSection(&m_cs);
    }
    
    void Initialize() {
        EnterCriticalSection(&m_cs);
        m_tasks.clear();
        m_nextTaskId = 1;
        m_executedCount = 0;
        m_failedCount = 0;
        m_totalExecutionTime = 0;
        LeaveCriticalSection(&m_cs);
        OutputDebugStringW(L"[TaskScheduler] Initialized\n");
    }
    
    void Shutdown() {
        EnterCriticalSection(&m_cs);
        m_tasks.clear();
        while (!m_executeQueue.empty()) m_executeQueue.pop();
        LeaveCriticalSection(&m_cs);
    }
    
    // Schedule a task
    int ScheduleTask(const wchar_t* description, int priority, DWORD delayMs) {
        if (!description || priority < 0 || priority > 10) return -1;
        
        EnterCriticalSection(&m_cs);
        
        ScheduledTask task;
        task.task_id = m_nextTaskId++;
        wcscpy_s(task.description, 512, description);
        task.priority = priority;
        task.scheduled_time = GetTickCount();
        task.execute_time = task.scheduled_time + delayMs;
        task.executed = FALSE;
        task.retry_count = 0;
        
        m_tasks.push_back(task);
        int taskId = task.task_id;
        
        LeaveCriticalSection(&m_cs);
        
        wchar_t buf[256];
        swprintf_s(buf, 256, L"[TaskScheduler] Task scheduled: ID=%d Priority=%d DelayMs=%lu\n",
                  taskId, priority, delayMs);
        OutputDebugStringW(buf);
        
        return taskId;
    }
    
    // Get next task to execute
    int GetNextTask() {
        EnterCriticalSection(&m_cs);
        
        DWORD now = GetTickCount();
        int bestTaskId = -1;
        int bestPriority = -1;
        int bestIndex = -1;
        
        // Find highest priority ready task
        for (int i = 0; i < (int)m_tasks.size(); i++) {
            ScheduledTask& task = m_tasks[i];
            
            if (!task.executed && now >= task.execute_time) {
                if (task.priority > bestPriority) {
                    bestPriority = task.priority;
                    bestTaskId = task.task_id;
                    bestIndex = i;
                }
            }
        }
        
        if (bestIndex >= 0) {
            m_tasks[bestIndex].executed = TRUE;
        }
        
        LeaveCriticalSection(&m_cs);
        return bestTaskId;
    }
    
    // Mark task as executed
    BOOL MarkTaskExecuted(int taskId, DWORD executionMs, BOOL success) {
        EnterCriticalSection(&m_cs);
        
        BOOL found = FALSE;
        for (auto& task : m_tasks) {
            if (task.task_id == taskId) {
                task.executed = TRUE;
                if (success) {
                    m_executedCount++;
                    m_totalExecutionTime += executionMs;
                } else {
                    m_failedCount++;
                    task.retry_count++;
                }
                found = TRUE;
                break;
            }
        }
        
        LeaveCriticalSection(&m_cs);
        return found;
    }
    
    // Get pending task count
    int GetPendingCount() {
        EnterCriticalSection(&m_cs);
        
        DWORD now = GetTickCount();
        int count = 0;
        
        for (const auto& task : m_tasks) {
            if (!task.executed && now >= task.execute_time) {
                count++;
            }
        }
        
        LeaveCriticalSection(&m_cs);
        return count;
    }
    
    // Get stats
    ScheduleStats GetStats() {
        ScheduleStats stats;
        stats.pending_tasks = 0;
        stats.executed_tasks = 0;
        stats.failed_tasks = 0;
        stats.avg_execution_ms = 0;
        
        EnterCriticalSection(&m_cs);
        
        DWORD now = GetTickCount();
        for (const auto& task : m_tasks) {
            if (!task.executed && now >= task.execute_time) {
                stats.pending_tasks++;
            }
        }
        
        stats.executed_tasks = m_executedCount;
        stats.failed_tasks = m_failedCount;
        
        if (m_executedCount > 0) {
            stats.avg_execution_ms = m_totalExecutionTime / m_executedCount;
        }
        
        LeaveCriticalSection(&m_cs);
        return stats;
    }
    
    const wchar_t* GetStatus() {
        static wchar_t status[512];
        ScheduleStats stats = GetStats();
        
        swprintf_s(status, 512,
            L"TaskScheduler: %d pending, %d executed, %d failed, Avg: %lu ms",
            stats.pending_tasks, stats.executed_tasks, stats.failed_tasks,
            stats.avg_execution_ms);
        
        return status;
    }
};

// ============================================================================
// C INTERFACE
// ============================================================================

extern "C" {

__declspec(dllexport) TaskScheduler* __stdcall CreateTaskScheduler(void) {
    return new TaskScheduler();
}

__declspec(dllexport) void __stdcall DestroyTaskScheduler(TaskScheduler* sched) {
    if (sched) delete sched;
}

__declspec(dllexport) void __stdcall TaskScheduler_Initialize(TaskScheduler* sched) {
    if (sched) sched->Initialize();
}

__declspec(dllexport) void __stdcall TaskScheduler_Shutdown(TaskScheduler* sched) {
    if (sched) sched->Shutdown();
}

__declspec(dllexport) int __stdcall TaskScheduler_Schedule(TaskScheduler* sched,
    const wchar_t* description, int priority, DWORD delayMs) {
    return sched ? sched->ScheduleTask(description, priority, delayMs) : -1;
}

__declspec(dllexport) int __stdcall TaskScheduler_GetNext(TaskScheduler* sched) {
    return sched ? sched->GetNextTask() : -1;
}

__declspec(dllexport) BOOL __stdcall TaskScheduler_MarkExecuted(TaskScheduler* sched,
    int taskId, DWORD executionMs, BOOL success) {
    return sched ? sched->MarkTaskExecuted(taskId, executionMs, success) : FALSE;
}

__declspec(dllexport) int __stdcall TaskScheduler_GetPending(TaskScheduler* sched) {
    return sched ? sched->GetPendingCount() : 0;
}

__declspec(dllexport) const wchar_t* __stdcall TaskScheduler_GetStatus(TaskScheduler* sched) {
    return sched ? sched->GetStatus() : L"Not initialized";
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
        OutputDebugStringW(L"[RawrXD_TaskScheduler] DLL loaded\n");
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringW(L"[RawrXD_TaskScheduler] DLL unloaded\n");
        break;
    }
    return TRUE;
}
