// Stub implementation for voice processing (without SAPI dependencies)
#include "voice_processor.h"
#include <iostream>

VoiceProcessor::VoiceProcessor() {}

VoiceProcessor::~VoiceProcessor() {
    stopRecording();
    stopSpeaking();
}

bool VoiceProcessor::startRecording() {
    if (m_isRecording.load()) return false;
    m_isRecording.store(true);
    if (m_recordingStateCallback) m_recordingStateCallback(true);
    return true;
}

void VoiceProcessor::stopRecording() {
    m_isRecording.store(false);
    if (m_recordingStateCallback) m_recordingStateCallback(false);
}

bool VoiceProcessor::speakText(const std::string& text, Accent accent) {
    if (m_isPlaying.load()) return false;
    m_currentAccent = accent;
    m_isPlaying.store(true);
    m_isPlaying.store(false);
    return true;
}

void VoiceProcessor::stopSpeaking() {
    m_isPlaying.store(false);
}

void VoiceProcessor::setAccent(Accent accent) {
    m_currentAccent = accent;
}

std::string VoiceProcessor::accentToLocale(Accent accent) const {
    return "en-US";
}

void VoiceProcessor::setVolume(int percent) {
    m_volume = (percent < 0) ? 0 : (percent > 100) ? 100 : percent;
}

void VoiceProcessor::setTextRecognizedCallback(TextRecognizedCallback callback) { 
    m_textRecognizedCallback = std::move(callback); 
}

void VoiceProcessor::setErrorCallback(ErrorCallback callback) { 
    m_errorCallback = std::move(callback); 
}

void VoiceProcessor::setRecordingStateCallback(RecordingStateCallback callback) { 
    m_recordingStateCallback = std::move(callback); 
}

void VoiceProcessor::setSampleRate(int rate) { m_sampleRate = rate; }

void VoiceProcessor::setChannels(int channels) { m_channels = channels; }

void VoiceProcessor::setBitsPerSample(int bits) { m_bitsPerSample = bits; }

void VoiceProcessor::initializeAudio() {}

void VoiceProcessor::cleanupAudio() {}