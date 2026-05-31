#include "ExtensionHostIpcBridge.hpp"

#include <mutex>

#if defined(RAWRXD_IPC_BRIDGE_QUIET)
#define LOG_INFO(msg) ((void)0)
#define LOG_WARNING(msg) ((void)0)
#else
#include "IDELogger.h"
#endif

namespace RawrXD::ExtensionHost
{

namespace
{
std::mutex g_bridgeInstanceMutex;
}  // namespace

ExtensionHostIpcBridge::ExtensionHostIpcBridge() : m_pipeName(kDefaultPipeName) {}

ExtensionHostIpcBridge::ExtensionHostIpcBridge(const wchar_t* pipeName)
    : m_pipeName(pipeName ? pipeName : kDefaultPipeName)
{
}

ExtensionHostIpcBridge::~ExtensionHostIpcBridge()
{
    Disconnect();
}

bool ExtensionHostIpcBridge::AttachServerHandle(HANDLE pipeHandle)
{
    if (pipeHandle == nullptr || pipeHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    if (m_ownsHandle && m_pipeHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_pipeHandle);
    }

    m_pipeHandle = pipeHandle;
    m_ownsHandle = false;
    m_receiver.Reset();
    return true;
}

bool ExtensionHostIpcBridge::ConnectAsClient(const wchar_t* pipeName, DWORD waitMs)
{
    Disconnect();

    const wchar_t* path = pipeName ? pipeName : m_pipeName.c_str();
    m_pipeName = path;

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        m_pipeHandle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

        if (m_pipeHandle != INVALID_HANDLE_VALUE)
        {
            break;
        }

        const DWORD err = GetLastError();
        if (err != ERROR_PIPE_BUSY)
        {
            return false;
        }

        if (!WaitNamedPipeW(path, waitMs))
        {
            return false;
        }
    }

    if (m_pipeHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD mode = PIPE_READMODE_BYTE;
    SetNamedPipeHandleState(m_pipeHandle, &mode, nullptr, nullptr);

    m_ownsHandle = true;
    m_receiver.Reset();
    return true;
}

void ExtensionHostIpcBridge::Disconnect()
{
    if (m_ownsHandle && m_pipeHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_pipeHandle);
    }

    m_pipeHandle = INVALID_HANDLE_VALUE;
    m_ownsHandle = false;
    m_receiver.Reset();
}

void ExtensionHostIpcBridge::SetMessageHandler(LogicalMessageHandler handler)
{
    m_onLogicalMessage = std::move(handler);
}

void ExtensionHostIpcBridge::PollIncomingTraffic()
{
    if (m_pipeHandle == INVALID_HANDLE_VALUE)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(g_bridgeInstanceMutex);
    DrainAvailableBytes();
    DispatchLogicalMessages();
}

bool ExtensionHostIpcBridge::WriteMessage(uint16_t msgType, const uint8_t* payload, uint32_t payloadLen)
{
    if (!payload && payloadLen > 0)
    {
        return false;
    }
    const std::string body(payload ? reinterpret_cast<const char*>(payload) : "",
                           payload ? static_cast<size_t>(payloadLen) : 0);
    return WriteLogicalMessage(static_cast<RawrXD::IPC::Chat::ChatMessageType>(msgType), body);
}

bool ExtensionHostIpcBridge::WriteLogicalMessage(RawrXD::IPC::Chat::ChatMessageType type,
                                                 const std::string& logicalPayload)
{
    if (m_pipeHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_bridgeInstanceMutex);
    RawrXD::IPC::Chat::MessageSegmenter segmenter;
    if (!segmenter.Begin(type, logicalPayload))
    {
        LOG_WARNING("ExtensionHostIpcBridge: segmenter rejected logical payload");
        return false;
    }

    while (segmenter.HasNext())
    {
        std::vector<uint8_t> prefixed;
        if (!segmenter.NextPrefixedWireBlob(prefixed) || prefixed.empty())
        {
            LOG_WARNING("ExtensionHostIpcBridge: failed to build prefixed wire blob");
            return false;
        }

        if (!WritePrefixedWireBlob(prefixed.data(), prefixed.size()))
        {
            return false;
        }
    }

    return true;
}

bool ExtensionHostIpcBridge::WritePrefixedWireBlob(const uint8_t* data, size_t size)
{
    if (!data || size == 0 || m_pipeHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    if (size > RawrXD::IPC::Chat::WIRE_FRAME_PREFIX_SIZE + RawrXD::IPC::Chat::MAX_PIPE_FRAME_BYTES)
    {
        return false;
    }

    return WriteAll(data, static_cast<DWORD>(size));
}

void ExtensionHostIpcBridge::DrainAvailableBytes()
{
    DWORD bytesAvailable = 0;
    while (PeekNamedPipe(m_pipeHandle, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0)
    {
        std::vector<uint8_t> chunk(bytesAvailable);
        DWORD bytesRead = 0;
        if (!ReadFile(m_pipeHandle, chunk.data(), bytesAvailable, &bytesRead, nullptr) || bytesRead == 0)
        {
            HandlePipeFailure();
            break;
        }

        chunk.resize(bytesRead);
        m_receiver.AppendStreamBytes(chunk);
    }
}

void ExtensionHostIpcBridge::DispatchLogicalMessages()
{
    RawrXD::IPC::Chat::ChatMessageType msgType = RawrXD::IPC::Chat::ChatMessageType::Error;
    std::string payload;

    while (m_receiver.TryPopLogicalMessage(&msgType, &payload))
    {
        if (m_onLogicalMessage)
        {
            m_onLogicalMessage(msgType, payload);
        }
        else
        {
            LOG_INFO("ExtensionHostIpcBridge: logical message type=" + std::to_string(static_cast<uint16_t>(msgType)) +
                     " bytes=" + std::to_string(payload.size()));
        }
    }
}

void ExtensionHostIpcBridge::HandlePipeFailure()
{
    const DWORD err = GetLastError();
    if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA || err == ERROR_PIPE_NOT_CONNECTED)
    {
        LOG_WARNING("ExtensionHostIpcBridge: pipe disconnected (err=" + std::to_string(err) + ")");
        Disconnect();
    }
}

bool ExtensionHostIpcBridge::WriteAll(const uint8_t* data, DWORD size)
{
    DWORD totalWritten = 0;
    while (totalWritten < size)
    {
        DWORD written = 0;
        const BOOL ok = WriteFile(m_pipeHandle, data + totalWritten, size - totalWritten, &written, nullptr);
        if (!ok || written == 0)
        {
            HandlePipeFailure();
            return false;
        }
        totalWritten += written;
    }
    return true;
}

}  // namespace RawrXD::ExtensionHost
