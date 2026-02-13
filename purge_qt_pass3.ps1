#!/usr/bin/env pwsh
# Third-pass: Fix override on destructors where base class was removed
$ErrorActionPreference = 'Continue'
$srcRoot = "D:\rawrxd\src"
$dirs = @("$srcRoot\agent", "$srcRoot\telemetry", "$srcRoot\terminal")
$totalModified = 0

# Files that need override removed from destructors 
# (classes that no longer inherit from anything)
# IDEAgentBridgeWithHotPatching still inherits from IDEAgentBridge, so keep override there

foreach ($dir in $dirs) {
    if (!(Test-Path $dir)) { continue }
    $files = Get-ChildItem -Path "$dir\*" -Include *.hpp,*.cpp,*.h | Where-Object { $_.FullName -notlike "*\.qt_backup\*" }
    foreach ($file in $files) {
        $content = [System.IO.File]::ReadAllText($file.FullName)
        $original = $content

        # Remove override from destructors in classes that no longer have base classes
        # We check: if class declaration has no ": public SomeBase" then remove override
        # Simple approach: just target the specific known classes

        # AgenticFailureDetector (no base)
        $content = $content -replace '(~AgenticFailureDetector\(\))\s+override', '$1'
        # AgenticPuppeteer (no base)
        $content = $content -replace '(~AgenticPuppeteer\(\))\s+override', '$1'
        # GGUFProxyServer (no base, was QTcpServer)
        $content = $content -replace '(~GGUFProxyServer\(\))\s+override', '$1'
        # IDEAgentBridge (no base)
        $content = $content -replace '(~IDEAgentBridge\(\))\s+override', '$1'
        # ModelInvoker (no base)
        $content = $content -replace '(~ModelInvoker\(\))\s+override', '$1'
        # ActionExecutor (no base)
        $content = $content -replace '(~ActionExecutor\(\))\s+override', '$1'
        # SandboxedTerminal (no base)
        $content = $content -replace '(~SandboxedTerminal\(\))\s+override', '$1'
        # ZeroRetentionManager (no base)
        $content = $content -replace '(~ZeroRetentionManager\(\))\s+override', '$1'
        # AutoBootstrap (no base)
        $content = $content -replace '(~AutoBootstrap\(\))\s+override', '$1'
        # AutoUpdate (no base)
        $content = $content -replace '(~AutoUpdate\(\))\s+override', '$1'
        # CodeSigner (no base)
        $content = $content -replace '(~CodeSigner\(\))\s+override', '$1'
        # HotReload (no base)
        $content = $content -replace '(~HotReload\(\))\s+override', '$1'
        # MetaLearn (no base)
        $content = $content -replace '(~MetaLearn\(\))\s+override', '$1'
        # Planner (no base)
        $content = $content -replace '(~Planner\(\))\s+override', '$1'
        # ReleaseAgent (no base)
        $content = $content -replace '(~ReleaseAgent\(\))\s+override', '$1'
        # Rollback (no base)
        $content = $content -replace '(~Rollback\(\))\s+override', '$1'
        # SelfCode (no base)
        $content = $content -replace '(~SelfCode\(\))\s+override', '$1'
        # SelfPatch (no base)
        $content = $content -replace '(~SelfPatch\(\))\s+override', '$1'
        # SelfTest (no base)
        $content = $content -replace '(~SelfTest\(\))\s+override', '$1'
        # SentryIntegration (no base)
        $content = $content -replace '(~SentryIntegration\(\))\s+override', '$1'
        # SignBinary (no base)
        $content = $content -replace '(~SignBinary\(\))\s+override', '$1'
        # TelemetryCollector (no base)
        $content = $content -replace '(~TelemetryCollector\(\))\s+override', '$1'
        # ZeroTouch (no base)
        $content = $content -replace '(~ZeroTouch\(\))\s+override', '$1'
        # AgenticCopilotBridge (no base)
        $content = $content -replace '(~AgenticCopilotBridge\(\))\s+override', '$1'

        # GGUFProxyServer: remove "void incomingConnection(...) override" → just regular method
        $content = $content -replace '(void\s+incomingConnection\([^)]*\))\s+override', '$1'

        # Note: IDEAgentBridgeWithHotPatching -> inherits IDEAgentBridge, keep override if IDEAgentBridge has virtual destructor
        # Keep it for now

        # Write if changed
        if ($content -ne $original) {
            [System.IO.File]::WriteAllText($file.FullName, $content)
            $totalModified++
            Write-Host "MODIFIED: $($file.FullName)"
        }
    }
}

Write-Host "`n=== Override cleanup complete: $totalModified files modified ==="
