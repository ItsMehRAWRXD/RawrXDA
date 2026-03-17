# Integration Framework — Extended Usage Guide

This document provides comprehensive guidance on using the optional integration framework for observability and configuration without modifying core logic.

## Module Overview

### ProdIntegration.h
**Purpose**: Core configuration and logging primitives.

**Key Components**:
- `Config`: Environment-based feature toggles
  - `loggingEnabled()` → `RAWRXD_LOGGING_ENABLED`
  - `stubLoggingEnabled()` → `RAWRXD_LOG_STUBS`
  - `metricsEnabled()` → `RAWRXD_ENABLE_METRICS`
  - `tracingEnabled()` → `RAWRXD_ENABLE_TRACING`

- `ScopedTimer`: Measures and logs execution latency
  ```cpp
  {
      RawrXD::Integration::ScopedTimer timer("ComponentName", "ObjectName", "operation");
      // ... work ...
      // Logs latency_ms on destruction
  }
  ```

- `logInfo()`, `recordMetric()`, `traceEvent()`: Lightweight event functions

### InitializationTracker.h
**Purpose**: Track subsystem startup order and latency.

**Key Components**:
- `InitializationTracker::instance()`: Global singleton
- `recordEvent(subsystem, event, latency_ms)`: Log init event
- `ScopedInitTimer`: Auto-record constructor latency
  ```cpp
  ScopedInitTimer init("MyWidget");
  // ...
  // Records init event when destroyed
  ```

### Logger.h
**Purpose**: Unified structured logging with chainable API (optional).

**Key Components**:
- `Logger::info()`, `Logger::debug()`, `Logger::warn()`, `Logger::error()`
- Chainable methods: `.component()`, `.event()`, `.message()`
  ```cpp
  RawrXD::Integration::Logger::info()
      .component("VCS")
      .event("commit")
      .message("User committed changes");
  ```

## Integration Patterns

### Pattern 1: Measuring Constructor Latency
```cpp
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"

MyWidget::MyWidget(QWidget* parent)
    : QWidget(parent) {
    RawrXD::Integration::ScopedInitTimer initTimer("MyWidget");
    // ... rest of constructor ...
}
```

### Pattern 2: Measuring Method Latency
```cpp
void MyWidget::expensiveOperation() {
    RawrXD::Integration::ScopedTimer timer("MyWidget", "op", "expensiveOperation");
    // ... operation ...
    // Logs latency when timer destroyed
}
```

### Pattern 3: Recording Metrics
```cpp
void MyWidget::processItem() {
    RawrXD::Integration::recordMetric("items_processed");
    // ... work ...
}
```

### Pattern 4: Tracing Event Flow
```cpp
void MyWidget::criticalPath() {
    RawrXD::Integration::traceEvent("MyWidget", "criticalPath_start");
    // ... work ...
    RawrXD::Integration::traceEvent("MyWidget", "criticalPath_end");
}
```

## Environment Variables

| Variable | Purpose | Values |
|----------|---------|--------|
| `RAWRXD_LOGGING_ENABLED` | Enable structured logging | `1` (enable), `0` or unset (disable) |
| `RAWRXD_LOG_STUBS` | Log stub widget construction | `1` (enable), `0` or unset (disable) |
| `RAWRXD_ENABLE_METRICS` | Emit metric events | `1` (enable), `0` or unset (disable) |
| `RAWRXD_ENABLE_TRACING` | Emit trace events | `1` (enable), `0` or unset (disable) |

## Running with Integration Enabled

### PowerShell
```powershell
$env:RAWRXD_LOGGING_ENABLED = "1"
$env:RAWRXD_LOG_STUBS = "1"
$env:RAWRXD_ENABLE_METRICS = "1"
$env:RAWRXD_ENABLE_TRACING = "1"
./RawrXD-AgenticIDE.exe
```

### Bash/Linux
```bash
export RAWRXD_LOGGING_ENABLED=1
export RAWRXD_LOG_STUBS=1
export RAWRXD_ENABLE_METRICS=1
export RAWRXD_ENABLE_TRACING=1
./RawrXD-AgenticIDE
```

## Performance Impact

- **When disabled (default)**: Zero overhead — all checks are compile-time constants due to `qEnvironmentVariableIsEmpty()` optimization.
- **When enabled**: Minimal overhead (~1-5% for typical scenarios).

## Migration Path

1. **Phase 1**: Add integration headers to codebase (done).
2. **Phase 2**: Instrument constructors and key methods with `ScopedTimer` (in progress).
3. **Phase 3**: Replace log stub implementations with real metrics/tracing backends (Prometheus, OpenTelemetry).
4. **Phase 4**: Add distributed tracing support across remote calls.

## Real Widget Instrumentation Status

| Widget | Constructor | Key Methods | Notes |
|--------|-------------|-------------|-------|
| `VersionControlWidget` | ✅ Instrumented | `refresh()` ✅ | ScopedInitTimer + ScopedTimer on refresh |
| `BuildSystemWidget` | ✅ Instrumented | `startBuild()` ✅ | ScopedInitTimer + ScopedTimer on startBuild |
| Stub widgets (macro) | ✅ Instrumented | N/A | Macro logs construction via logging/metrics/tracing |
| Others | ❌ Not yet | — | Can be instrumented on-demand |

## Best Practices

1. **Use ScopedTimer for critical paths**: Constructor, method entry/exit.
2. **Keep latency thresholds documented**: "Normal: 10-50ms, Slow: >100ms".
3. **Never instrument inside tight loops**: Record aggregate metrics instead.
4. **Always gate metrics behind env vars**: No performance cost when disabled.
5. **Log early, log often**: More observability = easier debugging in production.

## Troubleshooting

**Issue**: No logs appear despite `RAWRXD_LOGGING_ENABLED=1`
- Verify Qt debug mode is enabled.
- Check that `qDebug()` output is captured in your console/IDE.
- Confirm env var is set in the process, not just parent shell.

**Issue**: Build fails due to missing headers
- Ensure `src/qtapp/integration/` directory exists.
- Add to CMakeLists.txt include directories if not auto-detected.

**Issue**: Performance degrades when enabled
- Disable metrics/tracing (keep only logging).
- Review instrumented methods for excessive calls (e.g., tight loops).
