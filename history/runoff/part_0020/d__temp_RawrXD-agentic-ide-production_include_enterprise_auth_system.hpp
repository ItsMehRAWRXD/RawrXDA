#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>

namespace enterprise {

struct AuthToken {
    std::string token;
    std::string userId;
    long long issuedAt;
    long long expiresAt;
    bool isValid() const {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        return expiresAt > now;
    }
};

struct User {
    std::string userId;
    std::string username;
    std::string passwordHash;
    std::string email;
    std::string role;
    bool isActive;
    std::vector<std::string> permissions;
};

class AuthSystem {
private:
    static AuthSystem* s_instance;
    static std::mutex s_mutex;
    std::string m_configPath;
    bool m_isInitialized;
    std::vector<User> m_users;

    AuthSystem() : m_isInitialized(false) {}

public:
    static AuthSystem& instance();

    void initialize(const std::string& configPath);
    void shutdown();

    // User management
    bool registerUser(const User& user);
    std::shared_ptr<User> getUser(const std::string& userId) const;
    bool updateUser(const User& user);
    bool deleteUser(const std::string& userId);

    // Authentication
    std::shared_ptr<AuthToken> authenticate(const std::string& username, const std::string& password);
    bool validateToken(const AuthToken& token) const;
    bool hasPermission(const std::string& userId, const std::string& permission) const;
};

}  // namespace enterprise
