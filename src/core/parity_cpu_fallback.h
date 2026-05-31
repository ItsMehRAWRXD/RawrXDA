// parity_cpu_fallback.h — Deterministic CPU fallback for CLI/UI parity tests
// =============================================================================
// When RAWRXD_PARITY_CPU=1 is set in the environment, both the CLI and the UI
// inference pipelines short-circuit the real model invocation and emit a
// byte-identical, deterministic synthetic token stream derived from
// FNV-1a-64(model + "\n" + prompt). Same inputs ⇒ same tokens ⇒ structural
// parity diff can be performed on any machine without GPU or model weights.
//
// Two callers route through here:
//   1. RawrXD::runLocalInferencePipeline (Win32IDE)
//   2. rawrxd serve / rawrxd run (CLI)
//
// Header-only by design — keeps build wiring trivial.
// =============================================================================
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>

namespace RawrXD::ParityFallback {

// True iff RAWRXD_PARITY_CPU is set to a non-empty, non-"0" value.
inline bool isActive()
{
    const char* v = std::getenv("RAWRXD_PARITY_CPU");
    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

// Deterministic 64-bit FNV-1a over arbitrary bytes.
inline uint64_t fnv1a64(const void* data, size_t len, uint64_t seed = 1469598103934665603ULL)
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = seed;
    for (size_t i = 0; i < len; ++i)
    {
        h ^= static_cast<uint64_t>(p[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

// Splitmix64 step — produces a well-distributed 64-bit value from a 64-bit state.
inline uint64_t splitmix64(uint64_t& s)
{
    s += 0x9E3779B97F4A7C15ULL;
    uint64_t z = s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// Fixed lexicon — never changes; both surfaces draw from this exact array.
inline const char* const* lexicon(size_t& outCount)
{
    static const char* const kLex[] = {
        "The ",  "quick ", "brown ", "fox ",  "jumps ", "over ",  "the ",
        "lazy ", "dog. ", "Hello ", "world. ", "Test ", "token ", "stream ",
        "from ", "deterministic ", "fallback ", "lane.\n",
        "RawrXD ", "parity ", "verified. ",
    };
    outCount = sizeof(kLex) / sizeof(kLex[0]);
    return kLex;
}

// Deterministic generator. `numPredict` is clamped to [1, 32].
// Calls `onToken(tok, done=false)` per token, then once with (tok="", done=true).
inline void run(const std::string& model,
                const std::string& prompt,
                int numPredict,
                const std::function<void(const std::string&, bool)>& onToken)
{
    if (!onToken) return;

    // Seed = FNV-1a over (model + "\n" + prompt) — order-stable, encoding-agnostic.
    uint64_t state = fnv1a64(model.data(), model.size());
    state = fnv1a64("\n", 1, state);
    state = fnv1a64(prompt.data(), prompt.size(), state);

    size_t lexN = 0;
    const char* const* lex = lexicon(lexN);

    int n = numPredict;
    if (n < 1)  n = 1;
    if (n > 32) n = 32;

    for (int i = 0; i < n; ++i)
    {
        uint64_t r = splitmix64(state);
        const char* tok = lex[r % lexN];
        onToken(std::string(tok), false);
    }
    onToken(std::string(), true);
}

} // namespace RawrXD::ParityFallback
