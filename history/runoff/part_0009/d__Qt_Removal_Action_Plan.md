# Qt Dependency Removal - Systematic Action Plan

## Overview
This document provides a step-by-step action plan to systematically remove all Qt framework dependencies from the RawrXD project, replacing them with C++ standard library equivalents.

**Total Files Affected:** 50+  
**Total Dependencies:** 500+  
**Estimated Duration:** 10-20 days  

---

## PART 1: SETUP & INFRASTRUCTURE (Days 1-2)

### Step 1.1: Create Custom Logger Module
**Files to create:**
- `src/logging/logger.h` - Logger interface
- `src/logging/logger.cpp` - Implementation
- `src/logging/logger_factory.h` - Factory for logger creation

**Functionality needed:**
```cpp
// Replace these Qt macros:
qDebug()     -> Logger::debug()
qInfo()      -> Logger::info()
qWarning()   -> Logger::warning()
qCritical()  -> Logger::critical()
qFatal()     -> Logger::fatal()
```

**Implementation options:**
1. Simple printf-based logging to stdout/stderr
2. Spdlog integration (recommended)
3. Boost.Log integration

### Step 1.2: Create Callback/Signal System
**Files to create:**
- `src/events/callback.h` - Callback template
- `src/events/observer.h` - Observer pattern
- `src/events/event_dispatcher.h` - Event dispatcher

**Replaces:**
- Qt signals: declaration
- emit statements: callback invocation
- connect(): callback registration

**Example structure:**
```cpp
template<typename... Args>
class Signal {
    std::vector<std::function<void(Args...)>> handlers;
    void connect(std::function<void(Args...)> handler) { handlers.push_back(handler); }
    void emit(Args... args) { for(auto& h : handlers) h(args...); }
};
```

### Step 1.3: Update CMakeLists.txt
**Action items:**
- Remove all `find_package(Qt5 ...)` or `find_package(Qt6 ...)`
- Remove Qt include directories
- Remove Qt libraries from linking
- Add std::filesystem support (C++17)
- Add std::thread/mutex support

**Example:**
```cmake
# Remove: find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
# Add: set(CMAKE_CXX_STANDARD 17)
```

### Step 1.4: Create Standard Library Replacements Header
**File to create:** `src/std_replacements.h`

**Contents:**
```cpp
#include <vector>
#include <unordered_map>
#include <map>
#include <string>
#include <memory>
#include <thread>
#include <mutex>

// Already defined in QtReplacements.hpp but consolidate:
template<typename T> using QVector = std::vector<T>;
template<typename K, typename V> using QHash = std::unordered_map<K, V>;
template<typename K, typename V> using QMap = std::map<K, V>;
template<typename T> using QList = std::vector<T>;
using QString = std::string;
```

---

## PART 2: CONTAINER TYPE REPLACEMENT (Days 3-4)

### Step 2.1: Replace QVector (150+ occurrences)
**Pattern:**
```
Search: QVector<
Replace: std::vector<
```

**Files to update (priority order):**
1. intelligent_codebase_engine.h (13 occurrences)
2. model_interface.h (5 occurrences)
3. metrics_dashboard.h (2 occurrences)
4. lsp_client.h (3 occurrences)
5. model_router_adapter.cpp (3+ in QMap/function parameters)
6. performance_monitor.h/cpp (8+ occurrences)
7. All remaining files (100+ more)

### Step 2.2: Replace QHash (20+ occurrences)
**Pattern:**
```
Search: QHash<
Replace: std::unordered_map<
```

**Files:**
- intelligent_codebase_engine.h (3 occurrences)
- lsp_client.h (1 occurrence)
- model_router_adapter.cpp (1 occurrence)
- performance_monitor.h (1 occurrence)
- All others

### Step 2.3: Replace QMap (30+ occurrences)
**Pattern:**
```
Search: QMap<
Replace: std::map<
```

**Files:**
- metrics_dashboard.h (3 occurrences)
- model_interface.h (1 occurrence)
- model_router_adapter.cpp (2 occurrences)
- lsp_client.h (1 occurrence)
- And others

### Step 2.4: Replace QList (10+ occurrences)
**Pattern:**
```
Search: QList<
Replace: std::vector<
```

**Note:** QList is identical to QVector in Qt5, so same replacement applies.

### Step 2.5: Replace QString (100+ occurrences)
**Action:** Most are already using std::string in function signatures. Check for:
- `QString::number()` -> `std::to_string()`
- `QString::fromStdString()` -> Direct use of std::string
- `.toStdString()` -> Remove (already std::string)
- `.c_str()` -> Add where needed

---

## PART 3: LOGGING SYSTEM REPLACEMENT (Days 5-7)

### Step 3.1: Replace security_manager.cpp Logging (80+ calls)
**Pattern to replace:**
```cpp
qDebug()       << "message"     -> Logger::debug("message")
qInfo()        << "message"     -> Logger::info("message")
qWarning()     << "message"     -> Logger::warning("message")
qCritical()    << "message"     -> Logger::critical("message")
```

**Complexity:** HIGH - 80+ individual replacements, stream operators

**Action items:**
1. Update logging infrastructure first
2. Use find/replace with regex or manual conversion
3. Test after each major section

**Sections to update (in order):**
- Lines 36-82 (initialization logging)
- Lines 123-160 (encryption logging)
- Lines 175-214 (decryption logging)
- Lines 459-487 (HMAC logging)
- Lines 520-575 (key rotation logging)
- Lines 596-659 (credential logging)
- Lines 682-753 (token/ACL logging)
- Lines 777-815 (certificate logging)
- Lines 843-893 (audit logging)
- Lines 903-947 (config logging)

### Step 3.2: Replace model_router_adapter.cpp Logging (60+ calls)
**Sections:**
- Lines 59-70 (constructor/destructor logging)
- Lines 75-130 (initialization logging)
- Lines 137-170 (API key loading logging)
- Lines 204-350 (model selection and generation logging)
- Lines 365-415 (generation thread logging)
- Lines 456-550 (statistics and export logging)
- Lines 609-645 (thread callback logging)

### Step 3.3: Replace model_trainer.cpp Logging (50+ calls)
**Sections:**
- Line 25 (initialization)
- Lines 39-87 (error and config logging)
- Lines 83-202 (training execution logging)
- Lines 231-338 (data loading logging)
- Lines 387-794 (training loop logging)

### Step 3.4: Replace Remaining Files Logging (200+ calls)
**Files (priority order):**
1. plan_orchestrator.cpp (30+ calls)
2. performance_monitor.cpp (40+ calls)
3. lsp_client.cpp (40+ calls)
4. model_router_widget.cpp (30+ calls)
5. profiler.cpp (20+ calls)
6. multi_file_search.cpp (20+ calls)
7. model_registry.cpp (20+ calls)
8. And 15+ other files (100+ more calls)

---

## PART 4: FILE I/O REPLACEMENT (Day 8)

### Step 4.1: Replace QFile Operations
**Current usage:**
```cpp
QFile file(filename);
file.open(QIODevice::ReadOnly);
file.read(); file.write();
file.close();
```

**Replacement:**
```cpp
std::ifstream file(filename, std::ios::binary);
file.read(); file.write();
file.close();
// or use file automatically closes on destruction
```

**Files to update:**
- model_router_adapter.cpp (lines 498, 588)
- model_trainer.cpp (lines 251, 273, 300)
- security_manager.cpp (line 873)
- And 15+ other files

### Step 4.2: Replace QFileInfo Operations
**Current usage:**
```cpp
QFileInfo info(filePath);
info.fileName();
info.exists();
QFileInfo::exists(path);
```

**Replacement:**
```cpp
#include <filesystem>
namespace fs = std::filesystem;
auto filename = fs::path(filePath).filename();
fs::exists(filePath);
```

**Files to update:**
- model_registry.cpp (lines 223, 227)
- model_trainer.cpp (lines 217, 765)
- training_dialog.cpp (multiple lines)
- And others

### Step 4.3: Replace QFileDialog Operations
**Current usage:**
```cpp
QFileDialog::getSaveFileName(this, "Title", "", "*.csv");
QFileDialog::getOpenFileName(this, "Title", "", "*.json");
```

**Replacement approach:**
1. Use native file dialogs (platform-specific)
2. Or use simpler file selection system
3. Or remove GUI components entirely

**Files to update:**
- metrics_dashboard.cpp (lines 354, 364)
- model_router_console.cpp (line 303)
- training_dialog.cpp (lines 284, 317, 337)

### Step 4.4: Replace QDir Operations
**Current usage:**
```cpp
QDir dir(path);
dir.entryInfoList();
QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
```

**Replacement:**
```cpp
#include <filesystem>
for(auto& entry : std::filesystem::directory_iterator(path)) {
    // Process entry
}
```

---

## PART 5: THREADING REPLACEMENT (Days 9-10)

### Step 5.1: Replace QThread
**File: model_router_adapter.cpp**

**Current code (line 11):**
```cpp
class GenerationThread : public QThread {
    void run() override { /* ... */ }
};
```

**Replacement:**
```cpp
class GenerationThread {
    std::thread thread_;
    void run() { /* ... */ }
    void start() { thread_ = std::thread(&GenerationThread::run, this); }
    void wait() { if(thread_.joinable()) thread_.join(); }
};
```

**Update signal connection (line 365):**
```cpp
// Old: connect(_generation_thread, &QThread::finished, ...)
// New: Use callback when thread completes
onGenerationThreadFinished();
```

### Step 5.2: Replace QMutex/QMutexLocker
**File: multi_file_search.cpp**

**Current code (lines 118, 218, 261, 373, 403):**
```cpp
QMutexLocker locker(&m_resultsMutex);
```

**Replacement:**
```cpp
std::lock_guard<std::mutex> lock(m_resultsMutex);
```

**Update mutex declaration:**
```cpp
// Old: QMutex m_resultsMutex;
// New: std::mutex m_resultsMutex;
```

### Step 5.3: Replace model_trainer.h Thread
**Current code (line 303):**
```cpp
QThread* m_trainingThread = nullptr;
```

**Update model_trainer.cpp (lines 90-92):**
```cpp
// Old: new QThread(this); connect(...QThread::started, ...)
// New: m_trainingThread = std::make_unique<std::thread>(&ModelTrainer::runTraining, this);
```

---

## PART 6: SIGNAL/SLOT REPLACEMENT (Days 11-13)

### Step 6.1: Convert Signal Declarations to Callbacks

**Example pattern:**

**Old code (e.g., lsp_client.h line 133):**
```cpp
signals:
    void serverError(const QString& message);
    void serverReady();
    void completionsReceived(const std::string& uri, int line, int character, 
                            const QVector<CompletionItem>& items);
```

**New code:**
```cpp
class LSPClient {
public:
    // Callback types
    using ErrorCallback = std::function<void(const std::string&)>;
    using ReadyCallback = std::function<void()>;
    using CompletionsCallback = std::function<void(const std::string&, int, int, 
                                                   const std::vector<CompletionItem>&)>;
    
    // Signal registration
    void onServerError(ErrorCallback callback) { m_errorCallbacks.push_back(callback); }
    void onServerReady(ReadyCallback callback) { m_readyCallbacks.push_back(callback); }
    void onCompletionsReceived(CompletionsCallback cb) { m_completionCallbacks.push_back(cb); }
    
private:
    std::vector<ErrorCallback> m_errorCallbacks;
    std::vector<ReadyCallback> m_readyCallbacks;
    std::vector<CompletionsCallback> m_completionCallbacks;
};
```

### Step 6.2: Update emit() Statements to Callback Invocations

**Old pattern:**
```cpp
emit serverError("Error message");
```

**New pattern:**
```cpp
for(auto& cb : m_errorCallbacks) cb("Error message");
```

### Step 6.3: Update connect() Calls

**Old pattern (e.g., model_router_adapter.cpp line 365):**
```cpp
connect(m_generation_thread, &QThread::finished, this, &ModelRouterAdapter::onGenerationThreadFinished);
```

**New pattern:**
```cpp
m_generation_thread->onFinished([this]() { this->onGenerationThreadFinished(); });
```

### Step 6.4: Files Requiring Signal/Slot Conversion (in order)

**Priority 1 (Core system):**
1. lsp_client.h (6 signals)
2. model_router_adapter.h (10 signals)
3. model_trainer.h (8 signals)

**Priority 2 (Important subsystems):**
4. model_interface.h (3 signals)
5. model_registry.h (3 signals)
6. performance_monitor.h (4 signals)
7. plan_orchestrator.h (5 signals)

**Priority 3 (UI/secondary):**
8. model_router_widget.h (8 signals)
9. profiler.h (3 signals)
10. And 8+ other files

---

## PART 7: GUI CLEANUP (Day 14)

### Step 7.1: Review QtGUIStubs.hpp
**Current status:** Contains placeholder Qt GUI classes

**Options:**
1. Remove entirely if CLI-only
2. Replace with platform-specific file dialogs
3. Implement simple UI using a different framework

### Step 7.2: Clean Up GUI Files
**Files to review:**
- multi_tab_editor.h/cpp - Tab widget management
- model_router_widget.h/cpp - Model router UI
- training_dialog.h/cpp - Training configuration dialog
- todo_dock.h/cpp - Todo list UI

**Action:** Either reimplement with non-Qt framework or remove if moving to CLI

---

## PART 8: TESTING & VALIDATION (Days 15-18)

### Step 8.1: Compilation Testing
```bash
# Step 1: Update CMakeLists.txt to remove Qt dependencies
# Step 2: Try to compile
cmake -B build
cmake --build build 2>&1 | tee build.log
# Step 3: Fix compilation errors
```

### Step 8.2: Unit Testing
```bash
# Run existing unit tests
./build/tests/unit_tests
# Expected: All tests should pass
```

### Step 8.3: Integration Testing
```bash
# Run integration tests
./build/tests/integration_tests
# Verify all major features work without Qt
```

### Step 8.4: Functional Testing
- [ ] Model loading
- [ ] Text generation
- [ ] File I/O operations
- [ ] Logging output
- [ ] Threading operations
- [ ] Error handling

### Step 8.5: Performance Benchmarking
- Compare performance with Qt version
- Verify no performance degradation
- Check memory usage

---

## PART 9: CLEANUP & FINALIZATION (Day 19-20)

### Step 9.1: Remove Obsolete Files
```bash
# After verification, remove:
rm src/QtReplacements.hpp
rm src/QtReplacements_New.hpp
rm src/QtReplacements_Old.hpp
rm src/QtGUIStubs.hpp
```

### Step 9.2: Documentation Updates
- [ ] Update README.md - Remove Qt dependency mention
- [ ] Update BUILDING.md - Update build instructions
- [ ] Update CMakeLists.txt comments
- [ ] Document new callback system

### Step 9.3: Final Compilation
```bash
cmake -B build
cmake --build build
# Should succeed without any Qt-related errors or warnings
```

### Step 9.4: Verification Checklist
- [ ] Project compiles without Qt
- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] No Qt headers in include paths
- [ ] No Qt libraries linked
- [ ] No qDebug/qInfo/qWarning/qCritical remains
- [ ] No QVector/QHash/QMap remains
- [ ] No signal/slot declarations remain
- [ ] No QThread usage remains
- [ ] All logging uses new Logger system
- [ ] All callbacks work properly

---

## CRITICAL FILES TO ADDRESS FIRST

1. **security_manager.cpp** (100+ dependencies)
   - Highest complexity due to extensive logging
   - Extensive signal emissions
   - Should be addressed early to establish patterns

2. **model_router_adapter.cpp** (65+ dependencies)
   - Contains QThread usage
   - Many signal connections
   - Many logging calls

3. **model_trainer.cpp** (50+ dependencies)
   - Threading implementation
   - Extensive logging throughout
   - Signal/slot callbacks

---

## PARALLEL WORK OPPORTUNITIES

**Can be done in parallel:**
- Create logging infrastructure (Step 1.1) - 1 person
- Create callback system (Step 1.2) - 1 person
- Update CMakeLists.txt (Step 1.3) - 1 person

Then:
- Container type replacement (Step 2.1-2.5) - 1-2 persons
- Logging replacement in different files - Multiple persons

---

## ROLLBACK STRATEGY

**If issues arise:**
1. All changes should be git-committed frequently
2. Create branch for each major phase
3. Can revert specific commits if needed
4. Keep Qt dependencies in old branch for reference

```bash
git branch qt-removal
git checkout qt-removal
# Make changes in this branch
# If needed: git checkout main (to revert)
```

---

## ESTIMATED TIMELINE

| Phase | Duration | Start | End |
|-------|----------|-------|-----|
| 1. Infrastructure | 2 days | Day 1 | Day 2 |
| 2. Container Types | 2 days | Day 3 | Day 4 |
| 3. Logging System | 3 days | Day 5 | Day 7 |
| 4. File I/O | 1 day | Day 8 | Day 8 |
| 5. Threading | 2 days | Day 9 | Day 10 |
| 6. Signal/Slot | 3 days | Day 11 | Day 13 |
| 7. GUI Cleanup | 1 day | Day 14 | Day 14 |
| 8. Testing | 4 days | Day 15 | Day 18 |
| 9. Finalization | 2 days | Day 19 | Day 20 |
| **TOTAL** | **20 days** | | |

---

## SUCCESS CRITERIA

- [ ] Project compiles without Qt framework
- [ ] All 500+ Qt dependencies removed or replaced
- [ ] 100% of logging uses new Logger system
- [ ] 100% of signal/slot replaced with callbacks
- [ ] All threading uses std::thread
- [ ] All file I/O uses std::filesystem/ifstream/ofstream
- [ ] All container types use std:: equivalents
- [ ] All existing tests pass
- [ ] No compilation warnings related to Qt
- [ ] Project builds and runs successfully on Windows/Linux

---

**Report Generated:** January 30, 2026  
**Next Step:** Begin with Part 1 (Infrastructure Setup)
