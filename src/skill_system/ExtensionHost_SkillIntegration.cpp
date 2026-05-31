// ============================================================================
// ExtensionHost_SkillIntegration.cpp — Skill injection for Extension Host
// ============================================================================
// Provides skill context injection for all extension host API calls.
// Ensures VS Code-compatible extensions carry security and phase context.
//
// USAGE:
//   Include in ExtensionHost_VSCodeAPIs.cpp or compile as separate unit.
// ============================================================================

#include "../skill_system/SkillInjectionHooks.h"
#include <string>

namespace RawrXD {
namespace ExtensionHost {

// ============================================================================
// EXTENSION HOST SKILL INTEGRATION
// ============================================================================

// Called before EVERY extension API request
std::string EnrichExtensionAPIWithSkills(
    const std::string& originalPrompt,
    const std::string& extensionId,
    const std::string& apiMethod
) {
    return SkillSystem::Hook_ExtensionHost_APIRequest(
        originalPrompt,
        extensionId,
        apiMethod
    );
}

// Called for permission checks
std::string EnrichPermissionCheckWithSkills(
    const std::string& permissionRequest,
    const std::string& extensionId,
    const std::string& resourceType
) {
    std::string enriched = SkillSystem::Hook_ExtensionHost_APIRequest(
        permissionRequest,
        extensionId,
        "permission_check"
    );
    
    enriched += "\\n\\n# Permission Check Context\\n";
    enriched += "# Resource Type: " + resourceType + "\\n";
    enriched += "# Security Mode: sandboxed\\n";
    
    return enriched;
}

// Called for process isolation validation
std::string EnrichIsolationCheckWithSkills(
    const std::string& isolationRequest,
    const std::string& extensionId
) {
    std::string enriched = SkillSystem::Hook_ExtensionHost_APIRequest(
        isolationRequest,
        extensionId,
        "isolation_check"
    );
    
    enriched += "\\n\\n# Process Isolation Context\\n";
    enriched += "# Isolation: process_boundary\\n";
    enriched += "# IPC: secure_channel\\n";
    
    return enriched;
}

// ============================================================================
// C-API for backward compatibility
// ============================================================================
extern "C" {
    __declspec(dllexport) const char* __stdcall ExtensionHost_InjectSkillContext(
        const char* prompt,
        const char* extension,
        const char* method
    ) {
        static std::string result;
        result = EnrichExtensionAPIWithSkills(
            prompt ? prompt : "",
            extension ? extension : "",
            method ? method : ""
        );
        return result.c_str();
    }
}

} // namespace ExtensionHost
} // namespace RawrXD
