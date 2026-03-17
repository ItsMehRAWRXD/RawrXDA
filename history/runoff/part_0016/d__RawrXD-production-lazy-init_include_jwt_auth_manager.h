#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <mutex>

// JWT Token Structure
struct JWTToken {
    std::string user_id;
    std::vector<std::string> scopes;
    std::chrono::system_clock::time_point issued_at;
    std::chrono::system_clock::time_point expires_at;
    std::string signature;
    
    bool IsValid() const {
        auto now = std::chrono::system_clock::now();
        return now < expires_at;
    }
    
    bool HasScope(const std::string& scope) const {
        for (const auto& s : scopes) {
            if (s == scope || s == "*") {
                return true;
            }
        }
        return false;
    }
};

// JWT Authentication Manager
class JWTAuthManager {
public:
    explicit JWTAuthManager(const std::string& secret);
    ~JWTAuthManager() = default;
    
    // Generate a new JWT token
    std::string GenerateToken(const std::string& user_id, 
                             const std::vector<std::string>& scopes,
                             int expiration_hours = 24);
    
    // Validate and parse a JWT token
    bool ValidateToken(const std::string& token, JWTToken& out_token);
    
    // Check if token has required scope
    bool CheckScope(const std::string& token, const std::string& required_scope);
    
    // Revoke a token (add to blacklist)
    void RevokeToken(const std::string& token);
    
    // Check if token is revoked
    bool IsRevoked(const std::string& token) const;
    
    // Clean up expired tokens from blacklist
    void CleanupExpiredTokens();

private:
    std::string secret_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> revoked_tokens_;
    mutable std::mutex mutex_;
    
    // JWT encoding/decoding helpers
    std::string Base64Encode(const std::string& data);
    std::string Base64Decode(const std::string& data);
    std::string HmacSha256(const std::string& data, const std::string& key);
    std::string CreateSignature(const std::string& header, const std::string& payload);
    bool VerifySignature(const std::string& token);
};
