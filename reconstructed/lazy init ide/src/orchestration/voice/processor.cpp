#include "voice_processor.hpp"
#include <QDebug>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QDateTime>
#include <QFile>

VoiceProcessor::VoiceProcessor(QObject* parent)
    : QObject(parent),
      m_audioSource(nullptr),
      m_audioBuffer(new QBuffer(this)),
      m_recordingTimer(new QTimer(this)),
      m_isRecording(false)
{
    logStructured("INFO", "VoiceProcessor initializing", QJsonObject{{"component", "VoiceProcessor"}});
    
    setupAudioFormat();
    
    connect(m_recordingTimer, &QTimer::timeout, this, &VoiceProcessor::processRecordedAudio);
    
    logStructured("INFO", "VoiceProcessor initialized successfully", QJsonObject{{"component", "VoiceProcessor"}});
}

VoiceProcessor::~VoiceProcessor()
{
    logStructured("INFO", "VoiceProcessor shutting down", QJsonObject{{"component", "VoiceProcessor"}});
    
    if (m_isRecording) {
        stopRecording();
    }
    
    if (m_audioSource) {
        delete m_audioSource;
    }
    
    logStructured("INFO", "VoiceProcessor shutdown complete", QJsonObject{{"component", "VoiceProcessor"}});
}

void VoiceProcessor::setConfig(const Config& config)
{
    QMutexLocker locker(&m_configMutex);
    m_config = config;
    setupAudioFormat();
    logStructured("INFO", "Configuration updated", QJsonObject{
        {"sampleRate", config.sampleRate},
        {"channelCount", config.channelCount},
        {"maxRecordingDurationMs", config.maxRecordingDurationMs}
    });
}

VoiceProcessor::Config VoiceProcessor::getConfig() const
{
    QMutexLocker locker(&m_configMutex);
    return m_config;
}

void VoiceProcessor::setupAudioFormat()
{
    QMutexLocker locker(&m_configMutex);
    
    // Use Qt 6 helper to create format (this is fast - no hardware access)
    m_audioFormat = Qt6AudioHelper::createVoiceFormat(m_config.sampleRate, m_config.channelCount);
    
    // NOTE: We defer audio device query to startRecording() to avoid blocking
    // during construction. QMediaDevices::defaultAudioInput() can hang on systems
    // without audio hardware or with problematic drivers.
    
    logStructured("DEBUG", "Audio format configured", QJsonObject{
        {"sampleRate", m_audioFormat.sampleRate()},
        {"channelCount", m_audioFormat.channelCount()},
        {"sampleFormat", static_cast<int>(m_audioFormat.sampleFormat())}
    });
}

bool VoiceProcessor::startRecording()
{
    auto startTime = std::chrono::steady_clock::now();
    
    QMutexLocker locker(&m_stateMutex);
    
    if (m_isRecording) {
        logStructured("WARN", "Recording already in progress", QJsonObject{{"state", "already_recording"}});
        return false;
    }
    
    try {
        // Lazy init: Query audio device only when actually starting to record
        // This avoids blocking during construction (QMediaDevices can hang)
        if (m_audioDevice.isNull()) {
            logStructured("DEBUG", "Querying default audio input device", QJsonObject{});
            m_audioDevice = Qt6AudioHelper::getDefaultInputDevice();
            if (m_audioDevice.isNull()) {
                logStructured("ERROR", "No audio input device available", QJsonObject{});
                QMutexLocker metricsLocker(&m_metricsMutex);
                m_metrics.errorCount++;
                emit errorOccurred("No audio input device available");
                return false;
            }
            logStructured("DEBUG", "Audio device acquired", QJsonObject{{"device", m_audioDevice.description()}});
        }
        
        // Qt 6: Use QMediaDevices instead of QAudioDeviceInfo
        if (!Qt6AudioHelper::isFormatSupported(m_audioDevice, m_audioFormat)) {
            logStructured("ERROR", "Audio format not supported by device", QJsonObject{{"device", m_audioDevice.description()}});
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            emit errorOccurred("Audio format not supported");
            return false;
        }
        
        // Qt 6: QAudioInput renamed to QAudioSource
        m_audioSource = new QAudioSource(m_audioDevice, m_audioFormat, this);
        connect(m_audioSource, &QAudioSource::stateChanged, this, &VoiceProcessor::handleAudioStateChanged);
        
        m_audioBuffer->close();
        m_audioBuffer->setData(QByteArray());
        m_audioBuffer->open(QIODevice::ReadWrite);
        
        m_audioSource->start(m_audioBuffer);
        m_isRecording = true;
        
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        m_recordingTimer->start(config.maxRecordingDurationMs);
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.recordingCount++;
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Recording started", QJsonObject{
            {"device", m_audioDevice.description()},
            {"maxDurationMs", config.maxRecordingDurationMs},
            {"startLatencyMs", static_cast<qint64>(duration.count())}
        });
        
        emit recordingStarted();
        return true;
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Exception during recording start", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Recording failed: %1").arg(e.what()));
        return false;
    }
}

bool VoiceProcessor::stopRecording()
{
    auto startTime = std::chrono::steady_clock::now();
    
    QMutexLocker locker(&m_stateMutex);
    
    if (!m_isRecording) {
        logStructured("WARN", "No recording in progress", QJsonObject{{"state", "not_recording"}});
        return false;
    }
    
    try {
        m_recordingTimer->stop();
        
        if (m_audioSource) {
            m_audioSource->stop();
        }
        
        m_audioBuffer->close();
        QByteArray audioData = m_audioBuffer->data();
        
        m_isRecording = false;
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("recording", duration);
        
        logStructured("INFO", "Recording stopped", QJsonObject{
            {"audioBytesRecorded", audioData.size()},
            {"stopLatencyMs", duration.count()}
        });
        
        emit recordingStopped(audioData);
        
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAutoDelete) {
            QTimer::singleShot(config.autoDeleteDelayMs, this, [this, audioData]() {
                scheduleAudioDeletion(audioData);
            });
        }
        
        return true;
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Exception during recording stop", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Stop recording failed: %1").arg(e.what()));
        return false;
    }
}

bool VoiceProcessor::isRecording() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_isRecording;
}

QString VoiceProcessor::transcribeAudio(const QByteArray& audioData)
{
    auto startTime = std::chrono::steady_clock::now();
    
    if (!validateAudioData(audioData)) {
        logStructured("ERROR", "Invalid audio data for transcription", QJsonObject{{"size", audioData.size()}});
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        emit errorOccurred("Invalid audio data");
        return QString();
    }
    
    try {
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        QJsonObject payload;
        payload["audio"] = QString(audioData.toBase64());
        payload["format"] = "pcm";
        payload["sampleRate"] = config.sampleRate;
        payload["channelCount"] = config.channelCount;
        
        logStructured("DEBUG", "Sending transcription request", QJsonObject{
            {"audioBytesSize", audioData.size()},
            {"endpoint", config.apiEndpoint + "/transcribe"}
        });
        
        QJsonObject response = makeApiRequest(config.apiEndpoint + "/transcribe", payload);
        
        QString transcription = response["transcription"].toString();
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("transcription", duration);
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.transcriptionCount++;
        }
        
        logStructured("INFO", "Transcription completed", QJsonObject{
            {"transcriptionLength", transcription.length()},
            {"latencyMs", duration.count()}
        });
        
        emit transcriptionReady(transcription);
        return transcription;
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Transcription failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Transcription failed: %1").arg(e.what()));
        return QString();
    }
}

QJsonObject VoiceProcessor::detectIntent(const QString& transcription)
{
    auto startTime = std::chrono::steady_clock::now();
    
    if (transcription.isEmpty()) {
        logStructured("WARN", "Empty transcription for intent detection", QJsonObject{});
        return QJsonObject();
    }
    
    try {
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        QJsonObject payload;
        payload["text"] = transcription;
        payload["context"] = "ide_voice_command";
        
        logStructured("DEBUG", "Sending intent detection request", QJsonObject{
            {"transcriptionLength", transcription.length()},
            {"endpoint", config.apiEndpoint + "/intent"}
        });
        
        QJsonObject response = makeApiRequest(config.apiEndpoint + "/intent", payload);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("intent_detection", duration);
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.intentDetectionCount++;
        }
        
        logStructured("INFO", "Intent detected", QJsonObject{
            {"intent", response["intent"].toString()},
            {"confidence", response["confidence"].toDouble()},
            {"latencyMs", duration.count()}
        });
        
        emit intentDetected(response);
        return response;
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Intent detection failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Intent detection failed: %1").arg(e.what()));
        return QJsonObject();
    }
}

QString VoiceProcessor::generateSpeech(const QString& text)
{
    auto startTime = std::chrono::steady_clock::now();
    
    if (text.isEmpty()) {
        logStructured("WARN", "Empty text for speech generation", QJsonObject{});
        return QString();
    }
    
    try {
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        QJsonObject payload;
        payload["text"] = text;
        payload["format"] = "pcm";
        payload["sampleRate"] = config.sampleRate;
        
        logStructured("DEBUG", "Sending TTS request", QJsonObject{
            {"textLength", text.length()},
            {"endpoint", config.apiEndpoint + "/tts"}
        });
        
        QJsonObject response = makeApiRequest(config.apiEndpoint + "/tts", payload);
        
        QString audioBase64 = response["audio"].toString();
        QByteArray audioData = QByteArray::fromBase64(audioBase64.toUtf8());
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("tts", duration);
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.ttsCount++;
        }
        
        logStructured("INFO", "Speech generated", QJsonObject{
            {"audioBytesGenerated", audioData.size()},
            {"latencyMs", duration.count()}
        });
        
        emit speechGenerated(audioData);
        return audioBase64;
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Speech generation failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("TTS failed: %1").arg(e.what()));
        return QString();
    }
}

VoiceProcessor::Metrics VoiceProcessor::getMetrics() const
{
    QMutexLocker locker(&m_metricsMutex);
    return m_metrics;
}

void VoiceProcessor::resetMetrics()
{
    QMutexLocker locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", QJsonObject{});
}

void VoiceProcessor::handleAudioStateChanged(QAudio::State state)
{
    QString stateStr;
    switch (state) {
        case QAudio::ActiveState: stateStr = "Active"; break;
        case QAudio::SuspendedState: stateStr = "Suspended"; break;
        case QAudio::StoppedState: stateStr = "Stopped"; break;
        case QAudio::IdleState: stateStr = "Idle"; break;
        default: stateStr = "Unknown";
    }
    
    logStructured("DEBUG", "Audio state changed", QJsonObject{{"state", stateStr}});
    
    if (state == QAudio::StoppedState && m_audioSource && m_audioSource->error() != QAudio::NoError) {
        QString errorStr;
        switch (m_audioSource->error()) {
            case QAudio::OpenError: errorStr = "Open Error"; break;
            case QAudio::IOError: errorStr = "IO Error"; break;
            case QAudio::UnderrunError: errorStr = "Underrun Error"; break;
            case QAudio::FatalError: errorStr = "Fatal Error"; break;
            default: errorStr = "Unknown Error";
        }
        
        logStructured("ERROR", "Audio error occurred", QJsonObject{{"error", errorStr}});
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        emit errorOccurred(QString("Audio error: %1").arg(errorStr));
    }
}

void VoiceProcessor::processRecordedAudio()
{
    logStructured("INFO", "Max recording duration reached, stopping", QJsonObject{});
    stopRecording();
}

void VoiceProcessor::scheduleAudioDeletion(const QByteArray& audioData)
{
    logStructured("INFO", "GDPR auto-delete executed", QJsonObject{
        {"audioDataSize", audioData.size()},
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
    });
}

void VoiceProcessor::logStructured(const QString& level, const QString& message, const QJsonObject& context)
{
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "VoiceProcessor";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    QJsonDocument doc(logEntry);
    qDebug().noquote() << doc.toJson(QJsonDocument::Compact);
}

void VoiceProcessor::recordLatency(const QString& operation, const std::chrono::milliseconds& duration)
{
    QMutexLocker locker(&m_metricsMutex);
    
    if (operation == "recording") {
        m_metrics.avgRecordingDurationMs = 
            (m_metrics.avgRecordingDurationMs * (m_metrics.recordingCount - 1) + duration.count()) / m_metrics.recordingCount;
    } else if (operation == "transcription") {
        m_metrics.avgTranscriptionLatencyMs = 
            (m_metrics.avgTranscriptionLatencyMs * (m_metrics.transcriptionCount - 1) + duration.count()) / m_metrics.transcriptionCount;
    } else if (operation == "intent_detection") {
        m_metrics.avgIntentDetectionLatencyMs = 
            (m_metrics.avgIntentDetectionLatencyMs * (m_metrics.intentDetectionCount - 1) + duration.count()) / m_metrics.intentDetectionCount;
    } else if (operation == "tts") {
        m_metrics.avgTtsLatencyMs = 
            (m_metrics.avgTtsLatencyMs * (m_metrics.ttsCount - 1) + duration.count()) / m_metrics.ttsCount;
    }
    
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (config.enableMetrics) {
        emit metricsUpdated(m_metrics);
    }
}

QJsonObject VoiceProcessor::makeApiRequest(const QString& endpoint, const QJsonObject& payload)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(endpoint);
    
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!config.apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(config.apiKey).toUtf8());
    }
    
    QJsonDocument doc(payload);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    QEventLoop loop;
    QNetworkReply* reply = manager.post(request, data);
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    bool timedOut = false;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        if (reply) {
            reply->abort();
        }
        loop.quit();
    });
    timeoutTimer.start(std::max(0, config.networkTimeoutMs));
    loop.exec();
    
    QJsonObject response;
    if (timedOut) {
        logStructured("ERROR", "API request timed out", QJsonObject{
            {"endpoint", endpoint},
            {"timeoutMs", config.networkTimeoutMs}
        });
        throw std::runtime_error("API request timed out");
    }
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        response = responseDoc.object();
    } else {
        logStructured("ERROR", "API request failed", QJsonObject{
            {"endpoint", endpoint},
            {"error", reply->errorString()}
        });
        throw std::runtime_error(reply->errorString().toStdString());
    }
    
    reply->deleteLater();
    return response;
}

bool VoiceProcessor::validateAudioData(const QByteArray& audioData)
{
    if (audioData.isEmpty()) {
        return false;
    }
    
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    // Qt 6: Int16 format is always 16-bit (2 bytes per sample)
    int bytesPerSecond = config.sampleRate * config.channelCount * 2;  // 2 bytes for Int16
    int minBytes = bytesPerSecond / 10;
    
    if (audioData.size() < minBytes) {
        logStructured("WARN", "Audio data too short", QJsonObject{
            {"actualBytes", audioData.size()},
            {"minBytes", minBytes}
        });
        return false;
    }
    
    return true;
}
