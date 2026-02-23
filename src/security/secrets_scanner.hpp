// secrets_scanner.hpp — Top-50 Security: detect secrets/API keys in buffers
// Used by IDE (on open/save) and CLI/CI. No external deps.
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {
namespace Security {

struct SecretFinding {
    uint32_t    offset      = 0;   // byte offset in buffer
    uint32_t    length      = 0;   // length of matched span
    uint32_t    line        = 0;   // 1-based line number (if line info computed)
    uint32_t    column      = 0;   // 1-based column
    std::string kind;              // "api_key", "aws_key", "high_entropy", "password", etc.
    std::string snippet;           // short context (e.g. 20 chars around match)
};

/** Scan a buffer for likely secrets (API keys, high-entropy strings, common patterns). */
class SecretsScanner {
public:
    SecretsScanner() = default;

    /** Scan UTF-8 or binary buffer. Appends findings to outFindings. Returns count added. */
    size_t scan(const uint8_t* data, size_t size, std::vector<SecretFinding>& outFindings);

    /** Convenience: scan std::string. */
    size_t scan(const std::string& text, std::vector<SecretFinding>& outFindings);

    /** Enable/disable high-entropy check (can false-positive on base64 data). Default true. */
    void setCheckHighEntropy(bool enable) { m_checkHighEntropy = enable; }
    bool getCheckHighEntropy() const { return m_checkHighEntropy; }

    /** Push findings to ProblemsAggregator (source "Secrets"). Path optional for per-file scan. */
    void reportToProblems(const std::vector<SecretFinding>& findings, const std::string& path = "");

private:
    bool m_checkHighEntropy = true;

    void scanPatterns(const uint8_t* data, size_t size, std::vector<SecretFinding>& out);
    void scanHighEntropy(const uint8_t* data, size_t size, std::vector<SecretFinding>& out);
    void addLineColumn(const uint8_t* base, size_t offset, size_t size, SecretFinding& f);
};

} // namespace Security
} // namespace RawrXD
