#pragma once
#include <string>
#include <vector>
#include "language_server_integration.hpp" 

// Definition for Diagnostic if not in language_server_integration.hpp
// Assuming Diagnostic structure exists there.

class IDEAgentBridgeHotPatchingIntegration {
public:
    static void onLSPDiagnostic(const std::string& uri, const std::vector<Diagnostic>& diags);
    
    // Likely needs a signal or callback to notify editor
    static void diagnosticsAvailable(const std::string& uri, const std::vector<Diagnostic>& diags) {
        // Placeholder for IDE integration (signal emission)
    }
};
