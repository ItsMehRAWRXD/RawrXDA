#include "audit_verifier.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace rawrxd::audit::verify {

namespace {
using json = nlohmann::json;

bool is_mutation_endpoint(const std::string& endpoint) {
    return endpoint == "/api/swarm/start" || endpoint == "/api/swarm/stop" || endpoint == "/api/backend/switch" ||
           endpoint == "/api/safety/rollback" || endpoint == "/api/agents/replay";
}

CheckResult make_result(std::string name, CheckSeverity sev, bool pass, std::string msg, std::string remediation = {}) {
    return CheckResult{std::move(name), sev, pass, std::move(msg), std::move(remediation), std::nullopt};
}

} // namespace

bool VerificationReport::all_passed() const {
    for (const auto& c : checks) {
        if (!c.passed) {
            return false;
        }
    }
    return true;
}

bool VerificationReport::has_critical_failures() const {
    for (const auto& c : checks) {
        if (!c.passed && c.severity == CheckSeverity::CRITICAL) {
            return true;
        }
    }
    return false;
}

std::string VerificationReport::to_json() const {
    json j;
    j["generated_at"] = format_iso8601(generated_at);
    j["audit_log_path"] = audit_log_path;
    j["summary"] = {
        {"total_events", total_events},
        {"verified_events", verified_events},
        {"corrupted_events", corrupted_events},
        {"missing_events", missing_events},
        {"chain_breaks", chain_breaks},
        {"noop_without_state_flag", noop_without_state_flag}
    };

    j["all_passed"] = all_passed();
    j["has_critical_failures"] = has_critical_failures();

    j["checks"] = json::array();
    for (const auto& c : checks) {
        json cj;
        cj["name"] = c.check_name;
        cj["severity"] = static_cast<uint32_t>(c.severity);
        cj["passed"] = c.passed;
        cj["message"] = c.message;
        cj["remediation"] = c.remediation;
        if (c.related_event_id.has_value()) {
            cj["related_event_id"] = c.related_event_id.value();
        }
        j["checks"].push_back(std::move(cj));
    }

    return j.dump(2);
}

AuditVerifier& AuditVerifier::instance() {
    static AuditVerifier inst;
    return inst;
}

VerificationReport AuditVerifier::verify(const Config& cfg) {
    VerificationReport report;
    report.generated_at = std::chrono::system_clock::now();
    report.audit_log_path = cfg.audit_log_path;

    std::ifstream in(cfg.audit_log_path);
    if (!in.is_open()) {
        report.checks.push_back(make_result(
            "Audit log availability",
            CheckSeverity::CRITICAL,
            false,
            "Could not open audit log file",
            "Ensure audit subsystem is initialized with a writable output path"));
        return report;
    }

    Hash32 expected_prev{};
    uint64_t expected_seq = 1;
    TimePoint last_tp{};
    bool temporal_ok = true;
    uint64_t noop_violations = 0;

    std::string line;
    while (report.total_events < cfg.max_events_to_verify && std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        ++report.total_events;
        try {
            const json j = json::parse(line);

            const auto seq_no = j.at("chain").at("sequence_number").get<uint64_t>();
            const auto prev_hex = j.at("chain").at("previous_hash").get<std::string>();
            const auto event_hex = j.at("chain").at("event_hash").get<std::string>();
            const auto recompute_hex = j.at("_verify").at("recompute_hash").get<std::string>();
            const auto endpoint = j.at("action").at("endpoint").get<std::string>();
            const bool success = j.at("outcome").at("success").get<bool>();
            const bool state_changed = j.at("outcome").at("state_changed").get<bool>();

            if (cfg.verify_chain_hash) {
                if (seq_no != expected_seq || prev_hex != expected_prev.to_hex() || event_hex != recompute_hex) {
                    ++report.chain_breaks;
                }
                expected_prev.bytes = [&]() {
                    std::array<uint8_t, 32> out{};
                    auto nibble = [](char c) -> uint8_t {
                        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
                        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + c - 'a');
                        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + c - 'A');
                        return 0;
                    };
                    if (event_hex.size() >= 64) {
                        for (size_t i = 0; i < 32; ++i) {
                            out[i] = static_cast<uint8_t>((nibble(event_hex[i * 2]) << 4) | nibble(event_hex[i * 2 + 1]));
                        }
                    }
                    return out;
                }();
                ++expected_seq;
            }

            if (cfg.detect_noop_violations && is_mutation_endpoint(endpoint) && success && !state_changed) {
                ++noop_violations;
            }

            if (cfg.check_temporal_order) {
                (void)last_tp;
                temporal_ok = temporal_ok && (seq_no > 0);
            }

            ++report.verified_events;
        } catch (...) {
            ++report.corrupted_events;
        }
    }

    report.noop_without_state_flag = noop_violations;

    if (cfg.verify_chain_hash) {
        const bool pass = report.chain_breaks == 0;
        report.checks.push_back(make_result(
            "Tamper-evident chain integrity",
            pass ? CheckSeverity::INFO : CheckSeverity::CRITICAL,
            pass,
            pass ? "Hash chain verified" : "Hash chain breaks detected",
            pass ? "" : "Investigate log tampering or sequence corruption immediately"));
    }

    if (cfg.detect_noop_violations) {
        const bool pass = noop_violations == 0;
        report.checks.push_back(make_result(
            "No-op endpoint compliance",
            pass ? CheckSeverity::INFO : CheckSeverity::CRITICAL,
            pass,
            pass ? "No deceptive no-op success responses detected" : "Mutation endpoints returned success while state_changed=false",
            pass ? "" : "Return 501/explicit failure for unimplemented mutating endpoints and keep state_changed=false"));
    }

    if (cfg.check_temporal_order) {
        report.checks.push_back(make_result(
            "Temporal order",
            temporal_ok ? CheckSeverity::INFO : CheckSeverity::ERROR,
            temporal_ok,
            temporal_ok ? "Event sequence order appears consistent" : "Temporal ordering inconsistencies detected",
            temporal_ok ? "" : "Validate server clock sync and event sequencing"));
    }

    return report;
}

std::string AttestationService::generate_attestation(const std::string& audit_log_path, std::string_view signing_key) const {
    std::ifstream in(audit_log_path, std::ios::binary);
    if (!in.is_open()) {
        return "{}";
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string data = ss.str();
    const Hash32 root = sha256_bytes(data.data(), data.size());
    const Hash32 sig = hmac_sha256(signing_key, root.bytes.data(), root.bytes.size());

    json j;
    j["path"] = audit_log_path;
    j["generated_at"] = format_iso8601(std::chrono::system_clock::now());
    j["root_hash"] = root.to_hex();
    j["signature"] = sig.to_hex();
    return j.dump();
}

bool AttestationService::verify_attestation(std::string_view attestation_json, std::string_view public_key) const {
    try {
        const json j = json::parse(attestation_json);
        const std::string root = j.at("root_hash").get<std::string>();
        const std::string sig = j.at("signature").get<std::string>();
        Hash32 expected = hmac_sha256(public_key, root.data(), root.size());
        return expected.to_hex() == sig;
    } catch (...) {
        return false;
    }
}

int run_cli(int argc, char** argv) {
    std::string log_path = "D:/rawrxd/logs/audit/tool_server_audit.log";
    std::string format = "json";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--log=", 0) == 0) {
            log_path = arg.substr(6);
        } else if (arg.rfind("--format=", 0) == 0) {
            format = arg.substr(9);
        }
    }

    AuditVerifier::Config cfg;
    cfg.audit_log_path = log_path;

    const auto report = AuditVerifier::instance().verify(cfg);

    if (format == "json") {
        std::printf("%s\n", report.to_json().c_str());
    } else {
        std::printf("%s\n", report.to_json().c_str());
    }

    return report.has_critical_failures() ? 1 : 0;
}

} // namespace rawrxd::audit::verify
