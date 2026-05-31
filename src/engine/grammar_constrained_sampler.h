// ============================================================================
// grammar_constrained_sampler.h — Grammar-Constrained Token Sampling
// ============================================================================
// Enforces structured output via token-level grammar constraint masking.
// Integrates into the sampler's softmax filtering loop — validates tokens
// against grammar state BEFORE probability normalization, not as a post-pass.
//
// Supported grammar formats:
//   - JSON Schema  → compiled to token masks per schema node
//   - BNF / GBNF   → deterministic finite automaton (DFA) character matching
//   - Regex         → Thompson NFA compiled at init, stepped per token
//
// Architecture:
//   GrammarState    — tracks current DFA/NFA position in the grammar
//   TokenMask       — vocab-sized bitset marking allowed tokens
//   GrammarSpec     — compiled grammar (DFA transitions + token prefix table)
//   ConstrainedSampler — wraps Sampler with grammar-aware logit masking
//
// Hot-path constraint: zero heap allocation during applyMask().
// Overhead: ~5-8% latency per token (mask lookup is O(1) per vocab entry).
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace rawrxd {

// ---------------------------------------------------------------------------
// GrammarFormat — which grammar syntax is active
// ---------------------------------------------------------------------------
enum class GrammarFormat : uint8_t {
    None       = 0,
    JsonSchema = 1,
    BNF        = 2,
    GBNF       = 3,
    Regex      = 4
};

// ---------------------------------------------------------------------------
// TokenMask — vocab-sized bitmask (1 = allowed, 0 = masked)
// ---------------------------------------------------------------------------
struct TokenMask {
    std::vector<uint64_t> bits;   // ceil(vocab / 64) words
    int32_t               vocabSize = 0;

    void resize(int32_t vocab) {
        vocabSize = vocab;
        bits.assign((vocab + 63) / 64, 0ULL);
    }

    void allowAll() {
        std::memset(bits.data(), 0xFF, bits.size() * sizeof(uint64_t));
    }

    void denyAll() {
        std::memset(bits.data(), 0x00, bits.size() * sizeof(uint64_t));
    }

    void allow(int32_t tokenId) {
        if (tokenId >= 0 && tokenId < vocabSize)
            bits[tokenId >> 6] |= (1ULL << (tokenId & 63));
    }

    void deny(int32_t tokenId) {
        if (tokenId >= 0 && tokenId < vocabSize)
            bits[tokenId >> 6] &= ~(1ULL << (tokenId & 63));
    }

    bool isAllowed(int32_t tokenId) const {
        if (tokenId < 0 || tokenId >= vocabSize) return false;
        return (bits[tokenId >> 6] >> (tokenId & 63)) & 1ULL;
    }

    int32_t countAllowed() const {
        int32_t c = 0;
        for (auto w : bits) {
#ifdef _MSC_VER
            c += (int32_t)__popcnt64(w);
#else
            c += __builtin_popcountll(w);
#endif
        }
        return c;
    }
};

// ---------------------------------------------------------------------------
// DFA Transition — one edge in the compiled deterministic automaton
// ---------------------------------------------------------------------------
struct DFATransition {
    uint16_t fromState;
    uint16_t toState;
    uint8_t  charLow;    // inclusive
    uint8_t  charHigh;   // inclusive — range [charLow, charHigh]
};

// ---------------------------------------------------------------------------
// DFAState — per-state metadata
// ---------------------------------------------------------------------------
struct DFAState {
    uint16_t            stateId;
    bool                isAccept;   // accepting state (valid termination)
    std::vector<DFATransition> transitions;
};

// ---------------------------------------------------------------------------
// GrammarSpec — compiled grammar ready for constraint application
// ---------------------------------------------------------------------------
struct GrammarSpec {
    GrammarFormat              format;
    std::string                rawGrammar;    // original text
    std::vector<DFAState>      dfa;           // compiled DFA states
    uint16_t                   startState = 0;

    // Token prefix table: for each token ID → the byte string it decodes to.
    // Used to check if emitting that token keeps us in a valid DFA path.
    std::vector<std::string>   tokenStrings;  // indexed by token ID

    bool isValid() const { return !dfa.empty() && !tokenStrings.empty(); }
};

// ---------------------------------------------------------------------------
// GrammarState — mutable state tracker for in-flight generation
// ---------------------------------------------------------------------------
struct GrammarState {
    uint16_t            currentDFAState = 0;
    std::string         pendingBytes;     // partial UTF-8 at token boundary
    uint32_t            tokensGenerated = 0;
    bool                completed = false; // reached accept state
    bool                faulted   = false; // no valid transition possible
};

// ---------------------------------------------------------------------------
// Compile result
// ---------------------------------------------------------------------------
struct GrammarCompileResult {
    bool        ok = false;
    std::string error;
    GrammarSpec spec;
};

// ---------------------------------------------------------------------------
// ConstrainedSamplerConfig
// ---------------------------------------------------------------------------
struct ConstrainedSamplerConfig {
    float    maskedLogitValue = -1e9f;     // logit assigned to masked tokens
    bool     allowEOSAtAcceptState = true; // allow EOS only at accept states
    uint32_t maxTokens = 8192;             // force-stop generation
    int32_t  eosTokenId = 2;               // typical EOS token
};

// ---------------------------------------------------------------------------
// ConstrainedSampler — grammar-aware logit masking
// ---------------------------------------------------------------------------
class ConstrainedSampler {
public:
    explicit ConstrainedSampler(const ConstrainedSamplerConfig& cfg = {});
    ~ConstrainedSampler();

    // ── Compilation ─────────────────────────────────────────────────────
    // Compile a grammar string into a GrammarSpec + token table.
    // tokenStrings must map every token ID → its decoded byte string.
    GrammarCompileResult compile(GrammarFormat format,
                                 const std::string& grammar,
                                 const std::vector<std::string>& tokenStrings);

    // Compile from JSON Schema (shortcut: converts to GBNF internally)
    GrammarCompileResult compileJsonSchema(const std::string& jsonSchema,
                                           const std::vector<std::string>& tokenStrings);

    // ── Session Management ──────────────────────────────────────────────
    // Begin a new constrained generation session (resets state)
    void beginSession(const GrammarSpec& spec);

    // End current session
    void endSession();

    bool hasActiveSession() const { return m_active; }

    // ── Hot-Path Constraint Application ─────────────────────────────────
    // Apply grammar mask to logits IN-PLACE before softmax.
    // This is the key integration point with the sampler:
    //   1. Compute allowed tokens from current DFA state
    //   2. Set disallowed logits to maskedLogitValue
    //   3. Caller proceeds with normal top-k / top-p / temperature
    //
    // Returns number of allowed tokens (0 = generation should stop).
    int32_t applyMask(float* logits, int32_t vocabSize);

    // ── State Advance ───────────────────────────────────────────────────
    // After a token is sampled, advance the grammar state.
    // Returns false if the token was invalid (should not happen if
    // applyMask was called first, but defensive).
    bool advance(int32_t tokenId);

    // ── Query ───────────────────────────────────────────────────────────
    const GrammarState& state() const { return m_state; }
    bool  isCompleted()   const { return m_state.completed; }
    bool  isFaulted()     const { return m_state.faulted; }

private:
    // Build the token mask for the current DFA state
    void buildMaskForState(uint16_t dfaState);

    // Check if a token string is valid from a given DFA state.
    // Returns the resulting DFA state or UINT16_MAX if invalid.
    uint16_t traceToken(uint16_t fromState, const std::string& tokenStr) const;

    // DFA compilation helpers
    bool compileBNF(const std::string& bnf, GrammarSpec& out);
    bool compileRegex(const std::string& regex, GrammarSpec& out);
    bool compileGBNF(const std::string& gbnf, GrammarSpec& out);

    ConstrainedSamplerConfig m_cfg;
    GrammarSpec              m_spec;
    GrammarState             m_state;
    TokenMask                m_mask;       // reused each applyMask call
    bool                     m_active = false;
    bool                     m_maskDirty = true;
};

} // namespace rawrxd
