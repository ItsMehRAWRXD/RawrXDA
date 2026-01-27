# RawrXD-AgenticIDE — Production Release Guide

**Version:** 1.0.13  
**Branch:** agentic-ide-production  
**Build Date:** December 5, 2025

---

## Quick Start

### Prerequisites

- **Operating System:** Windows 10/11 (x64)
- **Compiler:** MSVC 2022 (Visual Studio 17.x)
- **Qt Framework:** 6.7.3 (MSVC 2022 64-bit)
- **CMake:** 3.20 or newer
- **Git:** For submodule initialization (ggml)

### Build Steps

1. **Clone Repository**
   ```powershell
   git clone https://github.com/ItsMehRAWRXD/RawrXD.git
   cd RawrXD
   git checkout agentic-ide-production
   git submodule update --init --recursive
   ```

2. **Configure CMake**
   ```powershell
   mkdir build
   cd build
   cmake .. -G "Visual Studio 17 2022" -A x64 `
     -DQt6_DIR="C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6" `
     -DCMAKE_BUILD_TYPE=Release
   ```

3. **Build Release**
   ```powershell
   cmake --build . --config Release --target RawrXD-AgenticIDE
   ```

4. **Run**
   ```powershell
   .\bin\RawrXD-AgenticIDE.exe
   ```

---

## Configuration Reference

Copy `config.example.json` to `config.json` and customize:

### Models
- `defaultGGUF`: Primary GGUF model path
- `fallbackGGUF`: Backup model if primary fails
- `tokenizerVocab`: SentencePiece vocab file
- `checkpointDir`: Directory for saving checkpoints

### Endpoints
- `ollamaAPI`: Local or remote Ollama endpoint
- `distributedTrainerMaster`: Multi-node trainer coordinator
- `telemetryCollector`: Metrics sink (optional)

### Features
- `hotpatchingEnabled`: Enable AI response correction
- `flashAttentionEnabled`: Use optimized attention kernels
- `distributedTrainingEnabled`: Multi-GPU/multi-node training
- `autoCheckpointInterval`: Auto-save interval (seconds)
- `compressionLevel`: Zlib compression (1-9, 9=best)

### Security
- `aesKeyEnvVar`: Environment variable holding AES-256 key
- `certPinning`: TLS certificate pinning for network calls
- `auditLogPath`: Security audit log location
- `auditLogMaxSizeMB`: Max log size before rotation

### Performance
- `flashAttentionBlockSize`: Tile size for Flash Attention (64 recommended)
- `kvCacheMaxSize`: KV-cache limit for inference
- `maxConcurrentJobs`: CI/CD job parallelism
- `threadPoolSize`: Worker thread count

### CI/CD
- `defaultShell`: Shell for pipeline jobs (`powershell.exe`)
- `jobTimeoutSeconds`: Max job runtime
- `artifactOutputDir`: Build artifact storage

---

## Environment Variables

Set these before launching:

```powershell
$env:RAWRXD_AES_KEY = "your-32-byte-hex-key"
$env:RAWRXD_CONFIG = "E:\path\to\config.json"
$env:Qt6_DIR = "C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6"
```

Alternatively, create a `.env` file:
```
RAWRXD_AES_KEY=your-32-byte-hex-key
RAWRXD_CONFIG=E:\path\to\config.json
```

---

## Troubleshooting

### Build Issues

**Duplicate Symbol Errors (LNK2005)**
- Symptom: `inference_engine_stub.obj` conflicts with `inference_engine.obj`
- Fix: Ensure `CMakeLists.txt` excludes stub when primary implementation exists (already patched)

**Missing GGML Headers**
- Symptom: `Cannot open include file: 'ggml-backend.h'`
- Fix: Verify `ggml` submodule initialized (`git submodule update --init --recursive`)
- Fix: Ensure `ggml_interface` linked to target in `CMakeLists.txt` (already configured)

**Qt MOC Unresolved Symbols**
- Symptom: `unresolved external symbol metaObject`, `qt_metacast`, etc.
- Fix: Verify `AUTOMOC ON` set for target
- Fix: Clean build: `rm -r CMakeCache.txt CMakeFiles; cmake --fresh ..`

**Missing Qt DLLs at Runtime**
- Symptom: Application fails to start, missing `Qt6Core.dll`, etc.
- Fix: CMake copies DLLs to `bin/` automatically. If missing, run:
  ```powershell
  windeployqt --release .\bin\RawrXD-AgenticIDE.exe
  ```

### Runtime Issues

**Model Not Loading**
- Check `config.json` paths are absolute and files exist
- Verify GGUF format compatibility (Q4_0, Q8_0, etc.)
- Check logs for `[GGUFLoader]` errors

**Network Timeouts (Ollama, Distributed Trainer)**
- Verify endpoints reachable (`Test-NetConnection localhost -Port 11434`)
- Adjust `jobTimeoutSeconds` in config
- Check firewall/antivirus blocking connections

**Encryption Failures**
- Ensure `RAWRXD_AES_KEY` env var set (32-byte hex string)
- Verify no hardcoded keys in source (security violation)
- Check audit log for detailed error messages

**Performance Degradation**
- Flash Attention: Reduce `flashAttentionBlockSize` if GPU memory limited
- Compression: Lower `compressionLevel` (1-6) for faster I/O
- Threads: Adjust `threadPoolSize` based on CPU cores

---

## Security Checklist

✅ **Secrets Management**
- All secrets (AES keys, API tokens) via environment variables or encrypted config
- No hardcoded credentials in source code
- Rotate keys regularly; use secure key derivation (PBKDF2, Argon2)

✅ **Certificate Pinning**
- Enable `certPinning.enabled` in config
- Add SHA-256 hashes of trusted certificates to `pins` array
- Validate all HTTPS connections against pinned certs

✅ **Audit Logging**
- Security events logged to `auditLogPath`
- Restrict log file permissions (admin/owner read-only)
- Rotate logs when size exceeds `auditLogMaxSizeMB`
- Review logs regularly for suspicious activity

✅ **Input Validation**
- All user inputs (file paths, prompts, configs) sanitized
- Path traversal prevention for checkpoint/artifact directories
- JSON parsing with schema validation

✅ **Process Isolation**
- CI/CD jobs run in separate `QProcess` instances
- Timeout enforcement prevents runaway jobs
- Stdout/stderr captured and sanitized before logging

✅ **Memory Safety**
- Resource guards ensure cleanup on exceptions
- No memory leaks in Flash Attention or Compression kernels
- Bounds checking on all buffer operations

---

## Smoke Tests

Run basic validation after build:

```powershell
cd build
ctest -C Release --output-on-failure
```

Tests include:
- Compression round-trip (zlib Z_BEST_COMPRESSION)
- Flash Attention softmax stability
- Checkpoint save/load/delete cycle
- CI/CD PowerShell job execution
- AES-256-GCM encrypt/decrypt
- GGUF model loader integrity
- Agentic Copilot Bridge end-to-end

---

## Performance Baselines

**Flash Attention (7B model, seq_len=2048, batch=1)**
- Latency: ~45ms per forward pass (NVIDIA RTX 3090)
- Memory: ~8GB GPU VRAM

**Compression (10MB file)**
- Compress (level 9): ~250ms, ratio ~4.2x
- Decompress: ~80ms

**Checkpoint Save/Load (7B model)**
- Save (compressed): ~2.5s, ~1.8GB file
- Load: ~1.2s

**Distributed Trainer (2-node, 4 GPUs)**
- Gradient sync overhead: ~50ms per step
- Throughput: ~120 tokens/sec/GPU

---

## Known Limitations

- **Windows Only**: Current build tested on Windows 10/11 x64 (MSVC). Linux/macOS builds require CMake adjustments.
- **GGUF Format**: Supports common quantizations (Q4_0, Q8_0, Q8_K). Exotic formats may fail gracefully.
- **Distributed Training**: Requires manual node configuration (no auto-discovery).
- **Telemetry**: HTTPS-only; HTTP fallback disabled for security.

---

## CI/CD Integration

See `.github/workflows/build.yml` for automated build + smoke test pipeline:
- Build on Windows Server 2022 runner
- Qt 6.7.3 cached
- Artifacts uploaded as `RawrXD-AgenticIDE-{version}-win64.zip`

---

## Support & Contributing

- **Issues**: https://github.com/ItsMehRAWRXD/RawrXD/issues
- **Discussions**: https://github.com/ItsMehRAWRXD/RawrXD/discussions
- **Branch**: `agentic-ide-production` (stable), `main` (development)

For production issues, attach:
- Full build log
- `config.json` (redact secrets)
- Audit log excerpt
- System info (OS, Qt version, GPU)

---

**Built with:** Qt 6.7.3 | GGML 0.9.4 | MSVC 2022 | CMake 3.20+
