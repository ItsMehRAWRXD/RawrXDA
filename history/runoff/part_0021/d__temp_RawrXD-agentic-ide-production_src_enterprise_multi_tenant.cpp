/**
 * @file multi_tenant.cpp
 * @brief Enterprise Multi-Tenant Support Implementation
 */

#include "enterprise/multi_tenant.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <regex>

namespace enterprise {

// =============================================================================
// TenantContext Implementation
// =============================================================================

thread_local TenantContext TenantContext::s_current;

TenantContext& TenantContext::current() {
    return s_current;
}

void TenantContext::setCurrent(const std::string& tenantId) {
    s_current.m_tenantId = tenantId;
}

void TenantContext::clear() {
    s_current.m_tenantId.clear();
    s_current.m_organizationId.clear();
    s_current.m_namespaceId.clear();
    s_current.m_userId.clear();
}

TenantContext::Scope::Scope(const std::string& tenantId) {
    m_previousTenantId = TenantContext::current().getTenantId();
    TenantContext::setCurrent(tenantId);
}

TenantContext::Scope::~Scope() {
    TenantContext::setCurrent(m_previousTenantId);
}

// =============================================================================
// TenantManager Implementation
// =============================================================================

TenantManager& TenantManager::instance() {
    static TenantManager instance;
    return instance;
}

TenantManager::TenantManager() {
    // Initialize with default quotas for each tier
}

std::string TenantManager::createTenant(const std::string& name, const std::string& ownerId,
                                         TenantTier tier) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Tenant tenant;
    tenant.id = generateTenantId();
    tenant.name = name;
    tenant.slug = generateSlug(name);
    tenant.status = TenantStatus::PENDING;
    tenant.tier = tier;
    tenant.createdAt = std::chrono::system_clock::now();
    tenant.updatedAt = tenant.createdAt;
    tenant.ownerId = ownerId;
    tenant.quotas = getQuotasForTier(tier);
    tenant.usage.lastResetDate = std::chrono::system_clock::now();
    tenant.config.displayName = name;
    
    m_tenants[tenant.id] = tenant;
    m_slugToId[tenant.slug] = tenant.id;
    
    return tenant.id;
}

std::optional<Tenant> TenantManager::getTenant(const std::string& tenantId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenantId);
    if (it == m_tenants.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<Tenant> TenantManager::getTenantBySlug(const std::string& slug) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_slugToId.find(slug);
    if (it == m_slugToId.end()) {
        return std::nullopt;
    }
    
    auto tenantIt = m_tenants.find(it->second);
    if (tenantIt == m_tenants.end()) {
        return std::nullopt;
    }
    
    return tenantIt->second;
}

bool TenantManager::updateTenant(const Tenant& tenant) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenant.id);
    if (it == m_tenants.end()) {
        return false;
    }
    
    // Update slug mapping if changed
    if (it->second.slug != tenant.slug) {
        m_slugToId.erase(it->second.slug);
        m_slugToId[tenant.slug] = tenant.id;
    }
    
    it->second = tenant;
    it->second.updatedAt = std::chrono::system_clock::now();
    
    return true;
}

bool TenantManager::deleteTenant(const std::string& tenantId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenantId);
    if (it == m_tenants.end()) {
        return false;
    }
    
    m_slugToId.erase(it->second.slug);
    it->second.status = TenantStatus::DELETED;
    it->second.updatedAt = std::chrono::system_clock::now();
    
    return true;
}

bool TenantManager::activateTenant(const std::string& tenantId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenantId);
    if (it == m_tenants.end()) {
        return false;
    }
    
    if (it->second.status == TenantStatus::DELETED) {
        return false;  // Cannot reactivate deleted tenant
    }
    
    it->second.status = TenantStatus::ACTIVE;
    it->second.updatedAt = std::chrono::system_clock::now();
    
    return true;
}

bool TenantManager::suspendTenant(const std::string& tenantId, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenantId);
    if (it == m_tenants.end()) {
        return false;
    }
    
    it->second.status = TenantStatus::SUSPENDED;
    it->second.updatedAt = std::chrono::system_clock::now();
    if (!reason.empty()) {
        it->second.metadata["suspendReason"] = reason;
    }
    
    return true;
}

bool TenantManager::upgradeTier(const std::string& tenantId, TenantTier newTier) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenantId);
    if (it == m_tenants.end()) {
        return false;
    }
    
    it->second.tier = newTier;
    it->second.quotas = getQuotasForTier(newTier);
    it->second.updatedAt = std::chrono::system_clock::now();
    
    return true;
}

ResourceQuotas TenantManager::getQuotasForTier(TenantTier tier) const {
    ResourceQuotas quotas;
    
    switch (tier) {
        case TenantTier::FREE:
            quotas.maxUsers = 5;
            quotas.maxProjects = 3;
            quotas.maxStorageBytes = 1024LL * 1024 * 1024;  // 1 GB
            quotas.maxApiCallsPerDay = 10000;
            quotas.maxConcurrentConnections = 10;
            quotas.maxModelRequestsPerDay = 1000;
            quotas.maxAgentExecutionsPerDay = 100;
            quotas.retentionDays = 30;
            quotas.customModelsAllowed = false;
            quotas.advancedAnalyticsAllowed = false;
            quotas.prioritySupport = false;
            quotas.ssoEnabled = false;
            quotas.auditLogsEnabled = false;
            break;
            
        case TenantTier::STARTER:
            quotas.maxUsers = 25;
            quotas.maxProjects = 20;
            quotas.maxStorageBytes = 10LL * 1024 * 1024 * 1024;  // 10 GB
            quotas.maxApiCallsPerDay = 100000;
            quotas.maxConcurrentConnections = 50;
            quotas.maxModelRequestsPerDay = 10000;
            quotas.maxAgentExecutionsPerDay = 1000;
            quotas.retentionDays = 90;
            quotas.customModelsAllowed = false;
            quotas.advancedAnalyticsAllowed = false;
            quotas.prioritySupport = false;
            quotas.ssoEnabled = false;
            quotas.auditLogsEnabled = true;
            break;
            
        case TenantTier::PROFESSIONAL:
            quotas.maxUsers = 100;
            quotas.maxProjects = 100;
            quotas.maxStorageBytes = 100LL * 1024 * 1024 * 1024;  // 100 GB
            quotas.maxApiCallsPerDay = 1000000;
            quotas.maxConcurrentConnections = 200;
            quotas.maxModelRequestsPerDay = 100000;
            quotas.maxAgentExecutionsPerDay = 10000;
            quotas.retentionDays = 365;
            quotas.customModelsAllowed = true;
            quotas.advancedAnalyticsAllowed = true;
            quotas.prioritySupport = true;
            quotas.ssoEnabled = true;
            quotas.auditLogsEnabled = true;
            break;
            
        case TenantTier::ENTERPRISE:
            quotas.maxUsers = 1000000;  // Unlimited
            quotas.maxProjects = 1000000;
            quotas.maxStorageBytes = 1024LL * 1024 * 1024 * 1024;  // 1 TB
            quotas.maxApiCallsPerDay = 100000000;
            quotas.maxConcurrentConnections = 10000;
            quotas.maxModelRequestsPerDay = 10000000;
            quotas.maxAgentExecutionsPerDay = 1000000;
            quotas.retentionDays = 3650;  // 10 years
            quotas.customModelsAllowed = true;
            quotas.advancedAnalyticsAllowed = true;
            quotas.prioritySupport = true;
            quotas.ssoEnabled = true;
            quotas.auditLogsEnabled = true;
            break;
    }
    
    return quotas;
}

bool TenantManager::updateConfig(const std::string& tenantId, const TenantConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenantId);
    if (it == m_tenants.end()) {
        return false;
    }
    
    it->second.config = config;
    it->second.updatedAt = std::chrono::system_clock::now();
    
    return true;
}

std::optional<TenantConfig> TenantManager::getConfig(const std::string& tenantId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenantId);
    if (it == m_tenants.end()) {
        return std::nullopt;
    }
    
    return it->second.config;
}

bool TenantManager::trackUsage(const std::string& tenantId, const std::string& resourceType, size_t amount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenantId);
    if (it == m_tenants.end()) {
        return false;
    }
    
    auto& usage = it->second.usage;
    
    // Check if we need to reset daily counters
    auto now = std::chrono::system_clock::now();
    auto lastReset = std::chrono::system_clock::to_time_t(usage.lastResetDate);
    auto currentTime = std::chrono::system_clock::to_time_t(now);
    
    // Reset if different day
    std::tm lastTm = *std::localtime(&lastReset);
    std::tm currentTm = *std::localtime(&currentTime);
    
    if (lastTm.tm_yday != currentTm.tm_yday || lastTm.tm_year != currentTm.tm_year) {
        usage.apiCallsToday = 0;
        usage.modelRequestsToday = 0;
        usage.agentExecutionsToday = 0;
        usage.lastResetDate = now;
    }
    
    // Track usage
    if (resourceType == "api_calls") {
        usage.apiCallsToday += amount;
    } else if (resourceType == "model_requests") {
        usage.modelRequestsToday += amount;
    } else if (resourceType == "agent_executions") {
        usage.agentExecutionsToday += amount;
    } else if (resourceType == "storage") {
        usage.currentStorageBytes += amount;
    } else if (resourceType == "connections") {
        usage.currentConnections += amount;
    } else if (resourceType == "users") {
        usage.currentUsers += amount;
    } else if (resourceType == "projects") {
        usage.currentProjects += amount;
    }
    
    return true;
}

bool TenantManager::checkQuota(const std::string& tenantId, const std::string& resourceType, size_t requestedAmount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenantId);
    if (it == m_tenants.end()) {
        return false;
    }
    
    const auto& quotas = it->second.quotas;
    const auto& usage = it->second.usage;
    
    if (resourceType == "api_calls") {
        return (usage.apiCallsToday + requestedAmount) <= quotas.maxApiCallsPerDay;
    } else if (resourceType == "model_requests") {
        return (usage.modelRequestsToday + requestedAmount) <= quotas.maxModelRequestsPerDay;
    } else if (resourceType == "agent_executions") {
        return (usage.agentExecutionsToday + requestedAmount) <= quotas.maxAgentExecutionsPerDay;
    } else if (resourceType == "storage") {
        return (usage.currentStorageBytes + requestedAmount) <= quotas.maxStorageBytes;
    } else if (resourceType == "connections") {
        return (usage.currentConnections + requestedAmount) <= quotas.maxConcurrentConnections;
    } else if (resourceType == "users") {
        return (usage.currentUsers + requestedAmount) <= quotas.maxUsers;
    } else if (resourceType == "projects") {
        return (usage.currentProjects + requestedAmount) <= quotas.maxProjects;
    }
    
    return true;  // Unknown resource type, allow
}

ResourceUsage TenantManager::getUsage(const std::string& tenantId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenantId);
    if (it == m_tenants.end()) {
        return ResourceUsage{};
    }
    
    return it->second.usage;
}

void TenantManager::resetDailyUsage(const std::string& tenantId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tenants.find(tenantId);
    if (it != m_tenants.end()) {
        auto& usage = it->second.usage;
        usage.apiCallsToday = 0;
        usage.modelRequestsToday = 0;
        usage.agentExecutionsToday = 0;
        usage.lastResetDate = std::chrono::system_clock::now();
    }
}

std::vector<Tenant> TenantManager::listTenants(TenantStatus status) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<Tenant> result;
    for (const auto& [_, tenant] : m_tenants) {
        if (tenant.status == status) {
            result.push_back(tenant);
        }
    }
    
    return result;
}

std::vector<Tenant> TenantManager::listTenantsByOwner(const std::string& ownerId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<Tenant> result;
    for (const auto& [_, tenant] : m_tenants) {
        if (tenant.ownerId == ownerId && tenant.status != TenantStatus::DELETED) {
            result.push_back(tenant);
        }
    }
    
    return result;
}

std::string TenantManager::generateTenantId() {
    uint64_t id = m_counter.fetch_add(1) + 1;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "tenant_" << std::hex << timestamp << "_" << id;
    return ss.str();
}

std::string TenantManager::generateSlug(const std::string& name) {
    std::string slug;
    slug.reserve(name.length());
    
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            slug += std::tolower(static_cast<unsigned char>(c));
        } else if (c == ' ' || c == '-' || c == '_') {
            if (!slug.empty() && slug.back() != '-') {
                slug += '-';
            }
        }
    }
    
    // Remove trailing dash
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    
    // Ensure uniqueness
    std::string baseSlug = slug;
    int counter = 1;
    while (m_slugToId.find(slug) != m_slugToId.end()) {
        slug = baseSlug + "-" + std::to_string(counter++);
    }
    
    return slug;
}

// =============================================================================
// OrganizationManager Implementation
// =============================================================================

OrganizationManager& OrganizationManager::instance() {
    static OrganizationManager instance;
    return instance;
}

std::string OrganizationManager::createOrganization(const std::string& tenantId, 
                                                      const std::string& name,
                                                      const std::string& creatorUserId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Organization org;
    org.id = generateOrgId();
    org.tenantId = tenantId;
    org.name = name;
    org.createdAt = std::chrono::system_clock::now();
    org.memberUserIds.push_back(creatorUserId);
    org.adminUserIds.push_back(creatorUserId);
    
    m_organizations[org.id] = org;
    
    return org.id;
}

std::optional<Organization> OrganizationManager::getOrganization(const std::string& orgId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_organizations.find(orgId);
    if (it == m_organizations.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

bool OrganizationManager::updateOrganization(const Organization& org) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_organizations.find(org.id);
    if (it == m_organizations.end()) {
        return false;
    }
    
    it->second = org;
    return true;
}

bool OrganizationManager::deleteOrganization(const std::string& orgId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_organizations.erase(orgId) > 0;
}

bool OrganizationManager::addMember(const std::string& orgId, const std::string& userId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_organizations.find(orgId);
    if (it == m_organizations.end()) {
        return false;
    }
    
    auto& members = it->second.memberUserIds;
    if (std::find(members.begin(), members.end(), userId) == members.end()) {
        members.push_back(userId);
    }
    
    return true;
}

bool OrganizationManager::removeMember(const std::string& orgId, const std::string& userId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_organizations.find(orgId);
    if (it == m_organizations.end()) {
        return false;
    }
    
    auto& members = it->second.memberUserIds;
    members.erase(std::remove(members.begin(), members.end(), userId), members.end());
    
    auto& admins = it->second.adminUserIds;
    admins.erase(std::remove(admins.begin(), admins.end(), userId), admins.end());
    
    return true;
}

bool OrganizationManager::promoteToAdmin(const std::string& orgId, const std::string& userId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_organizations.find(orgId);
    if (it == m_organizations.end()) {
        return false;
    }
    
    // Must be a member first
    auto& members = it->second.memberUserIds;
    if (std::find(members.begin(), members.end(), userId) == members.end()) {
        return false;
    }
    
    auto& admins = it->second.adminUserIds;
    if (std::find(admins.begin(), admins.end(), userId) == admins.end()) {
        admins.push_back(userId);
    }
    
    return true;
}

bool OrganizationManager::demoteFromAdmin(const std::string& orgId, const std::string& userId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_organizations.find(orgId);
    if (it == m_organizations.end()) {
        return false;
    }
    
    auto& admins = it->second.adminUserIds;
    admins.erase(std::remove(admins.begin(), admins.end(), userId), admins.end());
    
    return true;
}

std::vector<Organization> OrganizationManager::listOrganizations(const std::string& tenantId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<Organization> result;
    for (const auto& [_, org] : m_organizations) {
        if (org.tenantId == tenantId) {
            result.push_back(org);
        }
    }
    
    return result;
}

std::vector<Organization> OrganizationManager::listUserOrganizations(const std::string& userId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<Organization> result;
    for (const auto& [_, org] : m_organizations) {
        if (std::find(org.memberUserIds.begin(), org.memberUserIds.end(), userId) != org.memberUserIds.end()) {
            result.push_back(org);
        }
    }
    
    return result;
}

std::string OrganizationManager::generateOrgId() {
    uint64_t id = m_counter.fetch_add(1) + 1;
    
    std::stringstream ss;
    ss << "org_" << std::hex << id;
    return ss.str();
}

// =============================================================================
// NamespaceManager Implementation
// =============================================================================

NamespaceManager& NamespaceManager::instance() {
    static NamespaceManager instance;
    return instance;
}

std::string NamespaceManager::createNamespace(const std::string& tenantId, const std::string& name,
                                                bool isDefault) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Namespace ns;
    ns.id = generateNsId();
    ns.tenantId = tenantId;
    ns.name = name;
    ns.createdAt = std::chrono::system_clock::now();
    ns.isDefault = isDefault;
    
    // If this is default, unset other defaults
    if (isDefault) {
        for (auto& [_, existingNs] : m_namespaces) {
            if (existingNs.tenantId == tenantId && existingNs.isDefault) {
                existingNs.isDefault = false;
            }
        }
    }
    
    m_namespaces[ns.id] = ns;
    
    return ns.id;
}

std::optional<Namespace> NamespaceManager::getNamespace(const std::string& nsId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_namespaces.find(nsId);
    if (it == m_namespaces.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

std::optional<Namespace> NamespaceManager::getNamespaceByName(const std::string& tenantId, 
                                                                const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [_, ns] : m_namespaces) {
        if (ns.tenantId == tenantId && ns.name == name) {
            return ns;
        }
    }
    
    return std::nullopt;
}

bool NamespaceManager::updateNamespace(const Namespace& ns) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_namespaces.find(ns.id);
    if (it == m_namespaces.end()) {
        return false;
    }
    
    it->second = ns;
    return true;
}

bool NamespaceManager::deleteNamespace(const std::string& nsId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_namespaces.erase(nsId) > 0;
}

std::optional<Namespace> NamespaceManager::getDefaultNamespace(const std::string& tenantId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [_, ns] : m_namespaces) {
        if (ns.tenantId == tenantId && ns.isDefault) {
            return ns;
        }
    }
    
    return std::nullopt;
}

bool NamespaceManager::setDefaultNamespace(const std::string& tenantId, const std::string& nsId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_namespaces.find(nsId);
    if (it == m_namespaces.end() || it->second.tenantId != tenantId) {
        return false;
    }
    
    // Unset other defaults
    for (auto& [_, ns] : m_namespaces) {
        if (ns.tenantId == tenantId && ns.isDefault) {
            ns.isDefault = false;
        }
    }
    
    it->second.isDefault = true;
    return true;
}

bool NamespaceManager::addLabel(const std::string& nsId, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_namespaces.find(nsId);
    if (it == m_namespaces.end()) {
        return false;
    }
    
    it->second.labels[key] = value;
    return true;
}

bool NamespaceManager::removeLabel(const std::string& nsId, const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_namespaces.find(nsId);
    if (it == m_namespaces.end()) {
        return false;
    }
    
    return it->second.labels.erase(key) > 0;
}

std::vector<Namespace> NamespaceManager::findByLabel(const std::string& tenantId,
                                                       const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<Namespace> result;
    for (const auto& [_, ns] : m_namespaces) {
        if (ns.tenantId == tenantId) {
            auto labelIt = ns.labels.find(key);
            if (labelIt != ns.labels.end() && labelIt->second == value) {
                result.push_back(ns);
            }
        }
    }
    
    return result;
}

std::vector<Namespace> NamespaceManager::listNamespaces(const std::string& tenantId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<Namespace> result;
    for (const auto& [_, ns] : m_namespaces) {
        if (ns.tenantId == tenantId) {
            result.push_back(ns);
        }
    }
    
    return result;
}

std::string NamespaceManager::generateNsId() {
    uint64_t id = m_counter.fetch_add(1) + 1;
    
    std::stringstream ss;
    ss << "ns_" << std::hex << id;
    return ss.str();
}

// =============================================================================
// TenantMiddleware Implementation
// =============================================================================

bool TenantMiddleware::extractTenantFromHeader(const std::string& headerValue) {
    if (headerValue.empty()) {
        return false;
    }
    
    // Validate tenant exists
    auto tenant = TenantManager::instance().getTenant(headerValue);
    if (!tenant || tenant->status != TenantStatus::ACTIVE) {
        return false;
    }
    
    TenantContext::setCurrent(headerValue);
    return true;
}

bool TenantMiddleware::extractTenantFromSubdomain(const std::string& host) {
    // Extract first subdomain from host
    size_t dotPos = host.find('.');
    if (dotPos == std::string::npos || dotPos == 0) {
        return false;
    }
    
    std::string subdomain = host.substr(0, dotPos);
    
    // Look up tenant by slug
    auto tenant = TenantManager::instance().getTenantBySlug(subdomain);
    if (!tenant || tenant->status != TenantStatus::ACTIVE) {
        return false;
    }
    
    TenantContext::setCurrent(tenant->id);
    return true;
}

bool TenantMiddleware::extractTenantFromPath(const std::string& path) {
    // Expected format: /tenant/{tenantId}/...
    if (path.length() < 9 || path.substr(0, 8) != "/tenant/") {
        return false;
    }
    
    size_t endPos = path.find('/', 8);
    std::string tenantId = (endPos == std::string::npos) ? 
                           path.substr(8) : path.substr(8, endPos - 8);
    
    if (tenantId.empty()) {
        return false;
    }
    
    auto tenant = TenantManager::instance().getTenant(tenantId);
    if (!tenant || tenant->status != TenantStatus::ACTIVE) {
        return false;
    }
    
    TenantContext::setCurrent(tenantId);
    return true;
}

bool TenantMiddleware::validateTenantAccess(const std::string& tenantId, const std::string& userId) {
    // Basic validation - in real implementation, check user membership
    auto tenant = TenantManager::instance().getTenant(tenantId);
    if (!tenant || tenant->status != TenantStatus::ACTIVE) {
        return false;
    }
    
    // Check if user is owner
    if (tenant->ownerId == userId) {
        return true;
    }
    
    // Check organization membership
    auto orgs = OrganizationManager::instance().listUserOrganizations(userId);
    for (const auto& org : orgs) {
        if (org.tenantId == tenantId) {
            return true;
        }
    }
    
    return false;
}

void TenantMiddleware::apply(const std::string& tenantId, NextHandler next) {
    TenantContext::Scope scope(tenantId);
    next();
}

} // namespace enterprise
