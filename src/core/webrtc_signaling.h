// ============================================================================
// webrtc_signaling.h — Phase 20: WebRTC P2P Signaling for Swarm
// ============================================================================
// P2P signaling layer for NAT traversal in distributed swarm.
// Uses STUN/TURN and a custom signaling server for peer discovery.
// Enables direct node-to-node communication without port forwarding.
//
// Architecture:
//   1. SignalingClient — Connects to signaling server for SDP exchange
//   2. STUNResolver — Resolves public IP:port via STUN
//   3. PeerConnection — Manages ICE candidates and data channels
//   4. DataChannel — Binary data transport over WebRTC data channels
//
// Integrations:
//   - SwarmCoordinator (src/core/swarm_coordinator.h)
//   - SwarmDecisionBridge (src/core/swarm_decision_bridge.h)
//
// Pattern: PatchResult-style structured results, no exceptions.
// Threading: Async I/O via IOCP, callback-driven.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// ============================================================================
// ICE Candidate
// ============================================================================
struct ICECandidate {
    std::string     foundation;
    uint32_t        component;      // 1 = RTP, 2 = RTCP
    std::string     transport;      // "udp" or "tcp"
    uint32_t        priority;
    std::string     address;        // IP address
    uint16_t        port;
    std::string     type;           // "host", "srflx", "prflx", "relay"
    std::string     relAddress;     // Related address (STUN/TURN)
    uint16_t        relPort;
};

// ============================================================================
// Session Description Protocol (SDP) - Simplified
// ============================================================================
struct SDPMessage {
    enum Type : uint8_t { Offer = 0, Answer = 1, Pranswer = 2 };
    Type                        type;
    std::string                 sdpString;
    std::vector<ICECandidate>   candidates;
    std::string                 fingerprint;    // DTLS fingerprint
    uint64_t                    sessionId;
    std::string                 peerId;
};

// ============================================================================
// Peer State
// ============================================================================
enum class PeerState : uint8_t {
    New             = 0,
    Connecting      = 1,
    Connected       = 2,
    Disconnected    = 3,
    Failed          = 4,
    Closed          = 5,
};

// ============================================================================
// Peer Info
// ============================================================================
struct PeerInfo {
    std::string     peerId;
    std::string     nodeHostname;
    PeerState       state;
    std::string     publicIP;
    uint16_t        publicPort;
    std::string     localIP;
    uint16_t        localPort;
    uint64_t        connectedAtMs;
    uint64_t        lastActivityMs;
    uint64_t        bytesSent;
    uint64_t        bytesReceived;
    uint32_t        rttMs;          // Round-trip time
    bool            behindNAT;
    bool            usingRelay;     // Using TURN relay
};

// ============================================================================
// STUN Server Configuration
// ============================================================================
struct STUNConfig {
    std::string     serverUrl;      // e.g., "stun:stun.l.google.com:19302"
    std::string     username;       // For TURN
    std::string     credential;     // For TURN
    uint32_t        timeoutMs;
};

// ============================================================================
// Signaling Result
// ============================================================================
struct SignalingResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static SignalingResult ok(const char* msg = "Success") {
        return { true, msg, 0 };
    }
    static SignalingResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// Signaling Statistics
// ============================================================================
struct SignalingStats {
    std::atomic<uint64_t> stunRequests{0};
    std::atomic<uint64_t> stunSuccesses{0};
    std::atomic<uint64_t> sdpOffersCreated{0};
    std::atomic<uint64_t> sdpAnswersReceived{0};
    std::atomic<uint64_t> peersConnected{0};
    std::atomic<uint64_t> peersDisconnected{0};
    std::atomic<uint64_t> dataChannelMessages{0};
    std::atomic<uint64_t> bytesTransferred{0};
    std::atomic<uint64_t> natTraversals{0};
    std::atomic<uint64_t> relayFallbacks{0};
};

// ============================================================================
// Callbacks
// ============================================================================
typedef void (*PeerConnectedCallback)(const PeerInfo* peer, void* userData);
typedef void (*PeerDisconnectedCallback)(const char* peerId, void* userData);
typedef void (*DataReceivedCallback)(const char* peerId, const void* data,
                                      uint32_t dataLen, void* userData);
typedef void (*SignalingEventCallback)(const char* event, const char* detail, void* userData);

// ============================================================================
// WebRTCSignaling — P2P Signaling Engine
// ============================================================================
class WebRTCSignaling {
public:
    static WebRTCSignaling& instance();

    // ---- Lifecycle ----
    SignalingResult initialize(const std::string& signalingServerUrl);
    void shutdown();
    bool isRunning() const { return m_running.load(std::memory_order_relaxed); }

    // ---- STUN/TURN Configuration ----
    void addSTUNServer(const STUNConfig& config);
    void clearSTUNServers();

    // ---- STUN Resolution ----
    // Resolve our public IP:port via STUN.
    SignalingResult resolvePublicAddress(std::string& outIP, uint16_t& outPort);

    // ---- Signaling Server Connection ----
    // Connect to the signaling server (WebSocket-based).
    SignalingResult connectToSignaling();
    void disconnectFromSignaling();
    bool isSignalingConnected() const;

    // ---- Peer Management ----
    
    // Initiate connection to a peer (creates SDP offer).
    SignalingResult connectToPeer(const std::string& peerId);

    // Disconnect from a peer.
    void disconnectPeer(const std::string& peerId);

    // Get all connected peers.
    std::vector<PeerInfo> getPeers() const;

    // Get a specific peer's info.
    bool getPeerInfo(const std::string& peerId, PeerInfo& outInfo) const;

    // Get peer count.
    uint32_t getConnectedPeerCount() const;

    // ---- Data Channel ----

    // Send binary data to a peer.
    SignalingResult sendData(const std::string& peerId, const void* data, uint32_t dataLen);

    // Send text data to a peer.
    SignalingResult sendText(const std::string& peerId, const std::string& text);

    // Broadcast data to all connected peers.
    SignalingResult broadcastData(const void* data, uint32_t dataLen);

    // ---- Swarm Integration ----
    
    // Register this node as a swarm participant on the signaling server.
    SignalingResult registerAsSwarmNode(const std::string& nodeHostname,
                                        uint32_t swarmCapabilities);

    // Discover other swarm nodes via signaling server.
    std::vector<std::string> discoverSwarmPeers();

    // ---- SDP Exchange (for manual wiring) ----
    
    // Create an SDP offer.
    SDPMessage createOffer(const std::string& peerId);

    // Set a remote SDP (offer or answer).
    SignalingResult setRemoteDescription(const std::string& peerId, const SDPMessage& sdp);

    // Add an ICE candidate for a peer.
    SignalingResult addIceCandidate(const std::string& peerId, const ICECandidate& candidate);

    // ---- Callbacks ----
    void setPeerConnectedCallback(PeerConnectedCallback cb, void* userData);
    void setPeerDisconnectedCallback(PeerDisconnectedCallback cb, void* userData);
    void setDataReceivedCallback(DataReceivedCallback cb, void* userData);
    void setSignalingEventCallback(SignalingEventCallback cb, void* userData);

    // ---- Statistics ----
    const SignalingStats& getStats() const { return m_stats; }
    void resetStats();

    // ---- JSON Serialization ----
    std::string toJson() const;
    std::string peersToJson() const;

private:
    WebRTCSignaling();
    ~WebRTCSignaling();
    WebRTCSignaling(const WebRTCSignaling&) = delete;
    WebRTCSignaling& operator=(const WebRTCSignaling&) = delete;

    // Internal: STUN binding request
    bool sendSTUNBindingRequest(const STUNConfig& server,
                                 std::string& outIP, uint16_t& outPort);

    // Internal: Signaling server WebSocket handler
    static DWORD WINAPI signalingThread(LPVOID param);
    void signalingLoop();

    // Internal: ICE candidate gathering
    void gatherLocalCandidates(std::vector<ICECandidate>& candidates);
    void gatherSTUNCandidates(std::vector<ICECandidate>& candidates);

    // Internal: SDP serialization
    std::string serializeSDP(const SDPMessage& sdp) const;
    bool parseSDP(const std::string& sdpStr, SDPMessage& outSDP) const;

    // =========================================================================
    //                         MEMBER STATE
    // =========================================================================

    mutable std::mutex                      m_mutex;
    std::atomic<bool>                       m_running;
    std::atomic<bool>                       m_shutdownRequested;
    std::atomic<bool>                       m_signalingConnected;

    // Configuration
    std::string                             m_signalingServerUrl;
    std::vector<STUNConfig>                 m_stunServers;

    // Peers
    std::unordered_map<std::string, PeerInfo>  m_peers;

    // Our public address (STUN-resolved)
    std::string                             m_publicIP;
    uint16_t                                m_publicPort;

    // Signaling thread
    HANDLE                                  m_hSignalingThread;
    SOCKET                                  m_signalingSocket;

    // Statistics
    SignalingStats                           m_stats;

    // Callbacks
    PeerConnectedCallback                   m_peerConnectedCb;
    void*                                   m_peerConnectedData;
    PeerDisconnectedCallback                m_peerDisconnectedCb;
    void*                                   m_peerDisconnectedData;
    DataReceivedCallback                    m_dataReceivedCb;
    void*                                   m_dataReceivedData;
    SignalingEventCallback                  m_signalingEventCb;
    void*                                   m_signalingEventData;
};
