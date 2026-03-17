# Universal Model Router - Visual Architecture & Flow Diagrams

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      YOUR IDE APPLICATION                       │
│                    (ide_main_window.cpp, etc)                   │
└────────────────────────────────┬────────────────────────────────┘
                                 │
                                 │ #include "model_interface.h"
                                 │ ModelInterface ai;
                                 ▼
        ┌────────────────────────────────────────────────┐
        │         ModelInterface (UNIFIED API)           │
        │  ┌──────────────────────────────────────────┐  │
        │  │ generate(prompt, model_name)             │  │
        │  │ generateAsync(prompt, model_name, cb)    │  │
        │  │ generateStream(prompt, model_name, cb)   │  │
        │  │ generateBatch(prompts, model_name)       │  │
        │  │ selectBestModel(task, language)          │  │
        │  │ getAverageLatency() / getCost() / etc     │  │
        │  └──────────────────────────────────────────┘  │
        └────────────────────────────────────────────────┘
                         │              │
           ┌─────────────┘              └──────────────┐
           │                                            │
           ▼                                            ▼
  ┌─────────────────────┐            ┌────────────────────────┐
  │ LOCAL MODELS        │            │ CLOUD API CLIENT       │
  │ - GGUF Quantized    │            │ - Universal HTTP       │
  │ - Ollama API        │            │ - Provider Adapters    │
  │ - Zero Cost         │            │ - Request Builders     │
  │ - Zero Latency      │            │ - Response Parsers     │
  │ - 100% Private      │            │ - Logging & Metrics    │
  └─────────────────────┘            └────────────────────────┘
           │                                    │
           └────────┬─────────────────────────┬─┘
                    │                         │
        ┌───────────▼──────────┐  ┌──────────▼──────────┐
        │ UniversalModelRouter │  │  HTTP Requests to   │
        │ ┌──────────────────┐ │  │  Cloud Providers    │
        │ │ Model Registry   │ │  │  ┌────────────────┐ │
        │ │ Config Loading   │ │  │  │ OpenAI (GPT-4) │ │
        │ │ Backend Routing  │ │  │  └────────────────┘ │
        │ └──────────────────┘ │  │  ┌────────────────┐ │
        └──────────────────────┘  │  │ Anthropic      │ │
                                  │  │ (Claude)       │ │
                                  │  └────────────────┘ │
                                  │  ┌────────────────┐ │
                                  │  │ Google Gemini  │ │
                                  │  └────────────────┘ │
                                  │  ┌────────────────┐ │
                                  │  │ Moonshot (Kimi)│ │
                                  │  └────────────────┘ │
                                  │  ┌────────────────┐ │
                                  │  │ Azure OpenAI   │ │
                                  │  └────────────────┘ │
                                  │  ┌────────────────┐ │
                                  │  │ AWS Bedrock    │ │
                                  │  └────────────────┘ │
                                  └────────────────────┘
```

## Request Flow Sequence Diagram

```
Your Code                      ModelInterface              Router                CloudClient
    │                                │                        │                      │
    │─ generate(prompt, "gpt-4") ──→ │                        │                      │
    │                                │                        │                      │
    │                                │─ getModelConfig("gpt-4")─→                    │
    │                                │                        │  [Look up in registry]
    │                                │                        │  [Find: OPENAI backend]
    │                                │ ←─ ModelConfig ────────│                      │
    │                                │                        │                      │
    │                                │─────────────────────────────→                │
    │                                │  if isCloudModel {      │  generate()         │
    │                                │                        │                      │
    │                                │                        │  [Build OpenAI req]  │
    │                                │                        │  [POST to API]       │
    │                                │                        │  [Parse response]    │
    │                                │                        │  [Log metrics]       │
    │                                │ ←──── GenerationResult ─│                      │
    │                                │  }                     │                      │
    │ ← GenerationResult ────────────│                        │                      │
    │   (success, content, latency,  │                        │                      │
    │    cost, metadata)             │                        │                      │
    │                                │                        │                      │
```

## Model Selection Flow

```
                        ┌─ selectBestModel()
                        │
    Your Code          │   ├─ Query available models
                        │   ├─ Filter by task type
                        │   ├─ Filter by language
                        │   ├─ Consider local preference
                        │   │   ↓
                        │   │ Local available? ──Yes→ Return local (fast)
                        │   │   │
                        │   │   No
                        │   │   ↓
                        │   ├─ Return best cloud model
                        │   │
                        │   └─ Return model name
                        │
    ai.generate()      │
        ↑              │
        └──────────────┘
```

## Statistics Tracking (Automatic)

```
Each generate() call automatically:

    ┌─────────────────────────────────┐
    │ 1. Start timer (start_time)     │
    ├─────────────────────────────────┤
    │ 2. Call cloud API / local model │
    ├─────────────────────────────────┤
    │ 3. Calculate latency            │
    │    latency_ms = now - start_time│
    ├─────────────────────────────────┤
    │ 4. Update stats map:            │
    │    ├─ call_count++              │
    │    ├─ success_count++ (if good) │
    │    ├─ total_latency += latency  │
    │    └─ total_cost += estimate    │
    ├─────────────────────────────────┤
    │ 5. Return result                │
    │    ├─ content                   │
    │    ├─ success                   │
    │    ├─ latency_ms                │
    │    ├─ tokens_used               │
    │    └─ metadata                  │
    └─────────────────────────────────┘
    
    Then access via:
    - ai.getAverageLatency("model")
    - ai.getSuccessRate("model")
    - ai.getTotalCost()
    - ai.getUsageStatistics()
```

## Cloud API Provider Adapter Pattern

```
CloudApiClient maintains a map of API endpoints:

    ┌────────────────────────────────────────────┐
    │     api_endpoints Map (indexed by backend)  │
    ├────────────────────────────────────────────┤
    │                                             │
    │ ANTHROPIC → ┌──────────────────────────┐  │
    │             │ base_url: anthropic.com  │  │
    │             │ endpoint: /v1/messages   │  │
    │             │ request_builder: fn ptr  │──┼─→ buildAnthropicRequest()
    │             │ response_parser: fn ptr  │──┼─→ parseAnthropicResponse()
    │             └──────────────────────────┘  │
    │                                             │
    │ OPENAI   → ┌──────────────────────────┐  │
    │             │ base_url: openai.com     │  │
    │             │ endpoint: /v1/chat/...   │  │
    │             │ request_builder: fn ptr  │──┼─→ buildOpenAIRequest()
    │             │ response_parser: fn ptr  │──┼─→ parseOpenAIResponse()
    │             └──────────────────────────┘  │
    │                                             │
    │ GOOGLE   → ┌──────────────────────────┐  │
    │             │ base_url: googleapis.com │  │
    │             │ endpoint: /v1beta/models │  │
    │             │ request_builder: fn ptr  │──┼─→ buildGoogleRequest()
    │             │ response_parser: fn ptr  │──┼─→ parseGoogleResponse()
    │             └──────────────────────────┘  │
    │                                             │
    │ [... similar for MOONSHOT, AZURE, AWS ...] │
    │                                             │
    └────────────────────────────────────────────┘

    When generate() is called:
    1. Look up API in map by backend type
    2. Call request_builder(prompt, config)
    3. POST JSON to endpoint
    4. Get response
    5. Call response_parser(response_json)
    6. Return extracted content
```

## Configuration File Structure

```
model_config.json
├─ models
│  ├─ quantumide-q4km
│  │  ├─ backend: LOCAL_GGUF
│  │  ├─ model_id: path/to/model.gguf
│  │  ├─ api_key: ""
│  │  └─ parameters: { max_tokens, temperature, ... }
│  │
│  ├─ gpt-4
│  │  ├─ backend: OPENAI
│  │  ├─ model_id: "gpt-4"
│  │  ├─ api_key: ${OPENAI_API_KEY}  ← From environment
│  │  └─ parameters: { max_tokens, temperature, ... }
│  │
│  ├─ claude-3-opus
│  │  ├─ backend: ANTHROPIC
│  │  ├─ model_id: "claude-3-opus-20240229"
│  │  ├─ api_key: ${ANTHROPIC_API_KEY}  ← From environment
│  │  └─ parameters: { max_tokens, temperature, ... }
│  │
│  ├─ [... 9 more models ...]
│  │
│  └─ your-custom-model
│     ├─ backend: CLOUD_OR_LOCAL
│     ├─ model_id: "..."
│     ├─ api_key: ${YOUR_KEY}
│     └─ parameters: { ... }
│
├─ defaults
│  ├─ default_model: "quantumide-q4km"
│  ├─ timeout_seconds: 30
│  └─ retry_attempts: 3
│
└─ logging
   ├─ enabled: true
   ├─ level: "INFO"
   └─ log_api_calls: true
```

## Integration Points in Your IDE

```
┌─────────────────────────────────────────────────────────┐
│              Your IDE Components                         │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  CodeSuggestionEngine                                   │
│  └─ onCodeChanged() → ai.generate() → show suggestions  │
│                                                          │
│  TestGenerator                                          │
│  └─ generateTests() → ai.generate() → create tests      │
│                                                          │
│  SecurityAnalyzer                                       │
│  └─ analyzeCode() → ai.generate() → find vulnerabilities│
│                                                          │
│  CompilerOptimizer                                      │
│  └─ optimizeCode() → ai.generate() → suggest fixes      │
│                                                          │
│  CodeFormatter                                          │
│  └─ formatCode() → ai.generate() → format improvements  │
│                                                          │
│  DocumentationGenerator                                 │
│  └─ generateDocs() → ai.generate() → create docs        │
│                                                          │
│  All use the same interface:                            │
│  ModelInterface ai;                                     │
│  ai.initialize("model_config.json");                    │
│  auto result = ai.generate(prompt, model_name);        │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

## Error Handling Flow

```
generate() call
    │
    ├─ Validate ModelConfig
    │  └─ if invalid → error_callback() → return error
    │
    ├─ Check if model is local
    │  │
    │  └─ if local
    │     └─ Use local engine
    │        ├─ if success → return result
    │        └─ if error → log & return error
    │
    └─ if cloud
       ├─ Build HTTP request
       ├─ POST to cloud provider
       │
       ├─ Handle HTTP errors
       │  ├─ 401 → Auth error
       │  ├─ 429 → Rate limit
       │  ├─ 500 → Server error
       │  └─ Network error
       │
       ├─ Parse response
       │  ├─ if valid JSON → extract content
       │  └─ if invalid → error handling
       │
       ├─ Log API call
       │  ├─ Request body
       │  ├─ Response status
       │  ├─ Latency
       │  └─ Success/failure
       │
       └─ Return GenerationResult
          ├─ content (or empty if error)
          ├─ success flag
          ├─ error message
          └─ metadata
```

## Cost Tracking Example

```
Each request is tracked:

┌──────────────────────────────────────────┐
│ Request: Generate code with GPT-4        │
├──────────────────────────────────────────┤
│ Tokens: 150 input + 200 output = 350     │
│ Model: gpt-4                             │
│ Rate: $0.03 per 1K input + $0.06 per 1K │
│                                          │
│ Cost = (150 * 0.03 / 1000) +             │
│        (200 * 0.06 / 1000)               │
│      = 0.0045 + 0.012                    │
│      = $0.0165 per request               │
├──────────────────────────────────────────┤
│ Accumulated stats:                       │
│ Request #1: $0.0165                      │
│ Request #2: $0.0128                      │
│ Request #3: $0.0142                      │
│ ─────────────────────────                │
│ Total Cost: $0.0435                      │
│ Average per request: $0.0145              │
└──────────────────────────────────────────┘

Access via: ai.getTotalCost(), ai.getCostBreakdown()
```

## Performance Metrics Collection

```
Every request is measured:

Request Timeline:
┌─────────────────────────────────────────────┐
│ t=0ms: generate() called                    │
│        └─ Start timer                       │
├─────────────────────────────────────────────┤
│ t=0-50ms: Build request (serialization)     │
├─────────────────────────────────────────────┤
│ t=50-200ms: Network round trip              │
│            └─ POST request                  │
│            └─ Wait for response             │
│            └─ Download response             │
├─────────────────────────────────────────────┤
│ t=200-215ms: Parse response                 │
│             └─ JSON parsing                 │
│             └─ Content extraction           │
├─────────────────────────────────────────────┤
│ t=215ms: Complete (total latency)           │
│          └─ Stop timer                      │
│          └─ Record 215ms                    │
└─────────────────────────────────────────────┘

Tracked metrics:
├─ Individual request latency: 215ms
├─ Average latency: (sum of all) / count
├─ P95 latency: 95th percentile
├─ P99 latency: 99th percentile
└─ Success rate: success_count / total_count

Access via:
├─ ai.getAverageLatency()
├─ ai.getModelStats("gpt-4")
└─ ai.getUsageStatistics()
```

## Memory Architecture

```
ModelInterface instance:
┌──────────────────────────────────────────┐
│ router: UniversalModelRouter*            │
│   ├─ model_registry: QMap                │
│   │  ├─ "gpt-4" → ModelConfig            │
│   │  ├─ "claude-3-opus" → ModelConfig    │
│   │  └─ ... 10 more models               │
│   └─ cloud_client: CloudApiClient*       │
│                                           │
│ stats_map: QMap<QString, ModelStats>     │
│   ├─ "gpt-4" → {calls: 5, cost: $0.08}  │
│   ├─ "claude-3-opus" → {calls: 3, ...}  │
│   └─ ... stats for all used models       │
│                                           │
│ default_model: "quantumide-q4km"        │
│                                           │
│ error_callback: std::function            │
└──────────────────────────────────────────┘

CloudApiClient instance:
┌──────────────────────────────────────────┐
│ network_manager: QNetworkAccessManager*  │
│                                           │
│ api_endpoints: map                       │
│   ├─ ANTHROPIC → ApiEndpoint             │
│   ├─ OPENAI → ApiEndpoint                │
│   └─ ... 6 more providers                │
│                                           │
│ call_history: QVector<ApiCallLog>        │
│   ├─ [Request 1]: {provider, latency...} │
│   ├─ [Request 2]: {provider, latency...} │
│   └─ ... up to 1000 entries              │
└──────────────────────────────────────────┘
```

## Multi-Provider Comparison Example

```
Request: "Generate Python code to sort array"

Same prompt, 4 different models:

GPT-4 (OpenAI)
├─ Latency: 287ms
├─ Cost: $0.0156
├─ Quality: ⭐⭐⭐⭐⭐
└─ Response: "def sort_array(arr)..."

Claude 3 Opus (Anthropic)
├─ Latency: 412ms
├─ Cost: $0.0089
├─ Quality: ⭐⭐⭐⭐⭐
└─ Response: "def sort_array(arr)..."

Gemini Pro (Google)
├─ Latency: 156ms
├─ Cost: $0.0001
├─ Quality: ⭐⭐⭐⭐
└─ Response: "def sort_array(arr)..."

QuantumIDE Local (Your GGUF)
├─ Latency: 23ms
├─ Cost: $0.0000
├─ Quality: ⭐⭐⭐
└─ Response: "def sort_array(arr)..."

Selection Strategies:
├─ selectFastestModel() → QuantumIDE (23ms)
├─ selectCostOptimalModel(0.01) → Gemini (0.0001)
├─ selectBestModel("code_gen", "python", true) → QuantumIDE
└─ generate(prompt, "gpt-4") → Use what you specify
```

---

These diagrams illustrate the complete flow and architecture of the Universal Model Router system!
