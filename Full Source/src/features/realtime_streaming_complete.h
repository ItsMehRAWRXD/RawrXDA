#pragma once

#include <string>
#include <functional>

struct StreamChunk {
    std::string text;
    float confidence;
    bool isComplete;
};

struct StreamRequest {
    std::string code;
    int cursorPos;
    std::string instruction;
    std::string language;
    std::string model;
};

using StreamCallback = std::function<void(const StreamChunk&)>;

class RealtimeStreamingEngine {
public:
    RealtimeStreamingEngine();
    ~RealtimeStreamingEngine();
    
    bool initialize(const std::string& modelPath);
    void setExternalAPI(const std::string& provider, const std::string& apiKey);
    
    void startStreaming(const std::string& code,
                       int cursorPos,
                       const std::string& instruction,
                       const std::string& language,
                       StreamCallback callback);
    
    void stopStreaming();
    bool isStreaming() const;
    
private:
    class Impl;
    Impl* m_impl;
};
