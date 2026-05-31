// ============================================================================
// VoiceChat — Microphone-disabled implementation
// No direct microphone/audio capture backend is linked in this build.
// ============================================================================

#include "voice_chat.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>

void VoiceChat::logStructured(const char* level, const char* message, const char* context)
{
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
    if (strcmp(operation, "recording") == 0) {
        int64_t n = std::max<int64_t>(1, m_metrics.recordingCount);
        m_metrics.avgRecordingLatencyMs = (m_metrics.avgRecordingLatencyMs * (n - 1) + ms) / n;
    } else if (strcmp(operation, "transcription") == 0) {
        int64_t n = std::max<int64_t>(1, m_metrics.transcriptionCount);
        m_metrics.avgTranscriptionLatencyMs = (m_metrics.avgTranscriptionLatencyMs * (n - 1) + ms) / n;
    } else if (strcmp(operation, "tts") == 0) {
        int64_t n = std::max<int64_t>(1, m_metrics.ttsCount);
        m_metrics.avgTtsLatencyMs = (m_metrics.avgTtsLatencyMs * (n - 1) + ms) / n;
    } else if (strcmp(operation, "playback") == 0) {
        int64_t n = std::max<int64_t>(1, m_metrics.playbackCount);
        m_metrics.avgPlaybackLatencyMs = (m_metrics.avgPlaybackLatencyMs * (n - 1) + ms) / n;
    }
}

VoiceChat::VoiceChat()
{
    m_lastSpeechTime = std::chrono::steady_clock::now();
    logStructured("INFO", "VoiceChat initialized (microphone disabled build)");
}

VoiceChat::~VoiceChat()
{
    shutdown();
}

VoiceChatResult VoiceChat::configure(const VoiceChatConfig& config)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
    return VoiceChatResult::ok("Configured");
}

VoiceChatConfig VoiceChat::getConfig() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

float VoiceChat::computeRMS(const int16_t* samples, size_t count) const
{
    if (!samples || count == 0) {
        return 0.0f;
    }

    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double s = static_cast<double>(samples[i]) / 32768.0;
        sum += s * s;
    }
    return static_cast<float>(sqrt(sum / static_cast<double>(count)));
}

void VoiceChat::updateVAD(float rms)
{
    m_currentRMS.store(rms, std::memory_order_relaxed);

    VoiceChatConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

    if (!cfg.enableVAD) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (rms >= cfg.vadThreshold) {
        m_vadState = VADState::Speech;
        m_lastSpeechTime = now;
    } else {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastSpeechTime).count();
        if (elapsed >= cfg.vadSilenceMs) {
            m_vadState = VADState::Silence;
        }
    }

    if (m_vadCb) {
        m_vadCb(m_vadState, rms, m_vadCbUserData);
    }
}

VADState VoiceChat::getVADState() const { return m_vadState; }
float VoiceChat::getCurrentRMS() const { return m_currentRMS.load(std::memory_order_relaxed); }

VoiceChatResult VoiceChat::setMode(VoiceChatMode mode)
{
    m_mode = mode;
    return VoiceChatResult::ok("Mode set");
}

VoiceChatMode VoiceChat::getMode() const { return m_mode; }

VoiceChatResult VoiceChat::pttBegin()
{
    m_pttActive.store(true);
    return VoiceChatResult::error("Microphone capture is disabled in this build", 1);
}

VoiceChatResult VoiceChat::pttEnd()
{
    m_pttActive.store(false);
    return VoiceChatResult::ok("PTT ended");
}

bool VoiceChat::isPTTActive() const { return m_pttActive.load(); }

std::vector<VoiceChat::AudioDevice> VoiceChat::enumerateInputDevices()
{
    return {};
}

std::vector<VoiceChat::AudioDevice> VoiceChat::enumerateOutputDevices()
{
    std::vector<AudioDevice> devices;
    AudioDevice d{};
    d.id = 0;
    d.name = "Default output";
    d.isDefault = true;
    devices.push_back(d);
    return devices;
}

VoiceChatResult VoiceChat::selectInputDevice(int deviceId)
{
    m_inputDeviceId = deviceId;
    return VoiceChatResult::error("Microphone capture is disabled in this build", 1);
}

VoiceChatResult VoiceChat::selectOutputDevice(int deviceId)
{
    m_outputDeviceId = deviceId;
    return VoiceChatResult::ok("Output device set");
}

VoiceChatResult VoiceChat::startRecording()
{
    return VoiceChatResult::error("Microphone recording is disabled in this build", 1);
}

VoiceChatResult VoiceChat::stopRecording()
{
    return VoiceChatResult::error("Microphone recording is disabled in this build", 1);
}

bool VoiceChat::isRecording() const { return m_recording.load(); }

VoiceChatResult VoiceChat::playAudio(const int16_t* samples, size_t sampleCount)
{
    if (!samples || sampleCount == 0) {
        return VoiceChatResult::error("No audio data", 1);
    }

    m_playing.store(true);
    m_metrics.playbackCount++;
    m_metrics.bytesPlayed += static_cast<int64_t>(sampleCount * sizeof(int16_t));
    m_playing.store(false);
    emitEvent("playback_complete", nullptr);
    return VoiceChatResult::ok("Playback complete");
}

VoiceChatResult VoiceChat::playAudio(const std::vector<int16_t>& samples)
{
    return playAudio(samples.data(), samples.size());
}

VoiceChatResult VoiceChat::stopPlayback()
{
    m_playing.store(false);
    return VoiceChatResult::ok("Playback stopped");
}

bool VoiceChat::isPlaying() const { return m_playing.load(); }

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
    return VoiceChatResult::ok("Recording cleared");
}

VoiceChatResult VoiceChat::httpPostJSON(const std::string&, const std::string&, std::string&)
{
    return VoiceChatResult::error("Voice HTTP backend disabled in this build", 1);
}

VoiceChatResult VoiceChat::transcribe(const std::vector<int16_t>& audio, std::string& outText)
{
    if (audio.empty()) {
        return VoiceChatResult::error("No audio data to transcribe", 1);
    }

    outText = "";
    m_metrics.transcriptionCount++;
    emitEvent("transcription_ready", outText.c_str());
    return VoiceChatResult::ok("Transcription complete (disabled backend)");
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

VoiceChatResult VoiceChat::speak(const std::string& text)
{
    if (text.empty()) {
        return VoiceChatResult::error("No text to speak", 1);
    }

    m_metrics.ttsCount++;
    emitEvent("tts_complete", text.c_str());
    return VoiceChatResult::ok("TTS complete");
}

VoiceChatResult VoiceChat::joinRoom(const std::string& roomName)
{
    if (roomName.empty()) {
        return VoiceChatResult::error("Room name required", 1);
    }

    {
        std::lock_guard<std::mutex> lock(m_roomMutex);
        m_roomName = roomName;
        m_roomMembers.clear();
        m_roomMembers.push_back(m_config.userName);
    }
    m_inRoom.store(true);
    emitEvent("room_joined", roomName.c_str());
    return VoiceChatResult::ok("Joined room");
}

VoiceChatResult VoiceChat::leaveRoom()
{
    {
        std::lock_guard<std::mutex> lock(m_roomMutex);
        m_roomName.clear();
        m_roomMembers.clear();
    }
    m_inRoom.store(false);
    emitEvent("room_left", nullptr);
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

VoiceChatMetrics VoiceChat::getMetrics() const
{
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    return m_metrics;
}

VoiceChatResult VoiceChat::resetMetrics()
{
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    m_metrics = VoiceChatMetrics{};
    return VoiceChatResult::ok("Metrics reset");
}

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

VoiceChatResult VoiceChat::shutdown()
{
    m_shutdown.store(true);
    m_recording.store(false);
    m_playing.store(false);
    m_pttActive.store(false);
    return VoiceChatResult::ok("VoiceChat shutdown");
}
