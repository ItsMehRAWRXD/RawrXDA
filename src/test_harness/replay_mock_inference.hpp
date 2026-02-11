// ============================================================================
// replay_mock_inference.hpp — Deterministic Mock LLM for Replay
// ============================================================================
//
// During replay, this mock replaces the live LLM. It feeds back the exact
// responses recorded in the journal at the matching sequence position.
//
// This is the critical piece: everything else (failure detector, puppeteer,
// hotpatch routing, verification) runs live. Only inference is mocked.
// That makes this a full-pipeline test, not a unit test.
//
// Determinism enforcement:
//   - Prompts are canonicalized before comparison (strip timestamps, UUIDs,
//     absolute paths, normalize line endings, canonicalize JSON)
//   - If canonicalized prompt diverges from recorded → determinism violation
//   - Sequence deviations are counted and reported to the Oracle
//
// Pattern:  Structured results, no exceptions
// Threading: All public methods are thread-safe
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>

// ============================================================================
// RecordedInference — Single recorded model call from journal
// ============================================================================
struct RecordedInference {
    uint64_t    sequenceId;
    std::string canonicalPrompt;    // Canonicalized at recording time
    std::string response;           // Exact model response
    float       confidence;
    double      durationMs;         // Original inference duration
    std::string agentId;
    std::string metadata;           // JSON-encoded extra data
};

// ============================================================================
// MockInferenceResult — Return type for mock generation
// ============================================================================
struct MockInferenceResult {
    bool        found;              // True if a matching record was found
    std::string response;
    float       confidence;
    bool        promptAligned;      // True if canonicalized prompt matched
    std::string mismatchDetail;     // Non-empty if prompt diverged
    uint64_t    matchedSequenceId;
};

// ============================================================================
// PromptCanonicalizer — Strips non-deterministic content from prompts
// ============================================================================
class PromptCanonicalizer {
public:
    // Full canonicalization pipeline:
    //   1. Normalize line endings (\r\n → \n)
    //   2. Strip ISO 8601 timestamps
    //   3. Strip UUIDs (8-4-4-4-12 hex pattern)
    //   4. Strip absolute Windows paths (C:\..., D:\...)
    //   5. Strip absolute Unix paths (/home/..., /tmp/...)
    //   6. Normalize JSON whitespace
    //   7. Collapse multiple whitespace to single space
    //   8. Trim leading/trailing whitespace
    static std::string canonicalize(const std::string& prompt);

    // Individual canonicalization steps (composable)
    static std::string stripTimestamps(const std::string& s);
    static std::string stripUUIDs(const std::string& s);
    static std::string stripAbsolutePaths(const std::string& s);
    static std::string normalizeLineEndings(const std::string& s);
    static std::string normalizeWhitespace(const std::string& s);
};

// ============================================================================
// ReplayMockInference — The mock LLM backend
// ============================================================================
class ReplayMockInference {
public:
    ReplayMockInference() = default;
    ~ReplayMockInference() = default;

    // ── Loading ────────────────────────────────────────────────────
    // Load recorded inferences from a journal.replay file.
    // Filters to ModelInference + AgentResponse action types only.
    // Returns false if file cannot be read or is empty.
    bool loadJournal(const std::string& replayPath);

    // Load from pre-built records (for testing the mock itself)
    void loadRecords(const std::vector<RecordedInference>& records);

    // ── Generation ─────────────────────────────────────────────────
    // Returns the next recorded response in sequence order.
    // Canonicalizes the prompt and compares against the recorded one.
    // If prompt diverges → still returns response but flags mismatch.
    MockInferenceResult generate(const std::string& prompt);

    // Returns by explicit sequence ID (for random-access replay)
    MockInferenceResult generateBySequenceId(uint64_t sequenceId,
                                              const std::string& prompt);

    // ── State ──────────────────────────────────────────────────────
    size_t totalRecorded() const;
    size_t currentPosition() const;
    size_t sequenceDeviations() const;
    bool   isExhausted() const;

    // Reset position to beginning (for re-runs)
    void reset();

private:
    std::vector<RecordedInference> m_records;
    std::atomic<size_t> m_position{0};
    std::atomic<size_t> m_deviations{0};
    mutable std::mutex m_mutex;

    // Exact string equality after canonicalization
    bool promptsMatch(const std::string& recorded, const std::string& actual) const;
};
