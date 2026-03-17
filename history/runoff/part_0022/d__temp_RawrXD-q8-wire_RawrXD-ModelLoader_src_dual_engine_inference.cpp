// dual_engine_inference.cpp - 800B Dual-Engine Inference System Implementation
// Phase 1 Stub: Core interface with TOGGLEABLE license gating
// When gate is disabled: fully usable without Enterprise license
// When gate is enabled: auto-bridges to key creation for seamless beacon

#include "dual_engine_inference.h"
#include <sstream>
#include <chrono>
#include <fstream>

// ============================================================================
// Singleton
// ============================================================================
DualEngineInferenceManager& DualEngineInferenceManager::getInstance() {
    static DualEngineInferenceManager instance;
    return instance;
}

DualEngineInferenceManager::DualEngineInferenceManager() = default;
DualEngineInferenceManager::~DualEngineInferenceManager() = default;

// ============================================================================
// Internal: Toggleable License Gate
// Returns ok() if gate is disabled OR if license is valid/beaconed.
// When gate is enabled and no license exists, auto-bridges to key creator.
// ============================================================================
LicenseResult DualEngineInferenceManager::checkLicenseGate(EnterpriseFeature feature) {
    // Gate disabled — no license required, fully usable
    if (!m_enterpriseGateEnabled.load()) {
        return LicenseResult::ok("License gate disabled — feature unrestricted");
    }

    // Gate enabled — check if already beaconed
    if (m_licenseBeaconed.load()) {
        // Already bridged — verify still valid
        if (EnterpriseLicenseManager::getInstance().isFeatureUnlocked(feature)) {
            return LicenseResult::ok("Enterprise license valid (beaconed)");
        }
    }

    // Gate enabled but not yet beaconed — auto-bridge to key creator
    LicenseResult beaconResult = beaconEnterpriseLicense();
    if (!beaconResult.success) {
        return LicenseResult::error("Enterprise gate enabled but auto-beacon failed — "
                                    "use setEnterpriseGateEnabled(false) to disable, "
                                    "or manually create a license key");
    }

    // Re-check after beacon
    if (EnterpriseLicenseManager::getInstance().isFeatureUnlocked(feature)) {
        return LicenseResult::ok("Enterprise license activated via auto-beacon");
    }

    return LicenseResult::error("Feature requires Enterprise license and beacon did not unlock it");
}

// ============================================================================
// License Gate Control
// ============================================================================
LicenseResult DualEngineInferenceManager::setEnterpriseGateEnabled(bool enabled) {
    bool wasEnabled = m_enterpriseGateEnabled.exchange(enabled);

    if (enabled && !wasEnabled) {
        // Transitioning to gated mode — auto-beacon immediately
        LicenseResult r = beaconEnterpriseLicense();
        if (!r.success) {
            return LicenseResult::error("Enterprise gate enabled but auto-beacon failed — "
                                        "dual-engine will still function if beacon succeeds later");
        }
        return LicenseResult::ok("Enterprise gate enabled — license beaconed successfully");
    }

    if (!enabled && wasEnabled) {
        return LicenseResult::ok("Enterprise gate disabled — dual-engine fully unrestricted");
    }

    return LicenseResult::ok(enabled ? "Enterprise gate already enabled" 
                                     : "Enterprise gate already disabled");
}

bool DualEngineInferenceManager::isEnterpriseGateEnabled() const {
    return m_enterpriseGateEnabled.load();
}

bool DualEngineInferenceManager::isLicenseBeaconed() const {
    return m_licenseBeaconed.load();
}

// ============================================================================
// Auto-Beacon: Bridge to EnterpriseLicenseManager key creation
// Creates an Enterprise-tier key, activates it, and optionally writes to disk.
// This beacons the entire system together via the license manager singleton.
// ============================================================================
LicenseResult DualEngineInferenceManager::beaconEnterpriseLicense() {
    if (m_licenseBeaconed.load()) {
        return LicenseResult::ok("Already beaconed");
    }

    auto& licMgr = EnterpriseLicenseManager::getInstance();

    // Initialize the license manager if needed
    LicenseResult initResult = licMgr.initialize();
    // Ignore if already initialized

    // Create an Enterprise-tier license key via the built-in key creator
    LicenseKey key = EnterpriseLicenseManager::createLicenseKey(
        m_config.licensee.empty() ? "RawrXD-DualEngine-AutoBeacon" : m_config.licensee,
        m_config.licenseEmail.empty() ? "auto@rawrxd.local" : m_config.licenseEmail,
        LicenseTier::Enterprise,
        1,  // seatCount
        m_config.licenseValidDays > 0 ? m_config.licenseValidDays : 365,
        EnterpriseLicenseManager::generateMachineFingerprint()
    );

    // Sign the key
    key.signature = EnterpriseLicenseManager::signLicenseKey(key);

    // Serialize and activate
    std::string serialized = EnterpriseLicenseManager::serializeLicenseKey(key);
    LicenseResult activateResult = licMgr.activateLicense(serialized);
    if (!activateResult.success) {
        return LicenseResult::error("Auto-beacon: key creation succeeded but activation failed");
    }

    // Optionally persist to disk
    if (!m_config.licenseOutputPath.empty()) {
        LicenseResult saveResult = licMgr.saveLicense(m_config.licenseOutputPath);
        // Non-fatal if save fails — license is active in memory
        (void)saveResult;
    }

    m_licenseBeaconed.store(true);
    return LicenseResult::ok("Enterprise license auto-beaconed — system bridged");
}

// ============================================================================
// Lifecycle
// ============================================================================
LicenseResult DualEngineInferenceManager::initialize(const DualEngineConfig& config) {
    // Toggleable license gate — replaces hard RAWRXD_REQUIRE_ENTERPRISE
    {
        // Apply gate setting from config before checking
        m_enterpriseGateEnabled.store(config.requireEnterpriseLicense);
        LicenseResult gateResult = checkLicenseGate(EnterpriseFeature::DualEngineInference);
        if (!gateResult.success) return gateResult;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized.load()) {
        return LicenseResult::ok("Dual-Engine already initialized");
    }

    m_config = config;

    // Initialize primary engine status
    m_primaryStatus.state = EngineState::Uninitialized;
    m_primaryStatus.gpuIndex = config.primaryGPUIndex;

    // Initialize secondary engine status
    m_secondaryStatus.state = EngineState::Uninitialized;
    m_secondaryStatus.gpuIndex = config.secondaryGPUIndex;

    m_initialized.store(true);
    return LicenseResult::ok("Dual-Engine initialized (awaiting model load)");
}

LicenseResult DualEngineInferenceManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_primaryStatus.state = EngineState::Shutdown;
    m_secondaryStatus.state = EngineState::Shutdown;
    m_initialized.store(false);

    if (m_statusCb) {
        m_statusCb(EngineState::Shutdown, EngineState::Shutdown);
    }

    return LicenseResult::ok("Dual-Engine shut down");
}

// ============================================================================
// Model Loading (Phase 1: Stub — validates license + reports status)
// ============================================================================
LicenseResult DualEngineInferenceManager::loadPrimaryModel(const std::string& modelPath) {
    // Toggleable license gate
    {
        LicenseResult gateResult = checkLicenseGate(EnterpriseFeature::Engine800B);
        if (!gateResult.success) return gateResult;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_primaryStatus.state = EngineState::Loading;
    m_primaryStatus.modelName = modelPath;

    if (m_statusCb) {
        m_statusCb(EngineState::Loading, m_secondaryStatus.state);
    }

    // Phase 1: Stub — mark as ready without actual GPU loading
    // TODO Phase 2: Wire to actual GGUF loader with GPU memory mapping
    m_primaryStatus.state = EngineState::Ready;
    m_primaryStatus.loadedParametersMB = 0; // Will be populated by actual loader

    if (m_statusCb) {
        m_statusCb(EngineState::Ready, m_secondaryStatus.state);
    }

    return LicenseResult::ok("Primary engine model loaded (stub)");
}

LicenseResult DualEngineInferenceManager::loadSecondaryModel(const std::string& modelPath) {
    // Toggleable license gate
    {
        LicenseResult gateResult = checkLicenseGate(EnterpriseFeature::Engine800B);
        if (!gateResult.success) return gateResult;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_secondaryStatus.state = EngineState::Loading;
    m_secondaryStatus.modelName = modelPath;

    if (m_statusCb) {
        m_statusCb(m_primaryStatus.state, EngineState::Loading);
    }

    // Phase 1: Stub
    m_secondaryStatus.state = EngineState::Ready;
    m_secondaryStatus.loadedParametersMB = 0;

    if (m_statusCb) {
        m_statusCb(m_primaryStatus.state, EngineState::Ready);
    }

    return LicenseResult::ok("Secondary engine model loaded (stub)");
}

LicenseResult DualEngineInferenceManager::unloadAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_primaryStatus = EngineStatus{};
    m_secondaryStatus = EngineStatus{};
    return LicenseResult::ok("All models unloaded");
}

// ============================================================================
// Inference (Phase 1: Stub with license validation)
// ============================================================================
LicenseResult DualEngineInferenceManager::runInference(
    const InferenceRequest& request, InferenceResponse& response)
{
    // Toggleable license gate
    {
        LicenseResult gateResult = checkLicenseGate(EnterpriseFeature::DualEngineInference);
        if (!gateResult.success) return gateResult;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_primaryStatus.state != EngineState::Ready &&
        m_secondaryStatus.state != EngineState::Ready) {
        response.success = false;
        response.errorDetail = "No engines are in Ready state";
        return LicenseResult::error("No engines ready for inference");
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Phase 1: Stub response
    m_primaryStatus.state = EngineState::Running;
    if (m_secondaryStatus.state == EngineState::Ready) {
        m_secondaryStatus.state = EngineState::Running;
    }

    // TODO Phase 2: Actual dual-engine tensor parallel inference
    // 1. Split prompt across engines
    // 2. Each engine processes its tensor shard
    // 3. Sync intermediate states every N layers
    // 4. Merge final logits
    // 5. Sample and stream tokens

    response.success = true;
    response.output = "[Dual-Engine 800B Inference Stub] Model ready for inference. "
                      "Prompt received: " + std::to_string(request.prompt.size()) + " chars. "
                      "This is a Phase 1 stub — actual inference will be wired in Phase 2.";
    response.tokensGenerated = 0;

    auto end = std::chrono::high_resolution_clock::now();
    response.latencyMs = std::chrono::duration<float, std::milli>(end - start).count();

    // Reset engine states  
    m_primaryStatus.state = EngineState::Ready;
    m_secondaryStatus.state = (m_secondaryStatus.modelName.empty()) ?
                               EngineState::Uninitialized : EngineState::Ready;

    if (m_statusCb) {
        m_statusCb(m_primaryStatus.state, m_secondaryStatus.state);
    }

    return LicenseResult::ok("Inference completed (stub)");
}

// ============================================================================
// Status
// ============================================================================
EngineStatus DualEngineInferenceManager::getPrimaryStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_primaryStatus;
}

EngineStatus DualEngineInferenceManager::getSecondaryStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_secondaryStatus;
}

bool DualEngineInferenceManager::isDualEngineActive() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_primaryStatus.state == EngineState::Ready &&
           m_secondaryStatus.state == EngineState::Ready;
}

bool DualEngineInferenceManager::is800BModelLoaded() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_primaryStatus.state != EngineState::Uninitialized &&
           m_secondaryStatus.state != EngineState::Uninitialized;
}

std::string DualEngineInferenceManager::getStatusSummary() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream ss;
    ss << "800B Dual-Engine Status:\n";
    ss << "  Primary:   [" << EngineStateToString(m_primaryStatus.state) << "] "
       << m_primaryStatus.modelName << " (GPU " << m_primaryStatus.gpuIndex << ")\n";
    ss << "  Secondary: [" << EngineStateToString(m_secondaryStatus.state) << "] "
       << m_secondaryStatus.modelName << " (GPU " << m_secondaryStatus.gpuIndex << ")\n";
    ss << "  Dual Active: " << (isDualEngineActive() ? "YES" : "NO") << "\n";
    ss << "  License Gate: " << (m_enterpriseGateEnabled.load() ? "ENABLED" : "DISABLED (unrestricted)") << "\n";
    ss << "  License Beaconed: " << (m_licenseBeaconed.load() ? "YES" : "NO") << "\n";
    return ss.str();
}

// ============================================================================
// Callbacks
// ============================================================================
void DualEngineInferenceManager::setTokenCallback(TokenCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tokenCb = std::move(cb);
}

void DualEngineInferenceManager::setStatusCallback(StatusCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statusCb = std::move(cb);
}
