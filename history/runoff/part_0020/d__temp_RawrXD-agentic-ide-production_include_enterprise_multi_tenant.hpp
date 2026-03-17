/**
 * @file multi_tenant.hpp
 * @brief Enterprise Multi-Tenant Support
 * 
 * Features:
 * - Tenant isolation
 * - Organization management
 * - Namespace separation
 * - Per-tenant configuration
 * - Resource quotas
 * - Tenant-aware routing
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <optional>
#include <functional>
#include <set>

namespace enterprise {

// =============================================================================
// Tenant Types
// =============================================================================

enum class TenantStatus {
    ACTIVE,
    SUSPENDED,
    PENDING,
    DELETED
};

enum class TenantTier {
    FREE,
    STARTER,
    PROFESSIONAL,
    ENTERPRISE
};

// =============================================================================
// Resource Quotas
// =============================================================================

struct ResourceQuotas {
    size_t maxUsers = 5;
    size_t maxProjects = 3;
    size_t maxStorageBytes = 1024 * 1024 * 1024;  // 1 GB
    size_t maxApiCallsPerDay = 10000;
    size_t maxConcurrentConnections = 10;
    size_t maxModelRequestsPerDay = 1000;
    size_t maxAgentExecutionsPerDay = 100;
    int retentionDays = 30;
    
    // Feature flags
    bool customModelsAllowed = false;
    bool advancedAnalyticsAllowed = false;
    bool prioritySupport = false;
    bool ssoEnabled = false;
    bool auditLogsEnabled = false;
};

// =============================================================================
// Resource Usage
// =============================================================================

struct ResourceUsage {
    size_t currentUsers = 0;
    size_t currentProjects = 0;
    size_t currentStorageBytes = 0;
    size_t apiCallsToday = 0;
    size_t currentConnections = 0;
    size_t modelRequestsToday = 0;
    size_t agentExecutionsToday = 0;
    std::chrono::system_clock::time_point lastResetDate;
};

// =============================================================================
// Tenant Configuration
// =============================================================================

struct TenantConfig {
    std::string displayName;
    std::string timezone = "UTC";
    std::string locale = "en_US";
    std::string defaultModelId;
    bool mfaRequired = false;
    int sessionTimeoutMinutes = 480;
    int passwordMinLength = 12;
    bool requirePasswordComplexity = true;
    std::vector<std::string> allowedDomains;
    std::unordered_map<std::string, std::string> customSettings;
};

// =============================================================================
// Tenant Model
// =============================================================================

struct Tenant {
    std::string id;
    std::string name;
    std::string slug;  // URL-friendly identifier
    TenantStatus status = TenantStatus::PENDING;
    TenantTier tier = TenantTier::FREE;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point updatedAt;
    std::string ownerId;
    ResourceQuotas quotas;
    ResourceUsage usage;
    TenantConfig config;
    std::unordered_map<std::string, std::string> metadata;
};

// =============================================================================
// Organization Model
// =============================================================================

struct Organization {
    std::string id;
    std::string tenantId;
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point createdAt;
    std::vector<std::string> memberUserIds;
    std::vector<std::string> adminUserIds;
    std::unordered_map<std::string, std::string> settings;
};

// =============================================================================
// Namespace
// =============================================================================

struct Namespace {
    std::string id;
    std::string tenantId;
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point createdAt;
    std::unordered_map<std::string, std::string> labels;
    bool isDefault = false;
};

// =============================================================================
// Tenant Context
// =============================================================================

class TenantContext {
public:
    static TenantContext& current();
    static void setCurrent(const std::string& tenantId);
    static void clear();
    
    std::string getTenantId() const { return m_tenantId; }
    std::string getOrganizationId() const { return m_organizationId; }
    std::string getNamespaceId() const { return m_namespaceId; }
    std::string getUserId() const { return m_userId; }
    
    void setOrganizationId(const std::string& orgId) { m_organizationId = orgId; }
    void setNamespaceId(const std::string& nsId) { m_namespaceId = nsId; }
    void setUserId(const std::string& userId) { m_userId = userId; }
    
    bool isValid() const { return !m_tenantId.empty(); }
    
    // Scoped context
    class Scope {
    public:
        explicit Scope(const std::string& tenantId);
        ~Scope();
    private:
        std::string m_previousTenantId;
    };
    
private:
    TenantContext() = default;
    
    std::string m_tenantId;
    std::string m_organizationId;
    std::string m_namespaceId;
    std::string m_userId;
    
    static thread_local TenantContext s_current;
};

// =============================================================================
// Tenant Manager
// =============================================================================

class TenantManager {
public:
    static TenantManager& instance();
    
    // Tenant CRUD
    std::string createTenant(const std::string& name, const std::string& ownerId,
                             TenantTier tier = TenantTier::FREE);
    std::optional<Tenant> getTenant(const std::string& tenantId);
    std::optional<Tenant> getTenantBySlug(const std::string& slug);
    bool updateTenant(const Tenant& tenant);
    bool deleteTenant(const std::string& tenantId);
    
    // Status management
    bool activateTenant(const std::string& tenantId);
    bool suspendTenant(const std::string& tenantId, const std::string& reason = "");
    
    // Tier management
    bool upgradeTier(const std::string& tenantId, TenantTier newTier);
    ResourceQuotas getQuotasForTier(TenantTier tier) const;
    
    // Configuration
    bool updateConfig(const std::string& tenantId, const TenantConfig& config);
    std::optional<TenantConfig> getConfig(const std::string& tenantId);
    
    // Resource usage
    bool trackUsage(const std::string& tenantId, const std::string& resourceType, size_t amount = 1);
    bool checkQuota(const std::string& tenantId, const std::string& resourceType, size_t requestedAmount = 1);
    ResourceUsage getUsage(const std::string& tenantId);
    void resetDailyUsage(const std::string& tenantId);
    
    // Listing
    std::vector<Tenant> listTenants(TenantStatus status = TenantStatus::ACTIVE);
    std::vector<Tenant> listTenantsByOwner(const std::string& ownerId);
    
private:
    TenantManager();
    
    std::string generateTenantId();
    std::string generateSlug(const std::string& name);
    
    std::unordered_map<std::string, Tenant> m_tenants;
    std::unordered_map<std::string, std::string> m_slugToId;
    mutable std::mutex m_mutex;
    std::atomic<uint64_t> m_counter{0};
};

// =============================================================================
// Organization Manager
// =============================================================================

class OrganizationManager {
public:
    static OrganizationManager& instance();
    
    // Organization CRUD
    std::string createOrganization(const std::string& tenantId, const std::string& name,
                                    const std::string& creatorUserId);
    std::optional<Organization> getOrganization(const std::string& orgId);
    bool updateOrganization(const Organization& org);
    bool deleteOrganization(const std::string& orgId);
    
    // Member management
    bool addMember(const std::string& orgId, const std::string& userId);
    bool removeMember(const std::string& orgId, const std::string& userId);
    bool promoteToAdmin(const std::string& orgId, const std::string& userId);
    bool demoteFromAdmin(const std::string& orgId, const std::string& userId);
    
    // Listing
    std::vector<Organization> listOrganizations(const std::string& tenantId);
    std::vector<Organization> listUserOrganizations(const std::string& userId);
    
private:
    OrganizationManager() = default;
    
    std::string generateOrgId();
    
    std::unordered_map<std::string, Organization> m_organizations;
    mutable std::mutex m_mutex;
    std::atomic<uint64_t> m_counter{0};
};

// =============================================================================
// Namespace Manager
// =============================================================================

class NamespaceManager {
public:
    static NamespaceManager& instance();
    
    // Namespace CRUD
    std::string createNamespace(const std::string& tenantId, const std::string& name,
                                 bool isDefault = false);
    std::optional<Namespace> getNamespace(const std::string& nsId);
    std::optional<Namespace> getNamespaceByName(const std::string& tenantId, const std::string& name);
    bool updateNamespace(const Namespace& ns);
    bool deleteNamespace(const std::string& nsId);
    
    // Default namespace
    std::optional<Namespace> getDefaultNamespace(const std::string& tenantId);
    bool setDefaultNamespace(const std::string& tenantId, const std::string& nsId);
    
    // Labels
    bool addLabel(const std::string& nsId, const std::string& key, const std::string& value);
    bool removeLabel(const std::string& nsId, const std::string& key);
    std::vector<Namespace> findByLabel(const std::string& tenantId, 
                                       const std::string& key, const std::string& value);
    
    // Listing
    std::vector<Namespace> listNamespaces(const std::string& tenantId);
    
private:
    NamespaceManager() = default;
    
    std::string generateNsId();
    
    std::unordered_map<std::string, Namespace> m_namespaces;
    mutable std::mutex m_mutex;
    std::atomic<uint64_t> m_counter{0};
};

// =============================================================================
// Tenant-Aware Data Access
// =============================================================================

template<typename T>
class TenantAwareStore {
public:
    explicit TenantAwareStore(const std::string& storeName) : m_storeName(storeName) {}
    
    // Automatically scoped to current tenant
    bool store(const std::string& key, const T& value);
    std::optional<T> get(const std::string& key);
    bool remove(const std::string& key);
    std::vector<std::string> keys();
    void clear();
    
    // Explicit tenant access
    bool storeFor(const std::string& tenantId, const std::string& key, const T& value);
    std::optional<T> getFor(const std::string& tenantId, const std::string& key);
    bool removeFor(const std::string& tenantId, const std::string& key);
    
private:
    std::string buildKey(const std::string& tenantId, const std::string& key) const;
    
    std::string m_storeName;
    std::unordered_map<std::string, T> m_data;
    mutable std::mutex m_mutex;
};

// =============================================================================
// Template Implementation
// =============================================================================

template<typename T>
bool TenantAwareStore<T>::store(const std::string& key, const T& value) {
    if (!TenantContext::current().isValid()) {
        return false;
    }
    return storeFor(TenantContext::current().getTenantId(), key, value);
}

template<typename T>
std::optional<T> TenantAwareStore<T>::get(const std::string& key) {
    if (!TenantContext::current().isValid()) {
        return std::nullopt;
    }
    return getFor(TenantContext::current().getTenantId(), key);
}

template<typename T>
bool TenantAwareStore<T>::remove(const std::string& key) {
    if (!TenantContext::current().isValid()) {
        return false;
    }
    return removeFor(TenantContext::current().getTenantId(), key);
}

template<typename T>
std::vector<std::string> TenantAwareStore<T>::keys() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!TenantContext::current().isValid()) {
        return {};
    }
    
    std::string prefix = TenantContext::current().getTenantId() + ":";
    std::vector<std::string> result;
    
    for (const auto& [fullKey, _] : m_data) {
        if (fullKey.find(prefix) == 0) {
            result.push_back(fullKey.substr(prefix.length()));
        }
    }
    
    return result;
}

template<typename T>
void TenantAwareStore<T>::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!TenantContext::current().isValid()) {
        return;
    }
    
    std::string prefix = TenantContext::current().getTenantId() + ":";
    
    for (auto it = m_data.begin(); it != m_data.end();) {
        if (it->first.find(prefix) == 0) {
            it = m_data.erase(it);
        } else {
            ++it;
        }
    }
}

template<typename T>
bool TenantAwareStore<T>::storeFor(const std::string& tenantId, const std::string& key, const T& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data[buildKey(tenantId, key)] = value;
    return true;
}

template<typename T>
std::optional<T> TenantAwareStore<T>::getFor(const std::string& tenantId, const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_data.find(buildKey(tenantId, key));
    if (it == m_data.end()) {
        return std::nullopt;
    }
    return it->second;
}

template<typename T>
bool TenantAwareStore<T>::removeFor(const std::string& tenantId, const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.erase(buildKey(tenantId, key)) > 0;
}

template<typename T>
std::string TenantAwareStore<T>::buildKey(const std::string& tenantId, const std::string& key) const {
    return tenantId + ":" + key;
}

// =============================================================================
// Tenant Middleware
// =============================================================================

class TenantMiddleware {
public:
    using NextHandler = std::function<void()>;
    
    // Extract tenant from request header
    static bool extractTenantFromHeader(const std::string& headerValue);
    
    // Extract tenant from subdomain
    static bool extractTenantFromSubdomain(const std::string& host);
    
    // Extract tenant from path prefix
    static bool extractTenantFromPath(const std::string& path);
    
    // Validate tenant access
    static bool validateTenantAccess(const std::string& tenantId, const std::string& userId);
    
    // Apply tenant context middleware
    static void apply(const std::string& tenantId, NextHandler next);
};

} // namespace enterprise
