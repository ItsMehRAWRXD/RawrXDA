# Production Scaffolding Plan (PowerShell-First)

## Scope
This plan scaffolds missing production deployment components in **pure PowerShell** without altering existing complex logic.

## 1) Custom Model Loaders
- **Module**: `RawrXD.ModelLoader.psm1`
- **Config**: `model_config.json` (already present)
- **Backends**:
  - `OLLAMA_LOCAL`: Implemented via `/api/generate`
  - `LOCAL_GGUF`: Scaffolded for `RawrXD-ModelLoader.exe`
  - `OPENAI/ANTHROPIC/GOOGLE/AZURE_OPENAI/AWS_BEDROCK/MOONSHOT`: Scaffolded with consistent error responses
- **Observability**: Structured logs + trace spans on each request
- **Tests**: `tests/RawrXD.Scaffolds.Tests.ps1`

## 2) Agentic / Autonomous Systems
- **Module**: `RawrXD.Agentic.Autonomy.psm1`
- **Capabilities**:
  - Goal setter
  - Planner stub
  - Action executor stub
  - Background loop
- **Future**: Pluggable action handlers, safety filters, persistence

## 3) Win32 Deployment
- **Module**: `RawrXD.Win32Deployment.psm1`
- **Capabilities**:
  - Prereq checks (CMake, MinGW)
  - Build orchestration (dry-run enabled)

## 4) Observability, Metrics, Tracing
- **Modules**:
  - `RawrXD.Metrics.psm1` (Prometheus text export + optional HTTP listener)
  - `RawrXD.Tracing.psm1` (JSONL spans)
  - `RawrXD.ErrorHandling.psm1` (centralized safe execution wrapper)

## 5) Next Implementation Steps
1. Expand model backend implementations (OpenAI/Anthropic/Google/Azure/AWS)
2. Wire agentic actions into `RawrXD.Core.psm1`
3. Add Win32 packaging + installer pipeline
4. Add CI checks for scaffolds

## 6) Verification
```powershell
pwsh .\tests\RawrXD.Scaffolds.Tests.ps1
```

Status: **Scaffold complete; ready for deeper implementation.**
