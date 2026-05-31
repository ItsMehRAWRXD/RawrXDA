// privacy_redaction_layer.cpp - Implementation
#include "privacy_redaction_layer.h"

namespace rawrxd::parity {

PrivacyRedactionLayer::PrivacyRedactionLayer() { load_defaults(); }

void PrivacyRedactionLayer::load_defaults() {
    // A compact but high-value default set; avoids false-positive explosions.
    auto add = [&](const char* name, const char* rx, const char* rep) {
        try {
            patterns_.push_back({ name, std::regex(rx, std::regex::ECMAScript | std::regex::optimize), rep });
        } catch (...) {}
    };
    add("aws_access_key",   R"(AKIA[0-9A-Z]{16})",                                   "[REDACTED_AWS_KEY]");
    add("aws_secret",       R"((?i)aws(.{0,20})?(secret|key)[^\n]{0,3}[:=]\s*[A-Za-z0-9/+=]{32,})", "[REDACTED_AWS_SECRET]");
    add("github_token",     R"(gh[pousr]_[A-Za-z0-9]{30,})",                         "[REDACTED_GITHUB_TOKEN]");
    add("openai_key",       R"(sk-[A-Za-z0-9]{20,})",                                "[REDACTED_OPENAI_KEY]");
    add("google_api_key",   R"(AIza[0-9A-Za-z_\-]{35})",                             "[REDACTED_GOOGLE_KEY]");
    add("slack_token",      R"(xox[abpr]-[A-Za-z0-9-]{10,})",                        "[REDACTED_SLACK_TOKEN]");
    add("jwt",              R"(eyJ[A-Za-z0-9_\-]{8,}\.[A-Za-z0-9_\-]{8,}\.[A-Za-z0-9_\-]{8,})", "[REDACTED_JWT]");
    add("pem_block",        R"(-----BEGIN (?:RSA |DSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY-----[\s\S]*?-----END[^\n]*)", "[REDACTED_PRIVATE_KEY]");
    add("email",            R"([A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,})",   "[REDACTED_EMAIL]");
    add("ipv4",             R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)",                        "[REDACTED_IP]");
    add("credit_card",      R"(\b(?:\d[ -]*?){13,19}\b)",                            "[REDACTED_CC]");
    // Generic "password=XYZ" / bearer tokens
    add("password_kv",      R"((?i)(password|passwd|pwd)\s*[:=]\s*["']?[^"'\s]{4,})", "$1=[REDACTED]");
    add("bearer",           R"((?i)bearer\s+[A-Za-z0-9._\-]{10,})",                  "Bearer [REDACTED]");
}

void PrivacyRedactionLayer::set_mode(PrivacyMode m) { std::lock_guard lk(mu_); mode_ = m; }
PrivacyMode PrivacyRedactionLayer::mode() const { std::lock_guard lk(mu_); return mode_; }

bool PrivacyRedactionLayer::add_pattern(const std::string& name,
                                        const std::string& pattern,
                                        const std::string& replacement) {
    try {
        std::regex rx(pattern, std::regex::ECMAScript | std::regex::optimize);
        std::lock_guard lk(mu_);
        patterns_.push_back({ name, std::move(rx), replacement });
        return true;
    } catch (...) {
        return false;
    }
}

std::string PrivacyRedactionLayer::redact(std::string_view text,
                                          RedactionReport* out) const {
    std::lock_guard lk(mu_);
    if (out) { out->bytes_in = text.size(); out->matches = 0; out->by_pattern.clear(); }
    if (mode_ == PrivacyMode::Disabled) {
        if (out) out->bytes_out = text.size();
        return std::string(text);
    }
    std::string cur(text);
    for (const auto& p : patterns_) {
        std::size_t count = 0;
        std::string replaced;
        auto begin = std::sregex_iterator(cur.begin(), cur.end(), p.rx);
        auto end   = std::sregex_iterator();
        std::size_t last = 0;
        for (auto it = begin; it != end; ++it) {
            const auto& m = *it;
            replaced.append(cur, last, m.position() - last);
            replaced.append(m.format(p.replacement));
            last = m.position() + m.length();
            ++count;
        }
        if (count > 0) {
            replaced.append(cur, last, cur.size() - last);
            cur.swap(replaced);
            if (out) { out->matches += count; out->by_pattern.emplace_back(p.name, count); }
        }
    }
    if (out) out->bytes_out = cur.size();
    return cur;
}

bool PrivacyRedactionLayer::allow_external(std::string_view text) const {
    std::lock_guard lk(mu_);
    if (mode_ == PrivacyMode::Disabled) return true;
    if (mode_ == PrivacyMode::Relaxed)  return true;
    // Strict: disallow if any high-sensitivity pattern matches.
    static const std::string high[] = {
        "aws_access_key","aws_secret","github_token","openai_key",
        "google_api_key","slack_token","pem_block","jwt","credit_card"
    };
    for (const auto& p : patterns_) {
        for (const auto& h : high) {
            if (p.name != h) continue;
            if (std::regex_search(text.begin(), text.end(), p.rx)) return false;
        }
    }
    return true;
}

std::size_t PrivacyRedactionLayer::pattern_count() const {
    std::lock_guard lk(mu_); return patterns_.size();
}

} // namespace rawrxd::parity
