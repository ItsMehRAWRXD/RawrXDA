#!/usr/bin/env pwsh
# testbench.ps1 - Runtime smoke tests for all major subsystems
# Usage: .\testbench.ps1 [-BuildDir "build"] [-Verbose]

param(
    [string]$BuildDir = "build",
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$script:FailCount = 0
$script:PassCount = 0

function Test-Widget {
    param(
        [string]$Name,
        [string]$Flag,
        [string]$ExpectedPattern,
        [int]$TimeoutSec = 10
    )
    
    Write-Host "`n[TEST] $Name..." -ForegroundColor Cyan
    
    $exePath = Join-Path $BuildDir "bin-msvc\Release\RawrXD-QtShell.exe"
    if (-not (Test-Path $exePath)) {
        Write-Host "  ❌ FAIL: Binary not found at $exePath" -ForegroundColor Red
        $script:FailCount++
        return $false
    }
    
    try {
        $proc = Start-Process -FilePath $exePath -ArgumentList $Flag -NoNewWindow -PassThru -RedirectStandardOutput "test_output.tmp" -RedirectStandardError "test_error.tmp"
        $completed = $proc.WaitForExit($TimeoutSec * 1000)
        
        if (-not $completed) {
            $proc.Kill()
            Write-Host "  ❌ FAIL: Timeout after ${TimeoutSec}s" -ForegroundColor Red
            $script:FailCount++
            return $false
        }
        
        $output = Get-Content "test_output.tmp" -Raw -ErrorAction SilentlyContinue
        $stderr = Get-Content "test_error.tmp" -Raw -ErrorAction SilentlyContinue
        
        if ($Verbose) {
            Write-Host "  STDOUT: $output" -ForegroundColor Gray
            Write-Host "  STDERR: $stderr" -ForegroundColor Gray
        }
        
        if ($proc.ExitCode -ne 0) {
            Write-Host "  ❌ FAIL: Exit code $($proc.ExitCode)" -ForegroundColor Red
            $script:FailCount++
            return $false
        }
        
        if ($ExpectedPattern -and $output -notmatch $ExpectedPattern) {
            Write-Host "  ❌ FAIL: Output missing pattern '$ExpectedPattern'" -ForegroundColor Red
            if ($Verbose) {
                Write-Host "  Got: $output" -ForegroundColor Yellow
            }
            $script:FailCount++
            return $false
        }
        
        Write-Host "  ✅ PASS" -ForegroundColor Green
        $script:PassCount++
        return $true
        
    } finally {
        Remove-Item "test_output.tmp" -ErrorAction SilentlyContinue
        Remove-Item "test_error.tmp" -ErrorAction SilentlyContinue
    }
}

Write-Host "════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host "  RawrXD-QtShell Runtime Smoke Tests" -ForegroundColor Magenta
Write-Host "════════════════════════════════════════════════════" -ForegroundColor Magenta

Test-Widget -Name "CloudRunner Init" -Flag "--cloud-smoke" -ExpectedPattern "(CloudRunner ready|Workflow dispatched)"
Test-Widget -Name "LSP Client" -Flag "--lsp-smoke" -ExpectedPattern "(LSP started|serverCapabilities|clangd)"
Test-Widget -Name "InlineChat" -Flag "--chat-smoke" -ExpectedPattern "(InlineChat ready|ESC handled)"
Test-Widget -Name "PluginManager" -Flag "--plugin-smoke" -ExpectedPattern "(plugin|manifest|\.json)"
Test-Widget -Name "GUI Launch" -Flag "--version" -ExpectedPattern "(v0\.|RawrXD|QtShell)" -TimeoutSec 5

Write-Host "`n[SIZE CHECK]" -ForegroundColor Cyan
$exePath = Join-Path $BuildDir "bin-msvc\Release\RawrXD-QtShell.exe"
if (Test-Path $exePath) {
    $sizeBytes = (Get-Item $exePath).Length
    $sizeKB = [math]::Round($sizeBytes / 1024, 1)
    
    Write-Host "  Binary size: $sizeKB KB ($sizeBytes bytes)" -ForegroundColor White
    
    if ($sizeBytes -gt 204800) {
        Write-Host "  ⚠️  WARNING: Exceeds 200 KB budget!" -ForegroundColor Yellow
        $script:FailCount++
    } else {
        Write-Host "  ✅ Within 200 KB budget" -ForegroundColor Green
        $script:PassCount++
    }
}

Write-Host "`n════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host "  Results: $script:PassCount passed, $script:FailCount failed" -ForegroundColor $(if ($script:FailCount -eq 0) { "Green" } else { "Red" })
Write-Host "════════════════════════════════════════════════════" -ForegroundColor Magenta

if ($script:FailCount -gt 0) {
    Write-Host "`n❌ Some tests failed. Fix before shipping." -ForegroundColor Red
    exit 1
} else {
    Write-Host "`n✅ All tests passed! Safe to ship." -ForegroundColor Green
    exit 0
}
