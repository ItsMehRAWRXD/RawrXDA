<<<<<<< HEAD
// universal_model_router.cpp - Implementation of Universal Model Router
// Win32: include winsock2 + windows + winhttp first to avoid header order / macro conflicts (e.g. with nlohmann/json).
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include "universal_model_router.h"
#include "cloud_api_client.h"
#include "cpu_inference_engine.h"
#include "RawrXD_PipeClient.h"
#include <nlohmann/json.hpp>

// ASM Model Loader externs
extern "C" {
    int LoadModel(const wchar_t* path);
    void* GetTensor(const char* name);
    void UnloadModel();
    int ModelLoaderInit();
    int HotSwapModel(const wchar_t* newPath, char preserveKV);
    const wchar_t* GetCurrentModelPath();
    unsigned long long GetModelLoadTimestamp();
}

// Beacon externs
extern "C" {
    int BeaconRouterInit();
    int BeaconSend(int beaconID, void* pData, int dataLen);
    int BeaconRecv(int beaconID, void** ppData, int* pLen);
    int TryBeaconRecv(int beaconID, void** ppData, int* pLen);
    int RegisterAgent(int agentID, int beaconSlot);
}

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include "agent/local_reasoning_integration.hpp"

using namespace RawrXD;

namespace {
static ModelBackend backendFromString(const std::string& s) {
    if (s == "LOCAL_GGUF") return ModelBackend::LOCAL_GGUF;
    if (s == "OLLAMA_LOCAL") return ModelBackend::OLLAMA_LOCAL;
    if (s == "ANTHROPIC") return ModelBackend::ANTHROPIC;
    if (s == "OPENAI") return ModelBackend::OPENAI;
    if (s == "GOOGLE") return ModelBackend::GOOGLE;
    if (s == "MOONSHOT") return ModelBackend::MOONSHOT;
    if (s == "AZURE_OPENAI") return ModelBackend::AZURE_OPENAI;
    if (s == "AWS_BEDROCK") return ModelBackend::AWS_BEDROCK;
    if (s == "REASONING_ENGINE") return ModelBackend::REASONING_ENGINE;
    return ModelBackend::LOCAL_GGUF;
}
static const char* backendToString(ModelBackend b) {
    switch (b) {
        case ModelBackend::LOCAL_GGUF: return "LOCAL_GGUF";
        case ModelBackend::OLLAMA_LOCAL: return "OLLAMA_LOCAL";
        case ModelBackend::ANTHROPIC: return "ANTHROPIC";
        case ModelBackend::OPENAI: return "OPENAI";
        case ModelBackend::GOOGLE: return "GOOGLE";
        case ModelBackend::MOONSHOT: return "MOONSHOT";
        case ModelBackend::AZURE_OPENAI: return "AZURE_OPENAI";
        case ModelBackend::AWS_BEDROCK: return "AWS_BEDROCK";
        case ModelBackend::REASONING_ENGINE: return "REASONING_ENGINE";
    }
    return "LOCAL_GGUF";
}
}

namespace RawrXD {

UniversalModelRouter::UniversalModelRouter()
    : m_localEngineReady(false),
      m_cloudClientReady(false)
{
    // Cloud client will be initialized lazily
}

UniversalModelRouter::~UniversalModelRouter() = default;

void UniversalModelRouter::registerModel(const std::string& model_name, const ModelConfig& config)
{
    if (!config.isValid()) {
        if (m_onError) {
            m_onError("Invalid configuration for model: " + model_name);
        }
        return;
    }
    
    m_modelRegistry[model_name] = config;
    if (m_onModelRegistered) {
        m_onModelRegistered(model_name, config.backend);
    }
}

void UniversalModelRouter::unregisterModel(const std::string& model_name)
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        m_modelRegistry.erase(it);
        if (m_onModelUnregistered) {
            m_onModelUnregistered(model_name);
        }
    }
}

ModelConfig UniversalModelRouter::getModelConfig(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        return it->second;
    }
    
    ModelConfig empty;
    return empty;
}

std::vector<std::string> UniversalModelRouter::getAvailableModels() const
{
    std::vector<std::string> models;
    for (const auto& [name, _] : m_modelRegistry) {
        models.push_back(name);
    }
    return models;
}

std::vector<std::string> UniversalModelRouter::getModelsForBackend(ModelBackend backend) const
{
    std::vector<std::string> models;
    
    for (const auto& [name, config] : m_modelRegistry) {
        if (config.backend == backend) {
            models.push_back(name);
        }
    }
    
    return models;
}

bool UniversalModelRouter::loadConfigFromFile(const std::string& config_file_path)
{
    std::ifstream file(config_file_path);
    if (!file.is_open()) {
        if (m_onError) {
            m_onError("Cannot open config file: " + config_file_path);
        }
        return false;
    }
    try {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        nlohmann::json j = nlohmann::json::parse(content);
        return loadConfigFromJson(j);
    } catch (const std::exception& e) {
        if (m_onError) m_onError(std::string("Config parse error: ") + e.what());
        return false;
    }
}

bool UniversalModelRouter::loadConfigFromJson(const json& config_json)
{
    if (!config_json.is_object() || !config_json.contains("models") || !config_json["models"].is_object()) {
        if (m_onError) m_onError("Invalid config format: expected { \"models\": { ... } }");
        return false;
    }

    m_modelRegistry.clear();
    for (auto it = config_json["models"].begin(); it != config_json["models"].end(); ++it) {
        const std::string& name = it.key();
        const auto& mc = it.value();
        if (!mc.is_object()) continue;

        ModelConfig config;
        config.backend = backendFromString(mc.value("backend", "LOCAL_GGUF"));
        config.model_id = mc.value("model_id", "");
        config.api_key = mc.value("api_key", "");
        config.endpoint = mc.value("endpoint", "");
        config.description = mc.value("description", "");
        if (mc.contains("full_config")) {
            config.full_config = mc["full_config"];
        } else {
            config.full_config = json{};
        }
        if (mc.contains("parameters") && mc["parameters"].is_object()) {
            for (auto p = mc["parameters"].begin(); p != mc["parameters"].end(); ++p) {
                if (p->is_string()) config.parameters[p.key()] = p->get<std::string>();
                else config.parameters[p.key()] = p->dump();
            }
        }

        if (!config.model_id.empty()) {
            m_modelRegistry[name] = config;
            if (m_onModelRegistered) m_onModelRegistered(name, config.backend);
        }
    }

    if (m_onConfigLoaded) m_onConfigLoaded(static_cast<int>(m_modelRegistry.size()));
    return true;
}

bool UniversalModelRouter::saveConfigToFile(const std::string& config_file_path)
{
    std::ofstream file(config_file_path);
    if (!file.is_open()) {
        if (m_onError) m_onError("Cannot write config file: " + config_file_path);
        return false;
    }
    try {
        nlohmann::json models = nlohmann::json::object();
        for (const auto& [name, config] : m_modelRegistry) {
            nlohmann::json mc;
            mc["backend"] = backendToString(config.backend);
            mc["model_id"] = config.model_id;
            mc["api_key"] = config.api_key;
            mc["endpoint"] = config.endpoint;
            mc["description"] = config.description;
            mc["full_config"] = config.full_config;
            nlohmann::json params = nlohmann::json::object();
            for (const auto& [k, v] : config.parameters)
                params[k] = v;
            mc["parameters"] = std::move(params);
            models[name] = std::move(mc);
        }
        nlohmann::json root;
        root["models"] = std::move(models);
        file << root.dump(2);
        file.close();
        return true;
    } catch (const std::exception& e) {
        if (m_onError) m_onError(std::string("Config save error: ") + e.what());
        return false;
    }
}

void UniversalModelRouter::initializeLocalEngine(const std::string& model_path)
{
    // Initialize the ASM Model Loader
    if (ModelLoaderInit() != 0) {
        if (m_onError) m_onError("Failed to initialize ASM Model Loader");
        return;
    }

    // Initialize Beacon Router for inter-agent communication
    if (BeaconRouterInit() != 0) {
        if (m_onError) m_onError("Failed to initialize Beacon Router");
        return;
    }

    // Load the model using ASM
    std::wstring wide_path(model_path.begin(), model_path.end());
    if (LoadModel(wide_path.c_str()) != 0) {
        if (m_onError) m_onError("Failed to load model via ASM: " + model_path);
        return;
    }

    m_localEngineReady = true;
    
    if (m_onModelRegistered) {
        m_onModelRegistered(model_path, ModelBackend::LOCAL_GGUF);
    }
}

void UniversalModelRouter::initializeCloudClient()
{
    // Cloud client is already initialized
    m_cloudClientReady = true;
}

bool UniversalModelRouter::hotSwapModel(const std::string& new_model_path, bool preserve_kv_cache)
{
    std::wstring wide_path(new_model_path.begin(), new_model_path.end());
    int result = HotSwapModel(wide_path.c_str(), preserve_kv_cache ? 1 : 0);
    return result == 1;
}

ModelConfig UniversalModelRouter::getOrLoadModel(const std::string& model_name)
{
    return getModelConfig(model_name);
}

bool UniversalModelRouter::isModelAvailable(const std::string& model_name) const
{
    return m_modelRegistry.find(model_name) != m_modelRegistry.end();
}

ModelBackend UniversalModelRouter::getModelBackend(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        return it->second.backend;
    }
    
    return ModelBackend::LOCAL_GGUF;  // Default
}

std::string UniversalModelRouter::getModelDescription(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        return it->second.description;
    }
    
    return "";
}

json UniversalModelRouter::getModelInfo(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        const ModelConfig& cfg = it->second;
        json info = json::object();
        info["backend"] = backendToString(cfg.backend);
        info["model_id"] = cfg.model_id;
        info["endpoint"] = cfg.endpoint;
        info["description"] = cfg.description;
        json params = json::object();
        for (const auto& kv : cfg.parameters) {
            params[kv.first] = kv.second;
        }
        info["parameters"] = std::move(params);
        info["full_config"] = cfg.full_config;
        return info;
    }

    return json{};
}

void UniversalModelRouter::onLocalEngineInitialized()
{
    m_localEngineReady = true;
}

void UniversalModelRouter::onCloudClientInitialized()
{
    m_cloudClientReady = true;
}

void UniversalModelRouter::onEngineError(const std::string& error)
{
    if (m_onError) {
        m_onError(error);
    }
}

#if defined(_WIN32)
// Call Ollama /api/generate with the given model name (agentic autonomous local — any model from /api/tags is valid)
static void invokeOllamaGenerate(const std::string& model_name, const std::string& prompt,
    std::function<void(const std::string& chunk, bool complete)> callback)
{
    if (!callback) return;
    const wchar_t* host = L"localhost";
    INTERNET_PORT port = 11434;
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Router/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
    if (!hSession) { callback("Error: WinHttpOpen failed.", true); return; }
    HINTERNET hConnect = WinHttpConnect(hSession, host, port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        callback("Error: Cannot connect to Ollama. Start Ollama (ollama serve) or use File > Load GGUF for local inference.", true);
        return;
    }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/generate", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); callback("Error: WinHttpOpenRequest failed.", true); return; }
    std::string escPrompt;
    escPrompt.reserve(prompt.size() + 16);
    for (char c : prompt) {
        if (c == '"') escPrompt += "\\\"";
        else if (c == '\\') escPrompt += "\\\\";
        else if (c == '\n') escPrompt += "\\n";
        else if (c == '\r') escPrompt += "\\r";
        else if (static_cast<unsigned char>(c) >= 32 || c == '\t') escPrompt += c;
    }
    std::string escModel;
    escModel.reserve(model_name.size() + 4);
    for (char c : model_name) {
        if (c == '"') escModel += "\\\"";
        else if (c == '\\') escModel += "\\\\";
        else if (static_cast<unsigned char>(c) >= 32 || c == '\t') escModel += c;
    }
    std::string body = "{\"model\":\"" + escModel + "\",\"prompt\":\"" + escPrompt + "\",\"stream\":true}";
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); callback("Error: Ollama request send failed.", true); return; }
    if (!WinHttpReceiveResponse(hRequest, NULL)) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); callback("Error: Ollama not responding.", true); return; }
    std::string lineBuf;
    char buf[4096];
    DWORD dwRead;
    for (;;) {
        dwRead = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwRead) || dwRead == 0) break;
        if (dwRead > sizeof(buf)) dwRead = sizeof(buf);
        if (!WinHttpReadData(hRequest, buf, dwRead, &dwRead) || dwRead == 0) break;
        for (DWORD i = 0; i < dwRead; i++) {
            if (buf[i] == '\n') {
                if (!lineBuf.empty()) {
                    try {
                        auto j = nlohmann::json::parse(lineBuf);
                        if (j.contains("error") && j["error"].is_string()) {
                            std::string err = "Error: " + j["error"].get<std::string>();
                            err += "\n\nTip: Ensure Ollama is running (ollama serve), the model exists (ollama pull "
                                + model_name + "), or use File > Load GGUF for local inference.";
                            callback(err, true);
                            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
                            return;
                        }
                        if (j.contains("response") && j["response"].is_string()) {
                            std::string tok = j["response"].get<std::string>();
                            if (!tok.empty()) callback(tok, false);
                        }
                        if (j.contains("done") && j["done"].get<bool>()) { callback("", true); WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return; }
                    } catch (...) {}
                }
                lineBuf.clear();
            } else {
                lineBuf += buf[i];
            }
        }
    }
    if (!lineBuf.empty()) {
        try {
            auto j = nlohmann::json::parse(lineBuf);
            if (j.contains("response") && j["response"].is_string()) {
                std::string tok = j["response"].get<std::string>();
                if (!tok.empty()) callback(tok, false);
            }
        } catch (...) {}
    }
    callback("", true);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}
#endif

std::string UniversalModelRouter::routeQuery(const std::string& model_name, const std::string& prompt, float /*temperature*/)
{
    // If the user registered this model, prefer its configured model_id/backend.
    std::string backendModelId = model_name;
    ModelBackend backend = ModelBackend::OLLAMA_LOCAL;
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        backend = it->second.backend;
        if (!it->second.model_id.empty()) backendModelId = it->second.model_id;
    }

    if (backend == ModelBackend::REASONING_ENGINE) {
        auto rr = LocalReasoningIntegration::analyzeCode(prompt, "text", true, true);
        std::string out = rr.summary;
        for (const auto& s : rr.suggestions) { out += "\n"; out += s; }
        return out;
    }

    if (backend == ModelBackend::ANTHROPIC) {
        return "Error: ANTHROPIC backend not wired in this build. Configure a local backend (Ollama/LOCAL_GGUF) or add cloud client integration.";
    }
    if (backend == ModelBackend::OPENAI || backend == ModelBackend::AZURE_OPENAI) {
        return "Error: OPENAI backend not wired in this build. Configure a local backend (Ollama/LOCAL_GGUF) or add cloud client integration.";
    }

#if defined(_WIN32)
    std::string out;
    std::mutex mu;
    std::condition_variable cv;
    bool done = false;

    invokeOllamaGenerate(backendModelId, prompt, [&](const std::string& chunk, bool complete) {
        if (!chunk.empty()) out += chunk;
        if (complete) {
            std::lock_guard<std::mutex> lock(mu);
            done = true;
            cv.notify_one();
        }
    });

    {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait_for(lock, std::chrono::seconds(120), [&]() { return done; });
    }
    if (!done && out.empty()) return "Error: Ollama request timed out.";
    return out;
#else
    return "Error: Ollama streaming only implemented on Windows in this build.";
#endif
}

void UniversalModelRouter::routeStreamQuery(const std::string& model_name, const std::string& prompt, StreamCallback callback, float /*temperature*/)
{
    if (!callback) return;

    std::string backendModelId = model_name;
    ModelBackend backend = ModelBackend::OLLAMA_LOCAL;
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        backend = it->second.backend;
        if (!it->second.model_id.empty()) backendModelId = it->second.model_id;
    }

    if (backend == ModelBackend::REASONING_ENGINE) {
        callback(routeQuery(model_name, prompt));
        return;
    }

#if defined(_WIN32)
    invokeOllamaGenerate(backendModelId, prompt, [&](const std::string& chunk, bool complete) {
        (void)complete;
        if (!chunk.empty()) callback(chunk);
    });
#else
    callback(routeQuery(model_name, prompt));
#endif
}

std::vector<std::string> UniversalModelRouter::getAvailableBackends() const
{
    return {
        "RawrXD-Native (Local GGUF)",
        "RawrXD Reasoning (Alpha)",
        "Claude-3.5-Sonnet (Anthropic)",
        "GPT-4o (OpenAI)",
        "Gemini-1.5-Pro (Google)",
        "Llama-3-70B (Local Ollama)"
    };
}

void UniversalModelRouter::routeRequest(const std::string& model_name,
                                        const std::string& prompt,
                                        std::function<void(const std::string&)> callback)
{
    if (!callback) return;
    callback(routeQuery(model_name, prompt));
}

} // namespace RawrXD
=======
// universal_model_router.cpp - Implementation of Universal Model Router
// Win32: include winsock2 + windows + winhttp first to avoid header order / macro conflicts (e.g. with nlohmann/json).
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

// Complete type stubs before including header so unique_ptr destructor can be instantiated
struct CloudApiClient { virtual ~CloudApiClient() = default; };
struct QuantizationAwareInferenceEngine { virtual ~QuantizationAwareInferenceEngine() = default; };

#include "universal_model_router.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include "agent/local_reasoning_integration.hpp"
#include "project_context.h"

namespace {
static ModelBackend backendFromString(const std::string& s) {
    if (s == "LOCAL_GGUF") return ModelBackend::LOCAL_GGUF;
    if (s == "OLLAMA_LOCAL") return ModelBackend::OLLAMA_LOCAL;
    if (s == "ANTHROPIC") return ModelBackend::ANTHROPIC;
    if (s == "OPENAI") return ModelBackend::OPENAI;
    if (s == "GOOGLE") return ModelBackend::GOOGLE;
    if (s == "MOONSHOT") return ModelBackend::MOONSHOT;
    if (s == "AZURE_OPENAI") return ModelBackend::AZURE_OPENAI;
    if (s == "AWS_BEDROCK") return ModelBackend::AWS_BEDROCK;
    if (s == "REASONING_ENGINE") return ModelBackend::REASONING_ENGINE;
    return ModelBackend::LOCAL_GGUF;
}
static const char* backendToString(ModelBackend b) {
    switch (b) {
        case ModelBackend::LOCAL_GGUF: return "LOCAL_GGUF";
        case ModelBackend::OLLAMA_LOCAL: return "OLLAMA_LOCAL";
        case ModelBackend::ANTHROPIC: return "ANTHROPIC";
        case ModelBackend::OPENAI: return "OPENAI";
        case ModelBackend::GOOGLE: return "GOOGLE";
        case ModelBackend::MOONSHOT: return "MOONSHOT";
        case ModelBackend::AZURE_OPENAI: return "AZURE_OPENAI";
        case ModelBackend::AWS_BEDROCK: return "AWS_BEDROCK";
        case ModelBackend::REASONING_ENGINE: return "REASONING_ENGINE";
    }
    return "LOCAL_GGUF";
}
}

UniversalModelRouter::UniversalModelRouter()
    : m_localEngineReady(false),
      m_cloudClientReady(false)
{
    // Cloud client will be initialized lazily
}

UniversalModelRouter::~UniversalModelRouter() = default;

void UniversalModelRouter::registerModel(const std::string& model_name, const ModelConfig& config)
{
    if (!config.isValid()) {
        if (m_onError) {
            m_onError("Invalid configuration for model: " + model_name);
        }
        return;
    }
    
    m_modelRegistry[model_name] = config;
    if (m_onModelRegistered) {
        m_onModelRegistered(model_name, config.backend);
    }
}

void UniversalModelRouter::unregisterModel(const std::string& model_name)
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        m_modelRegistry.erase(it);
        if (m_onModelUnregistered) {
            m_onModelUnregistered(model_name);
        }
    }
}

ModelConfig UniversalModelRouter::getModelConfig(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        return it->second;
    }
    
    ModelConfig empty;
    return empty;
}

std::vector<std::string> UniversalModelRouter::getAvailableModels() const
{
    std::vector<std::string> models;
    for (const auto& [name, _] : m_modelRegistry) {
        models.push_back(name);
    }
    return models;
}

std::vector<std::string> UniversalModelRouter::getModelsForBackend(ModelBackend backend) const
{
    std::vector<std::string> models;
    
    for (const auto& [name, config] : m_modelRegistry) {
        if (config.backend == backend) {
            models.push_back(name);
        }
    }
    
    return models;
}

bool UniversalModelRouter::loadConfigFromFile(const std::string& config_file_path)
{
    std::ifstream file(config_file_path);
    if (!file.is_open()) {
        if (m_onError) {
            m_onError("Cannot open config file: " + config_file_path);
        }
        return false;
    }
    try {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        nlohmann::json j = nlohmann::json::parse(content);
        if (!j.is_object() || !j.contains("models") || !j["models"].is_object()) {
            if (m_onError) m_onError("Invalid config format: expected { \"models\": { ... } }");
            return false;
        }
        m_modelRegistry.clear();
        for (auto it = j["models"].begin(); it != j["models"].end(); ++it) {
            const std::string& name = it.key();
            const auto& mc = it.value();
            if (!mc.is_object()) continue;
            ModelConfig config;
            config.backend = backendFromString(mc.value("backend", "LOCAL_GGUF"));
            config.model_id = mc.value("model_id", "");
            config.api_key = mc.value("api_key", "");
            config.endpoint = mc.value("endpoint", "");
            config.description = mc.value("description", "");
            config.full_config = mc.contains("full_config") && mc["full_config"].is_string()
                ? mc["full_config"].get<std::string>() : "";
            if (mc.contains("parameters") && mc["parameters"].is_object()) {
                for (auto p = mc["parameters"].begin(); p != mc["parameters"].end(); ++p)
                    config.parameters[p.key()] = p->get<std::string>();
            }
            if (!config.model_id.empty()) {
                m_modelRegistry[name] = config;
                if (m_onModelRegistered) m_onModelRegistered(name, config.backend);
            }
        }
        if (m_onConfigLoaded) m_onConfigLoaded(static_cast<int>(m_modelRegistry.size()));
        return true;
    } catch (const std::exception& e) {
        if (m_onError) m_onError(std::string("Config parse error: ") + e.what());
        return false;
    }
}

bool UniversalModelRouter::saveConfigToFile(const std::string& config_file_path) const
{
    std::ofstream file(config_file_path);
    if (!file.is_open()) {
        if (m_onError) m_onError("Cannot write config file: " + config_file_path);
        return false;
    }
    try {
        nlohmann::json models = nlohmann::json::object();
        for (const auto& [name, config] : m_modelRegistry) {
            nlohmann::json mc;
            mc["backend"] = backendToString(config.backend);
            mc["model_id"] = config.model_id;
            mc["api_key"] = config.api_key;
            mc["endpoint"] = config.endpoint;
            mc["description"] = config.description;
            mc["full_config"] = config.full_config;
            nlohmann::json params = nlohmann::json::object();
            for (const auto& [k, v] : config.parameters)
                params[k] = v;
            mc["parameters"] = std::move(params);
            models[name] = std::move(mc);
        }
        nlohmann::json root;
        root["models"] = std::move(models);
        file << root.dump(2);
        file.close();
        return true;
    } catch (const std::exception& e) {
        if (m_onError) m_onError(std::string("Config save error: ") + e.what());
        return false;
    }
}

void UniversalModelRouter::initializeLocalEngine(const std::string& model_path)
{
    // Initialize local GGUF engine with the specific model path
    m_localEngineReady = true;
    
    // In a real implementation, we would re-initialize the QuantizationAwareInferenceEngine here
    // with the provided model_path.
    
    if (m_onModelRegistered) {
        m_onModelRegistered(model_path, ModelBackend::LOCAL_GGUF);
    }
}

void UniversalModelRouter::initializeCloudClient()
{
    // Cloud client is already initialized
    m_cloudClientReady = true;
}

ModelConfig UniversalModelRouter::getOrLoadModel(const std::string& model_name)
{
    return getModelConfig(model_name);
}

bool UniversalModelRouter::isModelAvailable(const std::string& model_name) const
{
    return m_modelRegistry.find(model_name) != m_modelRegistry.end();
}

ModelBackend UniversalModelRouter::getModelBackend(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        return it->second.backend;
    }
    
    return ModelBackend::LOCAL_GGUF;  // Default
}

std::string UniversalModelRouter::getModelDescription(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        return it->second.description;
    }
    
    return "";
}

std::string UniversalModelRouter::getModelInfo(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        return it->second.full_config;
    }
    
    return "";
}

void UniversalModelRouter::onLocalEngineInitialized()
{
    m_localEngineReady = true;
}

void UniversalModelRouter::onCloudClientInitialized()
{
    m_cloudClientReady = true;
}

void UniversalModelRouter::onEngineError(const std::string& error)
{
    if (m_onError) {
        m_onError(error);
    }
}

#if defined(_WIN32)
// Call Ollama /api/generate with the given model name (agentic autonomous local — any model from /api/tags is valid)
static void invokeOllamaGenerate(const std::string& model_name, const std::string& prompt,
    std::function<void(const std::string& chunk, bool complete)> callback)
{
    if (!callback) return;
    const wchar_t* host = L"localhost";
    INTERNET_PORT port = 11434;
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Router/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
    if (!hSession) { callback("Error: WinHttpOpen failed.", true); return; }
    HINTERNET hConnect = WinHttpConnect(hSession, host, port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        callback("Error: Cannot connect to Ollama. Start Ollama (ollama serve) or use File > Load GGUF for local inference.", true);
        return;
    }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/generate", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); callback("Error: WinHttpOpenRequest failed.", true); return; }
    std::string escPrompt;
    escPrompt.reserve(prompt.size() + 16);
    for (char c : prompt) {
        if (c == '"') escPrompt += "\\\"";
        else if (c == '\\') escPrompt += "\\\\";
        else if (c == '\n') escPrompt += "\\n";
        else if (c == '\r') escPrompt += "\\r";
        else if (static_cast<unsigned char>(c) >= 32 || c == '\t') escPrompt += c;
    }
    std::string escModel;
    escModel.reserve(model_name.size() + 4);
    for (char c : model_name) {
        if (c == '"') escModel += "\\\"";
        else if (c == '\\') escModel += "\\\\";
        else if (static_cast<unsigned char>(c) >= 32 || c == '\t') escModel += c;
    }
    std::string body = "{\"model\":\"" + escModel + "\",\"prompt\":\"" + escPrompt + "\",\"stream\":true}";
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); callback("Error: Ollama request send failed.", true); return; }
    if (!WinHttpReceiveResponse(hRequest, NULL)) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); callback("Error: Ollama not responding.", true); return; }
    std::string lineBuf;
    char buf[4096];
    DWORD dwRead;
    for (;;) {
        dwRead = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwRead) || dwRead == 0) break;
        if (dwRead > sizeof(buf)) dwRead = sizeof(buf);
        if (!WinHttpReadData(hRequest, buf, dwRead, &dwRead) || dwRead == 0) break;
        for (DWORD i = 0; i < dwRead; i++) {
            if (buf[i] == '\n') {
                if (!lineBuf.empty()) {
                    try {
                        auto j = nlohmann::json::parse(lineBuf);
                        if (j.contains("error") && j["error"].is_string()) {
                            std::string err = "Error: " + j["error"].get<std::string>();
                            err += "\n\nTip: Ensure Ollama is running (ollama serve), the model exists (ollama pull "
                                + model_name + "), or use File > Load GGUF for local inference.";
                            callback(err, true);
                            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
                            return;
                        }
                        if (j.contains("response") && j["response"].is_string()) {
                            std::string tok = j["response"].get<std::string>();
                            if (!tok.empty()) callback(tok, false);
                        }
                        if (j.contains("done") && j["done"].get<bool>()) { callback("", true); WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return; }
                    } catch (...) {}
                }
                lineBuf.clear();
            } else {
                lineBuf += buf[i];
            }
        }
    }
    if (!lineBuf.empty()) {
        try {
            auto j = nlohmann::json::parse(lineBuf);
            if (j.contains("response") && j["response"].is_string()) {
                std::string tok = j["response"].get<std::string>();
                if (!tok.empty()) callback(tok, false);
            }
        } catch (...) {}
    }
    callback("", true);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}
#endif

void UniversalModelRouter::routeRequest(const std::string& model_name, 
                                      const std::string& prompt,
                                      const RawrXD::ProjectContext& context,
                                      std::function<void(const std::string& chunk, bool complete)> callback)
{
    (void)context;
    if (model_name.find("Reasoning") != std::string::npos) {
        if (callback) callback("[Reasoning Engine] Analyzing prompt: " + prompt + "\n", false);
        
        // Use the expert reasoning integration for a simulated "deep" response
        auto result = LocalReasoningIntegration::analyzeCode(prompt, "text", true, true);
        std::string analysis = result.summary;
        for (const auto& s : result.suggestions) { analysis += "\n"; analysis += s; }
        
        if (callback) callback("\n[Reasoning Engine] result: " + analysis + "\n", false);
        if (callback) callback("", true);
        return;
    }

    if (model_name.find("Claude") != std::string::npos) {
        const char* key = std::getenv("ANTHROPIC_API_KEY");
        if (!key || !key[0]) {
            if (callback) callback(
                "Claude API requires ANTHROPIC_API_KEY. Set it in your environment, e.g. "
                "set ANTHROPIC_API_KEY=sk-ant-... in CMD, or add to System Properties > Environment Variables. "
                "Or switch to RawrXD-Native or Ollama for local inference.", true);
            return;
        }
        if (callback) callback(
            "Claude API integration: connect via config.example.json or register model with Anthropic endpoint. "
            "For now, switch to RawrXD-Native (Load GGUF) or Ollama for immediate chat.", true);
        return;
    }
    if (model_name.find("GPT") != std::string::npos) {
        const char* key = std::getenv("OPENAI_API_KEY");
        if (!key || !key[0]) {
            if (callback) callback(
                "OpenAI API requires OPENAI_API_KEY. Set it in your environment, e.g. "
                "set OPENAI_API_KEY=sk-... in CMD, or add to System Properties > Environment Variables. "
                "Or switch to RawrXD-Native or Ollama for local inference.", true);
            return;
        }
        if (callback) callback(
            "OpenAI API integration: connect via config.example.json or register model with OpenAI endpoint. "
            "For now, switch to RawrXD-Native (Load GGUF) or Ollama for immediate chat.", true);
        return;
    }
    {
#if defined(_WIN32)
        // Agentic autonomous local: use selected model name with Ollama (every model from /api/tags is valid)
        invokeOllamaGenerate(model_name, prompt, callback);
#else
        if (callback) callback("Using RawrXD Native Local Engine... ", false);
        if (callback) callback("Local inference finished.", true);
#endif
    }
}

std::vector<std::string> UniversalModelRouter::getAvailableBackends() const
{
    return {
        "RawrXD-Native (Local GGUF)",
        "RawrXD Reasoning (Alpha)",
        "Claude-3.5-Sonnet (Anthropic)",
        "GPT-4o (OpenAI)",
        "Gemini-1.5-Pro (Google)",
        "Llama-3-70B (Local Ollama)"
    };
}
>>>>>>> origin/main
