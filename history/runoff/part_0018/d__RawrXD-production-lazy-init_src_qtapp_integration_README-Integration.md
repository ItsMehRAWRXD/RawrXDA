# Integration Framework (Production Readiness)

Non-invasive, header-only integration points for observability, configuration, and resource management without touching core logic.

## Modules

### ProdIntegration.h
Core primitives: `Config`, `ScopedTimer`, `logInfo()`, `recordMetric()`, `traceEvent()`.

### InitializationTracker.h
Track subsystem startup order and latency; `ScopedInitTimer` auto-records constructor time.

### Logger.h
Unified structured logging with chainable API.

### Diagnostics.h
Runtime introspection: JSON reports, human-readable summaries of initialization events.

### ResourceGuards.h
Optional RAII wrappers for resource cleanup: `ResourceGuard<T>`, `ScopedAction`.

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `RAWRXD_LOGGING_ENABLED` | Enable structured logging |
| `RAWRXD_LOG_STUBS` | Log stub widget construction |
| `RAWRXD_ENABLE_METRICS` | Emit metric events |
| `RAWRXD_ENABLE_TRACING` | Emit trace events |

All default to disabled (zero overhead).

## Instrumented Components

- **Subsystems.h**: Stub widget macro records constructor latency and logs.
- **VersionControlWidget**: Constructor + `refresh()` method instrumented.
- **BuildSystemWidget**: Constructor + `startBuild()` method instrumented.

## Documentation

See `INTEGRATION-GUIDE.md` for comprehensive usage examples, patterns, and best practices.

## Notes

- Header-only design: No compilation overhead, pure opt-in via env vars.
- Zero cost when disabled: Environment checks compile out in release builds.
- Production-safe: All logging respects configuration; no side effects without explicit opt-in.

### Feature Flags
- `featureEnabled(key, defaultOn)` – Check `RAWRXD_FEATURE_<key>` env flag.

### Fault Tolerance
- `ResourceGuard<Fn>` – RAII wrapper ensuring cleanup.
- `retryWithBackoff(fn, maxRetries, initialDelayMs)` – Retry with exponential backoff.
- `CircuitBreaker` – Opens after repeated failures to prevent cascading issues.

### Health Checks
- `HealthCheck` struct with `markReady()`, `markUnhealthy()`, and `toJson()`.

## Usage Example
```cpp
#include "integration/ProdIntegration.h"
using namespace RawrXD::Integration;

void doWork() {
    ScopedTimer timer("MyComponent", "doWork", "execute");
    recordMetric("doWork_calls");
    // ... existing logic
}

auto result = retryWithBackoff([]{ return fetchData(); }, 3, 200);

CircuitBreaker cb(5, 30000);
if (cb.allowRequest()) {
    try { externalCall(); cb.recordSuccess(); }
    catch (...) { cb.recordFailure(); }
}

HealthCheck hc;
hc.markReady("All systems nominal");
```

## Notes
- These hooks are no-ops unless enabled via env variables.
- They are designed to be replaced by real metrics/tracing implementations later without code churn.
