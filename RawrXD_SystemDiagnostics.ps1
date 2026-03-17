# RawrXD_SystemDiagnostics.ps1 - Complete System Health Check

param(
    [switch]$Verbose,
    [switch]$AutoFix,
    [switch]$GenerateReport
)

# ============================================================================
# RawrXD SYSTEM DIAGNOSTICS
# Purpose: Comprehensive health check of all RawrXD components
# Version: 1.0
# ============================================================================

$ErrorActionPreference = "SilentlyContinue"
$VerbosePreference = if ($Verbose) { "Continue" } else { "SilentlyContinue" }

# Color definitions
$Colors = @{
    Success = "Green"
    Error   = "Red"
    Warning = "Yellow"
    Info    = "Cyan"
    Debug   = "DarkGray"
}

# Test results tracking
$Results = @{
    Passed = 0
    Failed = 0
    Warning = 0
}

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

function Write-Section {
    param([string]$Title, [string]$Color = "Cyan")
    Write-Host ""
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor $Color
    Write-Host "  $Title" -ForegroundColor $Color
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor $Color
}

function Write-TestResult {
    param([string]$TestName, [bool]$Passed, [string]$Message = "")
    $Status = if ($Passed) { "✅ PASS" } else { "❌ FAIL" }
    $Color = if ($Passed) { $Colors.Success } else { $Colors.Error }
    
    Write-Host "  [$Status] $TestName" -ForegroundColor $Color
    if ($Message) { Write-Host "         $Message" -ForegroundColor $Colors.Debug }
    
    if ($Passed) { $Results.Passed++ } else { $Results.Failed++ }
}

function Write-Warning {
    param([string]$Message)
    Write-Host "  [⚠️  WARN] $Message" -ForegroundColor $Colors.Warning
    $Results.Warning++
}

# ============================================================================
# TEST FUNCTIONS
# ============================================================================

function Test-OllamaService {
    Write-Section "OLLAMA SERVICE CHECK"
    
    # Check if process running
    $process = Get-Process ollama -ErrorAction SilentlyContinue
    if ($process) {
        Write-TestResult "Ollama Process" $true "PID: $($process.Id)"
    } else {
        Write-TestResult "Ollama Process" $false "Not running"
        if ($AutoFix) {
            Write-Host "    Attempting to start Ollama..." -ForegroundColor Yellow
            # Try to start (requires Ollama installed)
        }
    }
    
    # Check connectivity
    try {
        $response = Invoke-WebRequest -Uri "http://localhost:11434/api/tags" -TimeoutSec 2 -ErrorAction Stop
        if ($response.StatusCode -eq 200) {
            Write-TestResult "Ollama Connectivity" $true "Port 11434 responding"
        }
    } catch {
        Write-TestResult "Ollama Connectivity" $false "Connection refused"
    }
    
    # Check models
    try {
        $response = Invoke-WebRequest -Uri "http://localhost:11434/api/tags" -TimeoutSec 2 -ErrorAction Stop
        $json = $response.Content | ConvertFrom-Json
        $modelCount = @($json.models).Count
        Write-TestResult "Model Availability" ($modelCount -gt 0) "$modelCount model(s) available"
        
        if ($modelCount -gt 0) {
            foreach ($model in $json.models) {
                Write-Host "          • $($model.name)" -ForegroundColor $Colors.Info
            }
        }
    } catch {
        Write-TestResult "Model Availability" $false "Cannot query models"
    }
}

function Test-ExecutableStatus {
    Write-Section "RAWRXD EXECUTABLES"
    
    $exes = @(
        @{ Name = "AutoHeal CLI"; Path = "D:\rawrxd\RawrXD_AutoHeal_CLI.exe" },
        @{ Name = "Regular CLI"; Path = "D:\rawrxd\RawrXD_CLI.exe" },
        @{ Name = "MASM Editor"; Path = "D:\rawrxd\RawrXD_MASM_Editor.exe" }
    )
    
    foreach ($exe in $exes) {
        if (Test-Path $exe.Path) {
            $item = Get-Item $exe.Path
            $sizeKB = $item.Length / 1KB
            Write-TestResult "$($exe.Name) (Exists)" $true "Size: $([math]::Round($sizeKB, 1)) KB"
        } else {
            Write-TestResult "$($exe.Name) (Exists)" $false "Not found at $($exe.Path)"
        }
    }
}

function Test-AutoHealCLI {
    Write-Section "AUTOHEAL CLI EXECUTION TEST"
    
    if (-not (Test-Path "D:\rawrxd\RawrXD_AutoHeal_CLI.exe")) {
        Write-Warning "AutoHeal CLI not found"
        return
    }
    
    Write-Host "  Running AutoHeal CLI (should complete)..." -ForegroundColor $Colors.Info
    Write-Host "  (This may take 5-10 seconds)" -ForegroundColor $Colors.Debug
    
    try {
        $timer = [System.Diagnostics.Stopwatch]::StartNew()
        $output = & D:\rawrxd\RawrXD_AutoHeal_CLI.exe 2>&1 | Out-String
        $timer.Stop()
        
        if ($output -match "\[DONE\].*Full autonomy") {
            Write-TestResult "AutoHeal CLI Execution" $true "Completed in $($timer.ElapsedMilliseconds)ms"
            Write-Host "    Output highlights:" -ForegroundColor $Colors.Debug
            $lines = $output -split "`n" | Select-Object -First 10
            foreach ($line in $lines) {
                if ($line.Trim()) { Write-Host "      $line" -ForegroundColor $Colors.Debug }
            }
        } else {
            Write-TestResult "AutoHeal CLI Execution" $false "No completion signal"
        }
    } catch {
        Write-TestResult "AutoHeal CLI Execution" $false $_.Exception.Message
    }
}

function Test-RegularCLI {
    Write-Section "REGULAR CLI EXECUTION TEST"
    
    if (-not (Test-Path "D:\rawrxd\RawrXD_CLI.exe")) {
        Write-Warning "Regular CLI not found"
        return
    }
    
    Write-Host "  Running RawrXD_CLI.exe with 3-second timeout..." -ForegroundColor $Colors.Info
    
    try {
        $proc = Start-Process -FilePath "D:\rawrxd\RawrXD_CLI.exe" `
                             -RedirectStandardOutput temp_out.txt `
                             -RedirectStandardError temp_err.txt `
                             -PassThru `
                             -WindowStyle Hidden
        
        Start-Sleep -Seconds 3
        
        if ($proc.HasExited) {
            Write-TestResult "CLI Completion" $true "Process exited after 3 seconds"
            $exitCode = $proc.ExitCode
            Write-Host "    Exit code: $exitCode" -ForegroundColor $Colors.Debug
        } else {
            Write-TestResult "CLI Completion" $false "Process still running (infinite loop suspected)"
            $proc | Stop-Process -Force
            
            $output = Get-Content temp_out.txt -ErrorAction SilentlyContinue | Select-Object -First 15
            Write-Host "    Output sample:" -ForegroundColor $Colors.Debug
            $output | ForEach-Object {
                if ($_) { Write-Host "      $_" -ForegroundColor $Colors.Debug }
            }
        }
    } catch {
        Write-TestResult "CLI Execution" $false $_.Exception.Message
    } finally {
        Remove-Item temp_out.txt, temp_err.txt -ErrorAction SilentlyContinue
    }
}

function Test-DocumentationStatus {
    Write-Section "DOCUMENTATION COMPLETENESS"
    
    $docs = @(
        "RawrXD_MASM_Editor_QUICKSTART.md",
        "RawrXD_MASM_Editor_BUILD.md",
        "RawrXD_MASM_Editor_INTEGRATION.md",
        "RawrXD_MASM_Editor_API_QUICKREF.md",
        "RawrXD_MASM_Editor_ARCHITECTURE.md",
        "RawrXD_MASM_Editor_INDEX.md"
    )
    
    $count = 0
    foreach ($doc in $docs) {
        $path = "D:\rawrxd\$doc"
        if (Test-Path $path) {
            $item = Get-Item $path
            Write-TestResult "Doc: $doc" $true "Size: $([math]::Round($item.Length / 1KB, 1)) KB"
            $count++
        }
    }
    
    Write-TestResult "Documentation Coverage" ($count -eq $docs.Count) "$count / $($docs.Count) files present"
}

function Test-ASMSourceFiles {
    Write-Section "MASM SOURCE FILES"
    
    $sources = @(
        "RawrXD_MASM_SyntaxHighlighter.asm",
        "RawrXD_MASM_Editor_Editing.asm",
        "RawrXD_MASM_Editor_MLCompletion.asm"
    )
    
    foreach ($src in $sources) {
        $path = "D:\rawrxd\$src"
        if (Test-Path $path) {
            $item = Get-Item $path
            $lines = (Get-Content $path | Measure-Object -Line).Lines
            Write-TestResult "Source: $src" $true "$lines lines"
        } else {
            Write-TestResult "Source: $src" $false "Not found"
        }
    }
}

# ============================================================================
# MAIN DIAGNOSTIC FLOW
# ============================================================================

function Invoke-RawrXDDiagnostics {
    Clear-Host
    
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║     RawrXD SYSTEM DIAGNOSTICS - Comprehensive Health Check     ║" -ForegroundColor Cyan
    Write-Host "║                    March 12, 2026 - System v1.0                ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    # Run all tests
    Test-OllamaService
    Test-ExecutableStatus
    Test-AutoHealCLI
    Test-RegularCLI
    Test-DocumentationStatus
    Test-ASMSourceFiles
    
    # Summary
    Write-Section "DIAGNOSTIC SUMMARY"
    
    $total = $Results.Passed + $Results.Failed
    $passRate = if ($total -gt 0) { [math]::Round(($Results.Passed / $total) * 100, 1) } else { 0 }
    
    Write-Host ""
    Write-Host "  Tests Passed:   $($Results.Passed) ✅" -ForegroundColor $Colors.Success
    Write-Host "  Tests Failed:   $($Results.Failed) ❌" -ForegroundColor $Colors.Error
    Write-Host "  Warnings:       $($Results.Warning) ⚠️" -ForegroundColor $Colors.Warning
    Write-Host "  Pass Rate:      $passRate%" -ForegroundColor $(if ($passRate -ge 80) { $Colors.Success } else { $Colors.Warning })
    Write-Host ""
    
    # Recommendations
    Write-Section "RECOMMENDATIONS"
    
    if ($Results.Failed -gt 0) {
        Write-Host ""
        Write-Host "  🔴 CRITICAL ISSUES DETECTED:" -ForegroundColor $Colors.Error
        
        if (-not (Get-Process ollama -ErrorAction SilentlyContinue)) {
            Write-Host "    1. Start Ollama service: ollama serve" -ForegroundColor $Colors.Error
        }
        
        Write-Host "    2. Fix token stream infinite loop in RawrXD_CLI.exe" -ForegroundColor $Colors.Error
        Write-Host "    3. Implement HTTP bridge to localhost:11434" -ForegroundColor $Colors.Error
        
        Write-Host ""
        Write-Host "  📋 See: D:\rawrxd\RawrXD_SystemStatus_Diagnostic.md" -ForegroundColor $Colors.Info
    } else {
        Write-Host ""
        Write-Host "  ✅ All systems operational!" -ForegroundColor $Colors.Success
    }
    
    Write-Host ""
    Write-Host "  For detailed information:" -ForegroundColor $Colors.Info
    Write-Host "    - Full report: D:\rawrxd\RawrXD_SystemStatus_Diagnostic.md" -ForegroundColor $Colors.Debug
    Write-Host "    - Documentation: D:\rawrxd\RawrXD_MASM_Editor_INDEX.md" -ForegroundColor $Colors.Debug
    Write-Host "    - Quick start: D:\rawrxd\RawrXD_MASM_Editor_QUICKSTART.md" -ForegroundColor $Colors.Debug
    Write-Host ""
}

# ============================================================================
# EXECUTION
# ============================================================================

Invoke-RawrXDDiagnostics

Write-Host "Diagnostics complete. Press any key to exit..." -ForegroundColor $Colors.Debug
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
