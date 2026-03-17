#pragma once
// ============================================================================
// VoiceAutomation — Toggleable TTS Response Reader with Pluggable Voice Providers
// Phase 44: Voice Automation System
//
// When toggled ON, intercepts chat/AI responses and speaks them aloud.
// Voice providers are loaded via the plugin/extension system (C-ABI DLLs).
// Ships with a built-in Windows SAPI provider as the default.
//
// Features:
//   - Toggle voice on/off at runtime
//   - Pluggable voice providers via DLL (VoiceProviderPlugin C-ABI)
//   - Built-in Windows SAPI TTS provider
//   - Voice selection (enumerate available voices, switch at runtime)
//   - Speech rate, volume, pitch controls
//   - Async speech queue with interrupt/cancel
//   - Streaming TTS (speak chunks as they arrive during streaming responses)
//   - Structured logging, metrics, PatchResult pattern, no exceptions
//   - Thread-safe, lock_guard only
// ============================================================================

#ifndef RAWRXD_VOICE_AUTOMATION_HPP
#define RAWRXD_VOICE_AUTOMATION_HPP

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <functional>
#include <chrono>
#include <unordered_map>

// ============================================================================
// Result type (PatchResult pattern, no exceptions)
// ============================================================================
struct VoiceAutoResult {
    bool success;
    const char* detail;
    int errorCode;

    static VoiceAutoResult ok(const char* msg = "OK") {
        return { true, msg, 0 };
    }
    static VoiceAutoResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// Voice Provider Plugin C-ABI Interface
// DLL plugins export these functions to provide custom TTS voices.
// ============================================================================
struct VoiceInfo {
    char id[128];            // Unique voice ID (e.g., "SAPI:David", "ElevenLabs:Rachel")
    char name[128];          // Human-readable name
    char language[32];       // BCP-47 language tag (e.g., "en-US")
    char provider[64];       // Provider name (e.g., "WindowsSAPI", "ElevenLabs")
    int  gender;             // 0=unknown, 1=male, 2=female, 3=neutral
    int  quality;            // 0-100, higher = better
};

struct VoiceProviderInfo {
    char name[64];           // Provider name
    char version[32];        // Provider version string
    char description[256];   // Human-readable description
    int  maxConcurrent;      // Max concurrent speech streams (0 = unlimited)
};

// Speech request passed to the provider
struct SpeechRequest {
    const char* text;        // Text to speak (UTF-8)
    const char* voiceId;     // Voice ID to use (from VoiceInfo::id)
    float       rate;        // Speech rate multiplier (0.1 - 10.0, 1.0 = normal)
    float       volume;      // Volume (0.0 - 1.0)
    float       pitch;       // Pitch multiplier (0.5 - 2.0, 1.0 = normal)
    int         priority;    // 0=normal, 1=high (interrupts current)
    int         flags;       // Reserved, set to 0
};

// Speech event types
enum VoiceEventType : int {
    VoiceEvent_SpeechStarted   = 1,
    VoiceEvent_SpeechFinished  = 2,
    VoiceEvent_SpeechCancelled = 3,
    VoiceEvent_SpeechError     = 4,
    VoiceEvent_WordBoundary    = 5,
    VoiceEvent_SentenceBoundary= 6,
    VoiceEvent_Bookmark        = 7,
};

// Speech event data
struct VoiceEvent {
    VoiceEventType type;
    int            speechId;     // Which speech request
    int            charPosition; // Position in text (for boundaries)
    int            charLength;   // Length of word/sentence at boundary
    const char*    detail;       // Optional detail string
};

// Callback type for speech events
typedef void (*VoiceEventCallback)(const VoiceEvent* event, void* userData);

// ============================================================================
// C-ABI Plugin Entry Points
// Voice provider DLLs must export these functions.
// ============================================================================
extern "C" {
    // Get provider metadata
    typedef VoiceProviderInfo* (*VoiceProvider_GetInfoFn)();

    // Initialize the provider (called once on load)
    typedef int (*VoiceProvider_InitFn)(void* context);

    // Shut down the provider (called once on unload)
    typedef void (*VoiceProvider_ShutdownFn)();

    // Enumerate available voices (fills array, returns count)
    typedef int (*VoiceProvider_EnumVoicesFn)(VoiceInfo* outVoices, int maxCount);

    // Start speaking (returns speechId > 0, or negative on error)
    typedef int (*VoiceProvider_SpeakFn)(const SpeechRequest* request);

    // Cancel specific speech (by speechId), or all if speechId == 0
    typedef int (*VoiceProvider_CancelFn)(int speechId);

    // Check if currently speaking
    typedef int (*VoiceProvider_IsSpeakingFn)();

    // Set event callback
    typedef void (*VoiceProvider_SetCallbackFn)(VoiceEventCallback callback, void* userData);
}

// ============================================================================
// Loaded voice provider instance
// ============================================================================
struct LoadedVoiceProvider {
    std::string                  name;
    std::string                  dllPath;
    VoiceProviderInfo            info;
#ifdef _WIN32
    HMODULE                      hModule;
#else
    void*                        hModule;
#endif
    VoiceProvider_GetInfoFn      fnGetInfo;
    VoiceProvider_InitFn         fnInit;
    VoiceProvider_ShutdownFn     fnShutdown;
    VoiceProvider_EnumVoicesFn   fnEnumVoices;
    VoiceProvider_SpeakFn        fnSpeak;
    VoiceProvider_CancelFn       fnCancel;
    VoiceProvider_IsSpeakingFn   fnIsSpeaking;
    VoiceProvider_SetCallbackFn  fnSetCallback;
    bool                         initialized;
    std::vector<VoiceInfo>       voices;
};

// ============================================================================
// Voice Automation Configuration
// ============================================================================
struct VoiceAutoConfig {
    bool        enabled           = false;    // Master toggle
    std::string activeProviderId;             // Active provider name
    std::string activeVoiceId;               // Active voice ID
    float       rate              = 1.0f;    // Speech rate (0.1 - 10.0)
    float       volume            = 0.8f;    // Volume (0.0 - 1.0)
    float       pitch             = 1.0f;    // Pitch (0.5 - 2.0)
    bool        speakOnStream     = true;    // Speak chunks during streaming
    bool        interruptOnNew    = true;    // Cancel current speech for new message
    int         maxQueueSize      = 32;      // Max queued speech requests
    std::string pluginSearchPaths;           // Semicolon-separated DLL search paths
};

// ============================================================================
// Voice Automation Metrics
// ============================================================================
struct VoiceAutoMetrics {
    int64_t totalSpeechRequests   = 0;
    int64_t totalWordsSpoken      = 0;
    int64_t totalCharsSpoken      = 0;
    int64_t cancelledRequests     = 0;
    int64_t errorCount            = 0;
    int64_t providersLoaded       = 0;
    int64_t providerSwitches      = 0;
    double  avgSpeechLatencyMs    = 0.0;
    double  totalSpeechTimeMs     = 0.0;
};

// ============================================================================
// Queued speech item
// ============================================================================
struct QueuedSpeech {
    std::string text;
    int         priority;       // 0=normal, 1=high
    bool        isStreamChunk;  // True if from streaming response
    std::chrono::steady_clock::time_point enqueueTime;
};

// ============================================================================
// Voice Automation callback types (no std::function in hot path)
// ============================================================================
using VoiceAutoToggleCallback = void(*)(bool enabled, void* userData);
using VoiceAutoSpeechCallback = void(*)(const char* text, const char* voiceId, void* userData);
using VoiceAutoErrorCallback  = void(*)(const char* error, int code, void* userData);

// ============================================================================
// VoiceAutomation — Main Engine Class
// ============================================================================
class VoiceAutomation {
public:
    VoiceAutomation();
    ~VoiceAutomation();

    // ── Lifecycle ──────────────────────────────────────────────────────
    VoiceAutoResult initialize(const VoiceAutoConfig& config = {});
    VoiceAutoResult shutdown();
    bool isInitialized() const;

    // ── Toggle ─────────────────────────────────────────────────────────
    VoiceAutoResult enable();
    VoiceAutoResult disable();
    VoiceAutoResult toggle();
    bool isEnabled() const;

    // ── Configuration ──────────────────────────────────────────────────
    VoiceAutoResult configure(const VoiceAutoConfig& config);
    VoiceAutoConfig getConfig() const;
    VoiceAutoResult setRate(float rate);
    VoiceAutoResult setVolume(float volume);
    VoiceAutoResult setPitch(float pitch);

    // ── Provider Management (Plugin System) ────────────────────────────
    VoiceAutoResult loadProvider(const std::string& dllPath);
    VoiceAutoResult unloadProvider(const std::string& providerName);
    VoiceAutoResult setActiveProvider(const std::string& providerName);
    std::string getActiveProviderName() const;
    std::vector<std::string> listProviders() const;
    const LoadedVoiceProvider* getProvider(const std::string& name) const;
    VoiceAutoResult discoverProviders(const std::string& searchPath = "");

    // ── Voice Selection ────────────────────────────────────────────────
    VoiceAutoResult setVoice(const std::string& voiceId);
    std::string getActiveVoiceId() const;
    std::vector<VoiceInfo> listVoices() const;
    std::vector<VoiceInfo> listVoicesForProvider(const std::string& providerName) const;

    // ── Speech (called by chat pipeline) ───────────────────────────────
    VoiceAutoResult speakResponse(const std::string& responseText);
    VoiceAutoResult speakStreamChunk(const std::string& chunk);
    VoiceAutoResult cancelSpeech();
    VoiceAutoResult cancelAll();
    bool isSpeaking() const;

    // ── Metrics ────────────────────────────────────────────────────────
    VoiceAutoMetrics getMetrics() const;
    VoiceAutoResult resetMetrics();

    // ── Callbacks ──────────────────────────────────────────────────────
    void setToggleCallback(VoiceAutoToggleCallback cb, void* userData);
    void setSpeechCallback(VoiceAutoSpeechCallback cb, void* userData);
    void setErrorCallback(VoiceAutoErrorCallback cb, void* userData);

    // ── Built-in SAPI provider ─────────────────────────────────────────
    VoiceAutoResult initBuiltinSAPI();

private:
    // Speech queue worker thread
    void speechWorkerThread();
    void processSpeechItem(const QueuedSpeech& item);
    void enqueueSpeech(const std::string& text, int priority, bool isStreamChunk);

    // Provider helpers
    LoadedVoiceProvider* findProvider(const std::string& name);
    LoadedVoiceProvider* getActiveProviderMutable();

    // Logging
    void logStructured(const char* level, const char* message, const char* context = nullptr);
    void emitError(const char* error, int code);

    // ── State ──────────────────────────────────────────────────────────
    VoiceAutoConfig m_config;
    mutable std::mutex m_configMutex;

    std::unordered_map<std::string, LoadedVoiceProvider> m_providers;
    mutable std::mutex m_providerMutex;

    std::string m_activeProviderName;
    std::string m_activeVoiceId;

    std::queue<QueuedSpeech> m_speechQueue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCV;

    std::thread m_workerThread;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_speaking{false};
    std::atomic<bool> m_shutdownRequested{false};

    VoiceAutoMetrics m_metrics;
    mutable std::mutex m_metricsMutex;

    VoiceAutoToggleCallback m_toggleCb = nullptr;
    void* m_toggleCbUserData = nullptr;
    VoiceAutoSpeechCallback m_speechCb = nullptr;
    void* m_speechCbUserData = nullptr;
    VoiceAutoErrorCallback m_errorCb = nullptr;
    void* m_errorCbUserData = nullptr;

    // Built-in SAPI provider handle (if loaded)
    bool m_sapiBuiltinLoaded = false;
};

// ============================================================================
// Global accessor (singleton, lazy-initialized)
// ============================================================================
VoiceAutomation& getVoiceAutomation();

#endif // RAWRXD_VOICE_AUTOMATION_HPP
