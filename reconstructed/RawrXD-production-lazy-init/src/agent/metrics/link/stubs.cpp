#include <cstddef>

namespace RawrXD {

struct LLMMetrics {
    struct Request; // forward declaration of nested Request type
    static void recordRequest(const Request&) {}
};

struct CircuitBreakerMetrics {
    struct Event; // forward declaration of nested Event type
    static void recordEvent(const Event&) {}
};

} // namespace RawrXD
