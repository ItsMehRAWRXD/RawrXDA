/**
 * @file audit_trail.cpp
 * @brief Enterprise Audit Trail Implementation
 */

#include "enterprise/audit_trail.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <atomic>
// #include <nlohmann/json.hpp>  // Commented out - library not available

namespace enterprise {

AuditTrail& AuditTrail::instance() {
    static AuditTrail inst;
    return inst;
}

AuditTrail::AuditTrail() {
    m_persistencePath = "./audit_logs.jsonl";
}

void AuditTrail::setPersistencePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_persistencePath = path;
}

std::string AuditTrail::getPersistencePath() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_persistencePath;
}

std::string AuditTrail::generateId() {
    static std::atomic<uint64_t> counter{0};
    uint64_t id = counter.fetch_add(1) + 1;
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::stringstream ss;
    ss << "audit_" << std::hex << ts << "_" << id;
    return ss.str();
}

void AuditTrail::record(const AuditEvent& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.push_back(event);
    
    // Simple CSV-like format instead of JSON (no nlohmann dependency)
    try {
        std::string line = event.id + "|" + event.tenantId + "|" + event.actorId + "|" +
                          event.resource + "|" + event.action + "|" + event.details;
        
        std::ofstream ofs(m_persistencePath, std::ios::app);
        if (ofs) {
            ofs << line << std::endl;
            ofs.close();
        }
    } catch (...) {
        // ignore serialization errors
    }
}

void AuditTrail::recordNow(const std::string& tenantId,
                           const std::string& actorId,
                           const std::string& resource,
                           const std::string& action,
                           const std::string& details,
                           AuditLevel level) {
    AuditEvent ev;
    ev.id = generateId();
    ev.tenantId = tenantId;
    ev.actorId = actorId;
    ev.resource = resource;
    ev.action = action;
    ev.details = details;
    ev.level = level;
    ev.timestamp = std::chrono::system_clock::now();
    
    record(ev);
}

std::vector<AuditEvent> AuditTrail::query(const AuditQuery& q) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AuditEvent> out;
    for (const auto& e : m_events) {
        if (e.timestamp < q.from || e.timestamp > q.to) continue;
        if (q.tenantId && e.tenantId != *q.tenantId) continue;
        if (q.actorId && e.actorId != *q.actorId) continue;
        if (q.resource && e.resource != *q.resource) continue;
        if (q.action && e.action != *q.action) continue;
        if (q.level && e.level != *q.level) continue;
        out.push_back(e);
        if (out.size() >= q.limit) break;
    }
    return out;
}

void AuditTrail::purgeOlderThanDays(int days) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
    
    auto it = std::remove_if(m_events.begin(), m_events.end(), [&](const AuditEvent& e) {
        return e.timestamp < cutoff;
    });
    m_events.erase(it, m_events.end());
    
    // Rewrite persistence file
    try {
        std::ofstream ofs(m_persistencePath, std::ios::trunc);
        if (ofs) {
            for (const auto& e : m_events) {
                // Simple JSON-like string instead of nlohmann/json
                std::ostringstream json_str;
                json_str << "{";
                json_str << "\"id\":\"" << e.id << "\",";
                json_str << "\"tenantId\":\"" << e.tenantId << "\",";
                json_str << "\"actorId\":\"" << e.actorId << "\",";
                json_str << "\"resource\":\"" << e.resource << "\",";
                json_str << "\"action\":\"" << e.action << "\",";
                json_str << "\"details\":\"" << e.details << "\",";
                json_str << "\"level\":" << static_cast<int>(e.level) << ",";
                json_str << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(e.timestamp.time_since_epoch()).count();
                json_str << "}";
                ofs << json_str.str() << std::endl;
            }
            ofs.close();
        }
    } catch (...) {
        // ignore I/O errors
    }
}

std::string AuditTrail::exportJson(const AuditQuery& q) const {
    auto events = query(q);
    std::ostringstream result;
    result << "[";
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        result << "{";
        result << "\"id\":\"" << e.id << "\",";
        result << "\"tenantId\":\"" << e.tenantId << "\",";
        result << "\"actorId\":\"" << e.actorId << "\",";
        result << "\"resource\":\"" << e.resource << "\",";
        result << "\"action\":\"" << e.action << "\",";
        result << "\"details\":\"" << e.details << "\",";
        result << "\"level\":" << static_cast<int>(e.level) << ",";
        result << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(e.timestamp.time_since_epoch()).count();
        result << "}";
        if (i < events.size() - 1) result << ",";
    }
    result << "]";
    return result.str();
}

} // namespace enterprise
