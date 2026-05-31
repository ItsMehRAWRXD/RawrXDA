#pragma once
#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace RawrXD {

// Model host client for connecting to local/remote model servers
class ModelHostClient {
public:
    struct ModelInfo {
        std::string name;
        std::string path;
        size_t size;
        std::string format;
        bool loaded;
    };
    
    struct GenerationParams {
        std::string prompt;
        int maxTokens = 2048;
        float temperature = 0.7f;
        float topP = 0.9f;
        int topK = 40;
        float repeatPenalty = 1.1f;
        std::string stopSequence;
    };
    
    struct GenerationResult {
        std::string text;
        bool success;
        std::string error;
        int tokensGenerated;
        float tokensPerSecond;
    };
    
    using ProgressCallback = std::function<void(float progress, const std::string& status)>;
    using TokenCallback = std::function<void(const std::string& token)>;
    
    ModelHostClient();
    ~ModelHostClient();
    
    bool Connect(const std::string& endpoint);
    void Disconnect();
    bool IsConnected() const;
    
    std::vector<ModelInfo> ListModels();
    bool LoadModel(const std::string& modelPath, ProgressCallback callback = nullptr);
    bool UnloadModel();
    bool IsModelLoaded() const;
    
    GenerationResult Generate(const GenerationParams& params);
    bool GenerateStreaming(const GenerationParams& params, TokenCallback callback);
    
    std::string GetEndpoint() const { return endpoint_; }
    void SetTimeout(int seconds) { timeoutSeconds_ = seconds; }
    
private:
    std::string endpoint_;
    bool connected_;
    bool modelLoaded_;
    int timeoutSeconds_;
    void* httpClient_; // Opaque handle for HTTP client
    
    bool SendRequest(const std::string& path, const std::string& body, std::string& response);
};

} // namespace RawrXD
