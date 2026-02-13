#include "RawrXD_SignalSlot.h"
#include <map>

namespace RawrXD {

// --- Timer Implementation ---

std::unordered_map<UINT_PTR, Timer*> Timer::timers;

void CALLBACK Timer::TimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    if (timers.find(id) != timers.end()) {
        Timer* timer = timers[id];
        if (timer->callback) {
            timer->callback();
        }
        timer->timeout.emit();
        
        if (timer->singleShot) {
            timer->stop();
        }
    }
}

Timer::~Timer() {
    stop();
}

void Timer::start() {
    if (active) stop();
    
    if (!hwnd) {
        // Create message-only window for timer if not already created or use a shared one?
        // Ideally we should use a shared message-only window or just the thread's message queue if we use SetTimer with NULL hwnd.
        // However, SetTimer with NULL hwnd requires a message loop to dispatch WM_TIMER, which we will have in Application::exec().
        // Using a window is safer for callback routing.
        
        static const wchar_t* className = L"RawrXD_TimerWindow";
        static bool classRegistered = false;
        
        if (!classRegistered) {
            WNDCLASSEXW wc = {};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = className;
            RegisterClassExW(&wc);
            classRegistered = true;
        }
        
        hwnd = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0, 
                               HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    }
    
    // id = SetTimer(hwnd, 0, interval, TimerProc); // If using HWND, TimerProc is optional if we handle WM_TIMER in WndProc
    // But we are using a DefWindowProc, so we must provide a TimerProc or handle it.
    // Actually, SetTimer with a callback and HWND works.
    
    // We need a unique ID for the timer if we use the same HWND for multiple timers? 
    // SetTimer returns a new ID if nIDEvent (2nd arg) is 0 and hwnd is NULL.
    // If hwnd is not NULL, we must manage IDs.
    
    // Simplest: use NULL HWND to get a unique ID, but then we depend on the thread message loop content.
    // The plan said "Replace QTimer with SetTimer/KillTimer + callback map".
    
    id = SetTimer(hwnd, 0, interval, TimerProc);
    if (id != 0) {
        timers[id] = this;
        active = true;
    }
}

void Timer::stop() {
    if (active && id != 0) {
        KillTimer(hwnd, id);
        timers.erase(id);
        active = false;
        id = 0;
        // Don't destroy HWND to keep it simple, or destroy it? 
        // If we created it per timer, we should destroy it.
        if (hwnd) {
            DestroyWindow(hwnd);
            hwnd = nullptr;
        }
    }
}

void Timer::singleShot(int msec, std::function<void()> cb) {
    // Self-deleting timer: capture pointer, invoke callback, then post deletion
    // via SetTimer with a 0ms cleanup timer to avoid deleting inside callback.
    
    auto* timer = new Timer();
    timer->setInterval(msec);
    timer->setSingleShot(true);
    timer->onTimeout([timer, cb]() {
        if (cb) cb();
        // Schedule deferred deletion — SetTimer(0ms) fires after callback returns
        // so we don't delete `this` from within the callback stack.
        SetTimer(nullptr, reinterpret_cast<UINT_PTR>(timer) | 0x80000000u, 0,
            [](HWND, UINT, UINT_PTR id, DWORD) {
                KillTimer(nullptr, id);
                Timer* t = reinterpret_cast<Timer*>(id & ~0x80000000u);
                delete t;
            });
    });
    timer->start();
}

// --- FileWatcher Implementation ---

FileWatcher::FileWatcher() {
    memset(&overlapped, 0, sizeof(overlapped));
    buffer.resize(4096);
}

FileWatcher::~FileWatcher() {
    removePath(path);
}

bool FileWatcher::addPath(const String& p) {
    if (p.empty()) return false;

    // Stop existing watch if any
    if (running) removePath(path);

    path = p;

    // Open directory handle for ReadDirectoryChangesW
    std::wstring wpath(path.begin(), path.end());
    hDir = CreateFileW(wpath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (hDir == INVALID_HANDLE_VALUE) return false;

    hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hEvent) {
        CloseHandle(hDir);
        hDir = INVALID_HANDLE_VALUE;
        return false;
    }

    running = true;
    watchThread = std::thread([this]() { watchLoop(); });

    return true;
}

void FileWatcher::removePath(const String& p) {
    running = false;

    // Signal the event to unblock WaitForSingleObject in the watch thread
    if (hEvent) SetEvent(hEvent);

    if (watchThread.joinable()) watchThread.join();

    if (hDir != INVALID_HANDLE_VALUE) {
        CancelIo(hDir);
        CloseHandle(hDir);
        hDir = INVALID_HANDLE_VALUE;
    }
    if (hEvent) {
        CloseHandle(hEvent);
        hEvent = nullptr;
    }
}

void FileWatcher::watchLoop() {
    while (running && hDir != INVALID_HANDLE_VALUE) {
        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = hEvent;

        DWORD notifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME |
                             FILE_NOTIFY_CHANGE_DIR_NAME |
                             FILE_NOTIFY_CHANGE_LAST_WRITE |
                             FILE_NOTIFY_CHANGE_SIZE |
                             FILE_NOTIFY_CHANGE_CREATION;

        BOOL ok = ReadDirectoryChangesW(
            hDir, buffer.data(), (DWORD)buffer.size(),
            TRUE, // Watch subtree
            notifyFilter,
            nullptr, &overlapped, nullptr);

        if (!ok) break;

        DWORD waitResult = WaitForSingleObject(hEvent, 500); // 500ms polling interval
        if (!running) break;

        if (waitResult == WAIT_OBJECT_0) {
            DWORD bytesTransferred = 0;
            if (!GetOverlappedResult(hDir, &overlapped, &bytesTransferred, FALSE) ||
                bytesTransferred == 0) {
                ResetEvent(hEvent);
                continue;
            }

            // Parse FILE_NOTIFY_INFORMATION entries
            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
            while (info) {
                // Convert wide filename to std::string
                std::wstring wname(info->FileName, info->FileNameLength / sizeof(WCHAR));
                std::string name(wname.begin(), wname.end());

                int action = 0;
                switch (info->Action) {
                    case FILE_ACTION_ADDED:            action = Action::Added; break;
                    case FILE_ACTION_REMOVED:          action = Action::Removed; break;
                    case FILE_ACTION_MODIFIED:         action = Action::Modified; break;
                    case FILE_ACTION_RENAMED_OLD_NAME: action = Action::RenamedOld; break;
                    case FILE_ACTION_RENAMED_NEW_NAME: action = Action::RenamedNew; break;
                }

                fileChanged.emit(String(name), action);

                if (info->NextEntryOffset == 0) break;
                info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<uint8_t*>(info) + info->NextEntryOffset);
            }

            ResetEvent(hEvent);
        }
    }
}

} // namespace RawrXD
