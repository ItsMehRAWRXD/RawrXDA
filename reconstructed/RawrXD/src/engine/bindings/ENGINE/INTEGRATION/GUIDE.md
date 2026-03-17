# RawrXD Dynamic Prompt Engine — Unity/Unreal Integration Guide

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Game Engine Layer                      │
├──────────────────┬───────────────────────────────────────┤
│  Unity (C#)      │  Unreal Engine 5 (C++)               │
│  P/Invoke        │  Runtime DLL Loading                  │
│  MonoBehaviour   │  UObject + Blueprint                  │
├──────────────────┴───────────────────────────────────────┤
│           RawrXD_DynamicPromptEngine.dll                  │
│              (RAWRXD_PROMPT_API exports)                  │
├──────────────────────────────────────────────────────────┤
│  C Glue Layer (dynamic_prompt_engine_glue.cpp)           │
│  • PromptGen_ClassifyToStruct — struct return for FFI    │
│  • PromptGen_GetVersion — runtime version query          │
│  • PromptGen_GetModeName — human-readable mode names     │
├──────────────────────────────────────────────────────────┤
│  MASM64 Kernel (RawrXD_DynamicPromptEngine.asm)          │
│  • Classification with density-scored heuristics         │
│  • Template database with {{CONTEXT}} interpolation      │
│  • ForceMode override                                    │
│  • 8KB spinlocked scratch buffer — NO CRT, NO HEAP      │
└──────────────────────────────────────────────────────────┘
```

## Build

```bash
cmake --build . --config Release --target RawrXD_DynamicPromptEngine
```

Output: `bin/RawrXD_DynamicPromptEngine.dll`

## Unity Setup

1. Copy `RawrXD_DynamicPromptEngine.dll` → `Assets/Plugins/x86_64/`
2. Copy `RawrXDDynamicPromptEngine.cs` → `Assets/Scripts/`
3. Select the DLL in Unity Inspector → set CPU: x86_64, OS: Windows

```csharp
// Classify text
var result = RawrXDPromptEngine.Classify("sudo rm -rf /");
Debug.Log($"Mode: {result.PromptMode}, Score: {result.Score}");

// Generate critic prompt
string critic = RawrXDPromptEngine.BuildCritic("your code here");

// Force a mode
RawrXDPromptEngine.ForceMode(RawrXDPromptEngine.PromptMode.Security);
```

### IL2CPP Notes
- All P/Invoke calls use `CallingConvention.Cdecl` (Win64 default)
- `[StructLayout(LayoutKind.Sequential, Pack = 8)]` matches C ABI
- No `MonoPInvokeCallback` needed (we don't pass delegates to native)

## Unreal Engine 5 Setup

### Option A: Game Module
1. Copy DLL → `Binaries/Win64/`
2. Copy `.h` + `.cpp` → `Source/YourModule/`
3. Add to `Build.cs`:
```csharp
PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
```

### Option B: Plugin
1. Create plugin folder: `Plugins/RawrXD/`
2. Copy DLL → `Plugins/RawrXD/Binaries/Win64/`
3. Copy source → `Plugins/RawrXD/Source/RawrXD/`

### C++ Usage
```cpp
#include "RawrXDDynamicPromptEngine.h"

// Classify
FPromptClassifyResult Result = URawrXDPromptEngineLibrary::Classify(TEXT("check this code"));
UE_LOG(LogTemp, Log, TEXT("Mode: %s, Score: %d"), *Result.ModeName, Result.Score);

// Build prompts
FString Critic  = URawrXDPromptEngineLibrary::BuildCritic(TEXT("analyze this"));
FString Auditor = URawrXDPromptEngineLibrary::BuildAuditor(TEXT("review this"));
```

### Blueprint Usage
All functions are exposed as BlueprintCallable nodes under **RawrXD | PromptEngine**:
- **Classify Context** — Returns mode enum + score + name
- **Build Critic Prompt** — Auto-classifies and generates critic text
- **Build Auditor Prompt** — Auto-classifies and generates auditor text
- **Interpolate Template** — Custom template with {{CONTEXT}} replacement
- **Force Classification Mode** — Override auto-detection
- **Is Engine Available** — Runtime DLL availability check

## Exported Functions

| Function | Ordinal | Purpose |
|----------|---------|---------|
| `PromptGen_AnalyzeContext` | @1 | Classify input text → mode + score |
| `PromptGen_BuildCritic` | @2 | Generate critic prompt |
| `PromptGen_BuildAuditor` | @3 | Generate auditor prompt |
| `PromptGen_Interpolate` | @4 | Template interpolation |
| `PromptGen_GetTemplate` | @5 | Get raw template pointer |
| `PromptGen_ForceMode` | @6 | Override classification |
| `PromptGen_ClassifyToStruct` | @10 | Struct-return classify (FFI-safe) |
| `PromptGen_GetVersion` | @11 | Version query |
| `PromptGen_GetModeName` | @12 | Mode → string |

## Classification Modes

| ID | Name | Density Threshold | Weight |
|----|------|-------------------|--------|
| 0 | GENERIC | — | Default fallback |
| 1 | CASUAL | ≥2 | 1x |
| 2 | CODE | ≥2 | 1x (+5 for code fences) |
| 3 | SECURITY | ≥1 | 2x (safety-critical) |
| 4 | SHELL | ≥1 | 2x (dangerous commands) |
| 5 | ENTERPRISE | ≥2 | 1x |

## Thread Safety

| Function | Thread Safe | Notes |
|----------|-------------|-------|
| AnalyzeContext | ✅ | Read-only keyword scan |
| BuildCritic | ✅ | Uses spinlocked scratch |
| BuildAuditor | ✅ | Uses spinlocked scratch |
| Interpolate | ✅ | Caller-owned buffers |
| GetTemplate | ✅ | Returns static pointer |
| ForceMode | ⚠️ | Global state — game thread only |
| GetVersion | ✅ | Returns constant |
| GetModeName | ✅ | Returns static pointer |
