// ide_agent_bridge_hot_patching_integration_lsp.cpp
// Connects LSP diagnostics → Hot-patcher → Editor feedback

#include "ide_agent_bridge_hot_patching_integration.hpp"
#include "language_server_integration.hpp"
#include "agent_hot_patcher.hpp"

void IDEAgentBridgeHotPatchingIntegration::onLSPDiagnostic(const QString& uri, 
                                                           const QVector<Diagnostic>& diags) {
    for(const auto& diag : diags) {
        // Check if this is an AI-suggested auto-fix
        if(diag.source == "rawrxd-ai" && !diag.relatedInformation.isEmpty()) {
            // Pre-stage the hotpatch
            HotPatchCandidate candidate{
                .uri = uri,
                .range = diag.range,
                .replacement = diag.relatedInformation.first().edit.changes.value(uri).first().newText,
                .confidence = 0.95f,
                .autoApply = (diag.severity <= 2) // Auto-fix errors/warnings
            };
            
            AgentHotPatcher::instance()->stagePatch(candidate);
        }
    }
    
    // Forward to editor problems panel
    emit diagnosticsAvailable(uri, diags);
}
