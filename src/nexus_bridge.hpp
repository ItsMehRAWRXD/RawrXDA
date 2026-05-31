#ifndef NEXUS_BRIDGE_HPP
#define NEXUS_BRIDGE_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <future>
#include <set>
#include <unordered_map>
#include <deque>
#include <fstream>
#include <sstream>
#include <regex>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #include <windows.h>
    #include <direct.h>
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR SOCKET_ERROR
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <poll.h>
    #define CLOSE_SOCKET close
    typedef int socket_t;
    #define INVALID_SOCKET_VAL -1
#endif

#include "json.hpp"
#include "neural_core.hpp"

using json = nlohmann::json;

namespace nexus {

// ═══════════════════════════════════════════════════════════════════════
// KEY MANAGEMENT TYPES
// ═══════════════════════════════════════════════════════════════════════

enum class KeyProvider {
    OpenAI,
    Anthropic,
    Google,
    Azure,
    AWS,
    Local,
    Custom
};

struct APIKey {
    std::string id;
    std::string name;
    KeyProvider provider;
    std::string encrypted_key;
    std::string masked_key;      // e.g., "sk-...xxxx"
    time_t created_at;
    time_t last_used;
    int usage_count;
    double total_spent;
    bool is_active;
    int rate_limit_rpm;
    std::map<std::string, std::string> metadata;
};

struct KeyValidationResult {
    bool valid;
    std::string error;
    int quota_remaining;
    int quota_limit;
    std::string model;
    double cost_per_1k;
};

struct UsageStats {
    int total_requests;
    int successful_requests;
    int failed_requests;
    int64_t total_tokens;
    double total_cost;
    std::map<std::string, int> requests_by_model;
    std::map<std::string, int64_t> tokens_by_model;
};

struct KeyQuota {
    std::string provider;
    int limit_per_minute;
    int limit_per_day;
    int limit_per_month;
    double cost_limit;
    bool unlimited;
};

struct UsageAlert {
    int usage_percent;    // e.g., 80 for 80%
    bool sent;
    time_t last_sent;
};

// ═══════════════════════════════════════════════════════════════════════
// USER & SESSION TYPES
// ═══════════════════════════════════════════════════════════════════════

struct UserSession {
    std::string session_id;
    std::string user_id;
    std::string username;
    std::string email;
    std::vector<std::string> active_keys;
    std::string current_project;
    time_t created_at;
    time_t last_activity;
    std::string ip_address;
    std::string user_agent;
};

struct UserProfile {
    std::string id;
    std::string username;
    std::string email;
    std::string plan;  // free, pro, team, enterprise
    std::vector<APIKey> keys;
    UsageStats usage;
    std::map<std::string, std::string> preferences;
    std::vector<std::string> trusted_devices;
};

struct Project {
    std::string id;
    std::string name;
    std::string owner_id;
    std::string visibility;  // private, public, shared
    std::string last_branch;
    time_t last_opened;
    std::map<std::string, std::string> metadata;
};

// ═══════════════════════════════════════════════════════════════════════
// SYNC TYPES
// ═══════════════════════════════════════════════════════════════════════

enum class SyncDirection {
    Push,
    Pull,
    Bidirectional
};

struct SyncOperation {
    std::string id;
    std::string file_path;
    SyncDirection direction;
    time_t timestamp;
    std::string file_hash;
    std::string content;  // encrypted
    bool conflict;
    std::string resolved_version;
};

struct ConflictResolution {
    std::string file_path;
    std::string local_version;
    std::string remote_version;
    std::string resolved_version;
    std::string strategy;  // local, remote, manual, merge
};

struct SyncState {
    std::string project_id;
    bool synced;
    time_t last_sync;
    std::vector<std::string> pending_changes;
    std::vector<std::string> conflicts;
};

struct FileTree {
    std::string path;
    std::string name;
    bool is_directory;
    int64_t size;
    time_t modified;
    std::vector<FileTree> children;
};

// ═══════════════════════════════════════════════════════════════════════
// WEBSOCKET MESSAGE TYPES
// ═══════════════════════════════════════════════════════════════════════

enum class WSMessageType {
    Auth,
    KeyManagement,
    ProjectOpen,
    ProjectSync,
    FileOperation,
    Terminal,
    AIRequest,
    AIResponse,
    StreamChunk,
    Error,
    Heartbeat,
    Notification
};

struct WSMessage {
    std::string id;
    WSMessageType type;
    json payload;
    time_t timestamp;
    std::string session_id;
};

// ═══════════════════════════════════════════════════════════════════════
// REMOTE CONNECTION TYPES
// ═══════════════════════════════════════════════════════════════════════

enum class ConnectionStatus {
    Connected,
    Connecting,
    Disconnected,
    Error
};

struct RemotePeer {
    std::string id;
    std::string name;
    ConnectionStatus status;
    std::string external_ip;
    int port;
    time_t last_seen;
    std::string version;
};

struct WebRTCConfig {
    std::string stun_server;
    std::string turn_server;
    std::string turn_username;
    std::string turn_credential;
    int iceservers_timeout_ms;
};

struct TURNServer {
    std::string url;
    std::string username;
    std::string credential;
};

// ═══════════════════════════════════════════════════════════════════════
// BACKEND SERVER TYPES
// ═══════════════════════════════════════════════════════════════════════

struct HTTPServerConfig {
    int port;
    std::string bind_address;
    bool enable_tls;
    std::string cert_path;
    std::string key_path;
    int max_request_size_mb;
    int connection_timeout_ms;
    int keepalive_timeout_ms;
};

struct RateLimitConfig {
    int requests_per_minute;
    int requests_per_hour;
    int tokens_per_minute;
    bool per_ip_limiting;
    std::map<std::string, int> custom_limits;
};

struct ProxyConfig {
    std::string target_url;
    std::string path_prefix;
    std::map<std::string, std::string> headers;
    bool strip_prefix;
};

struct SessionConfig {
    bool persistent;
    int timeout_minutes;
    bool regenerate_on_login;
    bool enforce_same_ip;
};

// ═══════════════════════════════════════════════════════════════════════
// MAIN CLASS: NEXUS BRIDGE
// ═══════════════════════════════════════════════════════════════════════

class NexusBridge {
public:
    NexusBridge();
    ~NexusBridge();

    // Initialization
    bool initialize(const std::string& config_path = "");
    bool startServer(int port = 8080);
    bool startWebSocketServer(int port = 8081);
    void shutdown();

    // User Management
    std::string createSession(const std::string& username, const std::string& token);
    bool validateSession(const std::string& session_id);
    bool destroySession(const std::string& session_id);
    UserProfile getUserProfile(const std::string& session_id);
    bool updatePreferences(const std::string& session_id, 
                          const std::map<std::string, std::string>& prefs);

    // API Key Management (BYOK)
    std::string addAPIKey(const std::string& session_id, const APIKey& key);
    bool removeAPIKey(const std::string& session_id, const std::string& key_id);
    std::vector<APIKey> listAPIKeys(const std::string& session_id);
    KeyValidationResult validateAPIKey(const std::string& session_id, const std::string& key_id);
    std::string encryptAPIKey(const std::string& plain_key);
    std::string decryptAPIKey(const std::string& encrypted_key);
    bool rotateAPIKey(const std::string& session_id, const std::string& key_id);

    // Project Management
    std::string createProject(const std::string& session_id, const std::string& name);
    bool deleteProject(const std::string& session_id, const std::string& project_id);
    std::vector<Project> listProjects(const std::string& session_id);
    std::string getProjectToken(const std::string& session_id, const std::string& project_id);

    // AI Request Proxying
    std::string proxyAIRequest(const std::string& session_id,
                              const std::string& key_id,
                              const json& request);
    void proxyAIStream(const std::string& session_id,
                      const std::string& key_id,
                      const json& request,
                      std::function<void(const std::string&, bool)> callback);

    // File Sync
    bool syncFile(const std::string& session_id, 
                 const std::string& project_id,
                 const std::string& file_path,
                 const std::string& content);
    std::string getRemoteFile(const std::string& session_id,
                             const std::string& project_id,
                             const std::string& file_path);
    FileTree getProjectTree(const std::string& session_id, const std::string& project_id);
    std::vector<ConflictResolution> resolveConflicts(const std::string& session_id,
                                                   const std::string& project_id,
                                                   const std::vector<ConflictResolution>& resolutions);
    SyncState getSyncState(const std::string& session_id, const std::string& project_id);

    // WebSocket Server
    void handleWebSocketMessage(const std::string& client_id, const WSMessage& message);
    void broadcast(const WSMessage& message);
    void sendToClient(const std::string& client_id, const WSMessage& message);
    void onClientConnect(std::function<void(const std::string&)> callback);
    void onClientDisconnect(std::function<void(const std::string&)> callback);
    void onClientMessage(std::function<void(const std::string&, const WSMessage&)> callback);

    // WebRTC Signaling
    void relayWebRTCOffer(const std::string& from_peer, const std::string& to_peer, const json& offer);
    void relayWebRTCAnswer(const std::string& from_peer, const std::string& to_peer, const json& answer);
    void relayWebRTCCandidate(const std::string& from_peer, const std::string& to_peer, const json& candidate);
    void registerPeer(const RemotePeer& peer);
    std::vector<RemotePeer> listPeers();

    // Integration with Neural Core
    void setNeuralCore(std::shared_ptr<neuralcore::NeuralCore> core);
    std::string routeToNeuralCore(const json& request);
    
    // Rate Limiting & Quotas
    bool checkRateLimit(const std::string& session_id, int tokens = 0);
    KeyQuota getQuota(const std::string& key_id);
    void setQuota(const std::string& key_id, const KeyQuota& quota);
    UsageStats getUsageStats(const std::string& session_id);

    // Usage Alerts
    void setAlertThreshold(const std::string& key_id, int percent);
    std::vector<UsageAlert> getPendingAlerts(const std::string& session_id);

    // Configuration
    void setConfig(const HTTPServerConfig& config);
    HTTPServerConfig getConfig() const;
    void enableCORS(bool enable);
    void setAllowedOrigins(const std::vector<std::string>& origins);

    // Monitoring
    int getActiveConnections();
    int getActiveSessions();
    std::map<std::string, int> getConnectionStats();

    // Web Frontend
    std::string getWebFrontend();

private:
    // Internal helpers
    bool initNetwork();
    bool initEncryption();
    
    std::string generateSessionToken();
    std::string hashPassword(const std::string& password);
    bool verifyPassword(const std::string& password, const std::string& hash);
    
    bool saveKeyToVault(const std::string& session_id, const APIKey& key);
    APIKey loadKeyFromVault(const std::string& session_id, const std::string& key_id);
    
    json callOpenAI(const std::string& api_key, const json& request);
    json callAnthropic(const std::string& api_key, const json& request);
    json callGoogle(const std::string& api_key, const json& request);
    
    std::string encrypt(const std::string& data, const std::string& key);
    std::string decrypt(const std::string& data, const std::string& key);
    std::string generateEncryptionKey();
    
    void sessionCleanupThread();
    void heartbeatThread();
    void connectionMonitorThread();
    void processHTTPRequests();
    void processWebSocketRequests();
    void handleWebSocketClient(socket_t socket, const std::string& client_id);
    void handleHTTPClient(socket_t socket);
    
    void processAuthRequest(const std::string& client_id, const WSMessage& msg);
    void processKeyRequest(const std::string& client_id, const WSMessage& msg);
    void processAIRequest(const std::string& client_id, const WSMessage& msg);
    void processSyncRequest(const std::string& client_id, const WSMessage& msg);
    
    std::string handleRESTAPI(const std::string& method, const std::string& path,
                             const json& body, const std::string& session_id);
    
    std::string generateErrorResponse(int code, const std::string& message);
    std::string generateSuccessResponse(const json& data);

    // Members
    std::shared_ptr<neuralcore::NeuralCore> neural_core_;
    
    // Network
    socket_t server_socket_;
    socket_t ws_server_socket_;
    int http_port_;
    int ws_port_;
    bool running_;
    
    std::vector<socket_t> client_connections_;
    std::map<std::string, socket_t> client_sockets_;
    std::map<std::string, UserSession> active_sessions_;
    
    std::mutex session_mutex_;
    std::mutex connection_mutex_;
    std::mutex key_mutex_;
    std::mutex sync_mutex_;
    
    std::thread http_thread_;
    std::thread ws_thread_;
    std::thread cleanup_thread_;
    std::thread heartbeat_thread_;
    std::thread monitor_thread_;
    
    // Configuration
    HTTPServerConfig server_config_;
    RateLimitConfig rate_limit_config_;
    WebRTCConfig webrtc_config_;
    
    std::string encryption_key_;
    std::string vault_path_;
    
    // Session management
    std::atomic<int> session_counter_{0};
    std::chrono::steady_clock::time_point start_time_;
    
    // Callbacks
    std::function<void(const std::string&)> on_connect_;
    std::function<void(const std::string&)> on_disconnect_;
    std::function<void(const std::string&, const WSMessage&)> on_message_;
    
    // Rate limiting state
    std::map<std::string, std::deque<time_t>> request_times_;
    std::map<std::string, std::deque<time_t>> token_times_;
    std::mutex ratelimit_mutex_;
    
    // Usage tracking
    std::map<std::string, UsageStats> usage_stats_;
    std::map<std::string, KeyQuota> quotas_;
    std::map<std::string, int> alert_thresholds_;
    std::mutex usage_mutex_;
    
    // File sync state
    std::map<std::string, SyncState> project_sync_states_;
    
    // WebRTC peers
    std::map<std::string, RemotePeer> peers_;
    std::mutex peer_mutex_;
    
    // Stats
    std::atomic<int64_t> total_requests_{0};
    std::atomic<int64_t> total_connections_{0};
};

} // namespace nexus

#endif // NEXUS_BRIDGE_HPP
