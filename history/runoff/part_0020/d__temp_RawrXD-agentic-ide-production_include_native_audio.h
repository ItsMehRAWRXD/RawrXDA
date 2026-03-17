#pragma once

#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <string>

class NativeAudioOutput {
public:
    NativeAudioOutput();
    ~NativeAudioOutput();
    
    bool play(const std::vector<unsigned char>& data);
    void stop();
    bool isPlaying() const;
    void setVolume(int percent);
    
private:
    void playAudioDataInternal();
    
    void* m_platformHandle;
    bool m_playing;
    int m_volume;
    std::vector<unsigned char> m_audioData;
};