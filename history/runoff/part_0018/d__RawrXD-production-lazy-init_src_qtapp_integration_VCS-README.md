# VCS Integration Utilities

Header-only utilities for non-intrusive observability and fault tolerance in VCS operations.

## Files
- `VcsIntegration.h` – Git command runner, process/watcher guards, repository health tracking.

## Components

### GitCommandRunner
- Wraps git commands with **retry** (exponential backoff) and **circuit breaker**
- Logs structured events for success/failure
- Tracks metrics (success/failure counts)
- **Usage:**
  ```cpp
  using namespace RawrXD::Integration::Vcs;
  GitCommandRunner runner("/path/to/repo");
  
  auto result = runner.status();
  if (result.success) { /* ... */ }
  
  // With retry
  auto pullResult = runner.pull(); // Uses default retry config
  ```

### ProcessGuard
- RAII wrapper for `QProcess` with automatic cleanup
- Ensures processes are terminated on destruction
- **Usage:**
  ```cpp
  ProcessGuard guard;
  guard->start("git", {"status"});
  // Automatically cleaned up
  ```

### FileWatcherGuard
- Safe `QFileSystemWatcher` management with logging
- Automatically cleans up watcher on destruction
- **Usage:**
  ```cpp
  FileWatcherGuard watcher;
  watcher.addPath("/path/to/watch");
  watcher.connect(&QFileSystemWatcher::directoryChanged, this, &MyClass::onDirChanged);
  ```

### RepositoryHealth
- Tracks repository health using `HealthCheck`
- Monitors slow operations (configurable threshold)
- Generates health reports in JSON format
- **Usage:**
  ```cpp
  RepositoryHealth health("/path/to/repo");
  health.markOperationSlow("git_status", 6000); // Logs warning if >5s
  
  auto report = health.healthReport(); // JSON health report
  ```

## Environment Variables
- `RAWRXD_LOGGING_ENABLED` – Enable structured logging
- `RAWRXD_LOG_STUBS` – Log guard creation/destruction
- `RAWRXD_ENABLE_METRICS` – Enable metrics for git operations
- `RAWRXD_FEATURE_<Key>` – Feature flags (e.g., `RAWRXD_FEATURE_VcsRetry=on`)

## Integration Points
- Non-invasive: Can wrap existing `QProcess`-based git operations
- Disabled by default: Zero impact unless env flags are set
- Drop-in replacement: Use `GitCommandRunner` instead of direct `QProcess` calls

## Circuit Breaker Behavior
- **Closed:** Normal operation
- **Open:** After 5 failures (configurable), blocks new requests for 30s
- **Half-Open:** After timeout, allows one test request; closes on success

These utilities are designed for production readiness without modifying core VCS logic.
