#pragma once
// ============================================================================
// VoiceChat — Native Win32 Voice Chat Engine (Phase 33)
// Replaces Qt6 VoiceProcessor with pure Win32 audio APIs
// Features:
//   - Audio capture via Windows waveIn API (16-bit PCM, 16kHz mono)
//   - Audio playback via Windows waveOut API
//   - Push-to-talk (PTT) and voice-activity-detection (VAD) modes
//   - WebSocket-based voice relay for P2P chat rooms
//   - Speech-to-text transcription via local/remote AI backend
//   - Text-to-speech feedback via local/remote AI backend
//   - Structured logging with performance metrics
//   - GDPR-compliant audio data lifecycle
//   - Thread-safe, no exceptions, PatchResult pattern
// ============================================================================

#ifndef RAWRXD_VOICE_CHAT_HPP
#define RAWRXD_VOICE_CHAT_HPP

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <chrono>
#include <deque>

// ============================================================================
// Result type (project convention: PatchResult pattern, no exceptions)
// ============================================================================
struct VoiceChatResult {
    bool success;
    const char* detail;
    int errorCode;

    static VoiceChatResult ok(const char* msg = "OK") {
        return { true, msg, 0 };
    }
    static VoiceChatResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// Voice Chat Configuration
// ============================================================================
struct VoiceChatConfig {
    int sampleRate       = 16000;   // Hz
    int channels         = 1;       // mono
    int bitsPerSample    = 16;      // 16-bit PCM
    int bufferSizeMs     = 100;     // per-buffer duration
    int numBuffers       = 4;       // double/quad buffering
    int maxRecordMs      = 300000;  // 5 min max single recording
    bool enableVAD       = true;    // voice-activity detection
    float vadThreshold   = 0.02f;   // RMS threshold for voice detection
    int vadSilenceMs     = 1500;    // silence duration to auto-stop
    bool enableMetrics   = true;
    bool enableAutoDelete= true;
    int autoDeleteMs     = 60000;   // GDPR auto-purge delay
    std::string relayHost;          // WebSocket relay host (empty = local only)
    uint16_t relayPort   = 0;      // WebSocket relay port
    std::string apiEndpoint;        // STT/TTS API endpoint
    std::string apiKey;             // API key for STT/TTS
    std::string roomName;           // Chat room identifier
    std::string userName = "User";  // Display name
};

// ============================================================================
// Voice Activity Detection state
// ============================================================================
enum class VADState : int {
    Silence = 0,
    Speech  = 1,
    Trailing = 2  // speech ended, waiting for vadSilenceMs
};

// ============================================================================
// Voice Chat Mode
// ============================================================================
enum class VoiceChatMode : int {
    PushToTalk = 0,  // Manual PTT (hold key to talk)
    Continuous = 1,   // VAD-based auto-detection
    Disabled   = 2
};

// ============================================================================
// Audio Buffer
// ============================================================================
struct AudioBuffer {
    std::vector<int16_t> samples;
    uint64_t timestamp;  // ms since epoch
    float rmsLevel;      // RMS volume level 0.0 - 1.0
};

// ============================================================================
// Voice Chat Metrics (instrumented per project instructions)
// ============================================================================
struct VoiceChatMetrics {
    std::atomic<int64_t> recordingCount{0};
    std::atomic<int64_t> playbackCount{0};
    std::atomic<int64_t> transcriptionCount{0};
    std::atomic<int64_t> ttsCount{0};
    std::atomic<int64_t> errorCount{0};
    std::atomic<int64_t> bytesRecorded{0};
    std::atomic<int64_t> bytesPlayed{0};
    std::atomic<int64_t> vadSpeechEvents{0};
    std::atomic<int64_t> relayMessagesSent{0};
    std::atomic<int64_t> relayMessagesRecv{0};
    double avgRecordingLatencyMs = 0.0;
    double avgTranscriptionLatencyMs = 0.0;
    double avgTtsLatencyMs = 0.0;
    double avgPlaybackLatencyMs = 0.0;
};

// ============================================================================
// Callback typedefs (no std::function in hot path per project convention)
// ============================================================================
using VoiceChatAudioCallback   = void(*)(const int16_t* samples, size_t count, void* userData);
using VoiceChatTextCallback    = void(*)(const char* text, void* userData);
using VoiceChatEventCallback   = void(*)(const char* event, const char* detail, void* userData);
using VoiceChatVADCallback     = void(*)(VADState state, float rmsLevel, void* userData);

// ============================================================================
// VoiceChat Engine — Main class
// ============================================================================
class VoiceChat {
public:
    VoiceChat();
    ~VoiceChat();

    // ---- Configuration ----
    VoiceChatResult configure(const VoiceChatConfig& config);
    VoiceChatConfig getConfig() const;

    // ---- Recording (Capture) ----
    VoiceChatResult startRecording();
    VoiceChatResult stopRecording();
    bool isRecording() const;

    // ---- Playback ----
    VoiceChatResult playAudio(const int16_t* samples, size_t sampleCount);
    VoiceChatResult playAudio(const std::vector<int16_t>& samples);
    VoiceChatResult stopPlayback();
    bool isPlaying() const;

    // ---- Voice Activity Detection ----
    VADState getVADState() const;
    float getCurrentRMS() const;

    // ---- Mode control ----
    VoiceChatResult setMode(VoiceChatMode mode);
    VoiceChatMode getMode() const;

    // ---- PTT (Push-to-Talk) ----
    VoiceChatResult pttBegin();
    VoiceChatResult pttEnd();
    bool isPTTActive() const;

    // ---- Speech-to-Text transcription ----
    VoiceChatResult transcribe(const std::vector<int16_t>& audio, std::string& outText);
    VoiceChatResult transcribeLastRecording(std::string& outText);

    // ---- Text-to-Speech ----
    VoiceChatResult speak(const std::string& text);

    // ---- Chat Room (WebSocket relay) ----
    VoiceChatResult joinRoom(const std::string& roomName);
    VoiceChatResult leaveRoom();
    bool isInRoom() const;
    std::string getRoomName() const;
    std::vector<std::string> getRoomMembers() const;

    // ---- Recorded audio access ----
    const std::vector<int16_t>& getLastRecording() const;
    size_t getRecordedSampleCount() const;
    VoiceChatResult clearRecording();

    // ---- Audio device enumeration ----
    struct AudioDevice {
        int id;
        std::string name;
        bool isDefault;
    };
    static std::vector<AudioDevice> enumerateInputDevices();
    static std::vector<AudioDevice> enumerateOutputDevices();
    VoiceChatResult selectInputDevice(int deviceId);
    VoiceChatResult selectOutputDevice(int deviceId);

    // ---- Metrics ----
    VoiceChatMetrics getMetrics() const;
    VoiceChatResult resetMetrics();

    // ---- Callbacks ----
    void setAudioCaptureCallback(VoiceChatAudioCallback cb, void* userData);
    void setTranscriptionCallback(VoiceChatTextCallback cb, void* userData);
    void setEventCallback(VoiceChatEventCallback cb, void* userData);
    void setVADCallback(VoiceChatVADCallback cb, void* userData);

    // ---- Shutdown ----
    VoiceChatResult shutdown();

private:
    // Platform-specific audio handles
#ifdef _WIN32
    HWAVEIN  m_hWaveIn  = nullptr;
    HWAVEOUT m_hWaveOut = nullptr;
    std::vector<WAVEHDR> m_waveInHeaders;
    std::vector<std::vector<char>> m_waveInBuffers;
    std::vector<WAVEHDR> m_waveOutHeaders;
    int m_inputDeviceId  = WAVE_MAPPER;
    int m_outputDeviceId = WAVE_MAPPER;

    static void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                                     DWORD_PTR dwParam1, DWORD_PTR dwParam2);
    static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
                                      DWORD_PTR dwParam1, DWORD_PTR dwParam2);
    VoiceChatResult initWaveIn();
    VoiceChatResult closeWaveIn();
    VoiceChatResult initWaveOut();
    VoiceChatResult closeWaveOut();
    void processWaveInBuffer(WAVEHDR* header);
#endif

    // VAD processing
    float computeRMS(const int16_t* samples, size_t count) const;
    void updateVAD(float rms);

    // Transcription HTTP helper
    VoiceChatResult httpPostJSON(const std::string& url, const std::string& jsonBody,
                                  std::string& response);

    // State
    VoiceChatConfig m_config;
    mutable std::mutex m_configMutex;

    std::atomic<bool> m_recording{false};
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_pttActive{false};
    std::atomic<bool> m_inRoom{false};
    std::atomic<bool> m_shutdown{false};

    VoiceChatMode m_mode = VoiceChatMode::PushToTalk;
    VADState m_vadState  = VADState::Silence;
    std::atomic<float> m_currentRMS{0.0f};
    std::chrono::steady_clock::time_point m_lastSpeechTime;

    // Recorded audio accumulator
    std::vector<int16_t> m_recordedSamples;
    mutable std::mutex m_recordMutex;

    // Playback queue
    std::deque<std::vector<int16_t>> m_playbackQueue;
    mutable std::mutex m_playbackMutex;

    // Room state
    std::string m_roomName;
    std::vector<std::string> m_roomMembers;
    mutable std::mutex m_roomMutex;

    // Callbacks
    VoiceChatAudioCallback m_audioCb     = nullptr;
    void* m_audioCbUserData              = nullptr;
    VoiceChatTextCallback m_transcribeCb = nullptr;
    void* m_transcribeCbUserData         = nullptr;
    VoiceChatEventCallback m_eventCb     = nullptr;
    void* m_eventCbUserData              = nullptr;
    VoiceChatVADCallback m_vadCb         = nullptr;
    void* m_vadCbUserData                = nullptr;

    // Metrics
    VoiceChatMetrics m_metrics;
    mutable std::mutex m_metricsMutex;

    // Logging
    void logStructured(const char* level, const char* message, const char* context = nullptr);
    void emitEvent(const char* event, const char* detail = nullptr);
    void recordLatency(const char* operation, double ms);
};

#endif // RAWRXD_VOICE_CHAT_HPP
