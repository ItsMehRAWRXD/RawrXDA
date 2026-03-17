#include "Metrics.hpp"
#include <QString>
#include <QJsonObject>

namespace RawrXD {

struct LLMMetrics {
    struct Request {};
    static void recordRequest(const Request&) {}
};

struct CircuitBreakerMetrics {
    struct Event {};
    static void recordEvent(const Event&) {}
};

} // namespace RawrXD
