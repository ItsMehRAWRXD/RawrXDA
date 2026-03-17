#pragma once

#include <memory>
#include <string>
#include <utility>
#include <chrono>
#include <functional>
#include <atomic>

#include "agentic_agent_coordinator.h"

namespace RawrXD::IDE {

enum class AgenticError {
    None,
    BootstrapFailed,
    CoordinatorNotReady,
    InvalidState,
    CommunicationError
};

struct AgenticResult {
    bool success;
    AgenticError error;
    std::string message;

    static AgenticResult Ok(std::string msg = {}) {
        return {true, AgenticError::None, std::move(msg)};
    }

    static AgenticResult Fail(AgenticError err, std::string msg) {
        return {false, err, std::move(msg)};
    }
};

// Elapsed timer replacement for QElapsedTimer  
struct ElapsedTimer {
    std::chrono::steady_clock::time_point start_{};
    bool valid_ = false;

    void restart() { start_ = std::chrono::steady_clock::now(); valid_ = true; }
    bool isValid() const { return valid_; }
    int64_t elapsed() const {
        if (!valid_) return 0;
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_).count();
    }
};

// Simple repeating timer replacement for QTimer (poll-based)
struct HeartbeatTimer {
    int intervalMs = 0;
    bool running = false;
    std::chrono::steady_clock::time_point lastFire_{};

    void start(int ms) { intervalMs = ms; running = true; lastFire_ = std::chrono::steady_clock::now(); }
    void stop() { running = false; }
    bool shouldFire() {
        if (!running || intervalMs <= 0) return false;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFire_).count();
        if (elapsed >= intervalMs) { lastFire_ = now; return true; }
        return false;
    }
};

class AgenticController {
public:
    explicit AgenticController();
    ~AgenticController();

    AgenticResult bootstrap();

    // Callback hooks (replacing Qt signals)
    std::function<void()> onControllerReady = nullptr;
    std::function<void(const std::string&)> onControllerError = nullptr;
    std::function<void(const std::string&)> onLayoutHydrationRequested = nullptr;
    std::function<void(const std::string&)> onTelemetryHeartbeat = nullptr;

    // Public methods (replacing Qt slots)
    void handleLayoutRestored(const std::string& snapshotId);
    void handleWindowActivated();

    // Call periodically from main loop to fire heartbeat if due
    void tick();

private:
    AgenticResult ensureCoordinator();
    std::string resolveSnapshotPreference() const;
    void publishHeartbeat();

    std::unique_ptr<AgenticAgentCoordinator> m_coordinator;
    ElapsedTimer m_bootTimer;
    HeartbeatTimer m_heartbeatTimer;
};

} // namespace RawrXD::IDE
