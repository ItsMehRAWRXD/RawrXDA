// AsmMergeBridge.h - AI-powered merge conflict resolution bridge
#pragma once

#include "InferenceService.h"
#include "live_share.h"
#include <vector>

class AsmMergeBridge {
public:
    // Singleton – the bridge is cheap, but we want a single place to hold the HTTP endpoint.
    static AsmMergeBridge& instance();

    // Called by Live‑Share when a three‑way merge fails.
    // Returns true if the AI supplied a usable merge; the out parameter receives the edits.
    bool resolveConflict(const ConflictInfo& conflict,
                         std::vector<TextEdit>& outEdits,
                         std::string& outMessage,   // UI‑friendly message
                         double& outConfidence);    // AI confidence score

    // Configuration – you can call this at startup (e.g. read from a config file).
    void setEndpoint(const std::string& endpoint) { m_service = InferenceService(endpoint); }

private:
    AsmMergeBridge();                       // private ctor
    AsmMergeBridge(const AsmMergeBridge&) = delete;
    AsmMergeBridge& operator=(const AsmMergeBridge&) = delete;

    // Helper that builds the JSON payload the AI model expects.
    nlohmann::json buildPayload(const ConflictInfo& conflict) const;

    // Helper that translates the JSON response into TextEdits.
    bool parseResponse(const nlohmann::json& response,
                       std::vector<TextEdit>& edits,
                       std::string& message,
                       double& confidence) const;

    InferenceService m_service{
        "http://127.0.0.1:8000/v1/merge"}; // default – can be overridden
};
