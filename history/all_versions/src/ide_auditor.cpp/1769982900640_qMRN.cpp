#include "ide_auditor.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>

namespace RawrXD {

IDEAuditor& IDEAuditor::getInstance() {
    static IDEAuditor instance;
    return instance;
}

IDEAuditor::IDEAuditor() {
    std::filesystem::create_directories("logs");
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "logs/ide_audit_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".log";
    
    m_logFile.open(ss.str(), std::ios::app);
    if (m_logFile.is_open()) {
        m_logFile << "=== IDE Audit Session Started ===\n";
    }
}

IDEAuditor::~IDEAuditor() {
    if (m_logFile.is_open()) {
        m_logFile << "=== IDE Audit Session Ended ===\n";
        m_logFile.close();
    }
}

void IDEAuditor::logAction(const std::string& component, const std::string& action, const std::string& details) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "] "
       << "[ACTION] [" << component << "] " << action << ": " << details << "\n";
       
    std::string entry = ss.str();
    m_sessionLog += entry; // Keep in memory for report
    
    if (m_logFile.is_open()) {
        m_logFile << entry;
        m_logFile.flush();
    }
    std::cout << entry; // Also print to stdout
}

void IDEAuditor::logError(const std::string& component, const std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "] "
       << "[ERROR] [" << component << "] " << error << "\n";
       
    std::string entry = ss.str();
    m_sessionLog += entry;
    
    if (m_logFile.is_open()) {
        m_logFile << entry;
        m_logFile.flush();
    }
    std::cerr << entry;
}

void IDEAuditor::generateReport(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream report(path);
    if (report.is_open()) {
        report << "# IDE Action Report\n\n";
        report << m_sessionLog;
    }
}

} // namespace RawrXD