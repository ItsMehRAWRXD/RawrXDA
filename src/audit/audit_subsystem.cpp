#include "audit_subsystem.hpp"

#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>

#ifdef _WIN32
#include <bcrypt.h>
#include <windows.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace rawrxd::audit {

namespace {

using json = nlohmann::json;

std::string hex_encode(const uint8_t* data, size_t len) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out[i * 2] = kHex[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[data[i] & 0x0F];
    }
    return out;
}

std::array<uint8_t, 32> hex_decode32(std::string_view hex) {
    std::array<uint8_t, 32> out{};
    auto nibble = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + (c - 'A'));
        return 0;
    };
    if (hex.size() < 64) {
        return out;
    }
    for (size_t i = 0; i < 32; ++i) {
        out[i] = static_cast<uint8_t>((nibble(hex[i * 2]) << 4) | nibble(hex[i * 2 + 1]));
    }
    return out;
}

bool category_match(Category lhs, Category rhs_mask) {
    return (static_cast<uint16_t>(lhs) & static_cast<uint16_t>(rhs_mask)) != 0;
}

} // namespace

bool Actor::is_valid() const noexcept {
    return !user_id_hash.empty() && !session_id_hash.empty();
}

bool Action::is_valid() const noexcept {
    return !type.empty() && !endpoint.empty();
}

bool Resource::is_valid() const noexcept {
    return !type.empty();
}

std::string Hash32::to_hex() const {
    return hex_encode(bytes.data(), bytes.size());
}

AuditEvent::Builder& AuditEvent::Builder::with_id(std::string v) { event_.event_id = std::move(v); return *this; }
AuditEvent::Builder& AuditEvent::Builder::with_timestamp(TimePoint v) { event_.timestamp = v; return *this; }
AuditEvent::Builder& AuditEvent::Builder::with_severity(Severity v) { event_.severity = v; return *this; }
AuditEvent::Builder& AuditEvent::Builder::with_category(Category v) { event_.category = v; return *this; }
AuditEvent::Builder& AuditEvent::Builder::with_actor(Actor v) { event_.actor = std::move(v); return *this; }
AuditEvent::Builder& AuditEvent::Builder::with_action(Action v) { event_.action = std::move(v); return *this; }
AuditEvent::Builder& AuditEvent::Builder::with_resource(Resource v) { event_.resource = std::move(v); return *this; }
AuditEvent::Builder& AuditEvent::Builder::with_outcome(Outcome v) { event_.outcome = std::move(v); return *this; }
AuditEvent::Builder& AuditEvent::Builder::with_context(Context v) { event_.context = std::move(v); return *this; }
AuditEvent::Builder& AuditEvent::Builder::with_chain(ChainLink v) { event_.chain = std::move(v); return *this; }
AuditEvent AuditEvent::Builder::build() const { return event_; }

AuditEvent::Builder AuditEvent::builder() { return Builder{}; }

bool AuditEvent::is_valid() const noexcept {
    return !event_id.empty() && actor.is_valid() && action.is_valid() && resource.is_valid();
}

Hash32 AuditEvent::compute_hash() const {
    std::ostringstream oss;
    oss << event_id << '|'
        << std::chrono::duration_cast<std::chrono::microseconds>(timestamp.time_since_epoch()).count() << '|'
        << static_cast<uint32_t>(severity) << '|'
        << static_cast<uint32_t>(category) << '|'
        << actor.user_id_hash << '|' << actor.session_id_hash << '|' << actor.ip_address_hash << '|' << actor.user_agent_hash << '|'
        << actor.process_id << '|' << actor.thread_id << '|'
        << action.type << '|' << action.endpoint << '|' << action.method << '|' << action.operation << '|'
        << resource.type << '|' << resource.id << '|' << resource.container << '|' << resource.region << '|'
        << (outcome.success ? 1 : 0) << '|' << (outcome.state_changed ? 1 : 0) << '|'
        << outcome.reason << '|' << outcome.description << '|' << outcome.error_code << '|'
        << context.request_body_hash << '|' << context.response_body_hash << '|'
        << context.before_state_hash << '|' << context.after_state_hash << '|'
        << context.duration_us << '|' << context.memory_delta_kb;

    for (const auto& p : context.metadata) {
        oss << '|' << p.first << '=' << p.second;
    }

    const std::string payload = oss.str();
    return sha256_bytes(payload.data(), payload.size());
}

std::string AuditEvent::to_json() const {
    json j;
    j["event_id"] = event_id;
    j["timestamp"] = format_iso8601(timestamp);
    j["severity"] = static_cast<uint32_t>(severity);
    j["severity_name"] = to_string(severity);
    j["category"] = static_cast<uint32_t>(category);
    j["category_name"] = to_string(category);

    j["actor"] = {
        {"user_id_hash", actor.user_id_hash},
        {"session_id_hash", actor.session_id_hash},
        {"ip_address_hash", actor.ip_address_hash},
        {"user_agent_hash", actor.user_agent_hash},
        {"process_id", actor.process_id},
        {"thread_id", actor.thread_id}
    };

    j["action"] = {
        {"type", action.type},
        {"endpoint", action.endpoint},
        {"method", action.method},
        {"operation", action.operation},
        {"parameters", action.parameters}
    };

    j["resource"] = {
        {"type", resource.type},
        {"id", resource.id},
        {"container", resource.container},
        {"region", resource.region}
    };

    j["outcome"] = {
        {"success", outcome.success},
        {"state_changed", outcome.state_changed},
        {"reason", outcome.reason},
        {"description", outcome.description},
        {"error_code", outcome.error_code},
        {"is_noop", outcome.is_noop()}
    };

    json md = json::object();
    for (const auto& kv : context.metadata) {
        md[kv.first] = kv.second;
    }

    j["context"] = {
        {"request_body_hash", context.request_body_hash},
        {"response_body_hash", context.response_body_hash},
        {"before_state_hash", context.before_state_hash},
        {"after_state_hash", context.after_state_hash},
        {"duration_us", context.duration_us},
        {"memory_delta_kb", context.memory_delta_kb},
        {"metadata", md}
    };

    j["chain"] = {
        {"sequence_number", chain.sequence_number},
        {"previous_hash", chain.previous_hash.to_hex()},
        {"event_hash", chain.event_hash.to_hex()},
        {"signature", chain.signature.to_hex()},
        {"timestamp", format_iso8601(chain.timestamp)}
    };

    if (outcome.state_changed) {
        j["transition_envelope"] = {
            {"version", 1},
            {"transition_id", event_id},
            {"operation", action.operation},
            {"resource", resource.type + ":" + resource.id},
            {"chain_sequence", chain.sequence_number},
            {"chain_prev", chain.previous_hash.to_hex()},
            {"chain_head", chain.event_hash.to_hex()},
            {"signature", chain.signature.to_hex()},
            {"signed", chain.signature.to_hex() != std::string(64, '0')}
        };
    }

    j["_verify"] = {
        {"recompute_hash", compute_hash().to_hex()},
        {"hash_match", compute_hash() == chain.event_hash}
    };

    return j.dump();
}

std::string AuditEvent::to_cef() const {
    std::ostringstream ss;
    ss << "CEF:0|RawrXD|ToolServer|1.0|" << static_cast<uint32_t>(category)
       << '|' << action.type << '|' << static_cast<uint32_t>(severity) << '|'
       << "rt=" << format_iso8601(timestamp)
       << " request=" << action.endpoint
       << " requestMethod=" << action.method
       << " outcome=" << (outcome.success ? "success" : "failure")
       << " reason=" << outcome.reason
       << " cs1=" << event_id << " cs1Label=eventId"
       << " cs2=" << chain.event_hash.to_hex() << " cs2Label=eventHash"
       << " cs3=" << (outcome.state_changed ? "true" : "false") << " cs3Label=stateChanged";
    return ss.str();
}

std::string AuditEvent::to_leef() const {
    std::ostringstream ss;
    ss << "LEEF:1.0|RawrXD|ToolServer|1.0|" << static_cast<uint32_t>(category)
       << "\tdevTime=" << format_iso8601(timestamp)
       << "\teventId=" << event_id
       << "\teventType=" << action.type
       << "\trequestMethod=" << action.method
       << "\turl=" << action.endpoint
       << "\toutcome=" << (outcome.success ? "success" : "failure")
       << "\tstateChanged=" << (outcome.state_changed ? "true" : "false")
       << "\teventHash=" << chain.event_hash.to_hex();
    return ss.str();
}

class AuditOrchestrator::Impl {
public:
    static constexpr size_t kShardCount = 4;
    static constexpr size_t kRingCapacity = 4096;

    struct Shard {
        LockFreeRingBuffer<AuditEvent, kRingCapacity> ring;
        std::atomic<uint64_t> dropped{0};
    };

    Config config{};
    std::array<Shard, kShardCount> shards{};
    mutable std::mutex io_mutex{};
    mutable std::mutex stats_mutex{};
    mutable std::mutex chain_mutex{};
    std::ofstream out_file{};
    std::atomic<bool> running{false};
    std::condition_variable cv{};
    std::mutex cv_mutex{};
    std::thread flusher{};
    uint64_t seq{0};
    Hash32 last_hash{};
    std::vector<KernelEventHook> hooks{};
    Stats stats{};

    bool initialize(const Config& cfg) {
        config = cfg;
        if (!config.output_path.empty()) {
            out_file.open(config.output_path, std::ios::out | std::ios::app | std::ios::binary);
            if (!out_file.is_open()) {
                return false;
            }
        }
        running.store(true, std::memory_order_release);
        flusher = std::thread([this]() { flush_loop(); });
        return true;
    }

    void shutdown() {
        running.store(false, std::memory_order_release);
        cv.notify_all();
        if (flusher.joinable()) {
            flusher.join();
        }
        flush_all();
        if (out_file.is_open()) {
            out_file.flush();
            out_file.close();
        }
    }

    bool log_event(const AuditEvent& input) {
        if (!category_match(input.category, config.filter_mask)) {
            return true;
        }
        if (static_cast<uint8_t>(input.severity) < static_cast<uint8_t>(config.min_severity)) {
            return true;
        }

        const auto begin = std::chrono::high_resolution_clock::now();

        AuditEvent event = input;
        if (event.timestamp == TimePoint{}) {
            event.timestamp = std::chrono::system_clock::now();
        }
        if (event.event_id.empty()) {
            event.event_id = generate_uuid_v4();
        }

        {
            std::lock_guard<std::mutex> lock(chain_mutex);
            event.chain.previous_hash = last_hash;
            event.chain.event_hash = event.compute_hash();
            event.chain.sequence_number = ++seq;
            event.chain.timestamp = event.timestamp;
            if (!config.hmac_key.empty()) {
                event.chain.signature = hmac_sha256(config.hmac_key, event.chain.event_hash.bytes.data(), event.chain.event_hash.bytes.size());
            }
            last_hash = event.chain.event_hash;
        }

        const size_t shard_idx = std::hash<std::thread::id>{}(std::this_thread::get_id()) % kShardCount;
        bool ok = shards[shard_idx].ring.try_push(event);

        {
            std::lock_guard<std::mutex> lock(stats_mutex);
            if (ok) {
                ++stats.events_logged;
            } else {
                ++stats.events_dropped;
                shards[shard_idx].dropped.fetch_add(1, std::memory_order_relaxed);
            }

            const auto end = std::chrono::high_resolution_clock::now();
            const double us = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());
            if (stats.events_logged > 0) {
                const double n = static_cast<double>(stats.events_logged);
                stats.avg_latency_us = ((stats.avg_latency_us * (n - 1.0)) + us) / n;
            }
        }

        if (ok) {
            cv.notify_one();
        }

        if (ok) {
            std::lock_guard<std::mutex> lock(chain_mutex);
            for (const auto& hook : hooks) {
                if (hook) {
                    hook(event);
                }
            }
        }

        return ok;
    }

    void flush_loop() {
        while (running.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lock(cv_mutex);
            cv.wait_for(lock, std::chrono::milliseconds(config.flush_interval_ms));
            lock.unlock();
            flush_all();
        }
    }

    void flush_all() {
        for (auto& shard : shards) {
            while (true) {
                auto item = shard.ring.try_pop();
                if (!item.has_value()) {
                    break;
                }
                write_event(item.value());
            }
        }
    }

    void write_event(const AuditEvent& event) {
        const std::string line = event.to_json();
        {
            std::lock_guard<std::mutex> lock(io_mutex);
            if (out_file.is_open()) {
                out_file << line << '\n';
                out_file.flush();
            }
            if (config.enable_console) {
                std::fprintf(stderr, "[AUDIT] %s\n", line.c_str());
            }
            std::lock_guard<std::mutex> s_lock(stats_mutex);
            stats.bytes_written += static_cast<uint64_t>(line.size() + 1);
        }
    }

    bool verify_chain_integrity(size_t max_events) {
        if (config.output_path.empty()) {
            return false;
        }
        std::ifstream in(config.output_path);
        if (!in.is_open()) {
            return false;
        }

        Hash32 expected_prev{};
        uint64_t expected_seq = 1;
        size_t count = 0;
        bool ok = true;

        std::string line;
        while (count < max_events && std::getline(in, line)) {
            try {
                json j = json::parse(line);
                const auto seq_no = j.at("chain").at("sequence_number").get<uint64_t>();
                const auto prev_hex = j.at("chain").at("previous_hash").get<std::string>();
                const auto event_hex = j.at("chain").at("event_hash").get<std::string>();
                const auto recompute_hex = j.at("_verify").at("recompute_hash").get<std::string>();

                if (seq_no != expected_seq || prev_hex != expected_prev.to_hex() || event_hex != recompute_hex) {
                    ok = false;
                    break;
                }

                expected_prev.bytes = hex_decode32(event_hex);
                ++expected_seq;
                ++count;
            } catch (...) {
                ok = false;
                break;
            }
        }

        std::lock_guard<std::mutex> lock(stats_mutex);
        ++stats.chain_verifications;
        if (!ok) {
            ++stats.integrity_violations;
        }
        return ok;
    }

    std::vector<AuditEvent> query_events(Category category, Severity min_severity, TimePoint start, TimePoint end) const {
        std::vector<AuditEvent> out;
        if (config.output_path.empty()) {
            return out;
        }
        std::ifstream in(config.output_path);
        if (!in.is_open()) {
            return out;
        }

        std::string line;
        while (std::getline(in, line)) {
            try {
                json j = json::parse(line);
                const auto sev = static_cast<Severity>(j.at("severity").get<uint32_t>());
                const auto cat = static_cast<Category>(j.at("category").get<uint32_t>());
                if (!category_match(cat, category)) {
                    continue;
                }
                if (static_cast<uint8_t>(sev) < static_cast<uint8_t>(min_severity)) {
                    continue;
                }

                AuditEvent ev;
                ev.event_id = j.at("event_id").get<std::string>();
                ev.timestamp = std::chrono::system_clock::now();
                ev.severity = sev;
                ev.category = cat;
                ev.action.endpoint = j.at("action").at("endpoint").get<std::string>();
                ev.outcome.success = j.at("outcome").at("success").get<bool>();
                ev.outcome.state_changed = j.at("outcome").at("state_changed").get<bool>();
                ev.chain.sequence_number = j.at("chain").at("sequence_number").get<uint64_t>();

                (void)start;
                (void)end;
                out.push_back(std::move(ev));
            } catch (...) {
            }
        }
        return out;
    }

    void emergency_purge(const std::string& reason) {
        AuditEvent purge = AuditEvent::builder()
            .with_id(generate_uuid_v4())
            .with_timestamp(std::chrono::system_clock::now())
            .with_severity(Severity::ALERT)
            .with_category(Category::ADMIN_ACTION)
            .with_actor(Actor{.user_id_hash = "system", .session_id_hash = "system"})
            .with_action(Action{.type = "audit_emergency_purge", .endpoint = "internal", .method = "ADMIN", .operation = "purge"})
            .with_resource(Resource{.type = "audit_log", .id = config.output_path, .container = "local", .region = "local"})
            .with_outcome(Outcome{.success = true, .state_changed = true, .reason = "compliance_purge", .description = reason})
            .build();

        log_event(purge);
        flush_all();

        {
            std::lock_guard<std::mutex> lock(io_mutex);
            if (out_file.is_open()) {
                out_file.close();
            }
            std::ofstream trunc(config.output_path, std::ios::trunc);
            trunc.close();
            out_file.open(config.output_path, std::ios::out | std::ios::app | std::ios::binary);
        }
    }

    Stats get_stats() const {
        std::lock_guard<std::mutex> lock(stats_mutex);
        return stats;
    }
};

AuditOrchestrator::AuditOrchestrator() : impl_(std::make_unique<Impl>()) {}
AuditOrchestrator::~AuditOrchestrator() { shutdown(); }

AuditOrchestrator& AuditOrchestrator::instance() {
    static AuditOrchestrator inst;
    return inst;
}

bool AuditOrchestrator::initialize(const Config& cfg) { return impl_->initialize(cfg); }
void AuditOrchestrator::shutdown() { impl_->shutdown(); }
bool AuditOrchestrator::log_event(const AuditEvent& event) { return impl_->log_event(event); }
bool AuditOrchestrator::verify_chain_integrity(size_t max_events) { return impl_->verify_chain_integrity(max_events); }
std::vector<AuditEvent> AuditOrchestrator::query_events(Category category, Severity min_severity, TimePoint start, TimePoint end) const {
    return impl_->query_events(category, min_severity, start, end);
}
AuditOrchestrator::Stats AuditOrchestrator::get_stats() const { return impl_->get_stats(); }
void AuditOrchestrator::emergency_purge(const std::string& reason) { impl_->emergency_purge(reason); }

bool AuditOrchestrator::register_kernel_hook(KernelEventHook hook) {
    if (!hook) {
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->chain_mutex);
    impl_->hooks.push_back(std::move(hook));
    return true;
}

void AuditOrchestrator::clear_kernel_hooks() {
    std::lock_guard<std::mutex> lock(impl_->chain_mutex);
    impl_->hooks.clear();
}

std::string generate_uuid_v4() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    uint64_t a = dis(gen);
    uint64_t b = dis(gen);

    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << (a >> 32) << '-'
       << std::setw(4) << ((a >> 16) & 0xFFFF) << '-'
       << std::setw(4) << (a & 0xFFFF) << '-'
       << std::setw(4) << ((b >> 48) & 0xFFFF) << '-'
       << std::setw(12) << (b & 0xFFFFFFFFFFFFULL);
    return ss.str();
}

std::string format_iso8601(TimePoint tp) {
    using namespace std::chrono;
    const auto sec_tp = time_point_cast<seconds>(tp);
    const auto ms = duration_cast<milliseconds>(tp - sec_tp).count();
    const std::time_t t = system_clock::to_time_t(tp);

    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms << 'Z';
    return ss.str();
}

std::string hash_string(std::string_view input) {
    return sha256_bytes(input.data(), input.size()).to_hex();
}

Hash32 sha256_bytes(const void* data, size_t len) {
    Hash32 out;
#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st)) {
        return out;
    }

    st = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return out;
    }

    st = BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(len), 0);
    if (BCRYPT_SUCCESS(st)) {
        BCryptFinishHash(hHash, out.bytes.data(), static_cast<ULONG>(out.bytes.size()), 0);
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
#else
    // Minimal fallback keeps behavior deterministic in non-Windows builds.
    const uint64_t h = std::hash<std::string_view>{}(std::string_view(static_cast<const char*>(data), len));
    for (size_t i = 0; i < out.bytes.size(); ++i) {
        out.bytes[i] = static_cast<uint8_t>((h >> ((i % 8) * 8)) & 0xFF);
    }
#endif
    return out;
}

Hash32 hmac_sha256(std::string_view key, const void* data, size_t len) {
#ifdef _WIN32
    Hash32 out;
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(st)) {
        return out;
    }

    st = BCryptCreateHash(
        hAlg,
        &hHash,
        nullptr,
        0,
        reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),
        static_cast<ULONG>(key.size()),
        0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return out;
    }

    st = BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(len), 0);
    if (BCRYPT_SUCCESS(st)) {
        BCryptFinishHash(hHash, out.bytes.data(), static_cast<ULONG>(out.bytes.size()), 0);
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return out;
#else
    (void)key;
    return sha256_bytes(data, len);
#endif
}

const char* to_string(Severity severity) {
    switch (severity) {
        case Severity::DEBUG: return "DEBUG";
        case Severity::INFO: return "INFO";
        case Severity::NOTICE: return "NOTICE";
        case Severity::WARNING: return "WARNING";
        case Severity::ERROR: return "ERROR";
        case Severity::CRITICAL: return "CRITICAL";
        case Severity::ALERT: return "ALERT";
        case Severity::EMERGENCY: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}

const char* to_string(Category category) {
    switch (category) {
        case Category::AUTHENTICATION: return "AUTHENTICATION";
        case Category::AUTHORIZATION: return "AUTHORIZATION";
        case Category::DATA_ACCESS: return "DATA_ACCESS";
        case Category::DATA_MODIFICATION: return "DATA_MODIFICATION";
        case Category::SYSTEM_CONFIG: return "SYSTEM_CONFIG";
        case Category::NETWORK: return "NETWORK";
        case Category::AI_INFERENCE: return "AI_INFERENCE";
        case Category::SWARM_OPERATION: return "SWARM_OPERATION";
        case Category::BACKEND_SWITCH: return "BACKEND_SWITCH";
        case Category::SAFETY_ROLLBACK: return "SAFETY_ROLLBACK";
        case Category::AGENT_REPLAY: return "AGENT_REPLAY";
        case Category::ADMIN_ACTION: return "ADMIN_ACTION";
        case Category::SECURITY_VIOLATION: return "SECURITY_VIOLATION";
        case Category::COMPLIANCE: return "COMPLIANCE";
        case Category::ALL: return "ALL";
        default: return "UNKNOWN";
    }
}

} // namespace rawrxd::audit
