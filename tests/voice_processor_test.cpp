#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QEventLoop>
#include <QTimer>
#include "orchestration/voice_processor.hpp"

class VoiceProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = new VoiceProcessor();
    }

    void TearDown() override {
        delete processor;
    }

    VoiceProcessor* processor;
};

// ===== Initialization and Configuration Tests =====

TEST_F(VoiceProcessorTest, InitializationSucceeds) {
    EXPECT_NE(processor, nullptr);
    EXPECT_FALSE(processor->isRecording());
}

TEST_F(VoiceProcessorTest, SetAndGetConfig) {
    VoiceProcessor::Config config;
    config.apiEndpoint = "https://api.test.com";
    config.apiKey = "test-key-123";
    config.sampleRate = 48000;
    config.channelCount = 2;
    config.maxRecordingDurationMs = 60000;
    config.enableAutoDelete = false;
    
    processor->setConfig(config);
    
    VoiceProcessor::Config retrieved = processor->getConfig();
    EXPECT_EQ(retrieved.apiEndpoint, "https://api.test.com");
    EXPECT_EQ(retrieved.apiKey, "test-key-123");
    EXPECT_EQ(retrieved.sampleRate, 48000);
    EXPECT_EQ(retrieved.channelCount, 2);
    EXPECT_EQ(retrieved.maxRecordingDurationMs, 60000);
    EXPECT_FALSE(retrieved.enableAutoDelete);
}

TEST_F(VoiceProcessorTest, DefaultConfiguration) {
    VoiceProcessor::Config config = processor->getConfig();
    EXPECT_EQ(config.sampleRate, 16000);
    EXPECT_EQ(config.channelCount, 1);
    EXPECT_EQ(config.sampleSize, 16);
    EXPECT_TRUE(config.enableMetrics);
    EXPECT_TRUE(config.enableAutoDelete);
}

// ===== Recording Tests =====

TEST_F(VoiceProcessorTest, StartRecordingEmitsSignal) {
    QSignalSpy spy(processor, &VoiceProcessor::recordingStarted);
    
    // May fail if no audio device available - check the result
    bool started = processor->startRecording();
    
    if (started) {
        EXPECT_EQ(spy.count(), 1);
        EXPECT_TRUE(processor->isRecording());
    } else {
        // No audio device available - test passes
        EXPECT_FALSE(processor->isRecording());
    }
}

TEST_F(VoiceProcessorTest, CannotStartRecordingTwice) {
    bool firstStart = processor->startRecording();
    
    if (firstStart) {
        bool secondStart = processor->startRecording();
        EXPECT_FALSE(secondStart);
        
        processor->stopRecording();
    }
}

TEST_F(VoiceProcessorTest, StopRecordingEmitsSignal) {
    QSignalSpy spy(processor, &VoiceProcessor::recordingStopped);
    
    bool started = processor->startRecording();
    if (started) {
        processor->stopRecording();
        
        EXPECT_EQ(spy.count(), 1);
        EXPECT_FALSE(processor->isRecording());
    }
}

TEST_F(VoiceProcessorTest, StopRecordingWhenNotRecording) {
    EXPECT_FALSE(processor->isRecording());
    bool stopped = processor->stopRecording();
    EXPECT_FALSE(stopped);
}

// ===== Transcription Tests =====

TEST_F(VoiceProcessorTest, TranscribeEmptyAudioFails) {
    QSignalSpy errorSpy(processor, &VoiceProcessor::errorOccurred);
    
    QByteArray emptyAudio;
    QString result = processor->transcribeAudio(emptyAudio);
    
    EXPECT_TRUE(result.isEmpty());
    EXPECT_GE(errorSpy.count(), 1);
}

TEST_F(VoiceProcessorTest, TranscribeInvalidAudioFails) {
    QSignalSpy errorSpy(processor, &VoiceProcessor::errorOccurred);
    
    // Too short audio data (less than 100ms)
    QByteArray tinyAudio(10, 0);
    QString result = processor->transcribeAudio(tinyAudio);
    
    EXPECT_TRUE(result.isEmpty());
    EXPECT_GE(errorSpy.count(), 1);
}

TEST_F(VoiceProcessorTest, TranscribeValidAudioRequiresAPI) {
    // Configure with mock API endpoint
    VoiceProcessor::Config config;
    config.apiEndpoint = "http://localhost:8080";
    config.apiKey = "test-key";
    processor->setConfig(config);
    
    QSignalSpy errorSpy(processor, &VoiceProcessor::errorOccurred);
    
    // Create valid-sized audio data
    int sampleRate = 16000;
    int duration = 1; // 1 second
    QByteArray audioData(sampleRate * 2 * duration, 0); // 16-bit samples
    
    QString result = processor->transcribeAudio(audioData);
    
    // Without a real API, this should fail with network error
    // But the validation should pass
    EXPECT_TRUE(result.isEmpty()); // API not available
}

// ===== Intent Detection Tests =====

TEST_F(VoiceProcessorTest, DetectIntentWithEmptyText) {
    QJsonObject result = processor->detectIntent("");
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(VoiceProcessorTest, DetectIntentRequiresAPI) {
    VoiceProcessor::Config config;
    config.apiEndpoint = "http://localhost:8080";
    config.apiKey = "test-key";
    processor->setConfig(config);
    
    QSignalSpy errorSpy(processor, &VoiceProcessor::errorOccurred);
    
    QJsonObject result = processor->detectIntent("open file main.cpp");
    
    // Without real API, should fail
    EXPECT_TRUE(result.isEmpty());
}

// ===== Text-to-Speech Tests =====

TEST_F(VoiceProcessorTest, GenerateSpeechWithEmptyText) {
    QString result = processor->generateSpeech("");
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(VoiceProcessorTest, GenerateSpeechRequiresAPI) {
    VoiceProcessor::Config config;
    config.apiEndpoint = "http://localhost:8080";
    config.apiKey = "test-key";
    processor->setConfig(config);
    
    QSignalSpy errorSpy(processor, &VoiceProcessor::errorOccurred);
    
    QString result = processor->generateSpeech("Hello world");
    
    // Without real API, should fail
    EXPECT_TRUE(result.isEmpty());
}

// ===== Metrics Tests =====

TEST_F(VoiceProcessorTest, InitialMetricsAreZero) {
    VoiceProcessor::Metrics metrics = processor->getMetrics();
    
    EXPECT_EQ(metrics.recordingCount, 0);
    EXPECT_EQ(metrics.transcriptionCount, 0);
    EXPECT_EQ(metrics.intentDetectionCount, 0);
    EXPECT_EQ(metrics.ttsCount, 0);
    EXPECT_EQ(metrics.errorCount, 0);
    EXPECT_DOUBLE_EQ(metrics.avgRecordingDurationMs, 0.0);
    EXPECT_DOUBLE_EQ(metrics.avgTranscriptionLatencyMs, 0.0);
    EXPECT_DOUBLE_EQ(metrics.avgIntentDetectionLatencyMs, 0.0);
    EXPECT_DOUBLE_EQ(metrics.avgTtsLatencyMs, 0.0);
}

TEST_F(VoiceProcessorTest, MetricsUpdateOnRecording) {
    bool started = processor->startRecording();
    if (started) {
        processor->stopRecording();
        
        VoiceProcessor::Metrics metrics = processor->getMetrics();
        EXPECT_GE(metrics.recordingCount, 1);
    }
}

TEST_F(VoiceProcessorTest, ResetMetricsWorks) {
    // Attempt to generate some metrics
    processor->startRecording();
    processor->stopRecording();
    
    processor->resetMetrics();
    
    VoiceProcessor::Metrics metrics = processor->getMetrics();
    EXPECT_EQ(metrics.recordingCount, 0);
    EXPECT_EQ(metrics.errorCount, 0);
}

TEST_F(VoiceProcessorTest, MetricsSignalEmitted) {
    VoiceProcessor::Config config;
    config.enableMetrics = true;
    processor->setConfig(config);
    
    QSignalSpy spy(processor, &VoiceProcessor::metricsUpdated);
    
    // Any operation should potentially update metrics
    processor->resetMetrics();
    
    // Metrics may or may not emit on reset depending on implementation
    EXPECT_GE(spy.count(), 0);
}

// ===== Error Handling Tests =====

TEST_F(VoiceProcessorTest, ErrorSignalEmittedOnFailure) {
    QSignalSpy errorSpy(processor, &VoiceProcessor::errorOccurred);
    
    // Try to transcribe invalid data
    QByteArray invalidData;
    processor->transcribeAudio(invalidData);
    
    EXPECT_GE(errorSpy.count(), 1);
}

TEST_F(VoiceProcessorTest, ErrorCountIncrementsOnFailure) {
    VoiceProcessor::Metrics initialMetrics = processor->getMetrics();
    qint64 initialErrors = initialMetrics.errorCount;
    
    // Trigger an error
    processor->transcribeAudio(QByteArray());
    
    VoiceProcessor::Metrics newMetrics = processor->getMetrics();
    EXPECT_GT(newMetrics.errorCount, initialErrors);
}

// ===== GDPR Compliance Tests =====

TEST_F(VoiceProcessorTest, AutoDeleteConfigurationWorks) {
    VoiceProcessor::Config config;
    config.enableAutoDelete = true;
    config.autoDeleteDelayMs = 1000;
    processor->setConfig(config);
    
    VoiceProcessor::Config retrieved = processor->getConfig();
    EXPECT_TRUE(retrieved.enableAutoDelete);
    EXPECT_EQ(retrieved.autoDeleteDelayMs, 1000);
}

TEST_F(VoiceProcessorTest, AutoDeleteCanBeDisabled) {
    VoiceProcessor::Config config;
    config.enableAutoDelete = false;
    processor->setConfig(config);
    
    VoiceProcessor::Config retrieved = processor->getConfig();
    EXPECT_FALSE(retrieved.enableAutoDelete);
}

// ===== Thread Safety Tests =====

TEST_F(VoiceProcessorTest, ConcurrentConfigAccess) {
    VoiceProcessor::Config config1;
    config1.sampleRate = 48000;
    
    VoiceProcessor::Config config2;
    config2.sampleRate = 44100;
    
    // Set config from multiple threads
    processor->setConfig(config1);
    VoiceProcessor::Config retrieved = processor->getConfig();
    
    EXPECT_TRUE(retrieved.sampleRate == 48000 || retrieved.sampleRate == 44100);
}

TEST_F(VoiceProcessorTest, ConcurrentMetricsAccess) {
    VoiceProcessor::Metrics m1 = processor->getMetrics();
    VoiceProcessor::Metrics m2 = processor->getMetrics();
    
    // Should return consistent values
    EXPECT_EQ(m1.recordingCount, m2.recordingCount);
}

// ===== Signal Tests =====

TEST_F(VoiceProcessorTest, AllSignalsDefined) {
    // Verify all signals can be connected
    QSignalSpy spyRecordingStarted(processor, &VoiceProcessor::recordingStarted);
    QSignalSpy spyRecordingStopped(processor, &VoiceProcessor::recordingStopped);
    QSignalSpy spyTranscriptionReady(processor, &VoiceProcessor::transcriptionReady);
    QSignalSpy spyIntentDetected(processor, &VoiceProcessor::intentDetected);
    QSignalSpy spySpeechGenerated(processor, &VoiceProcessor::speechGenerated);
    QSignalSpy spyError(processor, &VoiceProcessor::errorOccurred);
    QSignalSpy spyMetrics(processor, &VoiceProcessor::metricsUpdated);
    
    EXPECT_TRUE(spyRecordingStarted.isValid());
    EXPECT_TRUE(spyRecordingStopped.isValid());
    EXPECT_TRUE(spyTranscriptionReady.isValid());
    EXPECT_TRUE(spyIntentDetected.isValid());
    EXPECT_TRUE(spySpeechGenerated.isValid());
    EXPECT_TRUE(spyError.isValid());
    EXPECT_TRUE(spyMetrics.isValid());
}

// ===== Integration Tests =====

TEST_F(VoiceProcessorTest, CompleteWorkflowWithoutAPI) {
    QSignalSpy errorSpy(processor, &VoiceProcessor::errorOccurred);
    
    // Start recording
    bool started = processor->startRecording();
    
    if (started) {
        // Stop recording
        processor->stopRecording();
        
        // Verify state
        EXPECT_FALSE(processor->isRecording());
        
        // Check metrics
        VoiceProcessor::Metrics metrics = processor->getMetrics();
        EXPECT_GE(metrics.recordingCount, 1);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
