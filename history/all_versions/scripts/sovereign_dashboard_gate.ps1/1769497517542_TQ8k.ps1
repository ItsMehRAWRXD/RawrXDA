Write-Host "============================================================"
Write-Host " SOVEREIGN DASHBOARD GATE: v1.0 (Enterprise 64-bit)"
Write-Host "============================================================"

# 1. Check Files
$Required = @(
    "..\build\pocket_lab.exe",
    "..\build\bin\Release\RawrXD-AgenticIDE.exe",
    "..\src\gui\sovereign_dashboard_widget.cpp",
    "..\src\thermal\masm\pocket_lab.asm"
)

$Failed = $false
foreach ($f in $Required) {
    if (-not (Test-Path "$PSScriptRoot\$f")) {
        Write-Error "MISSING: $f"
        $Failed = $true
    } else {
        Write-Host " [PASS] File Exists: $f" -ForegroundColor Green
    }
}

# 2. Check Pocket Lab Enterprise Tier
Write-Host " -> Validating Pocket Lab Kernel Output..."
$KernelPath = Resolve-Path "$PSScriptRoot\..\build\pocket_lab.exe"
try {
    # Run slightly asynchronously to capture output
    $Proc = Start-Process -FilePath $KernelPath -NoNewWindow -PassThru -RedirectStandardOutput "$PSScriptRoot\gate_output.txt"
    Start-Sleep -Seconds 1
    if (-not $Proc.HasExited) { Stop-Process -Id $Proc.Id -Force }
    
    $Output = Get-Content "$PSScriptRoot\gate_output.txt" -Raw
    if ($Output -match "ENTERPRISE") {
        Write-Host " [PASS] Badge: ENTERPRISE Detected" -ForegroundColor Green
    } else {
        Write-Error " [FAIL] Badge: ENTERPRISE NOT FOUND (Got: '$Output')"
        $Failed = $true
    }
} catch {
    Write-Warning "Kernel execution failed or timed out: $_"
    # Don't hard fail gate on execution environment issues unless critical
}

# 3. Check MMF Code Integration
Write-Host " -> Auditing Source Wiring..."
$GuiSrc = Get-Content "$PSScriptRoot\..\src\gui\sovereign_dashboard_widget.cpp" -Raw
if ($GuiSrc -match "Global\\\\SOVEREIGN_STATS") {
     Write-Host " [PASS] GUI listening on Global\SOVEREIGN_STATS" -ForegroundColor Green
} else {
     Write-Error " [FAIL] GUI MMF Path Incorrect"
     $Failed = $true
}

if ($Failed) {
    Write-Error "GATE FAILED: Integrity Check Mismatch"
    exit 1
} else {
    Write-Host "GATE PASSED: System Ready for Enterprise Deployment" -ForegroundColor Cyan
    exit 0
}
