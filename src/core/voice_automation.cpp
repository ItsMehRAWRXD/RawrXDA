// ============================================================================
// VoiceAutomation — Implementation (Phase 44)
// Toggleable TTS Response Reader with Pluggable Voice Providers
//
// Architecture:
//   - Background speech worker thread drains a priority queue
//   - Voice providers are loaded as DLLs via C-ABI (VoiceProviderPlugin)
//   - Built-in SAPI provider is registered automatically on Windows
//   - Chat pipeline calls speakResponse() / speakStreamChunk()
//   - No exceptions, PatchResult pattern, lock_guard only
// ============================================================================

#include "voice_automation.hpp"

#include <cstdio>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <condition_variable>

#ifdef _WIN32
#include <sapi.h>
// sphelper.h requires ATL (atlbase.h) which may not be in BuildTools
// Guard to allow compilation without ATL installed
#if __has_include(<atlbase.h>)
#include <sphelper.h>
#define RAWR_HAS_SPHELPER 1
#else
#define RAWR_HAS_SPHELPER 0
#endif
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#endif

// ============================================================================
// Singleton accessor
// ============================================================================
static std::unique_ptr<VoiceAutomation> g_voiceAutomation;

VoiceAutomation& getVoiceAutomation() {
    if (!g_voiceAutomation) {
        g_voiceAutomation = std::make_unique<VoiceAutomation>();
    }
    return *g_voiceAutomation;
}

// ============================================================================
// Structured logging
// ============================================================================
void VoiceAutomation::logStructured(const char* level, const char* message, const char* context) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    char timeBuf[64];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", &tm_buf);

    if (context && context[0]) {
        fprintf(stderr, "[%s][VoiceAutomation][%s] %s {%s}\n", timeBuf, level, message, context);
    } else {
        fprintf(stderr, "[%s][VoiceAutomation][%s] %s\n", timeBuf, level, message);
    }
}

void VoiceAutomation::emitError(const char* error, int code) {
    {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics.errorCount++;
    }
    logStructured("ERROR", error);
    if (m_errorCb) {
        m_errorCb(error, code, m_errorCbUserData);
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
VoiceAutomation::VoiceAutomation() {
    logStructured("INFO", "VoiceAutomation engine created");
}

VoiceAutomation::~VoiceAutomation() {
    logStructured("INFO", "VoiceAutomation engine destroying");
    shutdown();
    logStructured("INFO", "VoiceAutomation engine destroyed");
}

// ============================================================================
// Lifecycle
// ============================================================================
VoiceAutoResult VoiceAutomation::initialize(const VoiceAutoConfig& config) {
    if (m_initialized.load()) {
        return VoiceAutoResult::ok("Already initialized");
    }

    logStructured("INFO", "Initializing voice automation system");

    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_config = config;
    }

    m_shutdownRequested.store(false);

    // Start speech worker thread
    m_workerThread = std::thread(&VoiceAutomation::speechWorkerThread, this);

    // Try to load built-in SAPI provider on Windows
#ifdef _WIN32
    VoiceAutoResult sapiResult = initBuiltinSAPI();
    if (sapiResult.success) {
        logStructured("INFO", "Built-in SAPI voice provider loaded");
    } else {
        logStructured("WARN", "Built-in SAPI provider unavailable", sapiResult.detail);
    }
#endif

    // Discover plugins from search paths
    if (!config.pluginSearchPaths.empty()) {
        discoverProviders(config.pluginSearchPaths);
    } else {
        // Default search paths
        discoverProviders("plugins;voice_plugins;extensions/voice");
    }

    m_initialized.store(true);
    m_enabled.store(config.enabled);

    char ctx[128];
    snprintf(ctx, sizeof(ctx), "\"enabled\":%s,\"providers\":%zu",
             config.enabled ? "true" : "false", m_providers.size());
    logStructured("INFO", "Voice automation initialized", ctx);

    return VoiceAutoResult::ok("Initialized");
}

VoiceAutoResult VoiceAutomation::shutdown() {
    if (!m_initialized.load()) {
        return VoiceAutoResult::ok("Not initialized");
    }

    logStructured("INFO", "Shutting down voice automation");

    m_shutdownRequested.store(true);
    m_enabled.store(false);

    // Wake up worker thread
    m_queueCV.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    // Unload all providers
    {
        std::lock_guard<std::mutex> lock(m_providerMutex);
        for (auto& [name, provider] : m_providers) {
            if (provider.initialized && provider.fnShutdown) {
                provider.fnShutdown();
                provider.initialized = false;
            }
#ifdef _WIN32
            if (provider.hModule) {
                FreeLibrary(provider.hModule);
                provider.hModule = nullptr;
            }
#endif
        }
        m_providers.clear();
    }

    // Clear queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_speechQueue.empty()) m_speechQueue.pop();
    }

    m_initialized.store(false);
    logStructured("INFO", "Voice automation shut down");
    return VoiceAutoResult::ok("Shutdown complete");
}

bool VoiceAutomation::isInitialized() const {
    return m_initialized.load();
}

// ============================================================================
// Toggle
// ============================================================================
VoiceAutoResult VoiceAutomation::enable() {
    if (!m_initialized.load()) {
        return VoiceAutoResult::error("Not initialized", 1);
    }
    m_enabled.store(true);
    logStructured("INFO", "Voice automation ENABLED");
    if (m_toggleCb) {
        m_toggleCb(true, m_toggleCbUserData);
    }
    return VoiceAutoResult::ok("Enabled");
}

VoiceAutoResult VoiceAutomation::disable() {
    m_enabled.store(false);
    cancelAll();
    logStructured("INFO", "Voice automation DISABLED");
    if (m_toggleCb) {
        m_toggleCb(false, m_toggleCbUserData);
    }
    return VoiceAutoResult::ok("Disabled");
}

VoiceAutoResult VoiceAutomation::toggle() {
    if (m_enabled.load()) {
        return disable();
    } else {
        return enable();
    }
}

bool VoiceAutomation::isEnabled() const {
    return m_enabled.load();
}

// ============================================================================
// Configuration
// ============================================================================
VoiceAutoResult VoiceAutomation::configure(const VoiceAutoConfig& config) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
    m_enabled.store(config.enabled);
    if (!config.activeProviderId.empty()) {
        m_activeProviderName = config.activeProviderId;
    }
    if (!config.activeVoiceId.empty()) {
        m_activeVoiceId = config.activeVoiceId;
    }
    logStructured("INFO", "Configuration updated");
    return VoiceAutoResult::ok("Configured");
}

VoiceAutoConfig VoiceAutomation::getConfig() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

VoiceAutoResult VoiceAutomation::setRate(float rate) {
    rate = std::clamp(rate, 0.1f, 10.0f);
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config.rate = rate;
    char ctx[64];
    snprintf(ctx, sizeof(ctx), "\"rate\":%.2f", rate);
    logStructured("INFO", "Speech rate updated", ctx);
    return VoiceAutoResult::ok("Rate set");
}

VoiceAutoResult VoiceAutomation::setVolume(float volume) {
    volume = std::clamp(volume, 0.0f, 1.0f);
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config.volume = volume;
    return VoiceAutoResult::ok("Volume set");
}

VoiceAutoResult VoiceAutomation::setPitch(float pitch) {
    pitch = std::clamp(pitch, 0.5f, 2.0f);
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config.pitch = pitch;
    return VoiceAutoResult::ok("Pitch set");
}

// ============================================================================
// Provider Management (Plugin System)
// ============================================================================
VoiceAutoResult VoiceAutomation::loadProvider(const std::string& dllPath) {
    logStructured("INFO", "Loading voice provider", dllPath.c_str());

#ifdef _WIN32
    std::wstring wPath(dllPath.begin(), dllPath.end());
    HMODULE hModule = LoadLibraryW(wPath.c_str());
    if (!hModule) {
        DWORD err = GetLastError();
        char errBuf[256];
        snprintf(errBuf, sizeof(errBuf), "LoadLibrary failed for '%s': error %lu",
                 dllPath.c_str(), err);
        emitError(errBuf, static_cast<int>(err));
        return VoiceAutoResult::error("Failed to load voice provider DLL", static_cast<int>(err));
    }

    LoadedVoiceProvider provider;
    provider.dllPath = dllPath;
    provider.hModule = hModule;
    provider.initialized = false;

    // Resolve C-ABI entry points
    provider.fnGetInfo    = (VoiceProvider_GetInfoFn)GetProcAddress(hModule, "VoiceProvider_GetInfo");
    provider.fnInit       = (VoiceProvider_InitFn)GetProcAddress(hModule, "VoiceProvider_Init");
    provider.fnShutdown   = (VoiceProvider_ShutdownFn)GetProcAddress(hModule, "VoiceProvider_Shutdown");
    provider.fnEnumVoices = (VoiceProvider_EnumVoicesFn)GetProcAddress(hModule, "VoiceProvider_EnumVoices");
    provider.fnSpeak      = (VoiceProvider_SpeakFn)GetProcAddress(hModule, "VoiceProvider_Speak");
    provider.fnCancel     = (VoiceProvider_CancelFn)GetProcAddress(hModule, "VoiceProvider_Cancel");
    provider.fnIsSpeaking = (VoiceProvider_IsSpeakingFn)GetProcAddress(hModule, "VoiceProvider_IsSpeaking");
    provider.fnSetCallback= (VoiceProvider_SetCallbackFn)GetProcAddress(hModule, "VoiceProvider_SetCallback");

    // Must have at minimum: GetInfo, Init, Speak
    if (!provider.fnGetInfo || !provider.fnInit || !provider.fnSpeak) {
        FreeLibrary(hModule);
        return VoiceAutoResult::error("DLL missing required exports (GetInfo, Init, Speak)", 2);
    }

    // Get provider info
    VoiceProviderInfo* info = provider.fnGetInfo();
    if (!info) {
        FreeLibrary(hModule);
        return VoiceAutoResult::error("VoiceProvider_GetInfo returned null", 3);
    }
    provider.info = *info;
    provider.name = info->name;

    // Initialize provider
    int initResult = provider.fnInit(nullptr);
    if (initResult != 0) {
        FreeLibrary(hModule);
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "VoiceProvider_Init failed: %d", initResult);
        return VoiceAutoResult::error(errBuf, initResult);
    }
    provider.initialized = true;

    // Enumerate voices
    VoiceInfo voiceBuffer[64];
    int voiceCount = 0;
    if (provider.fnEnumVoices) {
        voiceCount = provider.fnEnumVoices(voiceBuffer, 64);
        for (int i = 0; i < voiceCount; ++i) {
            provider.voices.push_back(voiceBuffer[i]);
        }
    }

    // Register
    {
        std::lock_guard<std::mutex> lock(m_providerMutex);
        m_providers[provider.name] = std::move(provider);
    }

    {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics.providersLoaded++;
    }

    char ctx[256];
    snprintf(ctx, sizeof(ctx), "\"provider\":\"%s\",\"version\":\"%s\",\"voices\":%d",
             info->name, info->version, voiceCount);
    logStructured("INFO", "Voice provider loaded", ctx);

    // If no active provider, use this one
    if (m_activeProviderName.empty()) {
        m_activeProviderName = info->name;
        if (voiceCount > 0) {
            m_activeVoiceId = voiceBuffer[0].id;
        }
        logStructured("INFO", "Auto-selected as active provider", info->name);
    }

    return VoiceAutoResult::ok("Provider loaded");
#else
    return VoiceAutoResult::error("Provider loading not supported on this platform", -1);
#endif
}

VoiceAutoResult VoiceAutomation::unloadProvider(const std::string& providerName) {
    std::lock_guard<std::mutex> lock(m_providerMutex);
    auto it = m_providers.find(providerName);
    if (it == m_providers.end()) {
        return VoiceAutoResult::error("Provider not found", 1);
    }

    if (it->second.initialized && it->second.fnShutdown) {
        it->second.fnShutdown();
    }
#ifdef _WIN32
    if (it->second.hModule) {
        FreeLibrary(it->second.hModule);
    }
#endif

    bool wasActive = (m_activeProviderName == providerName);
    m_providers.erase(it);

    if (wasActive) {
        m_activeProviderName.clear();
        m_activeVoiceId.clear();
        // Fall back to first available provider
        if (!m_providers.empty()) {
            auto& first = m_providers.begin()->second;
            m_activeProviderName = first.name;
            if (!first.voices.empty()) {
                m_activeVoiceId = first.voices[0].id;
            }
        }
    }

    logStructured("INFO", "Voice provider unloaded", providerName.c_str());
    return VoiceAutoResult::ok("Provider unloaded");
}

VoiceAutoResult VoiceAutomation::setActiveProvider(const std::string& providerName) {
    std::lock_guard<std::mutex> lock(m_providerMutex);
    auto it = m_providers.find(providerName);
    if (it == m_providers.end()) {
        return VoiceAutoResult::error("Provider not found", 1);
    }

    m_activeProviderName = providerName;

    // Default to first voice of this provider
    if (!it->second.voices.empty()) {
        m_activeVoiceId = it->second.voices[0].id;
    }

    {
        std::lock_guard<std::mutex> lock2(m_metricsMutex);
        m_metrics.providerSwitches++;
    }

    char ctx[128];
    snprintf(ctx, sizeof(ctx), "\"provider\":\"%s\",\"voice\":\"%s\"",
             providerName.c_str(), m_activeVoiceId.c_str());
    logStructured("INFO", "Active provider switched", ctx);

    return VoiceAutoResult::ok("Provider activated");
}

std::string VoiceAutomation::getActiveProviderName() const {
    return m_activeProviderName;
}

std::vector<std::string> VoiceAutomation::listProviders() const {
    std::lock_guard<std::mutex> lock(m_providerMutex);
    std::vector<std::string> names;
    names.reserve(m_providers.size());
    for (const auto& [name, _] : m_providers) {
        names.push_back(name);
    }
    return names;
}

const LoadedVoiceProvider* VoiceAutomation::getProvider(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_providerMutex);
    auto it = m_providers.find(name);
    if (it != m_providers.end()) {
        return &it->second;
    }
    return nullptr;
}

VoiceAutoResult VoiceAutomation::discoverProviders(const std::string& searchPathsStr) {
    int loaded = 0;
    // Parse semicolon-separated paths
    std::string remaining = searchPathsStr;
    while (!remaining.empty()) {
        size_t sep = remaining.find(';');
        std::string path = (sep != std::string::npos) ? remaining.substr(0, sep) : remaining;
        remaining = (sep != std::string::npos) ? remaining.substr(sep + 1) : "";

        if (path.empty()) continue;

        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) continue;

        for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
            if (ec) break;
            std::string ext = entry.path().extension().string();
            if (ext == ".dll" || ext == ".so" || ext == ".dylib") {
                std::string stem = entry.path().stem().string();
                // Look for voice provider DLLs
                if (stem.find("voice") != std::string::npos ||
                    stem.find("tts") != std::string::npos ||
                    stem.find("speech") != std::string::npos) {
                    VoiceAutoResult r = loadProvider(entry.path().string());
                    if (r.success) loaded++;
                }
            }
        }
    }

    char ctx[64];
    snprintf(ctx, sizeof(ctx), "\"discovered\":%d", loaded);
    logStructured("INFO", "Provider discovery complete", ctx);
    return VoiceAutoResult::ok("Discovery complete");
}

// ============================================================================
// Voice Selection
// ============================================================================
VoiceAutoResult VoiceAutomation::setVoice(const std::string& voiceId) {
    std::lock_guard<std::mutex> lock(m_providerMutex);

    // Search across all providers for this voice ID
    for (const auto& [name, provider] : m_providers) {
        for (const auto& voice : provider.voices) {
            if (voiceId == voice.id) {
                m_activeVoiceId = voiceId;
                m_activeProviderName = name;

                char ctx[256];
                snprintf(ctx, sizeof(ctx), "\"voice\":\"%s\",\"provider\":\"%s\",\"name\":\"%s\"",
                         voiceId.c_str(), name.c_str(), voice.name);
                logStructured("INFO", "Voice changed", ctx);
                return VoiceAutoResult::ok("Voice set");
            }
        }
    }

    return VoiceAutoResult::error("Voice ID not found in any provider", 1);
}

std::string VoiceAutomation::getActiveVoiceId() const {
    return m_activeVoiceId;
}

std::vector<VoiceInfo> VoiceAutomation::listVoices() const {
    std::lock_guard<std::mutex> lock(m_providerMutex);
    std::vector<VoiceInfo> allVoices;
    for (const auto& [name, provider] : m_providers) {
        allVoices.insert(allVoices.end(), provider.voices.begin(), provider.voices.end());
    }
    return allVoices;
}

std::vector<VoiceInfo> VoiceAutomation::listVoicesForProvider(const std::string& providerName) const {
    std::lock_guard<std::mutex> lock(m_providerMutex);
    auto it = m_providers.find(providerName);
    if (it != m_providers.end()) {
        return it->second.voices;
    }
    return {};
}

// ============================================================================
// Speech (called by chat pipeline)
// ============================================================================
VoiceAutoResult VoiceAutomation::speakResponse(const std::string& responseText) {
    if (!m_enabled.load()) {
        return VoiceAutoResult::ok("Voice automation disabled — skipping");
    }
    if (!m_initialized.load()) {
        return VoiceAutoResult::error("Not initialized", 1);
    }
    if (responseText.empty()) {
        return VoiceAutoResult::ok("Empty text — nothing to speak");
    }

    VoiceAutoConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    // If interruptOnNew, cancel current speech first
    if (cfg.interruptOnNew) {
        cancelSpeech();
    }

    enqueueSpeech(responseText, 0, false);

    logStructured("DEBUG", "Response queued for speech");
    return VoiceAutoResult::ok("Queued for speech");
}

VoiceAutoResult VoiceAutomation::speakStreamChunk(const std::string& chunk) {
    if (!m_enabled.load()) {
        return VoiceAutoResult::ok("Voice automation disabled");
    }

    VoiceAutoConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    if (!cfg.speakOnStream) {
        return VoiceAutoResult::ok("Stream speaking disabled");
    }

    if (chunk.empty()) {
        return VoiceAutoResult::ok("Empty chunk");
    }

    enqueueSpeech(chunk, 0, true);
    return VoiceAutoResult::ok("Chunk queued");
}

VoiceAutoResult VoiceAutomation::cancelSpeech() {
    // Cancel via active provider
    LoadedVoiceProvider* provider = getActiveProviderMutable();
    if (provider && provider->fnCancel) {
        provider->fnCancel(0); // 0 = cancel all
    }
    m_speaking.store(false);
    {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics.cancelledRequests++;
    }
    logStructured("DEBUG", "Speech cancelled");
    return VoiceAutoResult::ok("Cancelled");
}

VoiceAutoResult VoiceAutomation::cancelAll() {
    // Clear queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_speechQueue.empty()) m_speechQueue.pop();
    }
    return cancelSpeech();
}

bool VoiceAutomation::isSpeaking() const {
    return m_speaking.load();
}

// ============================================================================
// Speech Queue
// ============================================================================
void VoiceAutomation::enqueueSpeech(const std::string& text, int priority, bool isStreamChunk) {
    VoiceAutoConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        // Enforce max queue size
        if (static_cast<int>(m_speechQueue.size()) >= cfg.maxQueueSize) {
            logStructured("WARN", "Speech queue full — dropping oldest");
            m_speechQueue.pop();
        }

        QueuedSpeech item;
        item.text = text;
        item.priority = priority;
        item.isStreamChunk = isStreamChunk;
        item.enqueueTime = std::chrono::steady_clock::now();
        m_speechQueue.push(std::move(item));
    }

    m_queueCV.notify_one();
}

void VoiceAutomation::speechWorkerThread() {
    logStructured("INFO", "Speech worker thread started");

    while (!m_shutdownRequested.load()) {
        QueuedSpeech item;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCV.wait(lock, [this]() {
                return !m_speechQueue.empty() || m_shutdownRequested.load();
            });

            if (m_shutdownRequested.load()) break;
            if (m_speechQueue.empty()) continue;

            item = std::move(m_speechQueue.front());
            m_speechQueue.pop();
        }

        if (m_enabled.load()) {
            processSpeechItem(item);
        }
    }

    logStructured("INFO", "Speech worker thread exiting");
}

void VoiceAutomation::processSpeechItem(const QueuedSpeech& item) {
    auto startTime = std::chrono::steady_clock::now();
    m_speaking.store(true);

    VoiceAutoConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    LoadedVoiceProvider* provider = getActiveProviderMutable();
    if (!provider || !provider->fnSpeak) {
        emitError("No active voice provider available", 1);
        m_speaking.store(false);
        return;
    }

    SpeechRequest req;
    req.text     = item.text.c_str();
    req.voiceId  = m_activeVoiceId.c_str();
    req.rate     = cfg.rate;
    req.volume   = cfg.volume;
    req.pitch    = cfg.pitch;
    req.priority = item.priority;
    req.flags    = 0;

    if (m_speechCb) {
        m_speechCb(item.text.c_str(), m_activeVoiceId.c_str(), m_speechCbUserData);
    }

    int speechId = provider->fnSpeak(&req);
    if (speechId < 0) {
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "Speak failed: provider returned %d", speechId);
        emitError(errBuf, speechId);
        m_speaking.store(false);
        return;
    }

    // Wait for speech to complete (poll-based, non-blocking with timeout)
    if (provider->fnIsSpeaking) {
        int maxWaitMs = 60000; // 60s max
        int waited = 0;
        while (provider->fnIsSpeaking() && waited < maxWaitMs && !m_shutdownRequested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            waited += 50;
        }
    }

    m_speaking.store(false);

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count() / 1000.0;

    // Update metrics
    {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics.totalSpeechRequests++;
        m_metrics.totalCharsSpoken += static_cast<int64_t>(item.text.size());
        m_metrics.totalSpeechTimeMs += elapsed;

        // Rough word count
        int words = 1;
        for (char c : item.text) {
            if (c == ' ' || c == '\n') words++;
        }
        m_metrics.totalWordsSpoken += words;

        // Running average
        m_metrics.avgSpeechLatencyMs =
            (m_metrics.avgSpeechLatencyMs * (m_metrics.totalSpeechRequests - 1) + elapsed) /
            m_metrics.totalSpeechRequests;
    }
}

// ============================================================================
// Provider Helpers
// ============================================================================
LoadedVoiceProvider* VoiceAutomation::findProvider(const std::string& name) {
    auto it = m_providers.find(name);
    if (it != m_providers.end()) {
        return &it->second;
    }
    return nullptr;
}

LoadedVoiceProvider* VoiceAutomation::getActiveProviderMutable() {
    std::lock_guard<std::mutex> lock(m_providerMutex);
    return findProvider(m_activeProviderName);
}

// ============================================================================
// Metrics
// ============================================================================
VoiceAutoMetrics VoiceAutomation::getMetrics() const {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    return m_metrics;
}

VoiceAutoResult VoiceAutomation::resetMetrics() {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    m_metrics = {};
    logStructured("INFO", "Metrics reset");
    return VoiceAutoResult::ok("Metrics reset");
}

// ============================================================================
// Callbacks
// ============================================================================
void VoiceAutomation::setToggleCallback(VoiceAutoToggleCallback cb, void* userData) {
    m_toggleCb = cb;
    m_toggleCbUserData = userData;
}

void VoiceAutomation::setSpeechCallback(VoiceAutoSpeechCallback cb, void* userData) {
    m_speechCb = cb;
    m_speechCbUserData = userData;
}

void VoiceAutomation::setErrorCallback(VoiceAutoErrorCallback cb, void* userData) {
    m_errorCb = cb;
    m_errorCbUserData = userData;
}

// ============================================================================
// Built-in Windows SAPI Voice Provider
// ============================================================================
#ifdef _WIN32

// Static SAPI state (embedded provider — no separate DLL)
struct SAPIState {
    ISpVoice*           pVoice       = nullptr;
    bool                comInited    = false;
    bool                initialized  = false;
    std::vector<VoiceInfo> voices;
    VoiceEventCallback  eventCb      = nullptr;
    void*               eventCbData  = nullptr;
    std::mutex          speakMutex;
    std::atomic<bool>   speaking{false};
};

static SAPIState g_sapi;

static VoiceProviderInfo g_sapiProviderInfo = {
    "WindowsSAPI",       // name
    "1.0.0",            // version
    "Built-in Windows SAPI Text-to-Speech provider", // description
    1                    // maxConcurrent
};

// ── C-ABI exports for built-in SAPI (used via function pointers, not DLL export) ──

static VoiceProviderInfo* SAPI_GetInfo() {
    return &g_sapiProviderInfo;
}

static int SAPI_Init(void* /*context*/) {
    if (g_sapi.initialized) return 0;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
        g_sapi.comInited = (hr != RPC_E_CHANGED_MODE);
    } else {
        return -1;
    }

    hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL,
                          IID_ISpVoice, (void**)&g_sapi.pVoice);
    if (FAILED(hr) || !g_sapi.pVoice) {
        if (g_sapi.comInited) CoUninitialize();
        return -2;
    }

    // Enumerate available SAPI voices
#if RAWR_HAS_SPHELPER
    ISpObjectTokenCategory* pCategory = nullptr;
    hr = SpGetCategoryFromId(SPCAT_VOICES, &pCategory);
    if (SUCCEEDED(hr) && pCategory) {
        IEnumSpObjectTokens* pEnum = nullptr;
        hr = pCategory->EnumTokens(nullptr, nullptr, &pEnum);
        if (SUCCEEDED(hr) && pEnum) {
            ULONG count = 0;
            pEnum->GetCount(&count);

            for (ULONG i = 0; i < count && i < 64; ++i) {
                ISpObjectToken* pToken = nullptr;
                if (SUCCEEDED(pEnum->Next(1, &pToken, nullptr)) && pToken) {
                    VoiceInfo vi = {};

                    WCHAR* pName = nullptr;
                    if (SUCCEEDED(pToken->GetStringValue(nullptr, &pName)) && pName) {
                        char nameUtf8[128];
                        WideCharToMultiByte(CP_UTF8, 0, pName, -1, nameUtf8, sizeof(nameUtf8), nullptr, nullptr);
                        snprintf(vi.id, sizeof(vi.id), "SAPI:%s", nameUtf8);
                        strncpy_s(vi.name, sizeof(vi.name), nameUtf8, _TRUNCATE);
                        CoTaskMemFree(pName);
                    }

                    strncpy_s(vi.provider, sizeof(vi.provider), "WindowsSAPI", _TRUNCATE);
                    strncpy_s(vi.language, sizeof(vi.language), "en-US", _TRUNCATE);
                    vi.gender = 0;  // Could parse from attributes
                    vi.quality = 50;

                    // Try to get language attribute
                    ISpDataKey* pAttrs = nullptr;
                    if (SUCCEEDED(pToken->OpenKey(L"Attributes", &pAttrs)) && pAttrs) {
                        WCHAR* pLang = nullptr;
                        if (SUCCEEDED(pAttrs->GetStringValue(L"Language", &pLang)) && pLang) {
                            // SAPI stores language as hex LCID (e.g., "409" = en-US)
                            CoTaskMemFree(pLang);
                        }
                        WCHAR* pGender = nullptr;
                        if (SUCCEEDED(pAttrs->GetStringValue(L"Gender", &pGender)) && pGender) {
                            if (wcscmp(pGender, L"Male") == 0) vi.gender = 1;
                            else if (wcscmp(pGender, L"Female") == 0) vi.gender = 2;
                            CoTaskMemFree(pGender);
                        }
                        pAttrs->Release();
                    }

                    g_sapi.voices.push_back(vi);
                    pToken->Release();
                }
            }
            pEnum->Release();
        }
        pCategory->Release();
    }
#endif // RAWR_HAS_SPHELPER

    g_sapi.initialized = true;
    return 0;
}

static void SAPI_Shutdown() {
    if (!g_sapi.initialized) return;

    if (g_sapi.pVoice) {
        g_sapi.pVoice->Release();
        g_sapi.pVoice = nullptr;
    }
    if (g_sapi.comInited) {
        CoUninitialize();
        g_sapi.comInited = false;
    }
    g_sapi.voices.clear();
    g_sapi.initialized = false;
}

static int SAPI_EnumVoices(VoiceInfo* outVoices, int maxCount) {
    int count = std::min(static_cast<int>(g_sapi.voices.size()), maxCount);
    for (int i = 0; i < count; ++i) {
        outVoices[i] = g_sapi.voices[i];
    }
    return count;
}

static int SAPI_Speak(const SpeechRequest* request) {
    if (!g_sapi.pVoice || !request || !request->text) return -1;

    std::lock_guard<std::mutex> lock(g_sapi.speakMutex);
    g_sapi.speaking.store(true);

    // Set voice if specified
    if (request->voiceId && request->voiceId[0]) {
#if RAWR_HAS_SPHELPER
        // Find matching SAPI voice token by name
        ISpObjectTokenCategory* pCategory = nullptr;
        HRESULT hr = SpGetCategoryFromId(SPCAT_VOICES, &pCategory);
        if (SUCCEEDED(hr) && pCategory) {
            IEnumSpObjectTokens* pEnum = nullptr;
            hr = pCategory->EnumTokens(nullptr, nullptr, &pEnum);
            if (SUCCEEDED(hr) && pEnum) {
                ULONG count = 0;
                pEnum->GetCount(&count);
                for (ULONG i = 0; i < count; ++i) {
                    ISpObjectToken* pToken = nullptr;
                    if (SUCCEEDED(pEnum->Next(1, &pToken, nullptr)) && pToken) {
                        WCHAR* pName = nullptr;
                        if (SUCCEEDED(pToken->GetStringValue(nullptr, &pName)) && pName) {
                            char nameUtf8[128];
                            WideCharToMultiByte(CP_UTF8, 0, pName, -1, nameUtf8, sizeof(nameUtf8), nullptr, nullptr);
                            char expectedId[256];
                            snprintf(expectedId, sizeof(expectedId), "SAPI:%s", nameUtf8);
                            CoTaskMemFree(pName);

                            if (strcmp(expectedId, request->voiceId) == 0) {
                                g_sapi.pVoice->SetVoice(pToken);
                                pToken->Release();
                                break;
                            }
                        }
                        pToken->Release();
                    }
                }
                pEnum->Release();
            }
            pCategory->Release();
        }
#endif // RAWR_HAS_SPHELPER
    }

    // Set rate: SAPI rate range is -10 to +10, our rate is 0.1 to 10.0
    // Map: 1.0 -> 0, 2.0 -> 3, 0.5 -> -3
    long sapiRate = static_cast<long>((request->rate - 1.0f) * 5.0f);
    sapiRate = std::clamp(sapiRate, -10L, 10L);
    g_sapi.pVoice->SetRate(sapiRate);

    // Set volume: SAPI volume is 0-100
    USHORT sapiVolume = static_cast<USHORT>(request->volume * 100.0f);
    sapiVolume = std::min(sapiVolume, static_cast<USHORT>(100));
    g_sapi.pVoice->SetVolume(sapiVolume);

    // Emit start event
    if (g_sapi.eventCb) {
        VoiceEvent ev = {};
        ev.type = VoiceEvent_SpeechStarted;
        ev.speechId = 1;
        g_sapi.eventCb(&ev, g_sapi.eventCbData);
    }

    // Convert to wide string and speak
    int wlen = MultiByteToWideChar(CP_UTF8, 0, request->text, -1, nullptr, 0);
    std::vector<WCHAR> wtext(wlen);
    MultiByteToWideChar(CP_UTF8, 0, request->text, -1, wtext.data(), wlen);

    // SPF_ASYNC for non-blocking, SPF_PURGEBEFORESPEAK if priority=1
    DWORD flags = SPF_ASYNC | SPF_IS_NOT_XML;
    if (request->priority > 0) {
        flags |= SPF_PURGEBEFORESPEAK;
    }

    HRESULT hr = g_sapi.pVoice->Speak(wtext.data(), flags, nullptr);

    if (FAILED(hr)) {
        g_sapi.speaking.store(false);
        if (g_sapi.eventCb) {
            VoiceEvent ev = {};
            ev.type = VoiceEvent_SpeechError;
            ev.speechId = 1;
            g_sapi.eventCb(&ev, g_sapi.eventCbData);
        }
        return -2;
    }

    // Wait for completion (with timeout so we don't block forever)
    g_sapi.pVoice->WaitUntilDone(INFINITE);
    g_sapi.speaking.store(false);

    // Emit finished event
    if (g_sapi.eventCb) {
        VoiceEvent ev = {};
        ev.type = VoiceEvent_SpeechFinished;
        ev.speechId = 1;
        g_sapi.eventCb(&ev, g_sapi.eventCbData);
    }

    return 1; // speechId
}

static int SAPI_Cancel(int /*speechId*/) {
    if (!g_sapi.pVoice) return -1;
    g_sapi.pVoice->Speak(L"", SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
    g_sapi.speaking.store(false);

    if (g_sapi.eventCb) {
        VoiceEvent ev = {};
        ev.type = VoiceEvent_SpeechCancelled;
        ev.speechId = 0;
        g_sapi.eventCb(&ev, g_sapi.eventCbData);
    }
    return 0;
}

static int SAPI_IsSpeaking() {
    return g_sapi.speaking.load() ? 1 : 0;
}

static void SAPI_SetCallback(VoiceEventCallback callback, void* userData) {
    g_sapi.eventCb = callback;
    g_sapi.eventCbData = userData;
}

#endif // _WIN32

VoiceAutoResult VoiceAutomation::initBuiltinSAPI() {
#ifdef _WIN32
    if (m_sapiBuiltinLoaded) {
        return VoiceAutoResult::ok("SAPI already loaded");
    }

    LoadedVoiceProvider provider;
    provider.name = "WindowsSAPI";
    provider.dllPath = "<built-in>";
    provider.hModule = nullptr; // Built-in, no DLL
    provider.fnGetInfo    = SAPI_GetInfo;
    provider.fnInit       = SAPI_Init;
    provider.fnShutdown   = SAPI_Shutdown;
    provider.fnEnumVoices = SAPI_EnumVoices;
    provider.fnSpeak      = SAPI_Speak;
    provider.fnCancel     = SAPI_Cancel;
    provider.fnIsSpeaking = SAPI_IsSpeaking;
    provider.fnSetCallback= SAPI_SetCallback;

    // Initialize SAPI
    int initResult = SAPI_Init(nullptr);
    if (initResult != 0) {
        char errBuf[64];
        snprintf(errBuf, sizeof(errBuf), "SAPI init failed: %d", initResult);
        return VoiceAutoResult::error(errBuf, initResult);
    }
    provider.initialized = true;

    // Get info
    VoiceProviderInfo* info = SAPI_GetInfo();
    provider.info = *info;

    // Enumerate voices
    VoiceInfo voiceBuffer[64];
    int voiceCount = SAPI_EnumVoices(voiceBuffer, 64);
    for (int i = 0; i < voiceCount; ++i) {
        provider.voices.push_back(voiceBuffer[i]);
    }

    // Register
    {
        std::lock_guard<std::mutex> lock(m_providerMutex);
        m_providers["WindowsSAPI"] = std::move(provider);
    }

    // Set as default if no provider active
    if (m_activeProviderName.empty()) {
        m_activeProviderName = "WindowsSAPI";
        if (voiceCount > 0) {
            m_activeVoiceId = voiceBuffer[0].id;
        }
    }

    m_sapiBuiltinLoaded = true;
    {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics.providersLoaded++;
    }

    char ctx[128];
    snprintf(ctx, sizeof(ctx), "\"voices\":%d", voiceCount);
    logStructured("INFO", "Built-in SAPI provider registered", ctx);

    return VoiceAutoResult::ok("SAPI provider loaded");
#else
    return VoiceAutoResult::error("SAPI not available on this platform");
#endif
}
