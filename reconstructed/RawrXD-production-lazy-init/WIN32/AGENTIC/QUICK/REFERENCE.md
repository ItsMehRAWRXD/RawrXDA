# Win32/Agentic/Autonomous Enhancement - Quick Reference & Implementation Guide

## Files Created & Modified

### New Files Created
| File | Lines | Purpose |
|------|-------|---------|
| `src/ai/AutonomousMissionScheduler.h` | 650 | Mission scheduler interface & data structures |
| `src/ai/AutonomousMissionScheduler.cpp` | 950 | Mission scheduler implementation |
| `tests/test_autonomous_agentic_win32_integration.h` | 350 | Test suite definitions |
| `tests/test_autonomous_agentic_win32_integration.cpp` | 750 | Test implementations |
| `WIN32_AGENTIC_ENHANCEMENT_SUMMARY.md` | 400 | Complete documentation |

### Files Modified
| File | Changes | Impact |
|------|---------|--------|
| `src/ai/production_readiness.cpp` | Added Win32 API headers, replaced placeholder getCurrentMemoryUsageMB(), enhanced collectMetrics() and monitorResources() | Real system monitoring instead of placeholders |
| `CMakeLists.txt` | Added AutonomousMissionScheduler.h/cpp to build configuration | Files now compile as part of main executable |

## Quick Build Guide

### Option 1: CMake Command Line
```powershell
cd d:\RawrXD-production-lazy-init
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release --parallel 8
```

### Option 2: Direct MSBuild
```powershell
cd d:\RawrXD-production-lazy-init\build
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" RawrXD-AgenticIDE.sln /p:Configuration=Release /p:Platform=x64
```

### Option 3: Using Ninja (faster)
```powershell
cd d:\RawrXD-production-lazy-init
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
```

## Key Code Examples

### 1. Register & Execute a Mission
```cpp
// Create mission
AutonomousMission systemMonitoringMission;
systemMonitoringMission.missionId = "system-monitor";
systemMonitoringMission.missionName = "System Health Monitor";
systemMonitoringMission.type = AutonomousMission::FixedInterval;
systemMonitoringMission.intervalMs = 5000;  // Every 5 seconds
systemMonitoringMission.priority = AutonomousMission::High;
systemMonitoringMission.task = [this]() {
    // Collect system metrics
    auto& processManager = Win32AgentAPI::GetProcessManager();
    auto processes = processManager.EnumerateProcesses();
    
    QJsonObject result;
    result["process_count"] = (int)processes.size();
    return result;
};

// Register with scheduler
AutonomousMissionScheduler scheduler;
scheduler.registerMission(systemMonitoringMission);
scheduler.start();
```

### 2. Use Win32 API for Real System Data
```cpp
#include <windows.h>
#include <psapi.h>

// Get current process memory
HANDLE hProcess = GetCurrentProcess();
PROCESS_MEMORY_COUNTERS pmc;

if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
    qint64 memoryMB = pmc.WorkingSetSize / (1024 * 1024);
    qDebug() << "Current memory usage:" << memoryMB << "MB";
}

// Get system-wide memory
MEMORYSTATUSEX memStatus;
memStatus.dwLength = sizeof(memStatus);

if (GlobalMemoryStatusEx(&memStatus)) {
    qint64 totalMB = memStatus.ullTotalPhys / (1024 * 1024);
    qint64 usedMB = (memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024);
    qDebug() << "System memory:" << usedMB << "/" << totalMB << "MB";
}
```

### 3. Monitoring Production System
```cpp
// Initialize production readiness
ProductionReadinessOrchestrator readiness;
readiness.initializeAllSystems(agenticExecutor, inferenceEngine);

// Get real system metrics
auto metrics = readiness.getCurrentMetrics();
qDebug() << "System memory usage:" 
         << metrics["system_memory_usage_percent"].toDouble() << "%";
qDebug() << "Process memory:" 
         << metrics["memory_usage_mb"].toInt() << "MB";
qDebug() << "CPU cores:" 
         << metrics["processor_count"].toInt();
qDebug() << "Health score:" 
         << readiness.getOverallHealthScore();
```

### 4. Adaptive Scheduling Under Load
```cpp
// Create adaptive mission
AutonomousMission adaptiveMission;
adaptiveMission.type = AutonomousMission::Adaptive;
adaptiveMission.minIntervalMs = 1000;   // Fast under low load
adaptiveMission.maxIntervalMs = 30000;  // Slow under high load
adaptiveMission.priority = AutonomousMission::Medium;

// Register with scheduler
scheduler.registerMission(adaptiveMission);

// Scheduler automatically adjusts frequency based on system load
// Under high load (>70%), missions execute less frequently
// Under low load (<30%), missions execute more frequently
```

### 5. Handle Mission Execution Results
```cpp
// Connect to mission completion signal
connect(&scheduler, &AutonomousMissionScheduler::missionCompleted,
        this, [](const QString& missionId, const QJsonObject& result) {
    qDebug() << "Mission completed:" << missionId;
    qDebug() << "Result:" << result;
});

// Connect to mission failure signal
connect(&scheduler, &AutonomousMissionScheduler::missionFailed,
        this, [](const QString& missionId, const QString& error) {
    qWarning() << "Mission failed:" << missionId << "Error:" << error;
});

// Connect to system load alerts
connect(&scheduler, &AutonomousMissionScheduler::systemLoadHigh,
        this, [](double cpuPercent, double memoryPercent) {
    qWarning() << "High system load - CPU:" << cpuPercent 
               << "% Memory:" << memoryPercent << "%";
});
```

## Performance Tuning

### Adjust Scheduling Parameters
```cpp
// Control concurrent task execution
scheduler.setMaxConcurrentTasks(16);  // Default: 8

// Set resource limits (in MB)
scheduler.setResourceConstraints(4096, 16);  // 4GB max, 16 concurrent tasks

// Adjust system load thresholds (in percent)
scheduler.setSystemLoadThreshold(85.0, 90.0);  // CPU: 85%, Memory: 90%

// Change metrics collection interval (in ms)
scheduler.setMetricsCollectionInterval(10000);  // Every 10 seconds
```

### Monitor Performance
```cpp
// Get scheduler metrics
auto metrics = scheduler.getSchedulerMetrics();
qDebug() << "Active tasks:" << metrics["active_tasks"].toInt();
qDebug() << "Pending missions:" << metrics["pending_missions"].toInt();
qDebug() << "Success rate:" << metrics["success_rate_percent"].toDouble() << "%";

// Get system load factor (0.0-1.0)
double loadFactor = scheduler.getSystemLoadFactor();
if (loadFactor > 0.8) {
    qWarning() << "System heavily loaded:" << loadFactor;
}

// Get individual mission metrics
for (const auto& metric : scheduler.getAllMissionMetrics()) {
    qDebug() << "Mission:" << metric["mission_name"].toString()
             << "Executions:" << metric["total_executions"].toInt()
             << "Success rate:" << metric["success_rate_percent"].toDouble() << "%";
}
```

## Testing & Validation

### Run All Tests
```powershell
cd d:\RawrXD-production-lazy-init\build
ctest --output-on-failure --verbose
```

### Run Specific Test Suite
```powershell
# Win32 API tests
ctest -R "Win32" --output-on-failure

# Mission scheduler tests
ctest -R "MissionScheduler" --output-on-failure

# Production readiness tests
ctest -R "ProductionReadiness" --output-on-failure

# Integration tests
ctest -R "EndToEnd" --output-on-failure
```

### Manual Testing
```powershell
# Run IDE and check system monitoring
.\bin\RawrXD.exe

# Enable debug logging for detailed output
set QT_LOGGING_RULES=*.debug=true

# Run with performance monitoring
.\bin\RawrXD.exe --enable-profiling
```

## Troubleshooting Common Issues

### Issue: CMake Can't Find Qt6
```powershell
# Set Qt path explicitly
cmake .. -DQt6_DIR="C:/Qt/6.x.x/msvc2022_64/lib/cmake/Qt6"
```

### Issue: Linker Errors (missing psapi.lib, etc)
```
Solution: Already handled via pragma comments
If persists: Check MSVC installation has Windows SDK
```

### Issue: Mission Not Executing
```cpp
// Verify mission is enabled
QVERIFY(scheduler.getMission("mission-id")->enabled);

// Check if scheduler is running
QVERIFY(scheduler.m_running);

// Verify resource constraints allow execution
QVERIFY(scheduler.canExecuteMission(*mission));

// Check pending queue
qDebug() << "Pending:" << scheduler.getPendingMissions().size();
```

### Issue: High Memory Usage
```cpp
// Reduce concurrent tasks
scheduler.setMaxConcurrentTasks(4);

// Disable adaptive scheduling
scheduler.enableAdaptiveScheduling(false);

// Clear mission metrics
// (add cleanup method if needed)
```

## API Quick Reference

### AutonomousMissionScheduler
```cpp
// Mission management
bool registerMission(const AutonomousMission& mission);
bool unregisterMission(const QString& missionId);
bool enableMission(const QString& missionId);
bool disableMission(const QString& missionId);
bool rescheduleMission(const QString& missionId, int newIntervalMs);

// Scheduler control
void start();
void stop();
void pauseAll();
void resumeAll();

// Configuration
void setSchedulingStrategy(SchedulingStrategy strategy);
void setResourceConstraints(qint64 maxMemoryMB, int maxTasks);
void setSystemLoadThreshold(double cpuPercent, double memoryPercent);

// Metrics
QJsonObject getSchedulerMetrics() const;
QJsonObject getMissionMetrics(const QString& missionId) const;
double getSystemLoadFactor() const;

// Signals
void missionCompleted(const QString& missionId, const QJsonObject& result);
void missionFailed(const QString& missionId, const QString& error);
void systemLoadHigh(double cpuPercent, double memoryPercent);
```

### ProductionReadinessOrchestrator
```cpp
// Initialization
bool initializeAllSystems(AgenticExecutor* executor, InferenceEngine* inference);
bool isInitialized() const;

// Monitoring
QJsonObject getCurrentMetrics() const;
double getOverallHealthScore() const;
QStringList getSystemRecommendations() const;

// Configuration
void loadProductionConfiguration(const QString& configPath);
void setResourceLimits(const QVariantMap& limits);

// Signals
void systemHealthChanged(double healthScore);
void resourceLimitExceeded(const QString& resource, double current, double limit);
```

## Integration Checklist

- [ ] Build compiles without errors
- [ ] All 150+ tests pass
- [ ] Memory profiling shows no leaks
- [ ] Performance meets baseline requirements
- [ ] Thread safety verified
- [ ] Error handling tested
- [ ] Documentation reviewed
- [ ] Code review completed
- [ ] Deployment plan approved
- [ ] Monitoring dashboards configured

## Next Steps

1. **Immediate (1-2 days)**
   - Run full test suite
   - Verify memory profiling
   - Performance baseline testing

2. **Short-term (1-2 weeks)**
   - Production deployment
   - Monitoring setup
   - Incident response testing

3. **Medium-term (1 month)**
   - User feedback collection
   - Performance optimization
   - Feature enhancements

4. **Long-term (3+ months)**
   - Machine learning scheduling
   - Distributed mission coordination
   - Advanced caching & deduplication

---

**For questions or support:** Reference WIN32_AGENTIC_ENHANCEMENT_SUMMARY.md for detailed documentation.
