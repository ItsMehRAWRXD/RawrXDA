# Qt Framework Dependencies Report
**Date:** January 30, 2026  
**Project:** RawrXD  
**Workspace:** D:\rawrxd\src  
**Purpose:** Systematic removal of Qt framework dependencies

---

## EXECUTIVE SUMMARY

The RawrXD project contains extensive Qt framework dependencies across 50+ source files. These dependencies fall into the following categories:

1. **Qt Container Types** (QVector, QHash, QMap, QList, QSet)
2. **Qt Logging System** (qDebug, qInfo, qWarning, qCritical)
3. **Qt Signal/Slot System** (signals:, slots:, emit, Q_OBJECT)
4. **Qt Threading** (QThread, QMutex, QMutexLocker)
5. **Qt File I/O** (QFile, QFileInfo, QFileDialog, QDir)
6. **Qt GUI Components** (QListWidget, QTableWidget, QTreeWidget, etc.)
7. **Qt Utility Classes** (QString, QByteArray, QDateTime, QElapsedTimer, etc.)

---

## DEPENDENCY BREAKDOWN BY TYPE

### 1. Qt Container Types

| Type | Usage Count | Replacement |
|------|------------|-------------|
| QVector | 150+ | std::vector |
| QHash | 20+ | std::unordered_map |
| QMap | 30+ | std::map |
| QList | 10+ | std::vector |
| QPair | 5+ | std::pair |
| QSet | 2+ | std::set/std::unordered_set |

### 2. Qt Logging System

**Total Logging Calls:** 500+

Files with most logging:
- security_manager.cpp: 100+ qDebug/qInfo/qWarning/qCritical
- model_router_adapter.cpp: 60+ logging calls
- model_trainer.cpp: 50+ logging calls
- plan_orchestrator.cpp: 30+ logging calls
- model_router_widget.cpp: 30+ logging calls
- performance_monitor.cpp: 40+ logging calls
- lsp_client.cpp: 40+ logging calls
- profiler.cpp: 20+ logging calls
- multi_file_search.cpp: 20+ logging calls

### 3. Qt Signal/Slot System

**Files with signals/slots:**
- lsp_client.h (signals section)
- model_interface.h (signals section)
- model_registry.h (signals section)
- model_router_adapter.h (signals section)
- model_trainer.h (signals section)
- performance_monitor.h (signals section)
- plan_orchestrator.h (signals section)
- profiler.h (signals section)
- model_router_widget.h (signals section)
- todo_manager.h (signals section)
- todo_dock.h (signals section)
- training_dialog.h (signals section)
- observability_dashboard.h (signals section)
- terminal_pool.h (signals section)
- planning_agent.h (signals section)
- tokenizer_selector.h (signals section)
- ollama_proxy.h (signals section)
- zero_day_agentic_engine.cpp (emit statements)

**Total emit() calls:** 200+

### 4. Qt Threading

**QThread Usage:**
- model_router_adapter.cpp: GenerationThread : public QThread (line 11)
- model_trainer.h: QThread* m_trainingThread (line 303)
- model_trainer.cpp: new QThread(this) (line 90)

**QMutex Usage:**
- multi_file_search.cpp: QMutexLocker (lines 118, 218, 261, 373, 403)

### 5. Qt File I/O Operations

**QFile Usage:**
- model_router_adapter.cpp: lines 498, 588
- model_trainer.cpp: lines 251, 273, 300
- multi_file_search.cpp: line 429
- multi_tab_editor.cpp: lines 35, 91
- lsp_client.cpp: line 607
- metrics_dashboard.cpp: line 372
- model_router_console.cpp: line 172
- observability_dashboard.cpp: line 344
- performance_monitor.cpp: line 659
- plan_orchestrator.cpp: lines 385, 400
- profiler.cpp: line 336
- security_manager.cpp: line 873
- telemetry.cpp: line 78
- tokenizer_selector.cpp: lines 283, 302
- training_dialog.cpp: lines 284, 317, 337
- universal_model_router.cpp: lines 65, 163

**QFileInfo Usage:**
- model_registry.cpp: lines 223, 227
- model_trainer.cpp: lines 217, 765
- multi_file_search.cpp: line 315
- training_dialog.cpp: lines 308, 328, 384, 385, 399, 401, 429, 435

**QFileDialog Usage:**
- metrics_dashboard.cpp: line 354, 364
- model_router_console.cpp: line 303
- multi_tab_editor.cpp: line 89
- training_dialog.cpp: lines 284, 317, 337

---

## DETAILED FILE-BY-FILE BREAKDOWN

### HIGH-PRIORITY FILES (50+ Qt dependencies each)

#### security_manager.cpp
**Total Dependencies:** 100+

**Breakdown:**
- Qt Logging (qDebug, qInfo, qWarning, qCritical): 80+
- Qt File I/O (QFile): 1
- Qt Signal/Slot (emit): 6

**Line Numbers with Dependencies:**
- Lines 36-82: Multiple qInfo, qWarning, qCritical logging
- Lines 123-160: Encryption logging (qDebug, qWarning, qCritical)
- Lines 175-214: Decryption logging
- Lines 459-487: HMAC operations logging
- Lines 520-575: Key rotation logging + emit keyRotationCompleted
- Lines 596-659: Credential storage logging + emit signals
- Lines 682-753: Token refresh and ACL logging
- Lines 777-815: Certificate pinning logging
- Lines 843-893: Audit logging + emit securityEventLogged
- Lines 903-947: Configuration loading logging

**Required Changes:**
1. Replace all qDebug/qInfo/qWarning/qCritical with custom logger
2. Replace emit statements with observer pattern or callbacks
3. Replace QFile with std::ofstream
4. Replace QString with std::string throughout

---

#### model_router_adapter.cpp
**Total Dependencies:** 65+

**Qt Types:**
- Line 11: `class GenerationThread : public QThread` - Replace with std::thread
- Line 16: `QThread(nullptr)` - Replace with std::thread
- Line 365: `connect(_generation_thread, &QThread::finished, ...)` - Replace with callback
- Line 456-458: `QMap<std::string, double>` - Replace with std::map
- Lines 498, 588: `QFile file(file_path)` - Replace with std::ofstream

**Qt Logging (60+ calls):**
- Lines 59, 69, 70, 75: qDebug, qCritical
- Lines 85, 93, 101, 117, 129: qWarning, qDebug, qCritical
- Lines 137-162: Multiple qDebug, qWarning
- Lines 176-250: qWarning and qDebug throughout
- Lines 261-625: emit statements and logging

---

#### model_trainer.cpp
**Total Dependencies:** 50+

**Qt Types:**
- Lines 90-92: QThread creation and connect()
- Lines 217, 251, 273, 300, 765, 770: QFileInfo, QFile, QFile::copy

**Qt Logging (50+ calls):**
- Line 25: qInfo
- Lines 39-65: emit and qInfo
- Lines 83-202: emit statements and logging
- Lines 231-338: File loading with logging
- Lines 387-794: Training execution with extensive logging

---

### MEDIUM-PRIORITY FILES (20-50 Qt dependencies)

#### model_registry.cpp
**Total Dependencies:** 20+

- Lines 37, 64, 67, 172: qCritical, qDebug, qWarning
- Line 202: QVector<ModelVersion>
- Lines 223, 227: QFileInfo operations
- Lines 285-346: emit statements (6 signals)
- Lines 420, 441: QVector<ModelVersion>
- Line 468: QList<QTableWidgetItem*>

#### lsp_client.cpp
**Total Dependencies:** 45+

- Lines 45, 49, 57, 103, 130, 136: Qt logging (qWarning, qInfo, qCritical, qDebug)
- Lines 157, 177: qDebug logging
- Lines 329, 471, 584: QVector types
- Lines 388-602: 20+ lines with emit, qCritical, qDebug
- Lines 607, 609: QFileInfo

#### plan_orchestrator.cpp
**Total Dependencies:** 30+

- Lines 23, 37, 43, 49, 62, 63, 84: emit and qDebug
- Lines 89, 95, 118-151: More emit and qDebug
- Lines 264-380: qDebug and qWarning logging
- Lines 385, 400: QFile operations

#### performance_monitor.cpp
**Total Dependencies:** 40+

- Lines 114-377: recordMetric and emit calls (30+)
- Line 659: QFile
- Lines 225, 241, 255, 285, 292, 327, 380, 381: QVector types

#### model_router_widget.cpp
**Total Dependencies:** 30+

- Line 9: qWarning
- Lines 17, 22, 283: qDebug
- Lines 401, 420, 434, 443, 449: qDebug/qWarning with emit
- Lines 491-551: qDebug + emit statements

---

### LOW-PRIORITY FILES (10-20 Qt dependencies)

#### multi_file_search.cpp
- Line 8: Comment about QMutex
- Lines 118, 218, 261, 373, 403: QMutexLocker
- Line 315: QFileInfo
- Line 372, 424, 426, 429, 461: QFile, QVector

#### profiler.cpp
- Lines 23, 41, 47, 57, 59, 75, 86: qWarning, qDebug
- Lines 122, 229, 233, 338, 345, 351, 365-368: More logging
- Line 336: QFile

#### security_manager.cpp (already detailed above - 100+)

#### model_trainer.h
- Line 303: QThread* m_trainingThread

#### intelligent_codebase_engine.h
- Lines 14, 17, 36, 72: QVector types (5)
- Lines 97-101: QHash types (5)
- Lines 103-105: QVector types (3)
- Line 108: QFileSystemWatcher*

---

## Qt REPLACEMENT FILES (To be consolidated or removed)

### QtReplacements.hpp
- Lines 327-336: Container type aliases
  - `using QList = std::vector<T>`
  - `using QVector = std::vector<T>`
  - `using QHash = std::unordered_map<K,V>`
  - `using QMap = std::map<K,V>`

### QtReplacements_New.hpp
- Lines 262-271: Same replacements as above
- Line 313: `emitSignal()` function stub
- Lines 328, 332: Q_SIGNAL, emit macro definitions

### QtReplacements_Old.hpp
- Lines 183-263: Similar replacements + emit macro stub

### QtGUIStubs.hpp
- Contains placeholder implementations for Qt GUI classes
- Should be replaced with actual UI framework or removed if CLI-only

---

## SIGNAL/SLOT CONVERSION STRATEGY

### Files requiring signal/slot conversion:

1. **lsp_client.h/cpp** - 6+ signals
2. **model_interface.h/cpp** - 3+ signals
3. **model_registry.h/cpp** - 3+ signals
4. **model_router_adapter.h/cpp** - 10+ signals
5. **model_trainer.h/cpp** - 8+ signals
6. **performance_monitor.h/cpp** - 4+ signals
7. **plan_orchestrator.h/cpp** - 5+ signals
8. **profiler.h/cpp** - 3+ signals
9. **model_router_widget.h/cpp** - 8+ signals
10. **todo_manager.h/cpp** - 3+ signals
11. **terminal_pool.h/cpp** - 1+ signal
12. **training_dialog.h/cpp** - 2+ signals
13. **observability_dashboard.h/cpp** - 1+ signal
14. **tokenizer_selector.cpp** - 3+ emit calls
15. **planning_agent.h/cpp** - 3+ signals
16. **ollama_proxy.h/cpp** - 3+ signals
17. **universal_model_router.cpp** - 6+ emit calls
18. **zero_day_agentic_engine.cpp** - 4+ emit calls

**Conversion Approach:**
- Replace with callback functions (std::function)
- Or implement observer pattern with callback vector
- Or use event queue with custom event dispatcher

---

## LOGGING SYSTEM REPLACEMENT

### Current Qt Logging Macros:
- `qDebug()` - Debug messages
- `qInfo()` - Information messages
- `qWarning()` - Warning messages
- `qCritical()` - Critical messages
- `qFatal()` - Fatal messages

### Replacement Options:
1. **Simple Custom Logger** - Static functions for each level
2. **Spdlog Integration** - Industry standard C++ logging
3. **Boost.Log** - Full-featured logging framework
4. **Custom Observer Pattern** - Simple and lightweight

### Files to Update:
- security_manager.cpp (80+ replacements)
- model_router_adapter.cpp (60+ replacements)
- model_trainer.cpp (50+ replacements)
- plan_orchestrator.cpp (30+ replacements)
- And 15+ other files with logging

---

## THREADING REPLACEMENT

### Current Qt Threading:
- `QThread` - Thread class
- `QMutex`/`QMutexLocker` - Mutual exclusion
- `connect()` for thread signals

### Replacement:
- `std::thread` - Standard thread class
- `std::mutex`/`std::lock_guard` - Standard synchronization
- Callbacks or promise/future for completion

### Files to Update:
- model_router_adapter.cpp (GenerationThread class)
- model_trainer.cpp (training thread)
- multi_file_search.cpp (QMutexLocker usage)

---

## ACTION ITEMS - PRIORITY ORDER

### Phase 1: Infrastructure (2-3 days)
1. Create custom logger to replace qDebug/qInfo/qWarning/qCritical
2. Create custom callback system to replace signal/slot
3. Update CMakeLists.txt to remove Qt dependency

### Phase 2: Container Type Replacement (1-2 days)
1. Review QtReplacements.hpp files - ensure they compile with std:: types
2. Replace all QVector with std::vector (150+ occurrences)
3. Replace all QHash with std::unordered_map (20+ occurrences)
4. Replace all QMap with std::map (30+ occurrences)
5. Replace all QList with std::vector (10+ occurrences)

### Phase 3: Logging System Replacement (2-3 days)
1. Replace security_manager.cpp logging (80+ calls)
2. Replace model_router_adapter.cpp logging (60+ calls)
3. Replace model_trainer.cpp logging (50+ calls)
4. Replace remaining files' logging (200+ calls)

### Phase 4: File I/O Replacement (1 day)
1. Replace QFile with std::ofstream/std::ifstream
2. Replace QFileInfo with std::filesystem
3. Replace QFileDialog with custom implementation or remove

### Phase 5: Threading Replacement (1-2 days)
1. Replace QThread with std::thread
2. Replace QMutex with std::mutex
3. Replace QMutexLocker with std::lock_guard

### Phase 6: Signal/Slot System Replacement (2-3 days)
1. Create observer/callback infrastructure
2. Convert all signal declarations to function pointers/callbacks
3. Convert all emit() calls to callback invocations
4. Convert all connect() calls to callback registrations

### Phase 7: GUI Cleanup (1 day)
1. Remove or replace Qt GUI components
2. Clean up QtGUIStubs.hpp

### Phase 8: Testing & Validation (2-3 days)
1. Compile without Qt framework
2. Run unit tests
3. Verify all functionality intact

---

## FILE LIST FOR SYSTEMATIC REMOVAL

### Remove Qt includes from these files:
- intelligent_codebase_engine.h/cpp
- lsp_client.h/cpp
- metrics_dashboard.h/cpp
- model_interface.h/cpp
- model_registry.h/cpp
- model_router_adapter.h/cpp
- model_trainer.h/cpp
- multi_file_search.h/cpp
- multi_tab_editor.h/cpp
- observability_dashboard.h/cpp
- performance_monitor.h/cpp
- plan_orchestrator.h/cpp
- profiler.h/cpp
- security_manager.h/cpp
- model_router_widget.h/cpp
- planning_agent.h/cpp
- terminal_pool.h/cpp
- todo_dock.h/cpp
- todo_manager.h/cpp
- training_dialog.h/cpp
- training_progress_dock.h/cpp
- tokenizer_selector.h/cpp
- universal_model_router.h/cpp
- zero_day_agentic_engine.h/cpp
- ollama_proxy.h/cpp
- scalar_server.cpp
- And 20+ others

---

## SUMMARY STATISTICS

| Metric | Count |
|--------|-------|
| Files with Qt dependencies | 50+ |
| QVector occurrences | 150+ |
| QHash occurrences | 20+ |
| QMap occurrences | 30+ |
| Qt logging calls (qDebug/qInfo/qWarning/qCritical) | 500+ |
| emit() statements | 200+ |
| connect() calls | 15+ |
| QThread usages | 3 |
| QMutex/QMutexLocker usages | 5 |
| QFile operations | 20+ |
| QFileInfo operations | 15+ |
| Signal declarations | 18+ |

---

## ESTIMATED EFFORT

- **Development Time:** 10-15 days
- **Testing Time:** 3-5 days
- **Total Effort:** 13-20 person-days

---

*Report Generated: January 30, 2026*
