#include "Win32IDE.h"
#include "IocpFileWatcher.h"
#include <windows.h>

// Handler for IOCP File Watcher feature
void HandleIOCPFileWatcher(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) return;

    // Initialize IOCP file watcher if not already done
    static IocpFileWatcher* watcher = nullptr;
    if (!watcher) {
        watcher = new IocpFileWatcher();
        wchar_t cwd[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, cwd);
        if (!watcher->Start(cwd)) {
            MessageBoxA(NULL, "Failed to initialize IOCP File Watcher", "IOCP File Watcher", MB_ICONERROR | MB_OK);
            delete watcher;
            watcher = nullptr;
            return;
        }
    }

    // Show watcher status
    std::string status = "IOCP File Watcher Active\n\n";
    status += "High-performance file monitoring:\n";
    status += "- Asynchronous I/O completion ports\n";
    status += "- Real-time file change detection\n";
    status += "- Directory tree monitoring\n";
    status += "- Change notification queuing\n";
    status += "- Low-latency event processing\n";

    MessageBoxA(NULL, status.c_str(), "IOCP File Watcher", MB_ICONINFORMATION | MB_OK);
}
