/**
 * @file telemetry_collector.cpp
 * @brief TelemetryCollector implementation – Qt-free (C++20 / Win32)
 *
 * Privacy-respecting, opt-in telemetry. Uses WinHTTP for data submission,
 * std::filesystem for consent persistence, and json_types.hpp for payloads.
 */

#include "telemetry_collector.hpp"
#include "process_utils.hpp"
#include <cstdio>
#include <chrono>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#pragma comment(lib, "ole32.lib")
#endif

namespace fs = std::filesystem;

// ── Helpers ──────────────────────────────────────────────────────────────

static int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

static std::string generateSessionId() {
#ifdef _WIN32
    GUID guid{};
    CoCreateGuid(&guid);
    char buf[64];
    snprintf(buf, sizeof(buf),
             "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             guid.Data1, guid.Data2, guid.Data3,
             guid.Data4[0], guid.Data4[1],
             guid.Data4[2], guid.Data4[3],
             guid.Data4[4], guid.Data4[5],
             guid.Data4[6], guid.Data4[7]);
    return buf;
#else
    char buf[64];
    snprintf(buf, sizeof(buf), "sess-%016llx%08x",
             (unsigned long long)nowMs(), (unsigned)rand());
    return buf;
#endif
}

static std::string isoTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    struct tm t{};
#ifdef _WIN32
    gmtime_s(&t, &tt);
#else
    gmtime_r(&tt, &t);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
    return buf;
}

static std::string consentFilePath() {
    std::string appData = getEnvVar("APPDATA", ".");
    return appData + "\\RawrXD\\telemetry_consent.txt";
}

// ── Singleton ────────────────────────────────────────────────────────────

TelemetryCollector* TelemetryCollector::s_instance = nullptr;

TelemetryCollector* TelemetryCollector::instance() {
    if (!s_instance) s_instance = new TelemetryCollector();
    return s_instance;
}

TelemetryCollector::TelemetryCollector()  = default;
TelemetryCollector::~TelemetryCollector() = default;

// ── initialize ───────────────────────────────────────────────────────────

bool TelemetryCollector::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_sessionId        = generateSessionId();
    m_sessionStartTime = nowMs();
    m_enabled          = loadUserConsent();

    fprintf(stderr, "[Telemetry] Initialized (enabled=%d, session=%s)\n",
            m_enabled, m_sessionId.c_str());
    return true;
}

// ── enable / disable ─────────────────────────────────────────────────────

void TelemetryCollector::enableTelemetry() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = true;
    saveUserConsent(true);
    fprintf(stderr, "[Telemetry] Enabled\n");
    if (m_enabledCb) m_enabledCb(m_enabledCtx);
}

void TelemetryCollector::disableTelemetry() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = false;
    saveUserConsent(false);
    fprintf(stderr, "[Telemetry] Disabled\n");
    if (m_disabledCb) m_disabledCb(m_disabledCtx);
}

// ── trackFeatureUsage ────────────────────────────────────────────────────

void TelemetryCollector::trackFeatureUsage(const std::string& featureName,
                                           const JsonObject& metadata) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled) return;

    std::string safe = sanitize(featureName);
    m_featureUsage[safe]++;

    JsonObject ev;
    ev["type"]      = "feature_usage";
    ev["feature"]   = safe;
    ev["count"]     = m_featureUsage[safe];
    ev["timestamp"] = isoTimestamp();
    ev["session"]   = m_sessionId;

    if (!metadata.empty()) {
        ev["metadata"] = metadata;
    }

    m_events.push_back(std::move(ev));
}

// ── trackCrash ───────────────────────────────────────────────────────────

void TelemetryCollector::trackCrash(const std::string& crashReason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled) return;

    JsonObject ev;
    ev["type"]      = "crash";
    ev["reason"]    = sanitize(crashReason);
    ev["timestamp"] = isoTimestamp();
    ev["session"]   = m_sessionId;
    ev["platform"]  = sysinfo::productType();
    ev["arch"]      = sysinfo::cpuArchitecture();

    m_events.push_back(std::move(ev));
}

// ── trackPerformance ─────────────────────────────────────────────────────

void TelemetryCollector::trackPerformance(const std::string& metricName,
                                          double value,
                                          const std::string& unit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled) return;

    JsonObject ev;
    ev["type"]      = "performance";
    ev["metric"]    = sanitize(metricName);
    ev["value"]     = value;
    ev["timestamp"] = isoTimestamp();
    ev["session"]   = m_sessionId;

    if (!unit.empty()) {
        ev["unit"] = unit;
    }

    m_events.push_back(std::move(ev));
}

// ── getAllTelemetryData ──────────────────────────────────────────────────

JsonObject TelemetryCollector::getAllTelemetryData() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    JsonObject result;
    result["session_id"]    = m_sessionId;
    result["enabled"]       = m_enabled;
    result["event_count"]   = static_cast<int64_t>(m_events.size());

    // Feature usage summary
    JsonObject usage;
    for (auto& [k, v] : m_featureUsage) {
        usage[k] = v;
    }
    result["feature_usage"] = std::move(usage);

    // Session duration
    int64_t durationMs = nowMs() - m_sessionStartTime;
    result["session_duration_ms"] = durationMs;

    return result;
}

// ── clearAllData ─────────────────────────────────────────────────────────

void TelemetryCollector::clearAllData() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.clear();
    m_featureUsage.clear();
    fprintf(stderr, "[Telemetry] All data cleared\n");
}

// ── flushData ────────────────────────────────────────────────────────────

void TelemetryCollector::flushData() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled || m_events.empty()) return;

    int count = static_cast<int>(m_events.size());

    // Build batch payload
    JsonObject payload;
    payload["session_id"] = m_sessionId;
    payload["platform"]   = sysinfo::productType();
    payload["arch"]       = sysinfo::cpuArchitecture();
    payload["timestamp"]  = isoTimestamp();

    JsonObject eventArr = JsonObject::array();
    for (auto& ev : m_events) {
        eventArr.push_back(ev);
    }
    payload["events"] = std::move(eventArr);

    sendTelemetry(payload);
    m_events.clear();

    if (m_flushedCb) m_flushedCb(m_flushedCtx, count);

    fprintf(stderr, "[Telemetry] Flushed %d events\n", count);
}

// ── sanitize (private) ──────────────────────────────────────────────────

std::string TelemetryCollector::sanitize(const std::string& input) const {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        // Keep alphanumeric, underscores, hyphens, dots, spaces
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' ||
            c == '.' || c == ' ') {
            out += c;
        }
    }
    return out;
}

// ── sendTelemetry (private) ──────────────────────────────────────────────

void TelemetryCollector::sendTelemetry(const JsonObject& payload) {
    std::string endpoint = getEnvVar("RAWRXD_TELEMETRY_URL");
    if (endpoint.empty()) {
        // No endpoint configured – just log locally
        fprintf(stderr, "[Telemetry] (no endpoint) payload: %s\n",
                payload.dump().c_str());
        return;
    }

    std::string body = payload.dump();

    http::Response resp = http::post(endpoint, body, {
        {"Content-Type", "application/json"},
        {"X-Session-Id", m_sessionId}
    });

    if (!resp.ok()) {
        fprintf(stderr, "[Telemetry] HTTP %d: %s\n",
                resp.statusCode, resp.error.c_str());
    }
}

// ── loadUserConsent (private) ────────────────────────────────────────────

bool TelemetryCollector::loadUserConsent() const {
    std::string path = consentFilePath();
    if (!fs::exists(path)) return false; // opt-in: default off

    std::string content = fileutil::readAll(path);
    // Trim whitespace
    while (!content.empty() && (content.back() == '\n' || content.back() == '\r' || content.back() == ' '))
        content.pop_back();

    return content == "1" || content == "true" || content == "yes";
}

// ── saveUserConsent (private) ────────────────────────────────────────────

void TelemetryCollector::saveUserConsent(bool enabled) {
    std::string path = consentFilePath();

    // Ensure directory exists
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);

    fileutil::writeAll(path, enabled ? "1" : "0");
}
