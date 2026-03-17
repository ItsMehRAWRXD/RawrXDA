// BeaconClient.h - C++ client for RawrXD Beacon System
// Enables Win32 IDE to communicate with the circular beacon network

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <windows.h>
#include <winhttp.h>

class BeaconClient {
public:
    struct BeaconMessage {
        std::string id;
        std::string sourceId;
        std::string targetType;
        std::string payload;
        long long timestamp;
        int ttl;
        std::string priority;
    };

    struct BeaconInfo {
        std::string componentId;
        std::string type;
        std::string address;
        std::string status;
        long long lastSeen;
    };

    using MessageCallback = std::function<void(const BeaconMessage&)>;

    BeaconClient();
    ~BeaconClient();

    bool initialize(const std::string& serverHost = "localhost", int serverPort = 3000);
    void shutdown();

    // Beacon registration
    bool registerBeacon(const std::string& componentId, const std::string& type);
    bool unregisterBeacon(const std::string& componentId);

    // Message sending
    bool sendMessage(const std::string& sourceId, const std::string& targetType,
                    const std::string& message, const std::string& options = "{}");

    // Specific component messaging
    bool sendToAgentic(const std::string& sourceId, const std::string& action,
                      const std::string& params = "{}");
    bool sendToHotpatch(const std::string& sourceId, const std::string& target,
                       const std::string& patch);
    bool sendToSecurity(const std::string& sourceId, const std::string& operation,
                       const std::string& data = "{}");
    bool sendToEncryption(const std::string& sourceId, const std::string& mode,
                         const std::string& data);

    // Status queries
    bool getBeaconStatus(std::vector<BeaconInfo>& beacons);
    bool getBeaconList(std::vector<std::string>& beaconIds);

    // Message handling
    void setMessageCallback(MessageCallback callback) { m_messageCallback = callback; }

    // Utility
    std::string getLastError() const { return m_lastError; }

private:
    bool makeHttpRequest(const std::string& method, const std::string& path,
                        const std::string& body, std::string& response);

    std::string m_serverHost;
    int m_serverPort;
    HINTERNET m_hSession;
    HINTERNET m_hConnect;
    MessageCallback m_messageCallback;
    std::string m_lastError;
    bool m_initialized;
};