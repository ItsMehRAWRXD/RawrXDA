#include "http_server_enhancements.h"
#include <httplib.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <zlib.h>

namespace HTTPEnhancements {

// ============================================================================
// AuthenticationManager Implementation
// ============================================================================

bool AuthenticationManager::UserSession::is_valid() const {
    return std::chrono::steady_clock::now() < expires_at;
}

AuthenticationManager::AuthenticationManager(const std::string& secret_key)
    : m_secret_key(secret_key) {
}

std::string AuthenticationManager::GenerateToken(const std::string& user_id, int ttl_seconds) {
    auto now = std::chrono::steady_clock::now();
    auto expires = now + std::chrono::seconds(ttl_seconds);
    
    // Create JWT payload
    std::stringstream payload;
    payload << "{\"sub\":\"" << user_id << "\","
            << "\"iat\":" << std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count() << ","
            << "\"exp\":" << std::chrono::duration_cast<std::chrono::seconds>(
                expires.time_since_epoch()).count() << "}";
    
    std::string token = EncodeJWT(payload.str());
    
    // Store session
    {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);
        UserSession session;
        session.user_id = user_id;
        session.token = token;
        session.created_at = now;
        session.expires_at = expires;
        m_active_sessions[token] = session;
    }
    
    return token;
}

bool AuthenticationManager::ValidateToken(const std::string& token, UserSession& session_out) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    
    // Check if revoked
    if (m_revoked_tokens.find(token) != m_revoked_tokens.end()) {
        return false;
    }
    
    // Find session
    auto it = m_active_sessions.find(token);
    if (it == m_active_sessions.end()) {
        return false;
    }
    
    // Check expiration
    if (!it->second.is_valid()) {
        m_active_sessions.erase(it);
        return false;
    }
    
    session_out = it->second;
    return true;
}

bool AuthenticationManager::AuthenticateRequest(const httplib::Request& req, httplib::Response& res) {
    // Extract token from Authorization header
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
        res.status = 401;
        res.set_content("{\"error\":\"Missing authorization header\"}", "application/json");
        return false;
    }
    
    // Check for "Bearer " prefix
    if (auth_header.substr(0, 7) != "Bearer ") {
        res.status = 401;
        res.set_content("{\"error\":\"Invalid authorization format\"}", "application/json");
        return false;
    }
    
    std::string token = auth_header.substr(7);
    UserSession session;
    
    if (!ValidateToken(token, session)) {
        res.status = 401;
        res.set_content("{\"error\":\"Invalid or expired token\"}", "application/json");
        return false;
    }
    
    return true;
}

void AuthenticationManager::RevokeToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    m_revoked_tokens[token] = true;
    m_active_sessions.erase(token);
}

std::string AuthenticationManager::EncodeJWT(const std::string& payload) {
    // Simplified JWT encoding - in production use a proper JWT library
    std::string header = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
    
    // Base64 encode (simplified)
    auto base64_encode = [](const std::string& input) -> std::string {
        static const char* base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string ret;
        int i = 0;
        unsigned char array_3[3];
        unsigned char array_4[4];
        
        for (char c : input) {
            array_3[i++] = c;
            if (i == 3) {
                array_4[0] = (array_3[0] & 0xfc) >> 2;
                array_4[1] = ((array_3[0] & 0x03) << 4) + ((array_3[1] & 0xf0) >> 4);
                array_4[2] = ((array_3[1] & 0x0f) << 2) + ((array_3[2] & 0xc0) >> 6);
                array_4[3] = array_3[2] & 0x3f;
                for (i = 0; i < 4; i++) ret += base64_chars[array_4[i]];
                i = 0;
            }
        }
        if (i) {
            for (int j = i; j < 3; j++) array_3[j] = '\0';
            array_4[0] = (array_3[0] & 0xfc) >> 2;
            array_4[1] = ((array_3[0] & 0x03) << 4) + ((array_3[1] & 0xf0) >> 4);
            array_4[2] = ((array_3[1] & 0x0f) << 2) + ((array_3[2] & 0xc0) >> 6);
            for (int j = 0; j < i + 1; j++) ret += base64_chars[array_4[j]];
            while (i++ < 3) ret += '=';
        }
        return ret;
    };
    
    std::string encoded_header = base64_encode(header);
    std::string encoded_payload = base64_encode(payload);
    
    // HMAC signature
    std::string to_sign = encoded_header + "." + encoded_payload;
    unsigned char hmac_result[EVP_MAX_MD_SIZE];
    unsigned int hmac_len;
    
    HMAC(EVP_sha256(), m_secret_key.c_str(), m_secret_key.length(),
         (unsigned char*)to_sign.c_str(), to_sign.length(),
         hmac_result, &hmac_len);
    
    std::string signature(reinterpret_cast<char*>(hmac_result), hmac_len);
    std::string encoded_signature = base64_encode(signature);
    
    return to_sign + "." + encoded_signature;
}

bool AuthenticationManager::DecodeJWT(const std::string& token, std::string& payload_out) {
    // Simplified - in production use proper JWT library
    auto parts = std::vector<std::string>();
    std::stringstream ss(token);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }
    
    if (parts.size() != 3) return false;
    
    // For now, just return the payload part (would need Base64 decode in production)
    payload_out = parts[1];
    return true;
}

std::string AuthenticationManager::GenerateOAuth2Token(const std::string& user_id, const std::string& scope) {
    // OAuth2 token with scope
    std::stringstream payload;
    payload << "{\"sub\":\"" << user_id << "\","
            << "\"scope\":\"" << scope << "\","
            << "\"iat\":" << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count() << "}";
    
    return EncodeJWT(payload.str());
}

bool AuthenticationManager::ValidateOAuth2Token(const std::string& token, 
                                               std::string& user_id_out, 
                                               std::string& scope_out) {
    UserSession session;
    if (!ValidateToken(token, session)) {
        return false;
    }
    
    user_id_out = session.user_id;
    // Extract scope from metadata if present
    auto it = session.metadata.find("scope");
    if (it != session.metadata.end()) {
        scope_out = it->second;
    }
    
    return true;
}

// ============================================================================
// CompressionManager Implementation
// ============================================================================

CompressionManager::CompressionManager() {
}

bool CompressionManager::CompressResponse(const std::string& content, Algorithm algo, std::string& compressed_out) {
    if (content.size() < m_min_compression_size) {
        compressed_out = content;
        return false;  // Not worth compressing
    }
    
    switch (algo) {
        case Algorithm::Gzip:
            return CompressGzip(content, compressed_out);
        case Algorithm::Brotli:
            return CompressBrotli(content, compressed_out);
        default:
            compressed_out = content;
            return false;
    }
}

void CompressionManager::ApplyCompression(const httplib::Request& req, httplib::Response& res) {
    std::string accept_encoding = req.get_header_value("Accept-Encoding");
    Algorithm algo = ChooseAlgorithm(accept_encoding);
    
    if (algo == Algorithm::None) {
        return;  // Client doesn't support compression
    }
    
    std::string original_content = res.body;
    std::string compressed;
    
    if (CompressResponse(original_content, algo, compressed)) {
        res.body = compressed;
        res.set_header("Content-Encoding", algo == Algorithm::Gzip ? "gzip" : "br");
        res.set_header("Vary", "Accept-Encoding");
    }
}

void CompressionManager::SetCompressionLevel(int level) {
    m_compression_level = std::max(1, std::min(9, level));
}

void CompressionManager::SetMinCompressionSize(size_t bytes) {
    m_min_compression_size = bytes;
}

CompressionManager::Algorithm CompressionManager::ChooseAlgorithm(const std::string& accept_encoding) {
    if (accept_encoding.find("br") != std::string::npos) {
        return Algorithm::Brotli;
    } else if (accept_encoding.find("gzip") != std::string::npos) {
        return Algorithm::Gzip;
    }
    return Algorithm::None;
}

bool CompressionManager::CompressGzip(const std::string& input, std::string& output) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (deflateInit2(&zs, m_compression_level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }
    
    zs.next_in = (Bytef*)input.data();
    zs.avail_in = input.size();
    
    int ret;
    char outbuffer[32768];
    std::string compressed;
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        
        ret = deflate(&zs, Z_FINISH);
        
        if (compressed.size() < zs.total_out) {
            compressed.append(outbuffer, zs.total_out - compressed.size());
        }
    } while (ret == Z_OK);
    
    deflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        return false;
    }
    
    output = compressed;
    return true;
}

bool CompressionManager::CompressBrotli(const std::string& input, std::string& output) {
    // Brotli compression would require brotli library
    // For now, fall back to gzip or return uncompressed
    return CompressGzip(input, output);
}

// ============================================================================
// CacheManager Implementation
// ============================================================================

CacheManager::CacheManager() {
}

std::string CacheManager::GenerateETag(const std::string& content) {
    return ComputeSHA256(content).substr(0, 16);  // Use first 16 chars of SHA256
}

bool CacheManager::IsETagValid(const std::string& client_etag, const std::string& content) {
    std::string current_etag = GenerateETag(content);
    return client_etag == current_etag;
}

bool CacheManager::HandleCachedResponse(const httplib::Request& req, httplib::Response& res) {
    // Check If-None-Match header
    std::string if_none_match = req.get_header_value("If-None-Match");
    if (if_none_match.empty()) {
        return false;  // No ETag from client, proceed with fresh response
    }
    
    // Get cache entry for this request path
    CacheEntry entry;
    if (!GetCache(req.path, entry)) {
        return false;  // Not in cache
    }
    
    // Check if ETag matches
    if (if_none_match == entry.etag) {
        res.status = 304;  // Not Modified
        res.set_header("ETag", entry.etag);
        return true;  // Client can use cached version
    }
    
    return false;
}

void CacheManager::StoreCache(const std::string& key, const std::string& content,
                             int max_age_seconds, const std::string& content_type) {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    
    CacheEntry entry;
    entry.etag = GenerateETag(content);
    entry.content = content;
    entry.created_at = std::chrono::steady_clock::now();
    entry.max_age = std::chrono::seconds(max_age_seconds);
    entry.content_type = content_type;
    
    m_cache[key] = entry;
}

bool CacheManager::GetCache(const std::string& key, CacheEntry& entry_out) {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        return false;
    }
    
    // Check if expired
    auto age = std::chrono::steady_clock::now() - it->second.created_at;
    if (age > it->second.max_age) {
        m_cache.erase(it);
        return false;
    }
    
    entry_out = it->second;
    return true;
}

void CacheManager::SetCacheHeaders(httplib::Response& res, const CacheEntry& entry) {
    res.set_header("ETag", entry.etag);
    res.set_header("Cache-Control", "max-age=" + std::to_string(entry.max_age.count()));
    
    // Add Last-Modified header
    auto time_t = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now() - 
        (std::chrono::steady_clock::now() - entry.created_at)
    );
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%a, %d %b %Y %H:%M:%S GMT");
    res.set_header("Last-Modified", ss.str());
}

void CacheManager::ClearExpiredEntries() {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    
    auto now = std::chrono::steady_clock::now();
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        auto age = now - it->second.created_at;
        if (age > it->second.max_age) {
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

std::string CacheManager::ComputeSHA256(const std::string& content) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)content.c_str(), content.length(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

// ============================================================================
// RateLimiter Implementation
// ============================================================================

RateLimiter::RateLimiter(int max_requests_per_minute) {
    m_default_limit.max_requests = max_requests_per_minute;
    m_default_limit.window = std::chrono::seconds(60);
}

bool RateLimiter::CheckRateLimit(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(m_limiter_mutex);
    
    auto& state = m_clients[client_ip];
    
    // Check if blocked
    if (state.is_blocked) {
        if (std::chrono::steady_clock::now() < state.blocked_until) {
            return false;  // Still blocked
        }
        // Unblock
        state.is_blocked = false;
        state.request_times.clear();
    }
    
    // Clean up old requests
    CleanupOldRequests(state);
    
    // Check rate limit
    if (state.request_times.size() >= (size_t)m_default_limit.max_requests) {
        // Block for 1 hour
        state.is_blocked = true;
        state.blocked_until = std::chrono::steady_clock::now() + std::chrono::hours(1);
        return false;
    }
    
    // Record this request
    state.request_times.push_back(std::chrono::steady_clock::now());
    return true;
}

bool RateLimiter::RateLimitRequest(const httplib::Request& req, httplib::Response& res) {
    std::string client_ip = GetClientIP(req);
    
    if (!CheckRateLimit(client_ip)) {
        res.status = 429;  // Too Many Requests
        res.set_header("Retry-After", "3600");
        res.set_content("{\"error\":\"Rate limit exceeded\"}", "application/json");
        return false;
    }
    
    // Add rate limit headers
    int remaining = m_default_limit.max_requests - GetRequestCount(client_ip);
    res.set_header("X-RateLimit-Limit", std::to_string(m_default_limit.max_requests));
    res.set_header("X-RateLimit-Remaining", std::to_string(remaining));
    res.set_header("X-RateLimit-Reset", std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count() + m_default_limit.window.count()
    ));
    
    return true;
}

void RateLimiter::SetEndpointRateLimit(const std::string& endpoint, int max_requests, int window_seconds) {
    std::lock_guard<std::mutex> lock(m_limiter_mutex);
    RateLimit limit;
    limit.max_requests = max_requests;
    limit.window = std::chrono::seconds(window_seconds);
    m_endpoint_limits[endpoint] = limit;
}

void RateLimiter::BlockIP(const std::string& ip, int duration_seconds) {
    std::lock_guard<std::mutex> lock(m_limiter_mutex);
    auto& state = m_clients[ip];
    state.is_blocked = true;
    state.blocked_until = std::chrono::steady_clock::now() + std::chrono::seconds(duration_seconds);
}

void RateLimiter::UnblockIP(const std::string& ip) {
    std::lock_guard<std::mutex> lock(m_limiter_mutex);
    auto it = m_clients.find(ip);
    if (it != m_clients.end()) {
        it->second.is_blocked = false;
    }
}

int RateLimiter::GetRequestCount(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(m_limiter_mutex);
    auto it = m_clients.find(client_ip);
    if (it == m_clients.end()) return 0;
    return it->second.request_times.size();
}

void RateLimiter::ResetRateLimit(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(m_limiter_mutex);
    m_clients.erase(client_ip);
}

void RateLimiter::CleanupOldRequests(ClientState& state) {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - m_default_limit.window;
    
    state.request_times.erase(
        std::remove_if(state.request_times.begin(), state.request_times.end(),
                      [cutoff](const auto& time) { return time < cutoff; }),
        state.request_times.end()
    );
}

std::string RateLimiter::GetClientIP(const httplib::Request& req) {
    // Check X-Forwarded-For header first (for proxies)
    std::string forwarded = req.get_header_value("X-Forwarded-For");
    if (!forwarded.empty()) {
        // Take first IP in comma-separated list
        size_t comma = forwarded.find(',');
        return comma != std::string::npos ? forwarded.substr(0, comma) : forwarded;
    }
    
    // Fall back to remote address
    return req.remote_addr;
}

// ============================================================================
// MetricsCollector Implementation
// ============================================================================

MetricsCollector::MetricsCollector() {
    m_start_time = std::chrono::steady_clock::now();
}

void MetricsCollector::RecordRequest(const std::string& endpoint, const std::string& method,
                                    int status_code, uint64_t latency_ms, 
                                    size_t bytes_sent, size_t bytes_received) {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    
    std::string key = method + " " + endpoint;
    auto& metrics = m_endpoint_metrics[key];
    
    metrics.request_count++;
    if (status_code >= 400) {
        metrics.error_count++;
    }
    metrics.total_latency_ms += latency_ms;
    metrics.total_bytes_sent += bytes_sent;
    metrics.total_bytes_received += bytes_received;
    
    m_total_requests++;
    if (status_code >= 400) {
        m_total_errors++;
    }
}

std::string MetricsCollector::GetPrometheusMetrics() {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    
    std::stringstream ss;
    
    // Total requests
    ss << FormatPrometheusMetric("http_requests_total", "counter",
                                "Total HTTP requests", std::to_string(m_total_requests.load()));
    
    // Total errors
    ss << FormatPrometheusMetric("http_errors_total", "counter",
                                "Total HTTP errors", std::to_string(m_total_errors.load()));
    
    // Uptime
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_start_time
    ).count();
    ss << FormatPrometheusMetric("http_server_uptime_seconds", "counter",
                                "Server uptime in seconds", std::to_string(uptime));
    
    // Per-endpoint metrics
    for (const auto& [endpoint, metrics] : m_endpoint_metrics) {
        std::string safe_endpoint = endpoint;
        std::replace(safe_endpoint.begin(), safe_endpoint.end(), ' ', '_');
        std::replace(safe_endpoint.begin(), safe_endpoint.end(), '/', '_');
        
        ss << "http_endpoint_requests_total{endpoint=\"" << endpoint << "\"} " 
           << metrics.request_count.load() << "\n";
        
        ss << "http_endpoint_errors_total{endpoint=\"" << endpoint << "\"} " 
           << metrics.error_count.load() << "\n";
        
        uint64_t avg_latency = metrics.request_count > 0 ? 
            metrics.total_latency_ms.load() / metrics.request_count.load() : 0;
        ss << "http_endpoint_latency_avg_ms{endpoint=\"" << endpoint << "\"} " 
           << avg_latency << "\n";
        
        ss << "http_endpoint_bytes_sent_total{endpoint=\"" << endpoint << "\"} " 
           << metrics.total_bytes_sent.load() << "\n";
        
        ss << "http_endpoint_bytes_received_total{endpoint=\"" << endpoint << "\"} " 
           << metrics.total_bytes_received.load() << "\n";
    }
    
    // Custom counters
    for (const auto& [name, value] : m_custom_counters) {
        ss << name << " " << value << "\n";
    }
    
    // Custom gauges
    for (const auto& [name, value] : m_custom_gauges) {
        ss << name << " " << value << "\n";
    }
    
    return ss.str();
}

void MetricsCollector::InstrumentRequest(const httplib::Request& req, httplib::Response& res,
                                        const std::chrono::steady_clock::time_point& start_time) {
    auto end_time = std::chrono::steady_clock::now();
    auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    RecordRequest(req.path, req.method, res.status, latency_ms, 
                 res.body.size(), req.body.size());
}

void MetricsCollector::IncrementCounter(const std::string& metric_name, double value) {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    m_custom_counters[metric_name] += value;
}

void MetricsCollector::SetGauge(const std::string& metric_name, double value) {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    m_custom_gauges[metric_name] = value;
}

void MetricsCollector::ObserveHistogram(const std::string& metric_name, double value) {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    m_custom_histograms[metric_name].push_back(value);
}

void MetricsCollector::CollectSystemMetrics() {
    // Collect system metrics (CPU, memory, etc.)
    // This would integrate with OS-specific APIs
}

MetricsCollector::EndpointMetrics MetricsCollector::GetEndpointMetrics(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    return m_endpoint_metrics[endpoint];
}

void MetricsCollector::Reset() {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    m_endpoint_metrics.clear();
    m_custom_counters.clear();
    m_custom_gauges.clear();
    m_custom_histograms.clear();
    m_total_requests = 0;
    m_total_errors = 0;
    m_start_time = std::chrono::steady_clock::now();
}

std::string MetricsCollector::FormatPrometheusMetric(const std::string& name, const std::string& type,
                                                     const std::string& help, const std::string& value) {
    std::stringstream ss;
    ss << "# HELP " << name << " " << help << "\n";
    ss << "# TYPE " << name << " " << type << "\n";
    ss << name << " " << value << "\n";
    return ss.str();
}

double MetricsCollector::CalculatePercentile(const std::vector<uint64_t>& values, double percentile) {
    if (values.empty()) return 0.0;
    
    std::vector<uint64_t> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    
    size_t index = static_cast<size_t>(percentile * sorted.size());
    if (index >= sorted.size()) index = sorted.size() - 1;
    
    return sorted[index];
}

// ============================================================================
// WebSocketManager Implementation
// ============================================================================

WebSocketManager::WebSocketManager() {
}

WebSocketManager::~WebSocketManager() {
    // Clean up all connections
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    m_clients.clear();
}

void WebSocketManager::SetupWebSocket(httplib::Server& server, const std::string& endpoint) {
    // Note: cpp-httplib doesn't have built-in WebSocket support
    // This is a placeholder for integration with a WebSocket library
    std::cout << "[WebSocket] Setup endpoint: " << endpoint << " (requires WebSocket library)" << std::endl;
}

bool WebSocketManager::SendToClient(const std::string& client_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    
    auto it = m_clients.find(client_id);
    if (it == m_clients.end() || it->second.ws_ptr == nullptr) {
        return false;
    }
    
    // Send message through WebSocket connection
    // Implementation depends on WebSocket library
    std::cout << "[WebSocket] Send to " << client_id << ": " << message << std::endl;
    return true;
}

void WebSocketManager::Broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    
    for (const auto& [client_id, conn] : m_clients) {
        if (conn.ws_ptr != nullptr) {
            // Send to each client
            std::cout << "[WebSocket] Broadcast to " << client_id << ": " << message << std::endl;
        }
    }
}

void WebSocketManager::BroadcastToRoom(const std::string& room, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    
    auto it = m_rooms.find(room);
    if (it == m_rooms.end()) return;
    
    for (const auto& client_id : it->second) {
        auto client_it = m_clients.find(client_id);
        if (client_it != m_clients.end() && client_it->second.ws_ptr != nullptr) {
            std::cout << "[WebSocket] Send to room '" << room << "', client " << client_id 
                     << ": " << message << std::endl;
        }
    }
}

void WebSocketManager::JoinRoom(const std::string& client_id, const std::string& room) {
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    
    auto it = m_clients.find(client_id);
    if (it != m_clients.end()) {
        it->second.rooms.push_back(room);
        m_rooms[room].push_back(client_id);
    }
}

void WebSocketManager::LeaveRoom(const std::string& client_id, const std::string& room) {
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    
    auto room_it = m_rooms.find(room);
    if (room_it != m_rooms.end()) {
        auto& clients = room_it->second;
        clients.erase(std::remove(clients.begin(), clients.end(), client_id), clients.end());
    }
}

void WebSocketManager::SetMessageCallback(MessageCallback callback) {
    m_on_message = callback;
}

void WebSocketManager::SetConnectionCallback(ConnectionCallback callback) {
    m_on_connection = callback;
}

std::vector<std::string> WebSocketManager::GetConnectedClients() const {
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    
    std::vector<std::string> clients;
    for (const auto& [id, conn] : m_clients) {
        clients.push_back(id);
    }
    return clients;
}

bool WebSocketManager::IsClientConnected(const std::string& client_id) const {
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    return m_clients.find(client_id) != m_clients.end();
}

void WebSocketManager::CloseConnection(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    m_clients.erase(client_id);
}

std::string WebSocketManager::GenerateClientID() {
    static std::atomic<uint64_t> counter{0};
    std::stringstream ss;
    ss << "client_" << std::chrono::steady_clock::now().time_since_epoch().count() 
       << "_" << counter++;
    return ss.str();
}

void WebSocketManager::HandleConnect(void* ws_ptr) {
    std::string client_id = GenerateClientID();
    
    {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        ClientConnection conn;
        conn.client_id = client_id;
        conn.ws_ptr = ws_ptr;
        conn.connected_at = std::chrono::steady_clock::now();
        m_clients[client_id] = conn;
    }
    
    if (m_on_connection) {
        m_on_connection(client_id, true);
    }
}

void WebSocketManager::HandleDisconnect(const std::string& client_id) {
    {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        
        // Remove from all rooms
        auto it = m_clients.find(client_id);
        if (it != m_clients.end()) {
            for (const auto& room : it->second.rooms) {
                LeaveRoom(client_id, room);
            }
        }
        
        m_clients.erase(client_id);
    }
    
    if (m_on_connection) {
        m_on_connection(client_id, false);
    }
}

void WebSocketManager::HandleMessage(const std::string& client_id, const std::string& message) {
    if (m_on_message) {
        m_on_message(client_id, message);
    }
}

// ============================================================================
// ServerEnhancementManager Implementation
// ============================================================================

ServerEnhancementManager::ServerEnhancementManager() {
}

ServerEnhancementManager::~ServerEnhancementManager() {
}

void ServerEnhancementManager::Initialize(const std::string& jwt_secret) {
    m_auth = std::make_unique<AuthenticationManager>(jwt_secret);
    m_compression = std::make_unique<CompressionManager>();
    m_cache = std::make_unique<CacheManager>();
    m_rate_limiter = std::make_unique<RateLimiter>(60);  // 60 requests/minute default
    m_metrics = std::make_unique<MetricsCollector>();
    m_websocket = std::make_unique<WebSocketManager>();
    
    std::cout << "[Enhancement] Initialized all HTTP server enhancements" << std::endl;
}

void ServerEnhancementManager::ApplyMiddleware(httplib::Server& server) {
    // Pre-routing middleware
    server.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();
        
        // Rate limiting
        if (m_rate_limiting_enabled && !m_rate_limiter->RateLimitRequest(req, res)) {
            return httplib::Server::HandlerResponse::Handled;
        }
        
        // Authentication (for protected endpoints)
        if (m_auth_enabled && req.path.find("/api/") == 0) {
            // Skip auth for public endpoints like /api/tags
            if (req.path != "/api/tags" && req.path != "/metrics") {
                if (!m_auth->AuthenticateRequest(req, res)) {
                    return httplib::Server::HandlerResponse::Handled;
                }
            }
        }
        
        // Caching
        if (m_caching_enabled && m_cache->HandleCachedResponse(req, res)) {
            return httplib::Server::HandlerResponse::Handled;
        }
        
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // Post-routing middleware
    server.set_post_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        auto end_time = std::chrono::steady_clock::now();
        
        // Compression
        if (m_compression_enabled) {
            m_compression->ApplyCompression(req, res);
        }
        
        // Metrics
        if (m_metrics_enabled) {
            m_metrics->InstrumentRequest(req, res, end_time);
        }
    });
    
    std::cout << "[Enhancement] Applied middleware to server" << std::endl;
}

void ServerEnhancementManager::SetupMetricsEndpoint(httplib::Server& server) {
    server.Get("/metrics", [this](const httplib::Request& req, httplib::Response& res) {
        std::string metrics = m_metrics->GetPrometheusMetrics();
        res.set_content(metrics, "text/plain; version=0.0.4");
        res.status = 200;
    });
    
    std::cout << "[Enhancement] Setup /metrics endpoint" << std::endl;
}

void ServerEnhancementManager::SetupWebSocketEndpoints(httplib::Server& server) {
    if (m_websocket_enabled) {
        m_websocket->SetupWebSocket(server, "/ws");
        std::cout << "[Enhancement] Setup WebSocket endpoints" << std::endl;
    }
}

} // namespace HTTPEnhancements
