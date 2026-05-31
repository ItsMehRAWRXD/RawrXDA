#include "ExtensionIPCChannel.h"
#include <iostream>

namespace RawrXD::Extensions {

ExtensionIPCChannel::ExtensionIPCChannel(const std::wstring& pipeName) 
    : m_pipeName(L"\\\\.\\pipe\\" + pipeName) {}

ExtensionIPCChannel::~ExtensionIPCChannel() {
    if (m_hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hPipe);
    }
}

bool ExtensionIPCChannel::Create() {
    m_hPipe = CreateNamedPipeW(
        m_pipeName.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        65536,
        65536,
        0,
        NULL
    );
    return m_hPipe != INVALID_HANDLE_VALUE;
}

bool ExtensionIPCChannel::Connect() {
    OVERLAPPED ol = {0};
    ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    BOOL result = ConnectNamedPipe(m_hPipe, &ol);
    if (!result && GetLastError() == ERROR_IO_PENDING) {
        WaitForSingleObject(ol.hEvent, INFINITE);
    }
    
    CloseHandle(ol.hEvent);
    return true;
}

bool ExtensionIPCChannel::Send(IPCMessageType type, uint32_t msgId, const std::vector<uint8_t>& payload) {
    std::lock_guard<std::mutex> lock(m_sendMutex);
    IPCHeader header;
    header.type = type;
    header.messageId = msgId;
    header.payloadSize = static_cast<uint32_t>(payload.size());

    DWORD written;
    if (!WriteFile(m_hPipe, &header, sizeof(header), &written, NULL)) return false;
    if (header.payloadSize > 0) {
        if (!WriteFile(m_hPipe, payload.data(), header.payloadSize, &written, NULL)) return false;
    }
    FlushFileBuffers(m_hPipe);
    return true;
}

bool ExtensionIPCChannel::Receive(IPCHeader& header, std::vector<uint8_t>& payload) {
    DWORD read;
    if (!ReadFile(m_hPipe, &header, sizeof(header), &read, NULL)) return false;
    if (header.magic != 0x52584449) return false;
    
    if (header.payloadSize > 0) {
        payload.resize(header.payloadSize);
        if (!ReadFile(m_hPipe, payload.data(), header.payloadSize, &read, NULL)) return false;
    }
    return true;
}

}
