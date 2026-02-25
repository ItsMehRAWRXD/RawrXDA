#include "RawrXD_SignalSlot.h"
#include <map>
#include <vector>
#include <string>

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
    
    id = SetTimer(NULL, 0, interval, TimerProc);
    if (id != 0) {
        timers[id] = this;
        active = true;
    }
}

void Timer::stop() {
    if (active && id != 0) {
        KillTimer(NULL, id);
        timers.erase(id);
        active = false;
        id = 0;
        if (hwnd) {
            DestroyWindow(hwnd);
            hwnd = nullptr;
        }
    }
}

void Timer::singleShot(int msec, std::function<void()> cb) {
    std::thread([msec, cb]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(msec));
        if (cb) cb();
    }).detach();
}

// --- FileWatcher Implementation ---

FileWatcher::FileWatcher() : m_running(false), m_hDir(INVALID_HANDLE_VALUE) {
    memset(&m_overlapped, 0, sizeof(m_overlapped));
}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::start(const String& path) {
    if (m_running) stop();

    watchPath = path;
    // Open directory for monitoring. 
    // basic blocking mode (no OVERLAPPED) for simplicity in a dedicated thread.
    m_hDir = CreateFileW(
        path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, // Required for directories
        NULL
    );

    if (m_hDir == INVALID_HANDLE_VALUE) {
        return;
    }

    m_running = true;
    watcherThread = std::thread(&FileWatcher::watchLoop, this);
}

void FileWatcher::stop() {
    if (!m_running) return;
    m_running = false;
    
    if (m_hDir != INVALID_HANDLE_VALUE) {
        // Closing handle should cause ReadDirectoryChangesW to fail and exit loop
        CancelIo(m_hDir);
        CloseHandle(m_hDir);
        m_hDir = INVALID_HANDLE_VALUE;
    }
    
    if (watcherThread.joinable()) {
        watcherThread.join();
    }
}

void FileWatcher::watchLoop() {
    // 16KB buffer for changes
    std::vector<uint8_t> changeBuf(16 * 1024);
    DWORD bytesReturned;
    
    while(m_running) {
        if(ReadDirectoryChangesW(
            m_hDir,
            changeBuf.data(),
            (DWORD)changeBuf.size(),
            TRUE, // Watch subtree
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
            &bytesReturned,
            NULL,
            NULL
        )) {
            if (bytesReturned > 0) {
                 directoryChanged.emit(watchPath);
                 // We could parse FILE_NOTIFY_INFORMATION here if needed for fileChanged
            }
        } else {
             // Error or handle closed
             break;
        }
    }
}

} // namespace RawrXD
