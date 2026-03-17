#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct AuditEvent {
    std::string eventId;
    std::string eventType;
    std::string userId;
    std::string tenantId;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
    json metadata;
};

class AuditTrail {
private:
    static AuditTrail* s_instance;
    static std::mutex s_mutex;
    std::vector<AuditEvent> m_events;
    std::string m_persistencePath;

    AuditTrail() = default;

public:
    static AuditTrail& instance();

    void setPersistencePath(const std::string& path);
    void logEvent(const AuditEvent& event);
    std::vector<AuditEvent> getEvents(const std::string& tenantId) const;
    void clearEvents();
    void persistToFile();
    void loadFromFile();
};
