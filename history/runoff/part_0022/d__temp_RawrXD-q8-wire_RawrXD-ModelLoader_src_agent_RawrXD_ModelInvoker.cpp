/**
 * @file RawrXD_ModelInvoker.cpp
 * @brief WinHTTP-based LLM invoker for RawrXD
 */

#include "RawrXD_ModelInvoker.hpp"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD::Agent {

ModelInvoker::ModelInvoker() {}
ModelInvoker::~ModelInvoker() {}

void ModelInvoker::set_backend(const std::string& backend, const std::string& endpoint, const std::string& api_key) {
    backend_ = backend;
    endpoint_ = endpoint;
    api_key_ = api_key;
}

void ModelInvoker::set_model(const std::string& model) {
    model_ = model;
}

std::string ModelInvoker::send_http_request(const std::string& url, const std::string& payload) {
    std::string response_data;
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    
    // Parse URL (very basic)
    // endpoint_ usually like http://localhost:11434
    std::wstring wendpoint = Platform::wide_from_utf8(endpoint_);
    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    
    wchar_t hostName[256], path[1024];
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = 1024;
    
    if (!WinHttpCrackUrl(wendpoint.c_str(), 0, 0, &urlComp)) {
        return "ERROR: Invalid URL";
    }

    hSession = WinHttpOpen(L"RawrXD Agent/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (hSession) {
        hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    }
    
    if (hConnect) {
        std::wstring wpath = Platform::wide_from_utf8(url); // e.g. "/api/generate"
        hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath.c_str(), NULL, 
                                      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                      (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    }

    if (hRequest) {
        std::string headers = "Content-Type: application/json\r\n";
        if (!api_key_.empty()) {
            headers += "Authorization: Bearer " + api_key_ + "\r\n";
        }
        std::wstring wheaders = Platform::wide_from_utf8(headers);

        if (WinHttpSendRequest(hRequest, wheaders.c_str(), -1L, (LPVOID)payload.c_str(), 
                               (DWORD)payload.size(), (DWORD)payload.size(), 0)) {
            if (WinHttpReceiveResponse(hRequest, NULL)) {
                DWORD dwSize = 0;
                do {
                    if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                    if (dwSize == 0) break;
                    
                    char* pszOutBuffer = new char[dwSize + 1];
                    DWORD dwDownloaded = 0;
                    if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                        response_data.append(pszOutBuffer, dwDownloaded);
                    }
                    delete[] pszOutBuffer;
                } while (dwSize > 0);
            }
        }
    }

    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    return response_data;
}

LLMResponse ModelInvoker::invoke(const std::string& wish, const std::vector<std::string>& tools) {
    LLMResponse res;
    
    std::string payload;
    std::string api_url;

    if (backend_ == "ollama") {
        api_url = "/api/generate";
        payload = "{\"model\":\"" + model_ + "\",\"prompt\":\"" + wish + "\",\"stream\":false}";
    } else if (backend_ == "openai") {
        api_url = "/v1/chat/completions";
        payload = "{\"model\":\"" + model_ + "\",\"messages\":[{\"role\":\"user\",\"content\":\"" + wish + "\"}]}";
    } else {
        res.error = "Unsupported backend: " + backend_;
        return res;
    }

    std::string response = send_http_request(api_url, payload);
    if (response.find("ERROR") == 0) {
        res.error = response;
    } else {
        res.raw_output = response;
        res.success = true;
    }

    return res;
}

std::vector<float> ModelInvoker::GenerateEmbedding(const std::string& model, const std::string& text) {
    std::string endpoint = "/api/embeddings";
    std::string jsonPayload = "{\"model\":\"" + model + "\",\"prompt\":\"" + text + "\"}";
    
    std::string response = send_http_request(endpoint_ + endpoint, jsonPayload);
    
    // Simple JSON parsing for ["embedding":[...]]
    std::vector<float> embedding;
    size_t embedPos = response.find("\"embedding\":[");
    if (embedPos != std::string::npos) {
        size_t start = embedPos + 13;
        size_t end = response.find("]", start);
        if (end != std::string::npos) {
            std::string vals = response.substr(start, end - start);
            size_t commaPos;
            size_t current = 0;
            while ((commaPos = vals.find(",", current)) != std::string::npos) {
                embedding.push_back(std::stof(vals.substr(current, commaPos - current)));
                current = commaPos + 1;
            }
            if (current < vals.size()) {
                embedding.push_back(std::stof(vals.substr(current)));
            }
        }
    }
    return embedding;
}

} // namespace RawrXD::Agent
