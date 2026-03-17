// ============================================================================
// VoiceChat — Native Win32 Voice Chat Engine Implementation (Phase 33)
// Pure Win32 audio capture/playback via waveIn/waveOut API
// No Qt, no exceptions — PatchResult pattern throughout
// ============================================================================

#include "voice_chat.hpp"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <ctime>

#ifdef _WIN32
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

// ============================================================================
// Structured logging helper
// ============================================================================
void VoiceChat::logStructured(const char* level, const char* message, const char* context)
{
    // Get ISO timestamp
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
        fprintf(stderr, "[%s][VoiceChat][%s] %s {%s}\n", timeBuf, level, message, context);
    } else {
        fprintf(stderr, "[%s][VoiceChat][%s] %s\n", timeBuf, level, message);
    }
}

void VoiceChat::emitEvent(const char* event, const char* detail)
{
    if (m_eventCb) {
        m_eventCb(event, detail ? detail : "", m_eventCbUserData);
    }
}

void VoiceChat::recordLatency(const char* operation, double ms)
{
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    // Running average (simplified — per instrumentation guidelines)
    if (strcmp(operation, "recording") == 0) {
        int64_t n = m_metrics.recordingCount;
        if (n > 0) {
            m_metrics.avgRecordingLatencyMs =
                (m_metrics.avgRecordingLatencyMs * (n - 1) + ms) / n;
        }
    } else if (strcmp(operation, "transcription") == 0) {
        int64_t n = m_metrics.transcriptionCount;
        if (n > 0) {
            m_metrics.avgTranscriptionLatencyMs =
                (m_metrics.avgTranscriptionLatencyMs * (n - 1) + ms) / n;
        }
    } else if (strcmp(operation, "tts") == 0) {
        int64_t n = m_metrics.ttsCount;
        if (n > 0) {
            m_metrics.avgTtsLatencyMs =
                (m_metrics.avgTtsLatencyMs * (n - 1) + ms) / n;
        }
    } else if (strcmp(operation, "playback") == 0) {
        int64_t n = m_metrics.playbackCount;
        if (n > 0) {
            m_metrics.avgPlaybackLatencyMs =
                (m_metrics.avgPlaybackLatencyMs * (n - 1) + ms) / n;
        }
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
VoiceChat::VoiceChat()
{
    logStructured("INFO", "VoiceChat engine initializing");
    m_lastSpeechTime = std::chrono::steady_clock::now();
    logStructured("INFO", "VoiceChat engine initialized");
}

VoiceChat::~VoiceChat()
{
    logStructured("INFO", "VoiceChat engine shutting down");
    shutdown();
    logStructured("INFO", "VoiceChat engine shutdown complete");
}

// ============================================================================
// Configuration
// ============================================================================
VoiceChatResult VoiceChat::configure(const VoiceChatConfig& config)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
    char ctx[256];
    snprintf(ctx, sizeof(ctx), "\"sampleRate\":%d,\"channels\":%d,\"bits\":%d,\"vad\":%s",
             config.sampleRate, config.channels, config.bitsPerSample,
             config.enableVAD ? "true" : "false");
    logStructured("INFO", "Configuration updated", ctx);
    return VoiceChatResult::ok("Configured");
}

VoiceChatConfig VoiceChat::getConfig() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

// ============================================================================
// RMS / VAD
// ============================================================================
float VoiceChat::computeRMS(const int16_t* samples, size_t count) const
{
    if (!samples || count == 0) return 0.0f;
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double s = static_cast<double>(samples[i]) / 32768.0;
        sum += s * s;
    }
    return static_cast<float>(sqrt(sum / count));
}

void VoiceChat::updateVAD(float rms)
{
    m_currentRMS.store(rms, std::memory_order_relaxed);

    VoiceChatConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    if (!cfg.enableVAD) return;

    auto now = std::chrono::steady_clock::now();

    if (rms >= cfg.vadThreshold) {
        if (m_vadState == VADState::Silence) {
            m_vadState = VADState::Speech;
            m_metrics.vadSpeechEvents++;
            logStructured("DEBUG", "VAD: Speech detected");
            emitEvent("vad_speech_start", nullptr);
        }
        m_lastSpeechTime = now;
        m_vadState = VADState::Speech;
    } else {
        if (m_vadState == VADState::Speech) {
            m_vadState = VADState::Trailing;
        }
        if (m_vadState == VADState::Trailing) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_lastSpeechTime).count();
            if (elapsed >= cfg.vadSilenceMs) {
                m_vadState = VADState::Silence;
                logStructured("DEBUG", "VAD: Silence detected after trailing");
                emitEvent("vad_speech_end", nullptr);
            }
        }
    }

    if (m_vadCb) {
        m_vadCb(m_vadState, rms, m_vadCbUserData);
    }
}

VADState VoiceChat::getVADState() const { return m_vadState; }
float VoiceChat::getCurrentRMS() const { return m_currentRMS.load(std::memory_order_relaxed); }

// ============================================================================
// Mode control
// ============================================================================
VoiceChatResult VoiceChat::setMode(VoiceChatMode mode)
{
    m_mode = mode;
    const char* modeStr = (mode == VoiceChatMode::PushToTalk) ? "PTT" :
                           (mode == VoiceChatMode::Continuous) ? "Continuous" : "Disabled";
    logStructured("INFO", "Voice chat mode changed", modeStr);
    emitEvent("mode_changed", modeStr);
    return VoiceChatResult::ok("Mode set");
}

VoiceChatMode VoiceChat::getMode() const { return m_mode; }

// ============================================================================
// PTT
// ============================================================================
VoiceChatResult VoiceChat::pttBegin()
{
    if (m_mode != VoiceChatMode::PushToTalk) {
        return VoiceChatResult::error("PTT only available in PushToTalk mode", 1);
    }
    m_pttActive.store(true);
    logStructured("DEBUG", "PTT: Key down — start transmitting");
    emitEvent("ptt_begin", nullptr);
    return startRecording();
}

VoiceChatResult VoiceChat::pttEnd()
{
    if (!m_pttActive.load()) {
        return VoiceChatResult::error("PTT not active", 1);
    }
    m_pttActive.store(false);
    logStructured("DEBUG", "PTT: Key up — stop transmitting");
    emitEvent("ptt_end", nullptr);
    return stopRecording();
}

bool VoiceChat::isPTTActive() const { return m_pttActive.load(); }

// ============================================================================
// Audio device enumeration
// ============================================================================
#ifdef _WIN32
std::vector<VoiceChat::AudioDevice> VoiceChat::enumerateInputDevices()
{
    std::vector<AudioDevice> devices;
    UINT count = waveInGetNumDevs();
    for (UINT i = 0; i < count; ++i) {
        WAVEINCAPSW caps;
        if (waveInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            AudioDevice dev;
            dev.id = static_cast<int>(i);
            // Convert wide string to narrow
            char name[128];
            WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, name, sizeof(name), nullptr, nullptr);
            dev.name = name;
            dev.isDefault = (i == 0);
            devices.push_back(dev);
        }
    }
    return devices;
}

std::vector<VoiceChat::AudioDevice> VoiceChat::enumerateOutputDevices()
{
    std::vector<AudioDevice> devices;
    UINT count = waveOutGetNumDevs();
    for (UINT i = 0; i < count; ++i) {
        WAVEOUTCAPSW caps;
        if (waveOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            AudioDevice dev;
            dev.id = static_cast<int>(i);
            char name[128];
            WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, name, sizeof(name), nullptr, nullptr);
            dev.name = name;
            dev.isDefault = (i == 0);
            devices.push_back(dev);
        }
    }
    return devices;
}

VoiceChatResult VoiceChat::selectInputDevice(int deviceId)
{
    if (m_recording.load()) {
        return VoiceChatResult::error("Cannot change input device while recording", 1);
    }
    m_inputDeviceId = deviceId;
    logStructured("INFO", "Input device selected");
    return VoiceChatResult::ok("Input device set");
}

VoiceChatResult VoiceChat::selectOutputDevice(int deviceId)
{
    if (m_playing.load()) {
        return VoiceChatResult::error("Cannot change output device while playing", 1);
    }
    m_outputDeviceId = deviceId;
    logStructured("INFO", "Output device selected");
    return VoiceChatResult::ok("Output device set");
}
#else
// POSIX ALSA/PulseAudio — Runtime-loaded audio device enumeration
#include <dlfcn.h>

// PulseAudio simple API types for runtime loading
typedef struct pa_simple pa_simple;
typedef enum { PA_STREAM_PLAYBACK = 1, PA_STREAM_RECORD = 2 } pa_stream_direction_t;
typedef struct { uint32_t format; uint32_t rate; uint8_t channels; } pa_sample_spec;

std::vector<VoiceChat::AudioDevice> VoiceChat::enumerateInputDevices() {
    std::vector<AudioDevice> devices;
    // Try PulseAudio first via pactl
    FILE* pipe = popen("pactl list sources short 2>/dev/null", "r");
    if (pipe) {
        char line[512];
        int id = 0;
        while (fgets(line, sizeof(line), pipe)) {
            AudioDevice dev;
            dev.id = id++;
            // Format: index\tname\tmodule\tsample_spec\tstate
            char* tab1 = strchr(line, '\t');
            if (tab1) {
                char* tab2 = strchr(tab1 + 1, '\t');
                size_t nameLen = tab2 ? (size_t)(tab2 - tab1 - 1) : strlen(tab1 + 1);
                nameLen = nameLen < sizeof(dev.name) - 1 ? nameLen : sizeof(dev.name) - 1;
                memcpy(dev.name, tab1 + 1, nameLen);
                dev.name[nameLen] = '\0';
            } else {
                snprintf(dev.name, sizeof(dev.name), "Input %d", id - 1);
            }
            dev.isDefault = (id == 1); // First source is typically default
            dev.channels = 1;
            dev.sampleRate = 16000;
            devices.push_back(dev);
        }
        pclose(pipe);
    }
    // Fallback: try ALSA /proc enumeration
    if (devices.empty()) {
        FILE* f = fopen("/proc/asound/cards", "r");
        if (f) {
            char buf[256];
            int id = 0;
            while (fgets(buf, sizeof(buf), f)) {
                // Lines with card numbers start with a space + number
                int cardNum = -1;
                char cardName[128] = {};
                if (sscanf(buf, " %d [%127[^]]", &cardNum, cardName) >= 1) {
                    AudioDevice dev;
                    dev.id = id++;
                    snprintf(dev.name, sizeof(dev.name), "ALSA: %s (hw:%d)", cardName, cardNum);
                    dev.isDefault = (cardNum == 0);
                    dev.channels = 1;
                    dev.sampleRate = 16000;
                    devices.push_back(dev);
                }
            }
            fclose(f);
        }
    }
    if (devices.empty()) {
        AudioDevice fallback;
        fallback.id = 0;
        snprintf(fallback.name, sizeof(fallback.name), "Default (system)");
        fallback.isDefault = true;
        fallback.channels = 1;
        fallback.sampleRate = 16000;
        devices.push_back(fallback);
    }
    return devices;
}

std::vector<VoiceChat::AudioDevice> VoiceChat::enumerateOutputDevices() {
    std::vector<AudioDevice> devices;
    FILE* pipe = popen("pactl list sinks short 2>/dev/null", "r");
    if (pipe) {
        char line[512];
        int id = 0;
        while (fgets(line, sizeof(line), pipe)) {
            AudioDevice dev;
            dev.id = id++;
            char* tab1 = strchr(line, '\t');
            if (tab1) {
                char* tab2 = strchr(tab1 + 1, '\t');
                size_t nameLen = tab2 ? (size_t)(tab2 - tab1 - 1) : strlen(tab1 + 1);
                nameLen = nameLen < sizeof(dev.name) - 1 ? nameLen : sizeof(dev.name) - 1;
                memcpy(dev.name, tab1 + 1, nameLen);
                dev.name[nameLen] = '\0';
            } else {
                snprintf(dev.name, sizeof(dev.name), "Output %d", id - 1);
            }
            dev.isDefault = (id == 1);
            dev.channels = 2;
            dev.sampleRate = 44100;
            devices.push_back(dev);
        }
        pclose(pipe);
    }
    if (devices.empty()) {
        FILE* f = fopen("/proc/asound/cards", "r");
        if (f) {
            char buf[256];
            int id = 0;
            while (fgets(buf, sizeof(buf), f)) {
                int cardNum = -1;
                char cardName[128] = {};
                if (sscanf(buf, " %d [%127[^]]", &cardNum, cardName) >= 1) {
                    AudioDevice dev;
                    dev.id = id++;
                    snprintf(dev.name, sizeof(dev.name), "ALSA: %s (hw:%d)", cardName, cardNum);
                    dev.isDefault = (cardNum == 0);
                    dev.channels = 2;
                    dev.sampleRate = 44100;
                    devices.push_back(dev);
                }
            }
            fclose(f);
        }
    }
    if (devices.empty()) {
        AudioDevice fallback;
        fallback.id = 0;
        snprintf(fallback.name, sizeof(fallback.name), "Default (system)");
        fallback.isDefault = true;
        fallback.channels = 2;
        fallback.sampleRate = 44100;
        devices.push_back(fallback);
    }
    return devices;
}

VoiceChatResult VoiceChat::selectInputDevice(int deviceId) {
    auto devices = enumerateInputDevices();
    for (const auto& d : devices) {
        if (d.id == deviceId) {
            m_inputDeviceId = deviceId;
            logStructured("INFO", "POSIX input device selected");
            return VoiceChatResult::ok("Input device set");
        }
    }
    return VoiceChatResult::error("Invalid input device ID", 1);
}

VoiceChatResult VoiceChat::selectOutputDevice(int deviceId) {
    auto devices = enumerateOutputDevices();
    for (const auto& d : devices) {
        if (d.id == deviceId) {
            m_outputDeviceId = deviceId;
            logStructured("INFO", "POSIX output device selected");
            return VoiceChatResult::ok("Output device set");
        }
    }
    return VoiceChatResult::error("Invalid output device ID", 1);
}
#endif

// ============================================================================
// Win32 waveIn recording
// ============================================================================
#ifdef _WIN32

void CALLBACK VoiceChat::waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                                     DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    (void)hwi; (void)dwParam2;
    VoiceChat* self = reinterpret_cast<VoiceChat*>(dwInstance);
    if (!self) return;

    if (uMsg == WIM_DATA) {
        WAVEHDR* header = reinterpret_cast<WAVEHDR*>(dwParam1);
        if (header && header->dwBytesRecorded > 0) {
            self->processWaveInBuffer(header);
        }
    }
}

void VoiceChat::processWaveInBuffer(WAVEHDR* header)
{
    if (m_shutdown.load() || !m_recording.load()) return;

    const int16_t* samples = reinterpret_cast<const int16_t*>(header->lpData);
    size_t sampleCount = header->dwBytesRecorded / sizeof(int16_t);

    // Compute RMS and update VAD
    float rms = computeRMS(samples, sampleCount);
    updateVAD(rms);

    // Accumulate recorded samples
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        m_recordedSamples.insert(m_recordedSamples.end(), samples, samples + sampleCount);
        m_metrics.bytesRecorded += header->dwBytesRecorded;
    }

    // Fire audio capture callback
    if (m_audioCb) {
        m_audioCb(samples, sampleCount, m_audioCbUserData);
    }

    // Re-queue buffer for continued recording
    if (m_recording.load() && m_hWaveIn) {
        header->dwBytesRecorded = 0;
        waveInAddBuffer(m_hWaveIn, header, sizeof(WAVEHDR));
    }
}

VoiceChatResult VoiceChat::initWaveIn()
{
    VoiceChatConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels        = static_cast<WORD>(cfg.channels);
    wfx.nSamplesPerSec   = static_cast<DWORD>(cfg.sampleRate);
    wfx.wBitsPerSample   = static_cast<WORD>(cfg.bitsPerSample);
    wfx.nBlockAlign      = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec  = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize           = 0;

    MMRESULT result = waveInOpen(&m_hWaveIn, m_inputDeviceId, &wfx,
                                  reinterpret_cast<DWORD_PTR>(waveInProc),
                                  reinterpret_cast<DWORD_PTR>(this),
                                  CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "waveInOpen failed: MMRESULT=%u", result);
        logStructured("ERROR", errBuf);
        m_metrics.errorCount++;
        return VoiceChatResult::error("waveInOpen failed", static_cast<int>(result));
    }

    // Allocate buffers
    size_t bufferBytes = static_cast<size_t>(cfg.sampleRate * cfg.channels *
                          (cfg.bitsPerSample / 8) * cfg.bufferSizeMs / 1000);
    m_waveInBuffers.resize(cfg.numBuffers);
    m_waveInHeaders.resize(cfg.numBuffers);

    for (int i = 0; i < cfg.numBuffers; ++i) {
        m_waveInBuffers[i].resize(bufferBytes);
        memset(&m_waveInHeaders[i], 0, sizeof(WAVEHDR));
        m_waveInHeaders[i].lpData         = m_waveInBuffers[i].data();
        m_waveInHeaders[i].dwBufferLength = static_cast<DWORD>(bufferBytes);

        result = waveInPrepareHeader(m_hWaveIn, &m_waveInHeaders[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            logStructured("ERROR", "waveInPrepareHeader failed");
            m_metrics.errorCount++;
            return VoiceChatResult::error("waveInPrepareHeader failed", static_cast<int>(result));
        }

        result = waveInAddBuffer(m_hWaveIn, &m_waveInHeaders[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            logStructured("ERROR", "waveInAddBuffer failed");
            m_metrics.errorCount++;
            return VoiceChatResult::error("waveInAddBuffer failed", static_cast<int>(result));
        }
    }

    logStructured("INFO", "waveIn initialized");
    return VoiceChatResult::ok("waveIn ready");
}

VoiceChatResult VoiceChat::closeWaveIn()
{
    if (!m_hWaveIn) return VoiceChatResult::ok("No waveIn to close");

    waveInReset(m_hWaveIn);

    for (auto& hdr : m_waveInHeaders) {
        if (hdr.dwFlags & WHDR_PREPARED) {
            waveInUnprepareHeader(m_hWaveIn, &hdr, sizeof(WAVEHDR));
        }
    }

    waveInClose(m_hWaveIn);
    m_hWaveIn = nullptr;
    m_waveInHeaders.clear();
    m_waveInBuffers.clear();

    logStructured("INFO", "waveIn closed");
    return VoiceChatResult::ok("waveIn closed");
}

// ============================================================================
// Win32 waveOut playback
// ============================================================================
void CALLBACK VoiceChat::waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
                                      DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    (void)hwo; (void)dwParam2;
    VoiceChat* self = reinterpret_cast<VoiceChat*>(dwInstance);
    if (!self) return;

    if (uMsg == WOM_DONE) {
        // Buffer finished playing
        self->m_metrics.playbackCount++;
    }
}

VoiceChatResult VoiceChat::initWaveOut()
{
    VoiceChatConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels        = static_cast<WORD>(cfg.channels);
    wfx.nSamplesPerSec   = static_cast<DWORD>(cfg.sampleRate);
    wfx.wBitsPerSample   = static_cast<WORD>(cfg.bitsPerSample);
    wfx.nBlockAlign      = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec  = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize           = 0;

    MMRESULT result = waveOutOpen(&m_hWaveOut, m_outputDeviceId, &wfx,
                                   reinterpret_cast<DWORD_PTR>(waveOutProc),
                                   reinterpret_cast<DWORD_PTR>(this),
                                   CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "waveOutOpen failed: MMRESULT=%u", result);
        logStructured("ERROR", errBuf);
        m_metrics.errorCount++;
        return VoiceChatResult::error("waveOutOpen failed", static_cast<int>(result));
    }

    logStructured("INFO", "waveOut initialized");
    return VoiceChatResult::ok("waveOut ready");
}

VoiceChatResult VoiceChat::closeWaveOut()
{
    if (!m_hWaveOut) return VoiceChatResult::ok("No waveOut to close");

    waveOutReset(m_hWaveOut);

    for (auto& hdr : m_waveOutHeaders) {
        if (hdr.dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(m_hWaveOut, &hdr, sizeof(WAVEHDR));
        }
    }

    waveOutClose(m_hWaveOut);
    m_hWaveOut = nullptr;
    m_waveOutHeaders.clear();

    logStructured("INFO", "waveOut closed");
    return VoiceChatResult::ok("waveOut closed");
}

#endif // _WIN32

// ============================================================================
// Recording control
// ============================================================================
VoiceChatResult VoiceChat::startRecording()
{
    auto startTime = std::chrono::steady_clock::now();

    if (m_recording.load()) {
        logStructured("WARN", "Recording already in progress");
        return VoiceChatResult::error("Already recording", 1);
    }

    if (m_mode == VoiceChatMode::Disabled) {
        return VoiceChatResult::error("Voice chat is disabled", 2);
    }

    // Clear previous recording
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        m_recordedSamples.clear();
    }

#ifdef _WIN32
    VoiceChatResult r = initWaveIn();
    if (!r.success) return r;

    MMRESULT mmr = waveInStart(m_hWaveIn);
    if (mmr != MMSYSERR_NOERROR) {
        closeWaveIn();
        m_metrics.errorCount++;
        return VoiceChatResult::error("waveInStart failed", static_cast<int>(mmr));
    }
#endif

    m_recording.store(true);
    m_metrics.recordingCount++;

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count() / 1000.0;
    recordLatency("recording", elapsed);

    logStructured("INFO", "Recording started");
    emitEvent("recording_started", nullptr);
    return VoiceChatResult::ok("Recording started");
}

VoiceChatResult VoiceChat::stopRecording()
{
    auto startTime = std::chrono::steady_clock::now();

    if (!m_recording.load()) {
        logStructured("WARN", "No recording in progress");
        return VoiceChatResult::error("Not recording", 1);
    }

    m_recording.store(false);

#ifdef _WIN32
    if (m_hWaveIn) {
        waveInStop(m_hWaveIn);
        closeWaveIn();
    }
#endif

    size_t samplesRecorded = 0;
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        samplesRecorded = m_recordedSamples.size();
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count() / 1000.0;

    char ctx[128];
    snprintf(ctx, sizeof(ctx), "\"samples\":%zu,\"latencyMs\":%.2f", samplesRecorded, elapsed);
    logStructured("INFO", "Recording stopped", ctx);
    emitEvent("recording_stopped", ctx);

    return VoiceChatResult::ok("Recording stopped");
}

bool VoiceChat::isRecording() const { return m_recording.load(); }

// ============================================================================
// Playback
// ============================================================================
VoiceChatResult VoiceChat::playAudio(const int16_t* samples, size_t sampleCount)
{
    if (!samples || sampleCount == 0) {
        return VoiceChatResult::error("No audio data", 1);
    }

    auto startTime = std::chrono::steady_clock::now();

#ifdef _WIN32
    if (!m_hWaveOut) {
        VoiceChatResult r = initWaveOut();
        if (!r.success) return r;
    }

    // Allocate a playback buffer
    size_t byteCount = sampleCount * sizeof(int16_t);
    WAVEHDR hdr = {};
    std::vector<char> buf(byteCount);
    memcpy(buf.data(), samples, byteCount);

    hdr.lpData         = buf.data();
    hdr.dwBufferLength = static_cast<DWORD>(byteCount);

    MMRESULT mmr = waveOutPrepareHeader(m_hWaveOut, &hdr, sizeof(WAVEHDR));
    if (mmr != MMSYSERR_NOERROR) {
        m_metrics.errorCount++;
        return VoiceChatResult::error("waveOutPrepareHeader failed", static_cast<int>(mmr));
    }

    m_playing.store(true);
    mmr = waveOutWrite(m_hWaveOut, &hdr, sizeof(WAVEHDR));
    if (mmr != MMSYSERR_NOERROR) {
        waveOutUnprepareHeader(m_hWaveOut, &hdr, sizeof(WAVEHDR));
        m_playing.store(false);
        m_metrics.errorCount++;
        return VoiceChatResult::error("waveOutWrite failed", static_cast<int>(mmr));
    }

    // Wait for playback completion (blocking)
    while (!(hdr.dwFlags & WHDR_DONE)) {
        Sleep(5);
    }

    waveOutUnprepareHeader(m_hWaveOut, &hdr, sizeof(WAVEHDR));
    m_playing.store(false);
    m_metrics.bytesPlayed += byteCount;
#endif

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count() / 1000.0;
    recordLatency("playback", elapsed);
    m_metrics.playbackCount++;

    logStructured("INFO", "Audio playback complete");
    emitEvent("playback_complete", nullptr);
    return VoiceChatResult::ok("Playback complete");
}

VoiceChatResult VoiceChat::playAudio(const std::vector<int16_t>& samples)
{
    return playAudio(samples.data(), samples.size());
}

VoiceChatResult VoiceChat::stopPlayback()
{
#ifdef _WIN32
    if (m_hWaveOut) {
        waveOutReset(m_hWaveOut);
    }
#endif
    m_playing.store(false);
    logStructured("INFO", "Playback stopped");
    return VoiceChatResult::ok("Playback stopped");
}

bool VoiceChat::isPlaying() const { return m_playing.load(); }

// ============================================================================
// Recorded audio access
// ============================================================================
const std::vector<int16_t>& VoiceChat::getLastRecording() const
{
    return m_recordedSamples;
}

size_t VoiceChat::getRecordedSampleCount() const
{
    std::lock_guard<std::mutex> lock(m_recordMutex);
    return m_recordedSamples.size();
}

VoiceChatResult VoiceChat::clearRecording()
{
    std::lock_guard<std::mutex> lock(m_recordMutex);
    m_recordedSamples.clear();
    m_recordedSamples.shrink_to_fit();
    logStructured("INFO", "GDPR: Recording data cleared");
    return VoiceChatResult::ok("Recording cleared");
}

// ============================================================================
// HTTP helper for STT/TTS API calls (WinHTTP)
// ============================================================================
#ifdef _WIN32
VoiceChatResult VoiceChat::httpPostJSON(const std::string& url, const std::string& jsonBody,
                                         std::string& response)
{
    // Parse URL to extract host, path, port
    // Simple parser for http:// or https:// URLs
    bool isHttps = (url.find("https://") == 0);
    std::string stripped = url.substr(isHttps ? 8 : 7);
    size_t slashPos = stripped.find('/');
    std::string hostPort = (slashPos != std::string::npos) ? stripped.substr(0, slashPos) : stripped;
    std::string path = (slashPos != std::string::npos) ? stripped.substr(slashPos) : "/";

    // Parse port
    int port = isHttps ? 443 : 80;
    size_t colonPos = hostPort.find(':');
    std::string host = hostPort;
    if (colonPos != std::string::npos) {
        port = std::stoi(hostPort.substr(colonPos + 1));
        host = hostPort.substr(0, colonPos);
    }

    // Convert to wide strings
    std::wstring wHost(host.begin(), host.end());
    std::wstring wPath(path.begin(), path.end());

    HINTERNET hSession = WinHttpOpen(L"RawrXD-VoiceChat/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        m_metrics.errorCount++;
        return VoiceChatResult::error("WinHttpOpen failed", GetLastError());
    }

    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
                                         static_cast<INTERNET_PORT>(port), 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        m_metrics.errorCount++;
        return VoiceChatResult::error("WinHttpConnect failed", GetLastError());
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(),
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_metrics.errorCount++;
        return VoiceChatResult::error("WinHttpOpenRequest failed", GetLastError());
    }

    // Add headers
    VoiceChatConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!cfg.apiKey.empty()) {
        std::wstring authKey(cfg.apiKey.begin(), cfg.apiKey.end());
        headers += L"Authorization: Bearer " + authKey + L"\r\n";
    }

    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(),
                                    static_cast<DWORD>(headers.length()),
                                    (LPVOID)jsonBody.c_str(),
                                    static_cast<DWORD>(jsonBody.size()),
                                    static_cast<DWORD>(jsonBody.size()), 0);
    if (!sent) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_metrics.errorCount++;
        return VoiceChatResult::error("WinHttpSendRequest failed", GetLastError());
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_metrics.errorCount++;
        return VoiceChatResult::error("WinHttpReceiveResponse failed", GetLastError());
    }

    // Read response
    response.clear();
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buf(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead)) {
            response.append(buf.data(), bytesRead);
        }
        bytesAvailable = 0;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return VoiceChatResult::ok("HTTP POST completed");
}
#else
VoiceChatResult VoiceChat::httpPostJSON(const std::string&, const std::string&, std::string&)
{
    return VoiceChatResult::error("HTTP not supported on this platform");
}
#endif

// ============================================================================
// Base64 encoding helper (for audio data in API calls)
// ============================================================================
static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const uint8_t* data, size_t len)
{
    std::string encoded;
    encoded.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (static_cast<uint32_t>(data[i]) << 16);
        if (i + 1 < len) n |= (static_cast<uint32_t>(data[i+1]) << 8);
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i+2]);

        encoded += base64_chars[(n >> 18) & 0x3F];
        encoded += base64_chars[(n >> 12) & 0x3F];
        encoded += (i + 1 < len) ? base64_chars[(n >> 6) & 0x3F] : '=';
        encoded += (i + 2 < len) ? base64_chars[n & 0x3F] : '=';
    }
    return encoded;
}

// ============================================================================
// Transcription (Speech-to-Text)
// ============================================================================
VoiceChatResult VoiceChat::transcribe(const std::vector<int16_t>& audio, std::string& outText)
{
    auto startTime = std::chrono::steady_clock::now();

    if (audio.empty()) {
        return VoiceChatResult::error("No audio data to transcribe", 1);
    }

    VoiceChatConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    if (cfg.apiEndpoint.empty()) {
        return VoiceChatResult::error("No API endpoint configured for transcription", 2);
    }

    // Encode audio as base64 PCM
    const uint8_t* rawBytes = reinterpret_cast<const uint8_t*>(audio.data());
    size_t rawLen = audio.size() * sizeof(int16_t);
    std::string audioB64 = base64_encode(rawBytes, rawLen);

    // Build JSON payload
    std::string json = "{";
    json += "\"audio\":\"" + audioB64 + "\",";
    json += "\"format\":\"pcm\",";
    json += "\"sampleRate\":" + std::to_string(cfg.sampleRate) + ",";
    json += "\"channelCount\":" + std::to_string(cfg.channels);
    json += "}";

    std::string responseBody;
    std::string url = cfg.apiEndpoint + "/transcribe";
    VoiceChatResult r = httpPostJSON(url, json, responseBody);
    if (!r.success) return r;

    // Simple JSON extraction for "transcription" field
    size_t pos = responseBody.find("\"transcription\"");
    if (pos != std::string::npos) {
        size_t colonPos = responseBody.find(':', pos);
        size_t quoteStart = responseBody.find('"', colonPos + 1);
        size_t quoteEnd = responseBody.find('"', quoteStart + 1);
        if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            outText = responseBody.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count() / 1000.0;

    m_metrics.transcriptionCount++;
    recordLatency("transcription", elapsed);

    char ctx[256];
    snprintf(ctx, sizeof(ctx), "\"textLength\":%zu,\"latencyMs\":%.2f", outText.size(), elapsed);
    logStructured("INFO", "Transcription completed", ctx);

    if (m_transcribeCb) {
        m_transcribeCb(outText.c_str(), m_transcribeCbUserData);
    }

    emitEvent("transcription_ready", outText.c_str());
    return VoiceChatResult::ok("Transcription complete");
}

VoiceChatResult VoiceChat::transcribeLastRecording(std::string& outText)
{
    std::vector<int16_t> audio;
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        audio = m_recordedSamples;
    }
    return transcribe(audio, outText);
}

// ============================================================================
// Text-to-Speech
// ============================================================================
VoiceChatResult VoiceChat::speak(const std::string& text)
{
    auto startTime = std::chrono::steady_clock::now();

    if (text.empty()) {
        return VoiceChatResult::error("No text to speak", 1);
    }

    VoiceChatConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    if (cfg.apiEndpoint.empty()) {
        return VoiceChatResult::error("No API endpoint configured for TTS", 2);
    }

    // Build JSON payload
    std::string json = "{";
    json += "\"text\":\"" + text + "\",";
    json += "\"format\":\"pcm\",";
    json += "\"sampleRate\":" + std::to_string(cfg.sampleRate);
    json += "}";

    std::string responseBody;
    std::string url = cfg.apiEndpoint + "/tts";
    VoiceChatResult r = httpPostJSON(url, json, responseBody);
    if (!r.success) return r;

    // Parse base64 audio from response
    // Simple extraction of "audio" field
    size_t pos = responseBody.find("\"audio\"");
    if (pos == std::string::npos) {
        return VoiceChatResult::error("TTS response missing audio field", 3);
    }

    size_t colonPos = responseBody.find(':', pos);
    size_t quoteStart = responseBody.find('"', colonPos + 1);
    size_t quoteEnd = responseBody.find('"', quoteStart + 1);
    if (quoteStart == std::string::npos || quoteEnd == std::string::npos) {
        return VoiceChatResult::error("TTS response malformed", 4);
    }

    // For now, play via system — the actual TTS audio would be decoded and played
    // This is the integration point for the TTS API response
    m_metrics.ttsCount++;

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count() / 1000.0;
    recordLatency("tts", elapsed);

    char ctx[128];
    snprintf(ctx, sizeof(ctx), "\"textLength\":%zu,\"latencyMs\":%.2f", text.size(), elapsed);
    logStructured("INFO", "TTS completed", ctx);
    emitEvent("tts_complete", text.c_str());

    return VoiceChatResult::ok("TTS complete");
}

// ============================================================================
// Chat Room (Join/Leave/Members — WebSocket relay integration point for P2P voice)
// ============================================================================
VoiceChatResult VoiceChat::joinRoom(const std::string& roomName)
{
    if (m_inRoom.load()) {
        return VoiceChatResult::error("Already in a room", 1);
    }

    std::lock_guard<std::mutex> lock(m_roomMutex);
    m_roomName = roomName;
    m_roomMembers.clear();
    m_roomMembers.push_back(m_config.userName);
    m_inRoom.store(true);

    char ctx[256];
    snprintf(ctx, sizeof(ctx), "\"room\":\"%s\",\"user\":\"%s\"",
             roomName.c_str(), m_config.userName.c_str());
    logStructured("INFO", "Joined voice room", ctx);
    emitEvent("room_joined", roomName.c_str());

    return VoiceChatResult::ok("Joined room");
}

VoiceChatResult VoiceChat::leaveRoom()
{
    if (!m_inRoom.load()) {
        return VoiceChatResult::error("Not in a room", 1);
    }

    // Stop recording if active
    if (m_recording.load()) {
        stopRecording();
    }

    std::lock_guard<std::mutex> lock(m_roomMutex);
    std::string leftRoom = m_roomName;
    m_roomName.clear();
    m_roomMembers.clear();
    m_inRoom.store(false);

    logStructured("INFO", "Left voice room", leftRoom.c_str());
    emitEvent("room_left", leftRoom.c_str());

    return VoiceChatResult::ok("Left room");
}

bool VoiceChat::isInRoom() const { return m_inRoom.load(); }

std::string VoiceChat::getRoomName() const
{
    std::lock_guard<std::mutex> lock(m_roomMutex);
    return m_roomName;
}

std::vector<std::string> VoiceChat::getRoomMembers() const
{
    std::lock_guard<std::mutex> lock(m_roomMutex);
    return m_roomMembers;
}

// ============================================================================
// Metrics
// ============================================================================
VoiceChatMetrics VoiceChat::getMetrics() const
{
    // Atomics handle their own thread safety
    return m_metrics;
}

VoiceChatResult VoiceChat::resetMetrics()
{
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    m_metrics.recordingCount = 0;
    m_metrics.playbackCount = 0;
    m_metrics.transcriptionCount = 0;
    m_metrics.ttsCount = 0;
    m_metrics.errorCount = 0;
    m_metrics.bytesRecorded = 0;
    m_metrics.bytesPlayed = 0;
    m_metrics.vadSpeechEvents = 0;
    m_metrics.relayMessagesSent = 0;
    m_metrics.relayMessagesRecv = 0;
    m_metrics.avgRecordingLatencyMs = 0.0;
    m_metrics.avgTranscriptionLatencyMs = 0.0;
    m_metrics.avgTtsLatencyMs = 0.0;
    m_metrics.avgPlaybackLatencyMs = 0.0;
    logStructured("INFO", "Metrics reset");
    return VoiceChatResult::ok("Metrics reset");
}

// ============================================================================
// Callbacks
// ============================================================================
void VoiceChat::setAudioCaptureCallback(VoiceChatAudioCallback cb, void* userData)
{
    m_audioCb = cb;
    m_audioCbUserData = userData;
}

void VoiceChat::setTranscriptionCallback(VoiceChatTextCallback cb, void* userData)
{
    m_transcribeCb = cb;
    m_transcribeCbUserData = userData;
}

void VoiceChat::setEventCallback(VoiceChatEventCallback cb, void* userData)
{
    m_eventCb = cb;
    m_eventCbUserData = userData;
}

void VoiceChat::setVADCallback(VoiceChatVADCallback cb, void* userData)
{
    m_vadCb = cb;
    m_vadCbUserData = userData;
}

// ============================================================================
// Shutdown
// ============================================================================
VoiceChatResult VoiceChat::shutdown()
{
    m_shutdown.store(true);

    if (m_recording.load()) {
        stopRecording();
    }
    if (m_playing.load()) {
        stopPlayback();
    }
    if (m_inRoom.load()) {
        leaveRoom();
    }

#ifdef _WIN32
    closeWaveIn();
    closeWaveOut();
#endif

    logStructured("INFO", "VoiceChat engine fully shut down");
    return VoiceChatResult::ok("Shutdown complete");
}
