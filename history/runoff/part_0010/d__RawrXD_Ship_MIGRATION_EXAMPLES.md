# Qt Migration Examples & Patterns

## Example 1: MainWindow.cpp Migration

### BEFORE (Qt Version)
```cpp
// src/qtapp/MainWindow.cpp
#include <QMainWindow>
#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QPushButton>
#include <QVBoxLayout>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow() {
        setWindowTitle("RawrXD IDE");
        setGeometry(100, 100, 1024, 768);
        createMenus();
        createToolBars();
        createStatusBar();
    }
    
private slots:
    void onNewFile() { 
        // Create new file logic
    }
    void onOpenFile() { 
        // Open file dialog
    }
    void onSaveFile() {
        // Save current file
    }
    
private:
    void createMenus() {
        QMenu* fileMenu = menuBar()->addMenu("&File");
        fileMenu->addAction("&New", this, &MainWindow::onNewFile);
        fileMenu->addAction("&Open", this, &MainWindow::onOpenFile);
        fileMenu->addAction("&Save", this, &MainWindow::onSaveFile);
        fileMenu->addSeparator();
        fileMenu->addAction("E&xit", this, &QApplication::quit);
    }
    
    void createToolBars() {
        QToolBar* fileToolbar = addToolBar("File");
        fileToolbar->addAction("New");
        fileToolbar->addAction("Open");
        fileToolbar->addAction("Save");
    }
    
    void createStatusBar() {
        statusBar()->showMessage("Ready");
    }
};
```

### AFTER (Win32 Version)
```cpp
// src/qtapp/MainWindow_Win32.cpp
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <functional>
#include <map>
#include <string>

#include "RawrXD_MainWindow_Win32.dll"
#pragma comment(lib, "RawrXD_MainWindow_Win32.lib")

// Use the Win32 replacement header
namespace RawrXD::Win32 {

class MainWindow {
public:
    // Callbacks instead of Qt slots
    std::function<void()> onNewFile;
    std::function<void()> onOpenFile;
    std::function<void()> onSaveFile;
    
    void Create(HINSTANCE hInstance, const wchar_t* title) {
        hInstance_ = hInstance;
        
        // Register window class
        WNDCLASSW wc = {};
        wc.lpfnWndProc = WndProc_;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"RawrXD_MainWindow";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassW(&wc);
        
        // Create main window
        hWnd_ = CreateWindowExW(
            0,
            L"RawrXD_MainWindow",
            title,
            WS_OVERLAPPEDWINDOW,
            100, 100, 1024, 768,
            nullptr, nullptr, hInstance, this
        );
        
        CreateMenus();
        CreateToolBars();
    }
    
    void Show(int nCmdShow = SW_SHOW) {
        ShowWindow(hWnd_, nCmdShow);
        UpdateWindow(hWnd_);
    }
    
    HWND GetHandle() const { return hWnd_; }
    
private:
    HWND hWnd_{nullptr};
    HINSTANCE hInstance_{nullptr};
    HMENU hMenuBar_{nullptr};
    HWND hToolbar_{nullptr};
    HWND hStatusBar_{nullptr};
    
    static LRESULT CALLBACK WndProc_(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        MainWindow* pThis = nullptr;
        
        if (msg == WM_CREATE) {
            CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        }
        
        if (pThis) {
            return pThis->WndProc(msg, wParam, lParam);
        }
        
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    
    LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_COMMAND:
                return OnCommand(wParam, lParam);
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProcW(hWnd_, msg, wParam, lParam);
        }
    }
    
    LRESULT OnCommand(WPARAM wParam, LPARAM lParam) {
        int id = LOWORD(wParam);
        
        switch (id) {
            case IDM_FILE_NEW:
                if (onNewFile) onNewFile();
                return 0;
            case IDM_FILE_OPEN:
                if (onOpenFile) onOpenFile();
                return 0;
            case IDM_FILE_SAVE:
                if (onSaveFile) onSaveFile();
                return 0;
            case IDM_FILE_EXIT:
                PostMessageW(hWnd_, WM_DESTROY, 0, 0);
                return 0;
        }
        return 0;
    }
    
    void CreateMenus() {
        hMenuBar_ = CreateMenuW();
        
        HMENU hFileMenu = CreateMenuW();
        AppendMenuW(hFileMenu, MFT_STRING, IDM_FILE_NEW, L"&New");
        AppendMenuW(hFileMenu, MFT_STRING, IDM_FILE_OPEN, L"&Open");
        AppendMenuW(hFileMenu, MFT_STRING, IDM_FILE_SAVE, L"&Save");
        AppendMenuW(hFileMenu, MFT_SEPARATOR, 0, nullptr);
        AppendMenuW(hFileMenu, MFT_STRING, IDM_FILE_EXIT, L"E&xit");
        
        AppendMenuW(hMenuBar_, MFT_POPUP, (UINT_PTR)hFileMenu, L"&File");
        SetMenuW(hWnd_, hMenuBar_);
    }
    
    void CreateToolBars() {
        // Win32 toolbar creation
        hToolbar_ = CreateWindowExW(
            0,
            TOOLBARCLASSNAMEW,
            nullptr,
            WS_CHILD | WS_VISIBLE | CCS_TOP,
            0, 0, 0, 0,
            hWnd_, (HMENU)1001, hInstance_, nullptr
        );
    }
};

} // namespace RawrXD::Win32

// Entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    RawrXD::Win32::MainWindow window;
    window.Create(hInstance, L"RawrXD IDE");
    
    // Set up callbacks instead of Qt connect()
    window.onNewFile = []() { 
        MessageBoxW(nullptr, L"New File", L"File", MB_OK); 
    };
    window.onOpenFile = []() {
        MessageBoxW(nullptr, L"Open File", L"File", MB_OK);
    };
    window.onSaveFile = []() {
        MessageBoxW(nullptr, L"Save File", L"File", MB_OK);
    };
    
    window.Show();
    
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    return (int)msg.wParam;
}
```

### Migration Checklist
```
☐ Step 1: Replace QMainWindow with Win32::MainWindow class
☐ Step 2: Replace Q_OBJECT macro - remove it entirely
☐ Step 3: Replace slots with std::function callbacks
☐ Step 4: Replace Qt connect() with direct callback assignment
☐ Step 5: Replace QMenu/QAction with CreateMenu/AppendMenu
☐ Step 6: Replace QToolBar with Win32 TOOLBARCLASSNAME
☐ Step 7: Add WndProc message handler
☐ Step 8: Replace main(int argc, char** argv) with wWinMain
☐ Step 9: Update CMakeLists.txt - remove Qt5::Widgets, add RawrXD_MainWindow_Win32.lib
☐ Step 10: Verify: dumpbin /dependents RawrXD_IDE.exe | findstr Qt (should be empty)
```

---

## Example 2: Agentic Executor Migration

### BEFORE (Qt Version)
```cpp
// src/agentic/agentic_executor.cpp
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <QString>
#include <QObject>
#include <QEventLoop>
#include <memory>

class AgenticExecutor : public QObject {
    Q_OBJECT
public:
    void executeCommand(const QString& cmd) {
        QProcess* process = new QProcess(this);
        
        // Qt signal/slot connection
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &AgenticExecutor::onProcessFinished);
        
        connect(process, &QProcess::readyReadStandardOutput,
                this, &AgenticExecutor::onReadyReadOutput);
        
        process->start(cmd);
    }
    
private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        emit executionCompleted(exitCode);
    }
    
    void onReadyReadOutput() {
        QProcess* process = qobject_cast<QProcess*>(sender());
        if (process) {
            emit outputAvailable(QString::fromUtf8(process->readAllStandardOutput()));
        }
    }
    
signals:
    void executionCompleted(int exitCode);
    void outputAvailable(const QString& output);
};
```

### AFTER (Win32 Version)
```cpp
// src/agentic/agentic_executor_win32.cpp
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <memory>

#include "RawrXD_Executor.dll"
#pragma comment(lib, "RawrXD_Executor.lib")

namespace RawrXD::Agentic {

struct ProcessResult {
    int exitCode{0};
    std::wstring output;
    std::wstring error;
    bool success{false};
};

class AgenticExecutor_Win32 {
public:
    // Callbacks instead of Qt signals
    std::function<void(const ProcessResult&)> onExecutionCompleted;
    std::function<void(const std::wstring&)> onOutputAvailable;
    
    void executeCommand(const std::wstring& cmd) {
        // Run in background thread to avoid blocking
        std::thread([this, cmd]() {
            ProcessResult result = RunProcess(cmd);
            
            if (onOutputAvailable) {
                onOutputAvailable(result.output);
            }
            
            if (onExecutionCompleted) {
                onExecutionCompleted(result);
            }
        }).detach();
    }
    
    ProcessResult RunProcess(const std::wstring& cmd) {
        ProcessResult result;
        
        HANDLE hReadPipe{nullptr}, hWritePipe{nullptr};
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        
        // Create pipe for stdout
        if (!CreatePipeW(&hReadPipe, &hWritePipe, &sa, 0)) {
            result.success = false;
            result.error = L"Failed to create pipe";
            return result;
        }
        
        // Start process
        STARTUPINFOW si{};
        si.cb = sizeof(STARTUPINFOW);
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        si.dwFlags = STARTF_USESTDHANDLES;
        
        PROCESS_INFORMATION pi{};
        
        wchar_t cmdCopy[1024]{};
        wcscpy_s(cmdCopy, cmd.c_str());
        
        if (!CreateProcessW(nullptr, cmdCopy, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
            result.success = false;
            result.error = L"Failed to create process";
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return result;
        }
        
        CloseHandle(hWritePipe);  // Close write end in parent
        
        // Read output
        const size_t bufSize = 4096;
        char buf[bufSize]{};
        DWORD bytesRead{0};
        
        while (ReadFileA(hReadPipe, buf, bufSize - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            
            // Convert to wstring
            int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, nullptr, 0);
            std::wstring wstr(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, buf, -1, wstr.data(), wlen);
            
            result.output += wstr;
        }
        
        // Wait for process to finish
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode{0};
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exitCode = exitCode;
        result.success = (exitCode == 0);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);
        
        return result;
    }
};

} // namespace RawrXD::Agentic
```

### Migration Checklist
```
☐ Step 1: Replace QProcess with CreateProcessW
☐ Step 2: Replace QThread with std::thread
☐ Step 3: Create PIPE for process communication instead of Qt signals
☐ Step 4: Replace Qt connect() with std::function callbacks
☐ Step 5: Replace QString with std::wstring
☐ Step 6: Replace Q_OBJECT macro - remove it
☐ Step 7: Remove QEventLoop usage - use WaitForSingleObject instead
☐ Step 8: Update CMakeLists.txt - remove Qt5::Core, add RawrXD_Executor.lib
☐ Step 9: Test process execution, output capture, error handling
☐ Step 10: Verify: grep "#include <Q" src/agentic/agentic_executor_win32.cpp (should be empty)
```

---

## General Migration Patterns

### Qt → Win32 Replacements

| Qt Component | Win32 Replacement | Notes |
|---|---|---|
| `QObject` | Plain C++ class | No base class needed |
| `Q_OBJECT` | Remove macro | Not needed for callbacks |
| `signals/slots` | `std::function` callbacks | No compile-time metaprogramming |
| `QString` | `std::wstring` | Native Unicode support |
| `QThread` | `std::thread` | C++11 standard threading |
| `QMutex` | `std::mutex` | Standard library mutex |
| `QMutexLocker` | `std::lock_guard` | RAII pattern |
| `QQueue<T>` | `std::queue<T>` | Standard library container |
| `QVector<T>` | `std::vector<T>` | Standard library container |
| `QMap<K,V>` | `std::map<K,V>` | Standard library container |
| `QProcess` | `CreateProcessW` + pipes | Win32 process management |
| `QFile` | `CreateFileW`, `ReadFile` | Win32 file I/O |
| `QDir` | `FindFirstFileW`, `FindNextFileW` | Win32 directory iteration |
| `QSettings` | `RegOpenKeyExW`, etc. | Windows Registry |
| `QTimer` | `SetTimer`, `WM_TIMER` | Win32 timer messages |
| `QMainWindow` | `CreateWindowExW` + `WndProc` | Win32 window management |
| `QPushButton` | `CreateWindowExW("BUTTON")` | Win32 button control |
| `QPlainTextEdit` | RichEdit (MSFTEDIT_CLASS) | Native text control |
| `QTcpServer` | `WinSock2` (WSASocket) | Win32 networking |
| `QApplication` | `WinMain` message loop | Win32 entry point |

### Code Structure Template

```cpp
// #include all Qt headers
// #include <Q...>
// class Foo : public QObject {
//     Q_OBJECT
// };

// BECOMES:

#include <windows.h>
#include <functional>
#include <string>
#include <thread>
#include <mutex>

namespace RawrXD::Component {

class Foo {  // No QObject inheritance
public:
    // Callbacks instead of signals
    std::function<void(const Result&)> onComplete;
    std::function<void(const Error&)> onError;
    
    void DoSomething() {
        // Use std::function directly
        if (onComplete) {
            onComplete(result);
        }
    }
    
private:
    std::mutex mtx_;  // Instead of QMutex
    // std::thread instead of QThread
    // std::wstring instead of QString
};

} // namespace
```

