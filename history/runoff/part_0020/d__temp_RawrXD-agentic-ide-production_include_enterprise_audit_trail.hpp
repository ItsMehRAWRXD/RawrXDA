/**
 * @file audit_trail.hpp
 * @brief Enterprise Audit Trail
 *
 * Features:
 * - Persistent audit events
 * - Queryable audit logs
 * - Compliance reports
 * - Per-tenant and global auditing
 * - Event types, actors, resources
 */

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <optional>
#include <unordered_map>

namespace enterprise {

enum class AuditLevel { INFO, WARN, ERR, SECURITY };

struct AuditEvent {
    std::string id;
    std::string tenantId; // optional
    std::string actorId;  // user or system
    std::string resource; // affected resource
    std::string action;   // created, updated, deleted, accessed
    std::string details;  // free-form JSON or text
    AuditLevel level = AuditLevel::INFO;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;
};

struct AuditQuery {
    std::optional<std::string> tenantId;
    std::optional<std::string> actorId;
    std::optional<std::string> resource;
    std::optional<std::string> action;
    std::optional<AuditLevel> level;
    std::chrono::system_clock::time_point from;
    std::chrono::system_clock::time_point to;
    size_t limit = 100;
};

class AuditTrail {
public:
    static AuditTrail& instance();

    // Configuration
    void setPersistencePath(const std::string& path);
    std::string getPersistencePath() const;

    // Recording
    void record(const AuditEvent& event);
    void recordNow(const std::string& tenantId,
                   const std::string& actorId,
                   const std::string& resource,
                   const std::string& action,
                   const std::string& details = "",
                   AuditLevel level = AuditLevel::INFO);

    // Query
    std::vector<AuditEvent> query(const AuditQuery& q) const;

    // Purge
    void purgeOlderThanDays(int days);
    
    // Export
    std::string exportJson(const AuditQuery& q) const;

private:
    AuditTrail();

    std::string generateId();
    
    std::string m_persistencePath;
    std::vector<AuditEvent> m_events;
    mutable std::mutex m_mutex;
};

} // namespace enterprise
