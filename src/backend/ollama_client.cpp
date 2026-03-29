#include "backend/ollama_client.h"
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
#endif

using json = nlohmann::json;

namespace RawrXD
{
namespace Backend
{

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
            // Keep default port.
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

OllamaClient::OllamaClient(const std::string& base_url) : m_base_url(base_url), m_timeout_seconds(300) {}

OllamaClient::~OllamaClient() {}

void OllamaClient::setBaseUrl(const std::string& url)
{
    m_base_url = url;
}

bool OllamaClient::testConnection()
{
    try
    {
        std::string version = getVersion();
        return !version.empty();
    }
    catch (...)
    {
        return false;
    }
}

std::string OllamaClient::getVersion()
{
    std::string response = makeGetRequest("/api/version");

    // Production-ready JSON parsing
    try
    {
        json j = json::parse(response);
        return j.value("version", std::string());
    }
    catch (const json::exception& e)
    {
        std::cerr << "JSON parsing error in getVersion: " << e.what() << std::endl;
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
    return parseModels(response);
}

OllamaResponse OllamaClient::generateSync(const OllamaGenerateRequest& request)
{
    OllamaGenerateRequest sync_req = request;
    sync_req.stream = false;

    std::string json = createGenerateRequestJson(sync_req);
    std::string response = makePostRequest("/api/generate", json);

    return parseResponse(response);
}

OllamaResponse OllamaClient::chatSync(const OllamaChatRequest& request)
{
    OllamaChatRequest sync_req = request;
    sync_req.stream = false;

    std::string json = createChatRequestJson(sync_req);
    std::string response = makePostRequest("/api/chat", json);

    return parseResponse(response);
}

bool OllamaClient::generate(const OllamaGenerateRequest& request, StreamCallback on_chunk, ErrorCallback on_error,
                            CompletionCallback on_complete)
{
    std::string json = createGenerateRequestJson(request);
    return makeStreamingPostRequest("/api/generate", json, on_chunk, on_error, on_complete);
}

bool OllamaClient::chat(const OllamaChatRequest& request, StreamCallback on_chunk, ErrorCallback on_error,
                        CompletionCallback on_complete)
{
    std::string json = createChatRequestJson(request);
    return makeStreamingPostRequest("/api/chat", json, on_chunk, on_error, on_complete);
}

std::vector<float> OllamaClient::embeddings(const std::string& model, const std::string& prompt)
{
    // Create JSON: {"model": "...", "prompt": "..."}
    std::ostringstream oss;
    oss << "{\"model\":\"" << model << "\",\"prompt\":\"" << prompt << "\"}";

    std::string response = makePostRequest("/api/embeddings", oss.str());

    // Parse embeddings array from JSON
    std::vector<float> result;
    // Simple parser for array of numbers
    size_t pos = response.find("[");
    if (pos != std::string::npos)
    {
        size_t end = response.find("]", pos);
        std::string array_str = response.substr(pos + 1, end - pos - 1);

        std::istringstream iss(array_str);
        std::string token;
        while (std::getline(iss, token, ','))
        {
            try
            {
                result.push_back(std::stof(token));
            }
            catch (...)
            {
            }
        }
    }

    return result;
}

// JSON creation helpers
std::string OllamaClient::createGenerateRequestJson(const OllamaGenerateRequest& req)
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"model\":\"" << req.model << "\",";
    oss << "\"prompt\":\"" << req.prompt << "\",";
    oss << "\"stream\":" << (req.stream ? "true" : "false");

    if (!req.options.empty())
    {
        oss << ",\"options\":{";
        bool first = true;
        for (const auto& [key, value] : req.options)
        {
            if (!first)
                oss << ",";
            oss << "\"" << key << "\":" << value;
            first = false;
        }
        oss << "}";
    }

    oss << "}";
    return oss.str();
}

std::string OllamaClient::createChatRequestJson(const OllamaChatRequest& req)
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"model\":\"" << req.model << "\",";
    oss << "\"messages\":[";

    for (size_t i = 0; i < req.messages.size(); ++i)
    {
        if (i > 0)
            oss << ",";
        oss << "{\"role\":\"" << req.messages[i].role << "\",";
        oss << "\"content\":\"" << req.messages[i].content << "\"}";
    }

    oss << "],";
    oss << "\"stream\":" << (req.stream ? "true" : "false");
    oss << "}";
    return oss.str();
}

OllamaResponse OllamaClient::parseResponse(const std::string& json_str)
{
    OllamaResponse resp;

    // Production-ready JSON parsing with nlohmann/json
    try
    {
        json j = json::parse(json_str);

        // Extract response fields with safe access
        resp.model = j.value("model", std::string());
        resp.response = j.value("response", std::string());
        resp.done = j.value("done", false);

        // Extract performance metrics
        resp.total_duration = j.value("total_duration", uint64_t(0));
        resp.prompt_eval_count = j.value("prompt_eval_count", uint64_t(0));
        resp.eval_count = j.value("eval_count", uint64_t(0));

        // Extract optional fields
        if (j.contains("load_duration"))
        {
            resp.load_duration = j["load_duration"].get<uint64_t>();
        }
        if (j.contains("prompt_eval_duration"))
        {
            resp.prompt_eval_duration = j["prompt_eval_duration"].get<uint64_t>();
        }
        if (j.contains("eval_duration"))
        {
            resp.eval_duration = j["eval_duration"].get<uint64_t>();
        }
    }
    catch (const std::exception& e)
    {
        // Log parsing error and return partial response
        std::cerr << "JSON parsing error in parseResponse: " << e.what() << std::endl;
        // resp already initialized with defaults
    }

    return resp;
}

std::vector<OllamaModel> OllamaClient::parseModels(const std::string& json_str)
{
    std::vector<OllamaModel> models;

    // Production-ready JSON parsing with nlohmann/json
    try
    {
        json j = json::parse(json_str);

        // Extract models array
        if (j.contains("models") && j["models"].is_array())
        {
            for (const auto& model_json : j["models"])
            {
                OllamaModel model;

                // Extract model fields with safe access
                model.name = model_json.value("name", std::string());
                model.modified_at = model_json.value("modified_at", std::string());
                model.size = model_json.value("size", uint64_t(0));
                model.digest = model_json.value("digest", std::string());

                // Extract details if present
                if (model_json.contains("details"))
                {
                    const auto& details = model_json["details"];
                    model.format = details.value("format", std::string());
                    model.family = details.value("family", std::string());
                    model.parameter_size = details.value("parameter_size", std::string());
                    model.quantization_level = details.value("quantization_level", std::string());
                }

                models.push_back(model);
            }
        }
    }
    catch (const std::exception& e)
    {
        // Log parsing error and return empty list
        std::cerr << "JSON parsing error in parseModels: " << e.what() << std::endl;
    }

    return models;
}

// Platform-specific HTTP implementations
#ifdef _WIN32

std::string OllamaClient::makeGetRequest(const std::string& endpoint)
{
    const ParsedBaseUrl parsed = parseBaseUrl(m_base_url);
    const std::string requestPath = joinEndpointPath(parsed.basePath, endpoint);
    std::wstring whost(parsed.host.begin(), parsed.host.end());
    std::wstring wendpoint(requestPath.begin(), requestPath.end());

    // Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return "";

    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), static_cast<INTERNET_PORT>(parsed.port), 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wendpoint.c_str(), NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, parsed.https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    std::string response;
    if (bResults)
    {
        bResults = WinHttpReceiveResponse(hRequest, NULL);

        if (bResults)
        {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;

            do
            {
                dwSize = 0;
                if (WinHttpQueryDataAvailable(hRequest, &dwSize))
                {
                    char* pszOutBuffer = new char[dwSize + 1];
                    ZeroMemory(pszOutBuffer, dwSize + 1);

                    if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded))
                    {
                        response.append(pszOutBuffer, dwDownloaded);
                    }

                    delete[] pszOutBuffer;
                }
            } while (dwSize > 0);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

std::string OllamaClient::makePostRequest(const std::string& endpoint, const std::string& json_body)
{
    const ParsedBaseUrl parsed = parseBaseUrl(m_base_url);
    const std::string requestPath = joinEndpointPath(parsed.basePath, endpoint);
    std::wstring whost(parsed.host.begin(), parsed.host.end());
    std::wstring wendpoint(requestPath.begin(), requestPath.end());

    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return "";

    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), static_cast<INTERNET_PORT>(parsed.port), 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wendpoint.c_str(), NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, parsed.https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring headers = L"Content-Type: application/json\r\n";

    BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), -1, (LPVOID)json_body.c_str(), json_body.length(),
                                       json_body.length(), 0);

    std::string response;
    if (bResults)
    {
        bResults = WinHttpReceiveResponse(hRequest, NULL);

        if (bResults)
        {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;

            do
            {
                dwSize = 0;
                if (WinHttpQueryDataAvailable(hRequest, &dwSize))
                {
                    char* pszOutBuffer = new char[dwSize + 1];
                    ZeroMemory(pszOutBuffer, dwSize + 1);

                    if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded))
                    {
                        response.append(pszOutBuffer, dwDownloaded);
                    }

                    delete[] pszOutBuffer;
                }
            } while (dwSize > 0);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

bool OllamaClient::makeStreamingPostRequest(const std::string& endpoint, const std::string& json_body,
                                            StreamCallback on_chunk, ErrorCallback on_error,
                                            CompletionCallback on_complete)
{
    const ParsedBaseUrl parsed = parseBaseUrl(m_base_url);
    const std::string requestPath = joinEndpointPath(parsed.basePath, endpoint);
    std::wstring whost(parsed.host.begin(), parsed.host.end());
    std::wstring wendpoint(requestPath.begin(), requestPath.end());

    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
    {
        if (on_error)
            on_error("WinHttpOpen failed");
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), static_cast<INTERNET_PORT>(parsed.port), 0);
    if (!hConnect)
    {
        if (on_error)
            on_error("WinHttpConnect failed");
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wendpoint.c_str(), NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, parsed.https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest)
    {
        if (on_error)
            on_error("WinHttpOpenRequest failed");
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    BOOL sendOk = WinHttpSendRequest(hRequest, headers.c_str(), -1, (LPVOID)json_body.c_str(),
                                     static_cast<DWORD>(json_body.size()), static_cast<DWORD>(json_body.size()), 0);
    if (!sendOk)
    {
        if (on_error)
            on_error("WinHttpSendRequest failed");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL))
    {
        if (on_error)
            on_error("WinHttpReceiveResponse failed");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX))
    {
        if (statusCode >= 400)
        {
            if (on_error)
                on_error("HTTP error status: " + std::to_string(statusCode));
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
    }

    std::string pending;
    OllamaResponse finalResp;

    DWORD available = 0;
    while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0)
    {
        std::vector<char> buffer(available + 1, 0);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, buffer.data(), available, &read) || read == 0)
            break;

        pending.append(buffer.data(), read);

        size_t nl = pending.find('\n');
        while (nl != std::string::npos)
        {
            std::string line = pending.substr(0, nl);
            pending.erase(0, nl + 1);

            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
            {
                nl = pending.find('\n');
                continue;
            }

            OllamaResponse chunkResp = parseResponse(line);
            if (!chunkResp.response.empty() && on_chunk)
                on_chunk(chunkResp.response);
            finalResp = chunkResp;

            nl = pending.find('\n');
        }
    }

    if (!pending.empty())
    {
        OllamaResponse tailResp = parseResponse(pending);
        if (!tailResp.response.empty() && on_chunk)
            on_chunk(tailResp.response);
        finalResp = tailResp;
    }

    finalResp.done = true;
    if (on_complete)
        on_complete(finalResp);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

#else
// Linux/Mac implementation would use libcurl here
#endif

}  // namespace Backend
}  // namespace RawrXD
