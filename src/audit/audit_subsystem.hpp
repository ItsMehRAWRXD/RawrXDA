#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace rawrxd::audit {

using TimePoint = std::chrono::system_clock::time_point;

enum class Severity : uint8_t {
    DEBUG = 0,
    INFO = 1,
    NOTICE = 2,
    WARNING = 3,
    ERROR = 4,
    CRITICAL = 5,
    ALERT = 6,
    EMERGENCY = 7,
};

enum class Category : uint16_t {
    AUTHENTICATION = 0x0001,
    AUTHORIZATION = 0x0002,
    DATA_ACCESS = 0x0004,
    DATA_MODIFICATION = 0x0008,
    SYSTEM_CONFIG = 0x0010,
    NETWORK = 0x0020,
    AI_INFERENCE = 0x0040,
    SWARM_OPERATION = 0x0080,
    BACKEND_SWITCH = 0x0100,
    SAFETY_ROLLBACK = 0x0200,
    AGENT_REPLAY = 0x0400,
    ADMIN_ACTION = 0x0800,
    SECURITY_VIOLATION = 0x1000,
    COMPLIANCE = 0x2000,
    ALL = 0xFFFF,
};

struct Actor {
    std::string user_id_hash;
    std::string session_id_hash;
    std::string ip_address_hash;
    std::string user_agent_hash;
    std::string process_id;
    std::string thread_id;

    [[nodiscard]] bool is_valid() const noexcept;
};

struct Action {
    std::string type;
    std::string endpoint;
    std::string method;
    std::string operation;
    std::vector<std::string> parameters;

    [[nodiscard]] bool is_valid() const noexcept;
};

struct Resource {
    std::string type;
    std::string id;
    std::string container;
    std::string region;

    [[nodiscard]] bool is_valid() const noexcept;
};

struct Outcome {
    bool success{false};
    bool state_changed{false};
    std::string reason;
    std::string description;
    uint32_t error_code{0};

    [[nodiscard]] bool is_noop() const noexcept { return success && !state_changed; }
};

struct Context {
    std::string request_body_hash;
    std::string response_body_hash;
    std::string before_state_hash;
    std::string after_state_hash;
    uint64_t duration_us{0};
    int64_t memory_delta_kb{0};
    std::vector<std::pair<std::string, std::string>> metadata;
};

struct Hash32 {
    std::array<uint8_t, 32> bytes{};

    [[nodiscard]] std::string to_hex() const;
    [[nodiscard]] bool operator==(const Hash32& rhs) const noexcept { return bytes == rhs.bytes; }
};

struct ChainLink {
    Hash32 previous_hash{};
    Hash32 event_hash{};
    Hash32 signature{};
    uint64_t sequence_number{0};
    TimePoint timestamp{};

    [[nodiscard]] bool is_valid() const noexcept { return sequence_number != 0; }
};

class AuditEvent {
public:
    std::string event_id;
    TimePoint timestamp{};
    Severity severity{Severity::INFO};
    Category category{Category::SYSTEM_CONFIG};
    Actor actor{};
    Action action{};
    Resource resource{};
    Outcome outcome{};
    Context context{};
    ChainLink chain{};

    class Builder {
    public:
        Builder& with_id(std::string v);
        Builder& with_timestamp(TimePoint v);
        Builder& with_severity(Severity v);
        Builder& with_category(Category v);
        Builder& with_actor(Actor v);
        Builder& with_action(Action v);
        Builder& with_resource(Resource v);
        Builder& with_outcome(Outcome v);
        Builder& with_context(Context v);
        Builder& with_chain(ChainLink v);
        [[nodiscard]] AuditEvent build() const;

    private:
        AuditEvent event_;
    };

    [[nodiscard]] static Builder builder();
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] Hash32 compute_hash() const;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] std::string to_cef() const;
    [[nodiscard]] std::string to_leef() const;
};

template <typename T, size_t Capacity>
class LockFreeRingBuffer {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

    bool try_push(const T& value) noexcept {
        const uint64_t head = head_.load(std::memory_order_relaxed);
        const uint64_t next = head + 1;
        if (next - tail_.load(std::memory_order_acquire) > Capacity) {
            return false;
        }
        slots_[head & kMask] = value;
        head_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> try_pop() noexcept {
        const uint64_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        T out = slots_[tail & kMask];
        tail_.store(tail + 1, std::memory_order_release);
        return out;
    }

private:
    static constexpr size_t kMask = Capacity - 1;
    std::array<T, Capacity> slots_{};
    std::atomic<uint64_t> head_{0};
    std::atomic<uint64_t> tail_{0};
};

class AuditOrchestrator {
public:
    struct Config {
        std::string output_path;
        bool enable_console{false};
        bool enable_syslog{false};
        bool enable_http{false};
        std::string http_endpoint;
        std::string hmac_key;
        uint32_t flush_interval_ms{1000};
        uint32_t retention_hours{720};
        Category filter_mask{Category::ALL};
        Severity min_severity{Severity::INFO};
    };

    struct Stats {
        uint64_t events_logged{0};
        uint64_t events_dropped{0};
        uint64_t bytes_written{0};
        uint64_t chain_verifications{0};
        uint64_t integrity_violations{0};
        double avg_latency_us{0.0};
    };

    using KernelEventHook = std::function<void(const AuditEvent&)>;

    [[nodiscard]] static AuditOrchestrator& instance();

    [[nodiscard]] bool initialize(const Config& cfg);
    void shutdown();

    [[nodiscard]] bool log_event(const AuditEvent& event);

    [[nodiscard]] bool verify_chain_integrity(size_t max_events = 10000);
    [[nodiscard]] std::vector<AuditEvent> query_events(
        Category category,
        Severity min_severity,
        TimePoint start,
        TimePoint end) const;

    [[nodiscard]] Stats get_stats() const;
    void emergency_purge(const std::string& reason);

    [[nodiscard]] bool register_kernel_hook(KernelEventHook hook);
    void clear_kernel_hooks();

private:
    AuditOrchestrator();
    ~AuditOrchestrator();
    AuditOrchestrator(const AuditOrchestrator&) = delete;
    AuditOrchestrator& operator=(const AuditOrchestrator&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::string generate_uuid_v4();
[[nodiscard]] std::string format_iso8601(TimePoint tp);
[[nodiscard]] std::string hash_string(std::string_view input);
[[nodiscard]] Hash32 sha256_bytes(const void* data, size_t len);
[[nodiscard]] Hash32 hmac_sha256(std::string_view key, const void* data, size_t len);

[[nodiscard]] const char* to_string(Severity severity);
[[nodiscard]] const char* to_string(Category category);

} // namespace rawrxd::audit
