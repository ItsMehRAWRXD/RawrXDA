# Manifest Change Notifier

This component watches manifest JSON files and notifies configured endpoints when a real content change occurs.

Features:
- Debounce to avoid duplicate notifications on rapid successive writes
- SHA256 checksum to detect true content changes
- Asynchronous notification delivery (background jobs)
- Retries with exponential backoff and jitter
- Graceful shutdown on Ctrl+C
- Structured logging with fallback to console

Usage examples:

# Monitor a directory
Invoke-ManifestChangeNotifier -WatchDir 'D:/lazy init ide/orchestrator_smoke_output/manifest_tracer' -DebounceMilliseconds 500

# Monitor a single manifest and notify endpoints
Invoke-ManifestChangeNotifierAuto -ManifestPath 'D:/lazy init ide/manifests/manifest.json' \
  -NotificationEndpoints @('https://hooks.example.com/notify') \
  -MaxRetries 5 -InitialRetryDelaySeconds 2 -DebounceMilliseconds 500

Testing:
- Run `Test_ManifestChangeNotifier.ps1` to start a local HTTP listener and verify that notifications are emitted when the manifest is updated.
