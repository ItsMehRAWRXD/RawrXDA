# Orchestra Integration - Quick Reference

## 🚀 Quick Start (5 minutes)

### 1. Build
```powershell
cd e:\
.\build-orchestra.ps1
```

### 2. Run CLI (Interactive)
```bash
build\Release\orchestra-cli.exe
# Then: add task1 "Analyze code" main.cpp
#       execute 4
#       results
```

### 3. Run GUI
```bash
build\Release\AutonomousIDE.exe
# Orchestra panel on main window
```

---

## CLI Commands

| Command | Example | Result |
|---------|---------|--------|
| `add` | `add task1 "Goal" file.cpp` | Submit task |
| `list` | `list` | Show pending |
| `execute` | `execute 4` | Run with 4 agents |
| `result` | `result task1` | Get task result |
| `results` | `results` | Show all results |
| `progress` | `progress` | Show % complete |
| `clear` | `clear` | Clear all tasks |
| `help` | `help` | Show all commands |
| `quit` | `quit` | Exit |

---

## Batch Commands

```bash
# Submit
orchestra-cli batch submit-task task1 "Goal" file.cpp

# Execute with N agents
orchestra-cli batch execute 4

# Get result
orchestra-cli batch result task1

# List all
orchestra-cli batch list

# Clear
orchestra-cli batch clear
```

---

## Code Integration

### CLI Mode
```cpp
#include "orchestra_integration.h"

// Main: Use CLIOrchestraHelper::parseAndExecute(argc, argv)
// Or: Create OrchestraManager::getInstance()
```

### GUI Mode
```cpp
#include "orchestra_integration.h"
#include "orchestra_gui_widget.h"

// In main window:
OrchestraTaskPanel* panel = new OrchestraTaskPanel(this);
mainLayout->addWidget(panel);
```

### Direct API
```cpp
auto& mgr = OrchestraManager::getInstance();

// Submit
mgr.submitTask("id", "goal", "file.cpp");

// Execute
auto metrics = mgr.executeAllTasks(4);

// Get results
auto result = mgr.getTaskResult("id");
auto all = mgr.getAllResults();

// Progress
double pct = mgr.getProgressPercentage();
```

---

## Task Result Structure

```cpp
struct TaskResult {
    bool success;
    std::string output;
    std::string error_message;
    int exit_code;
    std::chrono::milliseconds execution_time;
    std::string task_id;
};
```

---

## Execution Metrics Structure

```cpp
struct ExecutionMetrics {
    int total_tasks;
    int completed_tasks;
    int failed_tasks;
    int active_agents;
    double success_rate;
    std::chrono::milliseconds total_execution_time;
};
```

---

## Configuration

### Agent Count
- CLI: `execute [N]` (default: 4, max: 16)
- GUI: Spinbox (1-16)
- API: `executeAllTasks(N)`

### Task Properties
- **ID**: Unique identifier (required)
- **Goal**: Task description (required)
- **File**: Associated file path (optional)
- **Dependencies**: List of dependencies (optional)

---

## Performance Quick Tips

1. **Optimal Agents**: 4-8 for most tasks
2. **Large Tasks**: Use fewer agents, longer timeout
3. **Many Small Tasks**: Use more agents (8-16)
4. **Memory Concern**: Clear results between batches
5. **Real-time Feedback**: Check progress every 100ms

---

## Common Patterns

### Pattern 1: Batch Processing
```cpp
OrchestraManager& mgr = OrchestraManager::getInstance();
for (int i = 1; i <= 100; i++) {
    mgr.submitTask("task-" + std::to_string(i), "Process", "file" + std::to_string(i));
}
auto metrics = mgr.executeAllTasks(8);
```

### Pattern 2: Sequential Groups
```cpp
// Group 1
mgr.submitTask("g1-t1", "Goal 1");
mgr.submitTask("g1-t2", "Goal 1");
auto m1 = mgr.executeAllTasks(4);

mgr.clearAllTasks();

// Group 2
mgr.submitTask("g2-t1", "Goal 2");
mgr.submitTask("g2-t2", "Goal 2");
auto m2 = mgr.executeAllTasks(4);
```

### Pattern 3: Real-time Monitoring
```cpp
mgr.submitTask("task1", "Goal");
auto future = std::async([&]() {
    return mgr.executeAllTasks(4);
});

while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
    updateUI(mgr.getProgressPercentage());
}
auto metrics = future.get();
```

---

## Files Reference

| File | Purpose | Lines |
|------|---------|-------|
| `orchestra_integration.h` | Core manager + CLI | 490 |
| `orchestra_gui_widget.h` | Qt GUI panel | 350 |
| `cli_main.cpp` | CLI entry point | 320 |
| `main.cpp` | GUI entry point | 91 |
| `CMakeLists.txt` | Build config | 210 |
| `ORCHESTRA_INTEGRATION_GUIDE.md` | Full docs | 600+ |
| `ORCHESTRA_INTEGRATION_COMPLETION.md` | Summary | 400+ |

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| CLI not found | Run `.\build-orchestra.ps1` |
| GUI panel missing | Check `#include "orchestra_gui_widget.h"` |
| Tasks not executing | Verify agents > 0, check console for errors |
| High memory | Reduce agents, clear results periodically |
| Slow execution | Check task complexity, monitor CPU usage |

---

## Getting Help

1. **Quick Start**: See "Build" section above
2. **Commands**: Check "CLI Commands" table
3. **API**: Check "Code Integration" section
4. **Advanced**: Read `ORCHESTRA_INTEGRATION_GUIDE.md`
5. **Examples**: See "Common Patterns" section

---

## Version Info

- **Component**: Orchestra Integration System
- **Status**: ✅ Production Ready
- **Version**: 1.0.0
- **Build**: Release (O3 optimized)
- **Platforms**: Windows, macOS, Linux
- **Dependencies**: Qt6.5+, C++20
