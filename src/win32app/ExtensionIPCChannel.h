#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>

namespace RawrXD::Extensions {

enum class IPCMessageType : uint32_t {
    Handshake = 0,
    Request = 1,
    Response = 2,
    Notification = 3,
    Heartbeat = 4,
    Error = 5,
    Shutdown = 6
};

struct IPCHeader {
    uint32_t magic = 0x52584449; // 'RXDI'
    IPCMessageType type;
    uint32_t messageId;
    uint32_t payloadSize;
};

class ExtensionIPCChannel {
public:
    ExtensionIPCChannel(const std::wstring& pipeName);
    ~ExtensionIPCChannel();

    bool Create();
    bool Connect();
    bool Send(IPCMessageType type, uint32_t msgId, const std::vector<uint8_t>& payload);
    bool Receive(IPCHeader& header, std::vector<uint8_t>& payload);

private:
    std::wstring m_pipeName;
    HANDLE m_hPipe = INVALID_HANDLE_VALUE;
    std::mutex m_sendMutex;
};

}
