# Qt Dependency Removal - Comprehensive Todo List

## Current Status: IN PROGRESS
**Goal**: Remove ALL Qt dependencies and unnecessary logging/instrumentation  
**Replacements**: 
- `QString` → `std::string`
- `QObject`/signals → Plain C++ (callback functions or std::function)
- `QMutex`/`QMutexLocker` → `std::mutex`/`std::lock_guard`
- `QJson*` → `nlohmann/json` or `std::map`
- `QDebug` → remove entirely
- `QFile`, `QDir`, `QStandardPaths` → `std::filesystem`
- `QThread` → `std::thread`
- `QTimer` → custom timer or `std::chrono`
- `QList`, `QVector`, `QMap`, `QHash` → STL containers
- `QProcess` → direct system calls or subprocess library

---

## Phase 1: Core Agent System (rawrxd/src/agent/)

### Key Agent Files to Convert
- [x] universal_model_router.h - Started
- [ ] universal_model_router.cpp - TODO
- [ ] auto_bootstrap.cpp - Remove QCoreApplication, QInputDialog, QMessageBox, QClipboard, QTimer, QProcessEnvironment, QtConcurrent
- [ ] agent_hot_patcher.cpp/hpp - Remove QObject, QMutex, QMutexLocker, QDebug, QDateTime, QJsonArray, QFile, QDir
- [ ] agentic_copilot_bridge.cpp/hpp - Remove QObject, QDebug, QDateTime, QJsonDocument, QElapsedTimer, QTimer, QThread
- [ ] agentic_failure_detector.cpp/hpp - Remove QObject, QMutex, QMutexLocker, QDebug, QRegularExpression
- [ ] agentic_puppeteer.cpp/hpp - Remove QObject, QMutex, QMutexLocker, QJsonDocument, QDebug, QRegularExpression
- [ ] ide_agent_bridge.cpp/hpp - Remove QObject, QDebug, QDateTime, QTimer, QDir, QJsonArray
- [ ] model_invoker.cpp/hpp - Check for Qt usage
- [ ] planner.cpp/hpp - Check for Qt usage
- [ ] release_agent.cpp/hpp - Check for Qt usage
- [ ] self_patch.cpp/hpp - Check for Qt usage
- [ ] meta_learn.cpp/hpp - Check for Qt usage
- [ ] action_executor.cpp/hpp - Check for Qt usage

### Logging Calls to Remove
- Remove all `qDebug()` calls
- Remove all logging instrumentation from error handlers
- Keep error handling logic, but remove debug output

---

## Phase 2: Agent-Related Files (rawrxd/src/agentic/)

### Files in agentic/ directory
- [ ] agentic_agent_coordinator.cpp
- [ ] agentic_configuration.cpp
- [ ] agentic_controller.cpp
- [ ] agentic_core.cpp
- [ ] agentic_engine.cpp
- [ ] agentic_error_handler.cpp
- [ ] agentic_executor.cpp
- [ ] agentic_file_operations.cpp
- [ ] agentic_ide.cpp
- [ ] agentic_ide_main.cpp
- [ ] agentic_iterative_reasoning.cpp
- [ ] agentic_loop_state.cpp
- [ ] agentic_memory_system.cpp
- [ ] agentic_observability.cpp (REMOVE ENTIRELY - instrumentation)
- [ ] agentic_text_edit.cpp
- [ ] agentic_tools.obj
- [ ] gguf_proxy_server.cpp/hpp

### Logging/Observability to Remove
- agentic_observability.cpp - REMOVE ENTIRE FILE
- All telemetry calls
- All metrics collection
- All debug output

---

## Phase 3: Testing and Model Loading (testing_model_loaders/src/)

### Test Files
- [ ] test_model_trainer_validation.cpp - Remove QTest, QSignalSpy, QCoreApplication
- [ ] test_agentic_tools.cpp - Remove QTest, QSignalSpy, QTemporaryDir, QFile, QDebug
- [ ] test_agent_hot_patcher.cpp - Remove QCoreApplication, QDebug, QJsonObject, QTimer
- [ ] test_agent_hot_patcher_integration.cpp - Remove QCoreApplication, QFile, QTextStream
- [ ] test_agentic_executor.cpp - Check for Qt usage

### Model and UI Files
- [ ] model_trainer.h/cpp - Remove QObject, QString, QJsonObject, QThread, QElapsedTimer
- [ ] model_interface.h/cpp - Remove QObject, QString, QJsonObject, QMap
- [ ] autonomous_widgets.h/cpp - Remove QWidget, QLabel, QPushButton, QTextEdit, QVBoxLayout, QHBoxLayout
- [ ] ide_main_window.h/cpp - Remove QMainWindow, QTextEdit, QLabel, QPushButton
- [ ] intelligent_codebase_engine.h/cpp - Remove QObject, QString, QVector, QHash, QSet, QJsonObject, QDateTime

### Qt App Directory (testing_model_loaders/src/qtapp/)
- [ ] MainWindow.cpp/h - Remove QMainWindow, QWidget, QVBoxLayout, QPushButton
- [ ] main.cpp - Remove QApplication, QDebug
- [ ] HexMagConsole.cpp/h - Remove QPlainTextEdit
- [ ] inference_engine.cpp/hpp - Remove QDebug, QDateTime, QElapsedTimer, QUuid, QMutex, QQueue, QHash, QByteArray
- [ ] transformer_inference.cpp/hpp - Remove QObject, QDebug, QDateTime, QElapsedTimer, QHash, QByteArray
- [ ] health_check_server.cpp/hpp - Remove QObject, QTcpServer, QTcpSocket, QDebug, QDateTime, QJsonObject
- [ ] OverlayWidget.cpp/h - Remove QWidget, QPainter, QStyle, QEvent
- [ ] unified_hotpatch_manager.hpp - Check for Qt usage

---

## Phase 4: Other Utilities

### Foundation & Core
- [ ] intelligent_codebase_engine.h/cpp
- [ ] universal_model_router.h/cpp (partially done)
- [ ] model_router_adapter.h/cpp
- [ ] model_registry.h/cpp

### Autonomous Features
- [ ] autonomous_feature_engine.h/cpp
- [ ] autonomous_intelligence_orchestrator.h/cpp
- [ ] autonomous_model_manager.h/cpp

### Cloud/API
- [ ] cloud_api_client.h/cpp

### Telemetry (REMOVE ENTIRELY)
- [ ] telemetry/telemetry.h/cpp
- [ ] observability_dashboard.h/cpp
- [ ] metrics_dashboard.h/cpp
- [ ] performance_monitor.h/cpp
- [ ] profiler.h/cpp

---

## Summary of Changes Per Category

### 1. Header Guards and Includes (REMOVE ALL Q*)
```cpp
// REMOVE:
#include <QString>
#include <QObject>
#include <QDebug>
#include <QJsonObject>
#include <QMutex>
#include <QThread>
// etc.

// REPLACE WITH:
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
```

### 2. Type Replacements
```cpp
// REMOVE: QString → std::string
// REMOVE: QObject inheritance, replace with plain classes
// REMOVE: QMutex → std::mutex
// REMOVE: QMutexLocker → std::lock_guard
// REMOVE: QJsonObject → nlohmann::json
// REMOVE: QList → std::vector
// REMOVE: QMap/QHash → std::map
// REMOVE: QTimer → Timer class or std::chrono
// REMOVE: QThread → std::thread
```

### 3. Signal/Slot System
```cpp
// REMOVE: Q_OBJECT macro, signals:, slots:
// REPLACE WITH: Direct function pointers, std::function callbacks, or observer pattern
```

### 4. Logging Instrumentation
```cpp
// REMOVE ALL:
qDebug() << ...
qWarning() << ...
qCritical() << ...
// Keep functional code, just remove the logging output
```

### 5. Files to DELETE Entirely
- telemetry.cpp/h
- observability_dashboard.cpp/h
- metrics_dashboard.cpp/h
- performance_monitor.cpp/h (keep if needed for actual functionality)
- profiler.cpp/h
- agentic_observability.cpp/h

---

## Key Implementation Notes

1. **Thread Safety**: Replace QMutex/QMutexLocker with `std::mutex` and `std::lock_guard`
   ```cpp
   std::mutex m_mutex;
   std::lock_guard<std::mutex> lock(m_mutex);
   ```

2. **JSON**: Use `nlohmann/json` library
   ```cpp
   #include <nlohmann/json.hpp>
   using json = nlohmann::json;
   json obj = json::parse(jsonString);
   ```

3. **File Operations**: Use `std::filesystem` (C++17)
   ```cpp
   #include <filesystem>
   namespace fs = std::filesystem;
   ```

4. **Callbacks**: Use `std::function`
   ```cpp
   std::function<void(const std::string&)> callback;
   ```

5. **Strings**: Use `std::string` everywhere

---

## Files Status

### Phase 1 (Core Agent System)
- [ ] universal_model_router.h - STARTED
- [ ] universal_model_router.cpp - PENDING
- [ ] auto_bootstrap.cpp - PENDING
- [ ] agent_hot_patcher - PENDING
- [ ] agentic_copilot_bridge - PENDING
- [ ] agentic_failure_detector - PENDING
- [ ] agentic_puppeteer - PENDING
- [ ] ide_agent_bridge - PENDING

### Phase 2-4
- [ ] All other files as listed above

---

## Command to Find All Q* Includes
```bash
grep -r "#include <Q" --include="*.cpp" --include="*.h" --include="*.hpp" D:/rawrxd/src D:/testing_model_loaders/src
```

## Command to Find All qDebug Usage
```bash
grep -r "qDebug" --include="*.cpp" --include="*.h" --include="*.hpp" D:/rawrxd/src D:/testing_model_loaders/src
```

---

**Last Updated**: Now  
**Next Steps**: Continue with Phase 1, focusing on agent core files
