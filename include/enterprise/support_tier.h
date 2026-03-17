// ============================================================================
// support_tier.h — Enterprise Support Tier System (Feature 0x10)
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

namespace RawrXD::Enterprise {

enum class SupportLevel : uint32_t {
    Community   = 0,
    Pro         = 1,
    Enterprise  = 2,
    OEM         = 3,
};

struct SLAConfig {
    SupportLevel     level;
    uint32_t         responseTimeMinutes;
    uint32_t         resolutionTimeHours;
    bool             phoneSupport;
    bool             dedicatedEngineer;
    bool             weekendCoverage;
    const char*      description;
};

enum class TicketPriority : uint32_t {
    Low      = 0,
    Normal   = 1,
    High     = 2,
    Critical = 3,
    Blocker  = 4,
};

enum class TicketStatus : uint32_t {
    Open        = 0,
    InProgress  = 1,
    Escalated   = 2,
    Resolved    = 3,
    Closed      = 4,
};

struct SupportTicket {
    uint64_t        id;
    TicketPriority  priority;
    TicketStatus    status;
    SupportLevel    tier;
    const char*     subject;
    const char*     description;
    uint64_t        createdAtMs;
    uint64_t        updatedAtMs;
    uint64_t        slaDeadlineMs;
    bool            slaBreached;
};

struct SupportResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static SupportResult ok(const char* msg = "OK") {
        return { true, msg, 0 };
    }
    static SupportResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

using TicketCreatedFn    = void(*)(const SupportTicket& ticket);
using TicketEscalatedFn  = void(*)(const SupportTicket& ticket);
using SLABreachFn        = void(*)(const SupportTicket& ticket);

class SupportTierManager {
public:
    static SupportTierManager& Instance();

    SupportTierManager(const SupportTierManager&) = delete;
    SupportTierManager& operator=(const SupportTierManager&) = delete;

    SupportResult Initialize(SupportLevel level);
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }

    SupportLevel GetCurrentLevel() const;
    const SLAConfig& GetSLAConfig() const;
    const char* GetLevelName(SupportLevel level) const;

    SupportResult CreateTicket(TicketPriority priority,
                               const char* subject,
                               const char* description);
    SupportResult EscalateTicket(uint64_t ticketId);
    SupportResult ResolveTicket(uint64_t ticketId);
    SupportResult CloseTicket(uint64_t ticketId);

    void CheckSLABreaches();
    uint32_t GetOpenTicketCount() const;
    uint32_t GetBreachedTicketCount() const;

    std::string GenerateStatusReport() const;
    std::string GenerateTicketList() const;

    void OnTicketCreated(TicketCreatedFn fn)       { m_onCreated = fn; }
    void OnTicketEscalated(TicketEscalatedFn fn)   { m_onEscalated = fn; }
    void OnSLABreach(SLABreachFn fn)               { m_onBreach = fn; }

private:
    SupportTierManager() = default;
    ~SupportTierManager() = default;

    uint64_t nextTicketId();
    uint64_t nowMs() const;

    bool                        m_initialized = false;
    mutable std::mutex          m_mutex;
    SupportLevel                m_level = SupportLevel::Community;
    SLAConfig                   m_slaConfig{};
    std::vector<SupportTicket>  m_tickets;
    uint64_t                    m_nextId = 1;

    TicketCreatedFn             m_onCreated = nullptr;
    TicketEscalatedFn           m_onEscalated = nullptr;
    SLABreachFn                 m_onBreach = nullptr;
};

} // namespace RawrXD::Enterprise
