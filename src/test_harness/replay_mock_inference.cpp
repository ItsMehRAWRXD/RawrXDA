// ============================================================================
// replay_mock_inference.cpp — Deterministic Mock LLM Implementation
// ============================================================================
//
// Loads recorded inferences from journal.replay, feeds them back in
// sequence order. Canonicalizes prompts to strip non-deterministic
// content before comparison. Counts deviations for the Oracle.
//
// Pattern:  Structured results, no exceptions
// Threading: All public methods are thread-safe
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "replay_mock_inference.hpp"
#include <fstream>
#include <algorithm>
#include <regex>
#include <sstream>
#include <cctype>
#include <cstring>

// ============================================================================
// PromptCanonicalizer — Strip non-deterministic content
// ============================================================================

std::string PromptCanonicalizer::canonicalize(const std::string& prompt) {
    std::string s = prompt;
    s = normalizeLineEndings(s);
    s = stripTimestamps(s);
    s = stripUUIDs(s);
    s = stripAbsolutePaths(s);
    s = normalizeWhitespace(s);
    return s;
}

std::string PromptCanonicalizer::normalizeLineEndings(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\r') {
            result += '\n';
            if (i + 1 < s.size() && s[i + 1] == '\n') {
                i++; // skip \n after \r
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

std::string PromptCanonicalizer::stripTimestamps(const std::string& s) {
    // ISO 8601: 2026-02-11T14:30:00, 2026-02-11 14:30:00, etc.
    static const std::regex iso8601(
        R"(\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}(\.\d+)?([Zz]|[+-]\d{2}:?\d{2})?)",
        std::regex::optimize);

    // Unix epoch timestamps (10+ digits)
    static const std::regex epoch(R"(\b\d{10,13}\b)", std::regex::optimize);

    std::string result = std::regex_replace(s, iso8601, "<TIMESTAMP>");
    result = std::regex_replace(result, epoch, "<EPOCH>");
    return result;
}

std::string PromptCanonicalizer::stripUUIDs(const std::string& s) {
    // Standard UUID: 8-4-4-4-12 hex
    static const std::regex uuid(
        R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})",
        std::regex::optimize);

    return std::regex_replace(s, uuid, "<UUID>");
}

std::string PromptCanonicalizer::stripAbsolutePaths(const std::string& s) {
    // Windows paths: C:\..., D:\...
    static const std::regex winPath(
        R"([A-Za-z]:\\(?:[^\s\\/:*?"<>|]+\\)*[^\s\\/:*?"<>|]*)",
        std::regex::optimize);

    // Unix absolute paths: /home/..., /tmp/..., /var/...
    static const std::regex unixPath(
        R"(/(?:home|tmp|var|usr|opt|etc|mnt|proc|sys)/[^\s]*)",
        std::regex::optimize);

    std::string result = std::regex_replace(s, winPath, "<PATH>");
    result = std::regex_replace(result, unixPath, "<PATH>");
    return result;
}

std::string PromptCanonicalizer::normalizeWhitespace(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    bool lastWasSpace = false;

    for (char c : s) {
        if (c == ' ' || c == '\t') {
            if (!lastWasSpace) {
                result += ' ';
                lastWasSpace = true;
            }
        } else {
            result += c;
            lastWasSpace = false;
        }
    }

    // Trim
    size_t start = result.find_first_not_of(" \n");
    if (start == std::string::npos) return "";
    size_t end = result.find_last_not_of(" \n");
    return result.substr(start, end - start + 1);
}

// ============================================================================
// ReplayMockInference — Loading
// ============================================================================

bool ReplayMockInference::loadJournal(const std::string& replayPath) {
    std::ifstream f(replayPath, std::ios::binary);
    if (!f.is_open()) return false;

    m_records.clear();
    m_position.store(0);
    m_deviations.store(0);

    // Journal format: line-delimited JSON
    // Each line: {"seq": N, "type": "ModelInference", "input": "...", "output": "...",
    //             "confidence": 0.95, "durationMs": 123.4, "agentId": "primary", "meta": "..."}
    //
    // We only load records with type == "ModelInference" or type == "AgentResponse"
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue; // skip comments/empty

        // Minimal JSON-like parsing without pulling in full json.hpp dependency
        // (the mock must be lightweight — full json is used in fixture.cpp)
        //
        // For production robustness we parse the key fields manually:
        RecordedInference rec;

        auto extractStr = [&line](const std::string& key) -> std::string {
            std::string needle = "\"" + key + "\":\"";
            auto pos = line.find(needle);
            if (pos == std::string::npos) {
                // Try without quotes for non-string values
                needle = "\"" + key + "\":";
                pos = line.find(needle);
                if (pos == std::string::npos) return "";
                pos += needle.size();
                auto end = line.find_first_of(",}", pos);
                if (end == std::string::npos) return "";
                return line.substr(pos, end - pos);
            }
            pos += needle.size();
            // Find closing quote (handling escaped quotes)
            std::string result;
            for (size_t i = pos; i < line.size(); i++) {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    result += line[i + 1];
                    i++;
                } else if (line[i] == '"') {
                    break;
                } else {
                    result += line[i];
                }
            }
            return result;
        };

        std::string typeStr = extractStr("type");
        // Filter to inference-related records only
        if (typeStr != "ModelInference" && typeStr != "AgentResponse" &&
            typeStr != "20" && typeStr != "1") {
            continue;
        }

        std::string seqStr = extractStr("seq");
        if (!seqStr.empty()) {
            rec.sequenceId = std::stoull(seqStr);
        }

        rec.response = extractStr("output");
        if (rec.response.empty()) continue; // Skip records without output

        std::string promptRaw = extractStr("input");
        rec.canonicalPrompt = PromptCanonicalizer::canonicalize(promptRaw);

        std::string confStr = extractStr("confidence");
        if (!confStr.empty()) {
            rec.confidence = std::stof(confStr);
        } else {
            rec.confidence = 1.0f;
        }

        std::string durStr = extractStr("durationMs");
        if (!durStr.empty()) {
            rec.durationMs = std::stod(durStr);
        }

        rec.agentId  = extractStr("agentId");
        rec.metadata = extractStr("meta");

        m_records.push_back(std::move(rec));
    }

    // Sort by sequence ID for deterministic replay order
    std::sort(m_records.begin(), m_records.end(),
              [](const RecordedInference& a, const RecordedInference& b) {
                  return a.sequenceId < b.sequenceId;
              });

    return !m_records.empty();
}

void ReplayMockInference::loadRecords(const std::vector<RecordedInference>& records) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_records = records;
    m_position.store(0);
    m_deviations.store(0);
}

// ============================================================================
// ReplayMockInference — Generation (the core operation)
// ============================================================================

MockInferenceResult ReplayMockInference::generate(const std::string& prompt) {
    std::lock_guard<std::mutex> lock(m_mutex);

    MockInferenceResult result;

    size_t pos = m_position.load();
    if (pos >= m_records.size()) {
        result.found = false;
        result.promptAligned = false;
        result.mismatchDetail = "Mock exhausted: position " +
            std::to_string(pos) + " >= " + std::to_string(m_records.size()) +
            " recorded responses";
        return result;
    }

    const RecordedInference& rec = m_records[pos];

    // Canonicalize incoming prompt
    std::string canonicalPrompt = PromptCanonicalizer::canonicalize(prompt);

    // Check prompt alignment
    result.found             = true;
    result.response          = rec.response;
    result.confidence        = rec.confidence;
    result.matchedSequenceId = rec.sequenceId;
    result.promptAligned     = promptsMatch(rec.canonicalPrompt, canonicalPrompt);

    if (!result.promptAligned) {
        m_deviations.fetch_add(1);

        // Build diagnostic: show first divergence point
        size_t minLen = std::min(rec.canonicalPrompt.size(), canonicalPrompt.size());
        size_t divergeAt = minLen;
        for (size_t i = 0; i < minLen; i++) {
            if (rec.canonicalPrompt[i] != canonicalPrompt[i]) {
                divergeAt = i;
                break;
            }
        }

        std::ostringstream oss;
        oss << "Prompt diverged at seq " << rec.sequenceId
            << ", offset " << divergeAt
            << " (recorded len=" << rec.canonicalPrompt.size()
            << ", actual len=" << canonicalPrompt.size() << ")";
        result.mismatchDetail = oss.str();
    }

    m_position.fetch_add(1);
    return result;
}

MockInferenceResult ReplayMockInference::generateBySequenceId(
    uint64_t sequenceId, const std::string& prompt) {
    std::lock_guard<std::mutex> lock(m_mutex);

    MockInferenceResult result;

    // Binary search by sequence ID
    auto it = std::lower_bound(m_records.begin(), m_records.end(), sequenceId,
        [](const RecordedInference& rec, uint64_t seq) {
            return rec.sequenceId < seq;
        });

    if (it == m_records.end() || it->sequenceId != sequenceId) {
        result.found = false;
        result.promptAligned = false;
        result.mismatchDetail = "No record found for sequenceId " +
            std::to_string(sequenceId);
        return result;
    }

    std::string canonicalPrompt = PromptCanonicalizer::canonicalize(prompt);

    result.found             = true;
    result.response          = it->response;
    result.confidence        = it->confidence;
    result.matchedSequenceId = it->sequenceId;
    result.promptAligned     = promptsMatch(it->canonicalPrompt, canonicalPrompt);

    if (!result.promptAligned) {
        m_deviations.fetch_add(1);
        result.mismatchDetail = "Prompt diverged at explicit seq " +
            std::to_string(sequenceId);
    }

    return result;
}

// ============================================================================
// ReplayMockInference — State queries
// ============================================================================

size_t ReplayMockInference::totalRecorded() const {
    return m_records.size();
}

size_t ReplayMockInference::currentPosition() const {
    return m_position.load();
}

size_t ReplayMockInference::sequenceDeviations() const {
    return m_deviations.load();
}

bool ReplayMockInference::isExhausted() const {
    return m_position.load() >= m_records.size();
}

void ReplayMockInference::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_position.store(0);
    m_deviations.store(0);
}

// ============================================================================
// Internal — Prompt comparison
// ============================================================================

bool ReplayMockInference::promptsMatch(const std::string& recorded,
                                        const std::string& actual) const {
    // Exact match after canonicalization — no fuzzy matching.
    // Fuzzy matching is a last resort that hides real determinism bugs.
    return recorded == actual;
}
