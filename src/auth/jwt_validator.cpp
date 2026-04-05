// JWT validator — native (no Qt). HS256 via BCrypt; RS256 stub.
#include "auth/jwt_validator.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <sstream>
#ifdef _WIN32
#include <bcrypt.h>
#include <windows.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace
{

static constexpr size_t kMaxJwtTokenBytes = 16 * 1024;
static constexpr size_t kMaxJwtHeaderBytes = 4096;
static constexpr size_t kMaxJwtPayloadBytes = 12 * 1024;
static constexpr size_t kMaxClaims = 128;
static constexpr size_t kMaxClaimKeyBytes = 128;
static constexpr size_t kMaxClaimValueBytes = 2048;
static constexpr size_t kMaxHs256SecretBytes = 4096;
static constexpr size_t kMaxRs256PublicKeyBytes = 128 * 1024;

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

bool base64url_decode_strict(const std::string& in, std::string& out)
{
    out.clear();
    if (in.empty())
        return false;

    int buf = 0;
    int bits = 0;
    for (unsigned char c : in)
    {
        const int v = base64url_decode_char(static_cast<char>(c));
        if (v < 0)
            return false;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xff));
        }
    }

    // Reject dangling non-byte-aligned input and non-canonical trailing bits.
    if (!(bits == 0 || bits == 2 || bits == 4) || out.empty())
        return false;
    if (bits > 0)
    {
        const int mask = (1 << bits) - 1;
        if ((buf & mask) != 0)
            return false;
    }
    return true;
}

bool constant_time_equal_32(const uint8_t* a, const uint8_t* b)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < 32; ++i)
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    return diff == 0;
}

std::string extract_claim_string(const nlohmann::json& j)
{
    std::string out;
    if (j.is_string())
        out = j.get<std::string>();
    else if (j.is_number_integer())
        out = std::to_string(j.get<int64_t>());
    else if (j.is_number_float())
        out = std::to_string(j.get<double>());
    else if (j.is_boolean())
        out = j.get<bool>() ? "true" : "false";
    else if (j.is_object() || j.is_array())
        out = j.dump();

    if (out.size() > kMaxClaimValueBytes)
        out.resize(kMaxClaimValueBytes);
    return out;
}

void payload_to_claims(const std::string& payload, std::map<std::string, std::string>& claims)
{
    claims.clear();
    auto j = nlohmann::json::parse(payload, nullptr, false);
    if (j.is_discarded())
        return;
    if (j.is_object())
    {
        size_t count = 0;
        for (auto it = j.begin(); it != j.end() && count < kMaxClaims; ++it)
        {
            if (it.key().size() > kMaxClaimKeyBytes)
                continue;
            claims[it.key()] = extract_claim_string(it.value());
            ++count;
        }
    }
}

bool claim_to_int64(const nlohmann::json& payload, const char* key, int64_t& out)
{
    if (!payload.contains(key))
        return false;

    const auto& v = payload[key];
    if (v.is_number_integer())
    {
        try
        {
            out = v.get<int64_t>();
            return true;
        }
        catch (...)
        {
            // Older bundled json builds may not expose unsigned-number helpers.
            // Fallback to unsigned extraction and clamp into int64_t range.
            try
            {
                const auto uv = v.get<uint64_t>();
                if (uv > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
                    return false;
                out = static_cast<int64_t>(uv);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    }
    if (v.is_number())
    {
        // Preserve strict integer semantics for numeric claims like exp/nbf/iat.
        const std::string s = v.dump();
        if (s.find('.') != std::string::npos || s.find('e') != std::string::npos || s.find('E') != std::string::npos)
            return false;

        char* endp = nullptr;
        errno = 0;
        const long long parsed = std::strtoll(s.c_str(), &endp, 10);
        if (errno != 0 || !endp || *endp != '\0')
            return false;
        out = static_cast<int64_t>(parsed);
        return true;
    }
    if (v.is_string())
    {
        const std::string s = v.get<std::string>();
        if (s.empty())
            return false;
        char* endp = nullptr;
        errno = 0;
        const long long parsed = std::strtoll(s.c_str(), &endp, 10);
        if (errno != 0 || !endp || *endp != '\0')
            return false;
        out = static_cast<int64_t>(parsed);
        return true;
    }
    return false;
}

bool validate_registered_time_claims(const std::string& payload)
{
    auto j = nlohmann::json::parse(payload, nullptr, false);
    if (j.is_discarded())
        return false;

    if (!j.is_object())
        return false;

    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    int64_t exp = 0;
    if (claim_to_int64(j, "exp", exp))
    {
        if (exp <= 0 || now >= exp)
            return false;
    }

    int64_t nbf = 0;
    if (claim_to_int64(j, "nbf", nbf))
    {
        if (nbf <= 0 || now < nbf)
            return false;
    }

    int64_t iat = 0;
    if (claim_to_int64(j, "iat", iat))
    {
        // Allow small clock skew only; reject far-future issued-at.
        constexpr int64_t kClockSkewSec = 300;
        if (iat <= 0 || iat > (now + kClockSkewSec))
            return false;
    }

    return true;
}

}  // namespace

JWTValidator::JWTValidator() = default;
JWTValidator::~JWTValidator() = default;

void JWTValidator::setHS256Secret(const std::string& secret)
{
    if (secret.size() > kMaxHs256SecretBytes)
    {
        m_hs256Secret.clear();
        return;
    }
    m_hs256Secret = secret;
}

void JWTValidator::setRS256PublicKey(const std::string& publicKey)
{
    if (publicKey.size() > kMaxRs256PublicKeyBytes)
    {
        m_rs256PublicKey.clear();
        return;
    }
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
    return constant_time_equal_32(computed, reinterpret_cast<const uint8_t*>(signature.data()));
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
    if (token.empty() || token.size() > kMaxJwtTokenBytes)
        return false;
    size_t d1 = token.find('.');
    if (d1 == std::string::npos)
        return false;
    size_t d2 = token.find('.', d1 + 1);
    if (d2 == std::string::npos)
        return false;
    if (token.find('.', d2 + 1) != std::string::npos)
        return false;
    std::string header_b64 = token.substr(0, d1);
    std::string payload_b64 = token.substr(d1 + 1, d2 - d1 - 1);
    std::string sig_b64 = token.substr(d2 + 1);
    std::string header;
    std::string payload;
    std::string sig;
    if (!base64url_decode_strict(header_b64, header) ||
        !base64url_decode_strict(payload_b64, payload) ||
        !base64url_decode_strict(sig_b64, sig))
        return false;
    if (header.size() > kMaxJwtHeaderBytes || payload.size() > kMaxJwtPayloadBytes)
        return false;
    std::string message = token.substr(0, d2);

    std::string alg;
    auto hj = nlohmann::json::parse(header, nullptr, false);
    if (hj.is_discarded())
        return false;
    if (!hj.is_object() || !hj.contains("alg") || !hj["alg"].is_string())
        return false;
    alg = hj["alg"].get<std::string>();
    if (alg.size() > 16)
        return false;

    if (alg == "none")
        return false;

    if (!validate_registered_time_claims(payload))
        return false;
    payload_to_claims(payload, m_claims);
    if (alg == "HS256" && !m_hs256Secret.empty() && verifyHmacSha256(m_hs256Secret, message, sig))
        return true;
    if (alg == "RS256" && !m_rs256PublicKey.empty() && validateRS256(token))
        return true;
    m_claims.clear();
    return false;
}

bool JWTValidator::validateHS256(const std::string& token)
{
    m_claims.clear();
    if (m_hs256Secret.empty())
        return false;
    if (token.empty() || token.size() > kMaxJwtTokenBytes)
        return false;

    size_t d1 = token.find('.');
    if (d1 == std::string::npos)
        return false;
    size_t d2 = token.find('.', d1 + 1);
    if (d2 == std::string::npos)
        return false;
    if (token.find('.', d2 + 1) != std::string::npos)
        return false;
    std::string header_b64 = token.substr(0, d1);
    std::string payload_b64 = token.substr(d1 + 1, d2 - d1 - 1);
    std::string sig_b64 = token.substr(d2 + 1);
    std::string header;
    std::string payload;
    std::string sig;
    if (!base64url_decode_strict(header_b64, header) ||
        !base64url_decode_strict(payload_b64, payload) ||
        !base64url_decode_strict(sig_b64, sig))
        return false;
    if (header.size() > kMaxJwtHeaderBytes || payload.size() > kMaxJwtPayloadBytes)
        return false;
    std::string message = token.substr(0, d2);

    auto hj = nlohmann::json::parse(header, nullptr, false);
    if (hj.is_discarded())
        return false;
    if (!hj.is_object() || !hj.contains("alg") || !hj["alg"].is_string())
        return false;
    {
        const std::string algVal = hj["alg"].get<std::string>();
        if (algVal.size() > 16 || algVal != "HS256")
            return false;
    }

    if (!validate_registered_time_claims(payload))
        return false;
    payload_to_claims(payload, m_claims);
    if (!verifyHmacSha256(m_hs256Secret, message, sig))
    {
        m_claims.clear();
        return false;
    }
    return true;
}

bool JWTValidator::validateRS256(const std::string& token)
{
    (void)token;
    // RS256: would need PEM parse + BCryptVerifySignature. Stub.
    return false;
}
