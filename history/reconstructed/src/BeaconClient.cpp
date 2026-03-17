// BeaconClient.cpp - Implementation of C++ Beacon Client

#include "BeaconClient.h"
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")

BeaconClient::BeaconClient()
    : m_serverPort(3000), m_hSession(NULL), m_hConnect(NULL),
      m_initialized(false) {
}

BeaconClient::~BeaconClient() {
    shutdown();
}

bool BeaconClient::initialize(const std::string& serverHost, int serverPort) {
    if (m_initialized) {
        shutdown();
    }

    m_serverHost = serverHost;
    m_serverPort = serverPort;

    // Initialize WinHTTP session
    m_hSession = WinHttpOpen(L"RawrXD-BeaconClient/1.0",
                            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);

    if (!m_hSession) {
        m_lastError = "Failed to initialize WinHTTP session";
        return false;
    }

    // Connect to server
    std::wstring hostW(m_serverHost.begin(), m_serverHost.end());
    m_hConnect = WinHttpConnect(m_hSession, hostW.c_str(),
                               static_cast<INTERNET_PORT>(m_serverPort), 0);

    if (!m_hConnect) {
        m_lastError = "Failed to connect to beacon server";
        WinHttpCloseHandle(m_hSession);
        m_hSession = NULL;
        return false;
    }

    m_initialized = true;
    return true;
}

void BeaconClient::shutdown() {
    if (m_hConnect) {
        WinHttpCloseHandle(m_hConnect);
        m_hConnect = NULL;
    }
    if (m_hSession) {
        WinHttpCloseHandle(m_hSession);
        m_hSession = NULL;
    }
    m_initialized = false;
}

bool BeaconClient::registerBeacon(const std::string& componentId, const std::string& type) {
    if (!m_initialized) {
        m_lastError = "Beacon client not initialized";
        return false;
    }

    nlohmann::json body = {
        {"componentId", componentId},
        {"type", type}
    };

    std::string response;
    if (!makeHttpRequest("POST", "/api/beacon/register", body.dump(), response)) {
        return false;
    }

    try {
        auto json = nlohmann::json::parse(response);
        return json.value("success", false);
    } catch (...) {
        m_lastError = "Invalid response from server";
        return false;
    }
}

bool BeaconClient::unregisterBeacon(const std::string& componentId) {
    // For unregister, we could implement if needed
    // For now, just return true
    return true;
}

bool BeaconClient::sendMessage(const std::string& sourceId, const std::string& targetType,
                              const std::string& message, const std::string& options) {
    if (!m_initialized) {
        m_lastError = "Beacon client not initialized";
        return false;
    }

    nlohmann::json body = {
        {"sourceId", sourceId},
        {"targetType", targetType},
        {"message", message}
    };

    if (!options.empty() && options != "{}") {
        try {
            auto opts = nlohmann::json::parse(options);
            body["options"] = opts;
        } catch (...) {
            // Ignore invalid options
        }
    }

    std::string response;
    if (!makeHttpRequest("POST", "/api/beacon/send", body.dump(), response)) {
        return false;
    }

    try {
        auto json = nlohmann::json::parse(response);
        return json.value("success", false);
    } catch (...) {
        m_lastError = "Invalid response from server";
        return false;
    }
}

bool BeaconClient::sendToAgentic(const std::string& sourceId, const std::string& action,
                                const std::string& params) {
    if (!m_initialized) return false;

    nlohmann::json body = {
        {"sourceId", sourceId},
        {"action", action}
    };

    if (!params.empty() && params != "{}") {
        try {
            auto p = nlohmann::json::parse(params);
            body["params"] = p;
        } catch (...) {}
    }

    std::string response;
    if (!makeHttpRequest("POST", "/api/beacon/agentic", body.dump(), response)) {
        return false;
    }

    try {
        auto json = nlohmann::json::parse(response);
        return json.value("success", false);
    } catch (...) {
        return false;
    }
}

bool BeaconClient::sendToHotpatch(const std::string& sourceId, const std::string& target,
                                 const std::string& patch) {
    if (!m_initialized) return false;

    nlohmann::json body = {
        {"sourceId", sourceId},
        {"target", target},
        {"patch", patch}
    };

    std::string response;
    if (!makeHttpRequest("POST", "/api/beacon/hotpatch", body.dump(), response)) {
        return false;
    }

    try {
        auto json = nlohmann::json::parse(response);
        return json.value("success", false);
    } catch (...) {
        return false;
    }
}

bool BeaconClient::sendToSecurity(const std::string& sourceId, const std::string& operation,
                                 const std::string& data) {
    if (!m_initialized) return false;

    nlohmann::json body = {
        {"sourceId", sourceId},
        {"operation", operation}
    };

    if (!data.empty() && data != "{}") {
        try {
            auto d = nlohmann::json::parse(data);
            body["data"] = d;
        } catch (...) {}
    }

    std::string response;
    if (!makeHttpRequest("POST", "/api/beacon/security", body.dump(), response)) {
        return false;
    }

    try {
        auto json = nlohmann::json::parse(response);
        return json.value("success", false);
    } catch (...) {
        return false;
    }
}

bool BeaconClient::sendToEncryption(const std::string& sourceId, const std::string& mode,
                                   const std::string& data) {
    if (!m_initialized) return false;

    nlohmann::json body = {
        {"sourceId", sourceId},
        {"mode", mode},
        {"data", data}
    };

    std::string response;
    if (!makeHttpRequest("POST", "/api/beacon/encryption", body.dump(), response)) {
        return false;
    }

    try {
        auto json = nlohmann::json::parse(response);
        return json.value("success", false);
    } catch (...) {
        return false;
    }
}

bool BeaconClient::getBeaconStatus(std::vector<BeaconInfo>& beacons) {
    if (!m_initialized) return false;

    std::string response;
    if (!makeHttpRequest("GET", "/api/beacon/status", "", response)) {
        return false;
    }

    try {
        auto json = nlohmann::json::parse(response);
        auto beaconList = json["beacons"];
        for (auto& b : beaconList) {
            BeaconInfo info;
            info.componentId = b["id"];
            info.type = b["type"];
            info.status = b["status"];
            info.lastSeen = b["lastSeen"];
            beacons.push_back(info);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool BeaconClient::getBeaconList(std::vector<std::string>& beaconIds) {
    if (!m_initialized) return false;

    std::string response;
    if (!makeHttpRequest("GET", "/api/beacon/beacons", "", response)) {
        return false;
    }

    try {
        auto json = nlohmann::json::parse(response);
        auto list = json["list"];
        for (auto& id : list) {
            beaconIds.push_back(id);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool BeaconClient::makeHttpRequest(const std::string& method, const std::string& path,
                                  const std::string& body, std::string& response) {
    if (!m_hConnect) {
        m_lastError = "Not connected to server";
        return false;
    }

    std::wstring pathW(path.begin(), path.end());
    std::wstring methodW(method.begin(), method.end());

    HINTERNET hRequest = WinHttpOpenRequest(m_hConnect, methodW.c_str(), pathW.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);

    if (!hRequest) {
        m_lastError = "Failed to open HTTP request";
        return false;
    }

    // Set headers
    std::string headers = "Content-Type: application/json\r\n";
    if (!WinHttpAddRequestHeaders(hRequest, std::wstring(headers.begin(), headers.end()).c_str(),
                                 (ULONG)-1, WINHTTP_ADDREQ_FLAG_ADD)) {
        WinHttpCloseHandle(hRequest);
        m_lastError = "Failed to add request headers";
        return false;
    }

    // Send request
    BOOL result = WinHttpSendRequest(hRequest,
                                    WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    (LPVOID)body.c_str(), body.length(),
                                    body.length(), 0);

    if (!result) {
        WinHttpCloseHandle(hRequest);
        m_lastError = "Failed to send HTTP request";
        return false;
    }

    // Receive response
    result = WinHttpReceiveResponse(hRequest, NULL);
    if (!result) {
        WinHttpCloseHandle(hRequest);
        m_lastError = "Failed to receive HTTP response";
        return false;
    }

    // Read response data
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;
    char buffer[4096];

    response.clear();
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        if (!WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) || bytesRead == 0) {
            break;
        }
        response.append(buffer, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    return true;
}