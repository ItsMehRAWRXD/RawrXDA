// proxy_hotpatcher.cpp — Proxy Hotpatcher Implementation
// Byte-level output rewriting, token bias injection, stream termination.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "proxy_hotpatcher.hpp"
#include "license_enforcement.h"
#include <cctype>
#include <cstring>
#include <algorithm>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
ProxyHotpatcher& ProxyHotpatcher::instance() {
    static ProxyHotpatcher inst;
    return inst;
}

ProxyHotpatcher::ProxyHotpatcher() = default;
ProxyHotpatcher::~ProxyHotpatcher() = default;

// ---------------------------------------------------------------------------
// Token Bias Injection
// ---------------------------------------------------------------------------
PatchResult ProxyHotpatcher::add_token_bias(const TokenBias& bias) {
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
        RawrXD::License::FeatureID::ProxyHotpatching, __FUNCTION__))
        return PatchResult::error("[LICENSE] Proxy hotpatching requires Enterprise license", -1);

    std::lock_guard<std::mutex> lock(m_mutex);
    // Check for duplicate tokenId — update if exists
    for (auto& b : m_biases) {
        if (b.tokenId == bias.tokenId) {
            b.biasValue = bias.biasValue;
            b.permanent = bias.permanent;
            return PatchResult::ok("Token bias updated");
        }
    }
    m_biases.push_back(bias);
    return PatchResult::ok("Token bias added");
}

PatchResult ProxyHotpatcher::remove_token_bias(uint32_t tokenId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_biases.begin(); it != m_biases.end(); ++it) {
        if (it->tokenId == tokenId) {
            m_biases.erase(it);
            return PatchResult::ok("Token bias removed");
        }
    }
    return PatchResult::error("Token bias not found", 1);
}

PatchResult ProxyHotpatcher::clear_token_biases() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_biases.clear();
    return PatchResult::ok("All token biases cleared");
}

size_t ProxyHotpatcher::apply_token_biases(float* logits, size_t vocabSize) {
    if (!logits || vocabSize == 0) return 0;

    std::lock_guard<std::mutex> lock(m_mutex);
    size_t applied = 0;
    auto it = m_biases.begin();
    while (it != m_biases.end()) {
        if (it->tokenId < vocabSize) {
            logits[it->tokenId] += it->biasValue;
            ++applied;
        }
        if (!it->permanent) {
            it = m_biases.erase(it);
        } else {
            ++it;
        }
    }
    m_stats.biasesApplied.fetch_add(applied, std::memory_order_relaxed);
    return applied;
}

// ---------------------------------------------------------------------------
// Stream Termination
// ---------------------------------------------------------------------------
PatchResult ProxyHotpatcher::add_termination_rule(const StreamTerminationRule& rule) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_terminationRules.push_back(rule);
    return PatchResult::ok("Termination rule added");
}

PatchResult ProxyHotpatcher::remove_termination_rule(const char* name) {
    if (!name) return PatchResult::error("Null name", 1);
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_terminationRules.begin(); it != m_terminationRules.end(); ++it) {
        if (std::strcmp(it->name, name) == 0) {
            m_terminationRules.erase(it);
            return PatchResult::ok("Termination rule removed");
        }
    }
    return PatchResult::error("Termination rule not found", 2);
}

bool ProxyHotpatcher::check_termination(const char* output, size_t outputLen, size_t tokenCount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& rule : m_terminationRules) {
        if (!rule.enabled) continue;

        // Check max tokens
        if (rule.maxTokens > 0 && tokenCount >= rule.maxTokens) {
            m_stats.streamsTerminated.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        // Check stop sequence
        if (rule.stopSequence && output && outputLen > 0) {
            size_t seqLen = std::strlen(rule.stopSequence);
            if (seqLen > 0 && outputLen >= seqLen) {
                // Search for stop sequence in the last seqLen*2 bytes (near-end matching)
                size_t searchStart = (outputLen > seqLen * 2) ? (outputLen - seqLen * 2) : 0;
                const char* found = nullptr;
                // Manual search (memmem not available on MSVC/MinGW)
                for (size_t i = searchStart; i + seqLen <= outputLen; ++i) {
                    if (std::memcmp(output + i, rule.stopSequence, seqLen) == 0) {
                        found = output + i;
                        break;
                    }
                }
                if (found) {
                    m_stats.streamsTerminated.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Output Rewriting
// ---------------------------------------------------------------------------
PatchResult ProxyHotpatcher::add_rewrite_rule(const OutputRewriteRule& rule) {
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
        RawrXD::License::FeatureID::ProxyHotpatching, __FUNCTION__))
        return PatchResult::error("[LICENSE] Proxy hotpatching requires Enterprise license", -1);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_rewriteRules.push_back(rule);
    return PatchResult::ok("Rewrite rule added");
}

PatchResult ProxyHotpatcher::remove_rewrite_rule(const char* name) {
    if (!name) return PatchResult::error("Null name", 1);
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_rewriteRules.begin(); it != m_rewriteRules.end(); ++it) {
        if (std::strcmp(it->name, name) == 0) {
            m_rewriteRules.erase(it);
            return PatchResult::ok("Rewrite rule removed");
        }
    }
    return PatchResult::error("Rewrite rule not found", 2);
}

size_t ProxyHotpatcher::apply_rewrites(char* output, size_t outputLen, size_t bufferCapacity) {
    if (!output || outputLen == 0) return outputLen;

    std::lock_guard<std::mutex> lock(m_mutex);
    size_t currentLen = outputLen;

    for (auto& rule : m_rewriteRules) {
        if (!rule.enabled || !rule.pattern || !rule.replacement) continue;

        size_t patLen = std::strlen(rule.pattern);
        size_t repLen = std::strlen(rule.replacement);
        if (patLen == 0) continue;

        // Simple single-pass find-and-replace (first occurrence only per pass)
        for (size_t i = 0; i + patLen <= currentLen; ++i) {
            if (std::memcmp(output + i, rule.pattern, patLen) == 0) {
                // Check if replacement fits
                size_t newLen = currentLen - patLen + repLen;
                if (newLen > bufferCapacity) break; // Can't fit

                // Shift tail
                std::memmove(output + i + repLen, output + i + patLen, currentLen - i - patLen);
                std::memcpy(output + i, rule.replacement, repLen);
                currentLen = newLen;
                rule.hitCount++;
                m_stats.rewritesApplied.fetch_add(1, std::memory_order_relaxed);
                break; // One replacement per rule per call
            }
        }
    }

    output[currentLen] = '\0';
    return currentLen;
}

// ---------------------------------------------------------------------------
// Custom Validators
// ---------------------------------------------------------------------------
PatchResult ProxyHotpatcher::add_validator(const ProxyValidator& validator) {
    if (!validator.name || !validator.validate) {
        return PatchResult::error("Null validator name or function pointer", 1);
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_validators.push_back(validator);
    return PatchResult::ok("Validator added");
}

PatchResult ProxyHotpatcher::remove_validator(const char* name) {
    if (!name) return PatchResult::error("Null name", 1);
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_validators.begin(); it != m_validators.end(); ++it) {
        if (std::strcmp(it->name, name) == 0) {
            m_validators.erase(it);
            return PatchResult::ok("Validator removed");
        }
    }
    return PatchResult::error("Validator not found", 2);
}

bool ProxyHotpatcher::run_validators(const char* output, size_t outputLen) {
    std::lock_guard<std::mutex> lock(m_mutex);
    bool allPassed = true;
    for (const auto& v : m_validators) {
        if (!v.enabled || !v.validate) continue;
        if (v.validate(output, outputLen, v.userData)) {
            m_stats.validationsPassed.fetch_add(1, std::memory_order_relaxed);
        } else {
            m_stats.validationsFailed.fetch_add(1, std::memory_order_relaxed);
            allPassed = false;
        }
    }
    return allPassed;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------
const ProxyHotpatchStats& ProxyHotpatcher::getStats() const {
    return m_stats;
}

void ProxyHotpatcher::resetStats() {
    m_stats.tokensProcessed.store(0, std::memory_order_relaxed);
    m_stats.biasesApplied.store(0, std::memory_order_relaxed);
    m_stats.streamsTerminated.store(0, std::memory_order_relaxed);
    m_stats.rewritesApplied.store(0, std::memory_order_relaxed);
    m_stats.validationsPassed.store(0, std::memory_order_relaxed);
    m_stats.validationsFailed.store(0, std::memory_order_relaxed);
}

PatchResult ProxyHotpatcher::clear_termination_rules() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_terminationRules.clear();
    return PatchResult::ok("All termination rules cleared");
}

PatchResult ProxyHotpatcher::clear_rewrite_rules() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rewriteRules.clear();
    return PatchResult::ok("All rewrite rules cleared");
}

PatchResult ProxyHotpatcher::clear_validators() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_validators.clear();
    return PatchResult::ok("All validators cleared");
}

// ---------------------------------------------------------------------------
// Boyer-Moore Pattern Search (ported from Qt version)
// ---------------------------------------------------------------------------
static int64_t boyer_moore_search(const char* text, size_t textLen,
                                  const char* pattern, size_t patLen,
                                  size_t startPos = 0) {
    if (patLen == 0 || textLen == 0 || patLen > textLen) return -1;

    // Bad character table (256 entries for all byte values)
    int64_t badChar[256];
    for (int i = 0; i < 256; ++i) badChar[i] = -1;
    for (size_t i = 0; i < patLen - 1; ++i) {
        badChar[static_cast<uint8_t>(pattern[i])] = static_cast<int64_t>(i);
    }

    int64_t s = static_cast<int64_t>(startPos);
    int64_t n = static_cast<int64_t>(textLen);
    int64_t m = static_cast<int64_t>(patLen);

    while (s <= n - m) {
        int64_t j = m - 1;
        while (j >= 0 && pattern[j] == text[s + j]) --j;

        if (j < 0) return s;  // Found

        int64_t shift = badChar[static_cast<uint8_t>(text[s + j])];
        s += (j - shift > 1) ? (j - shift) : 1;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Agent Output Validation (ported from Qt version — pure C++20)
// ---------------------------------------------------------------------------
struct AgentValidationResult {
    bool        isValid{true};
    std::string errorMessage;
    std::string correctedOutput;
};

static char toLowerChar(unsigned char c) {
    return static_cast<char>(std::tolower(c));
}
static bool str_icontains(const std::string& haystack, const char* needle) {
    std::string hLower = haystack;
    std::transform(hLower.begin(), hLower.end(), hLower.begin(), toLowerChar);
    std::string nLower(needle);
    std::transform(nLower.begin(), nLower.end(), nLower.begin(), toLowerChar);
    return hLower.find(nLower) != std::string::npos;
}

AgentValidationResult validate_plan_format(const std::string& output) {
    AgentValidationResult result;
    bool hasPlan     = str_icontains(output, "plan");
    bool hasSubagent = str_icontains(output, "runSubagent");

    if (!hasPlan && !hasSubagent) {
        result.isValid = false;
        result.errorMessage = "Plan mode format violation";
        result.correctedOutput =
            "I'm in Plan mode, and I need to run a subagent first to gather information.\n\n"
            + output +
            "\n\nHere is the proposed plan:\n1. [Step 1]\n2. [Step 2]\n3. [Step 3]";
    }
    return result;
}

AgentValidationResult validate_agent_format(const std::string& output) {
    AgentValidationResult result;
    bool hasTodoList = str_icontains(output, "manage_todo_list");
    bool hasSubagent = str_icontains(output, "runSubagent");
    bool hasTodo     = str_icontains(output, "todo");

    if (!hasTodoList && !hasSubagent && !hasTodo) {
        result.isValid = false;
        result.errorMessage = "Agent mode format violation";
        result.correctedOutput =
            "I need to use manage_todo_list and runSubagent for this task.\n\n" + output;
    }
    return result;
}

AgentValidationResult validate_ask_format(const std::string& output) {
    AgentValidationResult result;
    bool hasVerify  = str_icontains(output, "verify");
    bool hasCheck   = str_icontains(output, "check");
    bool hasConfirm = str_icontains(output, "confirm");

    if (!hasVerify && !hasCheck && !hasConfirm) {
        result.isValid = false;
        result.errorMessage = "Ask mode should include verification steps";
    }
    return result;
}

// ---------------------------------------------------------------------------
// Enabled / Disabled state
// ---------------------------------------------------------------------------
void ProxyHotpatcher::setEnabled(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enable;
}

bool ProxyHotpatcher::isEnabled() const {
    return m_enabled;
}

// ---------------------------------------------------------------------------
// Batch byte-patch find-and-replace (Boyer-Moore powered)
// ---------------------------------------------------------------------------
size_t ProxyHotpatcher::apply_rewrites_advanced(
    char* output, size_t outputLen, size_t bufferCapacity,
    bool useBoyer) {
    if (!output || outputLen == 0) return outputLen;

    std::lock_guard<std::mutex> lock(m_mutex);
    size_t currentLen = outputLen;

    for (auto& rule : m_rewriteRules) {
        if (!rule.enabled || !rule.pattern || !rule.replacement) continue;

        size_t patLen = std::strlen(rule.pattern);
        size_t repLen = std::strlen(rule.replacement);
        if (patLen == 0) continue;

        int64_t pos = -1;
        if (useBoyer && patLen > 4) {
            pos = boyer_moore_search(output, currentLen, rule.pattern, patLen);
        } else {
            // Simple linear search
            for (size_t i = 0; i + patLen <= currentLen; ++i) {
                if (std::memcmp(output + i, rule.pattern, patLen) == 0) {
                    pos = static_cast<int64_t>(i);
                    break;
                }
            }
        }

        if (pos >= 0) {
            size_t newLen = currentLen - patLen + repLen;
            if (newLen > bufferCapacity) continue;

            std::memmove(output + pos + repLen,
                         output + pos + patLen,
                         currentLen - static_cast<size_t>(pos) - patLen);
            std::memcpy(output + pos, rule.replacement, repLen);
            currentLen = newLen;
            rule.hitCount++;
            m_stats.rewritesApplied.fetch_add(1, std::memory_order_relaxed);
        }
    }

    output[currentLen] = '\0';
    return currentLen;
}

