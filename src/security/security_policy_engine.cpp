// ============================================================================
// security_policy_engine.cpp — Custom Security Policies (Sovereign)
// ============================================================================
// Feature: CustomSecurityPolicies. Organization-specific security rules.
// ============================================================================

#include <string>
#include <unordered_map>
#include <mutex>

namespace RawrXD::Sovereign {

static std::mutex s_mutex;
static std::unordered_map<std::string, std::string> s_policies;

void SetSecurityPolicy(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_policies[key] = value;
}

bool GetSecurityPolicy(const std::string& key, std::string* out) {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_policies.find(key);
    if (it == s_policies.end()) return false;
    if (out) *out = it->second;
    return true;
}

} // namespace RawrXD::Sovereign
