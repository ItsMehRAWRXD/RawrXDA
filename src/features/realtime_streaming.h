#pragma once

#include <string>
#include <functional>

using StreamCallback = std::function<void(const std::string& token, bool done)>;

class RealtimeStreamingEngine {
public:
    RealtimeStreamingEngine();
    ~RealtimeStreamingEngine();
    
    bool initialize(const std::string& modelPath);
    
    void startStreaming(const std::string& prompt,
                       const std::string& context,
                       StreamCallback callback);
    
    void stopStreaming();
    bool isStreaming() const;
    
private:
    class Impl;
    Impl* m_impl;
};
