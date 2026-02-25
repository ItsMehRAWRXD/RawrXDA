#pragma once
// ============================================================================
// Voice Provider Plugin API — C-ABI Interface for Custom TTS Voices
// Phase 44: Voice Automation Extension System
//
// This header defines the stable C-ABI interface that voice provider plugins
// must implement. Build your DLL exporting these functions to create a custom
// voice for RawrXD's voice automation system.
//
// Required exports:
//   VoiceProvider_GetInfo     — Return provider metadata
//   VoiceProvider_Init        — Initialize the provider
//   VoiceProvider_Speak       — Start speaking text
//
// Optional exports:
//   VoiceProvider_Shutdown    — Clean up resources
//   VoiceProvider_EnumVoices  — List available voices
//   VoiceProvider_Cancel      — Cancel speech
//   VoiceProvider_IsSpeaking  — Check if speaking
//   VoiceProvider_SetCallback — Register event callback
//
// Build: Compile as a standard DLL (.dll / .so / .dylib)
// Place: In 'plugins/', 'voice_plugins/', or 'extensions/voice/' directory
//        (or any path configured in VoiceAutoConfig::pluginSearchPaths)
//
// Naming: DLL name should contain "voice", "tts", or "speech" for auto-discovery
//
// Example: my_custom_voice.dll with exports:
//   extern "C" __declspec(dllexport) VoiceProviderInfo* VoiceProvider_GetInfo();
//   extern "C" __declspec(dllexport) int VoiceProvider_Init(void* context);
//   extern "C" __declspec(dllexport) int VoiceProvider_Speak(const SpeechRevoid* req);
//   ...
//
// No Qt. No exceptions. No STL across DLL boundary (strings are C-style char*).
// ============================================================================

#ifndef RAWRXD_VOICE_PROVIDER_PLUGIN_H
#define RAWRXD_VOICE_PROVIDER_PLUGIN_H

#ifdef _WIN32
#ifdef VOICE_PROVIDER_EXPORTS
#define VOICE_API __declspec(dllexport)
#else
#define VOICE_API __declspec(dllimport)
#endif
#else
#define VOICE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Data Types (must match voice_automation.hpp)
// ============================================================================

/** Information about a single voice available in this provider */
typedef struct VoiceInfo {
    char id[128];            /**< Unique voice ID (e.g., "ElevenLabs:Rachel") */
    char name[128];          /**< Human-readable display name */
    char language[32];       /**< BCP-47 language tag (e.g., "en-US", "ja-JP") */
    char provider[64];       /**< Provider name that owns this voice */
    int  gender;             /**< 0=unknown, 1=male, 2=female, 3=neutral */
    int  quality;            /**< Quality score 0-100 (higher = better fidelity) */
} VoiceInfo;

/** Provider-level metadata */
typedef struct VoiceProviderInfo {
    char name[64];           /**< Provider identifier (e.g., "ElevenLabs") */
    char version[32];        /**< Provider version string (e.g., "2.1.0") */
    char description[256];   /**< Human-readable description */
    int  maxConcurrent;      /**< Max concurrent speech streams (0 = unlimited) */
} VoiceProviderInfo;

/** Speech request parameters */
typedef struct SpeechRequest {
    const char* text;        /**< Text to speak (UTF-8, null-terminated) */
    const char* voiceId;     /**< Voice ID to use (from VoiceInfo::id) */
    float       rate;        /**< Speech rate (0.1 = very slow, 1.0 = normal, 10.0 = max) */
    float       volume;      /**< Volume level (0.0 = silent, 1.0 = full) */
    float       pitch;       /**< Pitch multiplier (0.5 = low, 1.0 = normal, 2.0 = high) */
    int         priority;    /**< 0 = normal (queued), 1 = high (interrupts current) */
    int         flags;       /**< Reserved for future use, set to 0 */
} SpeechRequest;

/** Voice event types for callbacks */
typedef enum VoiceEventType {
    VoiceEvent_SpeechStarted    = 1,
    VoiceEvent_SpeechFinished   = 2,
    VoiceEvent_SpeechCancelled  = 3,
    VoiceEvent_SpeechError      = 4,
    VoiceEvent_WordBoundary     = 5,
    VoiceEvent_SentenceBoundary = 6,
    VoiceEvent_Bookmark         = 7
} VoiceEventType;

/** Voice event payload */
typedef struct VoiceEvent {
    VoiceEventType type;         /**< Event type */
    int            speechId;     /**< Which speech request this event belongs to */
    int            charPosition; /**< Character position in text (for boundaries) */
    int            charLength;   /**< Length at boundary position */
    const char*    detail;       /**< Optional detail string (may be NULL) */
} VoiceEvent;

/** Callback function signature for voice events */
typedef void (*VoiceEventCallback)(const VoiceEvent* event, void* userData);

// ============================================================================
// Required Plugin Exports
// ============================================================================

/**
 * Return provider metadata. Called once when the DLL is loaded.
 * The returned pointer must remain valid for the lifetime of the DLL.
 *
 * @return Pointer to static VoiceProviderInfo struct
 */
VOICE_API VoiceProviderInfo* VoiceProvider_GetInfo(void);

/**
 * Initialize the voice provider. Called once after loading.
 * Perform any setup here (COM init, API auth, model loading, etc.)
 *
 * @param context  Reserved, will be NULL. Future: host capabilities struct.
 * @return 0 on success, negative error code on failure
 */
VOICE_API int VoiceProvider_Init(void* context);

/**
 * Speak the given text. May be synchronous or asynchronous.
 * If async, return a positive speechId and call the event callback on completion.
 * If sync, block until speech is done and return a positive speechId.
 *
 * @param request  Speech request with text, voice, rate, volume, pitch
 * @return Positive speechId on success, negative error code on failure
 */
VOICE_API int VoiceProvider_Speak(const SpeechRevoid* request);

// ============================================================================
// Optional Plugin Exports
// ============================================================================

/**
 * Shut down and release all resources. Called before DLL unload.
 */
VOICE_API void VoiceProvider_Shutdown(void);

/**
 * Enumerate available voices from this provider.
 *
 * @param outVoices  Array to fill with VoiceInfo structs
 * @param maxCount   Maximum number of entries in the array
 * @return Number of voices written to the array
 */
VOICE_API int VoiceProvider_EnumVoices(VoiceInfo* outVoices, int maxCount);

/**
 * Cancel speech.
 *
 * @param speechId  ID of speech to cancel, or 0 to cancel all
 * @return 0 on success, negative error code on failure
 */
VOICE_API int VoiceProvider_Cancel(int speechId);

/**
 * Check if the provider is currently speaking.
 *
 * @return 1 if speaking, 0 if idle
 */
VOICE_API int VoiceProvider_IsSpeaking(void);

/**
 * Register a callback for speech events.
 *
 * @param callback  Function pointer to call on events
 * @param userData  User data passed through to callback
 */
VOICE_API void VoiceProvider_SetCallback(VoiceEventCallback callback, void* userData);

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_VOICE_PROVIDER_PLUGIN_H */
