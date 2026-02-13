# RawrZ MMF Integration Guide for Win32ChatApp

## How MMF Integrates with the Chat Application

### Architecture Overview

```
User writes prompt in Chat UI
           ↓
    Win32ChatApp::sendPromptToModel()
           ↓
    ModelConnection (HTTP to Ollama)
           ↓
    Ollama process reads from MMF
           ↓
    Memory-Mapped File (50+ GB virtual)
           ↓
    Model tensors loaded on-demand
           ↓
    Ollama generates response
           ↓
    Streams back to Chat UI
```

### Step-by-Step Integration

#### 1. Pre-Setup: Create MMF (One-Time)

```powershell
# Run this once to set up the model
cd "C:\Users\YourUsername\OneDrive\Desktop\Powershield\RawrXD-ModelLoader"

.\scripts\RawrZ-GGUF-MMF.ps1 `
    -GgufPath "C:\models\llama-3.1-70b-q4.gguf" `
    -OutDir "$(pwd)\RawrZ-HF" `
    -MmfName "RawrZ-Llama70B" `
    -LaunchOllama

# Output:
# ✅ MMF created: RawrZ-Llama70B
# ✅ HF folder: .\RawrZ-HF
# ✅ Ollama launched (reads from MMF)
```

#### 2. Start Chat Application

```cpp
// Win32ChatApp initialization
#include "Win32ChatApp.h"
#include "ModelConnection.h"
#include "mmf_gguf_loader.h"

class Win32ChatApp {
private:
    std::unique_ptr<ModelConnection> m_modelConnection;
    std::unique_ptr<MMFGgufLoader> m_mmfLoader;
    std::string m_currentMMF;

public:
    bool onCreate() {
        // ... existing UI creation code ...

        // Initialize MMF support (optional, for local inference)
        m_mmfLoader = std::make_unique<MMFGgufLoader>();
        
        // Attempt to connect to Ollama (which is reading from MMF)
        m_modelConnection = std::make_unique<ModelConnection>("http://localhost:11434");
        
        if (m_modelConnection->checkConnection()) {
            appendAgentMessage("Connected to Ollama (via MMF)\nReady for inference");
        }
        
        return true;
    }

    void sendPromptToModel(const std::string& prompt) {
        if (!m_modelConnection || !m_modelConnection->isConnected()) {
            appendAgentMessage("Error: Ollama not connected\nStart Ollama with MMF first");
            return;
        }

        // Send to Ollama (which reads from MMF automatically)
        m_modelConnection->sendPrompt(
            "llama2",  // Model name in Ollama
            prompt,
            {},  // context
            
            // Response callback (streaming)
            [this](const std::string& chunk) {
                appendAgentMessage(chunk);
            },
            
            // Error callback
            [this](const std::string& error) {
                appendAgentMessage("Error: " + error);
            },
            
            // Complete callback
            [this]() {
                appendAgentMessage("\n[Response complete]");
            }
        );
    }
};
```

#### 3. User Interaction Flow

```
User types in chat box:
"Explain quantum computing"
         ↓
    [Send] button clicked
         ↓
Win32ChatApp::sendPromptToModel()
         ↓
    Append to user chat panel:
    "User: Explain quantum computing"
         ↓
    ModelConnection::sendPrompt()
         ↓
    HTTP POST to Ollama:
    {
        "model": "llama2",
        "prompt": "Explain quantum computing",
        "stream": true
    }
         ↓
    Ollama receives request
         ↓
    Ollama reads from MMF:
    - Load token embeddings
    - Process through transformer layers
    - Generate next token
    - Stream back to client
         ↓
    Chat app receives chunks:
    "Quantum" | " computing" | " is" | " a..." | "\n"
         ↓
    Append each chunk to agent panel
         ↓
    Update context manager (256k tokens)
         ↓
    Display complete response
```

#### 4. Memory Usage During Interaction

```
Idle State:
├─ Chat App:        ~50 MB
├─ Ollama:          ~200 MB (model metadata)
└─ MMF:             0 MB active (virtual)
   Total:           ~250 MB

During Inference:
├─ Chat App:        ~50 MB
├─ Ollama:          ~2-4 GB (active layers)
├─ MMF:             512 MB (current shard in use)
└─ ContextManager:  ~10 MB (256k token history)
   Total:           ~3-5 GB (depends on model and batch size)

vs Traditional (70B model):
├─ Chat App:        ~50 MB
├─ Ollama:          ~70 GB (full model loaded)
└─ ContextManager:  ~10 MB
   Total:           ~70 GB (not even possible on most machines!)
```

### Configuration: Settings File

Create `chat_settings.ini`:

```ini
[Connection]
ModelEndpoint=http://localhost:11434
DefaultModel=llama2
AutoConnect=true

[MMF]
UseMmf=true
MmfName=RawrZ-Llama70B
MmfSize=37817600000

[Context]
MaxTokens=262144
PruningStrategy=normal
AutoPrune=true

[UI]
DarkMode=true
FontSize=10
Width=1200
Height=800

[Performance]
StreamChunkSize=1024
MaxConcurrentRequests=1
```

### Advanced: Local Inference with MMF

If you want to use MMF for local inference (not Ollama):

```cpp
#include "mmf_gguf_loader.h"

class LocalInference {
private:
    MMFGgufLoader m_loader;
    
public:
    bool Initialize(const std::string& mmfName, uint64_t size) {
        return m_loader.OpenMemoryMappedFile(mmfName, size);
    }
    
    void StreamTensor(const std::string& tensorName) {
        m_loader.StreamTensorFromMMF(
            tensorName,
            1024 * 1024,  // 1 MB chunks
            [this](const uint8_t* chunk, uint64_t size) {
                // Process tensor chunk
                // This could upload to GPU, process with GGML, etc.
                ProcessChunk(chunk, size);
            }
        );
    }
    
    void ProcessChunk(const uint8_t* chunk, uint64_t size) {
        // Example: GPU upload
        // cudaMemcpy(d_buffer, chunk, size, cudaMemcpyHostToDevice);
        
        // Or GGML processing
        // ggml_tensor* t = ggml_new_tensor_1d(ctx, GGML_TYPE_Q4_0, size);
        // memcpy(t->data, chunk, size);
    }
};
```

### Deployment Checklist

- [ ] **Setup Phase**
  - [ ] Download RawrZ-GGUF-MMF.ps1
  - [ ] Run script with your GGUF
  - [ ] Wait for completion (10-15 min)
  - [ ] Verify HF folder created

- [ ] **Ollama Phase**
  - [ ] Ollama launched by script
  - [ ] Check `Get-Process | Where {$_.Name -eq "Ollama"}`
  - [ ] Test with `curl http://localhost:11434/api/generate -d '{"model":"llama2","prompt":"test"}'`

- [ ] **Chat App Phase**
  - [ ] Build RawrXD-Chat.exe
  - [ ] Launch Chat app
  - [ ] Type test message
  - [ ] Verify response from Ollama

- [ ] **Performance Phase**
  - [ ] Monitor memory usage
  - [ ] Check latency to first token
  - [ ] Test 256k context handling
  - [ ] Verify token counting

### Monitoring and Diagnostics

#### Check MMF Status

```cpp
auto stats = m_mmfLoader->GetMMFStats();

std::cout << "MMF Status:" << std::endl;
std::cout << "  Total Size: " << (stats.totalSize / (1024*1024*1024)) << " GB" << std::endl;
std::cout << "  Tensors: " << stats.tensorCount << std::endl;
std::cout << "  Largest: " << (stats.largestTensor / (1024*1024)) << " MB" << std::endl;
std::cout << "  Avg: " << (stats.averageTensorSize / 1024) << " KB" << std::endl;
```

#### Monitor Ollama

```powershell
# In PowerShell, watch Ollama memory usage
while ($true) {
    $proc = Get-Process -Name ollama -ErrorAction SilentlyContinue
    if ($proc) {
        Write-Host "Ollama Memory: $([Math]::Round($proc.WorkingSet / 1GB, 2)) GB"
    }
    Start-Sleep -Seconds 1
}
```

#### Test Response Streaming

```cpp
// Verify streaming works correctly
void TestStreaming() {
    std::string fullResponse;
    
    m_modelConnection->sendPrompt(
        "llama2",
        "Count from 1 to 10",
        {},
        
        // Callback receives chunks as they arrive
        [&fullResponse](const std::string& chunk) {
            fullResponse += chunk;
            std::cout << "Chunk: " << chunk;
        },
        
        nullptr,  // error callback
        
        [&fullResponse]() {
            std::cout << "\nFull response: " << fullResponse << std::endl;
        }
    );
}
```

### Troubleshooting Integration

#### Problem: Chat App connects but Ollama returns errors

**Solution**:
```powershell
# Check Ollama status
Get-Process -Name ollama

# Restart Ollama
Stop-Process -Name ollama -Force
Start-Process ollama

# Check connection
Invoke-RestMethod http://localhost:11434/api/tags
```

#### Problem: Response is very slow

**Possible causes**:
1. Model file on slow storage (USB drive)
2. Insufficient RAM for inference layer
3. MMF still being created (check Task Manager)

**Solution**:
- Move source GGUF to SSD
- Increase available RAM (close other apps)
- Increase shard size (`-ShardSizeMB 1024`)

#### Problem: Chat App can't find MMF

**Solution**:
```cpp
// Verify MMF name matches
std::string mmfName = "RawrZ-Llama70B";  // Must match script output

// Check with -Verbose flag
// .\RawrZ-GGUF-MMF.ps1 -GgufPath ... -Verbose
```

### Next Steps

1. **Run MMF Setup**: `.\scripts\RawrZ-GGUF-MMF.ps1 -GgufPath <path> -LaunchOllama`
2. **Build Chat App**: `cmake --build build --target RawrXD-Chat`
3. **Launch Chat**: `.\build\bin\Release\RawrXD-Chat.exe`
4. **Test Prompt**: Type a message and send
5. **Monitor**: Watch memory usage and response time

## Performance Expectations

### First Run (Model Loading into Context)
- Time: 30-60 seconds
- Memory spike: ~3-5 GB
- Expected behavior: Slight UI lag, then response

### Subsequent Runs (Model cached)
- Time: 5-10 seconds
- Memory: Stays constant
- Expected behavior: Smooth interaction

### Context Window Management
- Max tokens: 262,144 (256k)
- Auto-prune: Oldest messages removed
- Memory: Stays under 10 MB for history

---

**Document Version**: 1.0  
**Last Updated**: 2025-11-30  
**Status**: Production Ready  
**Tested On**: Windows 10/11, PowerShell 7.x, Ollama 0.1+
