// ============================================================================
// Alert System — Implementation (Phase 33 Quick-Win Port)
// Shell_NotifyIcon, PDH counters, resource polling, deduplication
// ============================================================================

#include "alert_system.hpp"
#include <cstdio>
#include <ctime>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "psapi.lib")
#endif

// ============================================================================
// Tray icon constants
// ============================================================================
#define WM_TRAYICON      (WM_APP + 0x200)
#define TRAY_ICON_UID    42

// ============================================================================
// AlertSystem
// ============================================================================
AlertSystem::AlertSystem()
{
}

AlertSystem::~AlertSystem()
{
    stopResourceMonitor();
    removeTrayIcon();
}

void AlertSystem::configure(const AlertSystemConfig& config)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
}

// ============================================================================
// Timestamp
// ============================================================================
std::string AlertSystem::getTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm tb;
#ifdef _WIN32
    localtime_s(&tb, &t);
#else
    localtime_r(&t, &tb);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tb);
    return buf;
}

// ============================================================================
// Deduplication
// ============================================================================
bool AlertSystem::isDuplicate(const char* title, const char* message) const
{
    std::lock_guard<std::mutex> lock(m_historyMutex);

    auto now = std::chrono::steady_clock::now();
    int windowMs;
    {
        std::lock_guard<std::mutex> cl(m_configMutex);
        windowMs = m_config.deduplicateWindowMs;
    }

    // Search recent history in reverse
    for (auto it = m_history.rbegin(); it != m_history.rend(); ++it) {
        // Only check recent entries (approximate by checking last N)
        if (std::distance(m_history.rbegin(), it) > 20) break;

        if (it->title == title && it->message == message && !it->dismissed) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Alert Emission
// ============================================================================
uint64_t AlertSystem::emit(AlertPriority priority, AlertChannel channel,
                           const char* title, const char* message,
                           const char* source)
{
    if (isDuplicate(title, message)) {
        // Increment duplicate count on existing alert
        std::lock_guard<std::mutex> lock(m_historyMutex);
        for (auto it = m_history.rbegin(); it != m_history.rend(); ++it) {
            if (it->title == title && it->message == message && !it->dismissed) {
                it->duplicateCount++;
                return it->id;
            }
        }
    }

    AlertEvent evt;
    evt.id = m_nextId.fetch_add(1);
    evt.priority = priority;
    evt.channel = channel;
    evt.title = title ? title : "";
    evt.message = message ? message : "";
    evt.source = source ? source : "";
    evt.timestamp = getTimestamp();
    evt.dismissed = false;
    evt.duplicateCount = 1;

    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        m_history.push_back(evt);
    }

    pruneHistory();
    dispatchAlert(evt);
    logAlert(evt);

    return evt.id;
}

uint64_t AlertSystem::emitInfo(const char* title, const char* msg, const char* source)
{
    return emit(AlertPriority::Info, AlertChannel::StatusBar, title, msg, source);
}

uint64_t AlertSystem::emitWarning(const char* title, const char* msg, const char* source)
{
    return emit(AlertPriority::Warning, AlertChannel::TrayBalloon, title, msg, source);
}

uint64_t AlertSystem::emitError(const char* title, const char* msg, const char* source)
{
    return emit(AlertPriority::Error, AlertChannel::TrayBalloon, title, msg, source);
}

uint64_t AlertSystem::emitCritical(const char* title, const char* msg, const char* source)
{
    return emit(AlertPriority::Critical, AlertChannel::Dialog, title, msg, source);
}

// ============================================================================
// Dispatch to appropriate channel
// ============================================================================
void AlertSystem::dispatchAlert(const AlertEvent& evt)
{
    // Callback first
    if (m_alertCb) {
        m_alertCb(&evt, m_alertCbData);
    }

    // Channel-specific dispatch
    switch (evt.channel) {
        case AlertChannel::TrayBalloon:
            if (m_trayInitialized) {
                showBalloon(evt.title.c_str(), evt.message.c_str(), evt.priority);
            }
            break;

        case AlertChannel::Dialog:
#ifdef _WIN32
        {
            UINT flags = MB_OK;
            switch (evt.priority) {
                case AlertPriority::Info:     flags |= MB_ICONINFORMATION; break;
                case AlertPriority::Warning:  flags |= MB_ICONWARNING;     break;
                case AlertPriority::Error:    flags |= MB_ICONERROR;       break;
                case AlertPriority::Critical: flags |= MB_ICONERROR;       break;
            }
            MessageBoxA(static_cast<HWND>(m_hwndOwner),
                        evt.message.c_str(), evt.title.c_str(), flags);
        }
#endif
            break;

        case AlertChannel::StatusBar:
        case AlertChannel::Toast:
        case AlertChannel::Log:
            // Handled by callback or log — no direct OS dispatch
            break;
    }
}

// ============================================================================
// Alert Management
// ============================================================================
void AlertSystem::dismiss(uint64_t alertId)
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    for (auto& a : m_history) {
        if (a.id == alertId) {
            a.dismissed = true;
            return;
        }
    }
}

void AlertSystem::dismissAll()
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    for (auto& a : m_history) {
        a.dismissed = true;
    }
}

void AlertSystem::clearHistory()
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    m_history.clear();
}

// ============================================================================
// Query
// ============================================================================
std::vector<AlertEvent> AlertSystem::getAlertHistory() const
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    return m_history;
}

std::vector<AlertEvent> AlertSystem::getActiveAlerts() const
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    std::vector<AlertEvent> active;
    for (const auto& a : m_history) {
        if (!a.dismissed) active.push_back(a);
    }
    return active;
}

size_t AlertSystem::getActiveCount() const
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    size_t n = 0;
    for (const auto& a : m_history) {
        if (!a.dismissed) n++;
    }
    return n;
}

void AlertSystem::pruneHistory()
{
    int maxHistory;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        maxHistory = m_config.maxAlertHistory;
    }

    std::lock_guard<std::mutex> lock(m_historyMutex);
    while (static_cast<int>(m_history.size()) > maxHistory) {
        m_history.erase(m_history.begin());
    }
}

// ============================================================================
// Tray Icon
// ============================================================================
void AlertSystem::initTrayIcon(void* hwnd)
{
#ifdef _WIN32
    m_hwndOwner = hwnd;

    NOTIFYICONDATAA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = static_cast<HWND>(hwnd);
    nid.uID = TRAY_ICON_UID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    strncpy_s(nid.szTip, "RawrXD Engine", _TRUNCATE);

    Shell_NotifyIconA(NIM_ADD, &nid);
    m_trayInitialized = true;
#else
    (void)hwnd;
#endif
}

void AlertSystem::removeTrayIcon()
{
#ifdef _WIN32
    if (!m_trayInitialized) return;

    NOTIFYICONDATAA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = static_cast<HWND>(m_hwndOwner);
    nid.uID = TRAY_ICON_UID;
    Shell_NotifyIconA(NIM_DELETE, &nid);
    m_trayInitialized = false;
#endif
}

void AlertSystem::showBalloon(const char* title, const char* msg, AlertPriority priority)
{
#ifdef _WIN32
    if (!m_trayInitialized) return;

    NOTIFYICONDATAA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = static_cast<HWND>(m_hwndOwner);
    nid.uID = TRAY_ICON_UID;
    nid.uFlags = NIF_INFO;

    strncpy_s(nid.szInfoTitle, title, _TRUNCATE);
    strncpy_s(nid.szInfo, msg, _TRUNCATE);

    switch (priority) {
        case AlertPriority::Info:     nid.dwInfoFlags = NIIF_INFO;    break;
        case AlertPriority::Warning:  nid.dwInfoFlags = NIIF_WARNING; break;
        case AlertPriority::Error:
        case AlertPriority::Critical: nid.dwInfoFlags = NIIF_ERROR;   break;
    }

    Shell_NotifyIconA(NIM_MODIFY, &nid);
#else
    (void)title; (void)msg; (void)priority;
#endif
}

// ============================================================================
// Resource Monitoring
// ============================================================================
void AlertSystem::startResourceMonitor()
{
    if (m_monitorRunning.load()) return;

    m_monitorStopReq.store(false);
    m_monitorRunning.store(true);
    m_monitorThread = std::thread(&AlertSystem::monitorLoop, this);
}

void AlertSystem::stopResourceMonitor()
{
    if (!m_monitorRunning.load()) return;

    m_monitorStopReq.store(true);
    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }
    m_monitorRunning.store(false);
}

bool AlertSystem::isMonitoring() const
{
    return m_monitorRunning.load();
}

ResourceStatus AlertSystem::getResourceStatus(ResourceType type) const
{
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    for (const auto& rs : m_resourceCache) {
        if (rs.type == type) return rs;
    }
    return {};
}

std::vector<ResourceStatus> AlertSystem::getAllResourceStatus() const
{
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    return m_resourceCache;
}

void AlertSystem::monitorLoop()
{
    int pollMs;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        pollMs = m_config.resourcePollMs;
    }

    while (!m_monitorStopReq.load()) {
        // Check each enabled threshold
        AlertSystemConfig cfg;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            cfg = m_config;
        }

        for (const auto& thresh : cfg.thresholds) {
            if (!thresh.enabled) continue;
            checkResource(thresh.type);
        }

        // Sleep in chunks for responsive shutdown
        int remaining = pollMs;
        while (remaining > 0 && !m_monitorStopReq.load()) {
            int chunk = (remaining > 500) ? 500 : remaining;
            std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
            remaining -= chunk;
        }
    }
}

void AlertSystem::checkResource(ResourceType type)
{
    float pct = 0.0f;
    float val = 0.0f;
    float maxVal = 100.0f;

    switch (type) {
        case ResourceType::CPU:
            pct = queryCpuUsage();
            val = pct;
            break;
        case ResourceType::Memory:
            pct = queryMemoryUsage();
            val = pct;
            break;
        case ResourceType::Disk:
            pct = queryDiskUsage();
            val = pct;
            break;
        default:
            return; // GPU/Network/TokenRate not yet implemented
    }

    // Find threshold config
    AlertSystemConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    float warnPct = 80.0f, critPct = 95.0f;
    for (const auto& t : cfg.thresholds) {
        if (t.type == type) {
            warnPct = t.warningPct;
            critPct = t.criticalPct;
            break;
        }
    }

    bool inWarn = pct >= warnPct;
    bool inCrit = pct >= critPct;

    // Update cache
    {
        std::lock_guard<std::mutex> lock(m_resourceMutex);
        bool found = false;
        for (auto& rs : m_resourceCache) {
            if (rs.type == type) {
                rs.currentPct = pct;
                rs.currentValue = val;
                rs.maxValue = maxVal;
                rs.inWarning = inWarn;
                rs.inCritical = inCrit;
                found = true;
                break;
            }
        }
        if (!found) {
            m_resourceCache.push_back({type, pct, val, maxVal, inWarn, inCrit});
        }
    }

    // Emit alerts on threshold crossings
    const char* typeName = "Resource";
    switch (type) {
        case ResourceType::CPU:    typeName = "CPU Usage"; break;
        case ResourceType::Memory: typeName = "Memory Usage"; break;
        case ResourceType::Disk:   typeName = "Disk Usage"; break;
        default: break;
    }

    if (inCrit) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s at %.1f%% (threshold: %.1f%%)", typeName, pct, critPct);
        emit(AlertPriority::Critical, AlertChannel::TrayBalloon,
             "Critical Resource Warning", msg, "ResourceMonitor");
    } else if (inWarn) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s at %.1f%% (threshold: %.1f%%)", typeName, pct, warnPct);
        emit(AlertPriority::Warning, AlertChannel::StatusBar,
             "Resource Warning", msg, "ResourceMonitor");
    }
}

// ============================================================================
// OS Queries
// ============================================================================
float AlertSystem::queryCpuUsage() const
{
#ifdef _WIN32
    // Simplified CPU usage via GetSystemTimes
    static FILETIME prevIdle = {}, prevKernel = {}, prevUser = {};
    FILETIME idle, kernel, user;

    if (!GetSystemTimes(&idle, &kernel, &user)) return 0.0f;

    auto ft2ull = [](FILETIME ft) -> uint64_t {
        return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    };

    uint64_t idleDiff   = ft2ull(idle)   - ft2ull(prevIdle);
    uint64_t kernDiff   = ft2ull(kernel) - ft2ull(prevKernel);
    uint64_t userDiff   = ft2ull(user)   - ft2ull(prevUser);
    uint64_t totalDiff  = kernDiff + userDiff;

    prevIdle   = idle;
    prevKernel = kernel;
    prevUser   = user;

    if (totalDiff == 0) return 0.0f;
    return (1.0f - static_cast<float>(idleDiff) / static_cast<float>(totalDiff)) * 100.0f;
#else
    return 0.0f;
#endif
}

float AlertSystem::queryMemoryUsage() const
{
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        return static_cast<float>(ms.dwMemoryLoad);
    }
#endif
    return 0.0f;
}

float AlertSystem::queryDiskUsage() const
{
#ifdef _WIN32
    ULARGE_INTEGER freeBytes, totalBytes;
    if (GetDiskFreeSpaceExA(nullptr, &freeBytes, &totalBytes, nullptr)) {
        if (totalBytes.QuadPart == 0) return 0.0f;
        double used = static_cast<double>(totalBytes.QuadPart - freeBytes.QuadPart);
        return static_cast<float>((used / totalBytes.QuadPart) * 100.0);
    }
#endif
    return 0.0f;
}

// ============================================================================
// Logging
// ============================================================================
void AlertSystem::logAlert(const AlertEvent& evt) const
{
    bool logToFile;
    std::string logPath;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        logToFile = m_config.logToFile;
        logPath = m_config.logPath;
    }

    const char* prioStr = "INFO";
    switch (evt.priority) {
        case AlertPriority::Info:     prioStr = "INFO";     break;
        case AlertPriority::Warning:  prioStr = "WARN";     break;
        case AlertPriority::Error:    prioStr = "ERROR";    break;
        case AlertPriority::Critical: prioStr = "CRITICAL"; break;
    }

    char line[512];
    snprintf(line, sizeof(line), "[%s] [%s] [%s] %s: %s",
             evt.timestamp.c_str(), prioStr,
             evt.source.c_str(), evt.title.c_str(), evt.message.c_str());

    if (logToFile && !logPath.empty()) {
        std::ofstream out(logPath, std::ios::app);
        if (out.is_open()) {
            out << line << "\n";
        }
    }
}

// ============================================================================
// Callback
// ============================================================================
void AlertSystem::setAlertCallback(AlertCallback cb, void* userData)
{
    m_alertCb = cb;
    m_alertCbData = userData;
}
