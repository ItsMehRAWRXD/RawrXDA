// ============================================================================
// Alert System — Header (Phase 33 Quick-Win Port)
// Native Win32 notifications, resource monitoring, deduplication
// Replaces: Qt QSystemTrayIcon + QNotification-based alerts
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <chrono>

// ============================================================================
// Enums
// ============================================================================
enum class AlertPriority : int {
    Info     = 0,
    Warning  = 1,
    Error    = 2,
    Critical = 3
};

enum class AlertChannel : int {
    StatusBar   = 0,   // Bottom bar text
    TrayBalloon = 1,   // Shell_NotifyIcon balloon
    Dialog      = 2,   // MessageBox
    Log         = 3,   // File/console log only
    Toast       = 4    // Lightweight overlay
};

enum class ResourceType : int {
    CPU       = 0,
    Memory    = 1,
    Disk      = 2,
    GPU_VRAM  = 3,
    Network   = 4,
    TokenRate = 5
};

// ============================================================================
// Structs
// ============================================================================
struct AlertEvent {
    uint64_t       id;
    AlertPriority  priority;
    AlertChannel   channel;
    std::string    title;
    std::string    message;
    std::string    source;       // e.g. "FailureDetector", "SLOTracker"
    std::string    timestamp;
    bool           dismissed;
    int            duplicateCount;
};

struct ResourceThreshold {
    ResourceType  type;
    float         warningPct;     // e.g. 80.0
    float         criticalPct;    // e.g. 95.0
    bool          enabled;
};

struct ResourceStatus {
    ResourceType  type;
    float         currentPct;
    float         currentValue;   // absolute value (MB, %, etc.)
    float         maxValue;
    bool          inWarning;
    bool          inCritical;
};

struct AlertSystemConfig {
    bool   enableTrayIcon      = true;
    bool   enableResourceWatch = true;
    int    resourcePollMs      = 5000;    // 5 second poll
    int    deduplicateWindowMs = 30000;   // suppress duplicates within 30s
    int    maxAlertHistory     = 200;
    bool   logToFile           = true;
    std::string logPath;

    std::vector<ResourceThreshold> thresholds = {
        { ResourceType::CPU,      80.0f, 95.0f, true },
        { ResourceType::Memory,   85.0f, 95.0f, true },
        { ResourceType::Disk,     90.0f, 98.0f, true },
        { ResourceType::GPU_VRAM, 85.0f, 95.0f, false },
    };
};

// Callback: void(const AlertEvent*, void* userData)
using AlertCallback = void(*)(const AlertEvent*, void*);

// ============================================================================
// AlertSystem
// ============================================================================
class AlertSystem {
public:
    AlertSystem();
    ~AlertSystem();

    // No copy
    AlertSystem(const AlertSystem&) = delete;
    AlertSystem& operator=(const AlertSystem&) = delete;

    // Configuration
    void configure(const AlertSystemConfig& config);

    // ── Alert Emission ──────────────────────────────────────────────────
    uint64_t emit(AlertPriority priority, AlertChannel channel,
                  const char* title, const char* message,
                  const char* source = nullptr);

    uint64_t emitInfo(const char* title, const char* msg, const char* source = nullptr);
    uint64_t emitWarning(const char* title, const char* msg, const char* source = nullptr);
    uint64_t emitError(const char* title, const char* msg, const char* source = nullptr);
    uint64_t emitCritical(const char* title, const char* msg, const char* source = nullptr);

    // ── Alert Management ────────────────────────────────────────────────
    void dismiss(uint64_t alertId);
    void dismissAll();
    void clearHistory();

    // ── Query ───────────────────────────────────────────────────────────
    std::vector<AlertEvent> getAlertHistory() const;
    std::vector<AlertEvent> getActiveAlerts() const;
    size_t getActiveCount() const;

    // ── Resource Monitoring ─────────────────────────────────────────────
    void startResourceMonitor();
    void stopResourceMonitor();
    bool isMonitoring() const;
    ResourceStatus getResourceStatus(ResourceType type) const;
    std::vector<ResourceStatus> getAllResourceStatus() const;

    // ── Tray Icon ───────────────────────────────────────────────────────
    void initTrayIcon(void* hwnd);  // HWND
    void removeTrayIcon();
    void showBalloon(const char* title, const char* msg, AlertPriority priority);

    // ── Callbacks ───────────────────────────────────────────────────────
    void setAlertCallback(AlertCallback cb, void* userData);

private:
    AlertSystemConfig  m_config;
    mutable std::mutex m_configMutex;

    std::vector<AlertEvent>  m_history;
    mutable std::mutex       m_historyMutex;
    std::atomic<uint64_t>    m_nextId{1};

    // Resource monitoring thread
    std::thread       m_monitorThread;
    std::atomic<bool> m_monitorRunning{false};
    std::atomic<bool> m_monitorStopReq{false};
    std::vector<ResourceStatus> m_resourceCache;
    mutable std::mutex          m_resourceMutex;

    // Tray icon state
    bool   m_trayInitialized = false;
    void*  m_hwndOwner = nullptr;  // HWND

    // Callback
    AlertCallback m_alertCb     = nullptr;
    void*         m_alertCbData = nullptr;

    // Internal
    std::string getTimestamp() const;
    bool isDuplicate(const char* title, const char* message) const;
    void pruneHistory();
    void monitorLoop();
    void checkResource(ResourceType type);
    float queryCpuUsage() const;
    float queryMemoryUsage() const;
    float queryDiskUsage() const;
    void logAlert(const AlertEvent& evt) const;
    void dispatchAlert(const AlertEvent& evt);
};
