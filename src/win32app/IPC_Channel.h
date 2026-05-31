// IPC_Channel.h
// Phase 2: Inter-Process Communication channel interface
// Provides reliable, bounded IPC between IDE and extension host

#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include <functional>
#include <cstdint>
#include <atomic>
#include <queue>

namespace RawrXD::Extensions {

    // IPC message types
    enum class IPCMessageType : uint16_t {
        Handshake = 0x0001,
        Request = 0x0002,
        Response = 0x0003,
        Notification = 0x0004,
        Heartbeat = 0x0005,
        Error = 0x0006,
        Shutdown = 0x0007
    };

    // IPC message contract
    struct IPCMessage {
        IPCMessageType messageType;
        uint32_t messageId;
        uint32_t payloadSize;
        uint8_t* payload;         // Caller owns allocation
        int32_t timeoutMs;
        
        IPCMessage() 
            : messageType(IPCMessageType::Request), messageId(0), 
              payloadSize(0), payload(nullptr), timeoutMs(5000) {}
    };

    // IPC channel state
    enum class IPCChannelState {
        Disconnected,
        Connecting,
        Connected,
        Error,
        Disconnecting
    };

    // Callback for incoming messages
    using OnMessageReceivedCallback = std::function<void(const IPCMessage& message)>;
    using OnChannelDisconnectedCallback = std::function<void()>;

    // ============================================================================
    // IPC_Channel - Abstract interface for IPC communication
    // ============================================================================
    class IPC_Channel {
    public:
        virtual ~IPC_Channel() = default;

        // --- Connection Management ---
        
        /// Connect to remote process via named pipe
        /// Returns: HRESULT indicating connection success
        virtual HRESULT Connect(DWORD timeoutMs = 5000) = 0;

        /// Disconnect from remote process
        virtual void Disconnect() = 0;

        /// Check if channel is connected
        virtual bool IsConnected() const = 0;

        /// Get current channel state
        virtual IPCChannelState GetState() const = 0;

        // --- Message Communication ---
        
        /// Send message to remote process (synchronous)
        /// Returns: HRESULT indicating send success
        virtual HRESULT SendMessage(const IPCMessage& message) = 0;

        /// Send message and wait for response (synchronous)
        /// Returns: HRESULT indicating operation success
        /// Response payload is allocated by callee, caller must free
        virtual HRESULT SendMessageAndWait(
            const IPCMessage& request,
            IPCMessage& response,
            DWORD timeoutMs = 5000) = 0;

        /// Receive message (blocking)
        /// Returns: HRESULT indicating receive success
        virtual HRESULT ReceiveMessage(IPCMessage& message, DWORD timeoutMs = 5000) = 0;

        /// Post message asynchronously (for notifications)
        /// Returns: HRESULT indicating post success
        virtual HRESULT PostMessage(const IPCMessage& message) = 0;

        // --- Validation ---
        
        /// Validate IPC contract (handshake/protocol version)
        /// Returns: true if contract is valid
        virtual bool ValidateContract() = 0;

        /// Get contract version
        virtual uint32_t GetContractVersion() const = 0;

        // --- Resource Management ---
        
        /// Get pipe resource handle
        virtual HANDLE GetPipeHandle() const = 0;

        /// Close and cleanup resources
        virtual void Close() = 0;

        // --- Callback Registration ---
        
        virtual void SetOnMessageReceivedCallback(OnMessageReceivedCallback callback) = 0;
        virtual void SetOnDisconnectedCallback(OnChannelDisconnectedCallback callback) = 0;

        // --- Diagnostics ---
        
        /// Get queue depth (pending messages)
        virtual size_t GetQueueDepth() const = 0;

        /// Get total messages sent
        virtual uint64_t GetMessagesSent() const = 0;

        /// Get total messages received
        virtual uint64_t GetMessagesReceived() const = 0;

        /// Get last error
        virtual HRESULT GetLastError() const = 0;
    };

    // ============================================================================
    // RequestResponseChannel - Named pipe-based IPC implementation
    // ============================================================================
    class RequestResponseChannel : public IPC_Channel {
    public:
        RequestResponseChannel(const std::string& channelName, DWORD processId);
        
        ~RequestResponseChannel();

        // --- Connection Management ---
        HRESULT Connect(DWORD timeoutMs = 5000) override;
        void Disconnect() override;
        bool IsConnected() const override;
        IPCChannelState GetState() const override;

        // --- Message Communication ---
        HRESULT SendMessage(const IPCMessage& message) override;
        HRESULT SendMessageAndWait(
            const IPCMessage& request,
            IPCMessage& response,
            DWORD timeoutMs = 5000) override;
        HRESULT ReceiveMessage(IPCMessage& message, DWORD timeoutMs = 5000) override;
        HRESULT PostMessage(const IPCMessage& message) override;

        // --- Validation ---
        bool ValidateContract() override;
        uint32_t GetContractVersion() const override { return 0x00010001; }  // Version 1.1

        // --- Resource Management ---
        HANDLE GetPipeHandle() const override { return m_pipeHandle; }
        void Close() override;

        // --- Callback Registration ---
        void SetOnMessageReceivedCallback(OnMessageReceivedCallback callback) override;
        void SetOnDisconnectedCallback(OnChannelDisconnectedCallback callback) override;

        // --- Diagnostics ---
        size_t GetQueueDepth() const override;
        uint64_t GetMessagesSent() const override { return m_messagesSent; }
        uint64_t GetMessagesReceived() const override { return m_messagesReceived; }
        HRESULT GetLastError() const override { return m_lastError; }

    private:
        // --- Internal Methods ---
        
        /// Write message to pipe with bounds checking
        HRESULT WriteToPipe(const IPCMessage& message);

        /// Read message from pipe with bounds checking
        HRESULT ReadFromPipe(IPCMessage& message, DWORD timeoutMs);

        /// Setup pipe with security attributes
        HRESULT SetupPipeAttributes();

        /// Message queue management for async operations
        void ProcessMessageQueue();

        static DWORD WINAPI QueueProcessorThread(LPVOID param);
        void QueueProcessorMain();

        // --- Member Variables ---
        
        std::string m_channelName;
        DWORD m_processId;
        HANDLE m_pipeHandle = nullptr;
        std::atomic<IPCChannelState> m_state{IPCChannelState::Disconnected};
        std::atomic<HRESULT> m_lastError{S_OK};
        
        // Message tracking
        std::atomic<uint64_t> m_messagesSent{0};
        std::atomic<uint64_t> m_messagesReceived{0};
        
        // Message queue for async operations
        CRITICAL_SECTION m_queueLock;
        std::queue<IPCMessage> m_messageQueue;
        HANDLE m_queueEvent = nullptr;
        
        // Queue processor thread
        HANDLE m_processorThread = nullptr;
        std::atomic<bool> m_processorRunning{false};
        
        // Callbacks
        OnMessageReceivedCallback m_onMessageReceived;
        OnChannelDisconnectedCallback m_onDisconnected;
    };

} // namespace RawrXD::Extensions
