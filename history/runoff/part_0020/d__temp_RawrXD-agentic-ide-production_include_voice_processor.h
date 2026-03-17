#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>

// Minimal, cross-platform interface. Platform hooks are stubbed for CLI builds.
class VoiceProcessor {
public:
    enum Accent { English = 0, Australian, GBE, American };

    VoiceProcessor();
    ~VoiceProcessor();

    // Voice input
    bool startRecording();
    void stopRecording();
    bool isRecording() const { return m_isRecording.load(); }

    // Voice output
    bool speakText(const std::string& text, Accent accent = English);
    void speak(const std::string& text) { (void)speakText(text); }
    void startListening(std::function<void(const std::string&)> cb) { m_textRecognizedCallback = cb; }
    void stopListening() { m_textRecognizedCallback = nullptr; }
    void stopSpeaking();
    void setVolume(int percent);
    void setAccent(Accent accent);
    std::string accentToLocale(Accent accent) const;

    // Audio format configuration
    void setSampleRate(int rate);
    void setChannels(int channels);
    void setBitsPerSample(int bits);

    // Callback types
    using TextRecognizedCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using RecordingStateCallback = std::function<void(bool)>;

    void setTextRecognizedCallback(TextRecognizedCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setRecordingStateCallback(RecordingStateCallback callback);

private:
    void initializeAudio();
    void cleanupAudio();

    // Audio format
    int m_sampleRate = 44100;
    int m_channels = 2;
    int m_bitsPerSample = 16;
    int m_volume = 100;

    // State
    Accent m_currentAccent = English;
    std::atomic<bool> m_isRecording{false};
    std::atomic<bool> m_isPlaying{false};

    // Buffers
    std::vector<unsigned char> m_recordedData;
    std::vector<unsigned char> m_playbackData;

    // Callbacks
    TextRecognizedCallback m_textRecognizedCallback;
    ErrorCallback m_errorCallback;
    RecordingStateCallback m_recordingStateCallback;
};