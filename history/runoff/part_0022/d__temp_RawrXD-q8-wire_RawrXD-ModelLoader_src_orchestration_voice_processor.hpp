#pragma once

#include <QString>
#include <QJsonObject>
#include <QObject>
#include <QBuffer>
#include <QTimer>
#include <QMutex>
#include <QVector>
#include <QtMultimedia/qaudio.h>
#include <QtMultimedia/qaudioinput.h>
#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qaudiodevice.h>
#include <QtMultimedia/qmediadevices.h>
#include <QtMultimedia/qaudiosource.h>
#include <chrono>
#include <memory>

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
class VoiceProcessor : public QObject {
    Q_OBJECT

public:
    explicit VoiceProcessor(QObject* parent = nullptr);
    ~VoiceProcessor() override;

    // Configuration
    struct Config {
        QString apiEndpoint;
        QString apiKey;
        int sampleRate = 16000;
        int channelCount = 1;
        QAudioFormat::SampleFormat sampleFormat = QAudioFormat::Int16;
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
    
    QString transcribeAudio(const QByteArray& audioData);
    QJsonObject detectIntent(const QString& transcription);
    QString generateSpeech(const QString& text);
    
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

signals:
    void recordingStarted();
    void recordingStopped(const QByteArray& audioData);
    void transcriptionReady(const QString& text);
    void intentDetected(const QJsonObject& intent);
    void speechGenerated(const QByteArray& audioData);
    void errorOccurred(const QString& error);
    void metricsUpdated(const Metrics& metrics);

private slots:
    void handleAudioStateChanged(QAudio::State state);
    void processRecordedAudio();
    void scheduleAudioDeletion(const QByteArray& audioData);

private:
    // Audio capture
    QAudioSource* m_audioSource;
    QBuffer* m_audioBuffer;
    QAudioFormat m_audioFormat;
    QTimer* m_recordingTimer;
    
    // Configuration
    Config m_config;
    mutable QMutex m_configMutex;
    
    // State
    bool m_isRecording;
    mutable QMutex m_stateMutex;
    
    // Metrics
    Metrics m_metrics;
    mutable QMutex m_metricsMutex;
    
    // Helper methods
    void setupAudioFormat();
    void logStructured(const QString& level, const QString& message, const QJsonObject& context = QJsonObject());
    void updateMetric(const QString& metricName, qint64 value);
    void recordLatency(const QString& operation, const std::chrono::milliseconds& duration);
    QJsonObject makeApiRequest(const QString& endpoint, const QJsonObject& payload);
    bool validateAudioData(const QByteArray& audioData);
};
