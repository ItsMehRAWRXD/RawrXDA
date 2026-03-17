# RawrXD Recovery Logs - Solutions Reference Guide

**Quick lookup guide for solutions documented in recovery logs**

---

## Common Build Issues & Solutions

### Issue: Qt6 Path Not Found
**Log:** Recovery 7  
**Solution:** Set `Qt6_DIR` environment variable to your Qt6 installation:
```powershell
$env:Qt6_DIR = "C:\path\to\Qt\6.x\msvc2022_64"
cmake -DQt6_DIR=$env:Qt6_DIR ..
```

### Issue: GGUF Parser Conflicts
**Log:** Recovery 1-3  
**Solution:** Check CMakeLists.txt for duplicate tokenizer definitions. Ensure only ONE of each:
- `bpe_tokenizer.cpp/hpp`
- `sentencepiece_tokenizer.cpp/hpp`
- `vocabulary_loader.cpp/hpp`

### Issue: Compilation Errors (Template Instantiation)
**Log:** Recovery 1  
**Solution:** Use `d3d10effect.h` stub header:
```cpp
// In src/win32app/win32_ide.cpp
#pragma once
// Add forward declarations instead of full includes
namespace D3D10 { class Effect; }
```

### Issue: MSVC vs MinGW Mixing
**Log:** Recovery 6  
**Solution:** Use MSVC only for Qt builds:
```cmake
set(CMAKE_CXX_COMPILER "cl.exe")  # Visual Studio compiler
set(CMAKE_C_COMPILER "cl.exe")
```

---

## Model Loading Solutions

### Loading GGUF Models with GPU Backend
**Log:** Recovery 8  
**Solution:** Use `advanced_model_queue.cpp`:
```cpp
bool AdvancedModelQueue::loadModelWithGPU(const QString& path) {
    auto backend = GPUBackend::getInstance();
    auto memInfo = backend->loadModel(path);
    return memInfo.available > 0;
}
```

### Getting Actual GPU Memory Usage
**Log:** Recovery 8  
**Solution:** Replace TODO with actual GPU query:
```cpp
// Before (stubbed):
info.memoryUsage = 0; // TODO

// After (production):
info.memoryUsage = m_gpuBackend->getModelMemory(path);
```

---

## Quantization & Inference

### Understanding Q2_K vs Q4_K Performance
**Log:** Recovery 10  
**Metrics:**
- Q4_K: **514 M elements/sec** ⭐ Winner for inference
- Q2_K: **432 M elements/sec** (18.8% slower)
- Decision: Use Q4_K for real-time inference, Q2_K for storage

### Implementing Token Generation
**Log:** Recovery 11  
**Code:**
```cpp
std::vector<int> InferenceEngine::generate(
    const std::vector<int>& prompt,
    int maxTokens,
    float temperature,
    float topP
) {
    for (int i = 0; i < maxTokens; ++i) {
        int token = sampleToken(temperature, topP);
        if (token == EOS_TOKEN) break;
        result.push_back(token);
    }
    return result;
}
```

### Q2_K Dequantization Pipeline
**Log:** Recovery 13  
**Block Structure:**
```cpp
struct Q2K_Block {
    uint8_t scales[16];      // Scale factors
    uint8_t quants[32];      // 2-bit quantized values
    float d;                 // Delta
    float dmin;              // Min delta
} __attribute__((packed));  // Size: 84 bytes
```

---

## Agentic System

### 27 Agentic Capabilities
**Log:** Recovery 15  
**Categories:**
1. **File Operations** (5)
   - ReadFile, WriteFile, CreateFile, DeleteFile, SearchFile

2. **Code Analysis** (8)
   - Refactor, Analyze, FindBugs, GenerateTests, Document, Format, FindUsages, AutoComplete

3. **Multi-File Operations** (4)
   - EditMultiple, SemanticSearch, BatchRefactor, GlobalRename

4. **Advanced** (10)
   - PluginGeneration, SchemaValidation, ConfigGeneration, EnvSetup, DependencyAnalysis, PerformanceTuning, SecurityAudit, ComplianceCheck, ArchitectureReview, TestGeneration

### Hotpatching 3-Tier Architecture
**Log:** Recovery 16  
**Layers:**
1. **Memory Layer** - Live RAM modification via `ModelMemoryHotpatch`
2. **Byte Layer** - Persistent patches via `ByteLevelHotpatcher`
3. **Server Layer** - Protocol transforms via `GGUFServerHotpatch`

### Self-Correction Loop
**Log:** Recovery 16  
```cpp
// Failure Detection → Self-Correction → Memory Learning
AgenticFailureDetector detector;
AgenticSelfCorrector corrector;
AgenticMemoryModule memory;

if (detector.detect(inferenceResult)) {
    corrector.fix(inferenceResult);
    memory.learn(originalInput, correctedOutput);
}
```

---

## GPU Acceleration

### Installing ROCm on Windows WSL
**Log:** Recovery 21-23  
**Quick Steps:**
```powershell
# 1. Run as Administrator
$env:Path += ";$env:SystemRoot\System32\wsl.exe"

# 2. Run the installer
E:\Install-ROCm-WSL.ps1

# 3. Or manual install in WSL:
wsl
sudo apt update
sudo apt install -y amdgpu-install
sudo amdgpu-install -y --usecase=wsl,rocm --no-dkms
rocm-smi  # Verify
```

### Debugging WSL ROCm Issues
**Log:** Recovery 22  
**Common Fixes:**
```bash
# Fix CRLF line endings
dos2unix /path/to/script.sh

# Fix systemd issues
sudo service wsl-systemd start

# Verify GPU detection
rocm-smi
hip-smi
```

### GPU Backend Fallback
**Log:** Recovery 8  
```cpp
bool useGPU = GPUBackend::isAvailable();
if (!useGPU) {
    // Fall back to CPU inference
    useCPUBackend();
    LOG_WARNING("GPU not available, using CPU");
}
```

---

## Testing & Validation

### Running the Self-Test Harness
**Log:** Recovery 4  
```cpp
SelfTest tests;
if (!tests.runAll()) {
    qFatal("Self-test failed: %s", tests.lastError().toStdString().c_str());
    return 1;  // Gate the release
}
```

### Agentic Validation Testing
**Log:** Recovery 18-19  
```powershell
# Test framework
.\Test-Agentic-Models.ps1 -Model "bigdaddyg:latest"
.\Test-Agentic-Execution.ps1 -Verbose
.\Compare-Agentic-vs-Execution.ps1
```

### Benchmarking Models
**Log:** Recovery 10  
```bash
./bench_q2k_vs_q4k_e2e.exe 2000   # Small
./bench_q2k_vs_q4k_e2e.exe 5000   # Medium
./bench_q2k_vs_q4k_e2e.exe 10000  # Large (stable)
```

---

## Copilot Instructions

### Generated Instructions Location
**Log:** Recovery 6  
**File:** `.github/copilot-instructions.md`
**Size:** 252 lines
**Sections:**
- Project overview
- Build system
- Architecture
- Development workflows
- Common build issues
- Coding conventions
- API & integration
- Performance characteristics
- Special considerations

### Using Copilot Instructions
```markdown
# In .github/copilot-instructions.md:
- Document major architecture decisions
- List specific build commands
- Reference key files by path
- Include real code examples
- Document project-specific patterns
- Avoid generic advice
```

---

## Zero-Touch Improvement System

### Auto-Trigger Mechanisms
**Log:** Recovery 9  
**4 Types:**
1. **File System Watcher** - Rebuilds on code change
2. **Post-Commit Git Hook** - Runs tests automatically
3. **Windows Speech-to-Clipboard** - Voice commands
4. **CI Cron Job** - Scheduled validation

### Enabling Auto-Improvement
```powershell
# Set environment variable
$env:RAWRXD_AUTO_IMPROVE = "1"

# Or via config file
$configPath = "~/.rawrxd/auto-improve.json"
@{ enabled = $true } | ConvertTo-Json | Set-Content $configPath
```

---

## Performance Optimization

### Hybrid Quantization Benefits
**Log:** Recovery 14  
**Improvements over Q2_K:**
- **Size:** 15% smaller
- **Speed:** 8% faster
- **Quality:** Better preservation

### Zone-Based Streaming
**Log:** Recovery 14  
**Concept:**
- Stream model weights in zones
- Process in parallel on GPU
- Reduces latency
- Improves throughput

---

## Emergency Troubleshooting

### Build Completely Failing
**Log:** Recovery 1, 7  
**Steps:**
1. Delete `build/` directory
2. Delete `CMakeCache.txt`
3. Verify all dependencies installed
4. Run CMake fresh: `cmake -DQt6_DIR=... ..`
5. Build: `cmake --build . --config Release`

### Models Not Loading
**Log:** Recovery 8  
**Checklist:**
- [ ] GPU backend initialized
- [ ] GGUF file path correct
- [ ] Model format supported (v3 or v4)
- [ ] Disk space available
- [ ] Memory sufficient

### Inference Very Slow
**Log:** Recovery 10  
**Options:**
- Use Q4_K instead of Q2_K
- Enable GPU acceleration
- Use hybrid quantization
- Profile with benchmarks

---

## Reference Files by Topic

| Topic | Best Log | Key Files |
|-------|----------|-----------|
| Build System | Log 7 | CMakeLists.txt |
| Quantization | Log 12-13 | quantization_*.cpp |
| Model Loading | Log 8 | advanced_model_queue.cpp |
| Inference | Log 11 | inference_engine.cpp |
| GPU | Log 21-23 | gpu_backend.hpp |
| Agentic | Log 15 | agentic_orchestrator.cpp |
| Testing | Log 18-19 | Test-Agentic-*.ps1 |
| Benchmarks | Log 10 | bench_q2k_vs_q4k_e2e.cpp |

---

## One-Liners for Common Tasks

```bash
# Build everything
cd d:\temp\RawrXD-q8-wire\RawrXD-ModelLoader && cmake -DQt6_DIR=... .. && cmake --build . --config Release

# Run tests
SelfTest::runAll()

# Benchmark models
./bench_q2k_vs_q4k_e2e.exe 10000

# Load GPU
GPUBackend::getInstance()->initialize()

# Generate tokens
InferenceEngine::generate(prompt, 256, 0.7f, 0.9f)

# Validate agentic
.\Test-Agentic-Models.ps1 -Verbose
```

---

## Quick Contact Points

**For Build Issues:** See Recovery 1, 7  
**For GPU Issues:** See Recovery 21-23  
**For Performance:** See Recovery 10, 14  
**For Agentic:** See Recovery 15-20  
**For Testing:** See Recovery 4, 18-19  
**For Quantization:** See Recovery 12-14  

---

**Solutions Guide Created:** December 4, 2025  
**Coverage:** All 24 recovery logs  
**Total Solutions:** 50+  
**Status:** Ready for production reference
