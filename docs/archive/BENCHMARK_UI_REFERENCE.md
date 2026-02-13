# RawrXD IDE Benchmark Menu - Visual Reference

## Menu Structure

```
┌─ File
│
├─ Edit
│
├─ View
│
├─ Tools ◄─────────────────────────┐
│  ├─ Build                        │
│  ├─ Benchmarks ◄───────────┐    │
│  │  ├─ Run Benchmarks...    │    │ NEW
│  │  ├─ ─────────────────    │    │ FEATURES
│  │  └─ View Results         │    │
│  └─ ...                     │    │
│                             │    │
└─ Help                       │    │
                              │    │
                              └────┘
```

## Dialog Window Layout

### Full Dialog (1000x700 pixels)

```
╔═══════════════════════════════════════════════════════════════════════════╗
║ RawrXD Benchmark Suite                                             [_][□][X]║
╠═══════════════════════════════════════════════════════════════════════════╣
║                                                                           ║
║  ┌─────────────────────────┐  ┌──────────────────────────────────────┐  ║
║  │  LEFT PANEL             │  │  RIGHT PANEL                         │  ║
║  │                         │  │                                      │  ║
║  │  ┌───────────────────┐  │  │  ┌────────────────────────────────┐ │  ║
║  │  │ BENCHMARK TESTS   │  │  │  │ Benchmark Output:              │ │  ║
║  │  ├───────────────────┤  │  │  ├────────────────────────────────┤ │  ║
║  │  │ ☑ Cold Start      │  │  │  │ [14:23:45] ✓   RawrXD Suite   │ │  ║
║  │  │ ☑ Warm Cache      │  │  │  │ [14:23:45] INFO ═══════════════│ │  ║
║  │  │ ☑ Rapid Fire      │  │  │  │ [14:23:46] INFO Model: ...    │ │  ║
║  │  │ ☑ Multi-Language  │  │  │  │ [14:23:47] INFO Running tests  │ │  ║
║  │  │ ☑ Context Aware   │  │  │  │ [14:23:50] ✓   ✅ PASS cold...│ │  ║
║  │  │ ☑ Multi-Line      │  │  │  │ [14:23:52] ✓   ✅ PASS warm...│ │  ║
║  │  │ ☑ GPU Accel       │  │  │  │ [14:24:01] ✓   ✅ PASS rapid..│ │  ║
║  │  │ ☑ Memory          │  │  │  │ [14:24:15] ⏳  Running: multi..│ │  ║
║  │  │                   │  │  │  │                                │ │  ║
║  │  │ [Select All]      │  │  │  │ ↓ (scrolls as output arrives)  │ │  ║
║  │  │ [Deselect All]    │  │  │  └────────────────────────────────┘ │  ║
║  │  └───────────────────┘  │  │                                      │  ║
║  │                         │  │  ┌────────────────────────────────┐ │  ║
║  │  ┌───────────────────┐  │  │  │ Results Summary:               │ │  ║
║  │  │ CONFIGURATION     │  │  │  ├────────────────────────────────┤ │  ║
║  │  ├───────────────────┤  │  │  │ ✅ Cold Start - 125.5ms        │ │  ║
║  │  │ Model: [────────▼]│  │  │  │ ✅ Warm Cache - 18.7ms         │ │  ║
║  │  │                   │  │  │  │ ✅ Rapid Fire - 45.2ms         │ │  ║
║  │  │ ☑ GPU Accel.      │  │  │  │ ✅ Multi-Lang - 52.1ms         │ │  ║
║  │  │ ☐ Verbose Output  │  │  │  │ ⏳ Context - Running...        │ │  ║
║  │  │                   │  │  │  │ ⏳ Multi-Line - Running...     │ │  ║
║  │  └───────────────────┘  │  │  │ ⏳ GPU Accel - Running...      │ │  ║
║  │                         │  │  │ ⏳ Memory - Running...          │ │  ║
║  │  ┌───────────────────┐  │  │  └────────────────────────────────┘ │  ║
║  │  │ [Run Benchmarks]  │  │  │                                      │  ║
║  │  │ [Stop]            │  │  │  Progress: [██████████░░░░] 6/8     │  ║
║  │  └───────────────────┘  │  │                                      │  ║
║  │                         │  │                                      │  ║
║  └─────────────────────────┘  └──────────────────────────────────────┘  ║
║                                                                           ║
╚═══════════════════════════════════════════════════════════════════════════╝
```

## Color Scheme

### Log Colors (Solarized Dark)

```
Timestamp       Color           Foreground
────────────────────────────────────────────
[14:23:45]      Standard        #d4d4d4 (white)
✓ SUCCESS       Green           #6a9955 (bright green)
INFO            Blue            #569cd6 (bright blue)
⚠ WARNING       Yellow          #dcdcaa (bright yellow)
✗ ERROR         Red             #f48771 (bright red)
⚙ DEBUG         Gray            #808080 (dim gray)
```

### Background

```
Editor Theme    Dark (#1e1e1e)
Log Background  Dark (#1e1e1e)
Font            Courier, 9pt (monospace)
```

## Test Selector Widget

```
┌────────────────────────────────────────┐
│ BENCHMARK TESTS                        │
├────────────────────────────────────────┤
│                                        │
│  ☑ Cold Start Latency                 │
│    Initial model load + first requests│
│                                        │
│  ☑ Warm Cache Performance             │
│    Cached completions (target: <10ms) │
│                                        │
│  ☑ Rapid-Fire Stress Test             │
│    100+ burst requests                │
│                                        │
│  ☑ Multi-Language Support             │
│    C++, Python, JS, TS, Rust          │
│                                        │
│  ☑ Context-Aware Completions          │
│    Full context window utilization    │
│                                        │
│  ☑ Multi-Line Function Generation     │
│    Structural code completion         │
│                                        │
│  ☑ GPU Acceleration                   │
│    Vulkan compute shader performance  │
│                                        │
│  ☑ Memory Profiling                   │
│    Model loading and caching overhead │
│                                        │
│  [Select All]  [Deselect All]         │
│                                        │
└────────────────────────────────────────┘
```

## Configuration Panel

```
┌────────────────────────────────────────┐
│ CONFIGURATION                          │
├────────────────────────────────────────┤
│                                        │
│  Model: [──────────────────────────▼] │
│         • Default (Mistral 3B)        │
│         • Mistral 7B                  │
│         • Custom...                   │
│                                        │
│  ☑ Enable GPU Acceleration (Vulkan)   │
│    (checked by default)                │
│                                        │
│  ☐ Verbose Output                     │
│    (unchecked for normal mode)         │
│                                        │
└────────────────────────────────────────┘
```

## Progress Display

```
Overall Progress:
[██████████░░░░░░░░░░] 6/8 (75%)
```

States during execution:
- `[░░░░░░░░░░░░░░░░░░░░] 0/8 (0%)` - Starting
- `[████░░░░░░░░░░░░░░░░] 2/8 (25%)` - Running
- `[██████████░░░░░░░░░░] 6/8 (75%)` - Most done
- `[██████████████████████] 8/8 (100%)` - Complete

## Results Summary Format

```
INDIVIDUAL TEST RESULTS:
✅ Cold Start - Avg: 125.50ms, P95: 145.20ms, Success: 100%
✅ Warm Cache - Avg: 18.75ms, P95: 22.10ms, Success: 100%
✅ Rapid Fire - Avg: 45.20ms, P95: 58.30ms, Success: 98%
✅ Multi-Lang - Avg: 52.10ms, P95: 65.40ms, Success: 96%
✅ Context    - Avg: 78.50ms, P95: 95.20ms, Success: 95%
✅ Multi-Line - Avg: 95.30ms, P95: 110.60ms, Success: 92%
✅ GPU Accel  - Avg: 32.10ms, P95: 38.90ms, Success: 99%
✅ Memory     - Peak: 487MB, Avg: 420MB, Success: 100%

═══════════════════════════════════════════════════════════

SUMMARY:
Tests Passed:    8/8
Execution Time:  45.23 seconds

🎉 ALL TESTS PASSED!
```

## Execution Sequence

### Before Running

```
┌─────────────────────────────┐
│ Dialog Opens                │
├─────────────────────────────┤
│ • All tests selected        │
│ • Default model chosen      │
│ • GPU enabled               │
│ • Ready to run              │
└─────────────────────────────┘
        ↓
   [Run Benchmarks] (blue)
   [Stop] (disabled)
```

### During Running

```
┌─────────────────────────────┐
│ Dialog Running Tests        │
├─────────────────────────────┤
│ [Run Benchmarks] (disabled) │
│ [Stop] (red - enabled)      │
│                             │
│ Progress: [████░░░░] 3/8    │
│                             │
│ Output updating in real-    │
│ time with test progress     │
│                             │
│ Results appearing as tests  │
│ complete one by one         │
└─────────────────────────────┘
```

### After Completing

```
┌─────────────────────────────┐
│ Dialog Complete             │
├─────────────────────────────┤
│ [Run Benchmarks] (blue)     │
│ [Stop] (disabled)           │
│                             │
│ Progress: [████████████] 8/8│
│                             │
│ All results visible in      │
│ results table               │
│                             │
│ Summary shows:              │
│ • Tests passed/total        │
│ • Execution time            │
│ • Pass/fail status          │
│                             │
│ JSON file exported to disk  │
└─────────────────────────────┘
```

## Console Output Example

```
═══════════════════════════════════════════════════════════
RawrXD Benchmark Suite Starting
═══════════════════════════════════════════════════════════

Model: models/ministral-3b-instruct-v0.3-Q4_K_M.gguf
GPU: Enabled
Verbose: No

Running 8 tests...

[1/8] Running: cold_start
  ✓ Request 1/3: 125.50 ms
  ✓ Request 2/3: 128.20 ms
  ✓ Request 3/3: 122.40 ms
  ✅ PASS | Avg: 125.37ms | P95: 128.20ms | Success: 100%

[2/8] Running: warm_cache
  • Cached request 1/10: 18.75 ms ✓
  • Cached request 6/10: 19.20 ms ✓
  • Cached request 10/10: 18.50 ms ✓
  ✅ PASS | Avg: 18.85ms | P95: 19.20ms | Success: 100%

[3/8] Running: rapid_fire
  • Progress: 0/50 requests...
  • Progress: 25/50 requests...
  • Progress: 50/50 requests...
  ✓ Completed 50 requests in 1.09 seconds
  • Throughput: 45.87 req/sec
  ✅ PASS | Avg: 21.75ms

... (more tests) ...

═══════════════════════════════════════════════════════════
SUMMARY: 8/8 tests passed
Execution time: 45.23 seconds
═══════════════════════════════════════════════════════════

🎉 ALL TESTS PASSED - PRODUCTION READY!
```

## Integration Points

### Menu Bar Integration

```cpp
// Automatically added to Tools menu
auto benchmarkSubmenu = toolsMenu->addMenu("Benchmarks");
benchmarkSubmenu->addAction("Run Benchmarks...");
benchmarkSubmenu->addSeparator();
benchmarkSubmenu->addAction("View Results");
```

### Dialog Management

```cpp
// Opens as modeless dialog (doesn't block IDE)
auto dialog = new QDialog(mainWindow);
dialog->setModal(false);  // Non-blocking
dialog->show();
```

### Thread Management

```cpp
// Benchmarks run on background thread
runnerThread = new QThread();
runner->moveToThread(runnerThread);
connect(this, &run, runner, &execute);
runnerThread->start();
```

---

## User Experience Flow

```
1. User clicks Tools menu
   ↓
2. Sees "Benchmarks" submenu
   ↓
3. Clicks "Run Benchmarks..."
   ↓
4. Dialog opens with test selection
   ↓
5. User selects tests (or uses defaults)
   ↓
6. Clicks "Run Benchmarks"
   ↓
7. Real-time output appears (colored)
   ↓
8. Progress bar advances
   ↓
9. Results appear as tests complete
   ↓
10. Summary shows final results
   ↓
11. benchmark_results.json exported
   ↓
12. User can run again or close dialog
```

---

## Accessibility

### Keyboard Navigation
- `Tab` - Move between widgets
- `Space` - Toggle checkboxes
- `Enter` - Click focused button
- `Escape` - Close dialog

### Screen Reader Support
- All buttons have descriptive labels
- Checkboxes have descriptive text
- Progress bar has percentage
- Error messages clear and specific

### Color Blind Support
- Uses shapes and text, not color alone
- ✓ = success, ✗ = error, ⏳ = waiting
- Clear text descriptions

---

**This represents a complete, production-ready IDE benchmark integration!**
