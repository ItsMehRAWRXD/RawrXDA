#ifndef ENTERPRISE_AUTH_MANAGER_H
#define ENTERPRISE_AUTH_MANAGER_H

// C++20, no Qt. JWT & enterprise auth; config from enterprise.json.

#include <string>
#include <functional>

class EnterpriseAuthManager
{
public:
    using AuthSucceededFn = std::function<void(const std::string& upn)>;
    using AuthFailedFn    = std::function<void(const std::string& reason)>;

    EnterpriseAuthManager() = default;
    ~EnterpriseAuthManager() = default;

    void setOnAuthenticationSucceeded(AuthSucceededFn f) { m_onSucceeded = std::move(f); }
    void setOnAuthenticationFailed(AuthFailedFn f)         { m_onFailed = std::move(f); }

    bool loadConfig(const std::string& configPath);
    bool authenticateWithToken(const std::string& bearerToken);
    std::string getUserUPN() const;
    std::string getSettingsFolderPath() const;
    bool isAuthenticated() const { return m_authenticated; }

private:
    bool fetchPublicKeys();
    bool validateToken(const std::string& token);
    std::string extractUPN(const std::string& token);

    std::string m_provider;
    std::string m_clientId;
    std::string m_jwksUrl;
    std::string m_userUPN;
    bool m_authenticated = false;

    AuthSucceededFn m_onSucceeded;
    AuthFailedFn    m_onFailed;
};

#endif // ENTERPRISE_AUTH_MANAGER_H
