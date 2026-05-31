// privacy_redaction_layer.h - Privacy mode + secret/PII redaction
// Feature 7/15 (Cursor parity).
#pragma once

#include <mutex>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace rawrxd::parity {

enum class PrivacyMode { Disabled, Relaxed, Strict };

struct RedactionPattern {
    std::string name;
    std::regex  rx;
    std::string replacement;   // e.g. "[REDACTED_API_KEY]"
};

struct RedactionReport {
    std::size_t bytes_in{0};
    std::size_t bytes_out{0};
    std::size_t matches{0};
    std::vector<std::pair<std::string, std::size_t>> by_pattern; // name → count
};

class PrivacyRedactionLayer {
public:
    PrivacyRedactionLayer();

    void set_mode(PrivacyMode m);
    PrivacyMode mode() const;

    // Add a custom pattern (compiled). Returns false if regex compilation fails.
    bool add_pattern(const std::string& name, const std::string& pattern,
                     const std::string& replacement);

    // Redact in-place-like: returns new string with sensitive substrings replaced.
    std::string redact(std::string_view text, RedactionReport* out = nullptr) const;

    // Gate: returns true if text is safe to send to an external/cloud model in current mode.
    bool allow_external(std::string_view text) const;

    std::size_t pattern_count() const;

private:
    void load_defaults();

    mutable std::mutex         mu_;
    PrivacyMode                mode_{PrivacyMode::Relaxed};
    std::vector<RedactionPattern> patterns_;
};

} // namespace rawrxd::parity
