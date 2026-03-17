# Universal Model Router - Developer Quick Reference Card

## 🚀 One-Minute Setup

```bash
# 1. Copy files
cp universal_model_router.* cloud_api_client.* model_interface.* model_config.json ~/my_project/

# 2. Add to CMakeLists.txt
# - universal_model_router.cpp
# - cloud_api_client.cpp  
# - model_interface.cpp
# - target_link_libraries(AutonomousIDE Qt6::Network)

# 3. Set environment variables
export OPENAI_API_KEY="sk-..."

# 4. Build
cmake --build . --config Release

# 5. Use in code
```

## 📖 API Cheat Sheet

### Initialization
```cpp
ModelInterface ai;
ai.initialize("model_config.json");
```

### Generate (Sync)
```cpp
auto result = ai->generate("Hello", "gpt-4");
if (result.success) cout << result.content;
```

### Generate (Async)
```cpp
ai->generateAsync("Hello", "gpt-4", 
    [](const GenerationResult& r) {
        cout << r.content;
    }
);
```

### Generate (Stream)
```cpp
ai->generateStream("Hello", "gpt-4",
    [](const QString& chunk) { cout << chunk; }
);
```

### Generate (Batch)
```cpp
auto results = ai->generateBatch({"Q1", "Q2"}, "gpt-4");
```

### Smart Selection
```cpp
// Fastest model
QString model = ai->selectFastestModel("chat");

// Cheapest model
QString model = ai->selectCostOptimalModel(prompt, 0.01);

// Best for task
QString model = ai->selectBestModel("code", "python", false);
```

### Statistics
```cpp
ai->getAverageLatency()      // Double (ms)
ai->getSuccessRate()         // Int (%)
ai->getTotalCost()           // Double ($)
ai->getUsageStatistics()     // QJsonObject
ai->getCostBreakdown()       // QMap<QString, Double>
ai->getModelStats(model)     // QJsonObject
```

### Configuration
```cpp
ai->setDefaultModel("gpt-4");
ai->setRetryPolicy(3, 1000);
ai->setErrorCallback([](const QString& e) { qWarning() << e; });
ai->registerModel("custom", config);
ai->loadConfig("model_config.json");
```

## 🔧 Available Models

**Local (Fast, Free)**
- `quantumide-q4km` (GGUF, <50ms)
- `ollama-local` (HTTP API, <100ms)

**Cloud (Smart, Fast)**
- `gpt-4`, `gpt-4-turbo`, `gpt-3.5-turbo` (OpenAI)
- `claude-3-opus`, `claude-3-sonnet` (Anthropic)
- `gemini-pro`, `gemini-1.5-pro` (Google)
- `kimi`, `kimi-128k` (Moonshot)
- `azure-gpt4` (Azure OpenAI)
- `bedrock-claude`, `bedrock-mistral` (AWS)

## 🎯 Common Patterns

### Real-Time Feature (Use Local Model)
```cpp
auto result = ai->generate(prompt, "quantumide-q4km");
```

### Best Quality (Use GPT-4)
```cpp
auto result = ai->generate(prompt, "gpt-4");
```

### Balance (Use Claude)
```cpp
auto result = ai->generate(prompt, "claude-3-sonnet");
```

### Cost-Conscious (Use Cheap Model)
```cpp
QString model = ai->selectCostOptimalModel(prompt, 0.01);
auto result = ai->generate(prompt, model);
```

### Streaming Long Response
```cpp
ai->generateStream(long_prompt, "gpt-4",
    [](const QString& chunk) {
        cout << chunk; cout.flush();
    }
);
```

### Batch Multiple Prompts
```cpp
auto results = ai->generateBatch(
    {"Q1", "Q2", "Q3"},
    "gpt-4"
);
```

### Async Non-Blocking
```cpp
ai->generateAsync(prompt, "gpt-4",
    [this](const GenerationResult& r) {
        ui->update(r.content);
    }
);
```

## ⚙️ Configuration Reference

### model_config.json Structure
```json
{
  "models": {
    "model-name": {
      "backend": "OPENAI",
      "model_id": "gpt-4",
      "api_key": "${ENV_VAR}",
      "endpoint": "https://...",
      "parameters": {
        "max_tokens": 4096,
        "temperature": 0.7
      }
    }
  },
  "default_model": "gpt-4",
  "timeout_ms": 30000,
  "retry_count": 3
}
```

### Environment Variables
```bash
OPENAI_API_KEY=sk-...
ANTHROPIC_API_KEY=sk-ant-...
GOOGLE_API_KEY=AIza...
MOONSHOT_API_KEY=sk-...
AZURE_OPENAI_KEY=...
AZURE_OPENAI_ENDPOINT=https://...
AWS_ACCESS_KEY_ID=...
AWS_SECRET_ACCESS_KEY=...
AWS_BEDROCK_REGION=us-east-1
```

## 🛡️ Error Handling

```cpp
// Set error callback
ai->setErrorCallback([](const QString& error) {
    qWarning() << "Error:" << error;
    // Log to file, notify user, etc.
});

// Fallback chain
auto result = ai->generate(prompt, "gpt-4");
if (!result.success) {
    result = ai->generate(prompt, "claude-3-opus");
}
if (!result.success) {
    result = ai->generate(prompt, "quantumide-q4km");
}

// Check for errors
if (!result.success) {
    qCritical() << result.error;
}
```

## 📊 Monitoring

```cpp
// Log metrics
qDebug() << "Avg latency:" << ai->getAverageLatency() << "ms";
qDebug() << "Success rate:" << ai->getSuccessRate() << "%";
qDebug() << "Total cost: $" << ai->getTotalCost();

// Cost tracking
double cost = result.metadata["cost"].toDouble();
int tokens = result.metadata["tokens_used"].toInt();

// Per-model stats
auto stats = ai->getModelStats("gpt-4");
qDebug() << "GPT-4 latency:" << stats["avg_latency"];
```

## 🔐 Security

```cpp
// ✅ DO
QString api_key = qgetenv("OPENAI_API_KEY");
ai->initialize("model_config.json");

// ❌ DON'T
const QString api_key = "sk-...";  // Hardcoded!
qDebug() << config.api_key;        // Logs secret!
```

## 📁 File Locations

| File | Purpose |
|------|---------|
| `universal_model_router.h/cpp` | Router (internal) |
| `cloud_api_client.h/cpp` | Cloud client (internal) |
| `model_interface.h/cpp` | **YOUR MAIN API** |
| `model_config.json` | Model configuration |
| `model_interface_examples.cpp` | 14 working examples |
| `test_model_interface.cpp` | Test suite |

## 📚 Documentation

| File | Read When |
|------|-----------|
| Quick Start | 5-min setup |
| Complete API | Full reference |
| Operations Guide | Troubleshooting |
| Architecture | Understanding design |
| Performance | Benchmarking/tuning |
| Security | Production deployment |
| CMake Guide | Build integration |

## ⏱️ Performance Targets

| Model | Latency | Cost |
|-------|---------|------|
| quantumide-q4km | <50ms | $0 |
| ollama-local | <100ms | $0 |
| cloud models | 200-2000ms | $0.001-0.01 |

## 💡 Pro Tips

1. **Use local model for interactive features** (0-50ms)
2. **Use streaming for long responses** (better UX)
3. **Batch similar requests** (1 API call per batch)
4. **Cache frequent prompts** (instant response)
5. **Monitor costs daily** (budget control)
6. **Set retry policy** (resilience)
7. **Use async for UI** (no freezing)
8. **Pick model for task** (speed vs quality)

## 🐛 Quick Debugging

```cpp
// Check available models
for (const auto& model : ai->getAvailableModels()) {
    qDebug() << model;
}

// Verify config loaded
if (!ai->initialize("model_config.json")) {
    qCritical() << "Failed to load config!";
}

// Test API key
auto test = ai->generate("test", "gpt-4");
qDebug() << "Success:" << test.success;
qDebug() << "Error:" << test.error;

// Monitor in real-time
while (true) {
    qDebug() << "Avg latency:" << ai->getAverageLatency() << "ms";
    QThread::msleep(5000);
}
```

## 📋 Integration Checklist

- [ ] Files copied
- [ ] CMakeLists.txt updated
- [ ] Environment variables set
- [ ] Builds without errors
- [ ] Tests pass
- [ ] Local model works
- [ ] Cloud model works (with key)
- [ ] Error callback set
- [ ] Retry policy configured
- [ ] Monitoring in place

## 🚀 From Zero to Production

```
15 min: Copy files, update CMakeLists.txt
5 min: Set environment variables
5 min: Add initialization code
5 min: Test basic generation
60 min: Integrate into features
30 min: Add monitoring/logging
30 min: Security review
→ Ready for production!
```

---

**Keep this card handy! 📌**

For detailed info, see `UNIVERSAL_MODEL_ROUTER_COMPLETE.md`  
For examples, see `model_interface_examples.cpp`  
For troubleshooting, see `UNIVERSAL_MODEL_ROUTER_OPERATIONS_GUIDE.md`
