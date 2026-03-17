// ============================================================================
// File: src/agent/ide_agent_bridge_hot_patching_integration.cpp
// 
// Purpose: Integration implementation of hot patching into IDEAgentBridge
// Wires all components together for seamless hallucination correction
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#include "ide_agent_bridge_hot_patching_integration.hpp"
#include "model_invoker.hpp"

#include <memory>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <chrono>   
#include <algorithm>

using json = nlohmann::json;

// ============================================================================
// Helper: Ensure the logs/ directory exists (thread-safe)
// ============================================================================
static void ensureLogDirectory()
{
    static std::mutex dirMutex;
    std::lock_guard<std::mutex> locker(dirMutex);
    
    std::filesystem::path logDir("logs");
    std::error_code ec;
    if (!std::filesystem::exists(logDir)) {
        std::filesystem::create_directories(logDir, ec);
    }
}

// ============================================================================
// Validation helpers
// ============================================================================
static bool isValidPort(int port) { return port > 0 && port < 65536; }

static int stringToInt(const std::string& str, int defaultVal = -1) {
    try {
        return std::stoi(str);
    } catch (...) {
        return defaultVal;
    }
}

static bool isValidEndpoint(const std::string& ep)
{
    size_t colonPos = ep.find_last_of(':');
    if (colonPos == std::string::npos || colonPos == ep.length() - 1) {
        return false;
    }
    
    std::string portStr = ep.substr(colonPos + 1);
    int port = stringToInt(portStr);
    return isValidPort(port);
}

static std::string truncateString(const std::string& str, size_t maxLen) {
    if (str.length() <= maxLen) return str;
    return str.substr(0, maxLen);
}

// ============================================================================
// Helper: Current Time String
// ============================================================================
static std::string getCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// ============================================================================
// Helper: Load correction patterns from JSON
// ============================================================================
static std::vector<CorrectionPatternRecord> fetchCorrectionPatternsFromDb(
    const std::string& dbPath)
{
    std::vector<CorrectionPatternRecord> patterns;
    
    std::string jsonPath = dbPath;
    if (jsonPath.ends_with(".db")) {
        jsonPath.replace(jsonPath.length() - 3, 3, ".json");
    }

    if (!std::filesystem::exists(jsonPath)) {
        return patterns;
    }

    try {
        std::ifstream f(jsonPath);
        json j;
        f >> j;

        if (j.contains("correction_patterns") && j["correction_patterns"].is_array()) {
            for (const auto& item : j["correction_patterns"]) {
                CorrectionPatternRecord rec;
                rec.id = item.value("id", 0);
                rec.pattern = item.value("pattern", "");
                rec.type = item.value("type", "general");
                rec.confidenceThreshold = item.value("confidence_threshold", 0.8);
                patterns.push_back(rec);
            }
        }
    } catch (...) {
    }

    return patterns;
}

// ============================================================================
// Helper: Load behavior patches from JSON
// ============================================================================
static std::vector<BehaviorPatchRecord> fetchBehaviorPatchesFromDb(
    const std::string& dbPath)
{
    std::vector<BehaviorPatchRecord> patches;

    std::string jsonPath = dbPath;
    if (jsonPath.ends_with(".db")) {
        jsonPath.replace(jsonPath.length() - 3, 3, ".json");
    }

    if (!std::filesystem::exists(jsonPath)) {
        return patches;
    }

    try {
        std::ifstream f(jsonPath);
        json j;
        f >> j;
        
        if (j.contains("behavior_patches") && j["behavior_patches"].is_array()) {
            for (const auto& item : j["behavior_patches"]) {
                BehaviorPatchRecord rec;
                rec.id = item.value("id", 0);
                rec.description = item.value("description", "");
                rec.patchType = item.value("patch_type", "");
                rec.payloadJson = item.value("payload_json", "{}");
                patches.push_back(rec);
            }
        }
    } catch (...) {
    }

    return patches;
}

IDEAgentBridgeWithHotPatching::IDEAgentBridgeWithHotPatching()
    : IDEAgentBridge()
    , m_hotPatcher(nullptr)
    , m_proxyServer(nullptr)
    , m_hotPatchingEnabled(false)
    , m_proxyPort("11435")
    , m_ggufEndpoint("localhost:11434")
{
}

IDEAgentBridgeWithHotPatching::~IDEAgentBridgeWithHotPatching() noexcept
{
    try {
        if (m_proxyServer && m_proxyServer->isListening()) {
            m_proxyServer->stopServer();
        }
    } catch (...) {}
}

void IDEAgentBridgeWithHotPatching::initializeWithHotPatching()
{
    try {
        ensureLogDirectory();

        // Initialize parent class
        this->initialize("http://localhost:11434", "llama.cpp", "sk-none");

        // Create components
        m_hotPatcher = std::make_unique<AgentHotPatcher>();
        m_hotPatcher->initialize("./gguf_loader", 0);

        m_proxyServer = std::make_unique<GGUFProxyServer>();

        // Wire up callbacks
        m_hotPatcher->onHallucinationDetected = [this](const HallucinationDetection& d) {
            this->onHallucinationDetected(d);
        };
        m_hotPatcher->onHallucinationCorrected = [this](const HallucinationDetection& c) {
            this->onHallucinationCorrected(c);
        };
        m_hotPatcher->onNavigationFixApplied = [this](const NavigationFix& f) {
            this->onNavigationErrorFixed(f);
        };
        m_hotPatcher->onBehaviorPatchApplied = [this](const BehaviorPatch& p) {
            this->onBehaviorPatchApplied(p);
        };

        // Load data
        loadCorrectionPatterns("data/correction_patterns.json");
        loadBehaviorPatches("data/behavior_patches.json");

        // Start Proxy
        if (startHotPatchingProxy()) {
             // Redirect Parent Invoker to Proxy
             if (this->getModelInvoker()) {
                 this->getModelInvoker()->setEndpoint("http://localhost:" + m_proxyPort);
             }
             m_hotPatchingEnabled = true;
        } else {
             m_hotPatchingEnabled = false;
        }

    } catch (const std::exception& ex) {
        std::cerr << "Init failed: " << ex.what() << std::endl;
        m_hotPatchingEnabled = false;
    }
}

bool IDEAgentBridgeWithHotPatching::startHotPatchingProxy()
{
    if (!m_proxyServer) return false;
    if (m_proxyServer->isListening()) return true;

    try {
        int port = 0;
        try { port = std::stoi(m_proxyPort); } catch(...) { return false; }

        if (!isValidPort(port)) return false;
        
        m_proxyServer->initialize(port, m_hotPatcher.get(), m_ggufEndpoint);
        return m_proxyServer->startServer();
    } catch (...) {
        return false;
    }
}

void IDEAgentBridgeWithHotPatching::stopHotPatchingProxy()
{
    if (m_proxyServer && m_proxyServer->isListening()) {
        m_proxyServer->stopServer();
    }
}

AgentHotPatcher* IDEAgentBridgeWithHotPatching::getHotPatcher() const
{
    return m_hotPatcher.get();
}

GGUFProxyServer* IDEAgentBridgeWithHotPatching::getProxyServer() const
{
    return m_proxyServer.get();
}

bool IDEAgentBridgeWithHotPatching::isHotPatchingActive() const
{
    return m_hotPatchingEnabled && m_proxyServer && m_proxyServer->isListening();
}

json IDEAgentBridgeWithHotPatching::getHotPatchingStatistics() const
{
    if (!m_hotPatcher) return json::object();
    return m_hotPatcher->getCorrectionStatistics();
}

void IDEAgentBridgeWithHotPatching::setHotPatchingEnabled(bool enabled)
{
    if (m_hotPatchingEnabled == enabled) return;
    
    m_hotPatchingEnabled = enabled;
    if (m_hotPatcher) m_hotPatcher->setHotPatchingEnabled(enabled);
    
    if (m_proxyServer) {
        if (enabled && !m_proxyServer->isListening()) startHotPatchingProxy();
        else if (!enabled && m_proxyServer->isListening()) stopHotPatchingProxy();
    }
}

void IDEAgentBridgeWithHotPatching::loadCorrectionPatterns(const std::string& path)
{
    if (!m_hotPatcher) return;
    auto patterns = fetchCorrectionPatternsFromDb(path);
    for (const auto& rec : patterns) {
        HallucinationDetection h;
        h.detectionId = std::to_string(rec.id);
        h.hallucinationType = rec.type;
        h.detectedContent = rec.pattern;
        h.confidence = rec.confidenceThreshold;
        m_hotPatcher->registerCorrectionPattern(h);
    }
}

void IDEAgentBridgeWithHotPatching::loadBehaviorPatches(const std::string& path)
{
    if (!m_hotPatcher) return;
    auto patches = fetchBehaviorPatchesFromDb(path);
    for (const auto& rec : patches) {
        BehaviorPatch p;
        p.patchId = std::to_string(rec.id);
        p.patchType = rec.patchType;
        try {
            auto j = json::parse(rec.payloadJson);
            p.condition = j.value("condition", "");
            p.action = j.value("action", "");
            p.affectedModels = j.value("affectedModels", std::vector<std::string>{});
            p.successRate = j.value("successRate", 0.0);
            p.enabled = j.value("enabled", true);
            
            m_hotPatcher->createBehaviorPatch(p);
        } catch(...) {}
    }
}

void IDEAgentBridgeWithHotPatching::onModelInvokerReplaced()
{
    if (getModelInvoker() && isHotPatchingActive()) {
        getModelInvoker()->setEndpoint("http://localhost:" + m_proxyPort);
    }
}

void IDEAgentBridgeWithHotPatching::onHallucinationDetected(const HallucinationDetection& detection) {
    logCorrection(detection);
}

void IDEAgentBridgeWithHotPatching::onHallucinationCorrected(const HallucinationDetection& correction) {
    logCorrection(correction);
}

void IDEAgentBridgeWithHotPatching::onNavigationErrorFixed(const NavigationFix& fix) {
    logNavigationFix(fix);
}

void IDEAgentBridgeWithHotPatching::onBehaviorPatchApplied(const BehaviorPatch& patch) {
    // Optional logging
}

void IDEAgentBridgeWithHotPatching::proxyPortChanged() {
    if (isHotPatchingActive()) {
        stopHotPatchingProxy();
        startHotPatchingProxy();
        onModelInvokerReplaced();
    }
}

void IDEAgentBridgeWithHotPatching::ggufEndpointChanged() {
    if (isHotPatchingActive()) {
        stopHotPatchingProxy();
        startHotPatchingProxy();
    }
}

void IDEAgentBridgeWithHotPatching::logCorrection(const HallucinationDetection& correction)
{
    static std::mutex logMutex;
    std::lock_guard<std::mutex> locker(logMutex);
    ensureLogDirectory();
    std::ofstream logFile("logs/corrections.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << getCurrentTimeStr() << " | "
                << correction.hallucinationType << " | "
                << correction.confidence << " | "
                << truncateString(correction.detectedContent, 50) << "\n";
    }
}

void IDEAgentBridgeWithHotPatching::logNavigationFix(const NavigationFix& fix)
{
    static std::mutex logMutex;
    std::lock_guard<std::mutex> locker(logMutex);
    ensureLogDirectory();
    std::ofstream logFile("logs/navigation_fixes.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << getCurrentTimeStr() << " | "
                << fix.incorrectPath << " -> " << fix.correctPath << "\n";
    }
}
