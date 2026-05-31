/*
 * RawrXD_InferenceEngine_Win32.cpp
 * Pure Win32 replacement for Qt-based InferenceEngine
 * Pure C++20/Win32: CRITICAL_SECTION, CreateThread, std::thread, STL.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdarg>
#include <intrin.h>

// ── Last-error scratch buffer ─────────────────────────────────────────────────
// Thread-safety note: the DLL is single-engine and callers already serialise
// through CRITICAL_SECTION; plain static char[] is sufficient.
static char g_lastEngineError[512] = {};

static void SetLastEngineError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_lastEngineError, sizeof(g_lastEngineError), fmt, args);
    va_end(args);
    g_lastEngineError[sizeof(g_lastEngineError) - 1] = '\0';
}

static void ClearLastEngineError() {
    g_lastEngineError[0] = '\0';
}

// ── Native bridge loading + kernel mode gate ───────────────────────────────
enum class KernelMode {
    Auto,
    Stub,
    Bridge
};

static HMODULE g_bridgeModule = nullptr;
static int (*g_bridgeForward)(void*, int*, int, float*) = nullptr;
static int (*g_bridgeSample)(float*, int, float, float, int) = nullptr;
static int (*g_bridgeLoadModel)(const char*) = nullptr;
static void (*g_bridgeCleanup)(void) = nullptr;
static int (*g_bridgeDetokenize)(const int32_t*, int, char*, int) = nullptr;
static std::once_flag g_bridgeInitOnce;

static void InitBridgeOnce() {
    g_bridgeModule = LoadLibraryW(L"RawrXD_NativeModelBridge.dll");
    if (!g_bridgeModule) {
        return;
    }

    g_bridgeForward = (int (*)(void*, int*, int, float*))GetProcAddress(g_bridgeModule, "ForwardPass");
    g_bridgeSample = (int (*)(float*, int, float, float, int))GetProcAddress(g_bridgeModule, "SampleNext");
    g_bridgeLoadModel = (int (*)(const char*))GetProcAddress(g_bridgeModule, "LoadModelNative");
    g_bridgeCleanup = (void (*)(void))GetProcAddress(g_bridgeModule, "CleanupMathTables");

    // Optional bridge API for proper text decoding from token IDs.
    g_bridgeDetokenize = (int (*)(const int32_t*, int, char*, int))GetProcAddress(g_bridgeModule, "Detokenize");
    if (!g_bridgeDetokenize) {
        g_bridgeDetokenize = (int (*)(const int32_t*, int, char*, int))GetProcAddress(g_bridgeModule, "TokensToText");
    }
    if (!g_bridgeDetokenize) {
        g_bridgeDetokenize = (int (*)(const int32_t*, int, char*, int))GetProcAddress(g_bridgeModule, "TokenIdsToText");
    }
}

static bool EnsureBridgeLoaded() {
    std::call_once(g_bridgeInitOnce, InitBridgeOnce);
    return g_bridgeModule != nullptr;
}

static KernelMode GetKernelMode() {
    char mode[32] = {};
    DWORD len = GetEnvironmentVariableA("RAWRXD_KERNEL_MODE", mode, static_cast<DWORD>(sizeof(mode)));
    if (len > 0 && len < sizeof(mode)) {
        if (_stricmp(mode, "stub") == 0) {
            return KernelMode::Stub;
        }
        if (_stricmp(mode, "bridge") == 0) {
            return KernelMode::Bridge;
        }
    }
    return KernelMode::Auto;
}

static bool IsTruthyEnv(const char* value) {
    if (!value || !value[0]) return false;
    return (_stricmp(value, "1") == 0) || (_stricmp(value, "true") == 0) ||
           (_stricmp(value, "yes") == 0) || (_stricmp(value, "on") == 0);
}

static bool WantsNativeKernels() {
    // Primary gate for Phase 1 kernel bridge.
    char nativeGate[32] = {};
    const DWORD nativeLen = GetEnvironmentVariableA(
        "RAWRXD_USE_NATIVE_KERNELS", nativeGate, static_cast<DWORD>(sizeof(nativeGate)));
    if (nativeLen > 0 && nativeLen < sizeof(nativeGate)) {
        return IsTruthyEnv(nativeGate);
    }

    // Backward-compat override for earlier lane selector.
    const KernelMode mode = GetKernelMode();
    if (mode == KernelMode::Bridge) return true;
    if (mode == KernelMode::Stub) return false;
    return false;
}

static bool CpuSupportsAvx2() {
    int info[4] = {0, 0, 0, 0};
    __cpuid(info, 1);

    const bool osxsave = (info[2] & (1 << 27)) != 0;
    const bool avx = (info[2] & (1 << 28)) != 0;
    if (!osxsave || !avx) {
        return false;
    }

    const unsigned long long xcr0 = _xgetbv(0);
    const bool xmmYmmEnabled = (xcr0 & 0x6) == 0x6;
    if (!xmmYmmEnabled) {
        return false;
    }

    int ext[4] = {0, 0, 0, 0};
    __cpuidex(ext, 7, 0);
    const bool avx2 = (ext[1] & (1 << 5)) != 0;
    return avx2;
}

static std::string SanitizeForChatOutput(const std::string& input) {
    std::string out;
    out.reserve(input.size());

    size_t nonPrintable = 0;
    for (unsigned char ch : input) {
        if (ch == '\r' || ch == '\n' || ch == '\t') {
            out.push_back(static_cast<char>(ch));
            continue;
        }

        if (ch >= 32 && ch <= 126) {
            out.push_back(static_cast<char>(ch));
        } else {
            nonPrintable++;
            out.push_back(' ');
        }
    }

    // Collapse repeated spaces to keep the chat panel readable.
    std::string compact;
    compact.reserve(out.size());
    bool lastSpace = false;
    for (char c : out) {
        const bool isSpace = (c == ' ');
        if (isSpace && lastSpace) {
            continue;
        }
        compact.push_back(c);
        lastSpace = isSpace;
    }

    // If output is mostly binary noise, provide deterministic readable text.
    if (!input.empty() && nonPrintable * 3 > input.size() * 2) {
        return "[NativeKernelOutput] non-text token stream suppressed";
    }

    if (compact.empty()) {
        return "[NativeKernelOutput] empty";
    }

    return compact;
}

// Structures matching Qt interface
struct ModelMetrics {
    uint64_t total_tokens_generated = 0;
    double total_latency_ms = 0.0;
    double avg_latency_ms = 0.0;
    size_t inference_count = 0;
    ULONGLONG last_inference_tick = 0;
};

struct ModelMemory {
    size_t model_vram_mb = 0;
    size_t cache_vram_mb = 0;
    size_t system_ram_mb = 0;
    size_t peak_vram_mb = 0;
};

struct Quantization {
    wchar_t mode[64];
    int bits;
    float scale;
    Quantization() : bits(8), scale(1.0f) { wcscpy_s(mode, L"int8"); }
};

struct InferenceRequest {
    std::string prompt;
    size_t max_tokens;
    float temperature;
    float top_p;
    uint32_t request_id;
    HANDLE complete_event;
};

struct InferenceResult {
    std::string generated_text;
    size_t tokens_used;
    double latency_ms;
    uint32_t request_id;
};

struct ProcessorOutput {
    std::string generated_text;
    size_t tokens_used = 0;
};

class IInferenceProcessor {
public:
    virtual ~IInferenceProcessor() = default;
    virtual const char* Name() const = 0;
    virtual bool Generate(const InferenceRequest& request,
                          int contextWindow,
                          bool modelLoaded,
                          ProcessorOutput& out,
                          std::string& error) = 0;
};

class StubProcessor final : public IInferenceProcessor {
public:
    const char* Name() const override { return "stub"; }

    bool Generate(const InferenceRequest& request,
                  int contextWindow,
                  bool modelLoaded,
                  ProcessorOutput& out,
                  std::string& error) override {
        (void)contextWindow;
        (void)modelLoaded;
        (void)error;
        out.generated_text = request.prompt;
        out.tokens_used = request.max_tokens > 50 ? 50 : static_cast<size_t>(request.max_tokens);
        return true;
    }
};

class NativeKernelProcessor final : public IInferenceProcessor {
public:
    const char* Name() const override { return "native_bridge"; }

    bool Generate(const InferenceRequest& request,
                  int contextWindow,
                  bool modelLoaded,
                  ProcessorOutput& out,
                  std::string& error) override {
        if (!modelLoaded) {
            error = "model not loaded";
            return false;
        }
        if (!EnsureBridgeLoaded() || !g_bridgeForward) {
            error = "RawrXD_NativeModelBridge.dll missing ForwardPass";
            return false;
        }

        std::vector<int> inputTokens;
        inputTokens.reserve(request.prompt.size());
        for (unsigned char ch : request.prompt) {
            inputTokens.push_back(static_cast<int>(ch));
        }
        if (inputTokens.size() > static_cast<size_t>(contextWindow)) {
            inputTokens.erase(inputTokens.begin(),
                              inputTokens.begin() + (inputTokens.size() - contextWindow));
        }

        std::vector<float> logits(32000, 0.0f);
        std::vector<int32_t> generatedTokenIds;
        generatedTokenIds.reserve(std::min(request.max_tokens, static_cast<size_t>(2048)));

        int fwdResult = g_bridgeForward(nullptr, inputTokens.data(),
                                        static_cast<int>(inputTokens.size()), logits.data());
        if (fwdResult != 0) {
            error = "ForwardPass failed with code " + std::to_string(fwdResult);
            return false;
        }

        size_t generated = 0;
        const size_t maxGen = std::min(request.max_tokens, static_cast<size_t>(2048));
        for (size_t i = 0; i < maxGen; i++) {
            int nextToken = -1;
            if (g_bridgeSample) {
                nextToken = g_bridgeSample(logits.data(), 32000,
                                           request.temperature, request.top_p, 40);
            } else {
                float maxLogit = -1e30f;
                for (int v = 0; v < 32000; v++) {
                    if (logits[v] > maxLogit) maxLogit = logits[v];
                }

                const float temp = request.temperature > 0.01f ? request.temperature : 0.01f;
                float sumExp = 0.0f;
                for (int v = 0; v < 32000; v++) {
                    logits[v] = expf((logits[v] - maxLogit) / temp);
                    sumExp += logits[v];
                }
                if (sumExp <= 0.0f) {
                    error = "invalid logits softmax sum";
                    return false;
                }
                for (int v = 0; v < 32000; v++) {
                    logits[v] /= sumExp;
                }

                struct TokenProb {
                    int id;
                    float prob;
                };
                std::vector<TokenProb> sorted;
                sorted.reserve(32000);
                for (int v = 0; v < 32000; v++) {
                    sorted.push_back({v, logits[v]});
                }
                std::sort(sorted.begin(), sorted.end(),
                          [](const TokenProb& a, const TokenProb& b) { return a.prob > b.prob; });

                float cumProb = 0.0f;
                const float topP = request.top_p;
                size_t cutoff = sorted.size();
                for (size_t j = 0; j < sorted.size(); j++) {
                    cumProb += sorted[j].prob;
                    if (cumProb >= topP) {
                        cutoff = j + 1;
                        break;
                    }
                }

                float r = static_cast<float>(rand()) / RAND_MAX;
                float accum = 0.0f;
                float nucleusSum = 0.0f;
                for (size_t j = 0; j < cutoff; j++) {
                    nucleusSum += sorted[j].prob;
                }
                if (nucleusSum <= 0.0f) {
                    error = "invalid nucleus probability sum";
                    return false;
                }

                for (size_t j = 0; j < cutoff; j++) {
                    accum += sorted[j].prob / nucleusSum;
                    if (accum >= r) {
                        nextToken = sorted[j].id;
                        break;
                    }
                }
                if (nextToken < 0) {
                    nextToken = sorted[0].id;
                }
            }

            if (nextToken <= 0 || nextToken == 2) {
                break;
            }
            generatedTokenIds.push_back(static_cast<int32_t>(nextToken));
            generated++;

            int singleToken = nextToken;
            fwdResult = g_bridgeForward(nullptr, &singleToken, 1, logits.data());
            if (fwdResult != 0) {
                break;
            }
        }

        if (!generatedTokenIds.empty() && g_bridgeDetokenize) {
            std::vector<char> decoded(64 * 1024, 0);
            const int wrote = g_bridgeDetokenize(generatedTokenIds.data(),
                                                 static_cast<int>(generatedTokenIds.size()),
                                                 decoded.data(),
                                                 static_cast<int>(decoded.size()));
            if (wrote > 0) {
                out.generated_text.assign(decoded.data(), decoded.data() + wrote);
                out.generated_text = SanitizeForChatOutput(out.generated_text);
            } else {
                out.generated_text = "[NativeKernelOutput] detokenize failed";
            }
        } else {
            out.generated_text = "[NativeKernelOutput] token stream generated; bridge detokenizer export unavailable";
        }
        out.tokens_used = generated;
        return true;
    }
};

class RawrXDInferenceEngine {
private:
    // Synchronization
    mutable CRITICAL_SECTION m_criticalSection;
    mutable CRITICAL_SECTION m_cacheCriticalSection;
    HANDLE m_workerThreads[4];
    volatile bool m_shutdown;
    volatile bool m_modelLoaded;
    
    // Model state
    wchar_t m_modelPath[MAX_PATH];
    ModelMetrics m_metrics;
    ModelMemory m_memory;
    Quantization m_quantization;
    
    // Inference queue
    std::queue<InferenceRequest> m_inferenceQueue;
    std::map<uint32_t, InferenceResult> m_resultCache;
    HANDLE m_queueEvent;
    
    // Configuration
    float m_temperature;
    float m_top_p;
    int m_contextWindow;
    size_t m_batchSize;
    std::atomic<uint32_t> m_requestCounter;
    bool m_nativeKernelsRequested;
    bool m_avx2Available;
    std::unique_ptr<IInferenceProcessor> m_processor;
    
    // Callbacks
    mutable std::function<void(const wchar_t*)> m_progressCallback;
    mutable std::function<void(const wchar_t*)> m_errorCallback;

    void ConfigureProcessor() {
        m_nativeKernelsRequested = WantsNativeKernels();
        m_avx2Available = CpuSupportsAvx2();

        if (m_nativeKernelsRequested && m_avx2Available) {
            EnsureBridgeLoaded();
            if (g_bridgeForward) {
                m_processor = std::make_unique<NativeKernelProcessor>();
                return;
            }
            SetLastEngineError("Native kernel bridge requested but bridge DLL/ForwardPass not available; falling back to stub");
        } else if (m_nativeKernelsRequested && !m_avx2Available) {
            SetLastEngineError("Native kernel bridge requested but AVX2 unsupported; falling back to stub");
        }

        m_processor = std::make_unique<StubProcessor>();
    }
    
    // Worker thread function
    static DWORD WINAPI WorkerThreadProc(LPVOID param) {
        RawrXDInferenceEngine* self = static_cast<RawrXDInferenceEngine*>(param);
        return self->WorkerThread();
    }
    
    DWORD WorkerThread() {
        while (!m_shutdown) {
            DWORD result = WaitForSingleObject(m_queueEvent, 1000);
            
            if (result == WAIT_OBJECT_0) {
                EnterCriticalSection(&m_criticalSection);
                
                if (!m_inferenceQueue.empty()) {
                    InferenceRequest request = m_inferenceQueue.front();
                    m_inferenceQueue.pop();
                    
                    LeaveCriticalSection(&m_criticalSection);
                    
                    // Process inference
                    InferenceResult result = ProcessInference(request);
                    
                    EnterCriticalSection(&m_criticalSection);
                    m_resultCache[result.request_id] = result;
                    m_metrics.inference_count++;
                    m_metrics.total_latency_ms += result.latency_ms;
                    m_metrics.avg_latency_ms = m_metrics.total_latency_ms / m_metrics.inference_count;
                    LeaveCriticalSection(&m_criticalSection);
                    
                    // Signal completion
                    if (request.complete_event) {
                        SetEvent(request.complete_event);
                    }
                } else {
                    LeaveCriticalSection(&m_criticalSection);
                }
                
                ResetEvent(m_queueEvent);
            }
        }
        return 0;
    }
    
    InferenceResult ProcessInference(const InferenceRequest& request) {
        ULONGLONG startTick = GetTickCount64();
        
        InferenceResult result;
        result.request_id = request.request_id;
        result.tokens_used = 0;
        
        if (!m_processor) {
            ConfigureProcessor();
        }

        ProcessorOutput generated;
        std::string generateError;
        const bool ok = m_processor && m_processor->Generate(
            request, m_contextWindow, m_modelLoaded, generated, generateError);

        if (ok) {
            result.generated_text = std::move(generated.generated_text);
            result.tokens_used = generated.tokens_used;
        } else {
            if (GetKernelMode() == KernelMode::Bridge || m_nativeKernelsRequested) {
                result.generated_text = "[KernelBridgeError] " +
                    (generateError.empty() ? "Native processor failed" : generateError);
                result.tokens_used = 0;
            } else {
                StubProcessor stub;
                ProcessorOutput fallback;
                std::string ignored;
                stub.Generate(request, m_contextWindow, m_modelLoaded, fallback, ignored);
                result.generated_text = std::move(fallback.generated_text);
                result.tokens_used = fallback.tokens_used;
            }
        }
        
        result.latency_ms = static_cast<double>(GetTickCount64() - startTick);
        
        EnterCriticalSection(&m_criticalSection);
        m_metrics.total_tokens_generated += result.tokens_used;
        LeaveCriticalSection(&m_criticalSection);
        
        return result;
    }

public:
    RawrXDInferenceEngine() 
        : m_shutdown(false), m_modelLoaded(false), 
          m_temperature(0.7f), m_top_p(0.95f), 
          m_contextWindow(4096), m_batchSize(32),
          m_requestCounter(1000),
          m_nativeKernelsRequested(false),
          m_avx2Available(false) {
        
        InitializeCriticalSection(&m_criticalSection);
        InitializeCriticalSection(&m_cacheCriticalSection);
        
        m_queueEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        
        ZeroMemory(m_modelPath, sizeof(m_modelPath));
        ZeroMemory(m_workerThreads, sizeof(m_workerThreads));

        ConfigureProcessor();
        
        // Start worker threads
        for (int i = 0; i < 4; i++) {
            m_workerThreads[i] = CreateThread(
                nullptr, 0, WorkerThreadProc, this, 0, nullptr);
        }
    }
    
    ~RawrXDInferenceEngine() {
        m_shutdown = true;
        SetEvent(m_queueEvent);
        
        // Wait for worker threads
        for (int i = 0; i < 4; i++) {
            if (m_workerThreads[i]) {
                WaitForSingleObject(m_workerThreads[i], 5000);
                CloseHandle(m_workerThreads[i]);
            }
        }
        
        CloseHandle(m_queueEvent);
        DeleteCriticalSection(&m_criticalSection);
        DeleteCriticalSection(&m_cacheCriticalSection);
    }
    
    // Exported API
    bool LoadModel(const wchar_t* path) {
        if (!path) {
            SetLastEngineError("LoadModel: null path");
            return false;
        }
        if (!path[0]) {
            SetLastEngineError("LoadModel: empty path");
            return false;
        }

        const DWORD attrs = GetFileAttributesW(path);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            SetLastEngineError("LoadModel: file not found or inaccessible");
            return false;
        }
        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            SetLastEngineError("LoadModel: path points to a directory, expected file");
            return false;
        }
        
        EnterCriticalSection(&m_criticalSection);
        wcscpy_s(m_modelPath, MAX_PATH, path);
        m_modelLoaded = true;
        LeaveCriticalSection(&m_criticalSection);

        ClearLastEngineError();
        
        if (m_progressCallback) {
            m_progressCallback(L"Model loaded successfully");
        }
        
        return true;
    }
    
    uint32_t SubmitInference(const char* prompt, size_t max_tokens) {
        if (!m_modelLoaded) {
            SetLastEngineError("SubmitInference: no model loaded");
            return 0;
        }
        
        uint32_t requestId = m_requestCounter.fetch_add(1);
        
        InferenceRequest request;
        request.prompt = prompt ? prompt : "";
        request.max_tokens = max_tokens;
        request.temperature = m_temperature;
        request.top_p = m_top_p;
        request.request_id = requestId;
        request.complete_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        
        EnterCriticalSection(&m_criticalSection);
        m_inferenceQueue.push(request);
        LeaveCriticalSection(&m_criticalSection);
        
        SetEvent(m_queueEvent);

        ClearLastEngineError();
        
        return requestId;
    }
    
    bool GetResult(uint32_t requestId, char* buffer, size_t bufSize) {
        EnterCriticalSection(&m_criticalSection);
        
        auto it = m_resultCache.find(requestId);
        if (it != m_resultCache.end()) {
            size_t textLen = it->second.generated_text.size();
            size_t copyLen = bufSize - 1 > textLen ? textLen : bufSize - 1;
            strncpy_s(buffer, bufSize, it->second.generated_text.c_str(), copyLen);
            buffer[copyLen] = '\0';
            m_resultCache.erase(it);
            LeaveCriticalSection(&m_criticalSection);
            return true;
        }
        
        LeaveCriticalSection(&m_criticalSection);
        return false;
    }
    
    bool IsModelLoaded() const {
        return m_modelLoaded;
    }
    
    void GetModelPath(wchar_t* buffer, size_t bufSize) const {
        EnterCriticalSection(&m_criticalSection);
        wcscpy_s(buffer, bufSize, m_modelPath);
        LeaveCriticalSection(&m_criticalSection);
    }
    
    double GetTokensPerSecond() const {
        EnterCriticalSection(&m_criticalSection);
        double tps = (m_metrics.total_latency_ms > 0) 
            ? (m_metrics.total_tokens_generated * 1000.0) / m_metrics.total_latency_ms
            : 0.0;
        LeaveCriticalSection(&m_criticalSection);
        return tps;
    }
    
    size_t GetGPUMemoryUsedMB() const {
        EnterCriticalSection(&m_criticalSection);
        size_t total = m_memory.model_vram_mb + m_memory.cache_vram_mb;
        LeaveCriticalSection(&m_criticalSection);
        return total;
    }
    
    size_t GetMemoryUsageMB() const {
        EnterCriticalSection(&m_criticalSection);
        size_t total = m_memory.model_vram_mb + m_memory.cache_vram_mb + m_memory.system_ram_mb;
        LeaveCriticalSection(&m_criticalSection);
        return total;
    }
    
    void SetTemperature(float temp) {
        EnterCriticalSection(&m_criticalSection);
        m_temperature = temp;
        LeaveCriticalSection(&m_criticalSection);
    }
    
    void SetTopP(float p) {
        EnterCriticalSection(&m_criticalSection);
        m_top_p = p;
        LeaveCriticalSection(&m_criticalSection);
    }
    
    void SetQuantizationMode(const wchar_t* mode) {
        EnterCriticalSection(&m_criticalSection);
        wcscpy_s(m_quantization.mode, 64, mode);
        LeaveCriticalSection(&m_criticalSection);
    }
    
    void SetProgressCallback(std::function<void(const wchar_t*)> callback) {
        m_progressCallback = callback;
    }
    
    void SetErrorCallback(std::function<void(const wchar_t*)> callback) {
        m_errorCallback = callback;
    }
    
    int GetVocabSize() const { return 32000; }
    int GetEmbeddingDim() const { return 4096; }
    int GetContextWindow() const { return m_contextWindow; }
    size_t GetBatchSize() const { return m_batchSize; }
    
    ModelMetrics GetMetrics() const {
        EnterCriticalSection(&m_criticalSection);
        ModelMetrics metrics = m_metrics;
        LeaveCriticalSection(&m_criticalSection);
        return metrics;
    }
    
    ModelMemory GetMemoryInfo() const {
        EnterCriticalSection(&m_criticalSection);
        ModelMemory mem = m_memory;
        LeaveCriticalSection(&m_criticalSection);
        return mem;
    }
};

// Global instance
static RawrXDInferenceEngine* g_engine = nullptr;

// C-style exports
extern "C" {
    __declspec(dllexport) void* __stdcall CreateInferenceEngine() {
        if (!g_engine) {
            g_engine = new RawrXDInferenceEngine();
        }
        return g_engine;
    }
    
    __declspec(dllexport) void __stdcall DestroyInferenceEngine(void* engine) {
        if (engine && engine == g_engine) {
            delete g_engine;
            g_engine = nullptr;
        }
    }
    
    __declspec(dllexport) bool __stdcall InferenceEngine_LoadModel(void* engine, const wchar_t* path) {
        RawrXDInferenceEngine* e = static_cast<RawrXDInferenceEngine*>(engine);
        if (!e) { SetLastEngineError("InferenceEngine_LoadModel: null engine handle"); return false; }
        return e->LoadModel(path);
    }

    __declspec(dllexport) uint32_t __stdcall InferenceEngine_SubmitInference(
        void* engine, const char* prompt, size_t maxTokens) {
        RawrXDInferenceEngine* e = static_cast<RawrXDInferenceEngine*>(engine);
        if (!e) { SetLastEngineError("InferenceEngine_SubmitInference: null engine handle"); return 0; }
        if (!prompt || !prompt[0]) { SetLastEngineError("InferenceEngine_SubmitInference: empty prompt"); return 0; }
        return e->SubmitInference(prompt, maxTokens);
    }
    
    __declspec(dllexport) bool __stdcall InferenceEngine_GetResult(
        void* engine, uint32_t requestId, char* buffer, size_t bufSize) {
        RawrXDInferenceEngine* e = static_cast<RawrXDInferenceEngine*>(engine);
        return e ? e->GetResult(requestId, buffer, bufSize) : false;
    }
    
    __declspec(dllexport) bool __stdcall InferenceEngine_IsModelLoaded(void* engine) {
        RawrXDInferenceEngine* e = static_cast<RawrXDInferenceEngine*>(engine);
        return e ? e->IsModelLoaded() : false;
    }
    
    __declspec(dllexport) double __stdcall InferenceEngine_GetTokensPerSecond(void* engine) {
        RawrXDInferenceEngine* e = static_cast<RawrXDInferenceEngine*>(engine);
        return e ? e->GetTokensPerSecond() : 0.0;
    }
    
    __declspec(dllexport) size_t __stdcall InferenceEngine_GetMemoryUsageMB(void* engine) {
        RawrXDInferenceEngine* e = static_cast<RawrXDInferenceEngine*>(engine);
        return e ? e->GetMemoryUsageMB() : 0;
    }
    
    __declspec(dllexport) void __stdcall InferenceEngine_SetTemperature(void* engine, float temp) {
        RawrXDInferenceEngine* e = static_cast<RawrXDInferenceEngine*>(engine);
        if (e) e->SetTemperature(temp);
    }

    __declspec(dllexport) const char* __stdcall InferenceEngine_GetLastError() {
        return g_lastEngineError[0] ? g_lastEngineError : "No error recorded";
    }

    // -------------------------------------------------------------------------
    // Compatibility exports for RawrXD_Win32_IDE (LoadModel/UnloadModel/ForwardPass/SampleNext)
    // ------------------------------------------------------------------------
    static std::string WideToUtf8(const wchar_t* path) {
        if (!path || !*path) return "";
        int n = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
        if (n <= 0) return "";
        std::string s(n - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, path, -1, &s[0], n, nullptr, nullptr);
        return s;
    }

    __declspec(dllexport) void* __stdcall LoadModel(const wchar_t* path) {
        if (!path) return nullptr;
        if (!g_engine) g_engine = new RawrXDInferenceEngine();
        EnsureBridgeLoaded();
        if (g_bridgeLoadModel) {
            std::string utf8 = WideToUtf8(path);
            if (g_bridgeLoadModel(utf8.c_str()) != 0) return nullptr;
        }
        g_engine->LoadModel(path);
        return g_engine;
    }

    __declspec(dllexport) void __stdcall UnloadModel(void* ctx) {
        (void)ctx;
        if (g_bridgeCleanup) g_bridgeCleanup();
    }

    __declspec(dllexport) int __stdcall ForwardPass(void* ctx, int* tokens, int n_tokens, float* logits) {
        (void)ctx;
        EnsureBridgeLoaded();
        if (!g_bridgeForward) return -1;
        return g_bridgeForward(nullptr, tokens, n_tokens, logits);
    }

    __declspec(dllexport) int __stdcall SampleNext(float* logits, int vocab_size, float temperature, float top_p, int top_k) {
        EnsureBridgeLoaded();
        if (!g_bridgeSample) return -1;
        return g_bridgeSample(logits, vocab_size, temperature, top_p, top_k);
    }
}

// DLL entry point
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            OutputDebugStringW(L"RawrXD_InferenceEngine_Win32 loaded\n");
            break;
        case DLL_PROCESS_DETACH:
            if (g_engine) {
                delete g_engine;
                g_engine = nullptr;
            }
            OutputDebugStringW(L"RawrXD_InferenceEngine_Win32 unloaded\n");
            break;
    }
    return TRUE;
}
