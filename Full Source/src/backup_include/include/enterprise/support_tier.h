// ============================================================================
// support_tier.h — Enterprise Support Tier System (Feature 0x10)
// ============================================================================
// Provides priority support routing, SLA enforcement, and ticket lifecycle
// management for Enterprise-licensed users.
//
// Architecture:
//   SupportTierManager::Instance()
//    ├─ Tier classification (Community/Pro/Enterprise)
//    ├─ SLA guarantees per tier
//    ├─ Ticket routing & priority queuing
//    └─ Auto-escalation on SLA breach
//
// PATTERN:   No exceptions. No std::function. Raw function pointers only.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

namespace RawrXD::Enterprise {

// ============================================================================
// Support Tier Levels
// ============================================================================
enum class SupportLevel : uint32_t {
    Community   = 0,    // Forum-only, best-effort
    Pro         = 1,    // Email support, 48h SLA
    Enterprise  = 2,    // Priority, 4h SLA, phone, dedicated engineer
    OEM         = 3,    // Custom SLA, white-glove
};

// ============================================================================
// SLA Configuration per tier
// ============================================================================
struct SLAConfig {
    SupportLevel     level;
    uint32_t         responseTimeMinutes;   // Max response time SLA
    uint32_t         resolutionTimeHours;   // Max resolution time SLA
    bool             phoneSupport;
    bool             dedicatedEngineer;
    bool             weekendCoverage;
    const char*      description;
};

// ============================================================================
// Support Ticket
// ============================================================================
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
    uint64_t        createdAtMs;        // Unix epoch milliseconds
    uint64_t        updatedAtMs;
    uint64_t        slaDeadlineMs;      // When SLA breach occurs
    bool            slaBreached;
};

// ============================================================================
// Support Tier Result — structured return (no exceptions)
// ============================================================================
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

// ============================================================================
// Callbacks — raw function pointers (NO std::function)
// ============================================================================
using TicketCreatedFn    = void(*)(const SupportTicket& ticket);
using TicketEscalatedFn  = void(*)(const SupportTicket& ticket);
using SLABreachFn        = void(*)(const SupportTicket& ticket);

// ============================================================================
// Support Tier Manager — Singleton
// ============================================================================
class SupportTierManager {
public:
    static SupportTierManager& Instance();

    // Non-copyable
    SupportTierManager(const SupportTierManager&) = delete;
    SupportTierManager& operator=(const SupportTierManager&) = delete;

    // ---- Lifecycle ----
    SupportResult Initialize(SupportLevel level);
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }

    // ---- Tier Queries ----
    SupportLevel GetCurrentLevel() const;
    const SLAConfig& GetSLAConfig() const;
    const char* GetLevelName(SupportLevel level) const;

    // ---- Ticket Management ----
    SupportResult CreateTicket(TicketPriority priority,
                               const char* subject,
                               const char* description);
    SupportResult EscalateTicket(uint64_t ticketId);
    SupportResult ResolveTicket(uint64_t ticketId);
    SupportResult CloseTicket(uint64_t ticketId);

    // ---- SLA Monitoring ----
    void CheckSLABreaches();
    uint32_t GetOpenTicketCount() const;
    uint32_t GetBreachedTicketCount() const;

    // ---- Status / Reporting ----
    std::string GenerateStatusReport() const;
    std::string GenerateTicketList() const;

    // ---- Callbacks ----
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

    // Callbacks — raw pointers, no std::function
    TicketCreatedFn             m_onCreated = nullptr;
    TicketEscalatedFn           m_onEscalated = nullptr;
    SLABreachFn                 m_onBreach = nullptr;
};

} // namespace RawrXD::Enterprise

