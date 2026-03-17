#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

class Tenant {
public:
    std::string tenantId;
    std::string name;
    bool isActive;

    Tenant(const std::string& id, const std::string& tenantName)
        : tenantId(id), name(tenantName), isActive(true) {}
};

class TenantManager {
private:
    static TenantManager* s_instance;
    static std::mutex s_mutex;
    std::map<std::string, std::shared_ptr<Tenant>> m_tenants;
    std::string m_currentTenantId;

    TenantManager() = default;

public:
    static TenantManager& instance();

    void createTenant(const std::string& tenantId, const std::string& tenantName);
    std::shared_ptr<Tenant> getTenant(const std::string& tenantId);
    void setCurrentTenant(const std::string& tenantId);
    std::string getCurrentTenantId() const;
    std::vector<std::shared_ptr<Tenant>> getAllTenants() const;
    bool deactivateTenant(const std::string& tenantId);
};
