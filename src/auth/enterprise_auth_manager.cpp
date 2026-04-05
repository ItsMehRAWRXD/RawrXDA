#include "enterprise_auth_manager.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{

static constexpr size_t kMaxConfigBytes = 1024 * 1024;
static constexpr size_t kMaxTokenBytes = 16 * 1024;
static constexpr size_t kMaxProviderBytes = 128;
static constexpr size_t kMaxClientIdBytes = 256;
static constexpr size_t kMaxJwksUrlBytes = 2048;
static constexpr size_t kMaxUpnBytes = 320;

int base64urlDecodeChar(char c)
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

bool base64urlDecodeStrict(const std::string& in, std::string& out)
{
    out.clear();
    if (in.empty())
        return false;

    int buf = 0;
    int bits = 0;
    for (unsigned char c : in)
    {
        const int v = base64urlDecodeChar(static_cast<char>(c));
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

bool isValidTokenChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_' || c == '.';
}

}  // namespace

bool EnterpriseAuthManager::loadConfig(const std::string& configPath)
{
    if (configPath.empty())
        return false;

    std::ifstream configFile(configPath, std::ios::binary | std::ios::ate);
    if (!configFile.is_open())
        return false;

    const std::streamoff fileSize = configFile.tellg();
    if (fileSize <= 0 || static_cast<size_t>(fileSize) > kMaxConfigBytes)
        return false;
    configFile.seekg(0, std::ios::beg);

    std::string raw(static_cast<size_t>(fileSize), '\0');
    configFile.read(raw.data(), fileSize);
    if (!configFile)
        return false;

    auto doc = nlohmann::json::parse(raw, nullptr, false);
    if (doc.is_discarded() || !doc.is_object())
        return false;

    auto readBoundedString = [&](const char* key, size_t maxLen) -> std::string {
        if (!doc.contains(key) || !doc[key].is_string())
            return {};
        std::string v = doc[key].get<std::string>();
        if (v.size() > maxLen)
            v.resize(maxLen);
        return v;
    };

    m_provider = readBoundedString("provider", kMaxProviderBytes);
    m_clientId = readBoundedString("client_id", kMaxClientIdBytes);
    m_jwksUrl = readBoundedString("jwks_url", kMaxJwksUrlBytes);

    if (!m_jwksUrl.empty())
        return fetchPublicKeys();
    return true;
}

bool EnterpriseAuthManager::authenticateWithToken(const std::string& bearerToken)
{
    m_authenticated = false;
    m_userUPN.clear();

    std::string token = bearerToken;
    const std::string bearerPrefix = "Bearer ";
    if (token.size() > bearerPrefix.size() && token.rfind(bearerPrefix, 0) == 0)
        token = token.substr(bearerPrefix.size());

    if (!validateToken(token))
    {
        if (m_onFailed)
            m_onFailed("Invalid token");
        return false;
    }

    m_userUPN = extractUPN(token);
    if (m_userUPN.empty())
    {
        if (m_onFailed)
            m_onFailed("Failed to extract UPN from token");
        return false;
    }

    m_authenticated = true;
    if (m_onSucceeded)
        m_onSucceeded(m_userUPN);
    return true;
}

std::string EnterpriseAuthManager::getUserUPN() const
{
    return m_userUPN;
}

std::string EnterpriseAuthManager::getSettingsFolderPath() const
{
    std::string basePath;
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA"); appData && appData[0] != '\0')
        basePath = appData;
#else
    if (const char* home = std::getenv("HOME"); home && home[0] != '\0')
        basePath = home;
#endif
    if (basePath.empty())
        basePath = std::filesystem::current_path().string();

    basePath += "/RawrXD";
    if (!m_userUPN.empty())
        basePath += "/" + m_userUPN;
    return basePath;
}

bool EnterpriseAuthManager::fetchPublicKeys()
{
    return true;
}

bool EnterpriseAuthManager::validateToken(const std::string& token)
{
    if (token.empty() || token.size() > kMaxTokenBytes)
        return false;

    size_t dotCount = 0;
    for (char c : token)
    {
        if (!isValidTokenChar(c))
            return false;
        if (c == '.')
            ++dotCount;
    }

    if (dotCount != 2)
        return false;

    return true;
}

std::string EnterpriseAuthManager::extractUPN(const std::string& token)
{
    const size_t d1 = token.find('.');
    if (d1 == std::string::npos)
        return {};
    const size_t d2 = token.find('.', d1 + 1);
    if (d2 == std::string::npos || token.find('.', d2 + 1) != std::string::npos)
        return {};

    const std::string payloadB64 = token.substr(d1 + 1, d2 - d1 - 1);
    std::string payload;
    if (!base64urlDecodeStrict(payloadB64, payload))
        return {};

    auto j = nlohmann::json::parse(payload, nullptr, false);
    if (j.is_discarded() || !j.is_object())
        return {};

    auto readClaim = [&](const char* key) -> std::string {
        if (!j.contains(key) || !j[key].is_string())
            return {};
        std::string v = j[key].get<std::string>();
        if (v.size() > kMaxUpnBytes)
            v.resize(kMaxUpnBytes);
        return v;
    };

    std::string upn = readClaim("upn");
    if (upn.empty())
        upn = readClaim("preferred_username");
    return upn;
}

