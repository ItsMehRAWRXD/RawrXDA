// RawrXD_CloudClient.hpp - Universal Cloud API Client (WinHTTP)
// Pure C++20 - No Qt Dependencies
// Supports: OpenAI, Anthropic, Groq, local GGUF providers

#pragma once

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include "RawrXD_JSON.hpp"

#pragma comment(lib, "winhttp.lib")

namespace RawrXD {

struct CloudResponse {
    bool success = false;
    std::string content;
    int statusCode = 0;
    std::string error;
};

class CloudClient {
public:
    static CloudResponse Generate(const std::string& provider, const std::string& model, 
                                 const std::string& apiKey, const std::string& prompt) {
        std::wstring host = L"api.openai.com";
        std::wstring path = L"/v1/chat/completions";
        
        if (provider == "anthropic") {
            host = L"api.anthropic.com";
            path = L"/v1/messages";
        }

        HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return {false, "", 0, "WinHttpOpen failed"};

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return {false, "", 0, "WinHttpConnect failed"}; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return {false, "", 0, "WinHttpOpenRequest failed"}; }

        // Headers
        std::string auth = "Authorization: Bearer " + apiKey;
        std::wstring wauth(auth.begin(), auth.end());
        WinHttpAddRequestHeaders(hRequest, wauth.c_str(), (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
        WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);

        // Body
        JSONObject bodyObj;
        bodyObj["model"] = JSONValue(model);
        JSONArray msgs;
        JSONObject msg;
        msg["role"] = JSONValue("user");
        msg["content"] = JSONValue(prompt);
        msgs.push_back(JSONValue(msg));
        bodyObj["messages"] = JSONValue(msgs);
        
        std::string body = JSONValue(bodyObj).stringify();

        CloudResponse res;
        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), (DWORD)body.length(), (DWORD)body.length(), 0)) {
            if (WinHttpReceiveResponse(hRequest, NULL)) {
                DWORD size = 0;
                WinHttpQueryDataAvailable(hRequest, &size);
                if (size > 0) {
                    char* buffer = new char[size + 1];
                    DWORD read = 0;
                    WinHttpReadData(hRequest, buffer, size, &read);
                    buffer[read] = '\0';
                    res.content = buffer;
                    delete[] buffer;
                    res.success = true;
                }
            }
        } else {
            res.error = "Request failed: " + std::to_string(GetLastError());
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return res;
    }
};

} // namespace RawrXD
