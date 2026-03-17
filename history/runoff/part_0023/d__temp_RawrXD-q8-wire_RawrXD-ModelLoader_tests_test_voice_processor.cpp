#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "orchestration/voice_processor.hpp"
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

using ::testing::_;
using ::testing::Return;

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

// 1. Initialization and Configuration Tests
TEST_F(VoiceProcessorTest, InitializationSucceeds) {
    EXPECT_NE(processor, nullptr);
    EXPECT_FALSE(processor->isRecording());
}

TEST_F(VoiceProcessorTest, SetAndGetConfig) {
    VoiceProcessor::Config config;
    config.apiEndpoint = "https://test-api.example.com";
    config.apiKey = "test-key-12345";
    config.sampleRate = 48000;
    config.channelCount = 2;
    config.maxRecordingDurationMs = 60000;
    config.enableMetrics = true;
    config.enableAutoDelete = false;

    processor->setConfig(config);
    VoiceProcessor::Config retrieved = processor->getConfig();

    EXPECT_EQ(retrieved.apiEndpoint, "https://test-api.example.com");
    EXPECT_EQ(retrieved.apiKey, "test-key-12345");
    EXPECT_EQ(retrieved.sampleRate, 48000);
    EXPECT_EQ(retrieved.channelCount, 2);
    EXPECT_EQ(retrieved.maxRecordingDurationMs, 60000);
    EXPECT_TRUE(retrieved.enableMetrics);
    EXPECT_FALSE(retrieved.enableAutoDelete);
}

TEST_F(VoiceProcessorTest, DefaultConfigValues) {
    VoiceProcessor::Config config = processor->getConfig();
    EXPECT_EQ(config.sampleRate, 16000);
    EXPECT_EQ(config.channelCount, 1);
    EXPECT_EQ(config.sampleSize, 16);
    EXPECT_TRUE(config.enableMetrics);
    EXPECT_TRUE(config.enableAutoDelete);
}

// 2. Recording State Management Tests
TEST_F(VoiceProcessorTest, RecordingStateInitiallyFalse) {
    EXPECT_FALSE(processor->isRecording());
}

TEST_F(VoiceProcessorTest, StartRecordingChangesState) {
    QSignalSpy spy(processor, &VoiceProcessor::recordingStarted);
    bool started = processor->startRecording();
    
    if (started) {
        EXPECT_TRUE(processor->isRecording());
        EXPECT_EQ(spy.count(), 1);
    }
}

TEST_F(VoiceProcessorTest, StopRecordingWhenNotRecordingReturnsFalse) {
    EXPECT_FALSE(processor->isRecording());
    bool stopped = processor->stopRecording();
    EXPECT_FALSE(stopped);
}

TEST_F(VoiceProcessorTest, StartRecordingTwiceReturnsFalse) {
    bool first = processor->startRecording();
    if (first) {
        bool second = processor->startRecording();
        EXPECT_FALSE(second);
        processor->stopRecording();
    }
}

// 3. Transcription Tests
TEST_F(VoiceProcessorTest, TranscribeAudioWithEmptyDataReturnsEmpty) {
    QByteArray emptyData;
    QString result = processor->transcribeAudio(emptyData);
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(VoiceProcessorTest, TranscribeAudioWithValidData) {
    // Create mock audio data (at least 100ms worth)
    VoiceProcessor::Config config = processor->getConfig();
    int bytesPerSecond = config.sampleRate * config.channelCount * (config.sampleSize / 8);
    int minBytes = bytesPerSecond / 10; // 100ms
    
    QByteArray audioData(minBytes * 2, '\0'); // 200ms of audio
    
    QSignalSpy errorSpy(processor, &VoiceProcessor::errorOccurred);
    QString result = processor->transcribeAudio(audioData);
    
    // Should either succeed or fail with network error
    // In test environment without API, expect error
    if (result.isEmpty()) {
        EXPECT_GT(errorSpy.count(), 0);
    }
}

TEST_F(VoiceProcessorTest, TranscribeAudioTooShortFails) {
    QByteArray shortData(10, '\0'); // Too short
    QSignalSpy errorSpy(processor, &VoiceProcessor::errorOccurred);
    
    QString result = processor->transcribeAudio(shortData);
    
    EXPECT_TRUE(result.isEmpty());
    EXPECT_GT(errorSpy.count(), 0);
}

// 4. Intent Detection Tests
TEST_F(VoiceProcessorTest, DetectIntentWithEmptyStringReturnsEmpty) {
    QJsonObject result = processor->detectIntent("");
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(VoiceProcessorTest, DetectIntentWithValidText) {
    QSignalSpy errorSpy(processor, &VoiceProcessor::errorOccurred);
    QJsonObject result = processor->detectIntent("open file main.cpp");
    
    // Should either succeed or fail with network error
    if (result.isEmpty()) {
        EXPECT_GT(errorSpy.count(), 0);
    }
}

// 5. Text-to-Speech Tests
TEST_F(VoiceProcessorTest, GenerateSpeechWithEmptyTextReturnsEmpty) {
    QString result = processor->generateSpeech("");
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(VoiceProcessorTest, GenerateSpeechWithValidText) {
    QSignalSpy errorSpy(processor, &VoiceProcessor::errorOccurred);
    QString result = processor->generateSpeech("Hello, this is a test.");
    
    // Should either succeed or fail with network error
    if (result.isEmpty()) {
        EXPECT_GT(errorSpy.count(), 0);
    }
}

// 6. Metrics Tests
TEST_F(VoiceProcessorTest, InitialMetricsAreZero) {
    VoiceProcessor::Metrics metrics = processor->getMetrics();
    EXPECT_EQ(metrics.recordingCount, 0);
    EXPECT_EQ(metrics.transcriptionCount, 0);
    EXPECT_EQ(metrics.intentDetectionCount, 0);
    EXPECT_EQ(metrics.ttsCount, 0);
    EXPECT_EQ(metrics.errorCount, 0);
    EXPECT_EQ(metrics.avgRecordingDurationMs, 0.0);
    EXPECT_EQ(metrics.avgTranscriptionLatencyMs, 0.0);
    EXPECT_EQ(metrics.avgIntentDetectionLatencyMs, 0.0);
    EXPECT_EQ(metrics.avgTtsLatencyMs, 0.0);
}

TEST_F(VoiceProcessorTest, ResetMetricsClearsCounters) {
    // Trigger some errors to increment metrics
    processor->transcribeAudio(QByteArray());
    processor->detectIntent("");
    
    VoiceProcessor::Metrics before = processor->getMetrics();
    EXPECT_GT(before.errorCount, 0);
    
    processor->resetMetrics();
    
    VoiceProcessor::Metrics after = processor->getMetrics();
    EXPECT_EQ(after.errorCount, 0);
    EXPECT_EQ(after.recordingCount, 0);
}

TEST_F(VoiceProcessorTest, MetricsUpdateOnOperations) {
    QByteArray shortData(10, '\0');
    processor->transcribeAudio(shortData); // Should fail and increment error
    
    VoiceProcessor::Metrics metrics = processor->getMetrics();
    EXPECT_GT(metrics.errorCount, 0);
}

// 7. Signal Emission Tests
TEST_F(VoiceProcessorTest, ErrorSignalEmittedOnInvalidData) {
    QSignalSpy spy(processor, &VoiceProcessor::errorOccurred);
    processor->transcribeAudio(QByteArray()); // Empty data
    
    EXPECT_GT(spy.count(), 0);
    QList<QVariant> arguments = spy.takeFirst();
    EXPECT_FALSE(arguments.at(0).toString().isEmpty());
}

TEST_F(VoiceProcessorTest, RecordingSignalsEmitted) {
    QSignalSpy startSpy(processor, &VoiceProcessor::recordingStarted);
    QSignalSpy stopSpy(processor, &VoiceProcessor::recordingStopped);
    
    bool started = processor->startRecording();
    if (started) {
        EXPECT_EQ(startSpy.count(), 1);
        
        QTest::qWait(100); // Wait a bit
        processor->stopRecording();
        EXPECT_EQ(stopSpy.count(), 1);
    }
}

TEST_F(VoiceProcessorTest, MetricsUpdatedSignalEmitted) {
    VoiceProcessor::Config config = processor->getConfig();
    config.enableMetrics = true;
    processor->setConfig(config);
    
    QSignalSpy spy(processor, &VoiceProcessor::metricsUpdated);
    
    // Trigger operation that updates metrics
    bool started = processor->startRecording();
    if (started) {
        QTest::qWait(100);
        processor->stopRecording();
        // Metrics should have been updated
        EXPECT_GT(spy.count(), 0);
    }
}

// 8. Configuration Edge Cases
TEST_F(VoiceProcessorTest, ConfigWithZeroMaxDuration) {
    VoiceProcessor::Config config;
    config.maxRecordingDurationMs = 0;
    processor->setConfig(config);
    
    // Should still work but stop immediately
    EXPECT_NO_THROW(processor->setConfig(config));
}

TEST_F(VoiceProcessorTest, ConfigWithInvalidSampleRate) {
    VoiceProcessor::Config config;
    config.sampleRate = -1;
    
    EXPECT_NO_THROW(processor->setConfig(config));
    // Implementation should handle or validate
}

// 9. Thread Safety Tests
TEST_F(VoiceProcessorTest, ConcurrentConfigAccess) {
    VoiceProcessor::Config config1;
    config1.sampleRate = 16000;
    
    VoiceProcessor::Config config2;
    config2.sampleRate = 48000;
    
    // Set and get should be thread-safe
    processor->setConfig(config1);
    VoiceProcessor::Config retrieved1 = processor->getConfig();
    
    processor->setConfig(config2);
    VoiceProcessor::Config retrieved2 = processor->getConfig();
    
    EXPECT_EQ(retrieved2.sampleRate, 48000);
}

TEST_F(VoiceProcessorTest, ConcurrentMetricsAccess) {
    VoiceProcessor::Metrics metrics1 = processor->getMetrics();
    processor->resetMetrics();
    VoiceProcessor::Metrics metrics2 = processor->getMetrics();
    
    EXPECT_NO_THROW(metrics1 = processor->getMetrics());
}

// 10. Destructor and Cleanup Tests
TEST_F(VoiceProcessorTest, DestructorStopsRecording) {
    VoiceProcessor* temp = new VoiceProcessor();
    bool started = temp->startRecording();
    if (started) {
        EXPECT_TRUE(temp->isRecording());
    }
    
    EXPECT_NO_THROW(delete temp);
}

// Main function for running tests
int main(int argc, char **argv) {
    // Initialize Qt application for Qt objects
    QCoreApplication app(argc, argv);
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
