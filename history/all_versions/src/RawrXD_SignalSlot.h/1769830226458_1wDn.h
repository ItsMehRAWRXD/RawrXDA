#pragma once
// RawrXD_SignalSlot.h
// Zero Qt dependencies - Fast C++17 signal/slot implementation

#include "RawrXD_Win32_Foundation.h"
#include <functional>
#include <vector>
#include <mutex>
#include <algorithm>
#include <thread>

namespace RawrXD {

// Lightweight signal/slot - replaces QObject::connect
template<typename... Args>
class Signal {
    using Slot = std::function<void(Args...)>;
    std::vector<Slot> slots;
    mutable std::mutex mtx;
    
public:
    Signal() = default;
    ~Signal() = default;
    
    // Non-copyable
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    
    // Movable
    Signal(Signal&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.mtx);
        slots = std::move(other.slots);
    }
    Signal& operator=(Signal&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock1(mtx);
            std::lock_guard<std::mutex> lock2(other.mtx);
            slots = std::move(other.slots);
        }
        return *this;
    }
    
    // Connect any callable
    template<typename F>
    void connect(F&& f) {
        std::lock_guard<std::mutex> lock(mtx);
        slots.emplace_back(std::forward<F>(f));
    }
    
    // Connect member function
    template<typename T, typename F>
    void connect(T* obj, F&& f) {
        connect([obj, f](Args... args) {
            (obj->*f)(args...);
        });
    }
    
    // Emit signal
    void emit(Args... args) {
        std::vector<Slot> copy;
        {
            std::lock_guard<std::mutex> lock(mtx);
            copy = slots;
        }
        for (auto& slot : copy) {
            if (slot) slot(args...);
        }
    }
    
    // Operator() for emit
    void operator()(Args... args) { emit(args...); }
    
    // Disconnect all
    void disconnect_all() {
        std::lock_guard<std::mutex> lock(mtx);
        slots.clear();
    }
    
    // Check if has connections
    bool isConnected() const {
        std::lock_guard<std::mutex> lock(mtx);
        return !slots.empty();
    }
    
    int connectionCount() const {
        std::lock_guard<std::mutex> lock(mtx);
        return (int)slots.size();
    }
};

// Connection guard - auto-disconnect on destruction
class Connection {
    std::function<void()> disconnector;
    bool connected = false;
    
public:
    Connection() = default;
    explicit Connection(std::function<void()> d) : disconnector(d), connected(true) {}
    
    Connection(Connection&& other) noexcept 
        : disconnector(std::move(other.disconnector)), connected(other.connected) {
        other.connected = false;
    }
    
    Connection& operator=(Connection&& other) noexcept {
        if (this != &other) {
            disconnect();
            disconnector = std::move(other.disconnector);
            connected = other.connected;
            other.connected = false;
        }
        return *this;
    }
    
    ~Connection() { disconnect(); }
    
    void disconnect() {
        if (connected && disconnector) {
            disconnector();
            connected = false;
        }
    }
    
    bool isConnected() const { return connected; }
};

// Signal with connection tracking
template<typename... Args>
class TrackableSignal {
    using Slot = std::function<void(Args...)>;
    struct SlotInfo {
        Slot func;
        std::weak_ptr<void> tracker;
    };
    std::vector<SlotInfo> slots;
    mutable std::mutex mtx;
    
public:
    template<typename F>
    std::shared_ptr<void> connect(F&& f) {
        auto tracker = std::make_shared<int>(0);
        std::lock_guard<std::mutex> lock(mtx);
        slots.push_back({std::forward<F>(f), tracker});
        return tracker;
    }
    
    void emit(Args... args) {
        std::vector<SlotInfo> copy;
        {
            std::lock_guard<std::mutex> lock(mtx);
            // Remove expired connections
            slots.erase(std::remove_if(slots.begin(), slots.end(),
                [](const SlotInfo& s) { return s.tracker.expired(); }), slots.end());
            copy = slots;
        }
        for (auto& slot : copy) {
            if (!slot.tracker.expired() && slot.func) {
                slot.func(args...);
            }
        }
    }
};

// Property with change notification
template<typename T>
class Property {
    T value;
    Signal<const T&> changed;
    mutable std::mutex mtx;
    
public:
    Property() = default;
    explicit Property(const T& v) : value(v) {}
    explicit Property(T&& v) : value(std::move(v)) {}
    
    Property(const Property&) = delete;
    Property& operator=(const Property&) = delete;
    Property(Property&&) = delete;
    Property& operator=(Property&&) = delete;
    
    const T& get() const {
        std::lock_guard<std::mutex> lock(mtx);
        return value;
    }
    
    void set(const T& v) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (value != v) {
                value = v;
                changed = true;
            }
        }
        if (changed) this->changed(v);
    }
    
    void set(T&& v) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (value != v) {
                value = std::move(v);
                changed = true;
            }
        }
        if (changed) this->changed(value);
    }
    
    Property& operator=(const T& v) { set(v); return *this; }
    Property& operator=(T&& v) { set(std::move(v)); return *this; }
    operator T() const { return get(); }
    
    Signal<const T&>& onChanged() { return changed; }
    
    // Connect to another property (binding)
    void bindTo(Property<T>& other) {
        other.onChanged().connect([this](const T& v) { set(v); });
        set(other.get());
    }
};

// Timer replacement (no QTimer)
class Timer {
    HWND hwnd = nullptr;
    UINT_PTR id = 0;
    UINT interval = 0;
    bool m_singleShot = false;
    bool active = false;
    std::function<void()> callback;
    static std::unordered_map<UINT_PTR, Timer*> timers;
    
    static void CALLBACK TimerProc(HWND, UINT, UINT_PTR id, DWORD);
    
public:
    Timer() = default;
    ~Timer();
    
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    
    void setInterval(int msec) { interval = (UINT)msec; }
    void setSingleShot(bool ss) { m_singleShot = ss; }
    
    void start();
    void stop();
    
    bool isActive() const { return active; }
    
    void onTimeout(std::function<void()> cb) { callback = cb; }
    Signal<> timeout;
    
    // Static single-shot
    static void singleShot(int msec, std::function<void()> cb);
};

// File system watcher (no QFileSystemWatcher)
class FileWatcher {
    HANDLE hDir = INVALID_HANDLE_VALUE;
    HANDLE hEvent = nullptr;
    OVERLAPPED overlapped;
    std::vector<uint8_t> buffer;
    String path;
    bool running = false;
    std::thread watchThread;
    Signal<const String&, int> fileChanged; // path, action
    
    void watchLoop();
    
public:
    enum Action { Added = 1, Removed = 2, Modified = 3, RenamedOld = 4, RenamedNew = 5 };
    
    FileWatcher();
    ~FileWatcher();
    
    bool addPath(const String& p);
    void removePath(const String& p);
    bool isWatching() const { return running; }
    
    Signal<const String&, int>& onFileChanged() { return fileChanged; }
};

} // namespace RawrXD

// Convenience macros
#define RAWRXD_CONNECT(sender, signal, receiver, slot) \
    (sender)->signal.connect(receiver, &slot)

#define RAWRXD_CONNECT_LAMBDA(sender, signal, lambda) \
    (sender)->signal.connect(lambda)
