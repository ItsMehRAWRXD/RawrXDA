# Item #1: Automated Fuzzing & Robustness Tests
## Quick Start Implementation Guide

**Target Status:** `not-started` → `in-progress`  
**Estimated Duration:** 3-5 days  
**Complexity:** High (requires crash detection, mutation testing, coverage tracking)

---

## Overview

This implementation expands the existing fuzzing harness (`tools/fuzz_ollama.py`) with:
1. **Mutation testing framework** - Generate malformed payloads
2. **Crash detection engine** - Identify segfaults and hangs
3. **Coverage tracking** - Measure code path coverage
4. **Regression database** - Store and deduplicate crashes
5. **CI/CD integration** - Automated fuzzing in build pipeline

---

## Phase 1: Mutation Testing Framework

### Files to Create
- `tools/fuzzer_mutations.py` - Mutation strategy library
- `tools/fuzzer_payloads.py` - Payload generation
- `tests/test_fuzzer_mutations.py` - Unit tests

### Mutation Strategies

```python
# Byte-level mutations
- BitFlip(max_flips=10)          # Flip random bits
- IntOverflow()                   # Test integer boundaries
- BufferOverflow(sizes=[1KB, 64KB, 1MB])  # Large payloads
- UTF8Invalid()                   # Invalid Unicode sequences

# Protocol-level mutations
- MalformedJSON()                 # Truncated, nested, circular refs
- HTTPHeaderFuzzed()              # Missing/invalid headers
- PartialReceive()                # Incomplete frames
- TimeoutInjection()              # Simulated timeouts

# Adversarial payloads
- CompressionBomb()               # Zip bombs (decompression attacks)
- RecursiveStructures()           # Circular JSON references
- LargeFieldValues()              # Excessively long strings
```

### Implementation Steps

1. **Mutation Engine** (`fuzzer_mutations.py`)
   ```python
   class Mutator:
       def bitflip(data, num_flips)
       def byteflip(data, num_flips)
       def int_overflow(data)
       def utf8_corrupt(data)
       def json_malform(obj)
       def generate_batch(template, mutations=100)
   ```

2. **Payload Generator** (`fuzzer_payloads.py`)
   ```python
   def generate_http_response(malformed=False)
   def generate_json_payload(mutation_type)
   def generate_adversarial_packet()
   ```

3. **Unit Tests** (verify mutations don't crash generator)
   ```bash
   pytest tests/test_fuzzer_mutations.py -v
   ```

---

## Phase 2: Crash Detection Engine

### Files to Create
- `tools/crash_detector.py` - Crash detection harness
- `tools/crash_database.py` - Crash storage/deduplication
- `src/crash_handler.asm` - Exception handlers (optional)

### Crash Detection Mechanisms

```python
# Signal handling (Linux)
signal.signal(signal.SIGSEGV, crash_handler)
signal.signal(signal.SIGABRT, crash_handler)

# Exception handling (Windows)
try:
    bridge.execute(malformed_payload)
except OSError as e:
    if "segmentation" in str(e):
        log_crash(payload, stack_trace)

# Timeout detection
timeout_seconds = 5
signal.alarm(timeout_seconds)  # Or ThreadPoolExecutor with timeout
```

### Crash Deduplication

```python
class CrashDatabase:
    def add_crash(payload, stack_trace, registers):
        # Hash on:
        # 1. Crash type (SIGSEGV, assertion, etc.)
        # 2. Instruction pointer (RIP)
        # 3. Stack trace signature
        # Deduplicate identical crashes
        
    def find_root_cause(crash_signature):
        # Minimal reproducible example (delta debugging)
        # Binary search on payload to find minimal trigger
```

### Success Metrics
- Detect 100% of crashes in existing test suite
- <100ms detection latency
- <5% false positive rate

---

## Phase 3: Coverage Tracking

### Files to Create
- `tools/coverage_tracker.py` - Instrumentation controller
- `src/coverage_hooks.asm` - Basic block counters (optional)
- `tests/coverage_report.py` - Report generation

### Coverage Implementation

**Option A: LLVM Coverage (if rebuilding with LLVM)**
```bash
# Requires: clang -fprofile-instr-generate -fcoverage-mapping
clang -fprofile-instr-generate -fcoverage-mapping \
  src/ollama_native.asm -o build/ollama_native_cov.obj
```

**Option B: Instrumentation via Bridge (Python)**
```python
# Patch HTTP parser to log entry/exit points
class CoverageInstrumenter:
    def add_trace_points(bridge_module):
        # Inject logging at function boundaries
        # Track which code paths are executed
        
    def generate_coverage_map():
        # Map coverage data to source lines
```

**Option C: Sampling (Lightweight)**
```python
# Use debugger hooks to periodically capture RIP
# Build histogram of instruction pointers
# Map to assembly code regions
```

### Coverage Report
```markdown
# Coverage Report

- Total code: 2,317 lines (ollama_native.asm)
- Covered: 2,100 lines (90.6%)
- Uncovered: 217 lines (9.4%)

## Uncovered Regions

1. Error handling for recv timeout (Linux)
   - Reason: Test environment lacks Ollama server
   - Recommendation: Mock server or isolated test

2. Windows-specific socket options
   - Reason: Running on Linux CI
   - Recommendation: Platform-specific test matrix
```

---

## Phase 4: Regression Database

### Files to Create
- `tools/crash_registry.json` - Crash catalog
- `tools/regression_manager.py` - Tracking and reporting

### Schema

```json
{
  "crashes": [
    {
      "id": "crash_001",
      "type": "SIGSEGV",
      "location": "parse_models_json:0x1f3a",
      "timestamp": "2025-11-22T10:30:45Z",
      "payload_hash": "a1b2c3d4",
      "minimal_payload": "...",
      "stack_trace": [...],
      "fixed": false,
      "fix_commit": null
    }
  ]
}
```

### Regression Detection

```python
def is_regression(crash_id):
    # Check if crash_id was previously fixed
    # Alert if same crash re-appears
    
def track_fix(crash_id, fix_description, commit_hash):
    # Mark crash as fixed, link to fix
```

---

## Phase 5: CI/CD Integration

### Files to Create
- `.github/workflows/fuzz.yml` - GitHub Actions workflow
- `tools/fuzz_runner.sh` - Orchestration script

### Workflow Example

```yaml
name: Continuous Fuzzing

on:
  push:
    branches: [main, develop]
  pull_request:
  schedule:
    - cron: '0 */4 * * *'  # Every 4 hours

jobs:
  fuzz:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: nasm -f win64 src/ollama_native.asm -o build/ollama_native.obj
      - name: Run Fuzzer
        run: python -m tools.fuzzer_advanced \
          --iterations 100000 \
          --timeout 30 \
          --report build/fuzz_report.html
      - name: Upload Report
        uses: actions/upload-artifact@v3
        if: always()
        with:
          name: fuzz-report
          path: build/fuzz_report.html
      - name: Check Regressions
        run: python tools/regression_manager.py check
```

---

## Implementation Timeline

### Day 1-2: Mutation Testing
- [ ] Implement `fuzzer_mutations.py` with 8+ mutation strategies
- [ ] Write unit tests
- [ ] Generate first batch of mutated payloads

### Day 2-3: Crash Detection
- [ ] Implement `crash_detector.py`
- [ ] Set up exception/signal handling
- [ ] Deduplication logic

### Day 3-4: Coverage & Regression
- [ ] Implement coverage tracking (Option B or C)
- [ ] Build crash registry
- [ ] Regression detection

### Day 4-5: CI/CD & Reporting
- [ ] GitHub Actions workflow
- [ ] Coverage reports (HTML)
- [ ] Regression alerts

---

## Testing Strategy

### Unit Tests
```bash
pytest tests/test_fuzzer_mutations.py
pytest tests/test_crash_detector.py
pytest tests/test_coverage_tracker.py
```

### Integration Tests
```bash
# Run fuzzer on existing test suite
python -m tools.fuzzer_advanced --mode bridge --iterations 1000

# Verify no new crashes in clean build
python tools/regression_manager.py baseline
```

### Smoke Tests
```bash
# Quick 30-second fuzz run
python -m tools.fuzzer_advanced --iterations 100 --timeout 30
```

---

## Success Criteria

- [ ] **Mutation Coverage:** >90% of HTTP parser code paths exercised
- [ ] **Crash Detection:** 100% recall (catches all crashes)
- [ ] **False Positives:** <1% (avoid noise)
- [ ] **Performance:** <100ms per crash detection
- [ ] **Database:** Zero duplicate crashes (deduplication works)
- [ ] **CI/CD:** Fuzzing runs on every push, reports generated
- [ ] **Documentation:** Usage guide and results interpretable

---

## Deliverables Checklist

- [ ] `tools/fuzzer_mutations.py` (300+ lines, 8+ mutation strategies)
- [ ] `tools/crash_detector.py` (200+ lines, deduplication)
- [ ] `tools/coverage_tracker.py` (150+ lines)
- [ ] `tools/regression_manager.py` (200+ lines, crash registry)
- [ ] `tests/test_fuzzer_*.py` (300+ lines combined)
- [ ] `.github/workflows/fuzz.yml` (CI/CD integration)
- [ ] `docs/FUZZING_GUIDE.md` (Usage documentation)
- [ ] Sample reports and regression database

---

## Known Constraints

1. **Ollama Server:** Not required for network-mode fuzzing (uses WinSock API directly)
2. **Platform:** Primary testing on Windows (PLATFORM_WIN define)
3. **Coverage:** Full LLVM coverage requires rebuild; sampling alternative available
4. **Time:** Comprehensive fuzzing (100K+ iterations) may take hours

---

## Resources & References

- AFL/libFuzzer: https://llvm.org/docs/LibFuzzer/
- Hypothesis: https://hypothesis.readthedocs.io/ (Python mutation testing)
- Delta Debugging: https://www.st.cs.uni-saarland.de/dd/
- OWASP Fuzzing: https://owasp.org/www-project-web-security-testing-guide/

---

## Next Phase

After completing Item #1:
- Proceed to **Item #2: Security Audit & Sandboxing**
- Use fuzzer results to inform audit scope
- Focus on crashes found by fuzzer

---

**Status:** Ready to begin implementation  
**Contact:** AI Coding Agent  
**Last Updated:** November 22, 2025
