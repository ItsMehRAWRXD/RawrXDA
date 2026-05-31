#pragma once
/**
 * feature_connector.hpp - Drop-in connection for all 50 features
 *
 * Single-line initialization:
 *   FeatureConnector::connectAll(provider, editor, terminal);
 */

#include "ai_ide_features.hpp"
#include "features_code_completion.hpp"
#include "features_codebase_intelligence.hpp"
#include "features_agentic.hpp"
#include "features_chat.hpp"

namespace rawrxd {

//=============================================================================
// ONE-LINE CONNECTION FOR ALL 50 FEATURES
//=============================================================================

class FeatureConnector {
public:
    // Initialize all 50 features with single connection
    static void connectAll(
        std::shared_ptr<AIProvider> provider,
        EditorIntegration* editor = nullptr,
        TerminalInterface* terminal = nullptr) {

        auto& registry = FeatureRegistry::instance();

        // CODE COMPLETION (1-10)
        registry.registerFeature(FeatureRegistry::FeatureInfo{"autocomplete", "Real-Time Autocomplete", "Code Completion"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"multiline", "Multi-Line Completion", "Code Completion"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"tab-complete", "Tab Autocomplete", "Code Completion"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"context", "Context-Aware Completions", "Code Completion"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"nl2code", "Natural Language to Code", "Code Completion"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"snippet", "Snippet Generation", "Code Completion"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"autoimport", "Auto-Import", "Code Completion"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"params", "Parameter Completion", "Code Completion"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"error-fix", "Error Correction", "Code Completion"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"fast", "Fast Autocomplete", "Code Completion"});

        // CODEBASE INTELLIGENCE (11-18)
        registry.registerFeature(FeatureRegistry::FeatureInfo{"index", "Codebase Indexing", "Codebase Intelligence"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"codebase-ref", "@codebase References", "Codebase Intelligence"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"search", "Semantic Search", "Codebase Intelligence"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"cross-file", "Cross-File Context", "Codebase Intelligence"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"patterns", "Pattern Detection", "Codebase Intelligence"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"imports", "Import Mapping", "Codebase Intelligence"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"git-history", "Git History", "Codebase Intelligence"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"deps", "Dependency Graph", "Codebase Intelligence"});

        // AGENTIC (19-28)
        registry.registerFeature(FeatureRegistry::FeatureInfo{"multi-edit", "Multi-File Editing", "Agentic"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"composer", "Composer Mode", "Agentic"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"bg-agents", "Background Agents", "Agentic"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"parallel", "Parallel Agents", "Agentic"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"terminal", "Terminal Execution", "Agentic"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"test-iter", "Test Iteration", "Agentic"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"pr-create", "PR Creation", "Agentic"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"issue-impl", "Issue Implementation", "Agentic"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"plan-act", "Plan/Act Mode", "Agentic"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"boomerang", "Boomerang Tasks", "Agentic"});

        // CHAT & Q&A (29-35)
        registry.registerFeature(FeatureRegistry::FeatureInfo{"inline-chat", "Inline Chat", "Chat & Q&A"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"sidebar-chat", "Sidebar Chat", "Chat & Q&A"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"explain", "Explain Code", "Chat & Q&A"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"file-ref", "@ File References", "Chat & Q&A"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"docs", "@docs", "Chat & Q&A"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"web", "@web Search", "Chat & Q&A"});
        registry.registerFeature(FeatureRegistry::FeatureInfo{"image", "Image Understanding", "Chat & Q&A"});

        // Connect provider
        registry.connectAll(provider);

        // Connect editor
        if (editor) {
            registry.connectEditor(editor);
        }

        // Connect terminal
        if (terminal) {
            registry.connectTerminal(terminal);
        }
    }

    // Get feature instance by ID
    template<typename T>
    static std::shared_ptr<T> getFeature(const std::string& featureId);
};

//=============================================================================
// DROP-IN USAGE EXAMPLE
//=============================================================================

inline void example_usage() {
    // 1. Create AI provider (e.g., OpenAI, Claude, Local)
    // auto provider = std::make_shared<ClaudeProvider>("your-api-key");

    // 2. Create editor integration
    // EditorIntegration* editor = /* your editor */;
    // TerminalInterface* terminal = /* your terminal */;

    // 3. ONE LINE TO CONNECT ALL 50 FEATURES
    // FeatureConnector::connectAll(provider, editor, terminal);

    // Features are now ready!
}

} // namespace rawrxd
