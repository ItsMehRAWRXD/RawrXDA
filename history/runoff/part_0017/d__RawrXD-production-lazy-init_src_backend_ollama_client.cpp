#include "backend/ollama_client.h"
#include "utils/config.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
#endif

using json = nlohmann::json;

namespace RawrXD {
namespace Backend {

OllamaClient::OllamaClient(const std::string& base_url)
    : m_base_url(base_url), m_timeout_seconds(300) {
    // Allow environment overrides for configuration
    m_base_url = RawrXD::Utils::getenv_or("OLLAMA_BASE_URL", m_base_url);
    m_timeout_seconds = RawrXD::Utils::getenv_or_int("OLLAMA_TIMEOUT_SECONDS", m_timeout_seconds);

    // Structured logging: record client initialization
    RawrXD::Utils::log_info(LOGGER_COMPONENT, "Initialized OllamaClient", { {"base_url", m_base_url}, {"timeout_s", std::to_string(m_timeout_seconds)} });
}

OllamaClient::~OllamaClient() {
    RawrXD::Utils::log_info(LOGGER_COMPONENT, "Destroying OllamaClient");
}

void OllamaClient::setBaseUrl(const std::string& url) {
    m_base_url = url;
}

bool OllamaClient::testConnection() {
    try {
        std::string version = getVersion();
        return !version.empty();
    } catch (...) {
        return false;
    }
}

std::string OllamaClient::getVersion() {
    std::string response = makeGetRequest("/api/version");
    
    // Production-ready JSON parsing
    try {
        json j = json::parse(response);
        if (j.contains("version") && j["version"].is_string()) {
            return j["version"].get<std::string>();
        }
        return std::string();
    } catch (const std::exception& e) {
           RawrXD::Utils::log_error(LOGGER_COMPONENT, std::string("JSON parsing error in getVersion: ") + e.what());
        return "";
    }
}

bool OllamaClient::isRunning() {
    return testConnection();
}

std::vector<OllamaModel> OllamaClient::listModels() {
    RawrXD::Utils::log_debug(LOGGER_COMPONENT, "Requesting model list");
    std::string response = makeGetRequest("/api/tags");
    auto models = parseModels(response);
    RawrXD::Utils::log_info(LOGGER_COMPONENT, "Retrieved model list", {{"count", std::to_string(models.size())}});
    return models;
}

OllamaResponse OllamaClient::generateSync(const OllamaGenerateRequest& request) {
    OllamaGenerateRequest sync_req = request;
    sync_req.stream = false;
    
    std::string json = createGenerateRequestJson(sync_req);
    RawrXD::Utils::log_debug(LOGGER_COMPONENT, "generateSync request", {{"model", sync_req.model}});
    std::string response = makePostRequest("/api/generate", json);
    
    return parseResponse(response);
}

OllamaResponse OllamaClient::chatSync(const OllamaChatRequest& request) {
    OllamaChatRequest sync_req = request;
    sync_req.stream = false;
    
    std::string json = createChatRequestJson(sync_req);
    RawrXD::Utils::log_debug(LOGGER_COMPONENT, "chatSync request", {{"model", sync_req.model}});
    std::string response = makePostRequest("/api/chat", json);
    
    return parseResponse(response);
}

bool OllamaClient::generate(const OllamaGenerateRequest& request,
                           StreamCallback on_chunk,
                           ErrorCallback on_error,
                           CompletionCallback on_complete) {
    std::string json = createGenerateRequestJson(request);
    RawrXD::Utils::log_debug(LOGGER_COMPONENT, "Starting generate (stream)", {{"model", request.model}});
    return makeStreamingPostRequest("/api/generate", json, on_chunk, on_error, on_complete);
}

std::string OllamaClient::createGenerateRequestJson(const OllamaGenerateRequest& req) {
    json j;
    j["model"] = req.model;
    j["prompt"] = req.prompt;
    if (!req.system.empty()) j["system"] = req.system;
    if (!req.template_str.empty()) j["template"] = req.template_str;
    if (!req.context.empty()) j["context"] = req.context;
    j["stream"] = req.stream;
    if (!req.options.empty()) j["options"] = req.options;
    return j.dump();
}

std::string OllamaClient::createChatRequestJson(const OllamaChatRequest& req) {
    json j;
    j["model"] = req.model;
    j["messages"] = json::array_type();  // Create empty JSON array
    for (const auto& msg : req.messages) {
        json m;
        m["role"] = msg.role;
        m["content"] = msg.content;
        if (!msg.images.empty()) m["images"] = msg.images;
        j["messages"].push_back(m);
    }
    j["stream"] = req.stream;
    if (!req.options.empty()) j["options"] = req.options;
    return j.dump();
}

OllamaResponse OllamaClient::parseResponse(const std::string& json_str) {
    OllamaResponse resp;
    
    // Production-ready JSON parsing with nlohmann/json
    try {
        json j = json::parse(json_str);
        
        // Extract response fields with safe access
        if (j.contains("model") && j["model"].is_string()) resp.model = j["model"].get<std::string>();
        if (j.contains("response") && j["response"].is_string()) resp.response = j["response"].get<std::string>();
        if (j.contains("done") && j["done"].is_boolean()) resp.done = j["done"].get<bool>();
        
        // Extract performance metrics
        if (j.contains("total_duration") && j["total_duration"].is_number()) resp.total_duration = j["total_duration"].get<uint64_t>();
        if (j.contains("prompt_eval_count") && j["prompt_eval_count"].is_number()) resp.prompt_eval_count = j["prompt_eval_count"].get<uint64_t>();
        if (j.contains("eval_count") && j["eval_count"].is_number()) resp.eval_count = j["eval_count"].get<uint64_t>();
        
        // Extract optional fields
        if (j.contains("load_duration") && j["load_duration"].is_number()) resp.load_duration = j["load_duration"].get<uint64_t>();
        if (j.contains("prompt_eval_duration") && j["prompt_eval_duration"].is_number()) resp.prompt_eval_duration = j["prompt_eval_duration"].get<uint64_t>();
        if (j.contains("eval_duration") && j["eval_duration"].is_number()) resp.eval_duration = j["eval_duration"].get<uint64_t>();
        
    } catch (const std::exception& e) {
        // Log parsing error and return partial response
            RawrXD::Utils::log_error(LOGGER_COMPONENT, std::string("JSON parsing error in parseResponse: ") + e.what());
        // resp already initialized with defaults
    }
    
    return resp;
}

std::vector<OllamaModel> OllamaClient::parseModels(const std::string& json_str) {
    std::vector<OllamaModel> models;
    
    // Production-ready JSON parsing with nlohmann/json
    try {
        json j = json::parse(json_str);
        
        // Extract models array
        if (j.contains("models") && j["models"].is_array()) {
            const json& models_arr = j["models"];
            for (size_t i = 0; i < models_arr.size(); ++i) {
                const json& model_json = models_arr[i];
                OllamaModel model;
                
                // Extract model fields with safe access
                if (model_json.contains("name") && model_json["name"].is_string()) model.name = model_json["name"].get<std::string>();
                if (model_json.contains("modified_at") && model_json["modified_at"].is_string()) model.modified_at = model_json["modified_at"].get<std::string>();
                if (model_json.contains("size") && model_json["size"].is_number()) model.size = model_json["size"].get<uint64_t>();
                if (model_json.contains("digest") && model_json["digest"].is_string()) model.digest = model_json["digest"].get<std::string>();
                
                // Extract details if present
                if (model_json.contains("details")) {
                    const json& details_json = model_json["details"];
                    if (details_json.contains("format") && details_json["format"].is_string()) model.format = details_json["format"].get<std::string>();
                    if (details_json.contains("family") && details_json["family"].is_string()) model.family = details_json["family"].get<std::string>();
                    if (details_json.contains("parameter_size") && details_json["parameter_size"].is_string()) model.parameter_size = details_json["parameter_size"].get<std::string>();
                    if (details_json.contains("quantization_level") && details_json["quantization_level"].is_string()) model.quantization_level = details_json["quantization_level"].get<std::string>();
                }
                
                models.push_back(model);
            }
        }
        
    } catch (const std::exception& e) {
        // Log parsing error and return empty list
        RawrXD::Utils::log_error(LOGGER_COMPONENT, std::string("JSON parsing error in parseModels: ") + e.what(), {});
    }
    
    return models;
}

// Platform-specific HTTP implementations
#ifdef _WIN32
// RAII wrapper for WinHTTP handles
struct WinHttpHandle {
    HINTERNET h;
    WinHttpHandle(HINTERNET hh = NULL) : h(hh) {}
    ~WinHttpHandle() { if (h) WinHttpCloseHandle(h); }
    operator HINTERNET() const { return h; }
    HINTERNET release() { HINTERNET tmp = h; h = NULL; return tmp; }
};

static std::wstring widen(const std::string& s) {
    std::wstring ws;
    ws.assign(s.begin(), s.end());
    return ws;
}

// Parse base URL like "http://host:port"; returns host and port and secure flag
static void parse_base_url(const std::string& base, std::string& out_host, int& out_port, bool& out_secure) {
    std::string url = base;
    out_secure = false;
    // strip protocol
    if (url.rfind("http://", 0) == 0) { url = url.substr(7); out_secure = false; }
    else if (url.rfind("https://", 0) == 0) { url = url.substr(8); out_secure = true; }

    // cut at '/'
    auto slash = url.find('/');
    if (slash != std::string::npos) url = url.substr(0, slash);

    auto colon = url.find(':');
    if (colon != std::string::npos) {
        out_host = url.substr(0, colon);
        try { out_port = std::stoi(url.substr(colon + 1)); } catch (...) { out_port = (out_secure ? 443 : 11434); }
    } else {
        out_host = url;
        out_port = (out_secure ? 443 : 11434);
    }
}

std::string OllamaClient::makeGetRequest(const std::string& endpoint) {
    std::string host;
    int port = 11434;
    bool secure = false;
    parse_base_url(m_base_url, host, port, secure);

    WinHttpHandle hSession(WinHttpOpen(L"RawrXD/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession) {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, "WinHttpOpen failed");
        return "";
    }

    // Apply timeouts (ms)
    DWORD to_ms = static_cast<DWORD>(m_timeout_seconds * 1000);
    WinHttpSetTimeouts(hSession, to_ms, to_ms, to_ms, to_ms);

    WinHttpHandle hConnect(WinHttpConnect(hSession, widen(host).c_str(), port, 0));
    if (!hConnect) {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, "WinHttpConnect failed", {{"host", host}});
        return "";
    }

    std::wstring wendpoint = widen(endpoint);
    DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect, L"GET", wendpoint.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest) {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, "WinHttpOpenRequest failed (GET)");
        return "";
    }

    BOOL bResults = WinHttpSendRequest(hRequest,
                                       WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    std::string response;
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);

        if (bResults) {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;

            do {
                dwSize = 0;
                if (WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                    std::string buf;
                    buf.resize(dwSize);
                    if (WinHttpReadData(hRequest, (LPVOID)buf.data(), dwSize, &dwDownloaded)) {
                        response.append(buf.data(), dwDownloaded);
                    }
                }
            } while (dwSize > 0);
        }
    } else {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, "WinHttpSendRequest failed (GET)");
    }

    return response;
}

std::string OllamaClient::makePostRequest(const std::string& endpoint, const std::string& json_body) {
    std::string host;
    int port = 11434;
    bool secure = false;
    parse_base_url(m_base_url, host, port, secure);

    WinHttpHandle hSession(WinHttpOpen(L"RawrXD/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession) {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, "WinHttpOpen failed");
        return "";
    }

    DWORD to_ms = static_cast<DWORD>(m_timeout_seconds * 1000);
    WinHttpSetTimeouts(hSession, to_ms, to_ms, to_ms, to_ms);

    WinHttpHandle hConnect(WinHttpConnect(hSession, widen(host).c_str(), port, 0));
    if (!hConnect) {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, "WinHttpConnect failed", {{"host", host}});
        return "";
    }

    std::wstring wendpoint = widen(endpoint);
    DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect, L"POST", wendpoint.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest) {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, "WinHttpOpenRequest failed (POST)");
        return "";
    }

    std::wstring headers = L"Content-Type: application/json\r\n";

    BOOL bResults = WinHttpSendRequest(hRequest,
                                       headers.c_str(), -1,
                                       (LPVOID)json_body.c_str(), (DWORD)json_body.length(),
                                       (DWORD)json_body.length(), 0);

    std::string response;
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);

        if (bResults) {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;

            do {
                dwSize = 0;
                if (WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                    std::string buf;
                    buf.resize(dwSize);
                    if (WinHttpReadData(hRequest, (LPVOID)buf.data(), dwSize, &dwDownloaded)) {
                        response.append(buf.data(), dwDownloaded);
                    }
                }
            } while (dwSize > 0);
        }
    } else {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, "WinHttpSendRequest failed (POST)");
    }

    return response;
}

bool OllamaClient::makeStreamingPostRequest(const std::string& endpoint,
                                           const std::string& json_body,
                                           StreamCallback on_chunk,
                                           ErrorCallback on_error,
                                           CompletionCallback on_complete) {
    // Streaming implementation using WinHTTP (synchronous fallback with structured logs)
    RawrXD::Utils::log_debug(LOGGER_COMPONENT, "makeStreamingPostRequest (fallback sync)", {{"endpoint", endpoint}});
    OllamaResponse resp;
    try {
        resp.response = makePostRequest(endpoint, json_body);
        resp.done = true;
        if (on_chunk) on_chunk(resp);
        if (on_complete) on_complete(resp);
        RawrXD::Utils::log_info(LOGGER_COMPONENT, "Streaming post completed", {{"endpoint", endpoint}});
        return true;
    } catch (const std::exception& ex) {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, std::string("Streaming post failed: ") + ex.what(), {{"endpoint", endpoint}});
        if (on_error) on_error(ex.what());
        return false;
    }
}

#else
// Linux/Mac implementation uses libcurl

static size_t curl_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* s = static_cast<std::string*>(userdata);
    size_t bytes = size * nmemb;
    s->append(static_cast<char*>(ptr), bytes);
    return bytes;
}

std::string OllamaClient::makeGetRequest(const std::string& endpoint) {
    std::string url = m_base_url;
    if (!url.empty() && url.back() == '/' && !endpoint.empty() && endpoint.front() == '/') {
        url.pop_back();
    }
    url += endpoint;

    CURL* curl = curl_easy_init();
    if (!curl) {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, "curl init failed");
        return "";
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout_seconds);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, std::string("curl GET failed: ") + curl_easy_strerror(res), {{"url", url}});
    }

    curl_easy_cleanup(curl);
    return response;
}

std::string OllamaClient::makePostRequest(const std::string& endpoint, const std::string& json_body) {
    std::string url = m_base_url;
    if (!url.empty() && url.back() == '/' && !endpoint.empty() && endpoint.front() == '/') {
        url.pop_back();
    }
    url += endpoint;

    CURL* curl = curl_easy_init();
    if (!curl) {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, "curl init failed");
        return "";
    }

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)json_body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout_seconds);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        RawrXD::Utils::log_error(LOGGER_COMPONENT, std::string("curl POST failed: ") + curl_easy_strerror(res), {{"url", url}});
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}
#endif

} // namespace Backend
} // namespace RawrXD
