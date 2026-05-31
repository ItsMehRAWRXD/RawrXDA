#pragma once

#include <functional>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace RawrXD::Standalone {
class LlamaRuntime;
}

namespace RawrXD::Runtime {

struct RuntimeTelemetry {
    long long loadTimeMs = 0;
    float timeToFirstTokenS = 0.0f;
    float tokensPerSecond = 0.0f;
    unsigned long long vramUsageBytes = 0;
    unsigned long long vramTotalBytes = 0;
    std::string activeBackend;
};

class RuntimeProvider {
public:
    RuntimeProvider();
    ~RuntimeProvider();

    RuntimeProvider(const RuntimeProvider&) = delete;
    RuntimeProvider& operator=(const RuntimeProvider&) = delete;

    struct GenParams {
        std::string prompt;
        int maxTokens = 512;
        float temperature = 0.7f;
        std::function<void(const std::string&)> onToken;
    };

    bool LoadModel(const std::wstring& modelPath);
    void UnloadModel();
    bool Generate(const GenParams& params);

    // Optional Win32 token messenger bridge. When configured, each generated token
    // is posted as an allocated char* via lParam to messageId on the target HWND.
    // Receiver must free() the token buffer.
    static void ConfigureWin32TokenMessenger(void* hwnd, unsigned int messageId);

    const RuntimeTelemetry& GetTelemetry() const { return m_telemetry; }
    bool IsModelLoaded() const;
    std::string GetLastError() const;

private:
    struct HardwarePolicy {
        bool gpuOffload = false;
        bool avx512f = false;
        bool avx512bw = false;
        int gpuLayers = 0;
        int cpuThreads = 4;
        unsigned long long vramTotalBytes = 0;
        std::string backendLabel = "CPU";
    };

    HardwarePolicy AutoDetectHardware() const;
    static bool DetectAvx512(bool& hasF, bool& hasBW);
    static bool LlamaSupportsGpuOffload();
    static bool DetectAmdAdapter();
    static int DetectPhysicalCoreCount();
    static void DispatchWin32Token(const std::string& token, uint64_t sessionId);

    mutable std::mutex m_mutex;
    std::shared_ptr<Standalone::LlamaRuntime> m_runtime;
    bool m_isLoaded = false;
    std::wstring m_loadedModelPath;
    RuntimeTelemetry m_telemetry;
    HardwarePolicy m_policy;
    std::string m_lastError;
};

}  // namespace RawrXD::Runtime
