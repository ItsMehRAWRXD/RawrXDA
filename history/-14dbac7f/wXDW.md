# Fuzzing and Robustness Tests

This document explains how to run the basic fuzzing harness for Ollama HTTP/JSON parsing and the Ollama bridge.

Prerequisites:
- Python 3.8+
- `nasm` only required to build the native library; the harness works without native lib and will use network mode.

Steps:

1. Generate payloads (optional):

```powershell
python tools\fuzz_generator.py
```

2. Run the network fuzz (sends mutated HTTP requests to host:port):

```powershell
python -m tests.fuzz_ollama --mode net --host 127.0.0.1 --port 11434 --iterations 1000
```

3. Run the bridge fuzz (uses the native library if available):

```powershell
python -m tests.fuzz_ollama --mode bridge --host 127.0.0.1 --port 11434 --iterations 200
```

How it helps:
- Detects server-side parsing errors (unhandled exceptions, timeouts, malformed JSON cases)
- Validates bridge behavior when the native library is available (falls back to Python otherwise)

Notes:
- For Mirai and other research code: run in a sandboxed environment, isolated network.
- This is a lightweight harness; consider integrating a proper fuzzing engine (e.g., AFL, libFuzzer) for deeper coverage.
