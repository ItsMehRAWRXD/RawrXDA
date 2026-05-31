// llm_tls_bridge.hpp
// TLS 1.3 Secure Bridge for LLM Reverse Engineering
// Real, production-ready implementation for secure LLM communication
// 
// Features:
// 1. TLS 1.3 with perfect forward secrecy
// 2. Certificate pinning for API security
// 3. Mutual TLS (mTLS) authentication
// 4. Encrypted model weight transfer
// 5. Secure inference channel
// 6. Protocol reverse engineering hooks
// 7. Traffic analysis for LLM optimization

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdint>

#ifdef _WIN32
#define SECURITY_WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")

// ssize_t doesn't exist on Windows
#if !defined(ssize_t) && !defined(_SSIZE_T_DEFINED)
typedef SSIZE_T ssize_t;
#define _SSIZE_T_DEFINED
#endif

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace llm_tls {

// ═══════════════════════════════════════════════════════════════════════════════
// TLS Configuration
// ═══════════════════════════════════════════════════════════════════════════════

struct TLSConfig {
    std::string ca_cert_path;
    std::string client_cert_path;
    std::string client_key_path;
    std::string server_name;          // SNI
    bool verify_peer;
    bool verify_hostname;
    bool enable_session_resumption;
    bool enable_0rtt;                 // TLS 1.3 0-RTT
    int min_version;                  // TLS 1.2 = 0x0303, TLS 1.3 = 0x0304
    int max_version;
    std::vector<std::string> cipher_suites;
    std::vector<std::string> supported_groups;  // x25519, secp256r1, etc.
    int handshake_timeout_ms;
    int read_timeout_ms;
    int write_timeout_ms;
    size_t max_fragment_size;
    bool enable_ocsp_stapling;
    bool enable_ct_verification;      // Certificate Transparency
};

struct CertificateInfo {
    std::string subject;
    std::string issuer;
    std::string serial_number;
    std::string fingerprint_sha256;
    std::chrono::system_clock::time_point not_before;
    std::chrono::system_clock::time_point not_after;
    bool is_ca;
    bool is_self_signed;
    std::vector<std::string> san;     // Subject Alternative Names
    std::string public_key_type;       // RSA, ECDSA, etc.
    int public_key_bits;
};

// ═══════════════════════════════════════════════════════════════════════════════
// LLM Protocol Types
// ═══════════════════════════════════════════════════════════════════════════════

enum class LLMProvider {
    OpenAI,
    Anthropic,
    Google,
    Azure,
    AWS,
    Local,
    Custom
};

struct LLMEndpoint {
    std::string host;
    int port;
    std::string api_path;
    std::string api_key;
    LLMProvider provider;
    bool use_tls;
    bool use_mtls;
    std::string model_name;
    int max_tokens;
    float temperature;
};

struct InferenceRequest {
    std::string model;
    std::vector<std::map<std::string, std::string>> messages;
    int max_tokens;
    float temperature;
    float top_p;
    int top_k;
    std::vector<std::string> stop_sequences;
    bool stream;
    std::map<std::string, std::string> metadata;
};

struct InferenceResponse {
    std::string id;
    std::string model;
    std::vector<std::map<std::string, std::string>> choices;
    std::map<std::string, int> usage;  // prompt_tokens, completion_tokens, total_tokens
    std::string finish_reason;
    std::chrono::milliseconds latency;
    bool success;
    std::string error;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Protocol Reverse Engineering
// ═══════════════════════════════════════════════════════════════════════════════

struct ProtocolFrame {
    uint8_t type;
    uint8_t flags;
    uint16_t version;
    uint32_t length;
    std::vector<uint8_t> payload;
    std::chrono::system_clock::time_point timestamp;
    bool encrypted;
    bool compressed;
};

struct TrafficAnalysis {
    size_t total_bytes_sent;
    size_t total_bytes_received;
    size_t request_count;
    size_t response_count;
    std::map<uint8_t, int> frame_type_counts;
    std::map<std::string, int> endpoint_counts;
    float avg_latency_ms;
    float p50_latency_ms;
    float p99_latency_ms;
    std::vector<ProtocolFrame> captured_frames;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Secure Channel
// ═══════════════════════════════════════════════════════════════════════════════

class SecureChannel {
public:
    SecureChannel();
    ~SecureChannel();
    
    // Connection management
    bool connect(const std::string& host, int port, const TLSConfig& config);
    bool connect_mtls(const std::string& host, int port, 
                      const TLSConfig& config,
                      const std::string& client_cert,
                      const std::string& client_key);
    void disconnect();
    bool is_connected() const;
    
    // Data transfer (renamed to avoid Windows macro conflicts)
    ssize_t send_data(const void* data, size_t len);
    ssize_t send_data(const std::vector<uint8_t>& data);
    ssize_t receive_data(void* buffer, size_t max_len);
    std::vector<uint8_t> receive_data(size_t max_len);
    
    // Streaming
    bool send_stream(const std::vector<uint8_t>& data);
    std::vector<uint8_t> receive_stream();
    
    // Certificate info
    CertificateInfo get_peer_certificate() const;
    CertificateInfo get_local_certificate() const;
    
    // TLS info
    std::string get_cipher_suite() const;
    std::string get_protocol_version() const;
    bool is_session_resumed() const;
    
    // Raw socket access (for reverse engineering)
    int get_socket_fd() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::mutex mutex_;
    std::atomic<bool> connected_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// LLM TLS Bridge
// ═══════════════════════════════════════════════════════════════════════════════

class LLMTLSBridge {
public:
    LLMTLSBridge();
    ~LLMTLSBridge();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Initialization
    // ═══════════════════════════════════════════════════════════════════════════
    
    bool initialize(const TLSConfig& config);
    void shutdown();
    bool is_initialized() const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // LLM Connection
    // ═══════════════════════════════════════════════════════════════════════════
    
    bool connect_endpoint(const LLMEndpoint& endpoint);
    bool connect_openai(const std::string& api_key);
    bool connect_anthropic(const std::string& api_key);
    bool connect_google(const std::string& api_key);
    bool connect_azure(const std::string& api_key, const std::string& endpoint);
    bool connect_local(const std::string& host, int port);
    
    void disconnect();
    bool is_connected() const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Inference
    // ═══════════════════════════════════════════════════════════════════════════
    
    InferenceResponse infer(const InferenceRequest& request);
    void infer_stream(const InferenceRequest& request,
                      std::function<void(const std::string&)> callback);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Model Weight Transfer (Secure)
    // ═══════════════════════════════════════════════════════════════════════════
    
    bool download_model(const std::string& model_id, const std::string& output_path);
    bool upload_model(const std::string& model_path, const std::string& model_id);
    
    // Progress callbacks
    void set_progress_callback(std::function<void(float)> callback);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Protocol Reverse Engineering
    // ═══════════════════════════════════════════════════════════════════════════
    
    void enable_traffic_capture(bool enable);
    TrafficAnalysis get_traffic_analysis() const;
    std::vector<ProtocolFrame> get_captured_frames() const;
    
    // Protocol analysis
    std::map<std::string, std::string> analyze_openai_protocol();
    std::map<std::string, std::string> analyze_anthropic_protocol();
    std::map<std::string, std::string> analyze_google_protocol();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Certificate Management
    // ═══════════════════════════════════════════════════════════════════════════
    
    bool load_ca_certificates(const std::string& path);
    bool load_client_certificate(const std::string& cert_path, const std::string& key_path);
    bool pin_certificate(const std::string& fingerprint);
    bool verify_certificate(const CertificateInfo& cert);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Statistics
    // ═══════════════════════════════════════════════════════════════════════════
    
    std::map<std::string, std::string> get_stats() const;
    size_t get_bytes_sent() const;
    size_t get_bytes_received() const;
    int get_request_count() const;
    
private:
    std::unique_ptr<SecureChannel> channel_;
    TLSConfig config_;
    LLMEndpoint current_endpoint_;
    
    std::mutex mutex_;
    std::atomic<bool> initialized_;
    std::atomic<bool> traffic_capture_enabled_;
    
    // Statistics
    std::atomic<size_t> bytes_sent_;
    std::atomic<size_t> bytes_received_;
    std::atomic<int> request_count_;
    
    // Traffic capture
    std::vector<ProtocolFrame> captured_frames_;
    TrafficAnalysis traffic_analysis_;
    
    // Callbacks
    std::function<void(float)> progress_callback_;
    
    // Internal helpers
    std::string build_request(const InferenceRequest& request);
    InferenceResponse parse_response(const std::string& response);
    std::vector<uint8_t> encrypt_payload(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decrypt_payload(const std::vector<uint8_t>& data);
    ProtocolFrame parse_frame(const std::vector<uint8_t>& data);
    std::vector<uint8_t> serialize_frame(const ProtocolFrame& frame);
};

// ═══════════════════════════════════════════════════════════════════════════════
// Utility Functions
// ═══════════════════════════════════════════════════════════════════════════════

// Base64 encoding/decoding
std::string base64_encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> base64_decode(const std::string& encoded);

// Hex encoding/decoding
std::string hex_encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> hex_decode(const std::string& hex);

// SHA256 hash
std::vector<uint8_t> sha256(const std::vector<uint8_t>& data);
std::string sha256_hex(const std::string& data);

// Certificate fingerprint
std::string get_certificate_fingerprint(const std::string& cert_path);

// TLS version string
std::string tls_version_string(int version);

} // namespace llm_tls
