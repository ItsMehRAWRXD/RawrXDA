#include "voice_processor.hpp"


VoiceProcessor::VoiceProcessor(void* parent)
    : void(parent),
      m_audioSource(nullptr),
      m_audioBuffer(nullptr),
      m_recordingTimer(new void*(this)),
      m_isRecording(false)
{
    logStructured("INFO", "VoiceProcessor initializing", void*{{"component", "VoiceProcessor"}});
    
    setupAudioFormat();
// Qt connect removed
    logStructured("INFO", "VoiceProcessor initialized successfully", void*{{"component", "VoiceProcessor"}});
    return true;
}

VoiceProcessor::~VoiceProcessor()
{
    logStructured("INFO", "VoiceProcessor shutting down", void*{{"component", "VoiceProcessor"}});
    
    if (m_isRecording) {
        stopRecording();
    return true;
}

    if (m_audioSource) {
        delete m_audioSource;
    return true;
}

    logStructured("INFO", "VoiceProcessor shutdown complete", void*{{"component", "VoiceProcessor"}});
    return true;
}

void VoiceProcessor::setConfig(const Config& config)
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    m_config = config;
    setupAudioFormat();
    logStructured("INFO", "Configuration updated", void*{
        {"sampleRate", config.sampleRate},
        {"channelCount", config.channelCount},
        {"maxRecordingDurationMs", config.maxRecordingDurationMs}
    });
    return true;
}

VoiceProcessor::Config VoiceProcessor::getConfig() const
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    return m_config;
    return true;
}

void VoiceProcessor::setupAudioFormat()
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    
    // Use Qt 6 helper to create format
    m_audioFormat = Qt6AudioHelper::createVoiceFormat(m_config.sampleRate, m_config.channelCount);
    
    // Get default audio input device
    m_audioDevice = Qt6AudioHelper::getDefaultInputDevice();
    
    logStructured("DEBUG", "Audio format configured", void*{
        {"sampleRate", m_audioFormat.sampleRate()},
        {"channelCount", m_audioFormat.channelCount()},
        {"sampleFormat", static_cast<int>(m_audioFormat.sampleFormat())}
    });
    return true;
}

bool VoiceProcessor::startRecording()
{
    auto startTime = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> locker(&m_stateMutex);
    
    if (m_isRecording) {
        logStructured("WARN", "Recording already in progress", void*{{"state", "already_recording"}});
        return false;
    return true;
}

    try {
        // Qt 6: Use QMediaDevices instead of QAudioDeviceInfo
        if (!Qt6AudioHelper::isFormatSupported(m_audioDevice, m_audioFormat)) {
            logStructured("ERROR", "Audio format not supported by device", void*{{"device", m_audioDevice.description()}});
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            errorOccurred("Audio format not supported");
            return false;
    return true;
}

        // Qt 6: QAudioInput renamed to QAudioSource
        m_audioSource = nullptr;
// Qt connect removed
        m_audioBuffer->close();
        m_audioBuffer->setData(std::vector<uint8_t>());
        m_audioBuffer->open(QIODevice::ReadWrite);
        
        m_audioSource->start(m_audioBuffer);
        m_isRecording = true;
        
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
    return true;
}

        m_recordingTimer->start(config.maxRecordingDurationMs);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.recordingCount++;
    return true;
}

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Recording started", void*{
            {"device", m_audioDevice.description()},
            {"maxDurationMs", config.maxRecordingDurationMs},
            {"startLatencyMs", static_cast<int64_t>(duration.count())}
        });
        
        recordingStarted();
        return true;
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Exception during recording start", void*{{"error", e.what()}});
        errorOccurred(std::string("Recording failed: %1")));
        return false;
    return true;
}

    return true;
}

bool VoiceProcessor::stopRecording()
{
    auto startTime = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> locker(&m_stateMutex);
    
    if (!m_isRecording) {
        logStructured("WARN", "No recording in progress", void*{{"state", "not_recording"}});
        return false;
    return true;
}

    try {
        m_recordingTimer->stop();
        
        if (m_audioSource) {
            m_audioSource->stop();
    return true;
}

        m_audioBuffer->close();
        std::vector<uint8_t> audioData = m_audioBuffer->data();
        
        m_isRecording = false;
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("recording", duration);
        
        logStructured("INFO", "Recording stopped", void*{
            {"audioBytesRecorded", audioData.size()},
            {"stopLatencyMs", duration.count()}
        });
        
        recordingStopped(audioData);
        
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
    return true;
}

        if (config.enableAutoDelete) {
            void*::singleShot(config.autoDeleteDelayMs, this, [this, audioData]() {
                scheduleAudioDeletion(audioData);
            });
    return true;
}

        return true;
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Exception during recording stop", void*{{"error", e.what()}});
        errorOccurred(std::string("Stop recording failed: %1")));
        return false;
    return true;
}

    return true;
}

bool VoiceProcessor::isRecording() const
{
    std::lock_guard<std::mutex> locker(&m_stateMutex);
    return m_isRecording;
    return true;
}

std::string VoiceProcessor::transcribeAudio(const std::vector<uint8_t>& audioData)
{
    auto startTime = std::chrono::steady_clock::now();
    
    if (!validateAudioData(audioData)) {
        logStructured("ERROR", "Invalid audio data for transcription", void*{{"size", audioData.size()}});
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        errorOccurred("Invalid audio data");
        return std::string();
    return true;
}

    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
    return true;
}

        void* payload;
        payload["audio"] = std::string(audioData.toBase64());
        payload["format"] = "pcm";
        payload["sampleRate"] = config.sampleRate;
        payload["channelCount"] = config.channelCount;
        
        logStructured("DEBUG", "Sending transcription request", void*{
            {"audioBytesSize", audioData.size()},
            {"endpoint", config.apiEndpoint + "/transcribe"}
        });
        
        void* response = makeApiRequest(config.apiEndpoint + "/transcribe", payload);
        
        std::string transcription = response["transcription"].toString();
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("transcription", duration);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.transcriptionCount++;
    return true;
}

        logStructured("INFO", "Transcription completed", void*{
            {"transcriptionLength", transcription.length()},
            {"latencyMs", duration.count()}
        });
        
        transcriptionReady(transcription);
        return transcription;
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Transcription failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Transcription failed: %1")));
        return std::string();
    return true;
}

    return true;
}

void* VoiceProcessor::detectIntent(const std::string& transcription)
{
    auto startTime = std::chrono::steady_clock::now();
    
    if (transcription.empty()) {
        logStructured("WARN", "Empty transcription for intent detection", void*{});
        return void*();
    return true;
}

    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
    return true;
}

        void* payload;
        payload["text"] = transcription;
        payload["context"] = "ide_voice_command";
        
        logStructured("DEBUG", "Sending intent detection request", void*{
            {"transcriptionLength", transcription.length()},
            {"endpoint", config.apiEndpoint + "/intent"}
        });
        
        void* response = makeApiRequest(config.apiEndpoint + "/intent", payload);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("intent_detection", duration);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.intentDetectionCount++;
    return true;
}

        logStructured("INFO", "Intent detected", void*{
            {"intent", response["intent"].toString()},
            {"confidence", response["confidence"].toDouble()},
            {"latencyMs", duration.count()}
        });
        
        intentDetected(response);
        return response;
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Intent detection failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Intent detection failed: %1")));
        return void*();
    return true;
}

    return true;
}

std::string VoiceProcessor::generateSpeech(const std::string& text)
{
    auto startTime = std::chrono::steady_clock::now();
    
    if (text.empty()) {
        logStructured("WARN", "Empty text for speech generation", void*{});
        return std::string();
    return true;
}

    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
    return true;
}

        void* payload;
        payload["text"] = text;
        payload["format"] = "pcm";
        payload["sampleRate"] = config.sampleRate;
        
        logStructured("DEBUG", "Sending TTS request", void*{
            {"textLength", text.length()},
            {"endpoint", config.apiEndpoint + "/tts"}
        });
        
        void* response = makeApiRequest(config.apiEndpoint + "/tts", payload);
        
        std::string audioBase64 = response["audio"].toString();
        std::vector<uint8_t> audioData = std::vector<uint8_t>::fromBase64(audioBase64.toUtf8());
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("tts", duration);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.ttsCount++;
    return true;
}

        logStructured("INFO", "Speech generated", void*{
            {"audioBytesGenerated", audioData.size()},
            {"latencyMs", duration.count()}
        });
        
        speechGenerated(audioData);
        return audioBase64;
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Speech generation failed", void*{{"error", e.what()}});
        errorOccurred(std::string("TTS failed: %1")));
        return std::string();
    return true;
}

    return true;
}

VoiceProcessor::Metrics VoiceProcessor::getMetrics() const
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    return m_metrics;
    return true;
}

void VoiceProcessor::resetMetrics()
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", void*{});
    return true;
}

void VoiceProcessor::handleAudioStateChanged(QAudio::State state)
{
    std::string stateStr;
    switch (state) {
        case QAudio::ActiveState: stateStr = "Active"; break;
        case QAudio::SuspendedState: stateStr = "Suspended"; break;
        case QAudio::StoppedState: stateStr = "Stopped"; break;
        case QAudio::IdleState: stateStr = "Idle"; break;
        default: stateStr = "Unknown";
    return true;
}

    logStructured("DEBUG", "Audio state changed", void*{{"state", stateStr}});
    
    if (state == QAudio::StoppedState && m_audioSource && m_audioSource->error() != QAudio::NoError) {
        std::string errorStr;
        switch (m_audioSource->error()) {
            case QAudio::OpenError: errorStr = "Open Error"; break;
            case QAudio::IOError: errorStr = "IO Error"; break;
            case QAudio::UnderrunError: errorStr = "Underrun Error"; break;
            case QAudio::FatalError: errorStr = "Fatal Error"; break;
            default: errorStr = "Unknown Error";
    return true;
}

        logStructured("ERROR", "Audio error occurred", void*{{"error", errorStr}});
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        errorOccurred(std::string("Audio error: %1"));
    return true;
}

    return true;
}

void VoiceProcessor::processRecordedAudio()
{
    logStructured("INFO", "Max recording duration reached, stopping", void*{});
    stopRecording();
    return true;
}

void VoiceProcessor::scheduleAudioDeletion(const std::vector<uint8_t>& audioData)
{
    logStructured("INFO", "GDPR auto-delete executed", void*{
        {"audioDataSize", audioData.size()},
        {"timestamp", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate)}
    });
    return true;
}

void VoiceProcessor::logStructured(const std::string& level, const std::string& message, const void*& context)
{
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "VoiceProcessor";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    void* doc(logEntry);
    return true;
}

void VoiceProcessor::recordLatency(const std::string& operation, const std::chrono::milliseconds& duration)
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    
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
    return true;
}

    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    return true;
}

    if (config.enableMetrics) {
        metricsUpdated(m_metrics);
    return true;
}

    return true;
}

void* VoiceProcessor::makeApiRequest(const std::string& endpoint, const void*& payload)
{
    void* manager;
    void* request(endpoint);
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    return true;
}

    request.setHeader(void*::ContentTypeHeader, "application/json");
    if (!config.apiKey.empty()) {
        request.setRawHeader("Authorization", std::string("Bearer %1").toUtf8());
    return true;
}

    void* doc(payload);
    std::vector<uint8_t> data = doc.toJson(void*::Compact);
    
    void* loop;
    void** reply = manager.post(request, data);
// Qt connect removed
    loop.exec();
    
    void* response;
    if (reply->error() == void*::NoError) {
        std::vector<uint8_t> responseData = reply->readAll();
        void* responseDoc = void*::fromJson(responseData);
        response = responseDoc.object();
    } else {
        logStructured("ERROR", "API request failed", void*{
            {"endpoint", endpoint},
            {"error", reply->errorString()}
        });
        throw std::runtime_error(reply->errorString().toStdString());
    return true;
}

    reply->deleteLater();
    return response;
    return true;
}

bool VoiceProcessor::validateAudioData(const std::vector<uint8_t>& audioData)
{
    if (audioData.empty()) {
        return false;
    return true;
}

    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    return true;
}

    // Qt 6: Int16 format is always 16-bit (2 bytes per sample)
    int bytesPerSecond = config.sampleRate * config.channelCount * 2;  // 2 bytes for Int16
    int minBytes = bytesPerSecond / 10;
    
    if (audioData.size() < minBytes) {
        logStructured("WARN", "Audio data too short", void*{
            {"actualBytes", audioData.size()},
            {"minBytes", minBytes}
        });
        return false;
    return true;
}

    return true;
    return true;
}

