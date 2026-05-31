#pragma once
#include <string>
#include <vector>
#include <map>

namespace RawrXD::Security {

// JWT Claims structure
struct JWTClaims {
    std::string sub;      // Subject
    std::string iss;      // Issuer
    std::string aud;      // Audience
    uint64_t exp = 0;     // Expiration time
    uint64_t iat = 0;     // Issued at
    std::map<std::string, std::string> custom;
};

class JWTValidator {
public:
    // Validate a JWT token with HMAC-SHA256
    static bool validate(const std::string& token, const std::string& secret);
    
    // Parse claims without validation (for inspection)
    static JWTClaims parseClaims(const std::string& token);
    
    // Generate a JWT token
    static std::string generate(const JWTClaims& claims, const std::string& secret);
    
    // Check if token is expired
    static bool isExpired(const std::string& token);
    
    // Extract header info
    static std::string getAlgorithm(const std::string& token);

private:
    static std::vector<uint8_t> base64UrlDecode(const std::string& in);
    static std::string base64UrlEncode(const std::vector<uint8_t>& in);
    static std::string base64UrlEncode(const std::string& in);
    static std::vector<uint8_t> hmacSha256(const std::string& data, const std::string& key);
};

} // namespace RawrXD::Security
