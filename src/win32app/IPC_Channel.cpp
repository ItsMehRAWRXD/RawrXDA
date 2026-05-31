// IPC_Channel.cpp
// Phase 2: Inter-Process Communication channel implementation
// Named pipe-based reliable, bounded IPC with contract validation and timeouts

#include "IPC_Channel.h"
#include "IDELogger.h"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace RawrXD::Extensions {

    // ============================================================================
    // RequestResponseChannel Implementation
    // ============================================================================

    RequestResponseChannel::RequestResponseChannel(
        const std::string& channelName,
        DWORD processId)
        : m_channelName(channelName), m_processId(processId)
    {
        InitializeCriticalSection(&m_queueLock);
        m_queueEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    }

    RequestResponseChannel::~RequestResponseChannel()
    {
        Close();
        DeleteCriticalSection(&m_queueLock);
        if (m_queueEvent) {
            CloseHandle(m_queueEvent);
            m_queueEvent = nullptr;
        }
    }

    HRESULT RequestResponseChannel::Connect(DWORD timeoutMs)
    {
        if (m_state != IPCChannelState::Disconnected) {
            m_lastError = HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
            return m_lastError;
        }

        m_state = IPCChannelState::Connecting;

        // Build named pipe path
        std::string pipePath = "\\\\.\\pipe\\" + m_channelName;

        // Try to open existing pipe (created by extension process)
        m_pipeHandle = CreateFileA(
            pipePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr);

        if (m_pipeHandle == INVALID_HANDLE_VALUE) {
            DWORD dwError = GetLastError();
            
            if (dwError == ERROR_FILE_NOT_FOUND) {
                // Pipe doesn't exist yet - create it (IDE creates server-side pipe)
                m_pipeHandle = CreateNamedPipeA(
                    pipePath.c_str(),
                    PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                    1,                          // Max instances
                    64 * 1024,                  // Output buffer
                    64 * 1024,                  // Input buffer
                    timeoutMs,                  // Timeout
                    nullptr);                   // Security

                if (m_pipeHandle == INVALID_HANDLE_VALUE) {
                    dwError = GetLastError();
                    m_lastError = HRESULT_FROM_WIN32(dwError);
                    m_state = IPCChannelState::Error;
                    LOG_ERROR("Failed to create named pipe: " + std::to_string(dwError));
                    return m_lastError;
                }
            } else {
                m_lastError = HRESULT_FROM_WIN32(dwError);
                m_state = IPCChannelState::Error;
                LOG_ERROR("Failed to connect to named pipe: " + std::to_string(dwError));
                return m_lastError;
            }
        }

        // Setup pipe attributes
        HRESULT hr = SetupPipeAttributes();
        if (FAILED(hr)) {
            m_lastError = hr;
            m_state = IPCChannelState::Error;
            CloseHandle(m_pipeHandle);
            m_pipeHandle = nullptr;
            return hr;
        }

        // Validate contract (handshake)
        if (!ValidateContract()) {
            m_lastError = E_FAIL;
            m_state = IPCChannelState::Error;
            CloseHandle(m_pipeHandle);
            m_pipeHandle = nullptr;
            LOG_ERROR("IPC contract validation failed");
            return m_lastError;
        }

        // Start queue processor thread for async message handling
        m_processorRunning = true;
        m_processorThread = CreateThread(
            nullptr, 0,
            RequestResponseChannel::QueueProcessorThread,
            this, 0, nullptr);

        if (!m_processorThread) {
            m_lastError = HRESULT_FROM_WIN32(GetLastError());
            m_state = IPCChannelState::Error;
            Disconnect();
            return m_lastError;
        }

        m_state = IPCChannelState::Connected;
        m_lastError = S_OK;
        LOG_INFO("IPC channel connected: " + m_channelName);

        return S_OK;
    }

    void RequestResponseChannel::Disconnect()
    {
        if (m_state == IPCChannelState::Disconnecting || m_state == IPCChannelState::Disconnected) {
            return;
        }

        m_state = IPCChannelState::Disconnecting;

        // Stop processor thread
        m_processorRunning = false;
        if (m_queueEvent) {
            SetEvent(m_queueEvent);
        }

        if (m_processorThread) {
            WaitForSingleObject(m_processorThread, 2000);
            CloseHandle(m_processorThread);
            m_processorThread = nullptr;
        }

        // Close pipe
        if (m_pipeHandle && m_pipeHandle != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(m_pipeHandle);
            DisconnectNamedPipe(m_pipeHandle);
            CloseHandle(m_pipeHandle);
            m_pipeHandle = nullptr;
        }

        m_state = IPCChannelState::Disconnected;
        LOG_INFO("IPC channel disconnected: " + m_channelName);
    }

    bool RequestResponseChannel::IsConnected() const
    {
        return m_state == IPCChannelState::Connected;
    }

    IPCChannelState RequestResponseChannel::GetState() const
    {
        return m_state;
    }

    HRESULT RequestResponseChannel::SendMessage(const IPCMessage& message)
    {
        if (!IsConnected()) {
            m_lastError = E_NOT_VALID_STATE;
            return m_lastError;
        }

        HRESULT hr = WriteToPipe(message);
        if (SUCCEEDED(hr)) {
            m_messagesSent++;
        }
        m_lastError = hr;
        return hr;
    }

    HRESULT RequestResponseChannel::SendMessageAndWait(
        const IPCMessage& request,
        IPCMessage& response,
        DWORD timeoutMs)
    {
        if (!IsConnected()) {
            m_lastError = E_NOT_VALID_STATE;
            return m_lastError;
        }

        // Send request
        HRESULT hrSend = WriteToPipe(request);
        if (FAILED(hrSend)) {
            m_lastError = hrSend;
            return hrSend;
        }
        m_messagesSent++;

        // Wait for response
        HRESULT hrRecv = ReadFromPipe(response, timeoutMs);
        if (SUCCEEDED(hrRecv)) {
            m_messagesReceived++;
        }
        m_lastError = hrRecv;
        return hrRecv;
    }

    HRESULT RequestResponseChannel::ReceiveMessage(IPCMessage& message, DWORD timeoutMs)
    {
        if (!IsConnected()) {
            m_lastError = E_NOT_VALID_STATE;
            return m_lastError;
        }

        HRESULT hr = ReadFromPipe(message, timeoutMs);
        if (SUCCEEDED(hr)) {
            m_messagesReceived++;
        }
        m_lastError = hr;
        return hr;
    }

    HRESULT RequestResponseChannel::PostMessage(const IPCMessage& message)
    {
        if (!IsConnected()) {
            m_lastError = E_NOT_VALID_STATE;
            return m_lastError;
        }

        // Queue message for asynchronous sending
        EnterCriticalSection(&m_queueLock);
        m_messageQueue.push(message);
        LeaveCriticalSection(&m_queueLock);

        // Signal queue processor
        if (m_queueEvent) {
            SetEvent(m_queueEvent);
        }

        m_lastError = S_OK;
        return S_OK;
    }

    bool RequestResponseChannel::ValidateContract()
    {
        // Handshake: send and receive protocol version
        const uint32_t CONTRACT_VERSION = 0x00010001;
        
        DWORD bytesWritten = 0;
        if (!WriteFile(m_pipeHandle, &CONTRACT_VERSION, sizeof(CONTRACT_VERSION), &bytesWritten, nullptr)) {
            return false;
        }

        if (bytesWritten != sizeof(CONTRACT_VERSION)) {
            return false;
        }

        // Read version from peer
        DWORD bytesRead = 0;
        uint32_t peerVersion = 0;
        if (!ReadFile(m_pipeHandle, &peerVersion, sizeof(peerVersion), &bytesRead, nullptr)) {
            return false;
        }

        if (bytesRead != sizeof(peerVersion) || peerVersion != CONTRACT_VERSION) {
            return false;
        }

        return true;
    }

    HRESULT RequestResponseChannel::WriteToPipe(const IPCMessage& message)
    {
        if (!m_pipeHandle || m_pipeHandle == INVALID_HANDLE_VALUE) {
            return E_FAIL;
        }

        // Check payload size bounds (max 1MB per message)
        const DWORD MAX_PAYLOAD_SIZE = 1024 * 1024;
        if (message.payloadSize > MAX_PAYLOAD_SIZE) {
            LOG_WARNING("Message payload exceeds maximum size");
            return E_INVALIDARG;
        }

        // Write message header
        DWORD bytesWritten = 0;
        
        // Write message type (2 bytes)
        if (!WriteFile(m_pipeHandle, &message.messageType, sizeof(message.messageType), &bytesWritten, nullptr)) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        // Write message ID (4 bytes)
        if (!WriteFile(m_pipeHandle, &message.messageId, sizeof(message.messageId), &bytesWritten, nullptr)) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        // Write payload size (4 bytes)
        if (!WriteFile(m_pipeHandle, &message.payloadSize, sizeof(message.payloadSize), &bytesWritten, nullptr)) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        // Write payload if present
        if (message.payloadSize > 0 && message.payload) {
            if (!WriteFile(m_pipeHandle, message.payload, message.payloadSize, &bytesWritten, nullptr)) {
                return HRESULT_FROM_WIN32(GetLastError());
            }

            if (bytesWritten != message.payloadSize) {
                return E_FAIL;
            }
        }

        return S_OK;
    }

    HRESULT RequestResponseChannel::ReadFromPipe(IPCMessage& message, DWORD timeoutMs)
    {
        if (!m_pipeHandle || m_pipeHandle == INVALID_HANDLE_VALUE) {
            return E_FAIL;
        }

        DWORD bytesRead = 0;

        // Read message header
        // Message type (2 bytes)
        if (!ReadFile(m_pipeHandle, &message.messageType, sizeof(message.messageType), &bytesRead, nullptr)) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        if (bytesRead != sizeof(message.messageType)) {
            return E_FAIL;
        }

        // Message ID (4 bytes)
        if (!ReadFile(m_pipeHandle, &message.messageId, sizeof(message.messageId), &bytesRead, nullptr)) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        if (bytesRead != sizeof(message.messageId)) {
            return E_FAIL;
        }

        // Payload size (4 bytes)
        if (!ReadFile(m_pipeHandle, &message.payloadSize, sizeof(message.payloadSize), &bytesRead, nullptr)) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        if (bytesRead != sizeof(message.payloadSize)) {
            return E_FAIL;
        }

        // Bounds check on payload size
        const DWORD MAX_PAYLOAD_SIZE = 1024 * 1024;
        if (message.payloadSize > MAX_PAYLOAD_SIZE) {
            LOG_WARNING("Received message with oversized payload");
            return E_INVALIDARG;
        }

        // Read payload if present
        if (message.payloadSize > 0) {
            message.payload = new uint8_t[message.payloadSize];
            if (!ReadFile(m_pipeHandle, message.payload, message.payloadSize, &bytesRead, nullptr)) {
                delete[] message.payload;
                message.payload = nullptr;
                return HRESULT_FROM_WIN32(GetLastError());
            }

            if (bytesRead != message.payloadSize) {
                delete[] message.payload;
                message.payload = nullptr;
                return E_FAIL;
            }
        }

        return S_OK;
    }

    HRESULT RequestResponseChannel::SetupPipeAttributes()
    {
        if (!m_pipeHandle || m_pipeHandle == INVALID_HANDLE_VALUE) {
            return E_FAIL;
        }

        // Set default timeout
        DWORD dwMode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
        if (!SetNamedPipeHandleState(m_pipeHandle, &dwMode, nullptr, nullptr)) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        return S_OK;
    }

    DWORD WINAPI RequestResponseChannel::QueueProcessorThread(LPVOID param)
    {
        RequestResponseChannel* pThis = static_cast<RequestResponseChannel*>(param);
        if (pThis) {
            pThis->QueueProcessorMain();
        }
        return 0;
    }

    void RequestResponseChannel::QueueProcessorMain()
    {
        while (m_processorRunning) {
            DWORD dwWait = WaitForSingleObject(m_queueEvent, 1000);

            if (dwWait == WAIT_OBJECT_0 && m_processorRunning) {
                ProcessMessageQueue();
            }
        }
    }

    void RequestResponseChannel::ProcessMessageQueue()
    {
        EnterCriticalSection(&m_queueLock);
        
        while (!m_messageQueue.empty()) {
            IPCMessage msg = m_messageQueue.front();
            m_messageQueue.pop();

            LeaveCriticalSection(&m_queueLock);

            // Send message
            WriteToPipe(msg);
            m_messagesSent++;

            EnterCriticalSection(&m_queueLock);
        }

        LeaveCriticalSection(&m_queueLock);
    }

    size_t RequestResponseChannel::GetQueueDepth() const
    {
        // Note: This is inherently racy but OK for diagnostics
        return m_messageQueue.size();
    }

    void RequestResponseChannel::Close()
    {
        Disconnect();
    }

    void RequestResponseChannel::SetOnMessageReceivedCallback(OnMessageReceivedCallback callback)
    {
        m_onMessageReceived = callback;
    }

    void RequestResponseChannel::SetOnDisconnectedCallback(OnChannelDisconnectedCallback callback)
    {
        m_onDisconnected = callback;
    }

} // namespace RawrXD::Extensions
