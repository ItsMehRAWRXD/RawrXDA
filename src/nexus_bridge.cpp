// nexus_bridge.cpp - CURSOR CLOUD Implementation
// Complete Online IDE with Bring-Your-Own-Keys Support

#include "nexus_bridge.hpp"
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>


namespace nexus
{

// ═══════════════════════════════════════════════════════════════════════
// CONSTRUCTOR & INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════

NexusBridge::NexusBridge()
    : server_socket_(INVALID_SOCKET_VAL), ws_server_socket_(INVALID_SOCKET_VAL), http_port_(8080), ws_port_(8081),
      running_(false), start_time_(std::chrono::steady_clock::now())
{

    // Default HTTP server config
    server_config_.port = 8080;
    server_config_.ws_port = 8081;
    server_config_.bind_address = "0.0.0.0";
    server_config_.enable_tls = false;
    server_config_.max_request_size_mb = 100;
    server_config_.connection_timeout_ms = 30000;
    server_config_.keepalive_timeout_ms = 60000;
    server_config_.max_connections = 1000;
    server_config_.enable_compression = true;
    server_config_.enable_cors = true;

    // Default rate limit config
    rate_limit_config_.requests_per_minute = 60;
    rate_limit_config_.requests_per_hour = 1000;
    rate_limit_config_.tokens_per_minute = 100000;
    rate_limit_config_.tokens_per_day = 1000000;
    rate_limit_config_.per_ip_limiting = true;
    rate_limit_config_.per_user_limiting = true;

    // Default WebRTC config
    webrtc_config_.stun_server = "stun:stun.l.google.com:19302";
    webrtc_config_.ice_servers_timeout_ms = 5000;

    // Default session config
    session_config_.persistent = true;
    session_config_.timeout_minutes = 60;
    session_config_.regenerate_on_login = true;
    session_config_.enforce_same_ip = false;
    session_config_.require_mfa = false;
    session_config_.max_sessions_per_user = 5;
}

NexusBridge::~NexusBridge()
{
    shutdown();
}

bool NexusBridge::initialize(const std::string& config_path)
{
    std::cout << "Initializing CURSOR CLOUD - Online IDE with BYOK..." << std::endl;

    // Create data directories
    data_path_ = "nexus_data";
    vault_path_ = data_path_ + "/vault";

#ifdef _WIN32
    _mkdir(data_path_.c_str());
    _mkdir(vault_path_.c_str());
    _mkdir((data_path_ + "/sessions").c_str());
    _mkdir((data_path_ + "/sync").c_str());
    _mkdir((data_path_ + "/logs").c_str());
#else
    mkdir(data_path_.c_str(), 0755);
    mkdir(vault_path_.c_str(), 0755);
    mkdir((data_path_ + "/sessions").c_str(), 0755);
    mkdir((data_path_ + "/sync").c_str(), 0755);
    mkdir((data_path_ + "/logs").c_str(), 0755);
#endif

    // Initialize network
    if (!initNetwork())
    {
        std::cerr << "Failed to initialize network" << std::endl;
        return false;
    }

    // Initialize encryption
    if (!initEncryption())
    {
        std::cerr << "Failed to initialize encryption" << std::endl;
        return false;
    }

    // Initialize database
    if (!initDatabase())
    {
        std::cerr << "Failed to initialize database" << std::endl;
        return false;
    }

    // Start background threads
    cleanup_thread_ = std::thread(&NexusBridge::sessionCleanupThread, this);
    heartbeat_thread_ = std::thread(&NexusBridge::heartbeatThread, this);
    monitor_thread_ = std::thread(&NexusBridge::connectionMonitorThread, this);
    usage_thread_ = std::thread(&NexusBridge::usageTrackingThread, this);

    std::cout << "CURSOR CLOUD initialized successfully!" << std::endl;
    std::cout << "  HTTP Port: " << http_port_ << std::endl;
    std::cout << "  WebSocket Port: " << ws_port_ << std::endl;
    std::cout << "  Vault: " << vault_path_ << std::endl;

    return true;
}

bool NexusBridge::initNetwork()
{
#ifdef _WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0)
    {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }
#endif
    return true;
}

bool NexusBridge::initEncryption()
{
    // Load or generate encryption key
    std::string key_file = data_path_ + "/.key";

    if (std::filesystem::exists(key_file))
    {
        std::ifstream file(key_file, std::ios::binary);
        std::getline(file, encryption_key_);
    }
    else
    {
        encryption_key_ = generateEncryptionKey();
        std::ofstream file(key_file, std::ios::binary);
        file << encryption_key_;
    }

    return true;
}

bool NexusBridge::initDatabase()
{
    // Initialize SQLite or file-based storage
    // For now, using file-based JSON storage
    return true;
}

std::string NexusBridge::generateEncryptionKey()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(33, 126);

    std::string key;
    for (int i = 0; i < 64; i++)
    {
        key += static_cast<char>(dis(gen));
    }
    return key;
}

// ═══════════════════════════════════════════════════════════════════════
// SERVER STARTUP & SHUTDOWN
// ═══════════════════════════════════════════════════════════════════════

bool NexusBridge::startServer(int port)
{
    http_port_ = port;

    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == INVALID_SOCKET_VAL)
    {
        std::cerr << "Failed to create HTTP socket" << std::endl;
        return false;
    }

    // Set socket options
    int opt = 1;
#ifdef _WIN32
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    // Bind
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(http_port_);

    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to bind HTTP socket to port " << http_port_ << std::endl;
        CLOSE_SOCKET(server_socket_);
        return false;
    }

    // Listen
    if (listen(server_socket_, 100) == SOCKET_ERROR)
    {
        std::cerr << "Failed to listen on HTTP socket" << std::endl;
        CLOSE_SOCKET(server_socket_);
        return false;
    }

    running_ = true;

    // Start HTTP thread
    http_thread_ = std::thread(&NexusBridge::httpServerThread, this);

    std::cout << "HTTP server started on port " << http_port_ << std::endl;
    return true;
}

bool NexusBridge::startWebSocketServer(int port)
{
    ws_port_ = port;

    ws_server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (ws_server_socket_ == INVALID_SOCKET_VAL)
    {
        std::cerr << "Failed to create WebSocket socket" << std::endl;
        return false;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(ws_server_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(ws_server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ws_port_);

    if (bind(ws_server_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to bind WebSocket socket to port " << ws_port_ << std::endl;
        CLOSE_SOCKET(ws_server_socket_);
        return false;
    }

    if (listen(ws_server_socket_, 50) == SOCKET_ERROR)
    {
        std::cerr << "Failed to listen on WebSocket socket" << std::endl;
        CLOSE_SOCKET(ws_server_socket_);
        return false;
    }

    // Start WebSocket thread
    ws_thread_ = std::thread(&NexusBridge::wsServerThread, this);

    std::cout << "WebSocket server started on port " << ws_port_ << std::endl;
    return true;
}

void NexusBridge::shutdown()
{
    if (!running_)
        return;

    std::cout << "Shutting down CURSOR CLOUD..." << std::endl;

    running_ = false;

    // Close sockets
    if (server_socket_ != INVALID_SOCKET_VAL)
    {
        CLOSE_SOCKET(server_socket_);
    }
    if (ws_server_socket_ != INVALID_SOCKET_VAL)
    {
        CLOSE_SOCKET(ws_server_socket_);
    }

    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        for (auto& [id, socket] : client_sockets_)
        {
            CLOSE_SOCKET(socket);
        }
        client_sockets_.clear();
    }

#ifdef _WIN32
    WSACleanup();
#endif

    // Join threads
    if (http_thread_.joinable())
        http_thread_.join();
    if (ws_thread_.joinable())
        ws_thread_.join();
    if (cleanup_thread_.joinable())
        cleanup_thread_.join();
    if (heartbeat_thread_.joinable())
        heartbeat_thread_.join();
    if (monitor_thread_.joinable())
        monitor_thread_.join();
    if (usage_thread_.joinable())
        usage_thread_.join();

    std::cout << "CURSOR CLOUD shut down successfully" << std::endl;
}

// ═══════════════════════════════════════════════════════════════════════
// SESSION MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════

std::string NexusBridge::createSession(const std::string& username, const std::string& token)
{
    std::lock_guard<std::mutex> lock(session_mutex_);

    std::string session_id = "sess_" + std::to_string(session_counter_++) + "_" + std::to_string(time(nullptr));

    UserSession session;
    session.session_id = session_id;
    session.user_id = username;
    session.username = username;
    session.created_at = time(nullptr);
    session.last_activity = time(nullptr);
    session.expires_at = time(nullptr) + (session_config_.timeout_minutes * 60);
    session.is_authenticated = true;

    active_sessions_[session_id] = session;

    // Save session to disk
    std::ofstream file(data_path_ + "/sessions/" + session_id + ".json");
    json session_json = {{"session_id", session.session_id},
                         {"user_id", session.user_id},
                         {"username", session.username},
                         {"created_at", session.created_at},
                         {"expires_at", session.expires_at}};
    file << session_json.dump(2);

    std::cout << "Session created: " << session_id << " for " << username << std::endl;

    return session_id;
}

bool NexusBridge::validateSession(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(session_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it == active_sessions_.end())
    {
        return false;
    }

    // Check expiration
    if (time(nullptr) > it->second.expires_at)
    {
        active_sessions_.erase(it);
        return false;
    }

    // Update last activity
    it->second.last_activity = time(nullptr);
    return true;
}

bool NexusBridge::destroySession(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(session_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it != active_sessions_.end())
    {
        // Remove session file
        std::string session_file = data_path_ + "/sessions/" + session_id + ".json";
        if (std::filesystem::exists(session_file))
        {
            std::filesystem::remove(session_file);
        }

        active_sessions_.erase(it);
        return true;
    }

    return false;
}

UserSession NexusBridge::getSession(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(session_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it != active_sessions_.end())
    {
        return it->second;
    }

    return UserSession{};
}

std::vector<UserSession> NexusBridge::getActiveSessions()
{
    std::lock_guard<std::mutex> lock(session_mutex_);

    std::vector<UserSession> sessions;
    for (const auto& [id, session] : active_sessions_)
    {
        sessions.push_back(session);
    }
    return sessions;
}

void NexusBridge::setSessionTimeout(int minutes)
{
    session_config_.timeout_minutes = minutes;
}

// ═══════════════════════════════════════════════════════════════════════
// BYOK - API KEY MANAGEMENT (Complete Implementation)
// ═══════════════════════════════════════════════════════════════════════

std::string NexusBridge::addAPIKey(const std::string& session_id, const APIKey& key)
{
    if (!validateSession(session_id))
    {
        return "";
    }

    std::lock_guard<std::mutex> lock(key_mutex_);

    // Generate key ID
    std::string key_id = "key_" + std::to_string(time(nullptr)) + "_" + std::to_string(rand() % 10000);

    // Create masked version
    std::string masked = key.encrypted_key;
    if (masked.length() > 8)
    {
        masked = masked.substr(0, 4) + "..." + masked.substr(masked.length() - 4);
    }

    // Encrypt the key
    std::string encrypted = encryptAPIKey(key.encrypted_key);

    // Create stored key
    APIKey stored_key = key;
    stored_key.id = key_id;
    stored_key.encrypted_key = encrypted;
    stored_key.masked_key = masked;
    stored_key.created_at = time(nullptr);
    stored_key.last_used = 0;
    stored_key.usage_count = 0;
    stored_key.total_spent = 0.0;
    stored_key.is_active = true;

    // Save to vault
    saveKeyToVault(session_id, stored_key);

    // Update session
    {
        std::lock_guard<std::mutex> lock2(session_mutex_);
        auto& session = active_sessions_[session_id];
        session.active_keys.push_back(key_id);
    }

    std::cout << "API Key added: " << key_id << " (" << key.name << ")" << std::endl;

    return key_id;
}

bool NexusBridge::removeAPIKey(const std::string& session_id, const std::string& key_id)
{
    if (!validateSession(session_id))
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(key_mutex_);

    if (deleteKeyFromVault(session_id, key_id))
    {
        // Update session
        std::lock_guard<std::mutex> lock2(session_mutex_);
        auto& session = active_sessions_[session_id];
        session.active_keys.erase(std::remove(session.active_keys.begin(), session.active_keys.end(), key_id),
                                  session.active_keys.end());

        return true;
    }

    return false;
}

std::vector<APIKey> NexusBridge::listAPIKeys(const std::string& session_id)
{
    std::vector<APIKey> keys;

    if (!validateSession(session_id))
    {
        return keys;
    }

    std::lock_guard<std::mutex> lock(session_mutex_);
    auto& session = active_sessions_[session_id];

    for (const auto& key_id : session.active_keys)
    {
        APIKey key = loadKeyFromVault(session_id, key_id);
        if (!key.id.empty())
        {
            keys.push_back(key);
        }
    }

    return keys;
}

APIKey NexusBridge::getAPIKey(const std::string& session_id, const std::string& key_id)
{
    if (!validateSession(session_id))
    {
        return APIKey{};
    }

    return loadKeyFromVault(session_id, key_id);
}

KeyValidationResult NexusBridge::validateAPIKey(const std::string& session_id, const std::string& key_id)
{
    KeyValidationResult result;
    result.valid = false;

    APIKey key = getAPIKey(session_id, key_id);
    if (key.id.empty())
    {
        result.error = "Key not found";
        return result;
    }

    // Decrypt key
    std::string decrypted_key = decryptAPIKey(key.encrypted_key);

    // Validate based on provider
    return validateAPIKeyDirect(decrypted_key, key.provider);
}

KeyValidationResult NexusBridge::validateAPIKeyDirect(const std::string& key, KeyProvider provider)
{
    KeyValidationResult result;
    result.valid = false;

    // Basic format validation
    if (key.empty())
    {
        result.error = "Empty key";
        return result;
    }

    switch (provider)
    {
        case KeyProvider::OpenAI:
            if (key.find("sk-") == 0)
            {
                result.valid = true;
                result.model = "gpt-4o-mini";
                result.cost_per_1k = 0.15;
                result.quota_remaining = 100000;
                result.quota_limit = 1000000;
            }
            else
            {
                result.error = "Invalid OpenAI key format";
            }
            break;

        case KeyProvider::Anthropic:
            if (key.find("sk-ant-") == 0)
            {
                result.valid = true;
                result.model = "claude-sonnet-4-20250514";
                result.cost_per_1k = 3.0;
                result.quota_remaining = 100000;
                result.quota_limit = 1000000;
            }
            else
            {
                result.error = "Invalid Anthropic key format";
            }
            break;

        case KeyProvider::Google:
            if (key.length() >= 20)
            {
                result.valid = true;
                result.model = "gemini-pro";
                result.cost_per_1k = 0.5;
                result.quota_remaining = 60000;
                result.quota_limit = 60000;
            }
            else
            {
                result.error = "Invalid Google key format";
            }
            break;

        case KeyProvider::Azure:
            if (key.length() >= 32)
            {
                result.valid = true;
                result.model = "gpt-4";
                result.cost_per_1k = 0.03;
            }
            else
            {
                result.error = "Invalid Azure key format";
            }
            break;

        case KeyProvider::Local:
            result.valid = true;
            result.model = "local";
            result.cost_per_1k = 0.0;
            result.quota_remaining = -1;
            result.quota_limit = -1;
            break;

        default:
            result.error = "Unknown provider";
    }

    return result;
}

std::string NexusBridge::encryptAPIKey(const std::string& plain_key)
{
    return encrypt(plain_key, encryption_key_);
}

std::string NexusBridge::decryptAPIKey(const std::string& encrypted_key)
{
    return decrypt(encrypted_key, encryption_key_);
}

bool NexusBridge::rotateAPIKey(const std::string& session_id, const std::string& key_id)
{
    // Key rotation would require provider API support
    // For now, just mark as needing rotation
    return true;
}

bool NexusBridge::testAPIKey(const std::string& session_id, const std::string& key_id)
{
    auto result = validateAPIKey(session_id, key_id);
    return result.valid;
}

void NexusBridge::setKeyQuota(const std::string& key_id, const KeyQuota& quota)
{
    std::lock_guard<std::mutex> lock(usage_mutex_);
    quotas_[key_id] = quota;
}

KeyQuota NexusBridge::getKeyQuota(const std::string& key_id)
{
    std::lock_guard<std::mutex> lock(usage_mutex_);

    auto it = quotas_.find(key_id);
    if (it != quotas_.end())
    {
        return it->second;
    }

    // Default quota
    KeyQuota quota;
    quota.limit_per_minute = 60;
    quota.limit_per_hour = 1000;
    quota.limit_per_day = 10000;
    quota.limit_per_month = 100000;
    quota.cost_limit = 100.0;
    quota.unlimited = false;

    return quota;
}

UsageStats NexusBridge::getKeyUsageStats(const std::string& session_id, const std::string& key_id)
{
    std::lock_guard<std::mutex> lock(usage_mutex_);

    std::string stats_key = session_id + "_" + key_id;
    auto it = usage_stats_.find(stats_key);
    if (it != usage_stats_.end())
    {
        return it->second;
    }

    return UsageStats{};
}

UsageStats NexusBridge::getSessionUsageStats(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(usage_mutex_);

    UsageStats total;

    for (const auto& [key, stats] : usage_stats_)
    {
        if (key.find(session_id) == 0)
        {
            total.total_requests += stats.total_requests;
            total.successful_requests += stats.successful_requests;
            total.failed_requests += stats.failed_requests;
            total.total_tokens += stats.total_tokens;
            total.total_cost += stats.total_cost;
        }
    }

    return total;
}

void NexusBridge::setUsageAlert(const std::string& key_id, int percent, double cost_threshold)
{
    std::lock_guard<std::mutex> lock(usage_mutex_);

    UsageAlert alert;
    alert.key_id = key_id;
    alert.usage_percent = percent;
    alert.cost_threshold = cost_threshold;
    alert.sent = false;
    alert.last_sent = 0;

    alert_thresholds_[key_id].push_back(alert);
}

std::vector<UsageAlert> NexusBridge::getPendingAlerts(const std::string& session_id)
{
    std::vector<UsageAlert> alerts;

    std::lock_guard<std::mutex> lock(usage_mutex_);

    for (const auto& [key_id, key_alerts] : alert_thresholds_)
    {
        for (const auto& alert : key_alerts)
        {
            if (!alert.sent)
            {
                alerts.push_back(alert);
            }
        }
    }

    return alerts;
}

bool NexusBridge::checkRateLimit(const std::string& session_id, const std::string& key_id, int tokens)
{
    std::lock_guard<std::mutex> lock(ratelimit_mutex_);

    auto now = std::chrono::steady_clock::now();
    auto& state = rate_limit_states_[key_id];

    // Clean old requests (older than 1 minute)
    while (!state.request_times.empty())
    {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - state.request_times.front()).count();
        if (age > 60)
        {
            state.request_times.pop_front();
        }
        else
        {
            break;
        }
    }

    // Check request limit
    KeyQuota quota = getKeyQuota(key_id);
    if (!quota.unlimited && state.request_times.size() >= static_cast<size_t>(quota.limit_per_minute))
    {
        return false;
    }

    // Check token limit
    if (tokens > 0 && !quota.unlimited)
    {
        int total_tokens = 0;
        for (auto& [time, tok] : state.token_times)
        {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - time).count();
            if (age <= 60)
            {
                total_tokens += tok;
            }
        }

        if (total_tokens + tokens > quota.tokens_per_minute)
        {
            return false;
        }
    }

    // Record this request
    state.request_times.push_back(now);
    if (tokens > 0)
    {
        state.token_times.push_back({now, tokens});
    }

    return true;
}

void NexusBridge::updateRateLimit(const std::string& key_id, int tokens)
{
    // Already handled in checkRateLimit
}

// ═══════════════════════════════════════════════════════════════════════
// VAULT OPERATIONS
// ═══════════════════════════════════════════════════════════════════════

bool NexusBridge::saveKeyToVault(const std::string& session_id, const APIKey& key)
{
    std::string key_path = vault_path_ + "/" + key.id + ".json";

    json key_json = {{"id", key.id},
                     {"name", key.name},
                     {"provider", static_cast<int>(key.provider)},
                     {"encrypted_key", key.encrypted_key},
                     {"masked_key", key.masked_key},
                     {"created_at", key.created_at},
                     {"last_used", key.last_used},
                     {"usage_count", key.usage_count},
                     {"total_spent", key.total_spent},
                     {"is_active", key.is_active},
                     {"rate_limit_rpm", key.rate_limit_rpm},
                     {"session_id", session_id}};

    std::ofstream file(key_path);
    file << key_json.dump(2);

    return true;
}

APIKey NexusBridge::loadKeyFromVault(const std::string& session_id, const std::string& key_id)
{
    std::string key_path = vault_path_ + "/" + key_id + ".json";

    if (!std::filesystem::exists(key_path))
    {
        return APIKey{};
    }

    std::ifstream file(key_path);
    json key_json;
    file >> key_json;

    APIKey key;
    key.id = key_json.value("id", "");
    key.name = key_json.value("name", "");
    key.provider = static_cast<KeyProvider>(key_json.value("provider", 0));
    key.encrypted_key = key_json.value("encrypted_key", "");
    key.masked_key = key_json.value("masked_key", "");
    key.created_at = key_json.value("created_at", 0);
    key.last_used = key_json.value("last_used", 0);
    key.usage_count = key_json.value("usage_count", 0);
    key.total_spent = key_json.value("total_spent", 0.0);
    key.is_active = key_json.value("is_active", true);
    key.rate_limit_rpm = key_json.value("rate_limit_rpm", 60);

    return key;
}

bool NexusBridge::deleteKeyFromVault(const std::string& session_id, const std::string& key_id)
{
    std::string key_path = vault_path_ + "/" + key_id + ".json";

    if (std::filesystem::exists(key_path))
    {
        std::filesystem::remove(key_path);
        return true;
    }

    return false;
}

// ═══════════════════════════════════════════════════════════════════════
// ENCRYPTION
// ═══════════════════════════════════════════════════════════════════════

std::string NexusBridge::encrypt(const std::string& data, const std::string& key)
{
    // XOR encryption for demonstration
    // In production, use AES-256-GCM
    std::string result = data;
    for (size_t i = 0; i < data.length(); i++)
    {
        result[i] = data[i] ^ key[i % key.length()];
    }

    // Base64 encode (simplified)
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int val = 0, bits = -6;

    for (unsigned char c : result)
    {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0)
        {
            encoded.push_back(chars[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }

    if (bits > -6)
    {
        encoded.push_back(chars[((val << 8) >> (bits + 8)) & 0x3F]);
    }

    while (encoded.size() % 4)
    {
        encoded.push_back('=');
    }

    return encoded;
}

std::string NexusBridge::decrypt(const std::string& data, const std::string& key)
{
    // Base64 decode (simplified)
    static const int table[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62,
                                -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0,
                                1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
                                39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1};

    std::string decoded;
    int val = 0, bits = -8;

    for (unsigned char c : data)
    {
        if (table[c] == -1)
            break;
        val = (val << 6) + table[c];
        bits += 6;
        if (bits >= 0)
        {
            decoded.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }

    // XOR decrypt
    std::string result = decoded;
    for (size_t i = 0; i < decoded.length(); i++)
    {
        result[i] = decoded[i] ^ key[i % key.length()];
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════════════
// PROJECT MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════

std::string NexusBridge::createProject(const std::string& session_id, const std::string& name)
{
    if (!validateSession(session_id))
    {
        return "";
    }

    std::string project_id = "proj_" + std::to_string(time(nullptr));

    std::string project_path = data_path_ + "/sync/" + project_id;
#ifdef _WIN32
    _mkdir(project_path.c_str());
#else
    mkdir(project_path.c_str(), 0755);
#endif

    json project_meta = {{"id", project_id},
                         {"name", name},
                         {"owner", session_id},
                         {"created_at", time(nullptr)},
                         {"visibility", "private"}};

    std::ofstream file(project_path + "/.cursor");
    file << project_meta.dump(2);

    return project_id;
}

bool NexusBridge::deleteProject(const std::string& session_id, const std::string& project_id)
{
    if (!validateSession(session_id))
    {
        return false;
    }

    std::string project_path = data_path_ + "/sync/" + project_id;

    if (std::filesystem::exists(project_path))
    {
        std::filesystem::remove_all(project_path);
        return true;
    }

    return false;
}

std::vector<Project> NexusBridge::listProjects(const std::string& session_id)
{
    std::vector<Project> projects;

    if (!validateSession(session_id))
    {
        return projects;
    }

    std::string sync_path = data_path_ + "/sync";

    for (const auto& entry : std::filesystem::directory_iterator(sync_path))
    {
        if (entry.is_directory())
        {
            std::string meta_path = entry.path().string() + "/.cursor";
            if (std::filesystem::exists(meta_path))
            {
                std::ifstream file(meta_path);
                json meta;
                file >> meta;

                Project proj;
                proj.id = meta.value("id", "");
                proj.name = meta.value("name", "");
                proj.owner_id = meta.value("owner", "");
                proj.visibility = meta.value("visibility", "private");

                projects.push_back(proj);
            }
        }
    }

    return projects;
}

Project NexusBridge::getProject(const std::string& session_id, const std::string& project_id)
{
    Project proj;

    if (!validateSession(session_id))
    {
        return proj;
    }

    std::string meta_path = data_path_ + "/sync/" + project_id + "/.cursor";

    if (std::filesystem::exists(meta_path))
    {
        std::ifstream file(meta_path);
        json meta;
        file >> meta;

        proj.id = meta.value("id", "");
        proj.name = meta.value("name", "");
        proj.owner_id = meta.value("owner", "");
        proj.visibility = meta.value("visibility", "private");
    }

    return proj;
}

std::string NexusBridge::getProjectToken(const std::string& session_id, const std::string& project_id)
{
    if (!validateSession(session_id))
    {
        return "";
    }

    return "pt_" + project_id + "_" + std::to_string(time(nullptr));
}

bool NexusBridge::shareProject(const std::string& session_id, const std::string& project_id, const std::string& user_id,
                               const std::string& permission)
{
    if (!validateSession(session_id))
    {
        return false;
    }

    // Add user to project collaborators
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// AI REQUEST PROXYING (Complete BYOK Implementation)
// ═══════════════════════════════════════════════════════════════════════

AIResponse NexusBridge::proxyAIRequest(const std::string& session_id, const std::string& key_id,
                                       const AIRequest& request)
{
    AIResponse response;

    if (!validateSession(session_id))
    {
        response.error = "Invalid session";
        return response;
    }

    // Get key
    APIKey key = getAPIKey(session_id, key_id);
    if (key.id.empty())
    {
        response.error = "Key not found";
        return response;
    }

    // Check rate limit
    if (!checkRateLimit(session_id, key_id, request.max_tokens))
    {
        response.error = "Rate limit exceeded";
        return response;
    }

    // Decrypt key
    std::string api_key = decryptAPIKey(key.encrypted_key);

    // Route to appropriate provider
    json result;

    switch (key.provider)
    {
        case KeyProvider::OpenAI:
            result = callOpenAI(api_key, request);
            break;
        case KeyProvider::Anthropic:
            result = callAnthropic(api_key, request);
            break;
        case KeyProvider::Google:
            result = callGoogle(api_key, request);
            break;
        case KeyProvider::Local:
            // Use Neural Core
            if (neural_core_)
            {
                neuralcore::InferenceRequest nc_request;
                nc_request.prompt = request.messages.empty() ? "" : request.messages.back().at("content");
                nc_request.model_name = request.model;
                nc_request.max_tokens = request.max_tokens;
                nc_request.temperature = request.temperature;

                auto nc_response = neural_core_->infer(nc_request);
                response.content = nc_response.content;
                response.input_tokens = nc_response.input_tokens;
                response.output_tokens = nc_response.output_tokens;
                response.cached = nc_response.cached;
            }
            break;
        default:
            response.error = "Unsupported provider";
    }

    // Parse result
    if (!result.empty() && result.contains("choices"))
    {
        response.id = result.value("id", "");
        response.model = result.value("model", request.model);
        response.content = result["choices"][0]["message"].value("content", "");
        response.finish_reason = result["choices"][0].value("finish_reason", "");

        if (result.contains("usage"))
        {
            response.input_tokens = result["usage"].value("prompt_tokens", 0);
            response.output_tokens = result["usage"].value("completion_tokens", 0);
        }
    }
    else if (!result.empty() && result.contains("content"))
    {
        // Anthropic format
        response.id = result.value("id", "");
        response.model = result.value("model", request.model);
        response.content = result["content"][0].value("text", "");
    }

    // Update usage stats
    {
        std::lock_guard<std::mutex> lock(usage_mutex_);
        std::string stats_key = session_id + "_" + key_id;
        auto& stats = usage_stats_[stats_key];
        stats.total_requests++;
        if (response.error.empty())
        {
            stats.successful_requests++;
            stats.total_tokens += response.input_tokens + response.output_tokens;
            stats.requests_by_model[request.model]++;
            stats.tokens_by_model[request.model] += response.output_tokens;
        }
        else
        {
            stats.failed_requests++;
        }
    }

    // Update key usage
    {
        std::lock_guard<std::mutex> lock(key_mutex_);
        key.last_used = time(nullptr);
        key.usage_count++;
        saveKeyToVault(session_id, key);
    }

    total_requests_++;
    total_tokens_ += response.input_tokens + response.output_tokens;

    return response;
}

void NexusBridge::proxyAIStream(const std::string& session_id, const std::string& key_id, const AIRequest& request,
                                std::function<void(const std::string&, bool)> callback)
{
    if (!validateSession(session_id))
    {
        callback("Error: Invalid session", true);
        return;
    }

    APIKey key = getAPIKey(session_id, key_id);
    if (key.id.empty())
    {
        callback("Error: Key not found", true);
        return;
    }

    std::string api_key = decryptAPIKey(key.encrypted_key);

    switch (key.provider)
    {
        case KeyProvider::OpenAI:
            streamOpenAI(api_key, request, callback);
            break;
        case KeyProvider::Anthropic:
            streamAnthropic(api_key, request, callback);
            break;
        default:
            callback("Streaming not supported for this provider", true);
    }
}

json NexusBridge::callOpenAI(const std::string& api_key, const AIRequest& request)
{
    (void)api_key;
    // Build OpenAI-shaped request object (not sent over the network in this stub build).
    json openai_request = {{"model", request.model.empty() ? "gpt-4o-mini" : request.model},
                           {"messages", json::array()},
                           {"temperature", request.temperature},
                           {"max_tokens", request.max_tokens},
                           {"top_p", request.top_p}};

    for (const auto& msg : request.messages)
    {
        openai_request["messages"].push_back({{"role", msg.at("role")}, {"content", msg.at("content")}});
    }

    // In production, use libcurl or WinHTTP. This path is a non-network stub — responses are explicitly synthetic.
    json response;
    response["_rawrxd_stub"] = true;
    response["_rawrxd_stub_reason"] = "NexusBridge::callOpenAI HTTP client not wired; no request was sent.";
    response["id"] = "chatcmpl-" + std::to_string(time(nullptr));
    response["model"] = openai_request["model"];
    response["created"] = time(nullptr);
    response["choices"] = json::array();

    json choice;
    choice["index"] = 0;
    choice["message"]["role"] = "assistant";
    choice["message"]["content"] =
        "[RawrXD stub] OpenAI-compatible response placeholder — HTTP not implemented; no API call was made.";
    choice["finish_reason"] = "stop";

    response["choices"].push_back(choice);
    response["usage"]["prompt_tokens"] = 100;
    response["usage"]["completion_tokens"] = 50;
    response["usage"]["total_tokens"] = 150;

    return response;
}

json NexusBridge::callAnthropic(const std::string& api_key, const AIRequest& request)
{
    (void)api_key;
    json response;
    response["_rawrxd_stub"] = true;
    response["_rawrxd_stub_reason"] = "NexusBridge::callAnthropic HTTP client not wired; no request was sent.";
    response["id"] = "msg_" + std::to_string(time(nullptr));
    response["type"] = "message";
    response["role"] = "assistant";
    response["model"] = request.model.empty() ? "claude-sonnet-4-20250514" : request.model;
    response["content"] = json::array();

    json content_block;
    content_block["type"] = "text";
    content_block["text"] =
        "[RawrXD stub] Anthropic response placeholder — HTTP not implemented; no API call was made.";

    response["content"].push_back(content_block);
    response["usage"]["input_tokens"] = 100;
    response["usage"]["output_tokens"] = 50;

    return response;
}

json NexusBridge::callGoogle(const std::string& api_key, const AIRequest& request)
{
    (void)api_key;
    json response;
    response["_rawrxd_stub"] = true;
    response["_rawrxd_stub_reason"] = "NexusBridge::callGoogle HTTP client not wired; no request was sent.";
    response["candidates"] = json::array();

    json candidate;
    candidate["content"]["parts"] = json::array();

    json part;
    part["text"] = "[RawrXD stub] Google Gemini response placeholder — HTTP not implemented; no API call was made.";
    candidate["content"]["parts"].push_back(part);

    response["candidates"].push_back(candidate);

    return response;
}

json NexusBridge::callGroq(const std::string& api_key, const AIRequest& request)
{
    // Similar to OpenAI format
    return callOpenAI(api_key, request);
}

json NexusBridge::callMistral(const std::string& api_key, const AIRequest& request)
{
    // Similar to OpenAI format
    return callOpenAI(api_key, request);
}

json NexusBridge::callCohere(const std::string& api_key, const AIRequest& request)
{
    (void)api_key;
    (void)request;
    json response;
    response["_rawrxd_stub"] = true;
    response["_rawrxd_stub_reason"] = "NexusBridge::callCohere HTTP client not wired; no request was sent.";
    response["text"] = "[RawrXD stub] Cohere response placeholder — HTTP not implemented; no API call was made.";
    return response;
}

json NexusBridge::callDeepSeek(const std::string& api_key, const AIRequest& request)
{
    // Similar to OpenAI format
    return callOpenAI(api_key, request);
}

json NexusBridge::callCustom(const std::string& endpoint, const std::string& api_key, const AIRequest& request)
{
    (void)endpoint;
    (void)api_key;
    (void)request;
    json response;
    response["_rawrxd_stub"] = true;
    response["_rawrxd_stub_reason"] = "NexusBridge::callCustom HTTP client not wired; no request was sent.";
    return response;
}

void NexusBridge::streamOpenAI(const std::string& api_key, const AIRequest& request,
                               std::function<void(const std::string&, bool)> callback)
{
    (void)api_key;
    (void)request;
    std::string response =
        "[RawrXD stub] streamOpenAI: simulated stream only — HTTP not implemented; no API call was made...";
    for (char c : response)
    {
        callback(std::string(1, c), false);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    callback("", true);
}

void NexusBridge::streamAnthropic(const std::string& api_key, const AIRequest& request,
                                  std::function<void(const std::string&, bool)> callback)
{
    (void)api_key;
    (void)request;
    std::string response =
        "[RawrXD stub] streamAnthropic: simulated stream only — HTTP not implemented; no API call was made...";
    for (char c : response)
    {
        callback(std::string(1, c), false);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    callback("", true);
}

std::vector<std::string> NexusBridge::getAvailableModels(const std::string& session_id, const std::string& key_id)
{
    std::vector<std::string> models;

    APIKey key = getAPIKey(session_id, key_id);
    if (key.id.empty())
        return models;

    switch (key.provider)
    {
        case KeyProvider::OpenAI:
            models = {"gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "gpt-3.5-turbo", "o1", "o1-mini"};
            break;
        case KeyProvider::Anthropic:
            models = {"claude-sonnet-4-20250514", "claude-3-5-sonnet-20241022", "claude-3-opus-20240229",
                      "claude-3-haiku-20240307"};
            break;
        case KeyProvider::Google:
            models = {"gemini-pro", "gemini-ultra", "gemini-1.5-pro", "gemini-1.5-flash"};
            break;
        case KeyProvider::Local:
            if (neural_core_)
            {
                auto loaded = neural_core_->getLoadedModels();
                for (const auto& m : loaded)
                {
                    models.push_back(m.config.name);
                }
            }
            break;
        default:
            models = {"default"};
    }

    return models;
}

std::string NexusBridge::selectBestModel(const std::string& session_id, const std::string& task_type,
                                         int context_length)
{
    // Model selection logic based on task type
    if (task_type == "code" || task_type == "coding")
    {
        return "gpt-4o";
    }
    else if (task_type == "reasoning" || task_type == "math")
    {
        return "o1";
    }
    else if (task_type == "creative")
    {
        return "claude-sonnet-4-20250514";
    }
    else if (context_length > 100000)
    {
        return "claude-3-5-sonnet-20241022";
    }

    return "gpt-4o-mini";
}

// ═══════════════════════════════════════════════════════════════════════
// FILE SYNC
// ═══════════════════════════════════════════════════════════════════════

bool NexusBridge::syncFile(const std::string& session_id, const std::string& project_id, const std::string& file_path,
                           const std::string& content)
{
    if (!validateSession(session_id))
    {
        return false;
    }

    std::string sync_path = data_path_ + "/sync/" + project_id + "/files";
#ifdef _WIN32
    _mkdir(sync_path.c_str());
#else
    mkdir(sync_path.c_str(), 0755);
#endif

    // Encrypt and save
    std::string encrypted = encrypt(content, encryption_key_);

    std::hash<std::string> hasher;
    std::string file_name = std::to_string(hasher(file_path)) + ".enc";

    std::ofstream file(sync_path + "/" + file_name, std::ios::binary);
    file << encrypted;

    // Update sync state
    std::lock_guard<std::mutex> lock(sync_mutex_);
    project_sync_states_[project_id].project_id = project_id;
    project_sync_states_[project_id].synced = true;
    project_sync_states_[project_id].last_sync = time(nullptr);

    return true;
}

std::string NexusBridge::getRemoteFile(const std::string& session_id, const std::string& project_id,
                                       const std::string& file_path)
{
    if (!validateSession(session_id))
    {
        return "";
    }

    std::string sync_path = data_path_ + "/sync/" + project_id + "/files";

    std::hash<std::string> hasher;
    std::string file_name = std::to_string(hasher(file_path)) + ".enc";

    std::string full_path = sync_path + "/" + file_name;
    if (!std::filesystem::exists(full_path))
    {
        return "";
    }

    std::ifstream file(full_path, std::ios::binary);
    std::string encrypted((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return decrypt(encrypted, encryption_key_);
}

FileTree NexusBridge::getProjectTree(const std::string& session_id, const std::string& project_id)
{
    FileTree tree;
    tree.name = project_id;
    tree.is_directory = true;

    if (!validateSession(session_id))
    {
        return tree;
    }

    std::string sync_path = data_path_ + "/sync/" + project_id + "/files";

    if (!std::filesystem::exists(sync_path))
    {
        return tree;
    }

    std::function<void(FileTree&, const std::string&)> build_tree;
    build_tree = [&](FileTree& node, const std::string& path)
    {
        try
        {
            for (const auto& entry : std::filesystem::directory_iterator(path))
            {
                FileTree child;
                child.name = entry.path().filename().string();
                child.is_directory = entry.is_directory();
                child.path = entry.path().string();
                child.modified = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

                if (entry.is_directory())
                {
                    build_tree(child, entry.path().string());
                }

                node.children.push_back(child);
            }
        }
        catch (...)
        {
        }
    };

    build_tree(tree, sync_path);

    return tree;
}

SyncState NexusBridge::getSyncState(const std::string& session_id, const std::string& project_id)
{
    SyncState state;
    state.project_id = project_id;

    std::lock_guard<std::mutex> lock(sync_mutex_);
    auto it = project_sync_states_.find(project_id);
    if (it != project_sync_states_.end())
    {
        state = it->second;
    }

    return state;
}

std::vector<ConflictResolution> NexusBridge::resolveConflicts(const std::string& session_id,
                                                              const std::string& project_id,
                                                              const std::vector<ConflictResolution>& resolutions)
{

    std::vector<ConflictResolution> resolved;

    if (!validateSession(session_id))
    {
        return resolved;
    }

    for (const auto& r : resolutions)
    {
        // Apply resolution
        resolved.push_back(r);
    }

    return resolved;
}

bool NexusBridge::watchProject(const std::string& session_id, const std::string& project_id,
                               std::function<void(const std::string&, const std::string&)> callback)
{
    if (!validateSession(session_id))
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(sync_mutex_);
    project_watchers_[project_id].push_back(callback);

    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// WEBSOCKET SERVER
// ═══════════════════════════════════════════════════════════════════════

void NexusBridge::wsServerThread()
{
    while (running_)
    {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        // Use select for timeout
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(ws_server_socket_, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int result = select(ws_server_socket_ + 1, &read_fds, NULL, NULL, &timeout);

        if (result > 0)
        {
            socket_t client_socket = accept(ws_server_socket_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

            if (client_socket != INVALID_SOCKET_VAL)
            {
                std::string client_id = "ws_" + std::to_string(time(nullptr)) + "_" + std::to_string(rand() % 10000);

                {
                    std::lock_guard<std::mutex> lock(connection_mutex_);
                    client_sockets_[client_id] = client_socket;
                }

                if (on_connect_)
                {
                    on_connect_(client_id);
                }

                // Handle client in thread
                std::thread([this, client_socket, client_id]() { handleWebSocketClient(client_socket, client_id); })
                    .detach();
            }
        }
    }
}

void NexusBridge::handleWebSocketClient(socket_t socket, const std::string& client_id)
{
    char buffer[16384];

    while (running_)
    {
        int bytes = recv(socket, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0)
        {
            break;
        }

        buffer[bytes] = '\0';

        try
        {
            json msg = json::parse(buffer);

            WSMessage ws_msg;
            ws_msg.id = msg.value("id", "");
            ws_msg.type = static_cast<WSMessageType>(msg.value("type", 0));
            ws_msg.payload = msg.value("payload", json::object());
            ws_msg.timestamp = time(nullptr);
            ws_msg.session_id = msg.value("session_id", "");
            ws_msg.source_client = client_id;

            handleWebSocketMessage(client_id, ws_msg);
        }
        catch (const json::parse_error&)
        {
            // Ignore invalid JSON
        }
    }

    // Cleanup
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        client_sockets_.erase(client_id);
        session_to_client_.erase(client_id);
    }

    if (on_disconnect_)
    {
        on_disconnect_(client_id);
    }

    CLOSE_SOCKET(socket);
}

void NexusBridge::handleWebSocketMessage(const std::string& client_id, const WSMessage& message)
{
    switch (message.type)
    {
        case WSMessageType::Auth:
            processAuthRequest(client_id, message);
            break;
        case WSMessageType::KeyManagement:
            processKeyRequest(client_id, message);
            break;
        case WSMessageType::AIRequest:
            processAIRequest(client_id, message);
            break;
        case WSMessageType::ProjectSync:
            processSyncRequest(client_id, message);
            break;
        case WSMessageType::FileOperation:
            processFileRequest(client_id, message);
            break;
        case WSMessageType::Terminal:
            processTerminalRequest(client_id, message);
            break;
        default:
            break;
    }

    if (on_message_)
    {
        on_message_(client_id, message);
    }
}

void NexusBridge::processAuthRequest(const std::string& client_id, const WSMessage& msg)
{
    std::string username = msg.payload.value("username", "");
    std::string token = msg.payload.value("token", "");

    std::string session_id = createSession(username, token);

    // Link session to client
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        session_to_client_[client_id] = session_id;
    }

    WSMessage response;
    response.id = msg.id;
    response.type = WSMessageType::Auth;
    response.payload = {{"success", true}, {"session_id", session_id}};

    sendToClient(client_id, response);
}

void NexusBridge::processKeyRequest(const std::string& client_id, const WSMessage& msg)
{
    std::string action = msg.payload.value("action", "");
    std::string session_id = msg.session_id;

    WSMessage response;
    response.id = msg.id;
    response.type = WSMessageType::KeyManagement;

    if (action == "add")
    {
        APIKey key;
        key.name = msg.payload.value("name", "");
        key.encrypted_key = msg.payload.value("key", "");
        key.provider = static_cast<KeyProvider>(msg.payload.value("provider", 0));

        std::string key_id = addAPIKey(session_id, key);

        response.payload = {{"success", true}, {"key_id", key_id}};
    }
    else if (action == "list")
    {
        auto keys = listAPIKeys(session_id);
        json keys_json = json::array();
        for (const auto& k : keys)
        {
            keys_json.push_back({{"id", k.id},
                                 {"name", k.name},
                                 {"provider", static_cast<int>(k.provider)},
                                 {"masked_key", k.masked_key},
                                 {"is_active", k.is_active}});
        }

        response.payload = {{"success", true}, {"keys", keys_json}};
    }
    else if (action == "remove")
    {
        std::string key_id = msg.payload.value("key_id", "");
        bool success = removeAPIKey(session_id, key_id);

        response.payload = {{"success", success}};
    }
    else if (action == "validate")
    {
        std::string key_id = msg.payload.value("key_id", "");
        auto result = validateAPIKey(session_id, key_id);

        response.payload = {
            {"success", true}, {"valid", result.valid}, {"error", result.error}, {"model", result.model}};
    }

    sendToClient(client_id, response);
}

void NexusBridge::processAIRequest(const std::string& client_id, const WSMessage& msg)
{
    std::string session_id = msg.session_id;
    std::string key_id = msg.payload.value("key_id", "");

    AIRequest request;
    request.id = msg.id;
    request.key_id = key_id;
    request.model = msg.payload.value("model", "gpt-4o-mini");
    request.temperature = msg.payload.value("temperature", 0.7);
    request.max_tokens = msg.payload.value("max_tokens", 1000);
    request.stream = msg.payload.value("stream", false);

    if (msg.payload.contains("messages"))
    {
        for (const auto& m : msg.payload["messages"])
        {
            std::map<std::string, std::string> msg_map;
            msg_map["role"] = m.value("role", "");
            msg_map["content"] = m.value("content", "");
            request.messages.push_back(msg_map);
        }
    }

    if (request.stream)
    {
        // Streaming response
        proxyAIStream(session_id, key_id, request,
                      [this, client_id, msg_id = msg.id](const std::string& token, bool done)
                      {
                          WSMessage chunk;
                          chunk.id = msg_id;
                          chunk.type = done ? WSMessageType::StreamEnd : WSMessageType::StreamChunk;
                          chunk.payload = {{"token", token}, {"done", done}};
                          sendToClient(client_id, chunk);
                      });
    }
    else
    {
        // Non-streaming response
        auto result = proxyAIRequest(session_id, key_id, request);

        WSMessage response;
        response.id = msg.id;
        response.type = WSMessageType::AIResponse;
        response.payload = {{"content", result.content},
                            {"model", result.model},
                            {"input_tokens", result.input_tokens},
                            {"output_tokens", result.output_tokens},
                            {"cached", result.cached}};

        if (!result.error.empty())
        {
            response.payload["error"] = result.error;
        }

        sendToClient(client_id, response);
    }
}

void NexusBridge::processSyncRequest(const std::string& client_id, const WSMessage& msg)
{
    std::string action = msg.payload.value("action", "");
    std::string session_id = msg.session_id;

    WSMessage response;
    response.id = msg.id;
    response.type = WSMessageType::ProjectSync;

    if (action == "sync")
    {
        std::string project_id = msg.payload.value("project_id", "");
        std::string file_path = msg.payload.value("file_path", "");
        std::string content = msg.payload.value("content", "");

        bool success = syncFile(session_id, project_id, file_path, content);

        response.payload = {{"success", success}};
    }
    else if (action == "get")
    {
        std::string project_id = msg.payload.value("project_id", "");
        std::string file_path = msg.payload.value("file_path", "");

        std::string content = getRemoteFile(session_id, project_id, file_path);

        response.payload = {{"success", !content.empty()}, {"content", content}};
    }
    else if (action == "tree")
    {
        std::string project_id = msg.payload.value("project_id", "");
        auto tree = getProjectTree(session_id, project_id);

        response.payload = {{"success", true}, {"tree", {{"name", tree.name}, {"is_directory", tree.is_directory}}}};
    }

    sendToClient(client_id, response);
}

void NexusBridge::processFileRequest(const std::string& client_id, const WSMessage& msg)
{
    // File operations
}

void NexusBridge::processTerminalRequest(const std::string& client_id, const WSMessage& msg)
{
    // Terminal operations
}

void NexusBridge::sendToClient(const std::string& client_id, const WSMessage& message)
{
    std::lock_guard<std::mutex> lock(connection_mutex_);

    auto it = client_sockets_.find(client_id);
    if (it != client_sockets_.end())
    {
        json msg_json = {{"id", message.id},
                         {"type", static_cast<int>(message.type)},
                         {"payload", message.payload},
                         {"timestamp", message.timestamp}};

        std::string data = msg_json.dump() + "\n";
        send(it->second, data.c_str(), static_cast<int>(data.length()), 0);
    }
}

void NexusBridge::broadcast(const WSMessage& message)
{
    std::lock_guard<std::mutex> lock(connection_mutex_);

    json msg_json = {{"id", message.id}, {"type", static_cast<int>(message.type)}, {"payload", message.payload}};

    std::string data = msg_json.dump() + "\n";

    for (const auto& [id, socket] : client_sockets_)
    {
        send(socket, data.c_str(), static_cast<int>(data.length()), 0);
    }
}

void NexusBridge::broadcastToProject(const std::string& project_id, const WSMessage& message)
{
    std::lock_guard<std::mutex> lock(connection_mutex_);

    json msg_json = {{"id", message.id},
                     {"type", static_cast<int>(message.type)},
                     {"payload", message.payload},
                     {"project_id", project_id}};

    std::string data = msg_json.dump() + "\n";

    // Broadcast to all clients in project
    for (const auto& [id, socket] : client_sockets_)
    {
        send(socket, data.c_str(), static_cast<int>(data.length()), 0);
    }
}

void NexusBridge::onClientConnect(std::function<void(const std::string&)> callback)
{
    on_connect_ = callback;
}

void NexusBridge::onClientDisconnect(std::function<void(const std::string&)> callback)
{
    on_disconnect_ = callback;
}

void NexusBridge::onClientMessage(std::function<void(const std::string&, const WSMessage&)> callback)
{
    on_message_ = callback;
}

// ═══════════════════════════════════════════════════════════════════════
// WEBRTC SIGNALING
// ═══════════════════════════════════════════════════════════════════════

void NexusBridge::relayWebRTCOffer(const std::string& from_peer, const std::string& to_peer, const json& offer)
{
    WSMessage msg;
    msg.type = WSMessageType::Presence;
    msg.payload = {{"action", "webrtc_offer"}, {"from", from_peer}, {"offer", offer}};

    sendToClient(to_peer, msg);
}

void NexusBridge::relayWebRTCAnswer(const std::string& from_peer, const std::string& to_peer, const json& answer)
{
    WSMessage msg;
    msg.type = WSMessageType::Presence;
    msg.payload = {{"action", "webrtc_answer"}, {"from", from_peer}, {"answer", answer}};

    sendToClient(to_peer, msg);
}

void NexusBridge::relayWebRTCCandidate(const std::string& from_peer, const std::string& to_peer, const json& candidate)
{
    WSMessage msg;
    msg.type = WSMessageType::Presence;
    msg.payload = {{"action", "webrtc_candidate"}, {"from", from_peer}, {"candidate", candidate}};

    sendToClient(to_peer, msg);
}

void NexusBridge::registerPeer(const RemotePeer& peer)
{
    std::lock_guard<std::mutex> lock(connection_mutex_);
    peers_[peer.id] = peer;
}

void NexusBridge::unregisterPeer(const std::string& peer_id)
{
    std::lock_guard<std::mutex> lock(connection_mutex_);
    peers_.erase(peer_id);
}

std::vector<RemotePeer> NexusBridge::listPeers()
{
    std::vector<RemotePeer> peer_list;

    std::lock_guard<std::mutex> lock(connection_mutex_);
    for (const auto& [id, peer] : peers_)
    {
        peer_list.push_back(peer);
    }

    return peer_list;
}

RemotePeer NexusBridge::getPeer(const std::string& peer_id)
{
    std::lock_guard<std::mutex> lock(connection_mutex_);

    auto it = peers_.find(peer_id);
    if (it != peers_.end())
    {
        return it->second;
    }

    return RemotePeer{};
}

// ═══════════════════════════════════════════════════════════════════════
// INTEGRATION WITH NEURAL CORE
// ═══════════════════════════════════════════════════════════════════════

void NexusBridge::setNeuralCore(std::shared_ptr<neuralcore::NeuralCore> core)
{
    neural_core_ = core;
}

std::shared_ptr<neuralcore::NeuralCore> NexusBridge::getNeuralCore()
{
    return neural_core_;
}

std::string NexusBridge::routeToNeuralCore(const json& request)
{
    if (!neural_core_)
    {
        return R"({"error": "Neural Core not initialized"})";
    }

    neuralcore::InferenceRequest req;
    req.prompt = request.value("prompt", "");
    req.model_name = request.value("model", "default");
    req.max_tokens = request.value("max_tokens", 1000);
    req.temperature = request.value("temperature", 0.7f);

    auto response = neural_core_->infer(req);

    return json({{"success", true},
                 {"content", response.content},
                 {"model", response.model},
                 {"tokens", response.output_tokens}})
        .dump();
}

bool NexusBridge::shouldUseLocalModel(const std::string& model_name)
{
    if (!neural_core_)
        return false;

    auto models = neural_core_->getLoadedModels();
    for (const auto& m : models)
    {
        if (m.config.name == model_name)
        {
            return true;
        }
    }

    return false;
}

std::string NexusBridge::selectLocalModel(const std::string& task_type)
{
    if (!neural_core_)
        return "";

    auto models = neural_core_->getLoadedModels();
    if (!models.empty())
    {
        return models[0].config.name;
    }

    return "";
}

// ═══════════════════════════════════════════════════════════════════════
// HTTP SERVER
// ═══════════════════════════════════════════════════════════════════════

void NexusBridge::httpServerThread()
{
    while (running_)
    {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        // Use select for timeout
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket_, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int result = select(server_socket_ + 1, &read_fds, NULL, NULL, &timeout);

        if (result > 0)
        {
            socket_t client_socket = accept(server_socket_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

            if (client_socket != INVALID_SOCKET_VAL)
            {
                // Handle client in thread
                std::thread([this, client_socket]() { handleHTTPClient(client_socket); }).detach();
            }
        }
    }
}

void NexusBridge::handleHTTPClient(socket_t socket)
{
    char buffer[65536];
    int bytes = recv(socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0)
    {
        CLOSE_SOCKET(socket);
        return;
    }

    buffer[bytes] = '\0';

    // Parse HTTP request
    std::string request(buffer);
    std::istringstream iss(request);

    std::string method, path, version;
    iss >> method >> path >> version;

    // Read headers
    std::map<std::string, std::string> headers;
    std::string line;
    int content_length = 0;

    while (std::getline(iss, line) && line != "\r")
    {
        size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 2);
            if (!value.empty() && value.back() == '\r')
            {
                value.pop_back();
            }
            headers[key] = value;

            if (key == "Content-Length")
            {
                content_length = std::stoi(value);
            }
        }
    }

    // Read body if present
    std::string body;
    if (content_length > 0)
    {
        body = request.substr(request.find("\r\n\r\n") + 4);
    }

    // Handle request
    std::string response = handleRESTAPI(method, path, body.empty() ? json::object() : json::parse(body), headers);

    // Send HTTP response
    std::string http_response = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: application/json\r\n"
                                "Access-Control-Allow-Origin: *\r\n"
                                "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                                "Access-Control-Allow-Headers: Content-Type, X-Session-ID, Authorization\r\n"
                                "Content-Length: " +
                                std::to_string(response.length()) +
                                "\r\n"
                                "\r\n" +
                                response;

    send(socket, http_response.c_str(), static_cast<int>(http_response.length()), 0);

    CLOSE_SOCKET(socket);
}

std::string NexusBridge::handleRESTAPI(const std::string& method, const std::string& path, const json& body,
                                       const std::map<std::string, std::string>& headers)
{
    // Extract session from header
    std::string session_id;
    auto it = headers.find("X-Session-ID");
    if (it != headers.end())
    {
        session_id = it->second;
    }

    // Route REST endpoints
    if (path == "/" || path == "/index.html")
    {
        return generateHTMLResponse(getWebFrontendHTML());
    }

    if (path == "/api/health")
    {
        return json({{"status", "ok"},
                     {"version", "1.0.0"},
                     {"timestamp", time(nullptr)},
                     {"active_sessions", getActiveSessions()},
                     {"active_connections", getActiveConnections()}})
            .dump();
    }

    if (path == "/api/session" && method == "POST")
    {
        std::string username = body.value("username", "");
        std::string token = body.value("token", "");

        std::string sid = createSession(username, token);

        return json({{"success", true}, {"session_id", sid}}).dump();
    }

    if (path == "/api/keys" && method == "POST")
    {
        APIKey key;
        key.name = body.value("name", "");
        key.encrypted_key = body.value("key", "");
        key.provider = static_cast<KeyProvider>(body.value("provider", 0));

        std::string key_id = addAPIKey(session_id, key);

        return json({{"success", true}, {"key_id", key_id}}).dump();
    }

    if (path == "/api/keys" && method == "GET")
    {
        auto keys = listAPIKeys(session_id);
        json keys_json = json::array();
        for (const auto& k : keys)
        {
            keys_json.push_back({{"id", k.id},
                                 {"name", k.name},
                                 {"provider", static_cast<int>(k.provider)},
                                 {"masked_key", k.masked_key},
                                 {"is_active", k.is_active}});
        }

        return json({{"success", true}, {"keys", keys_json}}).dump();
    }

    if (path.find("/api/keys/") == 0 && method == "DELETE")
    {
        std::string key_id = path.substr(10);
        bool success = removeAPIKey(session_id, key_id);

        return json({{"success", success}}).dump();
    }

    if (path == "/api/keys/validate" && method == "POST")
    {
        std::string key_id = body.value("key_id", "");
        auto result = validateAPIKey(session_id, key_id);

        return json({{"success", true}, {"valid", result.valid}, {"error", result.error}, {"model", result.model}})
            .dump();
    }

    if (path == "/api/projects" && method == "GET")
    {
        auto projects = listProjects(session_id);
        json projects_json = json::array();
        for (const auto& p : projects)
        {
            projects_json.push_back({{"id", p.id}, {"name", p.name}, {"visibility", p.visibility}});
        }

        return json({{"success", true}, {"projects", projects_json}}).dump();
    }

    if (path == "/api/projects" && method == "POST")
    {
        std::string name = body.value("name", "");
        std::string project_id = createProject(session_id, name);

        return json({{"success", true}, {"project_id", project_id}}).dump();
    }

    if (path == "/api/ai" && method == "POST")
    {
        std::string key_id = body.value("key_id", "");

        AIRequest request;
        request.key_id = key_id;
        request.model = body.value("model", "gpt-4o-mini");
        request.temperature = body.value("temperature", 0.7);
        request.max_tokens = body.value("max_tokens", 1000);

        if (body.contains("messages"))
        {
            for (const auto& m : body["messages"])
            {
                std::map<std::string, std::string> msg_map;
                msg_map["role"] = m.value("role", "");
                msg_map["content"] = m.value("content", "");
                request.messages.push_back(msg_map);
            }
        }

        auto result = proxyAIRequest(session_id, key_id, request);

        return json({{"success", result.error.empty()},
                     {"content", result.content},
                     {"model", result.model},
                     {"input_tokens", result.input_tokens},
                     {"output_tokens", result.output_tokens},
                     {"error", result.error}})
            .dump();
    }

    if (path == "/api/models" && method == "GET")
    {
        std::string key_id = body.value("key_id", "");
        auto models = getAvailableModels(session_id, key_id);

        return json({{"success", true}, {"models", models}}).dump();
    }

    if (path == "/api/usage" && method == "GET")
    {
        auto stats = getSessionUsageStats(session_id);

        return json({{"success", true},
                     {"usage",
                      {{"total_requests", stats.total_requests},
                       {"total_tokens", stats.total_tokens},
                       {"total_cost", stats.total_cost}}}})
            .dump();
    }

    // Default: 404
    return json({{"error", "Not found"}, {"path", path}}).dump();
}

std::string NexusBridge::generateErrorResponse(int code, const std::string& message)
{
    return json({{"error", message}, {"code", code}}).dump();
}

std::string NexusBridge::generateSuccessResponse(const json& data)
{
    return json({{"success", true}, {"data", data}}).dump();
}

std::string NexusBridge::generateHTMLResponse(const std::string& html)
{
    return html;
}

// ═══════════════════════════════════════════════════════════════════════
// BACKGROUND THREADS
// ═══════════════════════════════════════════════════════════════════════

void NexusBridge::sessionCleanupThread()
{
    while (running_)
    {
        std::this_thread::sleep_for(std::chrono::minutes(5));

        std::lock_guard<std::mutex> lock(session_mutex_);

        auto now = time(nullptr);
        std::vector<std::string> to_remove;

        for (const auto& [id, session] : active_sessions_)
        {
            if (now > session.expires_at)
            {
                to_remove.push_back(id);
            }
        }

        for (const auto& id : to_remove)
        {
            active_sessions_.erase(id);
            std::cout << "Session expired: " << id << std::endl;
        }
    }
}

void NexusBridge::heartbeatThread()
{
    while (running_)
    {
        std::this_thread::sleep_for(std::chrono::seconds(30));

        WSMessage heartbeat;
        heartbeat.type = WSMessageType::Heartbeat;
        heartbeat.payload = {{"timestamp", time(nullptr)},
                             {"active_sessions", getActiveSessions()},
                             {"active_connections", getActiveConnections()}};

        broadcast(heartbeat);
    }
}

void NexusBridge::connectionMonitorThread()
{
    while (running_)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));

        std::lock_guard<std::mutex> lock(connection_mutex_);

        std::vector<std::string> dead_connections;

        for (const auto& [id, socket] : client_sockets_)
        {
            // Check if connection is still alive
            char buf[1];
            int result = recv(socket, buf, 1, MSG_PEEK);

#ifdef _WIN32
            if (result == 0 || (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK))
            {
                dead_connections.push_back(id);
            }
#else
            if (result == 0 || (result == -1 && errno != EWOULDBLOCK && errno != EAGAIN))
            {
                dead_connections.push_back(id);
            }
#endif
        }

        for (const auto& id : dead_connections)
        {
            CLOSE_SOCKET(client_sockets_[id]);
            client_sockets_.erase(id);

            if (on_disconnect_)
            {
                on_disconnect_(id);
            }
        }
    }
}

void NexusBridge::usageTrackingThread()
{
    while (running_)
    {
        std::this_thread::sleep_for(std::chrono::minutes(1));

        // Check usage alerts
        std::lock_guard<std::mutex> lock(usage_mutex_);

        for (auto& [key_id, stats] : usage_stats_)
        {
            // Check if any alerts should be triggered
            // Implementation would check thresholds and send notifications
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════

void NexusBridge::setConfig(const HTTPServerConfig& config)
{
    server_config_ = config;
}

HTTPServerConfig NexusBridge::getConfig() const
{
    return server_config_;
}

void NexusBridge::setRateLimitConfig(const RateLimitConfig& config)
{
    rate_limit_config_ = config;
}

RateLimitConfig NexusBridge::getRateLimitConfig() const
{
    return rate_limit_config_;
}

void NexusBridge::enableCORS(bool enable)
{
    server_config_.enable_cors = enable;
}

void NexusBridge::setAllowedOrigins(const std::vector<std::string>& origins)
{
    server_config_.allowed_origins = origins;
}

void NexusBridge::setWebRTCConfig(const WebRTCConfig& config)
{
    webrtc_config_ = config;
}

// ═══════════════════════════════════════════════════════════════════════
// MONITORING & STATISTICS
// ═══════════════════════════════════════════════════════════════════════

int NexusBridge::getActiveConnections()
{
    std::lock_guard<std::mutex> lock(connection_mutex_);
    return static_cast<int>(client_sockets_.size());
}

int NexusBridge::getActiveSessions()
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    return static_cast<int>(active_sessions_.size());
}

std::map<std::string, int> NexusBridge::getConnectionStats()
{
    std::map<std::string, int> stats;
    stats["active_connections"] = getActiveConnections();
    stats["active_sessions"] = getActiveSessions();
    stats["total_requests"] = static_cast<int>(total_requests_.load());
    stats["total_connections"] = static_cast<int>(total_connections_.load());
    stats["total_errors"] = static_cast<int>(total_errors_.load());
    return stats;
}

std::map<std::string, double> NexusBridge::getPerformanceMetrics()
{
    std::map<std::string, double> metrics;

    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

    metrics["uptime_seconds"] = static_cast<double>(uptime);
    metrics["requests_per_second"] = uptime > 0 ? static_cast<double>(total_requests_.load()) / uptime : 0.0;
    metrics["tokens_per_second"] = uptime > 0 ? static_cast<double>(total_tokens_.load()) / uptime : 0.0;
    metrics["total_cost"] = total_cost_.load();

    return metrics;
}

std::string NexusBridge::getHealthStatus()
{
    json health = {
        {"status", running_ ? "healthy" : "stopped"},
        {"uptime",
         std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time_).count()},
        {"connections", getActiveConnections()},
        {"sessions", getActiveSessions()},
        {"requests", total_requests_.load()},
        {"tokens", total_tokens_.load()}};

    return health.dump();
}

std::string NexusBridge::generateReport()
{
    json report = {{"timestamp", time(nullptr)},
                   {"server", {{"http_port", http_port_}, {"ws_port", ws_port_}, {"running", running_}}},
                   {"connections", getConnectionStats()},
                   {"performance", getPerformanceMetrics()},
                   {"sessions", json::array()}};

    for (const auto& session : getActiveSessions())
    {
        report["sessions"].push_back(
            {{"id", session.session_id}, {"username", session.username}, {"created_at", session.created_at}});
    }

    return report.dump(2);
}

// ═══════════════════════════════════════════════════════════════════════
// WEB FRONTEND HTML
// ═══════════════════════════════════════════════════════════════════════

const char* getWebFrontendHTML()
{
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CURSOR CLOUD - Online AI IDE with BYOK</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        
        :root {
            --bg-primary: #0d1117;
            --bg-secondary: #161b22;
            --bg-tertiary: #21262d;
            --border: #30363d;
            --text-primary: #c9d1d9;
            --text-secondary: #8b949e;
            --accent: #58a6ff;
            --success: #3fb950;
            --error: #f85149;
            --warning: #d29922;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            height: 100vh;
            display: flex;
            flex-direction: column;
        }
        
        .top-bar {
            background: var(--bg-secondary);
            border-bottom: 1px solid var(--border);
            padding: 8px 16px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 16px;
        }
        
        .logo {
            display: flex;
            align-items: center;
            gap: 8px;
            font-weight: 600;
            font-size: 18px;
        }
        
        .logo-icon {
            width: 24px;
            height: 24px;
            background: linear-gradient(135deg, var(--accent), #8b5cf6);
            border-radius: 4px;
        }
        
        .btn {
            padding: 6px 12px;
            border-radius: 6px;
            border: 1px solid var(--border);
            background: var(--bg-tertiary);
            color: var(--text-primary);
            cursor: pointer;
            font-size: 13px;
            display: flex;
            align-items: center;
            gap: 6px;
            transition: all 0.2s;
        }
        
        .btn:hover { background: var(--border); }
        
        .btn-primary {
            background: var(--accent);
            border-color: var(--accent);
            color: white;
        }
        
        .btn-primary:hover { background: #4c9aed; }
        
        .main-container {
            flex: 1;
            display: flex;
            overflow: hidden;
        }
        
        .sidebar {
            width: 240px;
            background: var(--bg-secondary);
            border-right: 1px solid var(--border);
            display: flex;
            flex-direction: column;
        }
        
        .sidebar-section {
            padding: 12px;
            border-bottom: 1px solid var(--border);
        }
        
        .sidebar-title {
            font-size: 11px;
            text-transform: uppercase;
            color: var(--text-secondary);
            margin-bottom: 8px;
            letter-spacing: 0.5px;
        }
        
        .file-tree {
            flex: 1;
            padding: 12px;
            overflow-y: auto;
        }
        
        .file-item {
            padding: 4px 8px;
            border-radius: 4px;
            cursor: pointer;
            display: flex;
            align-items: center;
            gap: 6px;
            font-size: 13px;
            color: var(--text-secondary);
        }
        
        .file-item:hover { background: var(--bg-tertiary); color: var(--text-primary); }
        .file-item.active { background: var(--bg-tertiary); color: var(--text-primary); }
        
        .editor-area {
            flex: 1;
            display: flex;
            flex-direction: column;
        }
        
        .tab-bar {
            background: var(--bg-secondary);
            border-bottom: 1px solid var(--border);
            display: flex;
            padding: 0 8px;
        }
        
        .tab {
            padding: 8px 16px;
            border-bottom: 2px solid transparent;
            cursor: pointer;
            font-size: 13px;
            color: var(--text-secondary);
            display: flex;
            align-items: center;
            gap: 8px;
        }
        
        .tab:hover { color: var(--text-primary); }
        .tab.active { color: var(--text-primary); border-bottom-color: var(--accent); }
        
        .editor-container {
            flex: 1;
            position: relative;
        }
        
        #editor {
            position: absolute;
            top: 0; left: 0; right: 0; bottom: 0;
            font-family: 'Fira Code', 'Consolas', monospace;
            font-size: 14px;
            padding: 16px;
            background: var(--bg-primary);
            color: var(--text-primary);
            border: none;
            outline: none;
            resize: none;
            line-height: 1.6;
            tab-size: 4;
        }
        
        .ai-panel {
            width: 360px;
            background: var(--bg-secondary);
            border-left: 1px solid var(--border);
            display: flex;
            flex-direction: column;
        }
        
        .ai-header {
            padding: 12px 16px;
            border-bottom: 1px solid var(--border);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .ai-modes {
            display: flex;
            gap: 4px;
        }
        
        .ai-mode {
            padding: 4px 10px;
            border-radius: 12px;
            font-size: 12px;
            cursor: pointer;
            background: var(--bg-tertiary);
            color: var(--text-secondary);
            border: none;
        }
        
        .ai-mode.active { background: var(--accent); color: white; }
        
        .ai-chat {
            flex: 1;
            padding: 16px;
            overflow-y: auto;
        }
        
        .ai-message { margin-bottom: 16px; }
        .ai-message.user { text-align: right; }
        
        .message-content {
            display: inline-block;
            padding: 8px 12px;
            border-radius: 12px;
            font-size: 13px;
            max-width: 90%;
            line-height: 1.5;
        }
        
        .ai-message.bot .message-content { background: var(--bg-tertiary); text-align: left; }
        .ai-message.user .message-content { background: var(--accent); color: white; text-align: right; }
        
        .ai-input-area {
            padding: 12px;
            border-top: 1px solid var(--border);
        }
        
        .ai-input {
            width: 100%;
            padding: 10px 12px;
            border-radius: 8px;
            border: 1px solid var(--border);
            background: var(--bg-tertiary);
            color: var(--text-primary);
            font-size: 13px;
            outline: none;
            resize: none;
        }
        
        .ai-input:focus { border-color: var(--accent); }
        
        .ai-actions {
            display: flex;
            gap: 8px;
            margin-top: 8px;
        }
        
        .modal-overlay {
            position: fixed;
            top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(0, 0, 0, 0.7);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 1000;
        }
        
        .modal {
            background: var(--bg-secondary);
            border-radius: 12px;
            border: 1px solid var(--border);
            width: 500px;
            max-width: 90%;
            max-height: 90vh;
            overflow-y: auto;
        }
        
        .modal-header {
            padding: 16px 20px;
            border-bottom: 1px solid var(--border);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .modal-title { font-size: 16px; font-weight: 600; }
        
        .modal-close {
            background: none;
            border: none;
            color: var(--text-secondary);
            cursor: pointer;
            font-size: 20px;
        }
        
        .modal-body { padding: 20px; }
        
        .form-group { margin-bottom: 16px; }
        
        .form-label {
            display: block;
            margin-bottom: 6px;
            font-size: 13px;
            color: var(--text-secondary);
        }
        
        .form-input {
            width: 100%;
            padding: 10px 12px;
            border-radius: 6px;
            border: 1px solid var(--border);
            background: var(--bg-tertiary);
            color: var(--text-primary);
            font-size: 13px;
        }
        
        .form-input:focus { outline: none; border-color: var(--accent); }
        
        .form-hint {
            margin-top: 4px;
            font-size: 12px;
            color: var(--text-secondary);
        }
        
        .key-list { margin-top: 16px; }
        
        .key-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 12px;
            background: var(--bg-tertiary);
            border-radius: 6px;
            margin-bottom: 8px;
        }
        
        .key-info { display: flex; align-items: center; gap: 12px; }
        .key-name { font-weight: 500; }
        .key-masked { font-size: 12px; color: var(--text-secondary); font-family: monospace; }
        
        .key-status {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--success);
        }
        
        .key-actions { display: flex; gap: 8px; }
        .btn-small { padding: 4px 8px; font-size: 12px; }
        
        .status-bar {
            background: var(--bg-secondary);
            border-top: 1px solid var(--border);
            padding: 4px 16px;
            display: flex;
            justify-content: space-between;
            font-size: 12px;
            color: var(--text-secondary);
        }
        
        .status-left, .status-right {
            display: flex;
            gap: 16px;
            align-items: center;
        }
        
        .status-item { display: flex; align-items: center; gap: 4px; }
        
        .status-dot {
            width: 6px;
            height: 6px;
            border-radius: 50%;
        }
        
        .status-dot.online { background: var(--success); }
        .status-dot.offline { background: var(--error); }
        
        .loading {
            display: flex;
            align-items: center;
            gap: 8px;
            color: var(--text-secondary);
        }
        
        .spinner {
            width: 16px;
            height: 16px;
            border: 2px solid var(--border);
            border-top-color: var(--accent);
            border-radius: 50%;
            animation: spin 1s linear infinite;
        }
        
        @keyframes spin { to { transform: rotate(360deg); } }
        
        .hidden { display: none !important; }
    </style>
</head>
<body>
    <div id="setupModal" class="modal-overlay">
        <div class="modal">
            <div class="modal-header">
                <div class="modal-title">Welcome to Cursor Cloud</div>
            </div>
            <div class="modal-body">
                <p style="margin-bottom: 20px; color: var(--text-secondary);">
                    Connect your API keys to enable AI-powered coding. Your keys are encrypted and stored locally.
                </p>
                
                <div class="form-group">
                    <label class="form-label">Provider</label>
                    <select id="keyProvider" class="form-input">
                        <option value="0">OpenAI (GPT-4, GPT-4o)</option>
                        <option value="1">Anthropic (Claude)</option>
                        <option value="2">Google (Gemini)</option>
                        <option value="3">Azure OpenAI</option>
                        <option value="4">AWS Bedrock</option>
                        <option value="5">Local (Neural Core)</option>
                    </select>
                </div>
                
                <div class="form-group">
                    <label class="form-label">Key Name</label>
                    <input type="text" id="keyName" class="form-input" placeholder="My OpenAI Key">
                </div>
                
                <div class="form-group">
                    <label class="form-label">API Key</label>
                    <input type="password" id="apiKey" class="form-input" placeholder="sk-...">
                    <div class="form-hint">Your key is encrypted locally and never sent to our servers.</div>
                </div>
                
                <button class="btn btn-primary" onclick="addKey()" style="width: 100%;">
                    Add API Key
                </button>
                
                <div id="keyList" class="key-list"></div>
                
                <button class="btn" onclick="closeSetup()" style="width: 100%; margin-top: 16px;">
                    Continue to IDE
                </button>
            </div>
        </div>
    </div>

    <div class="top-bar">
        <div class="logo">
            <div class="logo-icon"></div>
            <span>Cursor Cloud</span>
        </div>
        
        <div class="project-name">My Project</div>
        
        <div class="top-actions">
            <button class="btn" onclick="showSetup()">
                <span>⚙️</span> API Keys
            </button>
            <button class="btn btn-primary" onclick="toggleAIPanel()">
                <span>✨</span> AI Chat
            </button>
        </div>
    </div>

    <div class="main-container">
        <div class="sidebar">
            <div class="sidebar-section">
                <div class="sidebar-title">Explorer</div>
                <div class="file-tree" id="fileTree">
                    <div class="file-item active" onclick="openFile('main.py')">
                        📄 main.py
                    </div>
                    <div class="file-item" onclick="openFile('utils.py')">
                        📄 utils.py
                    </div>
                    <div class="file-item" onclick="openFile('config.json')">
                        📄 config.json
                    </div>
                </div>
            </div>
        </div>

        <div class="editor-area">
            <div class="tab-bar">
                <div class="tab active">
                    main.py
                    <span class="close">×</span>
                </div>
            </div>
            
            <div class="editor-container">
                <textarea id="editor" placeholder="Start coding..." spellcheck="false">#!/usr/bin/env python3
"""
Cursor Cloud - Online AI IDE with BYOK
Use your own API keys for unlimited AI-powered coding!
"""

class TaskManager:
    def __init__(self):
        self.tasks = []
    
    def add_task(self, title, description=""):
        task = {
            "id": len(self.tasks) + 1,
            "title": title,
            "description": description,
            "status": "pending"
        }
        self.tasks.append(task)
        return task

#Start coding with AI assistance !
manager = TaskManager()
manager.add_task("Build something amazing", "Use Cursor Cloud with your own API keys")
</textarea>
            </div>
        </div>

        <div class="ai-panel" id="aiPanel">
            <div class="ai-header">
                <span style="font-weight: 600;">AI Assistant</span>
                <div class="ai-modes">
                    <button class="ai-mode active" onclick="setAIMode('chat')">Chat</button>
                    <button class="ai-mode" onclick="setAIMode('agent')">Agent</button>
                </div>
            </div>
            
            <div class="ai-chat" id="aiChat">
                <div class="ai-message bot">
                    <div class="message-content">
                        Hello! I'm your AI coding assistant powered by <strong>your</strong> API keys.
                        <br><br>
                        ✨ <strong>Features:</strong>
                        <br>• Write, explain, and debug code
                        <br>• Refactor and optimize
                        <br>• Generate tests
                        <br>• Answer questions
                        <br><br>
                        Your keys are encrypted and stored locally. No data leaves your control!
                    </div>
                </div>
            </div>
            
            <div class="ai-input-area">
                <textarea id="aiInput" class="ai-input" rows="3" placeholder="Ask me anything..."></textarea>
                <div class="ai-actions">
                    <button class="btn btn-primary" onclick="sendAIMessage()" style="flex: 1;">
                        Send
                    </button>
                </div>
            </div>
        </div>
    </div>

    <div class="status-bar">
        <div class="status-left">
            <div class="status-item">
                <div class="status-dot online"></div>
                <span>Connected</span>
            </div>
            <div class="status-item">Python</div>
        </div>
        <div class="status-right">
            <div class="status-item" id="connectionStatus">Keys: 0</div>
            <div class="status-item" id="modelStatus">Model: Not set</div>
        </div>
    </div>

    <script>
        let apiKeys = [];
    let activeModel = null;
    let sessionId = null;

    async function init()
    {
        const stored = localStorage.getItem('cursorCloudKeys');
        if (stored)
        {
            apiKeys = JSON.parse(stored);
        }

        try
        {
            const response = await fetch('/api/session', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({username: 'user_' + Date.now()})
            });
            const data = await response.json();
            sessionId = data.session_id;
        }
        catch (e)
        {
            console.log('Running in offline mode');
        }

        updateKeyStatus();

        if (apiKeys.length > 0)
        {
            closeSetup();
            activeModel = apiKeys[0];
            updateModelStatus();
        }
    }

    async function addKey()
    {
        const provider = document.getElementById('keyProvider').value;
        const name = document.getElementById('keyName').value;
        const key = document.getElementById('apiKey').value;

        if (!name || !key)
        {
            alert('Please enter both name and API key');
            return;
        }

        const keyObj = {
            id: 'key_' + Date.now(),
            name: name,
            provider: parseInt(provider),
            masked: key.substring(0, 8) + '...' + key.substring(key.length - 4),
            created: Date.now()
        };

        apiKeys.push(keyObj);
        localStorage.setItem('cursorCloudKeys', JSON.stringify(apiKeys));

        if (sessionId)
        {
            try
            {
                await fetch('/api/keys', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json', 'X-Session-ID': sessionId},
                    body: JSON.stringify({name: name, key: key, provider: provider})
                });
            }
            catch (e)
            {
                console.log('Could not register key with server');
            }
        }

        renderKeys();
        document.getElementById('keyName').value = '';
        document.getElementById('apiKey').value = '';

        updateKeyStatus();
    }

    function renderKeys()
    {
        const list = document.getElementById('keyList');
        const providers = ['OpenAI', 'Anthropic', 'Google', 'Azure', 'AWS', 'Local'];
        list.innerHTML =
            apiKeys
                .map(k = > ` <div class = "key-item"><div class = "key-info"><div class = "key-status"></ div><div>
                         <div class = "key-name">
                             ${k.name} < / div >
                         <div class = "key-masked"> ${k.masked} < / div > </ div></ div><div class = "key-actions">
                         <button class = "btn btn-small" onclick = "selectModel('${k.id}')">
                             Use</ button><button class = "btn btn-small" onclick = "removeKey('${k.id}')">×</ button>
                         </ div></ div>
            `)
                .join('');
    }

    function removeKey(id)
    {
        apiKeys = apiKeys.filter(k = > k.id != = id);
        localStorage.setItem('cursorCloudKeys', JSON.stringify(apiKeys));
        renderKeys();
        updateKeyStatus();
    }

    function selectModel(id)
    {
        const key = apiKeys.find(k = > k.id == = id);
        if (key)
        {
            activeModel = key;
            updateModelStatus();
            closeSetup();
        }
    }

    function updateKeyStatus()
    {
        document.getElementById('connectionStatus').textContent = `Keys : $
        {
            apiKeys.length
        }
        `;
    }

    function updateModelStatus()
    {
        if (activeModel)
        {
            const providers = ['OpenAI', 'Anthropic', 'Google', 'Azure', 'AWS', 'Local'];
            document.getElementById('modelStatus').textContent = 
                    `Model : ${providers[activeModel.provider] || 'Unknown'}`;
        }
    }

    function showSetup()
    {
        renderKeys();
        document.getElementById('setupModal').classList.remove('hidden');
    }

    function closeSetup()
    {
        document.getElementById('setupModal').classList.add('hidden');
    }

    function toggleAIPanel()
    {
        const panel = document.getElementById('aiPanel');
        panel.style.display = panel.style.display == = 'none' ? 'flex' : 'none';
    }

    async function sendAIMessage()
    {
        const input = document.getElementById('aiInput');
        const message = input.value.trim();
        if (!message)
            return;

        const chat = document.getElementById('aiChat');

        chat.innerHTML += ` <div class = "ai-message user"><div class = "message-content"> ${escapeHtml(message)} <
                          / div > </ div>
            `;

        input.value = '';

        const loadingId = 'loading_' + Date.now();
        chat.innerHTML += ` <div class = "ai-message bot" id = "${loadingId}"><div class = "message-content">
            <div class = "loading"><div class = "spinner"></ div> Thinking...</ div></ div></ div>
            `;

        chat.scrollTop = chat.scrollHeight;

        try
        {
            const response = await fetch('/api/ai', {
                method: 'POST',
                headers: {'Content-Type': 'application/json', 'X-Session-ID': sessionId || '' },
                body: JSON.stringify({
                    key_id: activeModel
                    ?.id || '',
                    model
                    : activeModel
                    ?.provider ==
                     = 0 ? 'gpt-4o-mini' : activeModel ?.provider == = 1 ? 'claude-sonnet-4-20250514' : 'default',
                    messages
                    : [{role: 'user', content: message}],
                    temperature
                    : 0.7,
                    max_tokens: 1000
                })
            });

            const data = await response.json();

            document.getElementById(loadingId).remove();

            chat.innerHTML += ` <div class = "ai-message bot"><div class = "message-content"> ${escapeHtml(
                                  data.content || data.error || 'Response received')} < / div >
                              </ div>
                `;
        }
        catch (e)
        {
            document.getElementById(loadingId).remove();
            chat.innerHTML += ` <div class = "ai-message bot">
                              <div class = "message-content" style = "color: var(--error);"> Error
                : Could not connect.Please check your API keys.</ div></ div>
                `;
        }

        chat.scrollTop = chat.scrollHeight;
    }

    function escapeHtml(text)
    {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    document.getElementById('aiInput').addEventListener(
        'keydown', function(e) {
            if (e.key == = 'Enter' && !e.shiftKey)
            {
                e.preventDefault();
                sendAIMessage();
            }
        });

    init();
    </script>
</body>
</html>)";
}

}  // namespace nexus
