// ============================================================================
// LSPCompletionProvider_SkillIntegration.cpp — Skill injection for LSP
// ============================================================================
// Provides skill context injection for all LSP completion and intelligence
// requests. Ensures language server operations carry quality gate context.
//
// USAGE:
//   Include in ai_completion_provider.cpp or compile as separate unit.
// ============================================================================

#include "../skill_system/SkillInjectionHooks.h"
#include <string>

namespace RawrXD {
namespace LSP {

// ============================================================================
// LSP COMPLETION SKILL INTEGRATION
// ============================================================================

// Called before EVERY LSP completion request
std::string EnrichLSPCompletionWithSkills(
    const std::string& originalPrompt,
    const std::string& languageId,
    const std::string& triggerCharacter
) {
    return SkillSystem::Hook_LSP_CompletionRequest(
        originalPrompt,
        languageId,
        triggerCharacter
    );
}

// Called before symbol search operations
std::string EnrichSymbolSearchWithSkills(
    const std::string& query,
    const std::string& workspaceRoot
) {
    std::string enriched = SkillSystem::Hook_LSP_CompletionRequest(
        query,
        "*",  // All languages
        ""    // No trigger character
    );
    
    enriched += "\\n\\n# Symbol Search Context\\n";
    enriched += "# Workspace: " + workspaceRoot + "\\n";
    enriched += "# Search Type: global_symbol\\n";
    
    return enriched;
}

// Called before rename operations
std::string EnrichRenameWithSkills(
    const std::string& originalRequest,
    const std::string& symbolName,
    const std::string& filePath
) {
    std::string enriched = SkillSystem::Hook_LSP_CompletionRequest(
        originalRequest,
        "*",
        ""
    );
    
    enriched += "\\n\\n# Rename Operation Context\\n";
    enriched += "# Symbol: " + symbolName + "\\n";
    enriched += "# File: " + filePath + "\\n";
    enriched += "# Operation: cross_file_rename\\n";
    
    return enriched;
}

// ============================================================================
// C-API for backward compatibility
// ============================================================================
extern "C" {
    __declspec(dllexport) const char* __stdcall LSP_InjectSkillContext(
        const char* prompt,
        const char* language,
        const char* trigger
    ) {
        static std::string result;
        result = EnrichLSPCompletionWithSkills(
            prompt ? prompt : "",
            language ? language : "",
            trigger ? trigger : ""
        );
        return result.c_str();
    }
}

} // namespace LSP
} // namespace RawrXD
