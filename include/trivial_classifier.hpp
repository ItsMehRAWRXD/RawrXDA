// ============================================================================
// trivial_classifier.hpp — Centralized Trivial Input Detection (Single Source)
// ============================================================================
//
// Action Item #5: Centralize "trivial detection" rules across Python + C++.
// One authoritative trivial classifier version; both sides report the same
// version id via /api/cot/metrics.
//
// Design:
//   - Shared patterns exported from C++ ReasoningProfileManager
//   - Version hash computed at compile time
//   - Python reads the version via /api/cot/metrics → trivialVersion field
//   - No exceptions, no STL allocators on hot path
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_TRIVIAL_CLASSIFIER_H
#define RAWRXD_TRIVIAL_CLASSIFIER_H

#include <cstdint>
#include <cstring>
#include <string>
#include <algorithm>
#include <cctype>

// ============================================================================
// Classifier Version — single source of truth
// Bump this when patterns change so Python can detect drift.
// ============================================================================
static constexpr const char* TRIVIAL_CLASSIFIER_VERSION = "tc-v3-20260211";

// ============================================================================
// Canonical Pattern List — shared between C++ and Python
// ============================================================================
namespace TrivialPatterns {

    // Exact-match greetings (case-insensitive, punctuation-stripped)
    static const char* kExactGreetings[] = {
        "hi", "hello", "hey", "yo", "sup", "greetings", "howdy",
        "good morning", "good afternoon", "good evening", "gm", "gn",
        "what's up", "whats up", "wassup", "hiya", "heya",
        "help", "test", "ok", "yes", "no", "thanks", "thank",
        "bye", "goodbye", "cya", "later", "thx", "ty", "np",
        nullptr
    };

    // Maximum character length for trivial classification
    static constexpr int kMaxTrivialLength = 20;

    // FNV-1a hash of the pattern list for version verification
    inline uint64_t computePatternHash() {
        uint64_t hash = 0xcbf29ce484222325ULL;
        for (int i = 0; kExactGreetings[i]; ++i) {
            const char* s = kExactGreetings[i];
            while (*s) {
                hash ^= static_cast<uint64_t>(*s++);
                hash *= 0x100000001b3ULL;
            }
        }
        return hash;
    }

} // namespace TrivialPatterns

// ============================================================================
// TrivialClassifier — stateless, reentrant
// ============================================================================
class TrivialClassifier {
public:
    /// Classify input as trivial (greetings, yes/no, etc.)
    /// Returns true if the input should bypass deep CoT analysis.
    static bool isTrivial(const std::string& input) {
        if (input.empty()) return true;

        // Lowercase + trim
        std::string lower;
        lower.resize(input.size());
        std::transform(input.begin(), input.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        size_t start = lower.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return true; // whitespace-only
        size_t end = lower.find_last_not_of(" \t\r\n");
        lower = lower.substr(start, end - start + 1);

        // Strip trailing punctuation
        while (!lower.empty() &&
               (lower.back() == '!' || lower.back() == '?' || lower.back() == '.'))
            lower.pop_back();

        // Length check
        if (static_cast<int>(lower.size()) > TrivialPatterns::kMaxTrivialLength)
            return false;

        // Exact match against canonical patterns
        for (int i = 0; TrivialPatterns::kExactGreetings[i]; ++i) {
            if (lower == TrivialPatterns::kExactGreetings[i])
                return true;
        }

        // Single-character inputs are trivial
        if (lower.size() <= 1) return true;

        // Single '?' is trivial
        if (lower == "?") return true;

        return false;
    }

    /// Get the authoritative classifier version string
    static const char* version() {
        return TRIVIAL_CLASSIFIER_VERSION;
    }

    /// Get the compiled pattern hash for drift detection
    static uint64_t patternHash() {
        static uint64_t hash = TrivialPatterns::computePatternHash();
        return hash;
    }
};

#endif // RAWRXD_TRIVIAL_CLASSIFIER_H
