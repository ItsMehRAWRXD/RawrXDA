# RawrXD Instrumentation

This folder provides non-intrusive instrumentation utilities for observability, configuration, and error capture without modifying core logic.

## Files
- `Logging.h`: RAII `ScopedTimer` and helpers using `QLoggingCategory`.
- `Metrics.h`: Lightweight metrics scaffolding; logs counters and duration when enabled.
- `Config.h`: Environment and `QSettings` access with `featureEnabled` toggles.
- `ErrorCapture.h`: Wrapper to capture and log exceptions with timing.

## Configuration
- `RAWRXD_ENABLE_METRICS`: Set to `1`/`true`/`on` to enable metrics logging.
- `RAWRXD_FEATURE_<FeatureKey>`: Feature flags (e.g., `RAWRXD_FEATURE_Telemetry=on`).
- User INI settings path: `QSettings(QSettings::IniFormat, UserScope, "RawrXD", "RawrXD")`.

## Usage
```cpp
#include "instrumentation/Logging.h"
#include "instrumentation/Metrics.h"
#include "instrumentation/ErrorCapture.h"

void doWork() {
    QLoggingCategory cat("rawrxd.example");
    RAWRXD_SCOPED_TIMER("doWork", cat);
    rawrxd::metrics::Increment("doWork_calls");
    // ... existing logic
}

auto res = rawrxd::safety::captureErrors([](){ /* call existing complex function */ return 42; }, "complexOperation");
```

These utilities are header-only and disabled by default unless feature flags or env variables are set, ensuring no impact on existing behavior.
