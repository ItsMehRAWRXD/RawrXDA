#include "telemetry_hooks.hpp"

namespace RawrXD {

void LLMMetrics::recordRequest(const Request& request)
{
    (void)request;
}

void CircuitBreakerMetrics::recordEvent(const Event& event)
{
    (void)event;
}

} // namespace RawrXD
