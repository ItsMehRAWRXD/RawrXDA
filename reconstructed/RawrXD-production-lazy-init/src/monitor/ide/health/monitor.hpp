// RawrXD IDE Health Monitor - Real-time CLI feature status dashboard
// Probes IDE features and displays green/red status indicators
#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <memory>
#include <functional>

namespace RawrXD::Monitor {

// Feature status indicators
enum class FeatureStatus {
    Unknown = 0,      // ❓ Not tested yet
    Working = 1,      // ✅ Feature working
    Failing = 2,      // ❌ Feature broken
    Timeout = 3,      // ⏱️ Feature timed out
    Error = 4         // 🔴 Error during test
};

// Convert status to display character
inline const char* statusToChar(FeatureStatus status) {
    switch (status) {
        case FeatureStatus::Working: return "✅";
        case FeatureStatus::Failing: return "❌";
        case FeatureStatus::Timeout: return "⏱️";
        case FeatureStatus::Error:   return "🔴";
        default:                     return "❓";
    }
}

// Feature info structure
struct FeatureInfo {
    std::string id;              // Unique identifier (e.g., "chat", "terminals")
    std::string name;            // Display name (e.g., "AI Chat")
    std::string category;        // Category (e.g., "AI/ML", "Core Editor")
    FeatureStatus status;        // Current status
    std::string lastError;       // Last error message (if any)
    std::chrono::system_clock::time_point lastChecked;
    int healthScore;             // 0-100 (100 = all tests passed)
};

// Feature probe interface
class FeatureProbe {
public:
    virtual ~FeatureProbe() = default;
    
    // Test the feature and return status
    virtual FeatureStatus probe() = 0;
    
    // Get feature info
    virtual FeatureInfo getInfo() const = 0;
    
    // Get detailed test results
    virtual std::string getDetailedReport() = 0;
};

// IDE Health Monitor - main system
class IDEHealthMonitor {
public:
    IDEHealthMonitor();
    ~IDEHealthMonitor();
    
    // Register a feature probe
    void registerProbe(std::shared_ptr<FeatureProbe> probe);
    
    // Run all probes (blocking)
    void runFullDiagnostics();
    
    // Start continuous monitoring in background thread
    void startContinuousMonitoring(int intervalSeconds = 5);
    
    // Stop continuous monitoring
    void stopContinuousMonitoring();
    
    // Get all feature statuses
    std::vector<FeatureInfo> getAllFeatures() const;
    
    // Get features by category
    std::vector<FeatureInfo> getFeaturesByCategory(const std::string& category) const;
    
    // Get overall health score (0-100)
    int getOverallHealthScore() const;
    
    // Get summary (working/failing/timeout counts)
    struct Summary {
        int totalFeatures = 0;
        int working = 0;
        int failing = 0;
        int timeout = 0;
        int error = 0;
        int unknown = 0;
    };
    Summary getSummary() const;
    
    // Display current status to console (with colors)
    void displayStatus() const;
    
    // Export status as JSON
    std::string exportAsJSON() const;
    
    // Log to file
    void logToFile(const std::string& filename) const;
    
private:
    std::vector<std::shared_ptr<FeatureProbe>> m_probes;
    std::map<std::string, FeatureInfo> m_featureStatus;
    std::thread m_monitorThread;
    bool m_monitorRunning = false;
    mutable std::mutex m_statusMutex;
    
    void monitorThreadFunc(int intervalSeconds);
};

// Console color utilities (Windows console API)
class ConsoleColor {
public:
    static void setGreen();
    static void setRed();
    static void setYellow();
    static void setBlue();
    static void setGray();
    static void setWhite();
    static void reset();
    static void setCyan();
    static void setBold();
};

} // namespace RawrXD::Monitor
