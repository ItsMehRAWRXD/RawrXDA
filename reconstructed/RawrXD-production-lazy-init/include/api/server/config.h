#pragma once

#include <string>
#include <cstdint>
#include <cstdlib>

// API Server Configuration from Environment Variables
struct APIServerConfig {
    // Port configuration
    uint16_t port_min = 15000;
    uint16_t port_max = 25000;
    uint16_t default_port = 11434;
    bool api_disabled = false;
    
    // TLS/HTTPS configuration
    bool tls_enabled = false;
    std::string tls_cert_path;
    std::string tls_key_path;
    std::string tls_ca_path;
    
    // JWT Authentication
    bool jwt_enabled = false;
    std::string jwt_secret;
    int jwt_expiration_hours = 24;
    
    // WebSocket configuration
    bool websocket_enabled = true;
    int websocket_max_connections = 100;
    int websocket_ping_interval_sec = 30;
    
    // Service Registry
    bool service_registry_enabled = true;
    std::string service_name = "RawrXD-API";
    std::string service_registry_url;
    int service_heartbeat_interval_sec = 60;
    
    // Rate limiting
    int rate_limit_requests_per_minute = 60;
    bool rate_limit_enabled = true;
    
    // Metrics
    bool metrics_enabled = true;
    std::string metrics_endpoint = "/metrics";
    
    // Load configuration from environment variables
    static APIServerConfig FromEnvironment() {
        APIServerConfig config;
        
        // Port configuration
        if (const char* env_port_min = std::getenv("RAWRXD_PORT_MIN")) {
            config.port_min = static_cast<uint16_t>(std::atoi(env_port_min));
        }
        
        if (const char* env_port_max = std::getenv("RAWRXD_PORT_MAX")) {
            config.port_max = static_cast<uint16_t>(std::atoi(env_port_max));
        }
        
        if (const char* env_port = std::getenv("RAWRXD_PORT")) {
            config.default_port = static_cast<uint16_t>(std::atoi(env_port));
        }
        
        if (const char* env_disabled = std::getenv("RAWRXD_API_DISABLED")) {
            std::string val(env_disabled);
            config.api_disabled = (val == "1" || val == "true" || val == "TRUE" || val == "yes");
        }
        
        // TLS configuration
        if (const char* env_tls = std::getenv("RAWRXD_TLS_ENABLED")) {
            std::string val(env_tls);
            config.tls_enabled = (val == "1" || val == "true" || val == "TRUE" || val == "yes");
        }
        
        if (const char* env_cert = std::getenv("RAWRXD_TLS_CERT")) {
            config.tls_cert_path = env_cert;
        }
        
        if (const char* env_key = std::getenv("RAWRXD_TLS_KEY")) {
            config.tls_key_path = env_key;
        }
        
        if (const char* env_ca = std::getenv("RAWRXD_TLS_CA")) {
            config.tls_ca_path = env_ca;
        }
        
        // JWT configuration
        if (const char* env_jwt = std::getenv("RAWRXD_JWT_ENABLED")) {
            std::string val(env_jwt);
            config.jwt_enabled = (val == "1" || val == "true" || val == "TRUE" || val == "yes");
        }
        
        if (const char* env_secret = std::getenv("RAWRXD_JWT_SECRET")) {
            config.jwt_secret = env_secret;
        }
        
        if (const char* env_exp = std::getenv("RAWRXD_JWT_EXPIRATION_HOURS")) {
            config.jwt_expiration_hours = std::atoi(env_exp);
        }
        
        // WebSocket configuration
        if (const char* env_ws = std::getenv("RAWRXD_WEBSOCKET_ENABLED")) {
            std::string val(env_ws);
            config.websocket_enabled = (val == "1" || val == "true" || val == "TRUE" || val == "yes");
        }
        
        if (const char* env_ws_max = std::getenv("RAWRXD_WEBSOCKET_MAX_CONNECTIONS")) {
            config.websocket_max_connections = std::atoi(env_ws_max);
        }
        
        // Service Registry configuration
        if (const char* env_registry = std::getenv("RAWRXD_SERVICE_REGISTRY_ENABLED")) {
            std::string val(env_registry);
            config.service_registry_enabled = (val == "1" || val == "true" || val == "TRUE" || val == "yes");
        }
        
        if (const char* env_name = std::getenv("RAWRXD_SERVICE_NAME")) {
            config.service_name = env_name;
        }
        
        if (const char* env_url = std::getenv("RAWRXD_SERVICE_REGISTRY_URL")) {
            config.service_registry_url = env_url;
        }
        
        // Rate limiting
        if (const char* env_rate = std::getenv("RAWRXD_RATE_LIMIT_RPM")) {
            config.rate_limit_requests_per_minute = std::atoi(env_rate);
        }
        
        if (const char* env_rate_enabled = std::getenv("RAWRXD_RATE_LIMIT_ENABLED")) {
            std::string val(env_rate_enabled);
            config.rate_limit_enabled = (val == "1" || val == "true" || val == "TRUE" || val == "yes");
        }
        
        // Metrics
        if (const char* env_metrics = std::getenv("RAWRXD_METRICS_ENABLED")) {
            std::string val(env_metrics);
            config.metrics_enabled = (val == "1" || val == "true" || val == "TRUE" || val == "yes");
        }
        
        return config;
    }
    
    // Validate configuration
    bool IsValid() const {
        if (api_disabled) {
            return true; // Disabled is always valid
        }
        
        if (port_min > port_max) {
            return false;
        }
        
        if (tls_enabled && (tls_cert_path.empty() || tls_key_path.empty())) {
            return false;
        }
        
        if (jwt_enabled && jwt_secret.empty()) {
            return false;
        }
        
        return true;
    }
};
