# RawrXD Multi-Instance Enhancement - Quick Reference

## 🚀 Quick Start

### Launch New Instance
```bash
# Default (auto-allocate port)
RawrXD.exe

# Specific port
RawrXD.exe --port 11435

# Specific working directory
RawrXD.exe --working-dir D:\Projects\MyProject
```

**In IDE:** `File > New Instance Window` or `Ctrl+Shift+N`

---

## 📋 Key Components

### InstanceManager
**Purpose:** Multi-instance coordination and IPC  
**Files:** `src/qtapp/InstanceManager.h/cpp`

```cpp
// Get instance info
qDebug() << instanceMgr->instanceId();  // "12345_20260115_143000"
qDebug() << instanceMgr->port();         // 11434
qDebug() << instanceMgr->isPrimary();    // true

// List all instances
QMap<QString, InstanceInfo> instances = instanceMgr->getRunningInstances();

// Launch new instance
qint64 pid = instanceMgr->launchNewInstance("D:\\Projects");

// Send message to another instance
instanceMgr->sendMessageToInstance("12346_...", "FOCUS_FILE:main.cpp");
```

### AsyncTerminalManager
**Purpose:** Non-blocking terminal operations  
**Files:** `src/qtapp/AsyncTerminalManager.h/cpp`

```cpp
// Create session
QString sessionId = termMgr->createSession(
    RawrXD::AsyncTerminalManager::PowerShell,
    "D:\\Projects"
);

// Execute async (non-blocking)
QString taskId = termMgr->executeCommandAsync(sessionId, "dir", false);

// Execute sync (blocking)
RawrXD::TerminalResult result = termMgr->executeCommandSync(
    sessionId, "git status", 5000
);

// Save sessions
termMgr->saveSessions("path/to/sessions");
```

### WorkerThreadPool
**Purpose:** Background task execution  
**Files:** `src/qtapp/WorkerThread.h/cpp`

```cpp
// Create task
auto task = new RawrXD::WorkerTask("LoadModel", [](auto* t) {
    // Your CPU-intensive work here
    t->setProgress(50);
}, RawrXD::TaskPriority::High);

// Submit task
QString taskId = workerPool->submitTask(task);

// Monitor progress
connect(workerPool, &WorkerThreadPool::taskProgress, 
    [](const QString& id, int progress) {
        qDebug() << id << progress << "%";
    });
```

---

## 🔧 Configuration

### Port Range
- **Default:** 11434-11534 (100 ports)
- **Primary instance:** 11434 (Ollama default)
- **Additional instances:** Auto-allocated

### Limits
- **Max instances:** 32 (configurable)
- **Max terminals/instance:** 16 (configurable)
- **Max worker threads:** Auto (typically 4-16)

### Paths
**Windows:**
- Settings: `%APPDATA%\RawrXD\settings_<pid>.json`
- Terminal sessions: `%APPDATA%\RawrXD\terminal_sessions\`
- Shared memory: `RawrXD_InstanceRegistry` (system)

**Linux:**
- Settings: `~/.config/RawrXD/settings_<pid>.json`
- Terminal sessions: `~/.config/RawrXD/terminal_sessions/`

---

## 🎯 Common Tasks

### Check Running Instances
```cpp
QMap<QString, InstanceInfo> instances = instanceMgr->getRunningInstances();
qDebug() << "Running:" << instances.size() << "instances";

for (const auto& info : instances) {
    qDebug() << info.instanceId << info.port << info.workingDir;
}
```

### Create & Use Terminal Session
```cpp
// 1. Create session
QString sid = termMgr->createSession(AsyncTerminalManager::PowerShell);

// 2. Execute commands
termMgr->executeCommandAsync(sid, "cd src");
termMgr->executeCommandAsync(sid, "ls -la");

// 3. Get output
connect(termMgr, &AsyncTerminalManager::stdoutReady,
    [](const QString& sessionId, const QByteArray& data) {
        qDebug() << data;
    });

// 4. Close when done
termMgr->closeSession(sid, true); // Save state
```

### Submit Background Task
```cpp
// Long-running model loading
auto loadTask = new WorkerTask("LoadLargeModel", [](WorkerTask* t) {
    for (int i = 0; i <= 100; i += 10) {
        if (t->isCancelled()) return;
        
        // Load chunk of model
        loadModelChunk(i);
        
        t->setProgress(i);
        QThread::msleep(100);
    }
}, TaskPriority::High);

QString taskId = workerPool->submitTask(loadTask);

// Cancel if needed
workerPool->cancelTask(taskId);
```

---

## ⚡ Performance Tips

### DO ✅
- Use async terminal execution for long commands
- Submit independent tasks in batches
- Close unused sessions/instances
- Enable terminal session persistence
- Monitor resource usage

### DON'T ❌
- Launch > 10 instances on low-memory systems
- Use sync terminal execution in UI thread
- Create > 16 terminals per instance
- Submit High priority tasks excessively
- Ignore task cancellation signals

---

## 🐛 Troubleshooting

### "Failed to initialize instance"
```bash
# Check running instances
tasklist | findstr RawrXD

# Clear shared memory (Windows PowerShell as Admin)
# Restart IDE
```

### "No available ports"
```bash
# Check port usage
netstat -ano | findstr "1143[4-9]"

# Close unused instances or configure custom range
# Settings > Advanced > API Server > Port Range
```

### Terminal sessions not restoring
```bash
# Check sessions directory
ls %APPDATA%\RawrXD\terminal_sessions\

# Verify JSON files are valid
# Delete corrupted files if needed
```

### High CPU usage
```
Settings > Performance > Worker Threads > Set to 50% of cores
Help > Performance Monitor > Check active tasks
Restart IDE to reset thread pool
```

---

## 📊 Monitoring

### Instance Manager Dialog
`Help > Instance Manager` or `Ctrl+Shift+I`

Shows:
- All running instances (PID, port, status)
- Resource usage per instance
- IPC message history
- Launch/terminate controls

### Performance Monitor
`Help > Performance Monitor` or `F12`

Shows:
- Worker thread activity
- Terminal session status
- Task queue depth
- Memory/CPU per component

---

## 🔑 Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| New Instance | `Ctrl+Shift+N` |
| Instance Manager | `Ctrl+Shift+I` |
| New Terminal | `Ctrl+Shift+T` |
| Terminal Palette | `Ctrl+Shift+P` |
| Performance Monitor | `F12` |

---

## 📚 Architecture Summary

```
┌─────────────────────────────────────────┐
│         RawrXD Multi-Instance IDE       │
├─────────────────────────────────────────┤
│ ┌─────────────┐  ┌─────────────┐       │
│ │ Instance 1  │  │ Instance N  │       │
│ │ Port: 11434 │  │ Port: 1143N │       │
│ └──────┬──────┘  └──────┬──────┘       │
│        └─────────────────┘              │
│                │                        │
│     ┌──────────▼──────────┐             │
│     │ QSharedMemory       │             │
│     │ Instance Registry   │             │
│     └──────────┬──────────┘             │
│                │                        │
│    ┌───────────┼───────────┐            │
│    │           │           │            │
│ ┌──▼───┐  ┌───▼────┐  ┌──▼──────┐      │
│ │ Settings│ Terminal│ │ Workers  │      │
│ │(Isolated)│ (Async) │ │(Threads) │      │
│ └────────┘  └────────┘  └─────────┘      │
└─────────────────────────────────────────┘
```

**Design Decisions:**
- ✅ Fully isolated instances (Option B)
- ✅ Terminal session persistence enabled
- ✅ Soft resource limits with warnings
- ✅ Dynamic port allocation (11434-11534)
- ✅ Lock-free telemetry collection

---

## 📞 Support

**Documentation:** [MULTI_INSTANCE_ENHANCEMENT_GUIDE.md](./MULTI_INSTANCE_ENHANCEMENT_GUIDE.md)  
**Issues:** https://github.com/ItsMehRAWRXD/RawrXD/issues  
**Discord:** https://discord.gg/rawrxd

---

## 🎉 New in v1.0

- ✨ Multi-instance support with automatic port allocation
- ⚡ Async terminal manager with QtConcurrent
- 🔧 Worker thread pool with priority queues
- 💾 Terminal session persistence
- 🔒 Thread-safe resource management
- 📡 Inter-process communication (IPC)
- 📊 Lock-free telemetry buffer

---

**Version:** 1.0.0  
**Date:** January 15, 2026  
**License:** Same as RawrXD IDE

*Quick reference for RawrXD Multi-Instance Enhancement features*
