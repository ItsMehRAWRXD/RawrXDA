# Subagent Multitasking - Quick Reference

## One-Minute Setup

```cpp
#include "chat_session_subagent_bridge.hpp"

// 1. Get the global bridge
auto bridge = ChatSessionSubagentManager::getInstance();

// 2. Initialize for your chat session (creates up to 20 subagents)
bridge->initializeForSession("my_session_id", 5);

// 3. Submit tasks
QString taskId = bridge->submitChatTask("my_session_id", 
    "Do something",
    []() { return "Done!"; });

// 4. Get results
QString result = bridge->getTaskResultForSession("my_session_id", taskId);

// 5. Cleanup when done
bridge->cleanupSession("my_session_id");
```

## Key APIs

### Chat Session Management

| API | Purpose |
|-----|---------|
| `initializeForSession(id, count)` | Create subagent pool for session |
| `cleanupSession(id)` | Free resources |
| `isSessionInitialized(id)` | Check if initialized |

### Subagent Control

| API | Purpose |
|-----|---------|
| `getSubagentCountForSession(id)` | Get total agents |
| `getAvailableSubagentsForSession(id)` | Get idle agents |
| `addSubagentToSession(id)` | Add 1 agent (max 20) |
| `removeSubagentFromSession(id)` | Remove 1 agent |
| `scaleSubagentsForSession(id, count)` | Set to exact count |

### Task Submission

| API | Purpose |
|-----|---------|
| `submitChatTask(id, desc, handler)` | Single task |
| `submitParallelChatTasks(id, tasks)` | Run all at once |
| `submitSequentialChatTasks(id, tasks)` | Run one at a time |

### Task Management

| API | Purpose |
|-----|---------|
| `getTaskStatusForSession(id, taskId)` | Get task state |
| `getTaskResultForSession(id, taskId)` | Get result (JSON) |
| `cancelTaskForSession(id, taskId)` | Cancel task |

### Monitoring

| API | Purpose |
|-----|---------|
| `getSessionMetricsJson(id)` | Per-session metrics |
| `getGlobalMetricsJson()` | All sessions metrics |
| `getTotalActiveSubagents()` | Total agents across all |

## Task Types

### Simple Task
```cpp
bridge->submitChatTask("session_id", "Analyze code",
    []() { return "Analysis complete"; });
```

### Parallel Tasks
```cpp
bridge->submitParallelChatTasks("session_id", {
    "Fetch API 1",    // ├─ runs in parallel
    "Fetch API 2",    // ├─ runs in parallel
    "Fetch API 3"     // └─ runs in parallel
});
```

### Sequential Tasks
```cpp
bridge->submitSequentialChatTasks("session_id", {
    "Download file",  // 1st - waits for completion
    "Extract file",   // 2nd - waits for 1st
    "Process data"    // 3rd - waits for 2nd
});
```

## Constraints

- **Max 20 subagents per session** (hard limit)
- **Max 30 seconds per task** (configurable)
- **Auto-retry: 3 times** (configurable)
- **Memory: ~64MB per agent** (estimate)

## Status Codes

| Status | Meaning |
|--------|---------|
| `pending` | Waiting to execute |
| `running` | Currently executing |
| `completed` | Successfully finished |
| `failed` | Execution error |
| `cancelled` | User cancelled |
| `retrying` | Retrying after failure |

## Signals/Events

```cpp
auto bridge = ChatSessionSubagentManager::getInstance();

// Connect to events
connect(bridge, &ChatSessionSubagentBridge::taskCompleted,
    [](const QString& sessionId, const QString& taskId, const QString& result) {
        qInfo() << "Task" << taskId << "completed with:" << result;
    });

connect(bridge, &ChatSessionSubagentBridge::taskFailed,
    [](const QString& sessionId, const QString& taskId, const QString& error) {
        qWarning() << "Task" << taskId << "failed:" << error;
    });

connect(bridge, &ChatSessionSubagentBridge::subagentAdded,
    [](const QString& sessionId, int totalCount) {
        qInfo() << "Subagent added. Total:" << totalCount;
    });
```

## Performance Tips

1. **Parallel > Sequential** - Use parallel tasks for independent work
2. **Keep tasks small** - 100ms-1s is ideal, avoid > 30s
3. **Monitor metrics** - Check CPU/memory to prevent overload
4. **Scale dynamically** - Add agents when queue grows, remove when idle
5. **Handle failures** - Tasks auto-retry, but log important ones

## Example: Chat Integration

```cpp
class MyChatWindow {
    void onUserInput(const QString& message) {
        auto bridge = ChatSessionSubagentManager::getInstance();
        
        // Initialize if needed
        if (!bridge->isSessionInitialized(m_sessionId)) {
            bridge->initializeForSession(m_sessionId, 5);
        }
        
        // Submit analysis task
        QString taskId = bridge->submitChatTask(m_sessionId,
            "Analyze user message",
            [this, message]() -> QString {
                return analyzeMessage(message);
            });
        
        // Show in chat
        m_chat->addMessage("assistant", "Analyzing... " + taskId);
    }
    
    void onTaskCompleted(const QString& sessionId, 
                        const QString& taskId, 
                        const QString& result) {
        if (sessionId == m_sessionId) {
            // Update chat with result
            m_chat->addMessage("assistant", result);
        }
    }
};
```

## Files to Include

```cpp
#include "chat_session_subagent_bridge.hpp"  // Main API
#include "subagent_task_distributor.hpp"     // Advanced features
#include "subagent_manager.hpp"              // Core manager
```

## Metrics Output Example

```json
{
  "sessionId": "my_session_id",
  "subagentCount": 5,
  "availableSubagents": 3,
  "maxSubagents": 20,
  "maxConcurrentTasks": 20,
  "cpuUsagePercent": 42.5,
  "memoryUsageMB": 320,
  "autoScalingEnabled": true
}
```

## Troubleshooting Checklist

- [ ] Session initialized with `initializeForSession()`?
- [ ] Enough idle agents? `getAvailableSubagentsForSession()` > 0?
- [ ] Task within 30s timeout? Increase with `setResourceLimits()`?
- [ ] Memory usage < 80%? Check `getCoordinatorMetrics()`?
- [ ] Tasks being retried? Check logs for exceptions?
- [ ] Cleanup called on session end? Call `cleanupSession()`?

## Limits at a Glance

| Limit | Value |
|-------|-------|
| Max subagents/session | **20** |
| Max concurrent tasks | 100 |
| Max task timeout | 30s (default) |
| Max retries/task | 3 (default) |
| Memory/agent | ~64MB |
| Task queue | Unlimited |

---

**For full documentation**: See `SUBAGENT_MULTITASKING_GUIDE.md`
