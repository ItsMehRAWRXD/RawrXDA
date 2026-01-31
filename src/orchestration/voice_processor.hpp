#pragma once


#include <chrono>
#include <memory>
#include "qt6_audio_helper.hpp"

/**
 * @class VoiceProcessor
 * @brief Production-ready speech-to-text, intent recognition, and text-to-speech processor
 * 
 * Features:
 * - Audio input capture with configurable format
 * - Speech-to-text transcription with AI integration
 * - Intent detection and command parsing
 * - Plan generation from voice input
 * - Text-to-speech feedback
 * - Structured logging with performance metrics
 * - Error handling and recovery
 * - GDPR-compliant audio data handling
 */
class VoiceProcessor : public void {

public:
    explicit VoiceProcessor(void* parent = nullptr);
    ~VoiceProcessor() override;

    // Configuration
    struct Config {
        std::string apiEndpoint;
        std::string apiKey;
        int sampleRate = 16000;
        int channelCount = 1;
        // Note: Qt 6 uses setSampleFormat() instead of sampleSize/byteOrder/sampleType
        // Default is QAudioFormat::Int16 (16-bit signed little-endian)
        int maxRecordingDurationMs = 30000;
        bool enableMetrics = true;
        bool enableAutoDelete = true;
        int autoDeleteDelayMs = 60000;
    };

    void setConfig(const Config& config);
    Config getConfig() const;

    // Core functionality
    bool startRecording();
    bool stopRecording();
    bool isRecording() const;
    
    std::string transcribeAudio(const std::vector<uint8_t>& audioData);
    void* detectIntent(const std::string& transcription);
    std::string generateSpeech(const std::string& text);
    
    // Metrics
    struct Metrics {
        qint64 recordingCount = 0;
        qint64 transcriptionCount = 0;
        qint64 intentDetectionCount = 0;
        qint64 ttsCount = 0;
        qint64 errorCount = 0;
        double avgRecordingDurationMs = 0.0;
        double avgTranscriptionLatencyMs = 0.0;
        double avgIntentDetectionLatencyMs = 0.0;
        double avgTtsLatencyMs = 0.0;
    };
    
    Metrics getMetrics() const;
    void resetMetrics();

    void recordingStarted();
    void recordingStopped(const std::vector<uint8_t>& audioData);
    void transcriptionReady(const std::string& text);
    void intentDetected(const void*& intent);
    void speechGenerated(const std::vector<uint8_t>& audioData);
    void errorOccurred(const std::string& error);
    void metricsUpdated(const Metrics& metrics);

private:
    void handleAudioStateChanged(QAudio::State state);
    void processRecordedAudio();
    void scheduleAudioDeletion(const std::vector<uint8_t>& audioData);

private:
    // Audio capture
    QAudioSource* m_audioSource;  // Qt 6: QAudioInput renamed to QAudioSource
    QAudioDevice m_audioDevice;
    QBuffer* m_audioBuffer;
    QAudioFormat m_audioFormat;
    void** m_recordingTimer;
    
    // Configuration
    Config m_config;
    mutable std::mutex m_configMutex;
    
    // State
    bool m_isRecording;
    mutable std::mutex m_stateMutex;
    
    // Metrics
    Metrics m_metrics;
    mutable std::mutex m_metricsMutex;
    
    // Helper methods
    void setupAudioFormat();
    void logStructured(const std::string& level, const std::string& message, const void*& context = void*());
    void updateMetric(const std::string& metricName, qint64 value);
    void recordLatency(const std::string& operation, const std::chrono::milliseconds& duration);
    void* makeApiRequest(const std::string& endpoint, const void*& payload);
    bool validateAudioData(const std::vector<uint8_t>& audioData);
};

