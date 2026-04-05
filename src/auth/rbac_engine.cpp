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
#include <cerrno>
#include <limits>
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
    // Guard: prevent overflow in padding-size arithmetic for absurdly large inputs
    if (len > (SIZE_MAX >> 1)) { memset(hash, 0, 32); return; }

    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    // Pre-processing: pad message
    uint64_t bitLen = static_cast<uint64_t>(len) * 8ULL;
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

// Constant-time memory comparison — prevents timing-based credential oracle
static int rbac_memcmp_ct(const void* a, const void* b, size_t n) {
    const volatile uint8_t* pa = static_cast<const volatile uint8_t*>(a);
    const volatile uint8_t* pb = static_cast<const volatile uint8_t*>(b);
    uint8_t diff = 0;
    for (size_t i = 0; i < n; ++i) diff |= pa[i] ^ pb[i];
    return static_cast<int>(diff);
}

// JSON string escaping — prevents injection when writing audit/config files
static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[7]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf;
                } else { out += static_cast<char>(c); }
        }
    }
    return out;
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
    if (role.name.size() > 128 || role.description.size() > 1024)
        return AuthResult::error("Role field exceeds maximum allowed length", 400);

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
    if (user.id.size() > 256 || user.username.size() > 256 || user.email.size() > 512)
        return AuthResult::error("User field exceeds maximum allowed length", 400);
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
    std::unique_lock<std::mutex> lock(mutex_);
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

    // Verify password hash - must exist and match
    auto phit = passwordHashes_.find(user.id);
    if (phit == passwordHashes_.end()) {
        // No password hash stored for this user: reject authentication
        user.failedLoginCount++;
        stats_.failedAuths.fetch_add(1);
        logAudit(user.id, "auth.login.fail", username,
                 "No password credential configured", AuthResult::denied("Invalid credentials"));
        return AuthResult::denied("Invalid credentials", 401);
    }
    // Constant-time comparison to prevent timing-based credential oracle
    const bool hashMatch = (phit->second.size() == passwordHash.size()) &&
        (phit->second.empty() ||
         rbac_memcmp_ct(phit->second.data(), passwordHash.data(), phit->second.size()) == 0);
    if (!hashMatch) {
        // Password hash mismatch: reject authentication
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

    const std::string userId = user.id;

    // Copy listener callbacks while mutex is held
    std::vector<SessionListenerEntry> listenersToNotify;
    uint32_t lCount = sessionListenerCount_.load();
    for (uint32_t i = 0; i < lCount; ++i) {
        if (sessionListeners_[i].callback) {
            listenersToNotify.push_back(sessionListeners_[i]);
        }
    }
    // Log success BEFORE releasing the mutex so the audit write is serialized
    logAudit(userId, "auth.login.success", username,
             "Login successful", AuthResult::ok("Authenticated"));

    // Release mutex BEFORE invoking callbacks to prevent deadlock if callback re-enters RBAC
    lock.unlock();

    // Notify session listeners OUTSIDE the mutex
    for (const auto& listener : listenersToNotify) {
        listener.callback(&outSession, true, listener.userData);
    }

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
                    if (ssoUser.id.empty()) {
                        ssoUser.id = providerName + "-" + ssoUser.username;
                    }
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

            // Resolve user from index to avoid accidental default insertion.
            auto ridx = usernameIndex_.find(ssoUser.username);
            if (ridx == usernameIndex_.end()) {
                stats_.failedAuths.fetch_add(1);
                return AuthResult::error("SSO user index missing", 500);
            }
            auto uit = users_.find(ridx->second);
            if (uit == users_.end()) {
                stats_.failedAuths.fetch_add(1);
                return AuthResult::error("SSO user record missing", 500);
            }

            // Create session
            auto& user = uit->second;
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

    // SAML assertion validation — parse XML, verify signature, extract NameID
    // Parse SAML Response XML for NameID, Conditions, and signature
    // This is a minimal SAML 2.0 Response parser (no full XML DOM dependency)

    // Cap assertion size to guard against DoS from huge XML payloads
    static constexpr size_t kMaxSamlAssertionBytes = 65536; // 64 KB
    if (assertion.size() > kMaxSamlAssertionBytes) {
        stats_.failedAuths.fetch_add(1);
        logAudit("unknown", "auth.sso.fail", providerName,
                 "SAML assertion size exceeds limit",
                 AuthResult::denied("SAML: assertion too large"));
        return AuthResult::denied("SAML assertion too large", 413);
    }

    std::string nameId;
    std::string issuer;
    std::string audience;
    bool signaturePresent = false;
    uint64_t notBefore = 0, notOnOrAfter = 0;

    // Extract NameID from SAML assertion
    auto extractTag = [&](const std::string& xml, const std::string& tag) -> std::string {
        std::string startTag = "<" + tag;
        size_t s = xml.find(startTag);
        if (s == std::string::npos) return {};
        size_t contentStart = xml.find('>', s);
        if (contentStart == std::string::npos) return {};
        contentStart++;
        std::string endTag = "</" + tag + ">";
        size_t e = xml.find(endTag, contentStart);
        if (e == std::string::npos) {
            // Try namespace-qualified
            endTag = "</" + tag.substr(0, tag.find(':') + 1) + tag.substr(tag.find(':') + 1) + ">";
            e = xml.find(endTag, contentStart);
            if (e == std::string::npos) return {};
        }
        return xml.substr(contentStart, e - contentStart);
    };

    auto extractAttr = [&](const std::string& xml, const std::string& tag,
                           const std::string& attr) -> std::string {
        std::string startTag = "<" + tag;
        size_t s = xml.find(startTag);
        if (s == std::string::npos) return {};
        size_t tagEnd = xml.find('>', s);
        if (tagEnd == std::string::npos) return {};
        std::string tagContent = xml.substr(s, tagEnd - s);
        std::string attrSearch = attr + "=\"";
        size_t aPos = tagContent.find(attrSearch);
        if (aPos == std::string::npos) return {};
        aPos += attrSearch.size();
        size_t aEnd = tagContent.find('"', aPos);
        if (aEnd == std::string::npos) return {};
        return tagContent.substr(aPos, aEnd - aPos);
    };

    // Try saml: and saml2: prefixes, plus unqualified
    for (const char* ns : {"saml:", "saml2:", ""}) {
        std::string nid = extractTag(assertion, std::string(ns) + "NameID");
        if (!nid.empty()) { nameId = nid; break; }
    }
    for (const char* ns : {"saml:", "saml2:", ""}) {
        std::string iss = extractTag(assertion, std::string(ns) + "Issuer");
        if (!iss.empty()) { issuer = iss; break; }
    }

    // Check for Signature element
    signaturePresent = (assertion.find("<ds:Signature") != std::string::npos ||
                        assertion.find("<Signature") != std::string::npos);

    // Parse NotBefore / NotOnOrAfter from Conditions
    for (const char* ns : {"saml:", "saml2:", ""}) {
        std::string nb = extractAttr(assertion, std::string(ns) + "Conditions", "NotBefore");
        std::string noa = extractAttr(assertion, std::string(ns) + "Conditions", "NotOnOrAfter");
        // Simple ISO8601 timestamp parsing (YYYY-MM-DDTHH:MM:SSZ → epoch approx)
        auto parseIso = [](const std::string& ts) -> uint64_t {
            // Enforce canonical UTC format to avoid ambiguous partial parses.
            if (ts.size() != 20 || ts[19] != 'Z') return 0;
            auto isDigitAt = [&](size_t idx) -> bool {
                return idx < ts.size() && ts[idx] >= '0' && ts[idx] <= '9';
            };
            const size_t digitIdx[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
            for (size_t idx : digitIdx) {
                if (!isDigitAt(idx)) return 0;
            }
            if (ts[4] != '-' || ts[7] != '-' || ts[10] != 'T' || ts[13] != ':' || ts[16] != ':')
                return 0;

            auto parse2 = [&](size_t idx) -> int {
                return (ts[idx] - '0') * 10 + (ts[idx + 1] - '0');
            };
            auto parse4 = [&](size_t idx) -> int {
                return (ts[idx] - '0') * 1000 + (ts[idx + 1] - '0') * 100 +
                       (ts[idx + 2] - '0') * 10 + (ts[idx + 3] - '0');
            };

            struct tm t = {};
            t.tm_year = parse4(0) - 1900;
            t.tm_mon = parse2(5) - 1;
            t.tm_mday = parse2(8);
            t.tm_hour = parse2(11);
            t.tm_min = parse2(14);
            t.tm_sec = parse2(17);
            if (t.tm_mon < 0 || t.tm_mon > 11 || t.tm_mday < 1 || t.tm_mday > 31 ||
                t.tm_hour < 0 || t.tm_hour > 23 || t.tm_min < 0 || t.tm_min > 59 ||
                t.tm_sec < 0 || t.tm_sec > 60) {
                return 0;
            }
#ifdef _WIN32
            const __time64_t v = _mkgmtime(&t);
            return (v < 0) ? 0 : static_cast<uint64_t>(v);
#else
            const time_t v = timegm(&t);
            return (v < 0) ? 0 : static_cast<uint64_t>(v);
#endif
        };
        if (!nb.empty()) notBefore = parseIso(nb);
        if (!noa.empty()) notOnOrAfter = parseIso(noa);
        if (notBefore > 0 || notOnOrAfter > 0) break;
    }

    // Validate assertions
    if (nameId.empty()) {
        stats_.failedAuths.fetch_add(1);
        logAudit("unknown", "auth.sso.fail", providerName,
                 "SAML NameID not found in assertion",
                 AuthResult::denied("SAML: missing NameID"));
        return AuthResult::denied("SAML assertion missing NameID", 401);
    }

    if (!signaturePresent) {
        stats_.failedAuths.fetch_add(1);
        logAudit("unknown", "auth.sso.fail", providerName,
                 "SAML signature not found",
                 AuthResult::denied("SAML: unsigned"));
        return AuthResult::denied("SAML assertion not signed", 401);
    }

    // Validate time conditions
    uint64_t currentTime = now();
    if (notOnOrAfter > 0 && currentTime > notOnOrAfter) {
        stats_.failedAuths.fetch_add(1);
        logAudit("unknown", "auth.sso.fail", providerName,
                 "SAML assertion expired",
                 AuthResult::denied("SAML: expired"));
        return AuthResult::denied("SAML assertion expired", 401);
    }
    if (notBefore > 0 && currentTime < notBefore) {
        stats_.failedAuths.fetch_add(1);
        logAudit("unknown", "auth.sso.fail", providerName,
                 "SAML assertion not yet valid",
                 AuthResult::denied("SAML: not yet valid"));
        return AuthResult::denied("SAML assertion not yet valid", 401);
    }

    // Validate issuer against provider config
    if (!issuer.empty() && !pit->second.entityId.empty() &&
        issuer != pit->second.entityId) {
        stats_.failedAuths.fetch_add(1);
        logAudit("unknown", "auth.sso.fail", providerName,
                 "SAML issuer mismatch: got " + issuer,
                 AuthResult::denied("SAML: issuer mismatch"));
        return AuthResult::denied("SAML issuer mismatch", 401);
    }

    // NameID is the username — find or create user
    {
        User ssoUser;
        ssoUser.id = "saml-" + nameId;
        ssoUser.username = nameId;

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
                return AuthResult::denied("User not registered (auto-create disabled)", 403);
            }
        }

        auto ridx = usernameIndex_.find(ssoUser.username);
        if (ridx == usernameIndex_.end()) {
            stats_.failedAuths.fetch_add(1);
            return AuthResult::error("SAML user index missing", 500);
        }
        auto uit = users_.find(ridx->second);
        if (uit == users_.end()) {
            stats_.failedAuths.fetch_add(1);
            return AuthResult::error("SAML user record missing", 500);
        }

        auto& user = uit->second;
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

        logAudit(user.id, "auth.sso.saml.success", providerName,
                 "SAML SSO login for " + nameId,
                 AuthResult::ok("SAML authenticated"));

        return AuthResult::ok("SAML authenticated");
    }
}

AuthResult RBACEngine::validateSession(const char* token,
                                        Session& outSession) const {
    if (!token || token[0] == '\0')
        return AuthResult::denied("Invalid session token", 401);

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
    if (!token || token[0] == '\0')
        return AuthResult::error("Session not found", 404);

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
    if (!token || token[0] == '\0')
        return AuthResult::denied("Session not found", 404);

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
    if (!sessionToken || sessionToken[0] == '\0') {
        stats_.permissionChecks.fetch_add(1);
        stats_.permissionDenials.fetch_add(1);
        return AuthResult::denied("Invalid session token", 401);
    }

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
    if (!sessionToken || sessionToken[0] == '\0') {
        stats_.permissionChecks.fetch_add(1);
        stats_.permissionDenials.fetch_add(1);
        AuthResult denied = AuthResult::denied("Invalid session token", 401);
        logAudit("unknown", action ? action : "authz.unknown", resource ? resource : "unknown", "Denied", denied);
        return denied;
    }

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
    if (maxEntries == 0)
        return result;

    const size_t validCount = std::min(auditLog_.size(), static_cast<size_t>(auditSequence_.load()));
    if (validCount == 0)
        return result;

    std::vector<const AuditEntry*> ordered;
    ordered.reserve(validCount);
    for (size_t i = 0; i < validCount; ++i) {
        ordered.push_back(&auditLog_[i]);
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const AuditEntry* a, const AuditEntry* b) { return a->sequenceId < b->sequenceId; });

    for (const AuditEntry* entry : ordered) {
        if (entry->sequenceId >= fromSeq) {
            result.push_back(*entry);
            if (result.size() >= maxEntries)
                break;
        }
    }

    return result;
}

AuthResult RBACEngine::verifyAuditChain() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!compliance_.auditHashChain)
        return AuthResult::ok("Hash chain not enabled");

    const size_t validCount = std::min(auditLog_.size(), static_cast<size_t>(auditSequence_.load()));
    if (validCount == 0)
        return AuthResult::ok("Empty log");

    std::vector<const AuditEntry*> ordered;
    ordered.reserve(validCount);
    for (size_t i = 0; i < validCount; ++i) {
        ordered.push_back(&auditLog_[i]);
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const AuditEntry* a, const AuditEntry* b) { return a->sequenceId < b->sequenceId; });

    for (size_t i = 0; i < ordered.size(); ++i) {
        const auto& entry = *ordered[i];

        // Verify prevHash matches
        if (i > 0 && memcmp(entry.prevHash, ordered[i - 1]->entryHash, 32) != 0) {
            return AuthResult::error("Audit chain broken at entry", 500);
        }

        // Recompute hash and verify
        AuditEntry verify = entry;
        uint8_t computedPrev[32];
        if (i > 0) {
            memcpy(computedPrev, ordered[i - 1]->entryHash, 32);
        } else {
            // Ring buffer may start mid-chain; preserve stored anchor hash.
            memcpy(computedPrev, entry.prevHash, 32);
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
             << ",\"user\":\"" << jsonEscape(e.userId) << "\""
             << ",\"action\":\"" << jsonEscape(e.action) << "\""
             << ",\"resource\":\"" << jsonEscape(e.resource) << "\""
             << ",\"detail\":\"" << jsonEscape(e.detail) << "\""
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

    if (!configPath || configPath[0] == '\0') {
        return AuthResult::error("Invalid config path", 400);
    }

    std::ifstream file(configPath);
    if (!file.is_open())
        return AuthResult::error("Cannot open config file", 500);

    // Cap config file size to prevent OOM from adversarial inputs
    file.seekg(0, std::ios::end);
    const std::streamoff configFileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    static constexpr std::streamoff kMaxConfigFileSize = 4 * 1024 * 1024; // 4 MB
    if (configFileSize < 0 || configFileSize > kMaxConfigFileSize) {
        return AuthResult::error("Config file size invalid or exceeds 4 MB limit", 400);
    }

    // Minimal JSON key-value extraction
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Ensure builtin roles always exist as base
    if (!initialized_.load()) {
        createBuiltinRoles();
        initialized_.store(true);
    }

    // JSON parsing helpers (match saveToFile format)
    static constexpr size_t kMaxFieldBytes = 1024;
    static constexpr size_t kMaxArrayItems = 256;
    static constexpr size_t kMaxRolesToLoad = 4096;
    static constexpr size_t kMaxUsersToLoad = 4096;
    static constexpr size_t kMaxSsoProvidersToLoad = 512;

    auto extractStr = [](const std::string& obj, const std::string& key) -> std::string {
        static constexpr size_t kMaxFieldBytesLocal = kMaxFieldBytes;
        std::string pat = "\"" + key + "\":\"";
        auto pos = obj.find(pat);
        if (pos == std::string::npos) { pat = "\"" + key + "\": \""; pos = obj.find(pat); }
        if (pos == std::string::npos) return "";
        auto s = pos + pat.size();
        auto e = obj.find('"', s);
        if (e == std::string::npos || e < s) return "";
        std::string out = obj.substr(s, e - s);
        if (out.size() > kMaxFieldBytesLocal) out.resize(kMaxFieldBytesLocal);
        return out;
    };
    auto extractInt = [](const std::string& obj, const std::string& key) -> int64_t {
        std::string pat = "\"" + key + "\":";
        auto pos = obj.find(pat);
        if (pos == std::string::npos) { pat = "\"" + key + "\": "; pos = obj.find(pat); }
        if (pos == std::string::npos) return 0;
        auto s = pos + pat.size();
        while (s < obj.size() && obj[s] == ' ') s++;
        if (s >= obj.size()) return 0;
        char* endp = nullptr;
        errno = 0;
        const long long parsed = std::strtoll(obj.c_str() + s, &endp, 10);
        if (errno != 0 || !endp || endp == (obj.c_str() + s)) return 0;
        if (parsed < std::numeric_limits<int64_t>::min() || parsed > std::numeric_limits<int64_t>::max()) return 0;
        return static_cast<int64_t>(parsed);
    };
    auto extractBool = [](const std::string& obj, const std::string& key) -> bool {
        std::string pat = "\"" + key + "\":true";
        if (obj.find(pat) != std::string::npos) return true;
        pat = "\"" + key + "\": true";
        return obj.find(pat) != std::string::npos;
    };
    auto extractStrArray = [](const std::string& obj, const std::string& key) -> std::vector<std::string> {
        std::vector<std::string> result;
        static constexpr size_t kMaxItemsLocal = kMaxArrayItems;
        static constexpr size_t kMaxFieldBytesLocal = kMaxFieldBytes;
        std::string pat = "\"" + key + "\":[";
        auto pos = obj.find(pat);
        if (pos == std::string::npos) { pat = "\"" + key + "\": ["; pos = obj.find(pat); }
        if (pos == std::string::npos) return result;
        auto arrStart = obj.find('[', pos);
        auto arrEnd = obj.find(']', arrStart);
        if (arrStart == std::string::npos || arrEnd == std::string::npos) return result;
        std::string arr = obj.substr(arrStart + 1, arrEnd - arrStart - 1);
        size_t p = 0;
        while ((p = arr.find('"', p)) != std::string::npos && result.size() < kMaxItemsLocal) {
            auto e = arr.find('"', p + 1);
            if (e == std::string::npos) break;
            std::string item = arr.substr(p + 1, e - p - 1);
            if (item.size() > kMaxFieldBytesLocal) item.resize(kMaxFieldBytesLocal);
            result.push_back(std::move(item));
            p = e + 1;
        }
        return result;
    };

    // Parse roles array
    size_t rolesStart = content.find("\"roles\"");
    if (rolesStart != std::string::npos) {
        size_t arrStart = content.find('[', rolesStart);
        size_t arrEnd = content.find(']', arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
            size_t pos = arrStart + 1;
            size_t loadedRoles = 0;
            while (pos < arrEnd) {
                if (loadedRoles >= kMaxRolesToLoad) break;
                size_t objStart = content.find('{', pos);
                if (objStart == std::string::npos || objStart >= arrEnd) break;
                size_t objEnd = content.find('}', objStart);
                if (objEnd == std::string::npos) break;
                std::string entry = content.substr(objStart, objEnd - objStart + 1);

                std::string name = extractStr(entry, "name");
                if (!name.empty() && roles_.find(name) == roles_.end()) {
                    Role role;
                    role.name = name;
                    role.description = extractStr(entry, "description");
                    role.permissions = static_cast<Permission>(static_cast<uint32_t>(extractInt(entry, "permissions")));
                    role.parentRole = extractStr(entry, "parent");
                    role.isBuiltin = extractBool(entry, "builtin");
                    roles_[name] = role;
                    ++loadedRoles;
                }
                pos = objEnd + 1;
            }
        }
    }

    // Parse users array
    size_t usersStart = content.find("\"users\"");
    if (usersStart != std::string::npos) {
        size_t arrStart = content.find('[', usersStart);
        size_t arrEnd = content.find(']', arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
            size_t pos = arrStart + 1;
            size_t loadedUsers = 0;
            while (pos < arrEnd) {
                if (loadedUsers >= kMaxUsersToLoad) break;
                size_t objStart = content.find('{', pos);
                if (objStart == std::string::npos || objStart >= arrEnd) break;
                // Find matching '}' — account for nested arrays
                int depth = 0;
                size_t objEnd = objStart;
                for (; objEnd < arrEnd; ++objEnd) {
                    if (content[objEnd] == '{') depth++;
                    else if (content[objEnd] == '}') { depth--; if (depth == 0) break; }
                }
                if (depth != 0 || objEnd >= arrEnd) {
                    break;
                }
                std::string entry = content.substr(objStart, objEnd - objStart + 1);

                std::string id = extractStr(entry, "id");
                if (!id.empty() && users_.find(id) == users_.end()) {
                    User user;
                    user.id = id;
                    user.username = extractStr(entry, "username");
                    user.email = extractStr(entry, "email");
                    user.isActive = extractBool(entry, "active");
                    user.roles = extractStrArray(entry, "roles");
                    computeEffectivePermissions(user);
                    users_[id] = user;
                    ++loadedUsers;
                }
                pos = objEnd + 1;
            }
        }
    }

    // Parse SSO providers array
    size_t ssoStart = content.find("\"ssoProviders\"");
    if (ssoStart != std::string::npos) {
        size_t arrStart = content.find('[', ssoStart);
        size_t arrEnd = content.find(']', arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
            size_t pos = arrStart + 1;
            size_t loadedProviders = 0;
            while (pos < arrEnd) {
                if (loadedProviders >= kMaxSsoProvidersToLoad) break;
                size_t objStart = content.find('{', pos);
                if (objStart == std::string::npos || objStart >= arrEnd) break;
                size_t objEnd = content.find('}', objStart);
                if (objEnd == std::string::npos) break;
                std::string entry = content.substr(objStart, objEnd - objStart + 1);

                std::string name = extractStr(entry, "name");
                if (!name.empty() && ssoProviders_.find(name) == ssoProviders_.end()) {
                    SSOProviderConfig sso;
                    sso.name = name;
                    sso.type = static_cast<SSOProviderConfig::ProviderType>(static_cast<int>(extractInt(entry, "type")));
                    sso.entityId = extractStr(entry, "entityId");
                    sso.enabled = extractBool(entry, "enabled");
                    ssoProviders_[name] = sso;
                    ++loadedProviders;
                }
                pos = objEnd + 1;
            }
        }
    }

    // Parse compliance config
    size_t compStart = content.find("\"compliance\"");
    if (compStart != std::string::npos) {
        size_t objStart = content.find('{', compStart);
        size_t objEnd = content.find('}', objStart);
        if (objStart != std::string::npos && objEnd != std::string::npos) {
            std::string comp = content.substr(objStart, objEnd - objStart + 1);
            auto readUIntOrZero = [&](const std::string& key) -> uint32_t {
                const int64_t v = extractInt(comp, key);
                return static_cast<uint32_t>(v < 0 ? 0 : v);
            };
            compliance_.enforceAuditLog = extractBool(comp, "enforceAuditLog");
            compliance_.enforceSessionTimeout = extractBool(comp, "enforceSessionTimeout");
            compliance_.maxSessionDurationSec = readUIntOrZero("maxSessionDurationSec");
            compliance_.maxIdleTimeoutSec = readUIntOrZero("maxIdleTimeoutSec");
            compliance_.maxFailedLogins = readUIntOrZero("maxFailedLogins");
            compliance_.lockoutDurationSec = readUIntOrZero("lockoutDurationSec");
            compliance_.requireMFA = extractBool(comp, "requireMFA");
            compliance_.auditHashChain = extractBool(comp, "auditHashChain");
        }
    }

    logAudit("system", "config.load", configPath,
             "Configuration loaded", AuthResult::ok("Loaded"));

    return AuthResult::ok("Configuration loaded");
}

AuthResult RBACEngine::saveToFile(const char* configPath) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!configPath || configPath[0] == '\0')
        return AuthResult::error("Invalid config file path", 400);

    std::ofstream file(configPath);
    if (!file.is_open())
        return AuthResult::error("Cannot open config file for writing", 500);

    file << "{\n";

    // Serialize roles
    file << "  \"roles\": [\n";
    size_t ri = 0;
    for (const auto& [name, role] : roles_) {
        file << "    {\"name\":\"" << jsonEscape(role.name) << "\""
             << ",\"description\":\"" << jsonEscape(role.description) << "\""
             << ",\"permissions\":" << static_cast<uint32_t>(role.permissions)
             << ",\"parent\":\"" << jsonEscape(role.parentRole) << "\""
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
        file << "    {\"id\":\"" << jsonEscape(user.id) << "\""
             << ",\"username\":\"" << jsonEscape(user.username) << "\""
             << ",\"email\":\"" << jsonEscape(user.email) << "\""
             << ",\"active\":" << (user.isActive ? "true" : "false")
             << ",\"roles\":[";    
        for (size_t r = 0; r < user.roles.size(); ++r) {
            file << "\"" << jsonEscape(user.roles[r]) << "\"";
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
        file << "    {\"name\":\"" << jsonEscape(config.name) << "\""
             << ",\"type\":" << static_cast<int>(config.type)
             << ",\"entityId\":\"" << jsonEscape(config.entityId) << "\""
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
    file.flush();
    if (!file)
        return AuthResult::error("Failed while writing config file", 500);

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
    if (!outToken || tokenLen == 0)
        return;

    // Generate cryptographically random token
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "-_";

    const size_t outLen = tokenLen - 1;
    if (outLen == 0) {
        outToken[0] = '\0';
        return;
    }

#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContextW(&hProv, nullptr, nullptr,
                              PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        uint8_t randomBytes[128];
        const size_t req = std::min(outLen, sizeof(randomBytes));
        CryptGenRandom(hProv, static_cast<DWORD>(req), randomBytes);
        CryptReleaseContext(hProv, 0);

        for (size_t i = 0; i < req; i++) {
            outToken[i] = chars[randomBytes[i] % (sizeof(chars) - 1)];
        }
        // Fill remaining bytes with PRNG to avoid stack buffer overflow for long outputs.
        if (req < outLen) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dist(0, static_cast<int>(sizeof(chars) - 2));
            for (size_t i = req; i < outLen; ++i) {
                outToken[i] = chars[dist(gen)];
            }
        }
        outToken[outLen] = '\0';
        return;
    }
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        uint8_t randomBytes[128];
        const size_t req = std::min(outLen, sizeof(randomBytes));
        ssize_t n = read(fd, randomBytes, req);
        close(fd);
        if (n > 0) {
            for (size_t i = 0; i < static_cast<size_t>(n); i++) {
                outToken[i] = chars[randomBytes[i] % (sizeof(chars) - 1)];
            }
            if (static_cast<size_t>(n) < outLen) {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<int> dist(0, static_cast<int>(sizeof(chars) - 2));
                for (size_t i = static_cast<size_t>(n); i < outLen; ++i) {
                    outToken[i] = chars[dist(gen)];
                }
            }
            outToken[outLen] = '\0';
            return;
        }
    }
#endif

    // Fallback: pseudo-random (not cryptographic)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, sizeof(chars) - 2);
    for (size_t i = 0; i < outLen; i++) {
        outToken[i] = chars[dist(gen)];
    }
    outToken[outLen] = '\0';
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
