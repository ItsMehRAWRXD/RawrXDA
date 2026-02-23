// ============================================================================
// policy_engine.cpp — Custom Security Policy Engine
// ============================================================================
// Track C: Sovereign Tier Security Features
// Feature: CustomSecurityPolicies (Sovereign tier)
// Purpose: Organization-defined access control and data handling rules
// ============================================================================

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

// Stub license check for test mode
#ifdef BUILD_POLICY_TEST
#define LICENSE_CHECK(feature) true
#else
#include "../include/license_enforcement.h"
#define LICENSE_CHECK(feature) RawrXD::Enforce::LicenseEnforcer::Instance().allow(feature, __FUNCTION__)
#endif

namespace RawrXD::Sovereign {

// ============================================================================
// Security Policy Types
// ============================================================================

enum class PolicyAction {
    ALLOW,
    DENY,
    AUDIT,
    REDACT,
    ENCRYPT
};

enum class PolicySubject {
    USER,
    GROUP,
    ROLE,
    CLEARANCE_LEVEL
};

struct SecurityPolicy {
    std::string name;
    PolicySubject subject;
    std::string subjectValue;  // User ID, group name, or clearance level
    PolicyAction action;
    std::string resource;      // Model, feature, or data path
    std::string condition;     // Optional condition (e.g., "time > 9am && time < 5pm")
};

// ============================================================================
// Policy Engine
// ============================================================================

class PolicyEngine {
private:
    bool licensed;
    std::vector<SecurityPolicy> policies;
    std::unordered_map<std::string, std::string> userAttributes;

public:
    PolicyEngine() : licensed(false) {
        // License check (Sovereign tier required)
        licensed = LICENSE_CHECK(RawrXD::License::FeatureID::CustomSecurityPolicies);
        
        if (!licensed) {
            std::cerr << "[LICENSE] CustomSecurityPolicies requires Sovereign license\n";
            return;
        }
        
        std::cout << "[Policy] Security policy engine initialized\n";
    }

    // Add security policy
    bool addPolicy(const SecurityPolicy& policy) {
        if (!licensed) {
            return false;
        }

        policies.push_back(policy);
        std::cout << "[Policy] Added: " << policy.name 
                  << " (action=" << static_cast<int>(policy.action) << ")\n";
        return true;
    }

    // Evaluate policy for access decision
    PolicyAction evaluateAccess(const std::string& user, 
                                 const std::string& resource) {
        if (!licensed) {
            return PolicyAction::DENY;  // Deny by default if not licensed
        }

        std::cout << "[Policy] Evaluating access: user=" << user 
                  << ", resource=" << resource << "\n";

        // Default policy: deny all
        PolicyAction decision = PolicyAction::DENY;

        // Evaluate all matching policies
        for (const auto& policy : policies) {
            if (matchesPolicy(policy, user, resource)) {
                std::cout << "[Policy] Matched policy: " << policy.name << "\n";
                decision = policy.action;
                
                // First match wins (can be changed to most restrictive)
                break;
            }
        }

        std::cout << "[Policy] Decision: " 
                  << (decision == PolicyAction::ALLOW ? "ALLOW" :
                      decision == PolicyAction::DENY ? "DENY" :
                      decision == PolicyAction::AUDIT ? "AUDIT" :
                      decision == PolicyAction::REDACT ? "REDACT" : "ENCRYPT")
                  << "\n";

        return decision;
    }

    // Set user attributes (clearance level, groups, etc.)
    void setUserAttribute(const std::string& user, 
                          const std::string& attribute, 
                          const std::string& value) {
        if (!licensed) return;

        std::string key = user + ":" + attribute;
        userAttributes[key] = value;
        
        std::cout << "[Policy] Set " << user << "." << attribute 
                  << " = " << value << "\n";
    }

    // Get user attribute
    std::string getUserAttribute(const std::string& user, 
                                  const std::string& attribute) const {
        std::string key = user + ":" + attribute;
        auto it = userAttributes.find(key);
        return (it != userAttributes.end()) ? it->second : "";
    }

    // Load policy from file (JSON format supported)
    bool loadPolicyFile(const std::string& path) {
        if (!licensed) return false;

        std::cout << "[Policy] Loading policies from " << path << "...\n";
        
        // JSON format: {"policies":[{"name":"...","subject":"user|group|role|clearance_level",
        //   "value":"...","action":"allow|deny|audit|redact|encrypt","resource":"*|path"}]}
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cerr << "[Policy] Failed to open " << path << "\n";
            return false;
        }
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();

        // Simple JSON parse for policies array
        size_t arr = content.find("\"policies\"");
        if (arr != std::string::npos) arr = content.find('[', arr);
        if (arr == std::string::npos) {
            std::cout << "[Policy] No policies array in file\n";
            return true;
        }
        size_t pos = arr + 1;
        while (pos < content.size()) {
            size_t obj = content.find('{', pos);
            if (obj == std::string::npos) break;
            size_t end = content.find('}', obj);
            if (end == std::string::npos) break;
            std::string block = content.substr(obj, end - obj + 1);
            SecurityPolicy p;
            auto extract = [&block](const char* key) -> std::string {
                std::string k = std::string("\"") + key + "\":\"";
                size_t i = block.find(k);
                if (i == std::string::npos) return "";
                i += k.size();
                size_t j = i;
                while (j < block.size() && block[j] != '"') {
                    if (block[j] == '\\') j++;
                    j++;
                }
                std::string val;
                for (size_t kk = i; kk < j; kk++) {
                    if (block[kk] == '\\' && kk + 1 < j) { val += block[kk+1]; kk++; }
                    else val += block[kk];
                }
                return val;
            };
            p.name = extract("name");
            p.resource = extract("resource");
            if (p.resource.empty()) p.resource = "*";
            std::string subj = extract("subject");
            if (subj == "user") p.subject = PolicySubject::USER;
            else if (subj == "group") p.subject = PolicySubject::GROUP;
            else if (subj == "role") p.subject = PolicySubject::ROLE;
            else p.subject = PolicySubject::CLEARANCE_LEVEL;
            p.subjectValue = extract("value");
            std::string act = extract("action");
            if (act == "allow") p.action = PolicyAction::ALLOW;
            else if (act == "deny") p.action = PolicyAction::DENY;
            else if (act == "audit") p.action = PolicyAction::AUDIT;
            else if (act == "redact") p.action = PolicyAction::REDACT;
            else if (act == "encrypt") p.action = PolicyAction::ENCRYPT;
            else p.action = PolicyAction::DENY;
            if (!p.name.empty()) { policies.push_back(p); }
            pos = end + 1;
        }
        std::cout << "[Policy] Loaded " << policies.size() << " policies\n";
        return true;
    }

    // Audit log for policy violations
    void auditLog(const std::string& event, const std::string& user, 
                  const std::string& resource) {
        if (!licensed) return;

        std::cout << "[AUDIT] " << event 
                  << " | user=" << user 
                  << " | resource=" << resource 
                  << " | timestamp=" << std::time(nullptr) << "\n";
    }

    // Get policy status
    void printStatus() const {
        std::cout << "\n[Policy] Engine Status:\n";
        std::cout << "  Licensed: " << (licensed ? "YES" : "NO") << "\n";
        std::cout << "  Active policies: " << policies.size() << "\n";
        std::cout << "  User attributes: " << userAttributes.size() << "\n";
        
        if (!policies.empty()) {
            std::cout << "\nPolicies:\n";
            for (const auto& p : policies) {
                std::cout << "  - " << p.name << " (" << p.resource << ")\n";
            }
        }
    }

private:
    // Match resource with wildcards: *, prefix*, *suffix, prefix*suffix
    bool matchesResource(const SecurityPolicy& policy, const std::string& resource) const {
        if (policy.resource == "*") return true;
        if (policy.resource == resource) return true;
        size_t star = policy.resource.find('*');
        if (star == std::string::npos) {
            return resource.find(policy.resource) != std::string::npos;
        }
        std::string prefix = policy.resource.substr(0, star);
        std::string suffix = (star + 1 < policy.resource.size()) ? policy.resource.substr(star + 1) : "";
        if (!prefix.empty() && resource.compare(0, prefix.size(), prefix) != 0) return false;
        if (!suffix.empty() && (resource.size() < suffix.size() || resource.compare(resource.size() - suffix.size(), suffix.size(), suffix) != 0)) return false;
        return true;
    }

    // Match subject (user, clearance, group, role)
    bool matchesSubject(const SecurityPolicy& policy, const std::string& user) const {
        switch (policy.subject) {
            case PolicySubject::USER:
                return (user == policy.subjectValue);
            case PolicySubject::CLEARANCE_LEVEL: {
                std::string clearance = getUserAttribute(user, "clearance");
                return (clearance == policy.subjectValue);
            }
            case PolicySubject::GROUP: {
                std::string groups = getUserAttribute(user, "groups");
                return (groups.find(policy.subjectValue) != std::string::npos);
            }
            case PolicySubject::ROLE: {
                std::string role = getUserAttribute(user, "role");
                return (role == policy.subjectValue);
            }
        }
        return false;
    }

    // Check if policy matches user and resource (both must match)
    bool matchesPolicy(const SecurityPolicy& policy,
                       const std::string& user,
                       const std::string& resource) const {
        if (!matchesResource(policy, resource)) return false;
        if (!matchesSubject(policy, user)) return false;
        return true;
    }
};

} // namespace RawrXD::Sovereign

// ============================================================================
// Test Entry Point
// ============================================================================

#ifdef BUILD_POLICY_TEST
int main() {
    std::cout << "RawrXD Custom Security Policy Engine Test\n";
    std::cout << "Track C: Sovereign Security Feature\n\n";

    RawrXD::Sovereign::PolicyEngine engine;

    // Set user attributes
    engine.setUserAttribute("alice", "clearance", "TOP_SECRET");
    engine.setUserAttribute("bob", "clearance", "SECRET");
    engine.setUserAttribute("charlie", "clearance", "UNCLASSIFIED");

    // Add policies
    RawrXD::Sovereign::SecurityPolicy policy1;
    policy1.name = "Allow Top Secret Users";
    policy1.subject = RawrXD::Sovereign::PolicySubject::CLEARANCE_LEVEL;
    policy1.subjectValue = "TOP_SECRET";
    policy1.action = RawrXD::Sovereign::PolicyAction::ALLOW;
    policy1.resource = "*";
    engine.addPolicy(policy1);

    RawrXD::Sovereign::SecurityPolicy policy2;
    policy2.name = "Deny Unclassified Users";
    policy2.subject = RawrXD::Sovereign::PolicySubject::CLEARANCE_LEVEL;
    policy2.subjectValue = "UNCLASSIFIED";
    policy2.action = RawrXD::Sovereign::PolicyAction::DENY;
    policy2.resource = "classified/model";
    engine.addPolicy(policy2);

    // Test access decisions
    auto decision1 = engine.evaluateAccess("alice", "classified/model");
    auto decision2 = engine.evaluateAccess("bob", "classified/model");
    auto decision3 = engine.evaluateAccess("charlie", "classified/model");

    engine.printStatus();

    std::cout << "\n[SUCCESS] Custom security policies operational\n";
    return 0;
}
#endif
