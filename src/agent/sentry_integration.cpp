/**
 * @file sentry_integration.cpp
 * @brief SentryIntegration implementation – Qt-free (C++20 / Win32)
 *
 * Sends crash reports, performance transactions, and breadcrumbs
 * to Sentry via WinHTTP (through process_utils.hpp http:: namespace).
 */

#include "sentry_integration.hpp"
#include "process_utils.hpp"
#include "../json_types.hpp"
#include <cstdio>
#include <chrono>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>     // CoCreateGuid
#pragma comment(lib, "ole32.lib")
#endif

// ── Helpers ──────────────────────────────────────────────────────────────

static int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

static std::string generateEventId() {
#ifdef _WIN32
    GUID guid{};
    CoCreateGuid(&guid);
    char buf[64];
    snprintf(buf, sizeof(buf),
             "%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
             guid.Data1, guid.Data2, guid.Data3,
             guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
             guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return buf;
#else
    // Simple fallback: timestamp + random
    char buf[64];
    snprintf(buf, sizeof(buf), "%016llx%08x",
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

// ── Singleton ────────────────────────────────────────────────────────────

SentryIntegration* SentryIntegration::s_instance = nullptr;

SentryIntegration* SentryIntegration::instance() {
    if (!s_instance) s_instance = new SentryIntegration();
    return s_instance;
}

SentryIntegration::SentryIntegration()  = default;
SentryIntegration::~SentryIntegration() = default;

// ── initialize ───────────────────────────────────────────────────────────

bool SentryIntegration::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

    m_dsn = getEnvVar("SENTRY_DSN");
    if (m_dsn.empty()) {
        fprintf(stderr, "[Sentry] SENTRY_DSN not set – crash reporting disabled\n");
        // Not fatal: run without sentry
    }

    m_initialized = true;
    fprintf(stderr, "[Sentry] Initialized (dsn=%s)\n",
            m_dsn.empty() ? "<none>" : "***");
    return true;
}

// ── captureException ─────────────────────────────────────────────────────

void SentryIntegration::captureException(const std::string& exception,
                                         const JsonObject& context) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string eventId = generateEventId();

    JsonObject event;
    event["event_id"]  = JsonValue(eventId);
    event["timestamp"] = JsonValue(isoTimestamp());
    event["level"]     = JsonValue("error");
    event["platform"]  = JsonValue("native");

    // Exception payload
    JsonObject exVal;
    exVal["type"]  = JsonValue("RuntimeError");
    exVal["value"] = JsonValue(exception);

    JsonArray exValues;
    exValues.push_back(JsonValue(std::move(exVal)));

    JsonObject exBlock;
    exBlock["values"] = JsonValue(std::move(exValues));
    event["exception"] = JsonValue(std::move(exBlock));

    // Merge user context
    if (!context.empty()) {
        event["extra"] = JsonValue(context);
    }

    // Attach breadcrumbs
    if (!m_breadcrumbs.empty()) {
        JsonArray crumbs;
        for (auto& bc : m_breadcrumbs)
            crumbs.push_back(JsonValue(bc));
        event["breadcrumbs"] = JsonValue(std::move(crumbs));
    }

    sendEvent(event);

    if (m_errCb) m_errCb(m_errCtx, eventId.c_str());

    fprintf(stderr, "[Sentry] Exception captured: %s (id=%s)\n",
            exception.c_str(), eventId.c_str());
}

// ── captureMessage ───────────────────────────────────────────────────────

void SentryIntegration::captureMessage(const std::string& message,
                                       const std::string& level) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string eventId = generateEventId();

    JsonObject event;
    event["event_id"]  = JsonValue(eventId);
    event["timestamp"] = JsonValue(isoTimestamp());
    event["level"]     = JsonValue(level);
    event["message"]   = JsonValue(message);
    event["platform"]  = JsonValue("native");

    sendEvent(event);
    fprintf(stderr, "[Sentry] Message: [%s] %s\n", level.c_str(), message.c_str());
}

// ── addBreadcrumb ────────────────────────────────────────────────────────

void SentryIntegration::addBreadcrumb(const std::string& message,
                                      const std::string& category) {
    std::lock_guard<std::mutex> lock(m_mutex);

    JsonObject crumb;
    crumb["timestamp"] = JsonValue(isoTimestamp());
    crumb["message"]   = JsonValue(message);
    crumb["category"]  = JsonValue(category);

    m_breadcrumbs.push_back(std::move(crumb));

    // Cap at 100 breadcrumbs
    while (m_breadcrumbs.size() > 100) {
        m_breadcrumbs.erase(m_breadcrumbs.begin());
    }
}

// ── startTransaction ─────────────────────────────────────────────────────

std::string SentryIntegration::startTransaction(const std::string& operation) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string txId = generateEventId();
    m_activeTransactions[txId] = nowMs();

    fprintf(stderr, "[Sentry] Transaction started: %s (op=%s)\n",
            txId.c_str(), operation.c_str());
    return txId;
}

// ── finishTransaction ────────────────────────────────────────────────────

void SentryIntegration::finishTransaction(const std::string& transactionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_activeTransactions.find(transactionId);
    if (it == m_activeTransactions.end()) {
        fprintf(stderr, "[Sentry] finishTransaction: unknown id %s\n",
                transactionId.c_str());
        return;
    }

    int64_t startMs   = it->second;
    int64_t durationMs = nowMs() - startMs;
    m_activeTransactions.erase(it);

    // Send transaction event
    JsonObject event;
    event["event_id"]  = JsonValue(transactionId);
    event["type"]      = JsonValue("transaction");
    event["timestamp"] = JsonValue(isoTimestamp());
    event["platform"]  = JsonValue("native");

    JsonObject spans;
    spans["duration_ms"] = JsonValue(durationMs);
    event["spans"] = JsonValue(std::move(spans));

    sendEvent(event);

    if (m_txCb) m_txCb(m_txCtx, transactionId.c_str(), durationMs);

    fprintf(stderr, "[Sentry] Transaction finished: %s (%lld ms)\n",
            transactionId.c_str(), (long long)durationMs);
}

// ── setUser ──────────────────────────────────────────────────────────────

void SentryIntegration::setUser(const std::string& userId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Store for inclusion in future events (breadcrumb approach)
    addBreadcrumb("User set: " + userId, "auth");
}

// ── sendEvent (private) ──────────────────────────────────────────────────

void SentryIntegration::sendEvent(const JsonObject& event) {
    if (m_dsn.empty()) {
        // No DSN configured – log only
        fprintf(stderr, "[Sentry] (no DSN) event: %s\n",
                JsonDoc::toJson(event).c_str());
        return;
    }

    // Parse DSN: https://<key>@<host>/<project_id>
    // Format: https://KEY@HOST/PROJECT_ID
    std::string dsn = m_dsn;
    std::string key, host, projectId;

    // Strip scheme
    size_t schemeEnd = dsn.find("://");
    if (schemeEnd != std::string::npos) dsn = dsn.substr(schemeEnd + 3);

    // key@host/project
    size_t atPos = dsn.find('@');
    if (atPos != std::string::npos) {
        key = dsn.substr(0, atPos);
        std::string rest = dsn.substr(atPos + 1);
        size_t slashPos = rest.find('/');
        if (slashPos != std::string::npos) {
            host      = rest.substr(0, slashPos);
            projectId = rest.substr(slashPos + 1);
        }
    }

    if (key.empty() || host.empty() || projectId.empty()) {
        fprintf(stderr, "[Sentry] Invalid DSN format\n");
        return;
    }

    std::string url = "https://" + host + "/api/" + projectId + "/store/";
    std::string payload = JsonDoc::toJson(event);

    http::Response resp = http::post(url, payload, {
        {"Content-Type",          "application/json"},
        {"X-Sentry-Auth",
         "Sentry sentry_version=7, sentry_client=rawrxd/1.0, sentry_key=" + key}
    });

    if (!resp.ok()) {
        fprintf(stderr, "[Sentry] HTTP %d: %s\n",
                resp.statusCode, resp.error.c_str());
    }
}
