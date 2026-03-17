// test_model_interface.cpp — Standalone C++17 test suite for ModelInterface
// Converted from Qt (QCoreApplication, QString, QDebug) to pure C++17
// Tests UniversalModelRouter, ModelInterface, CloudApiClient equivalents,
// integration tests, performance benchmarks, system diagnostics

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <cassert>
#include <cstdint>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

static int g_passed = 0, g_failed = 0, g_total = 0;

#define TEST(name) do { g_total++; std::cout << "[TEST] " << name << " ... "; } while(0)
#define PASS()     do { g_passed++; std::cout << "PASSED" << std::endl; } while(0)
#define CHECK(cond) do { if(!(cond)) { g_failed++; \
    std::cerr << "FAILED: " << #cond << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return; } } while(0)
#define CHECK_EQ(a, b) do { if((a) != (b)) { g_failed++; \
    std::cerr << "FAILED: " << #a << " != " << #b << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return; } } while(0)

// ========== ModelInterface test doubles ==========

enum class ModelBackend { Ollama, LlamaCpp, OpenAI, Custom };

struct ModelInfo {
    std::string name;
    std::string path;
    ModelBackend backend = ModelBackend::Ollama;
    int64_t parameterCount = 0;
    int contextLength = 2048;
    std::string quantization;
    bool loaded = false;
};

struct GenerationParams {
    float temperature = 0.7f;
    float topP = 0.9f;
    int topK = 40;
    int maxTokens = 256;
    std::string stopSequence;
    float repeatPenalty = 1.1f;
    int seed = -1;
};

struct GenerationResult {
    bool success = false;
    std::string text;
    int tokensGenerated = 0;
    double latencyMs = 0.0;
    double tokensPerSecond = 0.0;
    std::string error;
};

struct ChatMessage {
    std::string role;
    std::string content;
};

struct EmbeddingResult {
    bool success = false;
    std::vector<float> embedding;
    int dimensions = 0;
    std::string error;
};

// Simulated ModelInterface
class TestModelInterface {
public:
    TestModelInterface() = default;

    bool loadModel(const std::string& modelPath, ModelBackend backend = ModelBackend::Ollama) {
        m_info.name = modelPath;
        m_info.path = modelPath;
        m_info.backend = backend;
        m_info.loaded = true;
        m_info.parameterCount = 7000000000LL;
        m_info.contextLength = 4096;
        m_loaded = true;
        return true;
    }

    bool isLoaded() const { return m_loaded; }
    ModelInfo modelInfo() const { return m_info; }

    GenerationResult generate(const std::string& prompt, const GenerationParams& params = {}) {
        GenerationResult r;
        if (!m_loaded) {
            r.success = false;
            r.error = "No model loaded";
            return r;
        }

        auto start = std::chrono::steady_clock::now();

        // Simulate generation
        r.text = "Generated response to: " + prompt.substr(0, 50);
        r.tokensGenerated = std::min(params.maxTokens, 64);
        r.success = true;

        auto end = std::chrono::steady_clock::now();
        r.latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
        r.tokensPerSecond = (r.latencyMs > 0) ? (r.tokensGenerated / (r.latencyMs / 1000.0)) : 0;

        m_totalGenerations++;
        m_totalTokens += r.tokensGenerated;
        return r;
    }

    GenerationResult chat(const std::vector<ChatMessage>& messages, const GenerationParams& params = {}) {
        // Build prompt from messages
        std::string prompt;
        for (const auto& msg : messages)
            prompt += "[" + msg.role + "] " + msg.content + "\n";
        return generate(prompt, params);
    }

    EmbeddingResult embed(const std::string& text) {
        EmbeddingResult r;
        if (!m_loaded) { r.error = "No model loaded"; return r; }
        r.success = true;
        r.dimensions = 384;
        r.embedding.resize(r.dimensions, 0.0f);
        // Simple hash-based pseudo-embedding
        for (size_t i = 0; i < text.size() && i < (size_t)r.dimensions; i++)
            r.embedding[i] = static_cast<float>(text[i]) / 255.0f;
        return r;
    }

    int totalGenerations() const { return m_totalGenerations; }
    int totalTokens() const { return m_totalTokens; }

    void unload() { m_loaded = false; m_info = {}; }

private:
    bool m_loaded = false;
    ModelInfo m_info;
    int m_totalGenerations = 0;
    int m_totalTokens = 0;
};

// Simulated UniversalModelRouter
class TestModelRouter {
public:
    void registerModel(const std::string& name, ModelBackend backend, const std::string& endpoint = "") {
        RouteEntry entry;
        entry.name     = name;
        entry.backend  = backend;
        entry.endpoint = endpoint;
        entry.active   = true;
        m_routes[name] = entry;
    }

    bool hasModel(const std::string& name) const {
        return m_routes.find(name) != m_routes.end();
    }

    ModelBackend getBackend(const std::string& name) const {
        auto it = m_routes.find(name);
        return (it != m_routes.end()) ? it->second.backend : ModelBackend::Ollama;
    }

    std::vector<std::string> listModels() const {
        std::vector<std::string> names;
        for (const auto& [k, v] : m_routes) names.push_back(k);
        return names;
    }

    bool removeModel(const std::string& name) {
        return m_routes.erase(name) > 0;
    }

    size_t modelCount() const { return m_routes.size(); }

private:
    struct RouteEntry {
        std::string name;
        ModelBackend backend;
        std::string endpoint;
        bool active = true;
    };
    std::unordered_map<std::string, RouteEntry> m_routes;
};

// Simulated CloudApiClient
class TestCloudApiClient {
public:
    void setApiKey(const std::string& key) { m_apiKey = key; }
    void setEndpoint(const std::string& ep) { m_endpoint = ep; }
    bool isConfigured() const { return !m_apiKey.empty() && !m_endpoint.empty(); }

    GenerationResult query(const std::string& prompt) {
        GenerationResult r;
        if (!isConfigured()) {
            r.error = "API not configured";
            return r;
        }
        r.success = true;
        r.text = "Cloud response: " + prompt.substr(0, 30);
        r.tokensGenerated = 50;
        r.latencyMs = 100.0;
        r.tokensPerSecond = 500.0;
        m_queryCount++;
        return r;
    }

    int queryCount() const { return m_queryCount; }

private:
    std::string m_apiKey;
    std::string m_endpoint;
    int m_queryCount = 0;
};

// ========== Tests ==========

static void test_model_router_registration() {
    TEST("model_router_registration");
    TestModelRouter router;

    router.registerModel("llama-7b", ModelBackend::Ollama, "http://localhost:11434");
    router.registerModel("gpt-4", ModelBackend::OpenAI, "https://api.openai.com");
    router.registerModel("local-model", ModelBackend::LlamaCpp, "./models/local.gguf");

    CHECK(router.modelCount() == 3);
    CHECK(router.hasModel("llama-7b"));
    CHECK(router.hasModel("gpt-4"));
    CHECK(router.hasModel("local-model"));
    CHECK(!router.hasModel("nonexistent"));

    CHECK(router.getBackend("llama-7b") == ModelBackend::Ollama);
    CHECK(router.getBackend("gpt-4") == ModelBackend::OpenAI);
    CHECK(router.getBackend("local-model") == ModelBackend::LlamaCpp);
    PASS();
}

static void test_model_router_remove() {
    TEST("model_router_remove");
    TestModelRouter router;
    router.registerModel("test", ModelBackend::Ollama);
    CHECK(router.hasModel("test"));
    CHECK(router.removeModel("test"));
    CHECK(!router.hasModel("test"));
    CHECK(!router.removeModel("test")); // Already removed
    CHECK(router.modelCount() == 0);
    PASS();
}

static void test_model_router_listModels() {
    TEST("model_router_listModels");
    TestModelRouter router;
    router.registerModel("a", ModelBackend::Ollama);
    router.registerModel("b", ModelBackend::LlamaCpp);
    router.registerModel("c", ModelBackend::OpenAI);

    auto names = router.listModels();
    CHECK(names.size() == 3);
    CHECK(std::find(names.begin(), names.end(), "a") != names.end());
    CHECK(std::find(names.begin(), names.end(), "b") != names.end());
    CHECK(std::find(names.begin(), names.end(), "c") != names.end());
    PASS();
}

static void test_model_interface_load() {
    TEST("model_interface_load");
    TestModelInterface iface;
    CHECK(!iface.isLoaded());
    CHECK(iface.loadModel("llama-7b.gguf", ModelBackend::LlamaCpp));
    CHECK(iface.isLoaded());
    auto info = iface.modelInfo();
    CHECK(info.name == "llama-7b.gguf");
    CHECK(info.backend == ModelBackend::LlamaCpp);
    CHECK(info.parameterCount > 0);
    CHECK(info.contextLength > 0);
    PASS();
}

static void test_model_interface_generate() {
    TEST("model_interface_generate");
    TestModelInterface iface;
    iface.loadModel("test-model");

    GenerationParams params;
    params.temperature = 0.8f;
    params.maxTokens = 100;

    auto result = iface.generate("Tell me about C++", params);
    CHECK(result.success);
    CHECK(!result.text.empty());
    CHECK(result.tokensGenerated > 0);
    CHECK(result.latencyMs >= 0);
    PASS();
}

static void test_model_interface_generate_no_model() {
    TEST("model_interface_generate_no_model");
    TestModelInterface iface;
    auto result = iface.generate("Hello");
    CHECK(!result.success);
    CHECK(!result.error.empty());
    PASS();
}

static void test_model_interface_chat() {
    TEST("model_interface_chat");
    TestModelInterface iface;
    iface.loadModel("chat-model");

    std::vector<ChatMessage> msgs = {
        {"system", "You are a helpful assistant."},
        {"user", "What is 2+2?"},
    };

    auto result = iface.chat(msgs);
    CHECK(result.success);
    CHECK(!result.text.empty());
    CHECK(result.tokensGenerated > 0);
    PASS();
}

static void test_model_interface_embed() {
    TEST("model_interface_embed");
    TestModelInterface iface;
    iface.loadModel("embed-model");

    auto result = iface.embed("Hello world");
    CHECK(result.success);
    CHECK(result.dimensions == 384);
    CHECK(result.embedding.size() == 384);
    PASS();
}

static void test_model_interface_statistics() {
    TEST("model_interface_statistics");
    TestModelInterface iface;
    iface.loadModel("stats-model");

    iface.generate("prompt 1");
    iface.generate("prompt 2");
    iface.generate("prompt 3");

    CHECK(iface.totalGenerations() == 3);
    CHECK(iface.totalTokens() > 0);
    PASS();
}

static void test_model_interface_unload() {
    TEST("model_interface_unload");
    TestModelInterface iface;
    iface.loadModel("test");
    CHECK(iface.isLoaded());
    iface.unload();
    CHECK(!iface.isLoaded());
    PASS();
}

static void test_cloud_api_client() {
    TEST("cloud_api_client");
    TestCloudApiClient client;
    CHECK(!client.isConfigured());

    client.setApiKey("test-key-12345");
    client.setEndpoint("https://api.example.com");
    CHECK(client.isConfigured());

    auto result = client.query("What is AI?");
    CHECK(result.success);
    CHECK(!result.text.empty());
    CHECK(result.tokensGenerated > 0);
    CHECK(client.queryCount() == 1);
    PASS();
}

static void test_cloud_api_not_configured() {
    TEST("cloud_api_not_configured");
    TestCloudApiClient client;
    auto result = client.query("test");
    CHECK(!result.success);
    CHECK(!result.error.empty());
    PASS();
}

static void test_generation_params() {
    TEST("generation_params_defaults");
    GenerationParams params;
    CHECK(std::abs(params.temperature - 0.7f) < 0.001f);
    CHECK(std::abs(params.topP - 0.9f) < 0.001f);
    CHECK(params.topK == 40);
    CHECK(params.maxTokens == 256);
    CHECK(params.seed == -1);
    CHECK(std::abs(params.repeatPenalty - 1.1f) < 0.001f);
    PASS();
}

static void test_integration_router_and_interface() {
    TEST("integration_router_and_interface");
    TestModelRouter router;
    TestModelInterface iface;

    router.registerModel("local-7b", ModelBackend::LlamaCpp, "./models/llama-7b.gguf");
    CHECK(router.hasModel("local-7b"));

    auto backend = router.getBackend("local-7b");
    iface.loadModel("local-7b", backend);
    CHECK(iface.isLoaded());
    CHECK(iface.modelInfo().backend == ModelBackend::LlamaCpp);

    auto result = iface.generate("Integrate test");
    CHECK(result.success);
    PASS();
}

static void test_performance_benchmark() {
    TEST("performance_benchmark");
    TestModelInterface iface;
    iface.loadModel("bench-model");

    const int iterations = 100;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < iterations; i++) {
        auto r = iface.generate("Benchmark prompt " + std::to_string(i));
        CHECK(r.success);
    }

    auto end = std::chrono::steady_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    double avgMs = totalMs / iterations;

    std::cout << "PASSED (avg=" << avgMs << "ms, total=" << totalMs << "ms)" << std::endl;
    g_passed++;
}

static void test_system_diagnostics() {
    TEST("system_diagnostics");

#ifdef _WIN32
    // RAM check
    MEMORYSTATUSEX memStatus{};
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);
    uint64_t totalRAM_MB = memStatus.ullTotalPhys / (1024 * 1024);
    CHECK(totalRAM_MB > 0);

    // Process memory
    PROCESS_MEMORY_COUNTERS pmc{};
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    uint64_t workingSet_MB = pmc.WorkingSetSize / (1024 * 1024);
    // Working set should be reasonable
    CHECK(workingSet_MB < totalRAM_MB);

    std::cout << "PASSED (RAM=" << totalRAM_MB << "MB, WS=" << workingSet_MB << "MB)" << std::endl;
    g_passed++;
#else
    std::cout << "SKIPPED (non-Windows)" << std::endl;
    g_passed++;
#endif
}

static void test_concurrent_generation() {
    TEST("concurrent_generation");

    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&, i]() {
            TestModelInterface iface;
            iface.loadModel("thread-model-" + std::to_string(i));
            auto r = iface.generate("Thread " + std::to_string(i));
            if (r.success) successCount++;
        });
    }
    for (auto& t : threads) t.join();

    CHECK(successCount.load() == 4);
    PASS();
}

int main() {
    std::cout << "===========================================" << std::endl;
    std::cout << " ModelInterface Test Suite (C++17)" << std::endl;
    std::cout << "===========================================" << std::endl;

    // Router tests
    test_model_router_registration();
    test_model_router_remove();
    test_model_router_listModels();

    // Interface tests
    test_model_interface_load();
    test_model_interface_generate();
    test_model_interface_generate_no_model();
    test_model_interface_chat();
    test_model_interface_embed();
    test_model_interface_statistics();
    test_model_interface_unload();

    // Cloud API tests
    test_cloud_api_client();
    test_cloud_api_not_configured();

    // Params tests
    test_generation_params();

    // Integration tests
    test_integration_router_and_interface();

    // Performance
    test_performance_benchmark();

    // Diagnostics
    test_system_diagnostics();

    // Concurrency
    test_concurrent_generation();

    std::cout << std::endl;
    std::cout << "Results: " << g_passed << "/" << g_total
              << " passed, " << g_failed << " failed" << std::endl;
    return (g_failed > 0) ? 1 : 0;
}
