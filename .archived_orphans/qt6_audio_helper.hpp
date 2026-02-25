#pragma once


/**
 * @class Qt6AudioHelper
 * @brief Helper class for Qt 6.x audio configuration
 * 
 * Provides compatibility layer for Qt 6 audio API changes:
 * - QAudioInput -> QAudioSource
 * - QAudioFormat enums replaced with setSampleFormat()
 * - QAudioDeviceInfo -> QMediaDevices
 */
class Qt6AudioHelper {
public:
    /**
     * @brief Create a production-ready QAudioFormat for voice recording
     * @param sampleRate Sample rate in Hz (default: 16000 for speech)
     * @param channelCount Number of channels (1=mono, 2=stereo)
     * @return Configured QAudioFormat
     */
    static QAudioFormat createVoiceFormat(int sampleRate = 16000, int channelCount = 1) {
        QAudioFormat format;
        
        // Set sample rate
        format.setSampleRate(sampleRate);
        
        // Set channel configuration (Qt 6 API)
        if (channelCount == 1) {
            format.setChannelConfig(QAudioFormat::ChannelConfigMono);
        } else if (channelCount == 2) {
            format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
        } else {
            // For multi-channel, use setChannelCount
            format.setChannelCount(channelCount);
        }
        
        // Set sample format (replaces old SampleType and Endian enums)
        // Int16 is little-endian signed 16-bit integer (replaces SignedInt + LittleEndian)
        format.setSampleFormat(QAudioFormat::Int16);
        
        return format;
    }
    
    /**
     * @brief Get the default audio input device
     * @return Default QAudioDevice for input
     */
    static QAudioDevice getDefaultInputDevice() {
        return QMediaDevices::defaultAudioInput();
    }
    
    /**
     * @brief Get the default audio output device
     * @return Default QAudioDevice for output
     */
    static QAudioDevice getDefaultOutputDevice() {
        return QMediaDevices::defaultAudioOutput();
    }
    
    /**
     * @brief Check if a format is supported by the given device
     * @param device Audio device to check
     * @param format Format to validate
     * @return true if format is supported, false otherwise
     */
    static bool isFormatSupported(const QAudioDevice& device, const QAudioFormat& format) {
        return device.isFormatSupported(format);
    }
    
    /**
     * @brief Get nearest supported format for a device
     * @param device Audio device
     * @param desiredFormat Desired format
     * @return Nearest supported format (or desiredFormat if already supported)
     */
    static QAudioFormat getNearestFormat(const QAudioDevice& device, const QAudioFormat& desiredFormat) {
        if (device.isFormatSupported(desiredFormat)) {
            return desiredFormat;
        }
        
        // Return device's preferred format if desired format not supported
        return device.preferredFormat();
    }
    
    /**
     * @brief Validate audio format configuration
     * @param format Format to validate
     * @return true if format is valid, false otherwise
     */
    static bool validateFormat(const QAudioFormat& format) {
        return format.isValid() 
            && format.sampleRate() > 0 
            && format.channelCount() > 0;
    }
};

