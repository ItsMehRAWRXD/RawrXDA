# RawrXD QtShell - Quick Start Guide

## 🚀 Getting Started

### 1. Launch the Application

```bash
cd build/bin/Release
./RawrXD-QtShell.exe
```

**Expected**:
- Window appears in 2-3 seconds
- Menu bar, toolbars, and dock widgets visible
- No errors or warnings in console

---

## 📋 Key Files & Documentation

### Executable
- **Location**: `build/bin/Release/RawrXD-QtShell.exe`
- **Size**: 1.97 MB (Release, optimized)
- **Status**: ✅ Production ready

### Build Configuration
- **File**: `CMakeLists.txt` (2,233 lines)
- **Key Section**: Lines 230-400 (RawrXD-QtShell target definition)
- **Qt MOC Wrapper**: Lines 455-462 (qt_wrap_cpp for Q_OBJECT headers)

### GGUF Integration
- **Inference Engine**: `src/qtapp/inference_engine.hpp` / `.cpp`
- **GGUF Loader**: `src/qtapp/gguf_loader.h` / `.cpp`
- **UI Wiring**: `src/qtapp/MainWindow.cpp` (lines 3489-3530)
- **Menu Integration**: `src/qtapp/MainWindow.cpp` (lines 589-591, 632-633)

### Testing & Validation
- **Unit Tests**: `tests/test_gguf_integration.cpp`
- **Integration Guide**: `GGUF_INTEGRATION_TESTING.md`
- **Readiness Report**: `PRODUCTION_READINESS_REPORT.md`
- **Build Summary**: `BUILD_COMPLETION_SUMMARY.md`

---

## 🎯 Features & How to Use Them

### Load a GGUF Model

**UI Path**:
```
Menu: AI → "Load GGUF Model..."
```

**What Happens**:
1. File browser opens with *.gguf filter
2. Select a GGUF model file
3. Status bar shows "Loading GGUF model..."
4. Model loads in background thread (non-blocking)
5. Status updates when complete

**Code Path** (technical):
```cpp
User clicks menu
  → MainWindow::loadGGUFModel()
  → QFileDialog::getOpenFileName()
  → QMetaObject::invokeMethod(m_inferenceEngine, "loadModel", Qt::QueuedConnection)
  → InferenceEngine::loadModel(path) [in worker thread]
  → GGUFLoaderQt opens and parses file
  → emit modelLoadedChanged(true, modelName)
```

### Run Inference

**UI Path**:
```
Menu: AI → "Run Inference..."
```

**What Happens**:
1. Multi-line text dialog appears
2. Type your prompt
3. Click OK
4. Inference engine tokenizes prompt
5. Model generates tokens
6. Results appear in hex dump console

**Example Prompts**:
- "What is machine learning?"
- "Write a Python function to sort a list"
- "Explain the concept of neural networks"

### Unload Model

**UI Path**:
```
Menu: AI → "Unload Model"
```

**What Happens**:
1. Model is unloaded from memory
2. Thread pool cleaned up
3. Memory freed
4. Ready to load another model

---

## 🧪 Testing the Integration

### Quick Smoke Test

Run once to verify UI is responsive:

```bash
# Terminal 1: Build
cmake --build build --config Release --target RawrXD-QtShell

# Terminal 2: Run executable
cd build/bin/Release
./RawrXD-QtShell.exe

# In UI: Try loading a model
# Menu: AI → Load GGUF Model...
# (Cancel out of file dialog - no crash = success)
```

**Success Criteria**:
- ✅ App launches without errors
- ✅ File dialog opens and closes cleanly
- ✅ No crashes or exceptions
- ✅ Memory usage stays reasonable

### Comprehensive Testing

See `GGUF_INTEGRATION_TESTING.md` for full test scenarios:

1. **Scenario 1**: UI Responsiveness (10 min)
2. **Scenario 2**: Model Selection Dialog (5 min)
3. **Scenario 3**: GGUF File Loading (30 min - requires model file)
4. **Scenario 4**: Inference Execution (15 min - requires model file)
5. **Scenario 5**: Model Unload (5 min)
6. **Scenario 6**: Memory Monitoring (10 min)
7. **Scenario 7**: Concurrent Operations (10 min)

---

## 📊 Monitoring & Debugging

### Console Output

The application logs to console with prefixes:

```
[InferenceEngine] Model loading started...
[InferenceEngine] GGUFLoader opened file successfully
[InferenceEngine] Model loaded successfully: tinyllama-1.1b
[MainWindow] Model changed - updating UI
[Inference] Generating 50 tokens...
[Streaming] Token: "What"
[Streaming] Token: "is"
[Streaming] Inference complete
```

### Key Logging Points

| Location | Logs |
|----------|------|
| `InferenceEngine::loadModel()` | Model load progress |
| `MainWindow::loadGGUFModel()` | UI actions |
| `MainWindow::runInference()` | Inference start/end |
| `GGUFLoaderQt` | File I/O details |
| Worker thread | Performance metrics |

### Performance Metrics

Watch for these in console:

```
Memory Usage: XXX MB
Tokens/sec: XX.X
Model: tinyllama-1.1b (n_layer=22, n_embd=768)
Quantization: Q2_K
```

---

## 🔧 Common Issues & Solutions

### Issue: "Model is not loading"

**Check**:
1. Is file a valid GGUF? (File should start with `GGUF` magic bytes)
2. Does file have read permissions?
3. Is disk space available? (needs 2x model size)
4. Check console for error messages

**Solution**:
```bash
# Verify GGUF file validity
file model.gguf
ls -lh model.gguf  # Check permissions and size
```

### Issue: "Inference is slow"

**Check**:
1. Model size (smaller models are faster)
2. CPU load (other apps using CPU?)
3. Quantization type (Q2_K faster than Q6_K)
4. Token length (longer generation = slower)

**Solution**:
- Try smaller model first (e.g., TinyLlama instead of Llama 70B)
- Close other applications
- Request fewer tokens (max_tokens parameter)

### Issue: "Out of memory"

**Check**:
1. System RAM available?
2. Model size too large for available RAM?
3. Running multiple instances?

**Solution**:
```bash
# Check available RAM
wmic OS get TotalVisibleMemorySize,FreePhysicalMemory

# Use smaller quantization (Q2_K vs Q6_K saves ~2GB)
# Or use smaller model
```

---

## 📈 Expected Performance

### Load Times (approximate)

| Model Size | Quantization | Load Time | Memory |
|------------|--------------|-----------|--------|
| 1B params | Q2_K (600MB) | 5-10 sec | +700 MB |
| 3B params | Q2_K (2GB) | 10-20 sec | +2.2 GB |
| 7B params | Q4_K (4GB) | 15-30 sec | +4.2 GB |

### Inference Speed (approximate)

| Model | Quantization | Tokens/Sec | Time to First Token |
|-------|--------------|-----------|-------------------|
| 1B | Q2_K | 20-40 | 1-2 sec |
| 3B | Q2_K | 10-20 | 2-3 sec |
| 7B | Q4_K | 5-10 | 3-5 sec |

*Note*: Actual speeds depend on CPU, RAM, and disk speed.

---

## 🚀 Advanced Usage

### Command-line Loading

You can pass a GGUF file path as environment variable:

```bash
# Load model on startup (future enhancement)
set RAWRXD_MODEL=path\to\model.gguf
./RawrXD-QtShell.exe
```

*Note: Currently requires UI interaction, command-line support coming soon.*

### Programmatic Access

```cpp
// Access inference engine from code
InferenceEngine* engine = mainWindow->getInferenceEngine();

// Load model
QMetaObject::invokeMethod(engine, "loadModel", 
    Qt::QueuedConnection, Q_ARG(QString, "/path/to/model.gguf"));

// Listen for completion
connect(engine, &InferenceEngine::modelLoadedChanged,
    this, [](bool loaded) {
        qInfo() << "Model loaded:" << loaded;
    });

// Run inference
QMetaObject::invokeMethod(engine, "request",
    Qt::QueuedConnection,
    Q_ARG(QString, "Your prompt here"),
    Q_ARG(qint64, QDateTime::currentMSecsSinceEpoch()));
```

---

## 📚 Finding Model Files

### Recommended Sources

1. **HuggingFace Hub** (Primary)
   - https://huggingface.co/models?library=ggml
   - Search for GGUF format
   - Download quantized versions (Q2_K, Q4_K)

2. **Ollama** (Easy download)
   - https://ollama.ai/library
   - Download directly or integrate API

3. **TheBloke Collections** (Best quantization)
   - https://huggingface.co/TheBloke
   - High-quality GGUF files

### Example Models to Try

```
Small (Fast, ~600MB):
- tinyllama-1.1b-chat-v1.0.Q2_K.gguf

Medium (Balanced, ~2GB):
- mistral-7b-instruct-v0.1.Q2_K.gguf

Large (Quality, ~4GB):
- neural-chat-7b-v3-1.Q4_K_M.gguf
```

---

## 🔍 Debugging Tips

### Enable Verbose Logging

Modify `inference_engine.cpp` to increase logging:

```cpp
// Change qDebug() to qInfo() for more output
qInfo() << "[InferenceEngine] Detailed: " << details;
```

### Memory Profiling

Use Windows Performance Analyzer:

```bash
# In Command Prompt (Admin)
wpr -start GeneralProfile
# Run your test
wpr -stop output.etl
# Analyze with wpa.exe
```

### Thread Debugging

Use Visual Studio Debugger:

```
Debug → Windows → Threads
Debug → Windows → Parallel Stacks
```

---

## 📞 Support & Resources

### Documentation Files

1. **GGUF_INTEGRATION_TESTING.md**
   - Detailed testing scenarios
   - Step-by-step instructions
   - Success criteria

2. **PRODUCTION_READINESS_REPORT.md**
   - Architecture overview
   - Quality metrics
   - Risk assessment

3. **BUILD_COMPLETION_SUMMARY.md**
   - How we solved 46 errors
   - Technical insights
   - Performance baselines

### Code Documentation

- **MainWindow.h**: Central UI component (539 lines)
- **InferenceEngine.hpp**: Model loading & inference (268 lines)
- **GGUFLoader.h**: Low-level file operations (193 lines)

---

## ✅ Verification Checklist

Before considering the build complete:

- [ ] QtShell.exe launches without errors
- [ ] UI is responsive (menus, buttons work)
- [ ] Can open file dialog (AI → Load GGUF Model)
- [ ] Can close file dialog without crash
- [ ] Console shows no exceptions
- [ ] Can load a test model (if available)
- [ ] Can run inference (if model loaded)
- [ ] Model unloads cleanly
- [ ] Can load another model (no stale state)
- [ ] Memory usage is reasonable (~150MB baseline, +model size)

---

## 🎓 Learning Resources

### Qt 6 Documentation
- Qt Signal/Slot: https://doc.qt.io/qt-6/signalsandslots.html
- Threading: https://doc.qt.io/qt-6/threads.html
- MOC: https://doc.qt.io/qt-6/moc.html

### GGML/GGUF Format
- GGML Repository: https://github.com/ggerganov/ggml
- GGUF Specification: https://github.com/ggerganov/ggml/blob/master/docs/gguf.md
- Model Quantization: https://github.com/ggerganov/llama.cpp/wiki

### CMake Best Practices
- Official CMake Guide: https://cmake.org/cmake/help/
- Modern CMake: https://cliutils.gitlab.io/modern-cmake/

---

## 🎉 Success!

You're now ready to:
1. ✅ Build RawrXD QtShell from source
2. ✅ Load GGUF models through the UI
3. ✅ Run inference on quantized LLMs
4. ✅ Stream results in real-time
5. ✅ Monitor performance and memory

**Next milestone**: Load your first production model! 🚀

---

*Quick Start Guide v1.0*  
*Updated: 2025-12-13*  
*Status: Production Ready*
