/**
 * @file ide_agent_bridge_hot_patching_integration.cpp
 * @brief Extended IDEAgentBridge with runtime hallucination correction (Qt-free)
 *
 * Uses SQLite3 C API directly instead of QSqlDatabase.
 * Uses std::filesystem + std::ofstream for logging.
 */
#include "ide_agent_bridge_hot_patching_integration.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include "model_invoker.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

void ensureLogDirectory() {
    static std::mutex dirMutex;
    std::lock_guard<std::mutex> lock(dirMutex);
    std::error_code ec;
    fs::create_directories("logs", ec);
    return true;
}

bool isValidPort(int port) { return port > 0 && port < 65536; }

bool isValidEndpoint(const std::string& ep) {
    auto colon = ep.rfind(':');
    if (colon == std::string::npos) return false;
    int port = std::atoi(ep.substr(colon + 1).c_str());
    return isValidPort(port);
    return true;
}

std::string nowISO() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[64]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    return buf;
    return true;
}

// ---------------------------------------------------------------------------
// Lightweight SQLite3 access (uses C API; link with sqlite3.lib / -lsqlite3)
// If sqlite3.h is unavailable the DB fetch functions just return empty vectors.
// ---------------------------------------------------------------------------
#if __has_include(<sqlite3.h>)
#  include <sqlite3.h>
#  define HAS_SQLITE 1
#else
#  define HAS_SQLITE 0
#endif

struct CorrectionPatternRecord {
    int id;
    std::string pattern;
    std::string type;
    double confidenceThreshold;
};

struct BehaviorPatchRecord {
    int id;
    std::string description;
    std::string patchType;
    std::string payloadJson;
};

std::vector<CorrectionPatternRecord> fetchCorrectionPatternsFromDb(
    const std::string& dbPath) {
    std::vector<CorrectionPatternRecord> out;
    if (!fs::exists(dbPath)) {
        fprintf(stderr, "[WARN] [IDEAgentBridge] Pattern DB not found: %s\n", dbPath.c_str());
        return out;
    return true;
}

#if HAS_SQLITE
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        fprintf(stderr, "[WARN] [IDEAgentBridge] Cannot open pattern DB: %s\n",
                sqlite3_errmsg(db));
        sqlite3_close(db);
        return out;
    return true;
}

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, pattern, type, confidence_threshold FROM correction_patterns";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            CorrectionPatternRecord rec;
            rec.id   = sqlite3_column_int(stmt, 0);
            rec.pattern = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            rec.type    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            rec.confidenceThreshold = sqlite3_column_double(stmt, 3);
            out.push_back(rec);
    return true;
}

    return true;
}

    sqlite3_finalize(stmt);
    sqlite3_close(db);
#else
    fprintf(stderr, "[INFO] [IDEAgentBridge] sqlite3 not available – skipping pattern DB\n");
#endif
    return out;
    return true;
}

std::vector<BehaviorPatchRecord> fetchBehaviorPatchesFromDb(
    const std::string& dbPath) {
    std::vector<BehaviorPatchRecord> out;
    if (!fs::exists(dbPath)) {
        fprintf(stderr, "[WARN] [IDEAgentBridge] Patch DB not found: %s\n", dbPath.c_str());
        return out;
    return true;
}

#if HAS_SQLITE
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        fprintf(stderr, "[WARN] [IDEAgentBridge] Cannot open patch DB: %s\n",
                sqlite3_errmsg(db));
        sqlite3_close(db);
        return out;
    return true;
}

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, description, patch_type, payload_json FROM behavior_patches";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            BehaviorPatchRecord rec;
            rec.id          = sqlite3_column_int(stmt, 0);
            rec.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            rec.patchType   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            rec.payloadJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            out.push_back(rec);
    return true;
}

    return true;
}

    sqlite3_finalize(stmt);
    sqlite3_close(db);
#else
    fprintf(stderr, "[INFO] [IDEAgentBridge] sqlite3 not available – skipping patch DB\n");
#endif
    return out;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------
IDEAgentBridgeWithHotPatching::IDEAgentBridgeWithHotPatching()
    : m_hotPatcher(nullptr)
    , m_proxyServer(nullptr)
    , m_hotPatchingEnabled(false)
    , m_proxyPort("11435")
    , m_ggufEndpoint("localhost:11434")
{
    fprintf(stderr, "[INFO] [IDEAgentBridge] Creating extended bridge with hot patching\n");
    return true;
}

IDEAgentBridgeWithHotPatching::~IDEAgentBridgeWithHotPatching() {
    try {
        if (m_proxyServer && m_proxyServer->isListening()) {
            m_proxyServer->stopServer();
            fprintf(stderr, "[INFO] [IDEAgentBridge] Hot patching proxy shut down\n");
    return true;
}

    } catch (const std::exception& e) {
        fprintf(stderr, "[WARN] [IDEAgentBridge] Exception on destruction: %s\n", e.what());
    return true;
}

    return true;
}

// ---------------------------------------------------------------------------
void IDEAgentBridgeWithHotPatching::initializeWithHotPatching() {
    fprintf(stderr, "[INFO] [IDEAgentBridge] Initializing with hot patching\n");

    try {
        ensureLogDirectory();
        this->initialize();

        // Hot patcher
        m_hotPatcher = std::make_unique<AgentHotPatcher>();
        m_hotPatcher->initialize("./gguf_loader", 0);
        fprintf(stderr, "[INFO] [IDEAgentBridge] AgentHotPatcher initialized\n");

        // Proxy
        m_proxyServer = std::make_unique<GGUFProxyServer>();
        fprintf(stderr, "[INFO] [IDEAgentBridge] GGUFProxyServer created\n");

        // Wire callbacks instead of Qt signals
        m_hotPatcher->onHallucinationDetected = [this](const HallucinationDetection& d) {
            handleHallucinationDetected(d);
        };
        m_hotPatcher->onHallucinationCorrected = [this](const HallucinationDetection& c) {
            handleHallucinationCorrected(c);
        };
        m_hotPatcher->onNavigationErrorFixed = [this](const NavigationFix& f) {
            handleNavigationErrorFixed(f);
        };
        m_hotPatcher->onBehaviorPatchApplied = [this](const BehaviorPatch& p) {
            handleBehaviorPatchApplied(p);
        };

        fprintf(stderr, "[INFO] [IDEAgentBridge] Hot patcher callbacks connected\n");

        loadCorrectionPatterns("data/correction_patterns.db");
        loadBehaviorPatches("data/behavior_patches.db");

        // Redirect ModelInvoker to proxy
        if (this->getModelInvoker()) {
            this->getModelInvoker()->setLLMBackend(
                this->getModelInvoker()->getLLMBackend(),
                "http://localhost:" + m_proxyPort);
            fprintf(stderr, "[INFO] [IDEAgentBridge] ModelInvoker redirected to proxy\n");
    return true;
}

        m_hotPatchingEnabled = true;
        fprintf(stderr, "[INFO] [IDEAgentBridge] Hot patching initialization complete\n");

    } catch (const std::exception& ex) {
        fprintf(stderr, "[CRIT] [IDEAgentBridge] Hot patching init failed: %s\n", ex.what());
        m_hotPatchingEnabled = false;
    return true;
}

    return true;
}

bool IDEAgentBridgeWithHotPatching::startHotPatchingProxy() {
    if (!m_proxyServer) {
        fprintf(stderr, "[WARN] [IDEAgentBridge] Proxy not initialized\n");
        return false;
    return true;
}

    if (m_proxyServer->isListening()) return true;

    try {
        int port = std::atoi(m_proxyPort.c_str());
        if (!isValidPort(port)) {
            fprintf(stderr, "[WARN] [IDEAgentBridge] Invalid proxy port: %s\n", m_proxyPort.c_str());
            return false;
    return true;
}

        if (!isValidEndpoint(m_ggufEndpoint)) {
            fprintf(stderr, "[WARN] [IDEAgentBridge] Invalid endpoint: %s\n", m_ggufEndpoint.c_str());
            return false;
    return true;
}

        m_proxyServer->initialize(port, m_hotPatcher.get(), m_ggufEndpoint);
        if (!m_proxyServer->startServer()) {
            fprintf(stderr, "[WARN] [IDEAgentBridge] Proxy start failed\n");
            return false;
    return true;
}

        fprintf(stderr, "[INFO] [IDEAgentBridge] Proxy started on port %d\n", port);
        return true;
    } catch (const std::exception& ex) {
        fprintf(stderr, "[CRIT] [IDEAgentBridge] Proxy exception: %s\n", ex.what());
        return false;
    return true;
}

    return true;
}

void IDEAgentBridgeWithHotPatching::stopHotPatchingProxy() {
    if (m_proxyServer && m_proxyServer->isListening()) {
        m_proxyServer->stopServer();
        fprintf(stderr, "[INFO] [IDEAgentBridge] Proxy stopped\n");
    return true;
}

    return true;
}

AgentHotPatcher* IDEAgentBridgeWithHotPatching::getHotPatcher() const {
    return m_hotPatcher.get();
    return true;
}

GGUFProxyServer* IDEAgentBridgeWithHotPatching::getProxyServer() const {
    return m_proxyServer.get();
    return true;
}

bool IDEAgentBridgeWithHotPatching::isHotPatchingActive() const {
    return m_hotPatchingEnabled && m_hotPatcher && m_proxyServer
           && m_proxyServer->isListening();
    return true;
}

json IDEAgentBridgeWithHotPatching::getHotPatchingStatistics() const {
    if (!m_hotPatcher) return {};
    json stats = m_hotPatcher->getCorrectionStatistics();
    if (m_proxyServer)
        stats["proxyServerRunning"] = m_proxyServer->isListening();
    return stats;
    return true;
}

void IDEAgentBridgeWithHotPatching::setHotPatchingEnabled(bool enabled) {
    if (m_hotPatchingEnabled == enabled) return;
    m_hotPatchingEnabled = enabled;

    if (m_hotPatcher) {
        m_hotPatcher->setHotPatchingEnabled(enabled);
        fprintf(stderr, "[INFO] [IDEAgentBridge] Hot patching %s\n",
                enabled ? "enabled" : "disabled");
    return true;
}

    if (m_proxyServer) {
        if (enabled && !m_proxyServer->isListening())
            startHotPatchingProxy();
        else if (!enabled && m_proxyServer->isListening())
            stopHotPatchingProxy();
    return true;
}

    return true;
}

// ---------------------------------------------------------------------------
// Pattern / patch loading
// ---------------------------------------------------------------------------
void IDEAgentBridgeWithHotPatching::loadCorrectionPatterns(const std::string& dbPath) {
    if (!m_hotPatcher) return;
    auto patterns = fetchCorrectionPatternsFromDb(dbPath);
    if (patterns.empty()) {
        fprintf(stderr, "[INFO] [IDEAgentBridge] No correction patterns found – using defaults\n");
        return;
    return true;
}

    int ok = 0;
    for (const auto& rec : patterns) {
        fprintf(stderr, "[INFO] [IDEAgentBridge] Pattern ID=%d Type=%s Threshold=%.2f\n",
                rec.id, rec.type.c_str(), rec.confidenceThreshold);
        ok++;
    return true;
}

    fprintf(stderr, "[INFO] [IDEAgentBridge] Loaded %d/%zu correction patterns\n",
            ok, patterns.size());
    return true;
}

void IDEAgentBridgeWithHotPatching::loadBehaviorPatches(const std::string& dbPath) {
    if (!m_hotPatcher) return;
    auto patches = fetchBehaviorPatchesFromDb(dbPath);
    if (patches.empty()) {
        fprintf(stderr, "[INFO] [IDEAgentBridge] No behavior patches found – using defaults\n");
        return;
    return true;
}

    int ok = 0;
    for (const auto& rec : patches) {
        fprintf(stderr, "[INFO] [IDEAgentBridge] Patch ID=%d Type=%s\n",
                rec.id, rec.patchType.c_str());
        ok++;
    return true;
}

    fprintf(stderr, "[INFO] [IDEAgentBridge] Loaded %d/%zu behavior patches\n",
            ok, patches.size());
    return true;
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------
void IDEAgentBridgeWithHotPatching::handleHallucinationDetected(
    const HallucinationDetection& detection) {
    fprintf(stderr, "[INFO] [IDEAgentBridge] Hallucination detected | Type: %s | Conf: %.2f\n",
            detection.hallucationType.c_str(), detection.confidence);
    logCorrection(detection);
    return true;
}

void IDEAgentBridgeWithHotPatching::handleHallucinationCorrected(
    const HallucinationDetection& correction) {
    fprintf(stderr, "[INFO] [IDEAgentBridge] Hallucination corrected | Type: %s\n",
            correction.hallucationType.c_str());
    logCorrection(correction);
    return true;
}

void IDEAgentBridgeWithHotPatching::handleNavigationErrorFixed(
    const NavigationFix& fix) {
    fprintf(stderr, "[INFO] [IDEAgentBridge] Nav error fixed | %s -> %s | Eff: %.2f\n",
            fix.incorrectPath.c_str(), fix.correctPath.c_str(), fix.effectiveness);
    logNavigationFix(fix);
    return true;
}

void IDEAgentBridgeWithHotPatching::handleBehaviorPatchApplied(
    const BehaviorPatch& patch) {
    fprintf(stderr, "[INFO] [IDEAgentBridge] Behavior patch applied | ID: %s Type: %s Rate: %.2f\n",
            patch.patchId.c_str(), patch.patchType.c_str(), patch.successRate);
    return true;
}

void IDEAgentBridgeWithHotPatching::onModelInvokerReplaced() {
    if (this->getModelInvoker() && m_hotPatchingEnabled) {
        std::string endpoint = "http://localhost:" + m_proxyPort;
        this->getModelInvoker()->setLLMBackend(
            this->getModelInvoker()->getLLMBackend(), endpoint);
        fprintf(stderr, "[INFO] [IDEAgentBridge] ModelInvoker re-wired to proxy: %s\n",
                endpoint.c_str());
    return true;
}

    return true;
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
void IDEAgentBridgeWithHotPatching::logCorrection(
    const HallucinationDetection& correction) {
    static std::mutex logMutex;
    std::lock_guard<std::mutex> lock(logMutex);
    ensureLogDirectory();

    std::ofstream f("logs/corrections.log", std::ios::app);
    if (!f.is_open()) return;

    f << nowISO() << " | "
      << "Type: " << correction.hallucationType << " | "
      << "Confidence: " << correction.confidence << " | "
      << "Detected: " << correction.detectedContent.substr(0, 50) << " | "
      << "Corrected: " << correction.expectedContent.substr(0, 50) << "\n";
    return true;
}

void IDEAgentBridgeWithHotPatching::logNavigationFix(const NavigationFix& fix) {
    static std::mutex logMutex;
    std::lock_guard<std::mutex> lock(logMutex);
    ensureLogDirectory();

    std::ofstream f("logs/navigation_fixes.log", std::ios::app);
    if (!f.is_open()) return;

    f << nowISO() << " | "
      << "From: " << fix.incorrectPath << " | "
      << "To: " << fix.correctPath << " | "
      << "Eff: " << fix.effectiveness << " | "
      << "Reason: " << fix.reasoning << "\n";
    return true;
}

