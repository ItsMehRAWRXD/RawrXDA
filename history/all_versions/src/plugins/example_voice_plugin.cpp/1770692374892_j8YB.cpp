// ============================================================================
// Example Voice Provider Plugin — Template for Custom TTS Extensions
// Phase 44: Voice Automation Extension System
//
// This is a complete example showing how to build a custom voice provider
// plugin for RawrXD's voice automation system. It implements all C-ABI
// functions from voice_provider_plugin.h using a simple beep-based
// "voice" for demonstration.
//
// To create your own provider (e.g., ElevenLabs, Azure TTS, Coqui, etc.):
//   1. Copy this file
//   2. Replace the implementation with your TTS backend calls
//   3. Build as a DLL: cl /LD /DVOICE_PROVIDER_EXPORTS example_voice_plugin.cpp
//   4. Place in plugins/ directory
//
// Build (MSVC):
//   cl /LD /O2 /EHsc /DVOICE_PROVIDER_EXPORTS example_voice_plugin.cpp /Fe:example_voice_provider.dll
//
// Build (MinGW):
//   g++ -shared -O2 -DVOICE_PROVIDER_EXPORTS example_voice_plugin.cpp -o example_voice_provider.dll
// ============================================================================

#define VOICE_PROVIDER_EXPORTS
#include "voice_provider_plugin.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cstdio>
#include <cstring>
#include <atomic>

// ============================================================================
// Provider State
// ============================================================================
static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_speaking{false};
static std::atomic<int>  g_nextSpeechId{1};
static VoiceEventCallback g_callback = nullptr;
static void*              g_callbackData = nullptr;

// ============================================================================
// Provider metadata (static, lives for DLL lifetime)
// ============================================================================
static VoiceProviderInfo g_providerInfo = {
    "ExampleBeep",                                      // name
    "1.0.0",                                            // version
    "Example voice provider using Windows Beep() API. " // description
    "Replace with your own TTS backend.",
    1                                                   // maxConcurrent
};

// ============================================================================
// Available voices
// ============================================================================
static VoiceInfo g_voices[] = {
    { "ExampleBeep:High",    "High Pitch Beep",    "en-US", "ExampleBeep", 2, 10 },
    { "ExampleBeep:Low",     "Low Pitch Beep",     "en-US", "ExampleBeep", 1, 10 },
    { "ExampleBeep:Medium",  "Medium Pitch Beep",  "en-US", "ExampleBeep", 3, 10 },
};
static const int g_voiceCount = 3;

// ============================================================================
// Required Exports
// ============================================================================

extern "C" VOICE_API VoiceProviderInfo* VoiceProvider_GetInfo(void) {
    return &g_providerInfo;
}

extern "C" VOICE_API int VoiceProvider_Init(void* /*context*/) {
    if (g_initialized.load()) return 0;

    fprintf(stderr, "[ExampleBeepProvider] Initialized\n");
    g_initialized.store(true);
    return 0;
}

extern "C" VOICE_API int VoiceProvider_Speak(const SpeechRequest* request) {
    if (!g_initialized.load() || !request || !request->text) return -1;

    g_speaking.store(true);
    int speechId = g_nextSpeechId.fetch_add(1);

    // Emit start event
    if (g_callback) {
        VoiceEvent ev = {};
        ev.type = VoiceEvent_SpeechStarted;
        ev.speechId = speechId;
        g_callback(&ev, g_callbackData);
    }

    // Determine beep frequency from voice ID
    DWORD freq = 800; // default medium
    if (request->voiceId) {
        if (strstr(request->voiceId, "High"))  freq = 1200;
        if (strstr(request->voiceId, "Low"))   freq = 400;
    }

    // Adjust frequency by pitch
    freq = static_cast<DWORD>(freq * request->pitch);

    // "Speak" each word as a beep
    // Duration per word = 200ms / rate
    DWORD durationMs = static_cast<DWORD>(200.0f / request->rate);
    if (durationMs < 50) durationMs = 50;
    if (durationMs > 2000) durationMs = 2000;

    const char* p = request->text;
    int charPos = 0;
    while (*p) {
        // Skip whitespace
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
            p++;
            charPos++;
        }
        if (!*p) break;

        // Find word end
        const char* wordStart = p;
        int wordPos = charPos;
        while (*p && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') {
            p++;
            charPos++;
        }
        int wordLen = charPos - wordPos;

        // Emit word boundary event
        if (g_callback) {
            VoiceEvent ev = {};
            ev.type = VoiceEvent_WordBoundary;
            ev.speechId = speechId;
            ev.charPosition = wordPos;
            ev.charLength = wordLen;
            g_callback(&ev, g_callbackData);
        }

#ifdef _WIN32
        // Beep at the frequency (volume not controllable via Beep)
        Beep(freq, durationMs);
#else
        // On non-Windows, just sleep
        usleep(durationMs * 1000);
#endif

        // Short pause between words
        Sleep(50);
    }

    g_speaking.store(false);

    // Emit finished event
    if (g_callback) {
        VoiceEvent ev = {};
        ev.type = VoiceEvent_SpeechFinished;
        ev.speechId = speechId;
        g_callback(&ev, g_callbackData);
    }

    fprintf(stderr, "[ExampleBeepProvider] Spoke %d chars (speechId=%d)\n",
            charPos, speechId);
    return speechId;
}

// ============================================================================
// Optional Exports
// ============================================================================

extern "C" VOICE_API void VoiceProvider_Shutdown(void) {
    g_initialized.store(false);
    g_speaking.store(false);
    g_callback = nullptr;
    g_callbackData = nullptr;
    fprintf(stderr, "[ExampleBeepProvider] Shutdown\n");
}

extern "C" VOICE_API int VoiceProvider_EnumVoices(VoiceInfo* outVoices, int maxCount) {
    int count = (maxCount < g_voiceCount) ? maxCount : g_voiceCount;
    for (int i = 0; i < count; ++i) {
        outVoices[i] = g_voices[i];
    }
    return count;
}

extern "C" VOICE_API int VoiceProvider_Cancel(int /*speechId*/) {
    // For the beep provider, we can't truly cancel mid-beep,
    // but we mark speaking as false so the loop will stop
    g_speaking.store(false);

    if (g_callback) {
        VoiceEvent ev = {};
        ev.type = VoiceEvent_SpeechCancelled;
        ev.speechId = 0;
        g_callback(&ev, g_callbackData);
    }
    return 0;
}

extern "C" VOICE_API int VoiceProvider_IsSpeaking(void) {
    return g_speaking.load() ? 1 : 0;
}

extern "C" VOICE_API void VoiceProvider_SetCallback(VoiceEventCallback callback, void* userData) {
    g_callback = callback;
    g_callbackData = userData;
}

// ============================================================================
// DLL Entry Point (Windows)
// ============================================================================
#ifdef _WIN32
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    (void)hModule; (void)lpReserved;
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            break;
        case DLL_PROCESS_DETACH:
            if (g_initialized.load()) {
                VoiceProvider_Shutdown();
            }
            break;
    }
    return TRUE;
}
#endif
