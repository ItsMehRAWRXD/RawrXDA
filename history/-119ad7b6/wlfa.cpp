// RawrXD IDE Health Monitor - Implementation
#include "ide_health_monitor.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <windows.h>

namespace RawrXD::Monitor {

// Console color implementation (Windows)
void ConsoleColor::setGreen() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
}

void ConsoleColor::setRed() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
}

void ConsoleColor::setYellow() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
}

void ConsoleColor::setBlue() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
}

void ConsoleColor::setGray() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void ConsoleColor::setWhite() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
}

void ConsoleColor::setCyan() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
}

void ConsoleColor::setBold() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY);
}

void ConsoleColor::reset() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

// IDEHealthMonitor implementation
IDEHealthMonitor::IDEHealthMonitor() = default;

IDEHealthMonitor::~IDEHealthMonitor() {
    stopContinuousMonitoring();
}

void IDEHealthMonitor::registerProbe(std::shared_ptr<FeatureProbe> probe) {
    m_probes.push_back(probe);
    auto info = probe->getInfo();
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_featureStatus[info.id] = info;
}

void IDEHealthMonitor::runFullDiagnostics() {
    std::cout << "\n";
    ConsoleColor::setCyan();
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     RawrXD IDE Health Monitor - Diagnostic Report               ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    ConsoleColor::reset();
    
    std::cout << "Running diagnostics on " << m_probes.size() << " features...\n\n";
    
    // Run all probes
    for (auto& probe : m_probes) {
        auto info = probe->getInfo();
        auto status = probe->probe();
        
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_featureStatus[info.id].status = status;
        m_featureStatus[info.id].lastChecked = std::chrono::system_clock::now();
        
        if (status == FeatureStatus::Failing || status == FeatureStatus::Error) {
            m_featureStatus[info.id].lastError = probe->getDetailedReport();
        }
    }
    
    displayStatus();
}

void IDEHealthMonitor::startContinuousMonitoring(int intervalSeconds) {
    if (m_monitorRunning) return;
    
    m_monitorRunning = true;
    m_monitorThread = std::thread(&IDEHealthMonitor::monitorThreadFunc, this, intervalSeconds);
}

void IDEHealthMonitor::stopContinuousMonitoring() {
    m_monitorRunning = false;
    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }
}

std::vector<FeatureInfo> IDEHealthMonitor::getAllFeatures() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    std::vector<FeatureInfo> result;
    for (const auto& [id, info] : m_featureStatus) {
        result.push_back(info);
    }
    return result;
}

std::vector<FeatureInfo> IDEHealthMonitor::getFeaturesByCategory(const std::string& category) const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    std::vector<FeatureInfo> result;
    for (const auto& [id, info] : m_featureStatus) {
        if (info.category == category) {
            result.push_back(info);
        }
    }
    return result;
}

int IDEHealthMonitor::getOverallHealthScore() const {
    auto summary = getSummary();
    if (summary.totalFeatures == 0) return 0;
    
    return (summary.working * 100) / summary.totalFeatures;
}

IDEHealthMonitor::Summary IDEHealthMonitor::getSummary() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    Summary summary;
    
    for (const auto& [id, info] : m_featureStatus) {
        summary.totalFeatures++;
        
        switch (info.status) {
            case FeatureStatus::Working: summary.working++; break;
            case FeatureStatus::Failing: summary.failing++; break;
            case FeatureStatus::Timeout: summary.timeout++; break;
            case FeatureStatus::Error: summary.error++; break;
            case FeatureStatus::Unknown: summary.unknown++; break;
        }
    }
    
    return summary;
}

void IDEHealthMonitor::displayStatus() const {
    auto summary = getSummary();
    auto features = getAllFeatures();
    
    // Group by category
    std::map<std::string, std::vector<FeatureInfo>> byCategory;
    for (const auto& feature : features) {
        byCategory[feature.category].push_back(feature);
    }
    
    // Display by category
    for (const auto& [category, items] : byCategory) {
        ConsoleColor::setBlue();
        std::cout << "\n" << category << ":\n";
        ConsoleColor::reset();
        
        for (const auto& feature : items) {
            std::cout << "  " << statusToChar(feature.status) << " ";
            
            // Set color based on status
            if (feature.status == FeatureStatus::Working) {
                ConsoleColor::setGreen();
            } else if (feature.status == FeatureStatus::Failing || feature.status == FeatureStatus::Error) {
                ConsoleColor::setRed();
            } else if (feature.status == FeatureStatus::Timeout) {
                ConsoleColor::setYellow();
            }
            
            std::cout << std::left << std::setw(35) << feature.name;
            ConsoleColor::reset();
            
            // Show error if present
            if (!feature.lastError.empty()) {
                std::cout << " [" << feature.lastError.substr(0, 30) << "...]";
            }
            std::cout << "\n";
        }
    }
    
    // Display summary
    ConsoleColor::setBold();
    std::cout << "\n" << std::string(65, '═') << "\n";
    std::cout << "Summary:\n";
    ConsoleColor::reset();
    
    int healthScore = getOverallHealthScore();
    
    ConsoleColor::setGreen();
    std::cout << "  ✅ Working:  " << summary.working << "\n";
    ConsoleColor::setRed();
    std::cout << "  ❌ Failing:  " << summary.failing << "\n";
    ConsoleColor::setYellow();
    std::cout << "  ⏱️ Timeout:  " << summary.timeout << "\n";
    ConsoleColor::setRed();
    std::cout << "  🔴 Errors:   " << summary.error << "\n";
    ConsoleColor::setGray();
    std::cout << "  ❓ Unknown:  " << summary.unknown << "\n";
    ConsoleColor::reset();
    
    std::cout << "\n";
    if (healthScore >= 80) {
        ConsoleColor::setGreen();
    } else if (healthScore >= 50) {
        ConsoleColor::setYellow();
    } else {
        ConsoleColor::setRed();
    }
    std::cout << "Overall Health Score: " << healthScore << "/100\n";
    ConsoleColor::reset();
    
    std::cout << "Total Features: " << summary.totalFeatures << "\n";
    std::cout << std::string(65, '═') << "\n\n";
}

std::string IDEHealthMonitor::exportAsJSON() const {
    std::stringstream ss;
    auto summary = getSummary();
    auto features = getAllFeatures();
    
    ss << "{\n";
    ss << "  \"timestamp\": \"" << std::chrono::system_clock::now().time_since_epoch().count() << "\",\n";
    ss << "  \"health_score\": " << getOverallHealthScore() << ",\n";
    ss << "  \"summary\": {\n";
    ss << "    \"total_features\": " << summary.totalFeatures << ",\n";
    ss << "    \"working\": " << summary.working << ",\n";
    ss << "    \"failing\": " << summary.failing << ",\n";
    ss << "    \"timeout\": " << summary.timeout << ",\n";
    ss << "    \"error\": " << summary.error << ",\n";
    ss << "    \"unknown\": " << summary.unknown << "\n";
    ss << "  },\n";
    ss << "  \"features\": [\n";
    
    for (size_t i = 0; i < features.size(); ++i) {
        const auto& f = features[i];
        ss << "    {\n";
        ss << "      \"id\": \"" << f.id << "\",\n";
        ss << "      \"name\": \"" << f.name << "\",\n";
        ss << "      \"category\": \"" << f.category << "\",\n";
        ss << "      \"status\": \"" << (int)f.status << "\",\n";
        ss << "      \"health_score\": " << f.healthScore << "\n";
        ss << "    }";
        if (i < features.size() - 1) ss << ",";
        ss << "\n";
    }
    
    ss << "  ]\n";
    ss << "}\n";
    
    return ss.str();
}

void IDEHealthMonitor::logToFile(const std::string& filename) const {
    // Implementation: write to file
    // This would save JSON export to file with timestamp
}

void IDEHealthMonitor::monitorThreadFunc(int intervalSeconds) {
    while (m_monitorRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
        
        // Re-run diagnostics
        for (auto& probe : m_probes) {
            auto info = probe->getInfo();
            auto status = probe->probe();
            
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_featureStatus[info.id].status = status;
            m_featureStatus[info.id].lastChecked = std::chrono::system_clock::now();
        }
    }
}

} // namespace RawrXD::Monitor
