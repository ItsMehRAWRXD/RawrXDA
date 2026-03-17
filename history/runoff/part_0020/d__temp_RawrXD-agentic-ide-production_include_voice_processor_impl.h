#pragma once

#include <string>
#include <vector>
#include <functional>

// ============================================================================
// VOICE PROCESSOR - AUDIO I/O AND SPEECH RECOGNITION
// ============================================================================

enum class VoiceAccent {
    BritishEnglish,
    AmericanEnglish,
    AustralianEnglish,
    IndianEnglish,
    CanadianEnglish,
    IrishEnglish
};

class VoiceProcessor {
public:
    VoiceProcessor();
    ~VoiceProcessor();

    // Recording
    void startRecording();
    void stopRecording();
    bool isRecording() const { return m_recording; }

    // Playback
    void play(const std::string& text, VoiceAccent accent = VoiceAccent::AmericanEnglish);
    void stop();

    // Audio settings
    void setMicrophoneVolume(int volume);  // 0-100
    void setSpeakerVolume(int volume);     // 0-100
    void setAccent(VoiceAccent accent) { m_accent = accent; }
    VoiceAccent getAccent() const { return m_accent; }

    // Voice recognition
    std::string recognizeAudio();
    std::vector<std::string> getAvailableAccents() const;

    // Callbacks
    using AudioCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    void setAudioReceivedCallback(AudioCallback cb) { m_audioCallback = cb; }
    void setErrorCallback(ErrorCallback cb) { m_errorCallback = cb; }

private:
    bool m_recording = false;
    VoiceAccent m_accent = VoiceAccent::AmericanEnglish;
    int m_micVolume = 100;
    int m_speakerVolume = 100;
    std::vector<uint8_t> m_audioBuffer;

    AudioCallback m_audioCallback;
    ErrorCallback m_errorCallback;
};

#endif
