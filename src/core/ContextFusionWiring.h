// ContextFusionWiring.h — Public API for ContextFusionEngine integration
// Call WireContextFusion() once during IDE initialization.

#pragma once

#include <string>

namespace RawrXD {

// Forward declarations
class Win32IDE_GhostText;
class IChatPanel;

// ─────────────────────────────────────────────────────────────────────────────
// Wiring API
// ─────────────────────────────────────────────────────────────────────────────

struct ContextFusionStats {
    bool isWired;
    uint64_t frameVersion;
    size_t subscriberCount;
    std::string activeSubscribers;
};

// Wire all IDE subsystems into ContextFusionEngine
// Call once during IDE initialization (after all subsystems are created)
bool WireContextFusion(
    Win32IDE_GhostText* ghostText,      // Can be nullptr if ghost text not enabled
    IChatPanel* chatPanel,              // Can be nullptr if chat not enabled
    const std::string& modelEndpoint   // Ollama endpoint, e.g. "http://localhost:11434"
);

// Unwire all subsystems (call during shutdown)
void UnwireContextFusion();

// Check if wiring is active
bool IsContextFusionWired();

// Get statistics for debugging
ContextFusionStats GetContextFusionStats();

} // namespace RawrXD