#include "jwt_auth_manager.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

// Base64 encoding table
static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

JWTAuthManager::JWTAuthManager(const std::string& secret)
    : secret_(secret) {
}

std::string JWTAuthManager::Base64Encode(const std::string& data) {
    std::string encoded;
    int val = 0;
    int valb = -6;
    
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (encoded.size() % 4) {
        encoded.push_back('=');
    }
    
    return encoded;
}

std::string JWTAuthManager::Base64Decode(const std::string& data) {
    std::string decoded;
    std::vector<int> T(256, -1);
    
    for (int i = 0; i < 64; i++) {
        T[base64_chars[i]] = i;
    }
    
    int val = 0;
    int valb = -8;
    
    for (unsigned char c : data) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return decoded;
}

std::string JWTAuthManager::HmacSha256(const std::string& data, const std::string& key) {
    // Simplified HMAC-SHA256 implementation
    // In production, use OpenSSL or similar library
    // This is a placeholder implementation
    std::string result;
    result.reserve(32);
    
    for (size_t i = 0; i < 32; ++i) {
        unsigned char val = 0;
        for (size_t j = 0; j < data.size(); ++j) {
            val ^= (data[j] + key[j % key.size()] + i) & 0xFF;
        }
        result.push_back(static_cast<char>(val));
    }
    
    return result;
}

std::string JWTAuthManager::CreateSignature(const std::string& header, const std::string& payload) {
    std::string data = header + "." + payload;
    return Base64Encode(HmacSha256(data, secret_));
}

std::string JWTAuthManager::GenerateToken(const std::string& user_id, 
                                         const std::vector<std::string>& scopes,
                                         int expiration_hours) {
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(expiration_hours);
    
    // Create header
    std::string header = R"({"alg":"HS256","typ":"JWT"})";
    std::string header_b64 = Base64Encode(header);
    
    // Create payload
    auto now_ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto exp_ts = std::chrono::duration_cast<std::chrono::seconds>(exp.time_since_epoch()).count();
    
    std::ostringstream payload_ss;
    payload_ss << R"({"sub":")" << user_id << R"(")";
    payload_ss << R"(,"iat":)" << now_ts;
    payload_ss << R"(,"exp":)" << exp_ts;
    payload_ss << R"(,"scopes":[)";
    
    for (size_t i = 0; i < scopes.size(); ++i) {
        if (i > 0) payload_ss << ",";
        payload_ss << R"(")" << scopes[i] << R"(")";
    }
    payload_ss << "]}";
    
    std::string payload = payload_ss.str();
    std::string payload_b64 = Base64Encode(payload);
    
    // Create signature
    std::string signature = CreateSignature(header_b64, payload_b64);
    
    return header_b64 + "." + payload_b64 + "." + signature;
}

bool JWTAuthManager::ValidateToken(const std::string& token, JWTToken& out_token) {
    // Split token into parts
    size_t pos1 = token.find('.');
    if (pos1 == std::string::npos) return false;
    
    size_t pos2 = token.find('.', pos1 + 1);
    if (pos2 == std::string::npos) return false;
    
    std::string header = token.substr(0, pos1);
    std::string payload = token.substr(pos1 + 1, pos2 - pos1 - 1);
    std::string signature = token.substr(pos2 + 1);
    
    // Verify signature
    std::string expected_sig = CreateSignature(header, payload);
    if (signature != expected_sig) {
        return false;
    }
    
    // Decode payload
    std::string payload_decoded = Base64Decode(payload);
    
    // Parse payload (simplified JSON parsing)
    // In production, use proper JSON library
    size_t sub_pos = payload_decoded.find(R"("sub":")");
    if (sub_pos != std::string::npos) {
        sub_pos += 7;
        size_t sub_end = payload_decoded.find('"', sub_pos);
        out_token.user_id = payload_decoded.substr(sub_pos, sub_end - sub_pos);
    }
    
    // Check if revoked
    if (IsRevoked(token)) {
        return false;
    }
    
    return out_token.IsValid();
}

bool JWTAuthManager::CheckScope(const std::string& token, const std::string& required_scope) {
    JWTToken jwt;
    if (!ValidateToken(token, jwt)) {
        return false;
    }
    return jwt.HasScope(required_scope);
}

void JWTAuthManager::RevokeToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    revoked_tokens_[token] = std::chrono::system_clock::now();
}

bool JWTAuthManager::IsRevoked(const std::string& token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return revoked_tokens_.find(token) != revoked_tokens_.end();
}

void JWTAuthManager::CleanupExpiredTokens() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::hours(48); // Keep revoked tokens for 48 hours
    
    for (auto it = revoked_tokens_.begin(); it != revoked_tokens_.end();) {
        if (it->second < cutoff) {
            it = revoked_tokens_.erase(it);
        } else {
            ++it;
        }
    }
}
