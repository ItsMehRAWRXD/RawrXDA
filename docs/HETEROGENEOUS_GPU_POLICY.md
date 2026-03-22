# Heterogeneous GPU policy table (E13)

**Done when:** CMake + registry document **AMD / Intel / ARM / external** lanes; UI never claims “supported” without a matching flag.

## Matrix (maintain in repo)

| Vendor / class | Discovery | Inference lane | Default | Feature flag |
|----------------|-----------|------------------|---------|--------------|
| AMD discrete | AMDGPU + Vulkan | GGUF / Vulkan | on where built | `RAWRXD_VULKAN` |
| Intel integrated | Vulkan | Same / CPU fallback | opt-in | document |
| ARM (Snapdragon etc.) | Platform | CPU or external | external | document |
| External API | N/A | Ollama / cloud | user opt-in | N/A |

## Rules

- **No fake green checks** in settings; use “unknown” until discovery runs.
- **Feature manifest** (PR07): stubs must be labeled.
