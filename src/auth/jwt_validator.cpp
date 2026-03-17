// JWT validator — native (no Qt). HS256 via BCrypt; RS256 stub.
#include "auth/jwt_validator.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#ifdef _WIN32
#include <bcrypt.h>
#include <windows.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace
{

int base64url_decode_char(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return 26 + (c - 'a');
    if (c >= '0' && c <= '9')
        return 52 + (c - '0');
    if (c == '-')
        return 62;
    if (c == '_')
        return 63;
    return -1;
}

std::string base64url_decode(const std::string& in)
{
    std::string out;
    int buf = 0, bits = 0;
    for (unsigned char c : in)
    {
        int v = base64url_decode_char(static_cast<char>(c));
        if (v < 0)
            continue;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xff));
        }
    }
    return out;
}

std::string extract_claim_string(const nlohmann::json& j)
{
    if (j.is_string())
        return j.get<std::string>();
    if (j.is_number_integer())
        return std::to_string(j.get<int64_t>());
    if (j.is_number_float())
        return std::to_string(j.get<double>());
    if (j.is_boolean())
        return j.get<bool>() ? "true" : "false";
    if (j.is_object() || j.is_array())
        return j.dump();
    return "";
}

void payload_to_claims(const std::string& payload, std::map<std::string, std::string>& claims)
{
    try
    {
        auto j = nlohmann::json::parse(payload);
        if (j.is_object())
        {
            for (auto it = j.begin(); it != j.end(); ++it)
                claims[it.key()] = extract_claim_string(it.value());
        }
    }
    catch (...)
    {
    }
}

}  // namespace

JWTValidator::JWTValidator() = default;
JWTValidator::~JWTValidator() = default;

void JWTValidator::setHS256Secret(const std::string& secret)
{
    m_hs256Secret = secret;
}

void JWTValidator::setRS256PublicKey(const std::string& publicKey)
{
    m_rs256PublicKey = publicKey;
}

static bool verifyHmacSha256(const std::string& secret, const std::string& message, const std::string& signature)
{
#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(st) || !hAlg)
        return false;
    struct CloseAlg
    {
        BCRYPT_ALG_HANDLE* p;
        ~CloseAlg()
        {
            if (p && *p)
                BCryptCloseAlgorithmProvider(*p, 0);
        }
    } ca{&hAlg};
    st = BCryptCreateHash(hAlg, &hHash, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(secret.data())),
                          static_cast<ULONG>(secret.size()), 0);
    if (!BCRYPT_SUCCESS(st) || !hHash)
        return false;
    struct CloseHash
    {
        BCRYPT_HASH_HANDLE* p;
        ~CloseHash()
        {
            if (p && *p)
                BCryptDestroyHash(*p);
        }
    } ch{&hHash};
    st = BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(message.data())),
                        static_cast<ULONG>(message.size()), 0);
    if (!BCRYPT_SUCCESS(st))
        return false;
    UCHAR computed[32];
    ULONG computedLen = sizeof(computed);
    st = BCryptFinishHash(hHash, computed, computedLen, 0);
    if (!BCRYPT_SUCCESS(st))
        return false;
    if (signature.size() != 32)
        return false;
    return std::memcmp(computed, signature.data(), 32) == 0;
#else
    (void)secret;
    (void)message;
    (void)signature;
    return false;
#endif
}

bool JWTValidator::validateToken(const std::string& token)
{
    m_claims.clear();
    if (token.empty())
        return false;
    size_t d1 = token.find('.');
    size_t d2 = token.find('.', d1 + 1);
    if (d1 == std::string::npos || d2 == std::string::npos)
        return false;
    std::string payload_b64 = token.substr(d1 + 1, d2 - d1 - 1);
    std::string sig_b64 = token.substr(d2 + 1);
    std::string payload = base64url_decode(payload_b64);
    std::string sig = base64url_decode(sig_b64);
    std::string message = token.substr(0, d2);
    if (payload.empty() || sig.empty())
        return false;
    payload_to_claims(payload, m_claims);
    if (!m_hs256Secret.empty() && verifyHmacSha256(m_hs256Secret, message, sig))
        return true;
    if (!m_rs256PublicKey.empty() && validateRS256(token))
        return true;
    m_claims.clear();
    return false;
}

bool JWTValidator::validateHS256(const std::string& token)
{
    size_t d1 = token.find('.');
    size_t d2 = token.find('.', d1 + 1);
    if (d1 == std::string::npos || d2 == std::string::npos)
        return false;
    std::string payload_b64 = token.substr(d1 + 1, d2 - d1 - 1);
    std::string sig_b64 = token.substr(d2 + 1);
    std::string payload = base64url_decode(payload_b64);
    std::string sig = base64url_decode(sig_b64);
    std::string message = token.substr(0, d2);
    if (payload.empty() || sig.empty())
        return false;
    payload_to_claims(payload, m_claims);
    return verifyHmacSha256(m_hs256Secret, message, sig);
}

bool JWTValidator::validateRS256(const std::string& token)
{
    (void)token;
    // RS256: would need PEM parse + BCryptVerifySignature. Stub.
    return false;
}
