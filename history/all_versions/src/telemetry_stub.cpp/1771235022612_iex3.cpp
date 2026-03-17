/* ============================================================================
 * RawrXD Telemetry Stub
 * Placeholder for telemetry collection - no-op implementation
 * ============================================================================ */
#include <string>
#include <cstdint>

namespace rawrxd {
namespace telemetry {

static bool g_telemetry_enabled = false;

void init() {
    g_telemetry_enabled = false; /* telemetry disabled by default */
}

void shutdown() {
    g_telemetry_enabled = false;
}

void set_enabled(bool enabled) {
    g_telemetry_enabled = enabled;
}

bool is_enabled() {
    return g_telemetry_enabled;
}

void track_event(const std::string& event_name, const std::string& category) {
    (void)event_name;
    (void)category;
    /* no-op: telemetry data not collected */
}

void track_metric(const std::string& metric_name, double value) {
    (void)metric_name;
    (void)value;
}

void track_timing(const std::string& operation, uint64_t duration_ms) {
    (void)operation;
    (void)duration_ms;
}

void track_error(const std::string& error_type, const std::string& message) {
    (void)error_type;
    (void)message;
}

void flush() {
    /* no-op */
}

} // namespace telemetry
} // namespace rawrxd
