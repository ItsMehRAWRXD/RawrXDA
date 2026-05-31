// AsmMergeBridge.cpp - Implementation of AI-powered merge bridge
#include "AsmMergeBridge.h"
#include "../include/collaboration/live_share.h"
#include <stdexcept>
#include <iostream>

using namespace RawrXD::Collaboration;

AsmMergeBridge::AsmMergeBridge() = default;

AsmMergeBridge& AsmMergeBridge::instance()
{
    static AsmMergeBridge s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
// Public API – called from live_share.cpp
// ---------------------------------------------------------------------------
bool AsmMergeBridge::resolveConflict(const ConflictInfo& conflict,
                                     std::vector<TextEdit>& outEdits,
                                     std::string& outMessage,
                                     double& outConfidence)
{
    try {
        // 1️⃣  Build request JSON
        nlohmann::json payload = buildPayload(conflict);

        // 2️⃣  Send to inference server (synchronous, but timeout is short)
        nlohmann::json response = m_service.requestResolution(payload);

        // 3️⃣  Parse the AI answer – if it contains a “mergedText” we turn it into edits.
        if (!parseResponse(response, outEdits, outMessage, outConfidence)) {
            // Response didn’t contain a usable merge.  Let Live‑Share fall back to its own resolver.
            return false;
        }
        
        // 4️⃣  Confidence threshold enforcement
        if (outConfidence < 0.85) {
            outMessage = "AI confidence low (" + std::to_string(outConfidence).substr(0, 4) + "). Review manually.";
            return false;
        }
        
        return true;
    }
    catch (const std::exception& ex) {
        // We deliberately **never crash** the host because AI can be flaky.
        outMessage = std::string("AI‑merge failed: ") + ex.what();
        std::cerr << "[AsmMergeBridge] " << outMessage << std::endl;
        return false;
    }
}

// ---------------------------------------------------------------------------
// JSON payload construction (real – matches the spec of the Sovereign model)
// ---------------------------------------------------------------------------
nlohmann::json AsmMergeBridge::buildPayload(const ConflictInfo& conflict) const
{
    // The model we ship with Sovereign expects a payload shaped like:
    // {
    //   "task": "merge",
    //   "priority": "high",
    //   "file_path": "...",
    //   "base": "<base_content>",
    //   "local": "<local_content>",
    //   "remote": "<remote_content>"
    // }
    // (All strings are UTF‑8 encoded – the engine guarantees that.)
    nlohmann::json j;
    j["task"] = "merge";
    j["priority"] = "high";
    j["file_path"] = conflict.filePath;
    j["base"] = conflict.baseContent;
    j["local"] = conflict.localContent;
    j["remote"] = conflict.remoteContent;
    return j;
}

// ---------------------------------------------------------------------------
// Parse AI response
// ---------------------------------------------------------------------------
bool AsmMergeBridge::parseResponse(const nlohmann::json& response,
                                   std::vector<TextEdit>& edits,
                                   std::string& message,
                                   double& confidence) const
{
    confidence = 1.0; // Default confidence
    
    // Expected response patterns (all three are accepted):
    //   1. {"merged_text": "..."}                     // Full file text
    //   2. {"edits": [{ "start": N, "len": M, "text": "..." }, ...]}
    //   3. {"error": "<human readable reason>"}
    //
    // Anything else → false.

    if (response.contains("error")) {
        message = "AI‑merge error: " + response["error"].get<std::string>();
        return false;
    }

    // Extract confidence if present
    if (response.contains("confidence")) {
        confidence = response["confidence"].get<double>();
    }

    if (response.contains("merged_text")) {
        std::string merged = response["merged_text"].get<std::string>();
        // Compute a diff against the local content – we use a simple line‑based diff to turn it
        // into a minimal set of edits.  A full diff library (myers‑diff) is already linked for
        // the existing three‑way merge, so we re‑use it.
        // For brevity the diff algorithm is not reproduced; we call a helper.
        // ---------------------------------------------------------------
        // Helper (already in the repo, used by live_share): computeEdits(base, merged)
        // ---------------------------------------------------------------
        // The helper returns a vector<TextEdit>.
        edits = computeEdits(conflict.localContent, merged);
        message = "AI‑suggested a full‑file merge.";
        return true;
    }

    if (response.contains("edits")) {
        const auto& jEdits = response["edits"];
        if (!jEdits.is_array()) return false;

        for (const auto& je : jEdits) {
            if (!je.contains("start") || !je.contains("len") || !je.contains("text"))
                return false;
            TextEdit te;
            te.start = je["start"].get<size_t>();
            te.length = je["len"].get<size_t>();
            te.replacement = je["text"].get<std::string>();
            edits.push_back(std::move(te));
        }
        message = "AI‑suggested a set of edits.";
        return true;
    }

    // Unknown format.
    message = "AI‑merge returned an unexpected format.";
    return false;
}
