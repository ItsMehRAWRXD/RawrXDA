// ============================================================================
// support_tier.cpp — Enterprise Support Tier System Implementation
// ============================================================================
// Implements SupportTierManager singleton for ticket management, SLA
// enforcement, and support routing. Gated by feature 0x10.
//
// PATTERN:   No exceptions. No std::function. Raw function pointers only.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "support_tier.h"
#include "enterprise_license.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

namespace RawrXD::Enterprise {

// ============================================================================
// Singleton
// ============================================================================
SupportTierManager& SupportTierManager::Instance() {
    static SupportTierManager instance;
    return instance;
}

// ============================================================================
// Static SLA configs per tier
// ============================================================================
static const SLAConfig s_slaConfigs[] = {
    // Community — best-effort, forum-only
    { SupportLevel::Community, 0, 0, false, false, false,
      "Community support: forums and documentation only" },

    // Pro — email support, 48h response, 120h resolution
    { SupportLevel::Pro, 2880, 120, false, false, false,
      "Professional support: email, 48h response SLA" },

    // Enterprise — priority, 4h response, 24h resolution, phone, dedicated
    { SupportLevel::Enterprise, 240, 24, true, true, true,
      "Enterprise support: 4h response, phone, dedicated engineer, 24/7" },

    // OEM — custom SLA, white-glove
    { SupportLevel::OEM, 60, 8, true, true, true,
      "OEM support: 1h response, white-glove, custom SLA" },
};

// ============================================================================
// Initialize
// ============================================================================
SupportResult SupportTierManager::Initialize(SupportLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return SupportResult::ok("Already initialized");
    }

    // Enterprise gate: feature 0x10
    if (!RawrXD::EnterpriseLicense::isFeatureEnabled(0x10)) {
        // Community users still get basic support tier, just no routing/SLA
        m_level = SupportLevel::Community;
        m_slaConfig = s_slaConfigs[0];
        m_initialized = true;
        std::cout << "[SupportTier] Community support tier active (no Enterprise license)\n";
        return SupportResult::ok("Community tier — enterprise features require license");
    }

    // Set the requested tier
    uint32_t idx = static_cast<uint32_t>(level);
    if (idx > 3) idx = 2;  // Fallback to Enterprise
    m_level = level;
    m_slaConfig = s_slaConfigs[idx];
    m_initialized = true;

    std::cout << "[SupportTier] Initialized: " << GetLevelName(level)
              << " (response SLA: " << m_slaConfig.responseTimeMinutes << " min)\n";

    return SupportResult::ok("Support tier initialized");
}

// ============================================================================
// Shutdown
// ============================================================================
void SupportTierManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    std::cout << "[SupportTier] Shutting down — " << m_tickets.size()
              << " tickets in queue\n";
    m_tickets.clear();
    m_initialized = false;
}

// ============================================================================
// Tier Queries
// ============================================================================
SupportLevel SupportTierManager::GetCurrentLevel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_level;
}

const SLAConfig& SupportTierManager::GetSLAConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_slaConfig;
}

const char* SupportTierManager::GetLevelName(SupportLevel level) const {
    switch (level) {
        case SupportLevel::Community:  return "Community";
        case SupportLevel::Pro:        return "Professional";
        case SupportLevel::Enterprise: return "Enterprise";
        case SupportLevel::OEM:        return "OEM";
        default:                       return "Unknown";
    }
}

// ============================================================================
// Ticket Management
// ============================================================================
SupportResult SupportTierManager::CreateTicket(TicketPriority priority,
                                                const char* subject,
                                                const char* description) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return SupportResult::error("Support tier not initialized", 1);
    }

    if (m_level == SupportLevel::Community && priority >= TicketPriority::High) {
        return SupportResult::error(
            "Community tier: high-priority tickets require Pro or Enterprise license", 2);
    }

    uint64_t now = nowMs();
    uint64_t slaDeadline = now + (static_cast<uint64_t>(m_slaConfig.responseTimeMinutes) * 60000);

    SupportTicket ticket{};
    ticket.id = nextTicketId();
    ticket.priority = priority;
    ticket.status = TicketStatus::Open;
    ticket.tier = m_level;
    ticket.subject = subject;
    ticket.description = description;
    ticket.createdAtMs = now;
    ticket.updatedAtMs = now;
    ticket.slaDeadlineMs = slaDeadline;
    ticket.slaBreached = false;

    m_tickets.push_back(ticket);

    if (m_onCreated) {
        m_onCreated(ticket);
    }

    std::cout << "[SupportTier] Ticket #" << ticket.id << " created: " << subject << "\n";
    return SupportResult::ok("Ticket created");
}

SupportResult SupportTierManager::EscalateTicket(uint64_t ticketId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& t : m_tickets) {
        if (t.id == ticketId) {
            if (t.status == TicketStatus::Closed || t.status == TicketStatus::Resolved) {
                return SupportResult::error("Cannot escalate closed/resolved ticket", 3);
            }
            t.status = TicketStatus::Escalated;
            t.updatedAtMs = nowMs();

            if (m_onEscalated) {
                m_onEscalated(t);
            }

            std::cout << "[SupportTier] Ticket #" << ticketId << " escalated\n";
            return SupportResult::ok("Ticket escalated");
        }
    }
    return SupportResult::error("Ticket not found", 4);
}

SupportResult SupportTierManager::ResolveTicket(uint64_t ticketId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& t : m_tickets) {
        if (t.id == ticketId) {
            t.status = TicketStatus::Resolved;
            t.updatedAtMs = nowMs();
            return SupportResult::ok("Ticket resolved");
        }
    }
    return SupportResult::error("Ticket not found", 4);
}

SupportResult SupportTierManager::CloseTicket(uint64_t ticketId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& t : m_tickets) {
        if (t.id == ticketId) {
            t.status = TicketStatus::Closed;
            t.updatedAtMs = nowMs();
            return SupportResult::ok("Ticket closed");
        }
    }
    return SupportResult::error("Ticket not found", 4);
}

// ============================================================================
// SLA Monitoring
// ============================================================================
void SupportTierManager::CheckSLABreaches() {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t now = nowMs();

    for (auto& t : m_tickets) {
        if (t.status == TicketStatus::Open || t.status == TicketStatus::InProgress) {
            if (!t.slaBreached && now > t.slaDeadlineMs) {
                t.slaBreached = true;
                std::cout << "[SupportTier] SLA BREACH: Ticket #" << t.id
                          << " — " << t.subject << "\n";
                if (m_onBreach) {
                    m_onBreach(t);
                }
            }
        }
    }
}

uint32_t SupportTierManager::GetOpenTicketCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = 0;
    for (const auto& t : m_tickets) {
        if (t.status == TicketStatus::Open || t.status == TicketStatus::InProgress ||
            t.status == TicketStatus::Escalated) {
            ++count;
        }
    }
    return count;
}

uint32_t SupportTierManager::GetBreachedTicketCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = 0;
    for (const auto& t : m_tickets) {
        if (t.slaBreached) ++count;
    }
    return count;
}

// ============================================================================
// Status Reports
// ============================================================================
std::string SupportTierManager::GenerateStatusReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ostringstream ss;
    ss << "\n";
    ss << "┌──────────────────────────────────────────────────────────┐\n";
    ss << "│          Enterprise Support Tier Status                  │\n";
    ss << "├──────────────────────────────────────────────────────────┤\n";
    ss << "│ Level:     " << std::left << std::setw(44) << GetLevelName(m_level) << "│\n";
    ss << "│ SLA:       " << std::left << std::setw(44) << m_slaConfig.description << "│\n";

    std::string responseStr = (m_slaConfig.responseTimeMinutes > 0)
        ? std::to_string(m_slaConfig.responseTimeMinutes) + " min"
        : "Best-effort";
    ss << "│ Response:  " << std::left << std::setw(44) << responseStr << "│\n";
    ss << "│ Phone:     " << std::left << std::setw(44) << (m_slaConfig.phoneSupport ? "Yes" : "No") << "│\n";
    ss << "│ Dedicated: " << std::left << std::setw(44) << (m_slaConfig.dedicatedEngineer ? "Yes" : "No") << "│\n";

    uint32_t openCount = 0, breachedCount = 0, totalCount = static_cast<uint32_t>(m_tickets.size());
    for (const auto& t : m_tickets) {
        if (t.status <= TicketStatus::Escalated) ++openCount;
        if (t.slaBreached) ++breachedCount;
    }

    std::string ticketStr = std::to_string(openCount) + " open / "
                          + std::to_string(totalCount) + " total"
                          + (breachedCount > 0 ? " (" + std::to_string(breachedCount) + " SLA breached)" : "");
    ss << "│ Tickets:   " << std::left << std::setw(44) << ticketStr << "│\n";
    ss << "└──────────────────────────────────────────────────────────┘\n";
    ss << std::right;  // Reset alignment

    return ss.str();
}

std::string SupportTierManager::GenerateTicketList() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_tickets.empty()) {
        return "  No support tickets.\n";
    }

    std::ostringstream ss;
    ss << "\n  ID   | Priority  | Status       | Subject\n";
    ss << " ------+-----------+--------------+----------------------------\n";

    const char* priorityNames[] = { "Low", "Normal", "High", "Critical", "Blocker" };
    const char* statusNames[] = { "Open", "InProgress", "Escalated", "Resolved", "Closed" };

    for (const auto& t : m_tickets) {
        uint32_t pi = static_cast<uint32_t>(t.priority);
        uint32_t si = static_cast<uint32_t>(t.status);
        if (pi > 4) pi = 1;
        if (si > 4) si = 0;

        ss << "  " << std::left << std::setw(5) << t.id
           << "| " << std::setw(10) << priorityNames[pi]
           << "| " << std::setw(13) << statusNames[si]
           << "| " << (t.subject ? t.subject : "(none)")
           << (t.slaBreached ? " [SLA BREACH]" : "")
           << "\n";
    }
    ss << std::right;  // Reset alignment
    ss << "\n";

    return ss.str();
}

// ============================================================================
// Private helpers
// ============================================================================
uint64_t SupportTierManager::nextTicketId() {
    return m_nextId++;
}

uint64_t SupportTierManager::nowMs() const {
    auto now = std::chrono::system_clock::now();
    auto dur = now.time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(dur).count());
}

} // namespace RawrXD::Enterprise

