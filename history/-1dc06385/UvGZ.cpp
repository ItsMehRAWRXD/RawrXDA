// ============================================================================
// rbac_engine.cpp — Enterprise RBAC Engine Implementation
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "rbac_engine.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace RawrXD {
namespace Auth {

// ============================================================================
// Minimal SHA-256 (self-contained, no OpenSSL dependency)
// ============================================================================
namespace {

static constexpr uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline uint32_t sha_rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t sha_ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
inline uint32_t sha_maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
inline uint32_t sha_sigma0(uint32_t x) { return sha_rotr(x, 2) ^ sha_rotr(x, 13) ^ sha_rotr(x, 22); }
inline uint32_t sha_sigma1(uint32_t x) { return sha_rotr(x, 6) ^ sha_rotr(x, 11) ^ sha_rotr(x, 25); }
inline uint32_t sha_gamma0(uint32_t x) { return sha_rotr(x, 7) ^ sha_rotr(x, 18) ^ (x >> 3); }
inline uint32_t sha_gamma1(uint32_t x) { return sha_rotr(x, 17) ^ sha_rotr(x, 19) ^ (x >> 10); }

void sha256_compute(const uint8_t* data, size_t len, uint8_t hash[32]) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    // Pre-processing: pad message
    uint64_t bitLen = len * 8;
    size_t padLen = ((len + 8) / 64 + 1) * 64;
    std::vector<uint8_t> padded(padLen, 0);
    memcpy(padded.data(), data, len);
    padded[len] = 0x80;
    for (int i = 0; i < 8; i++) {
        padded[padLen - 1 - i] = static_cast<uint8_t>(bitLen >> (i * 8));
    }

    // Process blocks
    for (size_t blk = 0; blk < padLen; blk += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = (static_cast<uint32_t>(padded[blk + i * 4]) << 24) |
                   (static_cast<uint32_t>(padded[blk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(padded[blk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(padded[blk + i * 4 + 3]));
        }
        for (int i = 16; i < 64; i++) {
            w[i] = sha_gamma1(w[i - 2]) + w[i - 7] +
                   sha_gamma0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; i++) {
            uint32_t t1 = hh + sha_sigma1(e) + sha_ch(e, f, g) +
                          sha256_k[i] + w[i];
            uint32_t t2 = sha_sigma0(a) + sha_maj(a, b, c);
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    for (int i = 0; i < 8; i++) {
        hash[i * 4]     = static_cast<uint8_t>(h[i] >> 24);
        hash[i * 4 + 1] = static_cast<uint8_t>(h[i] >> 16);
        hash[i * 4 + 2] = static_cast<uint8_t>(h[i] >> 8);
        hash[i * 4 + 3] = static_cast<uint8_t>(h[i]);
    }
}

} // anonymous namespace

// ============================================================================
// Singleton
// ============================================================================
RBACEngine& RBACEngine::instance() {
    static RBACEngine inst;
    return inst;
}

RBACEngine::RBACEngine() {
    memset(lastAuditHash_, 0, 32);
    memset(auditListeners_, 0, sizeof(auditListeners_));
    memset(sessionListeners_, 0, sizeof(sessionListeners_));
    auditLog_.reserve(1024);
}

RBACEngine::~RBACEngine() {
    shutdown();
}

// ============================================================================
// Initialization
// ============================================================================

AuthResult RBACEngine::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_.load()) return AuthResult::ok("Already initialized");

    createBuiltinRoles();
    initialized_.store(true);

    return AuthResult::ok("RBAC engine initialized");
}

AuthResult RBACEngine::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_.load()) return AuthResult::ok("Not initialized");

    sessions_.clear();
    initialized_.store(false);
    return AuthResult::ok("RBAC engine shut down");
}

void RBACEngine::createBuiltinRoles() {
    // Superadmin — all permissions
    Role superadmin;
    superadmin.name = "superadmin";
    superadmin.description = "Full system access";
    superadmin.permissions = Permission::ALL_PERMISSIONS;
    superadmin.isBuiltin = true;
    superadmin.createdAt = now();
    superadmin.modifiedAt = now();
    roles_["superadmin"] = superadmin;

    // Admin — all except PT driver and live binary
    Role admin;
    admin.name = "admin";
    admin.description = "Administrative access";
    admin.permissions = static_cast<Permission>(
        static_cast<uint32_t>(Permission::ALL_PERMISSIONS) &
        ~(static_cast<uint32_t>(Permission::HOTPATCH_PT_DRIVER) |
          static_cast<uint32_t>(Permission::HOTPATCH_LIVE_BINARY)));
    admin.isBuiltin = true;
    admin.createdAt = now();
    admin.modifiedAt = now();
    roles_["admin"] = admin;

    // Developer — model, inference, workspace, agent, extensions, read hotpatch
    Role developer;
    developer.name = "developer";
    developer.description = "Developer access";
    developer.permissions =
        Permission::MODEL_LOAD | Permission::MODEL_UNLOAD |
        Permission::INFERENCE_EXECUTE | Permission::INFERENCE_CONFIG |
        Permission::WORKSPACE_READ | Permission::WORKSPACE_WRITE |
        Permission::AGENT_EXECUTE | Permission::AGENT_CONFIGURE |
        Permission::EXTENSION_INSTALL | Permission::EXTENSION_UNINSTALL |
        Permission::EXTENSION_ACTIVATE | Permission::MARKETPLACE_BROWSE |
        Permission::HOTPATCH_MEMORY_READ | Permission::HOTPATCH_BYTE_READ;
    developer.isBuiltin = true;
    developer.createdAt = now();
    developer.modifiedAt = now();
    roles_["developer"] = developer;

    // Viewer — read-only access
    Role viewer;
    viewer.name = "viewer";
    viewer.description = "Read-only access";
    viewer.permissions =
        Permission::WORKSPACE_READ | Permission::MARKETPLACE_BROWSE |
        Permission::HOTPATCH_MEMORY_READ | Permission::HOTPATCH_BYTE_READ;
    viewer.isBuiltin = true;
    viewer.createdAt = now();
    viewer.modifiedAt = now();
    roles_["viewer"] = viewer;

    // Agent — execution focused (for automated agents)
    Role agent;
    agent.name = "agent";
    agent.description = "Automated agent access";
    agent.permissions =
        Permission::INFERENCE_EXECUTE |
        Permission::WORKSPACE_READ | Permission::WORKSPACE_WRITE |
        Permission::AGENT_EXECUTE |
        Permission::MODEL_LOAD | Permission::MODEL_UNLOAD;
    agent.isBuiltin = true;
    agent.createdAt = now();
    agent.modifiedAt = now();
    roles_["agent"] = agent;
}

// ============================================================================
// Role Management
// ============================================================================

AuthResult RBACEngine::createRole(const Role& role) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (role.name.empty())
        return AuthResult::error("Role name cannot be empty", 400);

    if (roles_.find(role.name) != roles_.end())
        return AuthResult::error("Role already exists", 409);

    Role newRole = role;
    newRole.createdAt = now();
    newRole.modifiedAt = now();
    roles_[role.name] = newRole;

    logAudit("system", "role.create", role.name, "Role created",
             AuthResult::ok("Created"));

    return AuthResult::ok("Role created");
}

AuthResult RBACEngine::deleteRole(const std::string& roleName) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = roles_.find(roleName);
    if (it == roles_.end())
        return AuthResult::error("Role not found", 404);

    if (it->second.isBuiltin)
        return AuthResult::error("Cannot delete builtin role", 403);

    // Remove role from all users who have it
    for (auto& [uid, user] : users_) {
        auto rit = std::find(user.roles.begin(), user.roles.end(), roleName);
        if (rit != user.roles.end()) {
            user.roles.erase(rit);
            computeEffectivePermissions(user);
        }
    }

    roles_.erase(it);
    logAudit("system", "role.delete", roleName, "Role deleted",
             AuthResult::ok("Deleted"));

    return AuthResult::ok("Role deleted");
}

AuthResult RBACEngine::updateRole(const Role& role) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = roles_.find(role.name);
    if (it == roles_.end())
        return AuthResult::error("Role not found", 404);

    if (it->second.isBuiltin)
        return AuthResult::error("Cannot modify builtin role", 403);

    it->second.description = role.description;
    it->second.permissions = role.permissions;
    it->second.parentRole = role.parentRole;
    it->second.modifiedAt = now();

    // Recompute permissions for users with this role
    for (auto& [uid, user] : users_) {
        for (const auto& r : user.roles) {
            if (r == role.name) {
                computeEffectivePermissions(user);
                break;
            }
        }
    }

    return AuthResult::ok("Role updated");
}

AuthResult RBACEngine::getRole(const std::string& roleName, Role& outRole) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = roles_.find(roleName);
    if (it == roles_.end())
        return AuthResult::error("Role not found", 404);
    outRole = it->second;
    return AuthResult::ok("Role found");
}

std::vector<Role> RBACEngine::listRoles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Role> result;
    result.reserve(roles_.size());
    for (const auto& [name, role] : roles_) {
        result.push_back(role);
    }
    return result;
}

Permission RBACEngine::resolvePermissions(const std::string& roleName) const {
    auto it = roles_.find(roleName);
    if (it == roles_.end()) return static_cast<Permission>(0);

    Permission perms = it->second.permissions;

    // Walk parent chain
    std::string parent = it->second.parentRole;
    std::unordered_set<std::string> visited;
    visited.insert(roleName);

    while (!parent.empty() && visited.find(parent) == visited.end()) {
        visited.insert(parent);
        auto pit = roles_.find(parent);
        if (pit == roles_.end()) break;
        perms = perms | pit->second.permissions;
        parent = pit->second.parentRole;
    }

    return perms;
}

// ============================================================================
// User Management
// ============================================================================

AuthResult RBACEngine::createUser(const User& user) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (user.id.empty())
        return AuthResult::error("User ID cannot be empty", 400);
    if (user.username.empty())
        return AuthResult::error("Username cannot be empty", 400);
    if (users_.find(user.id) != users_.end())
        return AuthResult::error("User ID already exists", 409);
    if (usernameIndex_.find(user.username) != usernameIndex_.end())
        return AuthResult::error("Username already exists", 409);

    User newUser = user;
    newUser.isActive = true;
    newUser.isLocked = false;
    newUser.createdAt = now();
    newUser.failedLoginCount = 0;
    computeEffectivePermissions(newUser);

    users_[user.id] = newUser;
    usernameIndex_[user.username] = user.id;

    logAudit("system", "user.create", user.id,
             "User created: " + user.username, AuthResult::ok("Created"));

    return AuthResult::ok("User created");
}

AuthResult RBACEngine::deleteUser(const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = users_.find(userId);
    if (it == users_.end())
        return AuthResult::error("User not found", 404);

    // Invalidate all sessions
    std::vector<std::string> tokensToRemove;
    for (auto& [token, session] : sessions_) {
        if (session.userId == userId) {
            tokensToRemove.push_back(token);
        }
    }
    for (const auto& token : tokensToRemove) {
        sessions_.erase(token);
    }

    usernameIndex_.erase(it->second.username);
    passwordHashes_.erase(userId);
    users_.erase(it);

    logAudit("system", "user.delete", userId, "User deleted",
             AuthResult::ok("Deleted"));

    return AuthResult::ok("User deleted");
}

AuthResult RBACEngine::updateUser(const User& user) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = users_.find(user.id);
    if (it == users_.end())
        return AuthResult::error("User not found", 404);

    // Update mutable fields
    it->second.email = user.email;
    it->second.displayName = user.displayName;
    it->second.roles = user.roles;
    computeEffectivePermissions(it->second);

    return AuthResult::ok("User updated");
}

AuthResult RBACEngine::getUser(const std::string& userId, User& outUser) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(userId);
    if (it == users_.end())
        return AuthResult::error("User not found", 404);
    outUser = it->second;
    return AuthResult::ok("User found");
}

AuthResult RBACEngine::getUserByUsername(const std::string& username,
                                          User& outUser) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto idx = usernameIndex_.find(username);
    if (idx == usernameIndex_.end())
        return AuthResult::error("User not found", 404);
    auto it = users_.find(idx->second);
    if (it == users_.end())
        return AuthResult::error("User not found (index stale)", 500);
    outUser = it->second;
    return AuthResult::ok("User found");
}

std::vector<User> RBACEngine::listUsers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<User> result;
    result.reserve(users_.size());
    for (const auto& [id, user] : users_) {
        result.push_back(user);
    }
    return result;
}

AuthResult RBACEngine::assignRole(const std::string& userId,
                                    const std::string& roleName) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto uit = users_.find(userId);
    if (uit == users_.end())
        return AuthResult::error("User not found", 404);
    if (roles_.find(roleName) == roles_.end())
        return AuthResult::error("Role not found", 404);

    // Check if already assigned
    for (const auto& r : uit->second.roles) {
        if (r == roleName) return AuthResult::ok("Role already assigned");
    }

    uit->second.roles.push_back(roleName);
    computeEffectivePermissions(uit->second);

    logAudit("system", "user.role.assign", userId,
             "Role assigned: " + roleName, AuthResult::ok("Assigned"));

    return AuthResult::ok("Role assigned");
}

AuthResult RBACEngine::revokeRole(const std::string& userId,
                                    const std::string& roleName) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto uit = users_.find(userId);
    if (uit == users_.end())
        return AuthResult::error("User not found", 404);

    auto rit = std::find(uit->second.roles.begin(),
                         uit->second.roles.end(), roleName);
    if (rit == uit->second.roles.end())
        return AuthResult::error("Role not assigned", 404);

    uit->second.roles.erase(rit);
    computeEffectivePermissions(uit->second);

    logAudit("system", "user.role.revoke", userId,
             "Role revoked: " + roleName, AuthResult::ok("Revoked"));

    return AuthResult::ok("Role revoked");
}

AuthResult RBACEngine::lockUser(const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(userId);
    if (it == users_.end())
        return AuthResult::error("User not found", 404);
    it->second.isLocked = true;

    // Invalidate sessions
    std::vector<std::string> toRemove;
    for (auto& [token, session] : sessions_) {
        if (session.userId == userId) toRemove.push_back(token);
    }
    for (auto& t : toRemove) sessions_.erase(t);

    logAudit("system", "user.lock", userId, "User locked",
             AuthResult::ok("Locked"));
    return AuthResult::ok("User locked");
}

AuthResult RBACEngine::unlockUser(const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(userId);
    if (it == users_.end())
        return AuthResult::error("User not found", 404);
    it->second.isLocked = false;
    it->second.failedLoginCount = 0;

    logAudit("system", "user.unlock", userId, "User unlocked",
             AuthResult::ok("Unlocked"));
    return AuthResult::ok("User unlocked");
}

// ============================================================================
// Authentication
// ============================================================================

AuthResult RBACEngine::authenticateLocal(const std::string& username,
                                          const std::string& passwordHash,
                                          Session& outSession) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.totalAuthAttempts.fetch_add(1);

    auto idx = usernameIndex_.find(username);
    if (idx == usernameIndex_.end()) {
        stats_.failedAuths.fetch_add(1);
        logAudit("unknown", "auth.login.fail", username,
                 "User not found", AuthResult::denied("Not found"));
        return AuthResult::denied("Invalid credentials", 401);
    }

    auto uit = users_.find(idx->second);
    if (uit == users_.end()) {
        stats_.failedAuths.fetch_add(1);
        return AuthResult::error("User index stale", 500);
    }

    User& user = uit->second;

    // Check lockout
    if (user.isLocked) {
        stats_.failedAuths.fetch_add(1);
        logAudit(user.id, "auth.login.locked", username,
                 "Account locked", AuthResult::denied("Locked"));
        return AuthResult::denied("Account is locked", 423);
    }

    if (!user.isActive) {
        stats_.failedAuths.fetch_add(1);
        return AuthResult::denied("Account is disabled", 403);
    }

    // Verify password hash
    auto phit = passwordHashes_.find(user.id);
    if (phit != passwordHashes_.end() && phit->second != passwordHash) {
        user.failedLoginCount++;
        if (compliance_.maxFailedLogins > 0 &&
            user.failedLoginCount >= compliance_.maxFailedLogins) {
            user.isLocked = true;
            logAudit(user.id, "auth.lockout", username,
                     "Max failed logins exceeded",
                     AuthResult::denied("Lockout"));
        }
        stats_.failedAuths.fetch_add(1);
        logAudit(user.id, "auth.login.fail", username,
                 "Invalid password", AuthResult::denied("Bad password"));
        return AuthResult::denied("Invalid credentials", 401);
    }

    // Successful auth — create session
    user.failedLoginCount = 0;
    user.lastLoginAt = now();

    Session session;
    generateSessionToken(session.token, sizeof(session.token));
    session.userId = user.id;
    session.createdAt = now();
    session.expiresAt = now() + compliance_.maxSessionDurationSec;
    session.lastActivityAt = now();
    session.isValid = true;
    session.cachedPermissions = user.effectivePermissions;

    sessions_[std::string(session.token)] = session;
    stats_.successfulAuths.fetch_add(1);
    stats_.activeSessions.fetch_add(1);
    outSession = session;

    // Notify session listeners
    uint32_t lCount = sessionListenerCount_.load();
    for (uint32_t i = 0; i < lCount; ++i) {
        if (sessionListeners_[i].callback) {
            sessionListeners_[i].callback(&session, true,
                                           sessionListeners_[i].userData);
        }
    }

    logAudit(user.id, "auth.login.success", username,
             "Login successful", AuthResult::ok("Authenticated"));

    return AuthResult::ok("Authenticated");
}

AuthResult RBACEngine::authenticateSSO(const std::string& providerName,
                                        const std::string& assertion,
                                        Session& outSession) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.totalAuthAttempts.fetch_add(1);

    auto pit = ssoProviders_.find(providerName);
    if (pit == ssoProviders_.end())
        return AuthResult::error("SSO provider not found", 404);

    if (!pit->second.enabled)
        return AuthResult::error("SSO provider disabled", 403);

    // Try external auth provider first
    if (externalAuthProvider_) {
        User ssoUser;
        AuthResult extResult = externalAuthProvider_(
            assertion.c_str(), &ssoUser, externalAuthCtx_);

        if (extResult.success) {
            // Check if user exists or auto-create
            auto uidx = usernameIndex_.find(ssoUser.username);
            if (uidx == usernameIndex_.end()) {
                if (pit->second.autoCreateUsers) {
                    ssoUser.ssoProvider = providerName;
                    ssoUser.createdAt = now();
                    ssoUser.isActive = true;
                    if (!pit->second.defaultRole.empty()) {
                        ssoUser.roles.push_back(pit->second.defaultRole);
                    }
                    computeEffectivePermissions(ssoUser);
                    users_[ssoUser.id] = ssoUser;
                    usernameIndex_[ssoUser.username] = ssoUser.id;
                } else {
                    stats_.failedAuths.fetch_add(1);
                    return AuthResult::denied(
                        "User not registered (auto-create disabled)", 403);
                }
            }

            // Create session
            auto& user = users_[ssoUser.id];
            user.lastLoginAt = now();

            Session session;
            generateSessionToken(session.token, sizeof(session.token));
            session.userId = user.id;
            session.createdAt = now();
            uint32_t timeout = pit->second.sessionTimeoutSec > 0 ?
                pit->second.sessionTimeoutSec :
                compliance_.maxSessionDurationSec;
            session.expiresAt = now() + timeout;
            session.lastActivityAt = now();
            session.isValid = true;
            session.cachedPermissions = user.effectivePermissions;

            sessions_[std::string(session.token)] = session;
            stats_.successfulAuths.fetch_add(1);
            stats_.activeSessions.fetch_add(1);
            outSession = session;

            logAudit(user.id, "auth.sso.success", providerName,
                     "SSO login via " + providerName,
                     AuthResult::ok("SSO authenticated"));

            return AuthResult::ok("SSO authenticated");
        }
    }

    // SAML assertion validation placeholder
    // In production: parse XML, verify signature, extract NameID
    stats_.failedAuths.fetch_add(1);
    logAudit("unknown", "auth.sso.fail", providerName,
             "SSO assertion validation failed",
             AuthResult::denied("SSO failed"));
    return AuthResult::denied("SSO authentication failed", 401);
}

AuthResult RBACEngine::validateSession(const char* token,
                                        Session& outSession) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(std::string(token));
    if (it == sessions_.end())
        return AuthResult::denied("Invalid session token", 401);

    const Session& session = it->second;

    if (!session.isValid)
        return AuthResult::denied("Session invalidated", 401);

    uint64_t currentTime = now();

    // Check expiry
    if (compliance_.enforceSessionTimeout && currentTime > session.expiresAt)
        return AuthResult::denied("Session expired", 401);

    // Check idle timeout
    if (compliance_.maxIdleTimeoutSec > 0 &&
        (currentTime - session.lastActivityAt) > compliance_.maxIdleTimeoutSec)
        return AuthResult::denied("Session idle timeout", 401);

    outSession = session;
    return AuthResult::ok("Session valid");
}

AuthResult RBACEngine::invalidateSession(const char* token) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(std::string(token));
    if (it == sessions_.end())
        return AuthResult::error("Session not found", 404);

    // Notify listeners
    uint32_t lCount = sessionListenerCount_.load();
    for (uint32_t i = 0; i < lCount; ++i) {
        if (sessionListeners_[i].callback) {
            sessionListeners_[i].callback(&it->second, false,
                                           sessionListeners_[i].userData);
        }
    }

    sessions_.erase(it);
    stats_.activeSessions.fetch_sub(1);

    return AuthResult::ok("Session invalidated");
}

AuthResult RBACEngine::invalidateAllSessions(const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> toRemove;
    for (auto& [token, session] : sessions_) {
        if (session.userId == userId) toRemove.push_back(token);
    }
    for (auto& t : toRemove) {
        sessions_.erase(t);
        stats_.activeSessions.fetch_sub(1);
    }

    return AuthResult::ok("All sessions invalidated");
}

AuthResult RBACEngine::refreshSession(const char* token, Session& outSession) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(std::string(token));
    if (it == sessions_.end())
        return AuthResult::denied("Session not found", 404);

    it->second.lastActivityAt = now();
    if (compliance_.enforceSessionTimeout) {
        it->second.expiresAt = now() + compliance_.maxSessionDurationSec;
    }

    // Refresh cached permissions from current user state
    auto uit = users_.find(it->second.userId);
    if (uit != users_.end()) {
        it->second.cachedPermissions = uit->second.effectivePermissions;
    }

    outSession = it->second;
    return AuthResult::ok("Session refreshed");
}

// ============================================================================
// Authorization
// ============================================================================

AuthResult RBACEngine::checkPermission(const char* sessionToken,
                                        Permission required) const {
    stats_.permissionChecks.fetch_add(1);

    Session session;
    AuthResult vr = validateSession(sessionToken, session);
    if (!vr.success) {
        stats_.permissionDenials.fetch_add(1);
        return vr;
    }

    return checkPermission(session, required);
}

AuthResult RBACEngine::checkPermission(const Session& session,
                                        Permission required) const {
    if (hasPermission(session.cachedPermissions, required))
        return AuthResult::ok("Permission granted");

    stats_.permissionDenials.fetch_add(1);
    return AuthResult::denied("Insufficient permissions", 403);
}

AuthResult RBACEngine::checkPermission(const User& user,
                                        Permission required) const {
    stats_.permissionChecks.fetch_add(1);

    if (hasPermission(user.effectivePermissions, required))
        return AuthResult::ok("Permission granted");

    stats_.permissionDenials.fetch_add(1);
    return AuthResult::denied("Insufficient permissions", 403);
}

AuthResult RBACEngine::authorize(const char* sessionToken,
                                  Permission required,
                                  const char* action,
                                  const char* resource) {
    AuthResult result = checkPermission(sessionToken, required);

    // Find user ID for audit
    std::string userId = "unknown";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto sit = sessions_.find(std::string(sessionToken));
        if (sit != sessions_.end()) {
            userId = sit->second.userId;
        }
    }

    logAudit(userId, action, resource,
             result.success ? "Authorized" : "Denied", result);

    return result;
}

// ============================================================================
// SSO Provider Management
// ============================================================================

AuthResult RBACEngine::addSSOProvider(const SSOProviderConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config.name.empty())
        return AuthResult::error("Provider name required", 400);

    ssoProviders_[config.name] = config;

    logAudit("system", "sso.provider.add", config.name,
             "SSO provider added", AuthResult::ok("Added"));

    return AuthResult::ok("SSO provider added");
}

AuthResult RBACEngine::removeSSOProvider(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = ssoProviders_.find(name);
    if (it == ssoProviders_.end())
        return AuthResult::error("Provider not found", 404);

    ssoProviders_.erase(it);
    return AuthResult::ok("SSO provider removed");
}

AuthResult RBACEngine::getSSOProvider(const std::string& name,
                                       SSOProviderConfig& outConfig) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = ssoProviders_.find(name);
    if (it == ssoProviders_.end())
        return AuthResult::error("Provider not found", 404);
    outConfig = it->second;
    return AuthResult::ok("Provider found");
}

std::vector<SSOProviderConfig> RBACEngine::listSSOProviders() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SSOProviderConfig> result;
    for (const auto& [name, config] : ssoProviders_) {
        result.push_back(config);
    }
    return result;
}

void RBACEngine::setExternalAuthProvider(ExternalAuthProvider provider,
                                          void* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    externalAuthProvider_ = provider;
    externalAuthCtx_ = ctx;
}

// ============================================================================
// Compliance
// ============================================================================

AuthResult RBACEngine::applyComplianceConfig(const ComplianceConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    compliance_ = config;

    logAudit("system", "compliance.update", "global",
             "Compliance config updated", AuthResult::ok("Updated"));

    // Enforce session timeout on existing sessions
    if (config.enforceSessionTimeout) {
        uint64_t currentTime = now();
        std::vector<std::string> expired;
        for (auto& [token, session] : sessions_) {
            if (currentTime > session.expiresAt) {
                expired.push_back(token);
            }
        }
        for (auto& t : expired) {
            sessions_.erase(t);
            stats_.activeSessions.fetch_sub(1);
        }
    }

    return AuthResult::ok("Compliance config applied");
}

ComplianceConfig RBACEngine::getComplianceConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return compliance_;
}

// ============================================================================
// Audit Log
// ============================================================================

void RBACEngine::logAudit(const std::string& userId,
                           const std::string& action,
                           const std::string& resource,
                           const std::string& detail,
                           const AuthResult& result) {
    // Note: caller may or may not hold mutex_ — audit log has own protection
    AuditEntry entry;
    entry.sequenceId = auditSequence_.fetch_add(1);
    entry.timestamp = now();
    entry.userId = userId;
    entry.action = action;
    entry.resource = resource;
    entry.detail = detail;
    entry.result = result;

    if (compliance_.auditHashChain) {
        hashAuditEntry(entry, lastAuditHash_);
        memcpy(lastAuditHash_, entry.entryHash, 32);
    } else {
        memset(entry.prevHash, 0, 32);
        memset(entry.entryHash, 0, 32);
    }

    if (auditLog_.size() < MAX_AUDIT_ENTRIES) {
        auditLog_.push_back(entry);
    } else {
        // Ring buffer: overwrite oldest
        auditLog_[entry.sequenceId % MAX_AUDIT_ENTRIES] = entry;
    }

    stats_.auditEntries.fetch_add(1);

    // Notify listeners
    uint32_t lCount = auditListenerCount_.load();
    for (uint32_t i = 0; i < lCount; ++i) {
        if (auditListeners_[i].callback) {
            auditListeners_[i].callback(&entry, auditListeners_[i].userData);
        }
    }
}

std::vector<AuditEntry> RBACEngine::getAuditLog(uint64_t fromSeq,
                                                   uint32_t maxEntries) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AuditEntry> result;

    for (const auto& entry : auditLog_) {
        if (entry.sequenceId >= fromSeq) {
            result.push_back(entry);
            if (result.size() >= maxEntries) break;
        }
    }

    return result;
}

AuthResult RBACEngine::verifyAuditChain() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!compliance_.auditHashChain)
        return AuthResult::ok("Hash chain not enabled");

    if (auditLog_.empty())
        return AuthResult::ok("Empty log");

    uint8_t prevHash[32] = {};

    for (size_t i = 0; i < auditLog_.size(); ++i) {
        const auto& entry = auditLog_[i];

        // Verify prevHash matches
        if (i > 0 && memcmp(entry.prevHash, auditLog_[i - 1].entryHash, 32) != 0) {
            return AuthResult::error("Audit chain broken at entry", 500);
        }

        // Recompute hash and verify
        AuditEntry verify = entry;
        uint8_t computedPrev[32];
        if (i > 0) {
            memcpy(computedPrev, auditLog_[i - 1].entryHash, 32);
        } else {
            memset(computedPrev, 0, 32);
        }

        // Build data to hash: seq + timestamp + userId + action + resource
        std::string hashInput;
        hashInput += std::to_string(entry.sequenceId);
        hashInput += std::to_string(entry.timestamp);
        hashInput += entry.userId;
        hashInput += entry.action;
        hashInput += entry.resource;
        hashInput += entry.detail;
        hashInput.append(reinterpret_cast<const char*>(computedPrev), 32);

        uint8_t recomputed[32];
        sha256_compute(reinterpret_cast<const uint8_t*>(hashInput.data()),
                       hashInput.size(), recomputed);

        if (memcmp(recomputed, entry.entryHash, 32) != 0) {
            return AuthResult::error("Audit hash mismatch - tamper detected", 500);
        }
    }

    return AuthResult::ok("Audit chain verified");
}

AuthResult RBACEngine::exportAuditLog(const char* path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream file(path);
    if (!file.is_open())
        return AuthResult::error("Cannot open export file", 500);

    file << "[\n";
    for (size_t i = 0; i < auditLog_.size(); ++i) {
        const auto& e = auditLog_[i];
        file << "  {\"seq\":" << e.sequenceId
             << ",\"ts\":" << e.timestamp
             << ",\"user\":\"" << e.userId << "\""
             << ",\"action\":\"" << e.action << "\""
             << ",\"resource\":\"" << e.resource << "\""
             << ",\"detail\":\"" << e.detail << "\""
             << ",\"ok\":" << (e.result.success ? "true" : "false")
             << "}";
        if (i + 1 < auditLog_.size()) file << ",";
        file << "\n";
    }
    file << "]\n";

    return AuthResult::ok("Audit log exported");
}

uint64_t RBACEngine::getAuditEntryCount() const {
    return auditSequence_.load();
}

// ============================================================================
// Event Listeners
// ============================================================================

void RBACEngine::addAuditListener(AuditEventCallback cb, void* userData) {
    uint32_t idx = auditListenerCount_.load();
    if (idx >= MAX_LISTENERS) return;
    auditListeners_[idx].callback = cb;
    auditListeners_[idx].userData = userData;
    auditListenerCount_.fetch_add(1);
}

void RBACEngine::removeAuditListener(AuditEventCallback cb) {
    uint32_t count = auditListenerCount_.load();
    for (uint32_t i = 0; i < count; ++i) {
        if (auditListeners_[i].callback == cb) {
            for (uint32_t j = i; j < count - 1; ++j) {
                auditListeners_[j] = auditListeners_[j + 1];
            }
            auditListenerCount_.fetch_sub(1);
            return;
        }
    }
}

void RBACEngine::addSessionListener(SessionEventCallback cb, void* userData) {
    uint32_t idx = sessionListenerCount_.load();
    if (idx >= MAX_LISTENERS) return;
    sessionListeners_[idx].callback = cb;
    sessionListeners_[idx].userData = userData;
    sessionListenerCount_.fetch_add(1);
}

void RBACEngine::removeSessionListener(SessionEventCallback cb) {
    uint32_t count = sessionListenerCount_.load();
    for (uint32_t i = 0; i < count; ++i) {
        if (sessionListeners_[i].callback == cb) {
            for (uint32_t j = i; j < count - 1; ++j) {
                sessionListeners_[j] = sessionListeners_[j + 1];
            }
            sessionListenerCount_.fetch_sub(1);
            return;
        }
    }
}

// ============================================================================
// Statistics
// ============================================================================

const RBACEngine::Stats& RBACEngine::getStats() const {
    return stats_;
}

void RBACEngine::resetStats() {
    stats_.totalAuthAttempts.store(0);
    stats_.successfulAuths.store(0);
    stats_.failedAuths.store(0);
    stats_.permissionChecks.store(0);
    stats_.permissionDenials.store(0);
    // activeSessions and auditEntries are not reset
}

// ============================================================================
// Config Persistence
// ============================================================================

AuthResult RBACEngine::loadFromFile(const char* configPath) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(configPath);
    if (!file.is_open())
        return AuthResult::error("Cannot open config file", 500);

    // Minimal JSON key-value extraction
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Parse roles section
    // TODO: Full JSON parser integration
    // For now, just ensure the builtin roles exist
    if (!initialized_.load()) {
        createBuiltinRoles();
        initialized_.store(true);
    }

    logAudit("system", "config.load", configPath,
             "Configuration loaded", AuthResult::ok("Loaded"));

    return AuthResult::ok("Configuration loaded");
}

AuthResult RBACEngine::saveToFile(const char* configPath) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream file(configPath);
    if (!file.is_open())
        return AuthResult::error("Cannot open config file for writing", 500);

    file << "{\n";

    // Serialize roles
    file << "  \"roles\": [\n";
    size_t ri = 0;
    for (const auto& [name, role] : roles_) {
        file << "    {\"name\":\"" << role.name << "\""
             << ",\"description\":\"" << role.description << "\""
             << ",\"permissions\":" << static_cast<uint32_t>(role.permissions)
             << ",\"parent\":\"" << role.parentRole << "\""
             << ",\"builtin\":" << (role.isBuiltin ? "true" : "false")
             << "}";
        if (++ri < roles_.size()) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // Serialize users (without password hashes)
    file << "  \"users\": [\n";
    size_t ui = 0;
    for (const auto& [id, user] : users_) {
        file << "    {\"id\":\"" << user.id << "\""
             << ",\"username\":\"" << user.username << "\""
             << ",\"email\":\"" << user.email << "\""
             << ",\"active\":" << (user.isActive ? "true" : "false")
             << ",\"roles\":[";
        for (size_t r = 0; r < user.roles.size(); ++r) {
            file << "\"" << user.roles[r] << "\"";
            if (r + 1 < user.roles.size()) file << ",";
        }
        file << "]}";
        if (++ui < users_.size()) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // Serialize SSO providers (without secrets)
    file << "  \"ssoProviders\": [\n";
    size_t si = 0;
    for (const auto& [name, config] : ssoProviders_) {
        file << "    {\"name\":\"" << config.name << "\""
             << ",\"type\":" << static_cast<int>(config.type)
             << ",\"entityId\":\"" << config.entityId << "\""
             << ",\"enabled\":" << (config.enabled ? "true" : "false")
             << "}";
        if (++si < ssoProviders_.size()) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // Serialize compliance config
    file << "  \"compliance\": {"
         << "\"enforceAuditLog\":" << (compliance_.enforceAuditLog ? "true" : "false")
         << ",\"enforceSessionTimeout\":" << (compliance_.enforceSessionTimeout ? "true" : "false")
         << ",\"maxSessionDurationSec\":" << compliance_.maxSessionDurationSec
         << ",\"maxIdleTimeoutSec\":" << compliance_.maxIdleTimeoutSec
         << ",\"maxFailedLogins\":" << compliance_.maxFailedLogins
         << ",\"lockoutDurationSec\":" << compliance_.lockoutDurationSec
         << ",\"requireMFA\":" << (compliance_.requireMFA ? "true" : "false")
         << ",\"auditHashChain\":" << (compliance_.auditHashChain ? "true" : "false")
         << "}\n";

    file << "}\n";

    return AuthResult::ok("Configuration saved");
}

// ============================================================================
// Internal Helpers
// ============================================================================

void RBACEngine::computeEffectivePermissions(User& user) const {
    Permission effective = static_cast<Permission>(0);
    for (const auto& roleName : user.roles) {
        effective = effective | resolvePermissions(roleName);
    }
    user.effectivePermissions = effective;
}

void RBACEngine::generateSessionToken(char* outToken, size_t tokenLen) const {
    // Generate cryptographically random token
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "-_";

#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContextW(&hProv, nullptr, nullptr,
                              PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        uint8_t randomBytes[128];
        CryptGenRandom(hProv, static_cast<DWORD>(tokenLen - 1), randomBytes);
        CryptReleaseContext(hProv, 0);

        for (size_t i = 0; i < tokenLen - 1; i++) {
            outToken[i] = chars[randomBytes[i] % (sizeof(chars) - 1)];
        }
        outToken[tokenLen - 1] = '\0';
        return;
    }
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        uint8_t randomBytes[128];
        ssize_t n = read(fd, randomBytes, tokenLen - 1);
        close(fd);
        if (n > 0) {
            for (size_t i = 0; i < static_cast<size_t>(n); i++) {
                outToken[i] = chars[randomBytes[i] % (sizeof(chars) - 1)];
            }
            outToken[n] = '\0';
            return;
        }
    }
#endif

    // Fallback: pseudo-random (not cryptographic)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, sizeof(chars) - 2);
    for (size_t i = 0; i < tokenLen - 1; i++) {
        outToken[i] = chars[dist(gen)];
    }
    outToken[tokenLen - 1] = '\0';
}

void RBACEngine::hashAuditEntry(AuditEntry& entry,
                                 const uint8_t* prevHash) const {
    memcpy(entry.prevHash, prevHash, 32);

    // Build data to hash
    std::string hashInput;
    hashInput += std::to_string(entry.sequenceId);
    hashInput += std::to_string(entry.timestamp);
    hashInput += entry.userId;
    hashInput += entry.action;
    hashInput += entry.resource;
    hashInput += entry.detail;
    hashInput.append(reinterpret_cast<const char*>(prevHash), 32);

    sha256_compute(reinterpret_cast<const uint8_t*>(hashInput.data()),
                   hashInput.size(), entry.entryHash);
}

uint64_t RBACEngine::now() const {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

} // namespace Auth
} // namespace RawrXD
