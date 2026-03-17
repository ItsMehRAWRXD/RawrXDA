#pragma once

#include <functional>
#include <cstdint>

class NativeTimer {
public:
    NativeTimer();
    ~NativeTimer();
    
    // One-shot timer
    void start(int milliseconds);
    
    // Periodic timer (fires repeatedly)
    void startPeriodic(int milliseconds);
    
    void stop();
    bool isRunning() const;
    void setTimeoutCallback(std::function<void()> callback);
    
private:
    std::function<void()> m_callback;
    uintptr_t m_timerId;
    bool m_running;
};