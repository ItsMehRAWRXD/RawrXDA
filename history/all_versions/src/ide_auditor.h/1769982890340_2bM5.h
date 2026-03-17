#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <memory>

namespace RawrXD {

class IDEAuditor {
public:
    static IDEAuditor& getInstance();
    
    IDEAuditor(const IDEAuditor&) = delete;
    IDEAuditor& operator=(const IDEAuditor&) = delete;
    
    void logAction(const std::string& component, const std::string& action, const std::string& details);
    void logError(const std::string& component, const std::string& error);
    void generateReport(const std::string& path);
    
private:
    std::mutex m_mutex;
    std::ofstream m_logFile;
    std::string m_sessionLog;
    
    IDEAuditor();
    ~IDEAuditor();
};

} // namespace RawrXD