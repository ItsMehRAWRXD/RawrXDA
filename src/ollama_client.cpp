#include "ollama_client.h"
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace RawrXD
{
namespace Backend
{

using json = nlohmann::json;

namespace
{
struct ParsedBaseUrl
{
    std::string host = "localhost";
    int port = 11434;
    std::string basePath;
    bool https = false;
};

static ParsedBaseUrl parseBaseUrl(const std::string& url)
{
    ParsedBaseUrl out;
    if (url.empty())
        return out;

    std::string u = url;
    size_t start = 0;
    if (u.rfind("https://", 0) == 0)
    {
        out.https = true;
        out.port = 443;
        start = 8;
    }
    else if (u.rfind("http://", 0) == 0)
    {
        out.https = false;
        out.port = 80;
        start = 7;
    }

    const size_t slash = u.find('/', start);
    const std::string hostPort = (slash == std::string::npos) ? u.substr(start) : u.substr(start, slash - start);
    out.basePath = (slash == std::string::npos) ? "" : u.substr(slash);

    const size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos && colon + 1 < hostPort.size())
    {
        out.host = hostPort.substr(0, colon);
        try
        {
            int parsed = std::stoi(hostPort.substr(colon + 1));
            if (parsed > 0 && parsed <= 65535)
                out.port = parsed;
        }
        catch (...)
        {
            // Keep defaults.
        }
    }
    else if (!hostPort.empty())
    {
        out.host = hostPort;
    }

    if (out.host.empty())
        out.host = "localhost";
    if (out.basePath == "/")
        out.basePath.clear();
    return out;
}

static std::string joinEndpointPath(const std::string& basePath, const std::string& endpoint)
{
    std::string ep = endpoint.empty() ? "/" : endpoint;
    if (ep.front() != '/')
        ep.insert(ep.begin(), '/');
    if (basePath.empty())
        return ep;
    if (basePath.back() == '/' && ep.front() == '/')
        return basePath.substr(0, basePath.size() - 1) + ep;
    if (basePath.back() != '/' && ep.front() != '/')
        return basePath + "/" + ep;
    return basePath + ep;
}
}  // namespace

// CURL callback for writing response data
size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
    userp->append((const char*)contents, size * nmemb);
    return size * nmemb;
}

// CURL callback for streaming response data
size_t curlStreamCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    auto* callback_data = static_cast<std::tuple<StreamCallback, ErrorCallback, CompletionCallback>*>(userp);
    auto& [on_chunk, on_error, on_complete] = *callback_data;

    std::string chunk((const char*)contents, size * nmemb);

    try
    {
        // Parse the JSON chunk
        json j = json::parse(chunk);

        if (j.contains("response"))
        {
            std::string response_chunk = j["response"];
            if (on_chunk)
            {
                on_chunk(response_chunk);
            }
        }

        if (j.contains("done") && j["done"].get<bool>())
        {
            OllamaResponse final_response;
            final_response.done = true;
            final_response.model = j.value("model", "");
            final_response.total_duration = j.value("total_duration", 0ULL);
            final_response.prompt_eval_count = j.value("prompt_eval_count", 0ULL);
            final_response.eval_count = j.value("eval_count", 0ULL);
            final_response.load_duration = j.value("load_duration", 0ULL);
            final_response.prompt_eval_duration = j.value("prompt_eval_duration", 0ULL);
            final_response.eval_duration = j.value("eval_duration", 0ULL);

            if (on_complete)
            {
                on_complete(final_response);
            }
        }
    }
    catch (const std::exception& e)
    {
        if (on_error)
        {
            on_error(std::string("JSON parse error: ") + e.what());
        }
    }

    return size * nmemb;
}

OllamaClient::OllamaClient(const std::string& base_url) : m_base_url(base_url) {}

OllamaClient::~OllamaClient() {}

void OllamaClient::setBaseUrl(const std::string& url)
{
    m_base_url = url;
}

bool OllamaClient::testConnection()
{
    std::string response = makeGetRequest("/api/tags");
    return !response.empty();
}

std::string OllamaClient::getVersion()
{
    std::string response = makeGetRequest("/api/version");
    if (response.empty())
        return "";

    try
    {
        json j = json::parse(response);
        return j.value("version", "");
    }
    catch (...)
    {
        return "";
    }
}

bool OllamaClient::isRunning()
{
    return testConnection();
}

std::vector<OllamaModel> OllamaClient::listModels()
{
    std::string response = makeGetRequest("/api/tags");
    if (response.empty())
        return {};

    return parseModels(response);
}

std::vector<OllamaModel> OllamaClient::filterModels(const std::vector<OllamaModel>& models,
                                                    std::function<bool(const OllamaModel&)> predicate) const
{

    std::vector<OllamaModel> filtered;
    for (const auto& model : models)
    {
        if (predicate(model))
        {
            filtered.push_back(model);
        }
    }
    return filtered;
}

const OllamaModel* OllamaClient::findModelById(const std::vector<OllamaModel>& models,
                                               const std::string& targetId) const
{

    for (const auto& model : models)
    {
        if (model.id == targetId)
        {
            return &model;
        }
    }
    return nullptr;
}

OllamaResponse OllamaClient::generateSync(const OllamaGenerateRequest& request)
{
    std::string json_body = createGenerateRequestJson(request);
    std::string response = makePostRequest("/api/generate", json_body);

    if (response.empty())
    {
        OllamaResponse res;
        res.error = true;
        res.error_message = "Empty response from server";
        return res;
    }

    return parseResponse(response);
}

OllamaResponse OllamaClient::chatSync(const OllamaChatRequest& request)
{
    std::string json_body = createChatRequestJson(request);
    std::string response = makePostRequest("/api/chat", json_body);

    if (response.empty())
    {
        OllamaResponse res;
        res.error = true;
        res.error_message = "Empty response from server";
        return res;
    }

    return parseResponse(response);
}

bool OllamaClient::generate(const OllamaGenerateRequest& request, StreamCallback on_chunk, ErrorCallback on_error,
                            CompletionCallback on_complete)
{
    std::string json_body = createGenerateRequestJson(request);
    return makeStreamingPostRequest("/api/generate", json_body, on_chunk, on_error, on_complete);
}

bool OllamaClient::chat(const OllamaChatRequest& request, StreamCallback on_chunk, ErrorCallback on_error,
                        CompletionCallback on_complete)
{
    std::string json_body = createChatRequestJson(request);
    return makeStreamingPostRequest("/api/chat", json_body, on_chunk, on_error, on_complete);
}

std::vector<float> OllamaClient::embeddings(const std::string& model, const std::string& prompt)
{
    json request_json = {{"model", model}, {"prompt", prompt}};

    std::string json_body = request_json.dump();
    std::string response = makePostRequest("/api/embeddings", json_body);

    if (response.empty())
        return {};

    try
    {
        json j = json::parse(response);
        if (j.contains("embedding"))
        {
            return j["embedding"].get<std::vector<float>>();
        }
    }
    catch (...)
    {
        return {};
    }

    return {};
}

std::string OllamaClient::createGenerateRequestJson(const OllamaGenerateRequest& req)
{
    json j = {{"model", req.model}, {"prompt", req.prompt}, {"stream", req.stream}};

    if (!req.options.empty())
    {
        json options = json::object();
        for (const auto& [key, value] : req.options)
        {
            options[key] = value;
        }
        j["options"] = options;
    }

    return j.dump();
}

std::string OllamaClient::createChatRequestJson(const OllamaChatRequest& req)
{
    json j = {{"model", req.model}, {"stream", req.stream}, {"messages", json::array()}};

    for (const auto& msg : req.messages)
    {
        j["messages"].push_back({{"role", msg.role}, {"content", msg.content}});
    }

    if (!req.options.empty())
    {
        json options = json::object();
        for (const auto& [key, value] : req.options)
        {
            options[key] = value;
        }
        j["options"] = options;
    }

    return j.dump();
}

OllamaResponse OllamaClient::parseResponse(const std::string& json_str)
{
    OllamaResponse response;

    try
    {
        json j = json::parse(json_str);

        response.model = j.value("model", "");
        response.response = j.value("response", "");
        response.done = j.value("done", false);

        if (j.contains("message"))
        {
            response.message.role = j["message"].value("role", "");
            response.message.content = j["message"].value("content", "");
        }

        response.total_duration = j.value("total_duration", 0ULL);
        response.prompt_eval_count = j.value("prompt_eval_count", 0ULL);
        response.eval_count = j.value("eval_count", 0ULL);
        response.load_duration = j.value("load_duration", 0ULL);
        response.prompt_eval_duration = j.value("prompt_eval_duration", 0ULL);
        response.eval_duration = j.value("eval_duration", 0ULL);
    }
    catch (const std::exception& e)
    {
        response.error = true;
        response.error_message = std::string("Parse error: ") + e.what();
    }

    return response;
}

std::vector<OllamaModel> OllamaClient::parseModels(const std::string& json_str)
{
    std::vector<OllamaModel> models;

    try
    {
        json j = json::parse(json_str);

        if (j.contains("models") && j["models"].is_array())
        {
            for (const auto& model_json : j["models"])
            {
                OllamaModel model;
                model.name = model_json.value("name", "");
                model.id = model.name;  // Use name as ID
                model.size = model_json.value("size", 0ULL);
                model.digest = model_json.value("digest", "");
                model.modified_at = model_json.value("modified_at", "");

                // Parse details if available
                if (model_json.contains("details"))
                {
                    auto details = model_json["details"];
                    model.format = details.value("format", "");
                    model.family = details.value("family", "");
                    model.parameter_size = details.value("parameter_size", "");
                    model.quantization_level = details.value("quantization_level", "");
                }

                if (!model.name.empty())
                {
                    models.push_back(model);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error parsing models: " << e.what() << std::endl;
    }

    return models;
}

std::string OllamaClient::makeGetRequest(const std::string& endpoint)
{
    const ParsedBaseUrl parsed = parseBaseUrl(m_base_url);
    const std::string requestPath = joinEndpointPath(parsed.basePath, endpoint);
    std::wstring whost(parsed.host.begin(), parsed.host.end());
    std::wstring wendpoint(requestPath.begin(), requestPath.end());

#ifdef _WIN32
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return {};

    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), static_cast<INTERNET_PORT>(parsed.port), 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return {};
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wendpoint.c_str(), NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, parsed.https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    std::string response;
    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok)
        ok = WinHttpReceiveResponse(hRequest, NULL);
    if (ok)
    {
        DWORD size = 0;
        do
        {
            size = 0;
            if (WinHttpQueryDataAvailable(hRequest, &size) && size > 0)
            {
                std::string chunk(size, '\0');
                DWORD downloaded = 0;
                if (WinHttpReadData(hRequest, chunk.data(), size, &downloaded) && downloaded > 0)
                {
                    chunk.resize(downloaded);
                    response += chunk;
                }
            }
        } while (size > 0);
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
#else
    return {};
#endif
}

std::string OllamaClient::makePostRequest(const std::string& endpoint, const std::string& json_body)
{
    const ParsedBaseUrl parsed = parseBaseUrl(m_base_url);
    const std::string requestPath = joinEndpointPath(parsed.basePath, endpoint);
    std::wstring whost(parsed.host.begin(), parsed.host.end());
    std::wstring wendpoint(requestPath.begin(), requestPath.end());

#ifdef _WIN32
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return {};

    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), static_cast<INTERNET_PORT>(parsed.port), 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return {};
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wendpoint.c_str(), NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, parsed.https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    std::string response;
    BOOL ok = WinHttpSendRequest(hRequest, headers.c_str(), -1, (LPVOID)json_body.data(), (DWORD)json_body.size(),
                                 (DWORD)json_body.size(), 0);
    if (ok)
        ok = WinHttpReceiveResponse(hRequest, NULL);
    if (ok)
    {
        DWORD size = 0;
        do
        {
            size = 0;
            if (WinHttpQueryDataAvailable(hRequest, &size) && size > 0)
            {
                std::string chunk(size, '\0');
                DWORD downloaded = 0;
                if (WinHttpReadData(hRequest, chunk.data(), size, &downloaded) && downloaded > 0)
                {
                    chunk.resize(downloaded);
                    response += chunk;
                }
            }
        } while (size > 0);
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
#else
    (void)json_body;
    return {};
#endif
}

bool OllamaClient::makeStreamingPostRequest(const std::string& endpoint, const std::string& json_body,
                                            StreamCallback on_chunk, ErrorCallback on_error,
                                            CompletionCallback on_complete)
{
    // HTTP fallback path: run synchronous POST and surface as a single chunk.
    const std::string response = makePostRequest(endpoint, json_body);
    if (response.empty())
    {
        if (on_error)
            on_error("HTTP backend unavailable or endpoint returned empty response.");
        return false;
    }
    if (on_chunk)
        on_chunk(response);
    if (on_complete)
    {
        OllamaResponse done;
        done.done = true;
        done.response = response;
        on_complete(done);
    }
    return true;
}

}  // namespace Backend
}  // namespace RawrXD
