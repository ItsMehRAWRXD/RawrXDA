# TODOAutoResolver v2

Pattern-aware TODO resolver for RawrXD with AST-based classification, dynamic template generation, and optional MASM bridge export.

## Quick Start

```powershell
# From D:\lazy init ide\scripts
Import-Module .\TODOAutoResolver_v2.psm1 -Force
Invoke-IntelligentTODOResolution -Mode Hybrid -ConfidenceThreshold 0.80 -GenerateReport -WhatIf
```

## Runner

```powershell
# Safe read-only run (WhatIf)
.\Run-TODOAutoResolver_v2.ps1
```

## Key Commands

```powershell
# Pattern dashboard
Show-PatternDashboard

# Pattern-only auto-fix with explicit confirmation
Invoke-IntelligentTODOResolution -Mode PatternOnly -ConfidenceThreshold 0.85

# Export MASM bridge stub
Export-PatternMASMBridge -OutputPath "D:\lazy init ide\src\RawrXD_PatternBridge.asm"
```

## Notes
- Default mode uses `-WhatIf` in the runner to avoid changing files.
- Use `-WhatIf` or `-Confirm` when testing on large codebases.
- Reports are written next to the module as `PatternReport-<id>.html`.
