#pragma once
// streaming_gguf_loader.h - Minimal stub for Win32 IDE build
// The actual streaming GGUF loading is done via the native model bridge DLL

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace GGUF {

class StreamingGGUFLoader {
public:
    using ProgressCallback = std::function<void(float progress, const std::string& status)>;
    using TokenCallback = std::function<void(const std::string& token)>;
    
    StreamingGGUFLoader() = default;
    ~StreamingGGUFLoader() = default;
    
    bool loadAsync(const std::string& filepath, ProgressCallback callback = nullptr) {
        m_filepath = filepath;
        m_loaded = true;
        if (callback) callback(1.0f, "Loaded");
        return true;
    }
    
    void unload() {
        m_loaded = false;
        m_filepath.clear();
    }
    
    bool isLoaded() const { return m_loaded; }
    bool isLoading() const { return false; }
    
    float getLoadProgress() const { return m_loaded ? 1.0f : 0.0f; }
    
    std::string generateToken(const std::string& prompt, TokenCallback callback = nullptr) {
        // Stub - actual implementation via native bridge
        return "";
    }
    
    void stopGeneration() {}
    
    void setMaxTokens(int tokens) { m_maxTokens = tokens; }
    void setTemperature(float temp) { m_temperature = temp; }
    void setTopP(float topP) { m_topP = topP; }
    void setTopK(int topK) { m_topK = topK; }

private:
    std::string m_filepath;
    bool m_loaded = false;
    int m_maxTokens = 512;
    float m_temperature = 0.7f;
    float m_topP = 0.9f;
    int m_topK = 40;
};

} // namespace GGUF
