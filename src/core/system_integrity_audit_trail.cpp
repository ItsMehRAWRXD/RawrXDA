// ============================================================================
// system_integrity_audit_trail.cpp — Audit Trail Implementation
// ============================================================================
#include "system_integrity_audit_trail.h"
#include "../config/IDEConfig.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <ctime>

namespace rawrxd {

// ============================================================================
// IntegrityAuditEvent JSON Serialization
// ============================================================================
std::string IntegrityAuditEvent::toJSON() const {
    std::ostringstream oss;
    oss << "{"
        << "\"timestamp\":" << timestampMs << ","
        << "\"context\":\"" << context << "\","
        << "\"layers\":{\"physical\":" << static_cast<int>((layerResults & 0x01) != 0)
        << ",\"logic\":" << static_cast<int>((layerResults & 0x02) != 0)
        << ",\"security\":" << static_cast<int>((layerResults & 0x04) != 0)
        << ",\"persistence\":" << static_cast<int>((layerResults & 0x08) != 0)
        << ",\"visibility\":" << static_cast<int>((layerResults & 0x10) != 0)
        << "},"
        << "\"passed\":" << (passed ? "true" : "false") << ","
        << "\"message\":\"" << message << "\","
        << "\"seqNo\":" << seqNo
        << "}";
    return oss.str();
}

IntegrityAuditEvent IntegrityAuditEvent::fromJSON(const std::string& json) {
    // Simplified parser (production should use nlohmann::json)
    IntegrityAuditEvent evt{};
    
    // Extract basic fields (naive substring search)
    size_t ts_pos = json.find("\"timestamp\":");
    if (ts_pos != std::string::npos) {
        size_t start = json.find_first_of("0123456789", ts_pos);
        size_t end = json.find_first_not_of("0123456789", start);
        if (start != std::string::npos) {
            evt.timestampMs = std::stoll(json.substr(start, end - start));
        }
    }

    // Extract passed field
    if (json.find("\"passed\":true") != std::string::npos) {
        evt.passed = true;
    }

    return evt;
}

// ============================================================================
// IntegrityAuditTrail Implementation
// ============================================================================
bool IntegrityAuditTrail::initialize(const std::string& storePath) {
    std::lock_guard<std::mutex> lk(m_mu);
    m_storePath = storePath;
    m_initialized.store(true, std::memory_order_release);
    return true;
}

void IntegrityAuditTrail::recordCheck(const IntegrityAuditEvent& event) {
    // Add sequence number
    IntegrityAuditEvent evt = event;
    evt.seqNo = m_seqNo.fetch_add(1, std::memory_order_acq_rel);

    {
        std::lock_guard<std::mutex> lk(m_mu);

        // Add to circular buffer
        m_buffer.push_back(evt);
        if (m_buffer.size() > MAX_BUFFER_SIZE) {
            m_buffer.pop_front();
        }
    }

    METRICS.increment("runtime.integrity.checks_total");
    METRICS.gauge("runtime.integrity.last_layers_mask", static_cast<double>(evt.layerResults));
    METRICS.gauge("runtime.integrity.last_passed", evt.passed ? 1.0 : 0.0);
    if (evt.passed) {
        METRICS.increment("runtime.integrity.pass_total");
    } else {
        METRICS.increment("runtime.integrity.fail_total");
    }

    auto stats = getStats();
    METRICS.gauge("runtime.integrity.total_checks", static_cast<double>(stats.total_checks));
    METRICS.gauge("runtime.integrity.total_failed", static_cast<double>(stats.total_failed));
    METRICS.gauge("runtime.integrity.pass_rate",
                  stats.total_checks > 0
                      ? static_cast<double>(stats.total_passed) / static_cast<double>(stats.total_checks)
                      : 0.0);

    // Async append to backing store (if configured)
    if (!m_storePath.empty()) {
        appendToBacking(evt);
    }
}

std::vector<IntegrityAuditEvent> IntegrityAuditTrail::getRecentEntries(size_t count) const {
    std::lock_guard<std::mutex> lk(m_mu);
    std::vector<IntegrityAuditEvent> result;

    // Return most recent entries (reverse order)
    size_t start_idx = (m_buffer.size() > count) ? (m_buffer.size() - count) : 0;
    for (size_t i = m_buffer.size(); i > start_idx; --i) {
        result.push_back(m_buffer[i - 1]);
    }
    return result;
}

std::vector<IntegrityAuditEvent> IntegrityAuditTrail::getEntriesWhere(
    std::function<bool(const IntegrityAuditEvent&)> predicate) const {
    std::lock_guard<std::mutex> lk(m_mu);
    std::vector<IntegrityAuditEvent> result;

    for (const auto& evt : m_buffer) {
        if (predicate(evt)) {
            result.push_back(evt);
        }
    }
    return result;
}

std::vector<IntegrityAuditEvent> IntegrityAuditTrail::getFailedChecks(size_t count) const {
    return getEntriesWhere([](const IntegrityAuditEvent& evt) { return !evt.passed; });
}

std::vector<IntegrityAuditEvent> IntegrityAuditTrail::getEntriesSince(uint64_t untilMs) const {
    return getEntriesWhere([untilMs](const IntegrityAuditEvent& evt) {
        return evt.timestampMs >= untilMs;
    });
}

bool IntegrityAuditTrail::exportToFile(const std::string& filepath) const {
    std::lock_guard<std::mutex> lk(m_mu);

    std::ofstream ofs(filepath, std::ios::out | std::ios::binary);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << "{\n  \"audit_trail\": [\n";
    bool first = true;
    for (const auto& evt : m_buffer) {
        if (!first) ofs << ",\n";
        ofs << "    " << evt.toJSON();
        first = false;
    }
    ofs << "\n  ]\n}\n";

    return true;
}

IntegrityAuditTrail::Stats IntegrityAuditTrail::getStats() const {
    std::lock_guard<std::mutex> lk(m_mu);
    Stats s{};

    s.total_checks = m_buffer.size();
    s.total_passed = 0;
    s.total_failed = 0;

    for (const auto& evt : m_buffer) {
        if (evt.passed) {
            s.total_passed++;
        } else {
            s.total_failed++;
            if (!(evt.layerResults & 0x01)) s.failed_physical++;
            if (!(evt.layerResults & 0x02)) s.failed_logic++;
            if (!(evt.layerResults & 0x04)) s.failed_security++;
            if (!(evt.layerResults & 0x08)) s.failed_persistence++;
            if (!(evt.layerResults & 0x10)) s.failed_visibility++;
        }
    }

    return s;
}

void IntegrityAuditTrail::clear() {
    std::lock_guard<std::mutex> lk(m_mu);
    m_buffer.clear();
    m_seqNo.store(0, std::memory_order_release);
}

void IntegrityAuditTrail::appendToBacking(const IntegrityAuditEvent& event) {
    // In production, this would:
    // 1. Use RocksDB or SQLite for persistent storage
    // 2. Run async on background thread
    // 3. Handle compression/rotation of old entries
    // For now, this is a placeholder for the interface
    std::ofstream logfile(m_storePath, std::ios::app);
    if (logfile.is_open()) {
        logfile << event.toJSON() << "\n";
    }
}

// ============================================================================
// IntegrityProverWithAudit Implementation
// ============================================================================
bool IntegrityProverWithAudit::initialize(const std::string& auditStorePath) {
    return IntegrityAuditTrail::Instance().initialize(auditStorePath);
}

bool IntegrityProverWithAudit::runBeforeCriticalOpWithAudit(const std::string& opName,
                                                            uint8_t& outLayerResults) {
    auto started = std::chrono::steady_clock::now();
    // TODO: Call actual SystemIntegrityProver verification methods
    // For now, use placeholder implementation
    uint8_t layers = 0;
    bool passed = true;

    // In production, would call:
    // - SystemIntegrityProver::Instance().VerifyPhysical()
    // - SystemIntegrityProver::Instance().VerifyLogic()
    // - SystemIntegrityProver::Instance().VerifySecurity()
    // - SystemIntegrityProver::Instance().VerifyPersistence()
    // - SystemIntegrityProver::Instance().VerifyVisibility()

    // Set bits accordingly and determine overall pass
    outLayerResults = layers;

    // Create audit event
    auto now = std::chrono::system_clock::now();
    uint64_t timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    IntegrityAuditEvent evt{
        timestampMs,
        opName,
        layers,
        passed,
        passed ? "All layers passed" : "One or more layers failed"
    };

    // Record and fire callbacks
    IntegrityAuditTrail::Instance().recordCheck(evt);
    fireCallbacks(evt);
    auto finished = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(finished - started).count();
    METRICS.recordDuration("runtime.integrity.check_ms", elapsedMs);

    return passed;
}

bool IntegrityProverWithAudit::runOnStartupWithAudit(bool force) {
    uint8_t layers = 0;
    return runBeforeCriticalOpWithAudit("startup", layers);
}

IntegrityAuditEvent IntegrityProverWithAudit::buildAuditEvent(
    const std::string& context,
    uint8_t layerResults,
    bool passed,
    const std::string& message) const {
    auto now = std::chrono::system_clock::now();
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return IntegrityAuditEvent{
        ms,
        context,
        layerResults,
        passed,
        message
    };
}

}  // namespace rawrxd
