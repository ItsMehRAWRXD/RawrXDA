# PowerShell Deployment Scaffolds

This document tracks PowerShell-first deployment scaffolds for RawrXD.

## Modules Added
- `RawrXD.Metrics.psm1`
- `RawrXD.Tracing.psm1`
- `RawrXD.ErrorHandling.psm1`
- `RawrXD.ModelLoader.psm1`
- `RawrXD.Agentic.Autonomy.psm1`
- `RawrXD.Win32Deployment.psm1`

## Test Harness
- `tests/RawrXD.Scaffolds.Tests.ps1`

## Status
- Modules compile under PowerShell 5.1
- Instrumentation hooks are in place
- Backends and agentic actions are scaffolded

Next steps: implement backend-specific logic and plug into orchestrator.
