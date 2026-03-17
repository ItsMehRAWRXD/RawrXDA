#include "voice_processor.h"
#include "logger.h"
#include <iostream>

VoiceProcessor::VoiceProcessor() {
    log_info("VoiceProcessor initialized");
    m_accent = VoiceAccent::AmericanEnglish;
}

VoiceProcessor::~VoiceProcessor() {
    if (m_recording) {
        stopRecording();
    }
    log_info("VoiceProcessor destroyed");
}

void VoiceProcessor::startRecording() {
    if (!m_recording) {
        m_recording = true;
        m_audioBuffer.clear();
        log_info("Voice recording started");
    }
}

void VoiceProcessor::stopRecording() {
    if (m_recording) {
        m_recording = false;
        log_info("Voice recording stopped, buffer size: " + std::to_string(m_audioBuffer.size()));
        
        if (m_audioCallback) {
            // Convert audio buffer to string for callback
            std::string audioData(m_audioBuffer.begin(), m_audioBuffer.end());
            m_audioCallback(audioData);
        }
    }
}

void VoiceProcessor::setMicrophoneVolume(int volume) {
    m_micVolume = std::max(0, std::min(100, volume));
    log_debug("Microphone volume set to: " + std::to_string(m_micVolume));
}

void VoiceProcessor::setSpeakerVolume(int volume) {
    m_speakerVolume = std::max(0, std::min(100, volume));
    log_debug("Speaker volume set to: " + std::to_string(m_speakerVolume));
}

void VoiceProcessor::play(const std::string& text, VoiceAccent accent) {
    log_info("Playing audio: " + text);
    // Placeholder for actual audio playback
}

void VoiceProcessor::stop() {
    stopRecording();
    log_debug("Audio playback stopped");
}

std::string VoiceProcessor::recognizeAudio() {
    // Placeholder for speech recognition
    log_debug("Audio recognition requested");
    return "";
}

std::vector<std::string> VoiceProcessor::getAvailableAccents() const {
    return {
        "British English",
        "American English",
        "Australian English",
        "Indian English",
        "Canadian English",
        "Irish English"
    };
}
