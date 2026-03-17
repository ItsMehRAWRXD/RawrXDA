# Comprehensive Integration Utilities

Non-intrusive, header-only integration utilities for production readiness across multiple subsystems.

## Overview

These utilities provide observability, fault tolerance, and configuration management without modifying core logic. All features are disabled by default and activated via environment variables.

## Available Modules

### 1. Core Integration (`ProdIntegration.h`)
**Purpose:** Foundational logging, metrics, and fault tolerance utilities.

**Features:**
- Structured JSON logging (`logInfo`, `logDebug`, `logWarn`, `logError`)
- RAII `ScopedTimer` for latency measurement
- `recordMetric` for lightweight metrics
- `traceEvent` for request tracing
- `featureEnabled(key, default)` for feature flags
- `MetricBatch` for aggregated metrics
- `ResourceGuard<Fn>` for cleanup guarantees
- `HealthCheck` for readiness/liveness probes
- `retryWithBackoff(fn, retries, delayMs)` for transient failures
- `CircuitBreaker` for fault isolation

**Usage:**
```cpp
#include "integration/ProdIntegration.h"
using namespace RawrXD::Integration;

void process() {
    ScopedTimer timer("Component", "process", "start");
    recordMetric("process_calls");
    
    auto result = retryWithBackoff([]{ return riskyOperation(); }, 3, 200);
}
```

### 2. VCS Integration (`VcsIntegration.h`)
**Purpose:** Safe Git operations with observability and fault tolerance.

**Features:**
- `GitCommandRunner` with retry and circuit breaker
- `ProcessGuard` RAII for QProcess cleanup
- `FileWatcherGuard` for safe QFileSystemWatcher management
- `RepositoryHealth` for VCS health monitoring

**Usage:**
```cpp
#include "integration/VcsIntegration.h"
using namespace RawrXD::Integration::Vcs;

GitCommandRunner runner("/path/to/repo");
auto result = runner.status();
if (!result.success) {
    // Circuit breaker prevents cascading failures
}
```

### 3. Widget Lifecycle (`WidgetLifecycle.h`)
**Purpose:** Track UI widget creation/destruction for memory monitoring.

**Features:**
- `WidgetLifecycleTracker` singleton
- `WidgetGuard<T>` RAII wrapper
- Creation/destruction counts
- Active widget snapshot

**Usage:**
```cpp
#include "integration/WidgetLifecycle.h"

auto widget = createTrackedWidget<QWidget>("MyWidget", parent);
WidgetLifecycleTracker::instance().activeWidgetCount();
```

### 4. Network Integration (`NetworkIntegration.h`)
**Purpose:** Safe network operations with timeout and retry.

**Features:**
- `NetworkTracker` with configurable timeout
- `HttpClient` convenience wrapper
- Retry with exponential backoff
- Request/response logging

**Usage:**
```cpp
#include "integration/NetworkIntegration.h"
using namespace RawrXD::Integration::Network;

HttpClient client;
auto response = client.get("https://api.example.com/data");
```

### 5. Error Aggregation (`ErrorAggregator.h`)
**Purpose:** Centralized error collection and reporting.

**Features:**
- Structured error entries with severity
- Error aggregation and deduplication
- Thread-safe error storage
- JSON error reports
- `ErrorGuard` RAII wrapper

**Usage:**
```cpp
#include "integration/ErrorAggregator.h"
using namespace RawrXD::Integration::Errors;

recordError("Component", "Operation", "Something failed");
auto errors = getRecentErrors(50);
auto summary = getErrorSummary();
```

## Environment Variables

| Variable | Purpose | Example |
|----------|---------|---------|
| `RAWRXD_LOGGING_ENABLED` | Enable structured logging | `1` or `true` |
| `RAWRXD_LOG_STUBS` | Log stub widget creation | `1` or `true` |
| `RAWRXD_ENABLE_METRICS` | Enable metrics collection | `1` or `true` |
| `RAWRXD_ENABLE_TRACING` | Enable request tracing | `1` or `true` |
| `RAWRXD_FEATURE_<Key>` | Feature toggles | `RAWRXD_FEATURE_Telemetry=on` |

## Configuration Pattern

All utilities support the same pattern:
1. **Opt-in via environment** – disabled by default
2. **Structured JSON output** – machine-parseable logs
3. **Feature flags** – granular control
4. **No external dependencies** – header-only implementations
5. **Zero overhead when disabled** – compile-time and runtime guards

## Integration Points

These utilities are designed to wrap existing code:

| Core Area | Integration Utility | Example |
|-----------|-------------------|---------|
| Git/VCS | `GitCommandRunner` | Replace `QProcess::execute("git status")` |
| UI Widgets | `WidgetGuard<T>` | Wrap `new QWidget()` |
| Network | `HttpClient` | Replace raw `QNetworkAccessManager` |
| Errors | `ErrorGuard` | Wrap try/catch blocks |
| Metrics | `recordMetric` | Instrument key operations |

## Production Readiness Checklist

- ✅ **Observability:** Structured logging, metrics, health checks
- ✅ **Fault Tolerance:** Retry, circuit breaker, resource guards
- ✅ **Configuration:** Environment variables, feature flags
- ✅ **Error Handling:** Centralized error collection, RAII guards
- ✅ **Resource Management:** RAII wrappers, automatic cleanup
- ✅ **Zero Overhead:** Disabled by default, no-op when not configured

## Migration Guide

### Before (Direct QProcess)
```cpp
QProcess git;
git.start("git", {"status"});
git.waitForFinished();
```

### After (With Integration)
```cpp
GitCommandRunner runner(repoPath);
auto result = runner.status();
// Automatic retry, logging, metrics, circuit breaker
```

### Before (Untracked Widgets)
```cpp
auto* widget = new QWidget(parent);
```

### After (Tracked Widgets)
```cpp
auto widget = createTrackedWidget<QWidget>("ComponentWidget", parent);
// Automatic lifecycle tracking and memory monitoring
```

## Best Practices

1. **Wrap External Resources:** Use `ProcessGuard`, `ResourceGuard` for any external resource
2. **Instrument Critical Paths:** Add `ScopedTimer` to expensive operations
3. **Feature Flag New Code:** Use `featureEnabled()` for experimental features
4. **Aggregate Errors:** Use `ErrorAggregator` for consistent error handling
5. **Health Checks:** Add `HealthCheck` to long-running services

## Testing

All utilities can be tested independently:
- Enable logging: `export RAWRXD_LOGGING_ENABLED=1`
- Enable metrics: `export RAWRXD_ENABLE_METRICS=1`
- Test feature flags: `export RAWRXD_FEATURE_TestFeature=on`

Verify with structured output:
```bash
./RawrXD-AgenticIDE 2>&1 | jq '.'
```

## Notes

- **Thread Safety:** All utilities are thread-safe where noted
- **Qt Compatibility:** Requires Qt 5.12+ for JSON and threading features
- **Extensibility:** Utilities are designed to be extended with real implementations later
- **No Code Churn:** Drop-in replacements that don't change calling code
