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
}

bool isValidPort(int port) { return port > 0 && port < 65536; }

bool isValidEndpoint(const std::string& ep) {
    auto colon = ep.rfind(':');
    if (colon == std::string::npos) return false;
    int port = std::atoi(ep.substr(colon + 1).c_str());
    return isValidPort(port);
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
        return out;
    }
#if HAS_SQLITE
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return out;
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
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
#else
    (void)dbPath;
#endif
    return out;
}

std::vector<BehaviorPatchRecord> fetchBehaviorPatchesFromDb(
    const std::string& dbPath) {
    std::vector<BehaviorPatchRecord> out;
    if (!fs::exists(dbPath)) {
        return out;
    }
#if HAS_SQLITE
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return out;
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
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
#else
    (void)dbPath;
#endif
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------
IDEAgentBridgeWithHotPatching::IDEAgentBridgeWithHotPatching()
    : m_hotPatcher(nullptr)
    , m_proxyServer(nullptr)
    , m_hotPatchingEnabled(false)
    , m_proxyPort("11434")  // Default Ollama port
    , m_ggufEndpoint("localhost:11434")  // Default Ollama port
{
}

IDEAgentBridgeWithHotPatching::~IDEAgentBridgeWithHotPatching() {
    try {
        if (m_proxyServer && m_proxyServer->isListening()) {
            m_proxyServer->stopServer();
        }
    } catch (const std::exception& e) {
        (void)e;
    }
}

// ---------------------------------------------------------------------------
void IDEAgentBridgeWithHotPatching::initializeWithHotPatching() {
    try {
        ensureLogDirectory();
        this->initialize();

        // Hot patcher
        m_hotPatcher = std::make_unique<AgentHotPatcher>();
        m_hotPatcher->initialize("./gguf_loader", 0);

        // Proxy
        m_proxyServer = std::make_unique<GGUFProxyServer>();

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

        loadCorrectionPatterns("data/correction_patterns.db");
        loadBehaviorPatches("data/behavior_patches.db");

        // Redirect ModelInvoker to proxy
        if (this->getModelInvoker()) {
            this->getModelInvoker()->setLLMBackend(
                this->getModelInvoker()->getLLMBackend(),
                "http://localhost:" + m_proxyPort);
        }

        m_hotPatchingEnabled = true;

    } catch (const std::exception& ex) {
        (void)ex;
        m_hotPatchingEnabled = false;
    }
}

bool IDEAgentBridgeWithHotPatching::startHotPatchingProxy() {
    if (!m_proxyServer) {
        return false;
    }
    if (m_proxyServer->isListening()) return true;

    try {
        int port = std::atoi(m_proxyPort.c_str());
        if (!isValidPort(port)) {
            return false;
        }
        if (!isValidEndpoint(m_ggufEndpoint)) {
            return false;
        }

        m_proxyServer->initialize(port, m_hotPatcher.get(), m_ggufEndpoint);
        if (!m_proxyServer->startServer()) {
            return false;
        }

        return true;
    } catch (const std::exception& ex) {
        if (m_logger) {
            m_logger->error("HotPatchingProxy init failed: " + std::string(ex.what()));
        }
        return false;
    }
}

void IDEAgentBridgeWithHotPatching::stopHotPatchingProxy() {
    if (m_proxyServer && m_proxyServer->isListening()) {
        m_proxyServer->stopServer();
    }
}

AgentHotPatcher* IDEAgentBridgeWithHotPatching::getHotPatcher() const {
    return m_hotPatcher.get();
}

GGUFProxyServer* IDEAgentBridgeWithHotPatching::getProxyServer() const {
    return m_proxyServer.get();
}

bool IDEAgentBridgeWithHotPatching::isHotPatchingActive() const {
    return m_hotPatchingEnabled && m_hotPatcher && m_proxyServer
           && m_proxyServer->isListening();
}

json IDEAgentBridgeWithHotPatching::getHotPatchingStatistics() const {
    if (!m_hotPatcher) return {};
    json stats = m_hotPatcher->getCorrectionStatistics();
    if (m_proxyServer)
        stats["proxyServerRunning"] = m_proxyServer->isListening();
    return stats;
}

void IDEAgentBridgeWithHotPatching::setHotPatchingEnabled(bool enabled) {
    if (m_hotPatchingEnabled == enabled) return;
    m_hotPatchingEnabled = enabled;

    if (m_hotPatcher) {
        m_hotPatcher->setHotPatchingEnabled(enabled);
    }

    if (m_proxyServer) {
        if (enabled && !m_proxyServer->isListening())
            startHotPatchingProxy();
        else if (!enabled && m_proxyServer->isListening())
            stopHotPatchingProxy();
    }
}

// ---------------------------------------------------------------------------
// Pattern / patch loading
// ---------------------------------------------------------------------------
void IDEAgentBridgeWithHotPatching::loadCorrectionPatterns(const std::string& dbPath) {
    if (!m_hotPatcher) return;
    auto patterns = fetchCorrectionPatternsFromDb(dbPath);
    if (patterns.empty()) {
        return;
    }

    for (const auto& rec : patterns) {
        (void)rec;
    }
}

void IDEAgentBridgeWithHotPatching::loadBehaviorPatches(const std::string& dbPath) {
    if (!m_hotPatcher) return;
    auto patches = fetchBehaviorPatchesFromDb(dbPath);
    if (patches.empty()) {
        return;
    }

    for (const auto& rec : patches) {
        (void)rec;
    }
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------
void IDEAgentBridgeWithHotPatching::handleHallucinationDetected(
    const HallucinationDetection& detection) {
    (void)detection;
}

void IDEAgentBridgeWithHotPatching::handleHallucinationCorrected(
    const HallucinationDetection& correction) {
    (void)correction;
}

void IDEAgentBridgeWithHotPatching::handleNavigationErrorFixed(
    const NavigationFix& fix) {
    (void)fix;
}

void IDEAgentBridgeWithHotPatching::handleBehaviorPatchApplied(
    const BehaviorPatch& patch) {
    (void)patch;
}

void IDEAgentBridgeWithHotPatching::onModelInvokerReplaced() {
    if (this->getModelInvoker() && m_hotPatchingEnabled) {
        std::string endpoint = "http://localhost:" + m_proxyPort;
        this->getModelInvoker()->setLLMBackend(
            this->getModelInvoker()->getLLMBackend(), endpoint);
    }
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
}
