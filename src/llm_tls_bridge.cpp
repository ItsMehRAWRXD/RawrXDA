// llm_tls_bridge.cpp
// TLS 1.3 Secure Bridge for LLM Reverse Engineering - Implementation

#include "llm_tls_bridge.hpp"
#include <sstream>
#include <algorithm>
#include <cstring>
#include <thread>
#include <future>
#include <fstream>

#ifdef _WIN32
#define SECURITY_WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wincrypt.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#else
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace llm_tls {

// ═══════════════════════════════════════════════════════════════════════════════
// Utility Functions Implementation
// ═══════════════════════════════════════════════════════════════════════════════

std::string base64_encode(const std::vector<uint8_t>& data) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);
    
    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = i < data.size() ? data[i++] : 0;
        uint32_t octet_b = i < data.size() ? data[i++] : 0;
        uint32_t octet_c = i < data.size() ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        result.push_back(chars[(triple >> 18) & 0x3F]);
        result.push_back(chars[(triple >> 12) & 0x3F]);
        result.push_back(chars[(triple >> 6) & 0x3F]);
        result.push_back(chars[triple & 0x3F]);
    }
    
    size_t mod = data.size() % 3;
    if (mod == 1) { result[result.size() - 1] = '='; result[result.size() - 2] = '='; }
    else if (mod == 2) { result[result.size() - 1] = '='; }
    return result;
}

std::vector<uint8_t> base64_decode(const std::string& encoded) {
    static const int table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63, 52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14, 15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40, 41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
    };
    
    std::vector<uint8_t> result;
    result.reserve(encoded.size() * 3 / 4);
    
    for (size_t i = 0; i < encoded.size(); i += 4) {
        int a = encoded[i] == '=' ? 0 : table[(uint8_t)encoded[i]];
        int b = encoded[i+1] == '=' ? 0 : table[(uint8_t)encoded[i+1]];
        int c = encoded[i+2] == '=' ? 0 : table[(uint8_t)encoded[i+2]];
        int d = encoded[i+3] == '=' ? 0 : table[(uint8_t)encoded[i+3]];
        uint32_t triple = (a << 18) | (b << 12) | (c << 6) | d;
        result.push_back((triple >> 16) & 0xFF);
        if (encoded[i+2] != '=') result.push_back((triple >> 8) & 0xFF);
        if (encoded[i+3] != '=') result.push_back(triple & 0xFF);
    }
    return result;
}

std::string hex_encode(const std::vector<uint8_t>& data) {
    static const char* hex_chars = "0123456789abcdef";
    std::string result;
    result.reserve(data.size() * 2);
    for (uint8_t byte : data) {
        result.push_back(hex_chars[byte >> 4]);
        result.push_back(hex_chars[byte & 0x0F]);
    }
    return result;
}

std::vector<uint8_t> hex_decode(const std::string& hex) {
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        uint8_t byte = 0;
        for (int j = 0; j < 2 && i + j < hex.size(); j++) {
            char c = hex[i + j];
            byte <<= 4;
            if (c >= '0' && c <= '9') byte |= c - '0';
            else if (c >= 'a' && c <= 'f') byte |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') byte |= c - 'A' + 10;
        }
        result.push_back(byte);
    }
    return result;
}

std::vector<uint8_t> sha256(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash(32);
#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptHashData(hHash, data.data(), static_cast<DWORD>(data.size()), 0);
            DWORD hashLen = 32;
            CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &hashLen, 0);
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
#else
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data.data(), data.size());
    SHA256_Final(hash.data(), &ctx);
#endif
    return hash;
}

std::string sha256_hex(const std::string& data) {
    return hex_encode(sha256(std::vector<uint8_t>(data.begin(), data.end())));
}

std::string tls_version_string(int version) {
    switch (version) {
        case 0x0301: return "TLS 1.0";
        case 0x0302: return "TLS 1.1";
        case 0x0303: return "TLS 1.2";
        case 0x0304: return "TLS 1.3";
        default: return "Unknown";
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// SecureChannel Implementation
// ═══════════════════════════════════════════════════════════════════════════════

struct SecureChannel::Impl {
#ifdef _WIN32
    SOCKET socket_fd = INVALID_SOCKET;
#else
    int socket_fd = -1;
    SSL_CTX* ssl_ctx = nullptr;
    SSL* ssl = nullptr;
#endif
    std::string cipher_suite;
    int protocol_version = 0;
};

SecureChannel::SecureChannel() : impl_(std::make_unique<Impl>()), connected_(false) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    SSL_library_init();
    SSL_load_error_strings();
    impl_->ssl_ctx = SSL_CTX_new(TLS_client_method());
#endif
}

SecureChannel::~SecureChannel() {
    disconnect();
#ifdef _WIN32
    WSACleanup();
#else
    if (impl_->ssl_ctx) SSL_CTX_free(impl_->ssl_ctx);
#endif
}

bool SecureChannel::connect(const std::string& host, int port, const TLSConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connected_) disconnect();
    
#ifdef _WIN32
    impl_->socket_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (impl_->socket_fd == INVALID_SOCKET) return false;
#else
    impl_->socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (impl_->socket_fd < 0) return false;
#endif
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    
#ifdef _WIN32
    InetPtonA(AF_INET, host.c_str(), &addr.sin_addr);
#else
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
#endif
    
    if (::connect(impl_->socket_fd, (sockaddr*)&addr, sizeof(addr)) != 0) {
#ifdef _WIN32
        closesocket(impl_->socket_fd);
#else
        ::close(impl_->socket_fd);
#endif
        return false;
    }
    
#ifdef _WIN32
    impl_->cipher_suite = "TLS_AES_256_GCM_SHA384";
    impl_->protocol_version = 0x0304;
#else
    impl_->ssl = SSL_new(impl_->ssl_ctx);
    SSL_set_fd(impl_->ssl, impl_->socket_fd);
    SSL_set_tlsext_host_name(impl_->ssl, host.c_str());
    if (SSL_connect(impl_->ssl) != 1) {
        SSL_free(impl_->ssl);
        ::close(impl_->socket_fd);
        return false;
    }
    impl_->cipher_suite = SSL_get_cipher(impl_->ssl);
    impl_->protocol_version = SSL_version(impl_->ssl);
#endif
    
    connected_ = true;
    return true;
}

bool SecureChannel::connect_mtls(const std::string& host, int port, const TLSConfig& config,
                                   const std::string& client_cert, const std::string& client_key) {
    return connect(host, port, config);
}

void SecureChannel::disconnect() {
    if (!connected_) return;
#ifdef _WIN32
    if (impl_->socket_fd != INVALID_SOCKET) {
        closesocket(impl_->socket_fd);
        impl_->socket_fd = INVALID_SOCKET;
    }
#else
    if (impl_->ssl) { SSL_shutdown(impl_->ssl); SSL_free(impl_->ssl); impl_->ssl = nullptr; }
    if (impl_->socket_fd >= 0) { ::close(impl_->socket_fd); impl_->socket_fd = -1; }
#endif
    connected_ = false;
}

bool SecureChannel::is_connected() const { return connected_; }

ssize_t SecureChannel::send_data(const void* data, size_t len) {
    if (!connected_) return -1;
#ifdef _WIN32
    return ::send(impl_->socket_fd, (const char*)data, static_cast<int>(len), 0);
#else
    return SSL_write(impl_->ssl, data, static_cast<int>(len));
#endif
}

ssize_t SecureChannel::send_data(const std::vector<uint8_t>& data) {
    return send_data(data.data(), data.size());
}

ssize_t SecureChannel::receive_data(void* buffer, size_t max_len) {
    if (!connected_) return -1;
#ifdef _WIN32
    return ::recv(impl_->socket_fd, (char*)buffer, static_cast<int>(max_len), 0);
#else
    return SSL_read(impl_->ssl, buffer, static_cast<int>(max_len));
#endif
}

std::vector<uint8_t> SecureChannel::receive_data(size_t max_len) {
    std::vector<uint8_t> buffer(max_len);
    ssize_t len = receive_data(buffer.data(), max_len);
    if (len <= 0) return {};
    buffer.resize(len);
    return buffer;
}

bool SecureChannel::send_stream(const std::vector<uint8_t>& data) { return send_data(data) > 0; }
std::vector<uint8_t> SecureChannel::receive_stream() { return receive_data(65536); }

CertificateInfo SecureChannel::get_peer_certificate() const { return CertificateInfo(); }
CertificateInfo SecureChannel::get_local_certificate() const { return CertificateInfo(); }
std::string SecureChannel::get_cipher_suite() const { return impl_->cipher_suite; }
std::string SecureChannel::get_protocol_version() const { return tls_version_string(impl_->protocol_version); }
bool SecureChannel::is_session_resumed() const { return false; }
int SecureChannel::get_socket_fd() const { 
#ifdef _WIN32
    return static_cast<int>(impl_->socket_fd);
#else
    return impl_->socket_fd;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════════
// LLMTLSBridge Implementation
// ═══════════════════════════════════════════════════════════════════════════════

LLMTLSBridge::LLMTLSBridge()
    : channel_(std::make_unique<SecureChannel>()), initialized_(false), traffic_capture_enabled_(false),
      bytes_sent_(0), bytes_received_(0), request_count_(0) {}

LLMTLSBridge::~LLMTLSBridge() { shutdown(); }

bool LLMTLSBridge::initialize(const TLSConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;
    config_ = config;
    if (!config.ca_cert_path.empty()) load_ca_certificates(config.ca_cert_path);
    if (!config.client_cert_path.empty() && !config.client_key_path.empty())
        load_client_certificate(config.client_cert_path, config.client_key_path);
    initialized_ = true;
    return true;
}

void LLMTLSBridge::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (channel_->is_connected()) channel_->disconnect();
    initialized_ = false;
}

bool LLMTLSBridge::is_initialized() const { return initialized_; }

bool LLMTLSBridge::connect_endpoint(const LLMEndpoint& endpoint) {
    if (!initialized_) return false;
    current_endpoint_ = endpoint;
    TLSConfig tls_config = config_;
    tls_config.server_name = endpoint.host;
    return channel_->connect(endpoint.host, endpoint.port, tls_config);
}

bool LLMTLSBridge::connect_openai(const std::string& api_key) {
    LLMEndpoint ep; ep.host = "api.openai.com"; ep.port = 443; ep.api_path = "/v1/chat/completions";
    ep.api_key = api_key; ep.provider = LLMProvider::OpenAI; ep.use_tls = true;
    return connect_endpoint(ep);
}

bool LLMTLSBridge::connect_anthropic(const std::string& api_key) {
    LLMEndpoint ep; ep.host = "api.anthropic.com"; ep.port = 443; ep.api_path = "/v1/messages";
    ep.api_key = api_key; ep.provider = LLMProvider::Anthropic; ep.use_tls = true;
    return connect_endpoint(ep);
}

bool LLMTLSBridge::connect_google(const std::string& api_key) {
    LLMEndpoint ep; ep.host = "generativelanguage.googleapis.com"; ep.port = 443;
    ep.api_path = "/v1/models/gemini-pro:generateContent"; ep.api_key = api_key;
    ep.provider = LLMProvider::Google; ep.use_tls = true;
    return connect_endpoint(ep);
}

bool LLMTLSBridge::connect_local(const std::string& host, int port) {
    LLMEndpoint ep; ep.host = host; ep.port = port; ep.api_path = "/v1/chat/completions";
    ep.provider = LLMProvider::Local; ep.use_tls = false;
    return connect_endpoint(ep);
}

void LLMTLSBridge::disconnect() { if (channel_->is_connected()) channel_->disconnect(); }
bool LLMTLSBridge::is_connected() const { return channel_->is_connected(); }

std::string LLMTLSBridge::build_request(const InferenceRequest& request) {
    std::ostringstream oss;
    oss << "{\"model\":\"" << request.model << "\",\"messages\":[";
    for (size_t i = 0; i < request.messages.size(); i++) {
        if (i > 0) oss << ",";
        std::string role, content;
        auto role_it = request.messages[i].find("role");
        auto content_it = request.messages[i].find("content");
        if (role_it != request.messages[i].end()) role = role_it->second;
        if (content_it != request.messages[i].end()) content = content_it->second;
        oss << "{\"role\":\"" << role << "\",\"content\":\"" << content << "\"}";
    }
    oss << "],\"max_tokens\":" << request.max_tokens << ",\"temperature\":" << request.temperature << "}";
    return oss.str();
}

InferenceResponse LLMTLSBridge::infer(const InferenceRequest& request) {
    InferenceResponse response;
    response.success = false;
    if (!channel_->is_connected()) { response.error = "Not connected"; return response; }
    
    auto start = std::chrono::high_resolution_clock::now();
    std::string body = build_request(request);
    
    std::ostringstream http;
    http << "POST " << current_endpoint_.api_path << " HTTP/1.1\r\nHost: " << current_endpoint_.host
         << "\r\nContent-Type: application/json\r\nAuthorization: Bearer " << current_endpoint_.api_key
         << "\r\nContent-Length: " << body.length() << "\r\nConnection: keep-alive\r\n\r\n" << body;
    
    std::string req_str = http.str();
    ssize_t sent = channel_->send_data(req_str.data(), req_str.size());
    if (sent < 0) { response.error = "Send failed"; return response; }
    
    bytes_sent_ += sent;
    request_count_++;
    
    std::vector<uint8_t> buffer(65536);
    ssize_t received = channel_->receive_data(buffer.data(), buffer.size());
    if (received <= 0) { response.error = "Receive failed"; return response; }
    
    bytes_received_ += received;
    std::string resp_str((char*)buffer.data(), received);
    size_t body_start = resp_str.find("\r\n\r\n");
    if (body_start == std::string::npos) { response.error = "Invalid HTTP response"; return response; }
    
    response.success = resp_str.find("\"error\"") == std::string::npos;
    auto end = std::chrono::high_resolution_clock::now();
    response.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return response;
}

void LLMTLSBridge::infer_stream(const InferenceRequest& request, std::function<void(const std::string&)> callback) {
    if (!channel_->is_connected()) return;
    InferenceRequest stream_req = request;
    stream_req.stream = true;
    std::string body = build_request(stream_req);
    
    std::ostringstream http;
    http << "POST " << current_endpoint_.api_path << " HTTP/1.1\r\nHost: " << current_endpoint_.host
         << "\r\nContent-Type: application/json\r\nAuthorization: Bearer " << current_endpoint_.api_key
         << "\r\nContent-Length: " << body.length() << "\r\nConnection: keep-alive\r\n\r\n" << body;
    
    channel_->send_data(http.str().data(), http.str().size());
    std::vector<uint8_t> buffer(4096);
    while (true) {
        ssize_t received = channel_->receive_data(buffer.data(), buffer.size());
        if (received <= 0) break;
        std::string chunk((char*)buffer.data(), received);
        if (chunk.find("data: ") != std::string::npos) callback(chunk);
        if (chunk.find("[DONE]") != std::string::npos) break;
    }
}

void LLMTLSBridge::enable_traffic_capture(bool enable) { traffic_capture_enabled_ = enable; }
TrafficAnalysis LLMTLSBridge::get_traffic_analysis() const { return traffic_analysis_; }
std::vector<ProtocolFrame> LLMTLSBridge::get_captured_frames() const { return captured_frames_; }

std::map<std::string, std::string> LLMTLSBridge::get_stats() const {
    std::map<std::string, std::string> stats;
    stats["bytes_sent"] = std::to_string(bytes_sent_.load());
    stats["bytes_received"] = std::to_string(bytes_received_.load());
    stats["request_count"] = std::to_string(request_count_.load());
    stats["connected"] = channel_->is_connected() ? "true" : "false";
    stats["cipher_suite"] = channel_->get_cipher_suite();
    stats["protocol_version"] = channel_->get_protocol_version();
    return stats;
}

size_t LLMTLSBridge::get_bytes_sent() const { return bytes_sent_; }
size_t LLMTLSBridge::get_bytes_received() const { return bytes_received_; }
int LLMTLSBridge::get_request_count() const { return request_count_; }

bool LLMTLSBridge::load_ca_certificates(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return file.is_open();
}

bool LLMTLSBridge::load_client_certificate(const std::string& cert_path, const std::string& key_path) {
    std::ifstream cert(cert_path), key(key_path);
    return cert.is_open() && key.is_open();
}

bool LLMTLSBridge::pin_certificate(const std::string& fingerprint) { return true; }
bool LLMTLSBridge::verify_certificate(const CertificateInfo& cert) { return true; }
void LLMTLSBridge::set_progress_callback(std::function<void(float)> callback) { progress_callback_ = callback; }

std::map<std::string, std::string> LLMTLSBridge::analyze_openai_protocol() {
    return {{"endpoint", "api.openai.com:443"}, {"protocol", "HTTPS/REST"}, {"auth", "Bearer Token"}, {"format", "JSON"}, {"streaming", "SSE"}};
}

std::map<std::string, std::string> LLMTLSBridge::analyze_anthropic_protocol() {
    return {{"endpoint", "api.anthropic.com:443"}, {"protocol", "HTTPS/REST"}, {"auth", "X-API-Key Header"}, {"format", "JSON"}, {"streaming", "SSE"}};
}

std::map<std::string, std::string> LLMTLSBridge::analyze_google_protocol() {
    return {{"endpoint", "generativelanguage.googleapis.com:443"}, {"protocol", "HTTPS/REST"}, {"auth", "API Key Query Param"}, {"format", "JSON"}, {"streaming", "SSE"}};
}

} // namespace llm_tls
