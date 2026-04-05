#pragma once

#include "audit_subsystem.hpp"

#include <optional>
#include <string>
#include <vector>

namespace rawrxd::audit::verify {

enum class CheckSeverity : uint8_t {
    INFO = 0,
    WARNING = 1,
    ERROR = 2,
    CRITICAL = 3,
};

struct CheckResult {
    std::string check_name;
    CheckSeverity severity{CheckSeverity::INFO};
    bool passed{false};
    std::string message;
    std::string remediation;
    std::optional<std::string> related_event_id;
};

struct VerificationReport {
    TimePoint generated_at{};
    std::string audit_log_path;
    uint64_t total_events{0};
    uint64_t verified_events{0};
    uint64_t corrupted_events{0};
    uint64_t missing_events{0};
    uint64_t chain_breaks{0};
    uint64_t noop_without_state_flag{0};

    std::vector<CheckResult> checks;

    [[nodiscard]] bool all_passed() const;
    [[nodiscard]] bool has_critical_failures() const;
    [[nodiscard]] std::string to_json() const;
};

class AuditVerifier {
public:
    struct Config {
        std::string audit_log_path;
        bool verify_chain_hash{true};
        bool detect_noop_violations{true};
        bool check_temporal_order{true};
        size_t max_events_to_verify{100000};
    };

    [[nodiscard]] static AuditVerifier& instance();
    [[nodiscard]] VerificationReport verify(const Config& cfg);

private:
    AuditVerifier() = default;
};

class AttestationService {
public:
    [[nodiscard]] std::string generate_attestation(const std::string& audit_log_path, std::string_view signing_key) const;
    [[nodiscard]] bool verify_attestation(std::string_view attestation_json, std::string_view public_key) const;
};

[[nodiscard]] int run_cli(int argc, char** argv);

} // namespace rawrxd::audit::verify
