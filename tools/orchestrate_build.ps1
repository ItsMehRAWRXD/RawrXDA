# MONOLITHIC MASM IDE - Complete Build & Link Orchestrator
# Executes all remediation steps in optimized parallel batches

param(
    [switch]$SkipForensics,
    [switch]$SkipRebuild,
    [switch]$CleanFirst
)

$ErrorActionPreference = "Continue"
$toolsDir = "D:\rawrxd\tools"
$buildDir = "D:\rawrxd\build"

# Ensure MSVC + NMake tools are on PATH for any cmake/nmake invocations.
$vsEnv = Join-Path $toolsDir "ensure_vsenv.ps1"
if (Test-Path $vsEnv) {
    & $vsEnv | Out-Null
}

Write-Host @"
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║        🔥 MONOLITHIC MASM IDE BUILD - COMPLETE ORCHESTRATION 🔥            ║
║                                                                            ║,.
║  Executing parallel remediation to fix section overflow and link the      ║
║  complete RawrXD IDE from monolithic MASM + C++ components                ║
║                                                                            ║
╚════════════════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

# Clean build artifacts if requested
if ($CleanFirst) {
    Write-Host "`n🧹 Cleaning previous build artifacts..." -ForegroundColor Yellow
    Remove-Item "$buildDir\*.exe", "$buildDir\*.dll", "$buildDir\*.lib", "$buildDir\*.log" -Force -ErrorAction SilentlyContinue
    Write-Host "   ✅ Clean complete" -ForegroundColor Green
}

# Ensure directories exist
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

# =============================================================================
# PHASE 1: FORENSIC ANALYSIS
# =============================================================================
if (-not $SkipForensics) {
    Write-Host "`n" + ("=" * 80) -ForegroundColor Cyan
    Write-Host "PHASE 1: COFF FORENSICS & CONFLICT DETECTION" -ForegroundColor Cyan
    Write-Host ("=" * 80) -ForegroundColor Cyan
    
    $forensicsScript = Join-Path $toolsDir "monolithic_forensics.ps1"
    if (Test-Path $forensicsScript) {
        & $forensicsScript
        Start-Sleep -Seconds 2
    } else {
        Write-Host "⚠️  Forensics script not found: $forensicsScript" -ForegroundColor Yellow
    }
} else {
    Write-Host "`n⏭️  Skipping forensics phase" -ForegroundColor Gray
}

# =============================================================================
# PHASE 2: REBUILD MONOLITHIC OBJECTS (if sources exist)
# =============================================================================
if (-not $SkipRebuild) {
    Write-Host "`n" + ("=" * 80) -ForegroundColor Cyan
    Write-Host "PHASE 2: REBUILD MONOLITHIC ASM WITH /BIGOBJ" -ForegroundColor Cyan
    Write-Host ("=" * 80) -ForegroundColor Cyan
    
    $rebuildScript = Join-Path $toolsDir "rebuild_monolithic.ps1"
    if (Test-Path $rebuildScript) {
        & $rebuildScript
        Start-Sleep -Seconds 2
    } else {
        Write-Host "⚠️  Rebuild script not found: $rebuildScript" -ForegroundColor Yellow
    }
} else {
    Write-Host "`n⏭️  Skipping rebuild phase" -ForegroundColor Gray
}

# =============================================================================
# PHASE 3: PARALLEL LINKING STRATEGIES
# =============================================================================
Write-Host "`n" + ("=" * 80) -ForegroundColor Cyan
Write-Host "PHASE 3: EXECUTE PARALLEL LINKING STRATEGIES" -ForegroundColor Cyan
Write-Host ("=" * 80) -ForegroundColor Cyan

$linkScript = Join-Path $toolsDir "link_strategies.ps1"
if (Test-Path $linkScript) {
    & $linkScript
} else {
    Write-Host "❌ Link strategies script not found: $linkScript" -ForegroundColor Red
    exit 1
}

# =============================================================================
# PHASE 4: VERIFICATION & FINAL OUTPUT
# =============================================================================
Write-Host "`n" + ("=" * 80) -ForegroundColor Cyan
Write-Host "PHASE 4: VERIFICATION & OUTPUT" -ForegroundColor Cyan
Write-Host ("=" * 80) -ForegroundColor Cyan

# Find successful builds
$candidates = @(
    "RawrXD_StrategyA.exe",
    "RawrXD_StrategyB.exe",
    "RawrXD_StrategyC.exe"
)

$successful = @()
foreach ($candidate in $candidates) {
    $path = Join-Path $buildDir $candidate
    if (Test-Path $path) {
        $successful += $path
    }
}

if ($successful.Count -gt 0) {
    Write-Host "`n🎉 SUCCESS! Built $($successful.Count) executable(s):" -ForegroundColor Green
    
    foreach ($exe in $successful) {
        $info = Get-Item $exe
        $sizeMB = [math]::Round($info.Length / 1MB, 2)
        Write-Host "   ✅ $($info.Name) - $sizeMB MB" -ForegroundColor Green
    }
    
    # Copy best candidate to final output
    $best = $successful[0]
    $finalPath = Join-Path $buildDir "RawrXD_Full_IDE.exe"
    Copy-Item $best $finalPath -Force
    
    Write-Host "`n🏆 FINAL OUTPUT: $finalPath" -ForegroundColor Cyan
    Write-Host "   Size: $([math]::Round((Get-Item $finalPath).Length / 1MB, 2)) MB" -ForegroundColor Green
    
    # Quick validation
    Write-Host "`n🔍 Quick Validation:" -ForegroundColor Yellow
    
    # Check if it's a valid PE executable
    $bytes = [System.IO.File]::ReadAllBytes($finalPath)
    $isPE = ($bytes[0] -eq 0x4D -and $bytes[1] -eq 0x5A)  # MZ header
    
    if ($isPE) {
        Write-Host "   ✅ Valid PE executable (MZ header present)" -ForegroundColor Green
    } else {
        Write-Host "   ❌ Invalid PE format!" -ForegroundColor Red
    }
    
    # Try to get file version info
    try {
        $versionInfo = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($finalPath)
        if ($versionInfo.FileDescription) {
            Write-Host "   ✅ File version info: $($versionInfo.FileDescription)" -ForegroundColor Green
        }
    } catch {
        Write-Host "   ⚠️  Could not read version info (may be normal)" -ForegroundColor Yellow
    }
    
    Write-Host "`n📋 Next Steps:" -ForegroundColor Cyan
    Write-Host "   1. Test launch: & '$finalPath'" -ForegroundColor White
    Write-Host "   2. Check exports: dumpbin /EXPORTS '$finalPath'" -ForegroundColor White
    Write-Host "   3. Run full IDE: Start-Process '$finalPath'" -ForegroundColor White
    
} else {
    Write-Host "`n❌ ALL LINKING STRATEGIES FAILED" -ForegroundColor Red
    Write-Host "   Check logs in: $buildDir" -ForegroundColor Yellow
    Write-Host "   Common issues:" -ForegroundColor Yellow
    Write-Host "     • Missing monolithic ASM sources" -ForegroundColor Gray
    Write-Host "     • Section overflow in large C++ objects" -ForegroundColor Gray
    Write-Host "     • Symbol collisions" -ForegroundColor Gray
    Write-Host "     • Incomplete MSVC toolchain setup" -ForegroundColor Gray
    
    # Show log excerpts
    Write-Host "`n📄 Log excerpts (last 10 lines each):" -ForegroundColor Yellow
    Get-ChildItem "$buildDir\*.log" | ForEach-Object {
        Write-Host "`n   --- $($_.Name) ---" -ForegroundColor Gray
        Get-Content $_.FullName -Tail 10 | ForEach-Object { Write-Host "   $_" -ForegroundColor DarkGray }
    }
    
    exit 1
}

Write-Host "`n" + ("=" * 80) -ForegroundColor Cyan
Write-Host "✅ ORCHESTRATION COMPLETE" -ForegroundColor Green
Write-Host ("=" * 80) -ForegroundColor Cyan
