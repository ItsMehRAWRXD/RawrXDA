# Qt Migration Command Center - Quick Reference

## 🚀 Quick Start (5 minutes)

### Step 1: Run Scanner (2 min)
```powershell
cd D:\RawrXD\Ship
.\Scan-QtDependencies.ps1 > qt_report.txt
Get-Content qt_report.txt | head -50
```

**What it does**: Scans all 1,186 source files, finds 280+ with Qt deps, ranks by priority

**Output**: Categorized list with priority scores (10=critical, 3=low)

---

### Step 2: Review Top Priority (1 min)
```powershell
Get-Content qt_report.txt | Select-String "CRITICAL|HIGH"
```

**Expected output**:
```
[🔴 CRITICAL] src/qtapp/MainWindow.cpp - 18 Qt refs, 39 lines
[🔴 CRITICAL] src/qtapp/main_qt.cpp - 5 Qt refs, 20 lines
[🔴 CRITICAL] src/qtapp/TerminalWidget.cpp - 12 Qt refs, 80 lines
```

---

### Step 3: Start First Migration (2 min)
```powershell
# Open MainWindow.cpp
code D:\RawrXD\src\qtapp\MainWindow.cpp

# Reference the migration example
code D:\RawrXD\Ship\MIGRATION_EXAMPLES.md

# Use template from Win32_MainWindow.hpp
code D:\RawrXD\Ship\Win32_MainWindow.hpp
```

**Start rewriting** using MIGRATION_EXAMPLES.md as guide

---

## 📊 Command Reference

### Verify Progress
```powershell
# Full verification (4 checks)
.\Verify-QtRemoval.ps1

# Quick check: count remaining Qt files
Get-ChildItem D:\RawrXD\src -Recurse -Include *.cpp,*.hpp,*.h | 
  Select-String "#include\s*<Q" | Measure-Object

# Check specific file
Get-Content D:\RawrXD\src\path\file.cpp | Select-String "#include\s*<Q"
```

### Migration Tracking
```cpp
// In C++ code - use migration tracker
#include "QtMigrationTracker.hpp"

auto& tracker = RawrXD::Migration::MigrationTracker::Instance();
tracker.Initialize();
tracker.StartTask(L"src/qtapp/MainWindow.cpp");
tracker.CompleteStep(L"src/qtapp/MainWindow.cpp", 4); // Mark done
tracker.PrintProgressReport();
```

### Build & Test
```powershell
# Full build
cd D:\RawrXD
cmake --build . --config Release

# Quick compile check (single file)
cl /std:c++20 /EHsc /DNOMINMAX /I. /c src/qtapp/MainWindow_Win32.cpp

# Check binary dependencies
dumpbin /dependents D:\RawrXD\Ship\RawrXD_IDE.exe | findstr Qt
# Should be empty
```

---

## 🔄 Translation Patterns (Copy-Paste Reference)

### Pattern 1: Remove Q_OBJECT Class
```cpp
// BEFORE
class MainWindow : public QMainWindow {
    Q_OBJECT
    Q_SIGNALS:
        void somethingHappened();
    Q_SLOTS:
        void onSomething();
};

// AFTER
class MainWindow {  // No inheritance, no Q_OBJECT
    std::function<void()> somethingHappened;
    void onSomething() { }
};
```

### Pattern 2: Replace Qt Connects
```cpp
// BEFORE
connect(button, &QPushButton::clicked, this, &MainWindow::onClicked);

// AFTER
// In Win32 message handler:
case WM_COMMAND:
    if (onClicked) onClicked();
    break;

// OR use callback directly
button.onClick = [this]() { onClicked(); };
```

### Pattern 3: Replace QString
```cpp
// BEFORE
QString name = "Hello";
QString path = QDir::currentPath() + "/file.txt";
const char* str = name.toUtf8().constData();

// AFTER
std::wstring name = L"Hello";
std::wstring path = GetCurrentDirectoryW();  // Win32 API
path += L"/file.txt";
const wchar_t* str = name.c_str();
```

### Pattern 4: Replace QProcess
```cpp
// BEFORE
QProcess process;
connect(&process, QOverload<int>::of(&QProcess::finished), 
        this, &Executor::onFinished);
process.start("cmd.exe");
process.waitForFinished();

// AFTER
PROCESS_INFORMATION pi;
STARTUPINFOW si = {sizeof(si)};
CreateProcessW(L"cmd.exe", nullptr, nullptr, nullptr, 
               FALSE, 0, nullptr, nullptr, &si, &pi);
WaitForSingleObject(pi.hProcess, INFINITE);
CloseHandle(pi.hProcess);
CloseHandle(pi.hThread);
```

### Pattern 5: Replace QThread
```cpp
// BEFORE
class Worker : public QObject {
    Q_OBJECT
    void run() { /* work */ }
};
QThread thread;
Worker* worker = new Worker();
worker->moveToThread(&thread);
thread.start();

// AFTER
class Worker {
    void run() { /* work */ }
};
Worker worker;
std::thread t([&worker]() { worker.run(); });
t.detach();
```

### Pattern 6: Replace QMutex
```cpp
// BEFORE
QMutex mutex;
QMutexLocker lock(&mutex);
sharedData = newValue;

// AFTER
std::mutex mutex;
{
    std::lock_guard<std::mutex> lock(mutex);
    sharedData = newValue;
}
```

### Pattern 7: Replace QFile
```cpp
// BEFORE
QFile file("data.txt");
if (file.open(QIODevice::ReadOnly)) {
    QByteArray data = file.readAll();
    file.close();
}

// AFTER
HANDLE hFile = CreateFileW(L"data.txt", GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
if (hFile != INVALID_HANDLE_VALUE) {
    DWORD fileSize = GetFileSize(hFile, nullptr);
    std::vector<char> data(fileSize);
    DWORD bytesRead;
    ReadFile(hFile, data.data(), fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);
}
```

### Pattern 8: Replace QSettings
```cpp
// BEFORE
QSettings settings("RawrXD", "IDE");
QString value = settings.value("key", "default").toString();
settings.setValue("key", newValue);

// AFTER
HKEY hKey;
RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\RawrXD\\IDE", 0, KEY_READ, &hKey);
wchar_t value[256] = L"default";
DWORD size = sizeof(value);
RegQueryValueExW(hKey, L"key", nullptr, nullptr, (LPBYTE)value, &size);
RegCloseKey(hKey);

// For writing:
RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\RawrXD\\IDE", 0, KEY_WRITE, &hKey);
RegSetValueExW(hKey, L"key", 0, REG_SZ, (LPBYTE)newValue, (wcslen(newValue)+1)*sizeof(wchar_t));
RegCloseKey(hKey);
```

---

## 📋 Checklist per File

Use this for each file you migrate:

```
[ ] File: ___________________________

[ ] Analysis Phase
    [ ] Identify all #include <Q*>
    [ ] Identify all Q_OBJECT uses
    [ ] Identify all slots/signals
    [ ] Identify all Qt container usage (QVector, QMap, etc.)
    [ ] Identify all threading patterns (QThread, QMutex)
    [ ] Identify all file I/O (QFile, QDir)
    [ ] Identify all settings usage (QSettings)

[ ] Implementation Phase
    [ ] Create Win32 replacement class
    [ ] Replace inheritance (remove : public QObject)
    [ ] Replace Q_OBJECT with callbacks
    [ ] Replace signals/slots with std::function
    [ ] Replace #include <Q*> with #include <windows.h>, <functional>, etc.
    [ ] Replace QString with std::wstring
    [ ] Replace QThread with std::thread
    [ ] Replace QMutex with std::mutex
    [ ] Replace QFile/QDir with Win32 API
    [ ] Replace QSettings with Registry API
    [ ] Replace Qt containers with STL containers

[ ] Build Phase
    [ ] Add to CMakeLists.txt
    [ ] Link correct Win32 libs (kernel32, user32, etc.)
    [ ] Compile: zero errors
    [ ] Warnings acceptable (unreferenced params OK)

[ ] Verification Phase
    [ ] dumpbin /dependents [binary] | findstr Qt (should be empty)
    [ ] Check file: grep "#include <Q" file.cpp (should be empty)
    [ ] Check file: grep "Q_OBJECT" file.cpp (should be empty)
    [ ] Integration test: component works with Foundation
    [ ] Update QtMigrationTracker.hpp status

[ ] Sign-Off
    [ ] Commit: git add <file>, git commit -m "Migrate <file> to Win32"
    [ ] Update Migration Tracker
    [ ] Update this checklist
```

---

## 🎯 Priority Decision Tree

**When deciding which file to migrate next:**

1. **Is IDE not launching?** → Migrate Week 1 files first
   - MainWindow.cpp (Priority 10)
   - main_qt.cpp (Priority 10)
   - TerminalWidget.cpp (Priority 9)

2. **Can I run the IDE?** → Pick highest priority remaining
   - Check: `Get-Content qt_report.txt | Select-String "\[🔴 CRITICAL\]"`
   - Migrate agentic files (Priority 9)
   - Then orchestration files (Priority 8)

3. **Are agentic features working?** → Migrate utilities (Priority 5-6)
   - Batch process utils/* files
   - Most are 1-2 hour jobs
   - High volume, low risk

4. **Nearly done?** → Finish remaining files + tests
   - Check remaining with scanner
   - Complete by Friday EOW

---

## 🔍 Debugging Common Issues

### Issue: "unresolved external symbol Q..."
**Cause**: Still linking against Qt libraries
**Fix**: 
```cmake
# In CMakeLists.txt
# ❌ WRONG: target_link_libraries(mylib Qt5::Core)
# ✅ RIGHT: target_link_libraries(mylib RawrXD_Foundation.lib kernel32.lib)
```

### Issue: "error: undefined reference to 'QApplication'"
**Cause**: File still has Qt #include but no Qt lib linked
**Fix**: 
```cpp
// Replace this entire section
#include <QApplication>
QApplication app(argc, argv);

// With this
#include <windows.h>
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR cmdLine, int nCmdShow) {
    // Win32 initialization
}
```

### Issue: "error: 'QString' does not name a type"
**Cause**: Removed #include <QString> but code still uses QString
**Fix**: Replace all QString with std::wstring
```cpp
// ❌ WRONG
QString text = "Hello";

// ✅ RIGHT
std::wstring text = L"Hello";
```

### Issue: Compile succeeds but dumpbin shows Qt5Core.dll dependency
**Cause**: Some object file still contains Qt symbol references
**Fix**: 
1. Check all .cpp files for remaining Qt #includes: `grep "#include <Q" *.cpp`
2. Recompile individual files: `cl /c /std:c++20 file.cpp`
3. Check .obj files: `dumpbin /symbols file.obj | findstr Qt`

### Issue: CreateWindowEx fails with "not declared in scope"
**Cause**: Missing #include <windows.h> or wrong header order
**Fix**: 
```cpp
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>  // For common controls
```

---

## 📈 Progress Milestones

Track these for morale & scheduling:

| Milestone | Target | Verification |
|---|---|---|
| **IDE Launches** | End Week 1 | `./RawrXD_IDE.exe` works |
| **50% Files Done** | Mid Week 2 | `Scan-QtDependencies.ps1` shows 140/280 |
| **Agentic Features** | End Week 2 | Plan execution works |
| **80% Files Done** | Wed Week 3 | `Scan-QtDependencies.ps1` shows 224/280 |
| **100% Files Done** | Thu Week 4 | `Get-Content qt_report.txt | findstr "CRITICAL\|HIGH"` = empty |
| **Verification Pass** | Fri Week 4 | `Verify-QtRemoval.ps1` = 4/4 PASS |

---

## 💡 Pro Tips

1. **Use templates**: Copy Win32_*.hpp headers and adapt them
2. **Batch similar files**: If you do one Qt container replacement, you've done them all
3. **Test often**: Build after every 2-3 files, don't let issues pile up
4. **Parallel work**: Multiple devs can work on different components simultaneously
5. **Save intermediate**: Commit to git after each file for easy rollback
6. **Check performance**: Monitor binary size - Win32 versions often smaller
7. **Use dumpbin religiously**: It's your friend - check after every major change

---

## 📞 Support

### If you get stuck:
1. Check MIGRATION_EXAMPLES.md for your pattern
2. Reference Win32_*.hpp headers for working code
3. Check CMAKE_MIGRATION_GUIDE.md for build issues
4. Run Verify-QtRemoval.ps1 to identify specific problems
5. Check git log for similar migrations

### Key Files to Keep Open:
1. MIGRATION_EXAMPLES.md - Patterns & templates
2. Win32_MainWindow.hpp - Reference implementation
3. QtMigrationTracker.hpp - Status tracking
4. EXECUTION_ROADMAP_4WEEK.md - Timeline & milestones
5. Verify-QtRemoval.ps1 - Verification

---

## ✅ Final Checklist

Before marking Week/Project as COMPLETE:

- [ ] All dumpbin /dependents show zero Qt.dll
- [ ] All source files show zero #include <Q
- [ ] All source files show zero Q_OBJECT
- [ ] Full build succeeds: zero errors, acceptable warnings only
- [ ] IDE launches and runs
- [ ] All features work (agentic, terminal, files, etc.)
- [ ] Verify-QtRemoval.ps1 returns 4/4 PASS
- [ ] Documentation updated
- [ ] Team trained on Win32 patterns
- [ ] Ready to ship 🚀

