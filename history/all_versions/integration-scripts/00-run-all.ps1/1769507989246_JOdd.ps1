# 00-run-all.ps1
# Master integration script - runs all integration steps

param(
    [switch]$DryRun = $false,
    [switch]$SkipValidation = $false
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   RawrXD Complete Integration Pipeline v1.0       ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$steps = @(
    @{
        Name = "Step 1: Copy Missing Files"
        Script = "01-copy-missing-files.ps1"
        Args = @{ DryRun = $DryRun; Verbose = $true }
    },
    @{
        Name = "Step 2: Update CMakeLists.txt"
        Script = "02-update-cmake.ps1"
        Args = @{ DryRun = $DryRun; Backup = $true }
    },
    @{
        Name = "Step 3: Validate Integration"
        Script = "03-validate-integration.ps1"
        Args = @{}
        Skip = $SkipValidation
    }
)

$results = @()

foreach ($step in $steps) {
    if ($step.Skip) {
        Write-Host "⊗ Skipping: $($step.Name)" -ForegroundColor Yellow
        Write-Host ""
        continue
    }
    
    Write-Host "▶ Running: $($step.Name)" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    Write-Host ""
    
    $scriptPath = Join-Path $scriptDir $step.Script
    
    try {
        $params = @{}
        foreach ($arg in $step.Args.GetEnumerator()) {
            if ($arg.Value -is [bool] -and $arg.Value) {
                $params["-$($arg.Key)"] = $true
            } elseif ($arg.Value -isnot [bool]) {
                $params["-$($arg.Key)"] = $arg.Value
            }
        }
        
        & $scriptPath @params
        $exitCode = $LASTEXITCODE
        
        if ($exitCode -eq 0) {
            Write-Host ""
            Write-Host "✓ $($step.Name) completed successfully" -ForegroundColor Green
            $results += @{ Step = $step.Name; Status = "Success"; ExitCode = $exitCode }
        } else {
            Write-Host ""
            Write-Host "⚠ $($step.Name) completed with warnings (exit code: $exitCode)" -ForegroundColor Yellow
            $results += @{ Step = $step.Name; Status = "Warning"; ExitCode = $exitCode }
        }
    }
    catch {
        Write-Host ""
        Write-Host "✗ $($step.Name) failed: $($_.Exception.Message)" -ForegroundColor Red
        $results += @{ Step = $step.Name; Status = "Failed"; Error = $_.Exception.Message }
    }
    
    Write-Host ""
    Write-Host ""
}

# Summary
Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║              Integration Summary                   ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

foreach ($result in $results) {
    $icon = switch ($result.Status) {
        "Success" { "✓"; $color = "Green" }
        "Warning" { "⚠"; $color = "Yellow" }
        "Failed"  { "✗"; $color = "Red" }
    }
    Write-Host "  $icon $($result.Step)" -ForegroundColor $color
}

Write-Host ""

if ($DryRun) {
    Write-Host "DRY RUN COMPLETE" -ForegroundColor Magenta
    Write-Host "Run without -DryRun to perform actual integration" -ForegroundColor Magenta
} else {
    Write-Host "Next Steps:" -ForegroundColor Cyan
    Write-Host "  1. Review cmake-integration-instructions.txt" -ForegroundColor White
    Write-Host "  2. Update CMakeLists.txt with new source files" -ForegroundColor White
    Write-Host "  3. Run: cd build && cmake --build . --config Release" -ForegroundColor White
    Write-Host "  4. Test the integrated features" -ForegroundColor White
}

Write-Host ""
