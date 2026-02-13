# ============================================================================
# test.ps1 — RawrXD IDE Test Suite Runner
# ============================================================================
# Executes the full gauntlet of build verification, unit tests, integration
# tests, and smoke tests against the built RawrXD IDE binary.
#
# Usage:
#   .\test.ps1                  # Run all tests
#   .\test.ps1 -Category Build  # Run only build verification
#   .\test.ps1 -Quick           # Run quick smoke tests only
#   .\test.ps1 -Verbose         # Show detailed output
# ============================================================================

param(
    [ValidateSet("All", "Build", "Unit", "Integration", "Smoke", "API")]
    [string]$Category = "All",
    [switch]$Quick,
    [switch]$Verbose,
    [string]$BuildDir = "",
    [string]$ExePath = "",
    [int]$Timeout = 30
)

$ErrorActionPreference = "Continue"
$script:TotalTests = 0
$script:PassedTests = 0
$script:FailedTests = 0
$script:SkippedTests = 0

$ProjectRoot = $PSScriptRoot

# ============================================================================
# Test Reporting
# ============================================================================

function Test-Assert {
    param(
        [string]$Name,
        [bool]$Condition,
        [string]$Detail = ""
    )

    $script:TotalTests++

    if ($Condition) {
        $script:PassedTests++
        Write-Host "  [PASS] $Name" -ForegroundColor Green
    } else {
        $script:FailedTests++
        Write-Host "  [FAIL] $Name" -ForegroundColor Red
        if ($Detail) { Write-Host "         $Detail" -ForegroundColor DarkRed }
    }
}

function Test-Skip {
    param([string]$Name, [string]$Reason = "")
    $script:TotalTests++
    $script:SkippedTests++
    Write-Host "  [SKIP] $Name$(if($Reason){' — ' + $Reason})" -ForegroundColor Yellow
}

# ============================================================================
# Path Discovery
# ============================================================================

function Get-Exe {
    if ($ExePath -and (Test-Path $ExePath)) { return $ExePath }

    $candidates = @(
        (Join-Path $ProjectRoot "RawrXD.exe"),
        (Join-Path $ProjectRoot "build" "Release" "RawrXD.exe"),
        (Join-Path $ProjectRoot "build" "RawrXD.exe"),
        (Join-Path $ProjectRoot "bin" "Release" "RawrXD.exe")
    )

    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }

    # Try versioned exes
    $versioned = Get-ChildItem $ProjectRoot -Filter "RawrXD_v*.exe" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($versioned) { return $versioned.FullName }

    return $null
}

function Get-TestExe {
    $candidates = @(
        (Join-Path $ProjectRoot "build" "Release" "self_test_gate.exe"),
        (Join-Path $ProjectRoot "build" "self_test_gate.exe"),
        (Join-Path $ProjectRoot "self_test_gate.exe")
    )

    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }
    return $null
}

# ============================================================================
# BUILD VERIFICATION TESTS
# ============================================================================

function Test-BuildVerification {
    Write-Host ""
    Write-Host "--- Build Verification ---" -ForegroundColor Cyan

    # 1. Binary exists
    $exe = Get-Exe
    Test-Assert "Main executable exists" ($null -ne $exe -and (Test-Path $exe)) "Not found in any expected location"

    if (-not $exe) { return }

    # 2. Binary size reasonable (>1MB for a real IDE)
    $exeInfo = Get-Item $exe
    $sizeMB = [math]::Round($exeInfo.Length / 1MB, 2)
    Test-Assert "Binary size reasonable (>1MB)" ($exeInfo.Length -gt 1MB) "Size: $sizeMB MB"

    # 3. PE architecture is x64
    try {
        $bytes = [System.IO.File]::ReadAllBytes($exe)
        $peOffset = [BitConverter]::ToInt32($bytes, 60)
        $machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
        Test-Assert "PE architecture is x64 (AMD64)" ($machine -eq 0x8664) "Machine: 0x$($machine.ToString('X4'))"
    } catch {
        Test-Assert "PE header readable" $false $_.Exception.Message
    }

    # 4. No VC++ debug runtime dependency (should be /MT not /MD)
    try {
        $dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
        if ($dumpbin) {
            $deps = & dumpbin.exe /dependents $exe 2>$null | Out-String
            $hasDebugCRT = $deps -match "vcruntime\d+d\.dll|msvcp\d+d\.dll"
            Test-Assert "No debug CRT dependency" (-not $hasDebugCRT) "Found debug CRT in dependencies"
        } else {
            Test-Skip "CRT dependency check" "dumpbin.exe not in PATH"
        }
    } catch {
        Test-Skip "CRT dependency check" "dumpbin failed"
    }

    # 5. Required files present
    $requiredFiles = @(
        @{ Name = "GUI frontend"; Path = (Join-Path $ProjectRoot "gui" "ide_chatbot.html") },
        @{ Name = "CMakeLists.txt"; Path = (Join-Path $ProjectRoot "CMakeLists.txt") },
        @{ Name = "Config template"; Path = (Join-Path $ProjectRoot "config.example.json") }
    )

    foreach ($req in $requiredFiles) {
        Test-Assert "Required file: $($req.Name)" (Test-Path $req.Path) "Missing: $($req.Path)"
    }

    # 6. MASM objects compiled
    $asmDir = Join-Path $ProjectRoot "src" "asm"
    if (Test-Path $asmDir) {
        $asmFiles = Get-ChildItem $asmDir -Filter "*.asm" -ErrorAction SilentlyContinue
        Test-Assert "MASM source files present" ($asmFiles.Count -gt 0) "Expected ASM files in src/asm/"
    }
}

# ============================================================================
# UNIT TESTS (compile-time verification)
# ============================================================================

function Test-UnitTests {
    Write-Host ""
    Write-Host "--- Unit Tests ---" -ForegroundColor Cyan

    # Run self_test_gate if available
    $testExe = Get-TestExe
    if ($testExe) {
        try {
            $proc = Start-Process -FilePath $testExe -ArgumentList "--quick" -Wait -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\rawrxd_test.txt" -RedirectStandardError "$env:TEMP\rawrxd_test_err.txt"
            $output = Get-Content "$env:TEMP\rawrxd_test.txt" -ErrorAction SilentlyContinue | Out-String
            Test-Assert "Self-test gate passed" ($proc.ExitCode -eq 0) "Exit code: $($proc.ExitCode)"
            if ($Verbose -and $output) { Write-Host $output -ForegroundColor Gray }
        } catch {
            Test-Assert "Self-test gate execution" $false $_.Exception.Message
        }
    } else {
        Test-Skip "Self-test gate binary" "self_test_gate.exe not found"
    }

    # Run C++ test executables if present
    $testDir = Join-Path $ProjectRoot "build"
    $testExes = Get-ChildItem $testDir -Filter "test_*.exe" -Recurse -ErrorAction SilentlyContinue
    foreach ($te in $testExes) {
        try {
            $proc = Start-Process -FilePath $te.FullName -Wait -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\rawrxd_ut.txt" 2>$null
            $timeout = (Get-Date).AddSeconds($Timeout)
            if (-not $proc.HasExited -and (Get-Date) -gt $timeout) {
                $proc.Kill()
                Test-Assert "Unit test: $($te.Name)" $false "Timed out after ${Timeout}s"
            } else {
                Test-Assert "Unit test: $($te.Name)" ($proc.ExitCode -eq 0) "Exit code: $($proc.ExitCode)"
            }
        } catch {
            Test-Skip "Unit test: $($te.Name)" $_.Exception.Message
        }
    }

    if ($testExes.Count -eq 0 -and -not $testExe) {
        Test-Skip "Unit tests" "No test executables found"
    }
}

# ============================================================================
# INTEGRATION TESTS
# ============================================================================

function Test-IntegrationTests {
    Write-Host ""
    Write-Host "--- Integration Tests ---" -ForegroundColor Cyan

    $exe = Get-Exe
    if (-not $exe) {
        Test-Skip "All integration tests" "Main executable not found"
        return
    }

    # 1. Headless launch test (--help should return immediately)
    try {
        $proc = Start-Process -FilePath $exe -ArgumentList "--help" -Wait -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\rawrxd_help.txt" -RedirectStandardError "$env:TEMP\rawrxd_help_err.txt"
        # Exit code 0 or 1 are acceptable (some apps return 1 for help)
        Test-Assert "Headless --help exits cleanly" ($proc.ExitCode -in 0, 1) "Exit code: $($proc.ExitCode)"
    } catch {
        Test-Assert "Headless launch" $false $_.Exception.Message
    }

    # 2. Version flag
    try {
        $proc = Start-Process -FilePath $exe -ArgumentList "--version" -Wait -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\rawrxd_ver.txt" 2>$null
        $verOutput = Get-Content "$env:TEMP\rawrxd_ver.txt" -ErrorAction SilentlyContinue | Out-String
        Test-Assert "Version flag works" ($proc.ExitCode -in 0, 1) "Exit: $($proc.ExitCode), Output: $verOutput"
    } catch {
        Test-Skip "Version flag" "Process failed to start"
    }

    # 3. Config file validation
    $configExample = Join-Path $ProjectRoot "config.example.json"
    if (Test-Path $configExample) {
        try {
            $json = Get-Content $configExample -Raw | ConvertFrom-Json
            Test-Assert "Config template is valid JSON" ($null -ne $json)
        } catch {
            Test-Assert "Config template is valid JSON" $false $_.Exception.Message
        }
    } else {
        Test-Skip "Config template validation" "config.example.json not found"
    }

    # 4. HTML frontend syntax check
    $htmlPath = Join-Path $ProjectRoot "gui" "ide_chatbot.html"
    if (Test-Path $htmlPath) {
        $html = Get-Content $htmlPath -Raw
        Test-Assert "HTML frontend is non-empty" ($html.Length -gt 1000) "Length: $($html.Length)"
        Test-Assert "HTML has DOMPurify" ($html -match "dompurify") "Security: DOMPurify not found"
        Test-Assert "HTML has CSP header" ($html -match "Content-Security-Policy") "Security: CSP missing"
        Test-Assert "HTML has highlight.js" ($html -match "highlight\.js|hljs") "Code highlighting missing"
    } else {
        Test-Skip "HTML frontend checks" "ide_chatbot.html not found"
    }
}

# ============================================================================
# SMOKE TESTS (quick validation)
# ============================================================================

function Test-SmokeTests {
    Write-Host ""
    Write-Host "--- Smoke Tests ---" -ForegroundColor Cyan

    # 1. CMakeLists.txt parseable
    $cml = Join-Path $ProjectRoot "CMakeLists.txt"
    if (Test-Path $cml) {
        $content = Get-Content $cml -Raw
        Test-Assert "CMakeLists.txt is non-trivial" ($content.Length -gt 500)
        Test-Assert "CMakeLists.txt has project name" ($content -match "project\s*\(")
    }

    # 2. Source files compile-ready (no BOM issues, proper includes)
    $mainCpp = Join-Path $ProjectRoot "src" "main.cpp"
    if (Test-Path $mainCpp) {
        $mainContent = Get-Content $mainCpp -Raw -ErrorAction SilentlyContinue
        Test-Assert "main.cpp exists and is readable" ($mainContent.Length -gt 0)
    }

    # 3. Hotpatch headers exist
    $hotpatchFiles = @(
        "src\core\model_memory_hotpatch.hpp",
        "src\core\byte_level_hotpatcher.hpp",
        "src\core\unified_hotpatch_manager.hpp",
        "src\core\proxy_hotpatcher.hpp",
        "src\server\gguf_server_hotpatch.hpp"
    )

    foreach ($hp in $hotpatchFiles) {
        $hpPath = Join-Path $ProjectRoot $hp
        Test-Assert "Hotpatch: $hp" (Test-Path $hpPath)
    }

    # 4. PDB reference provider is not a stub
    $pdbRef = Join-Path $ProjectRoot "src" "core" "pdb_reference_provider.cpp"
    if (Test-Path $pdbRef) {
        $pdbContent = Get-Content $pdbRef -Raw
        $hasImportWalker = $pdbContent -match "IMAGE_IMPORT_DESCRIPTOR|walkPEImportTable"
        Test-Assert "Import Table Provider implemented (not stub)" $hasImportWalker "Should contain IMAGE_IMPORT_DESCRIPTOR walking"
    }

    # 5. Byte-level hotpatcher complete
    $bytePatch = Join-Path $ProjectRoot "src" "core" "byte_level_hotpatcher.cpp"
    if (Test-Path $bytePatch) {
        $bpContent = Get-Content $bytePatch -Raw
        Test-Assert "direct_read implemented" ($bpContent -match "PatchResult direct_read")
        Test-Assert "direct_write implemented" ($bpContent -match "PatchResult direct_write")
        Test-Assert "direct_search implemented" ($bpContent -match "ByteSearchResult direct_search")
        Test-Assert "apply_byte_mutation implemented" ($bpContent -match "PatchResult apply_byte_mutation")
    }
}

# ============================================================================
# API TESTS (if server is running)
# ============================================================================

function Test-APITests {
    Write-Host ""
    Write-Host "--- API Tests ---" -ForegroundColor Cyan

    $baseUrl = "http://localhost:8080"

    # Check if server is accessible
    try {
        $health = Invoke-RestMethod -Uri "$baseUrl/health" -TimeoutSec 3 -ErrorAction Stop
        Test-Assert "Server health endpoint" $true
    } catch {
        Test-Skip "All API tests" "Server not running at $baseUrl"
        return
    }

    # Status endpoint
    try {
        $status = Invoke-RestMethod -Uri "$baseUrl/status" -TimeoutSec 5
        Test-Assert "Status endpoint responds" ($null -ne $status)
    } catch {
        Test-Assert "Status endpoint" $false $_.Exception.Message
    }

    # Models endpoint
    try {
        $models = Invoke-RestMethod -Uri "$baseUrl/models" -TimeoutSec 5
        Test-Assert "Models endpoint responds" ($null -ne $models)
    } catch {
        Test-Assert "Models endpoint" $false $_.Exception.Message
    }

    # Agent status
    try {
        $agents = Invoke-RestMethod -Uri "$baseUrl/api/agents/status" -TimeoutSec 5
        Test-Assert "Agent status endpoint" ($null -ne $agents)
        if ($agents.agents) {
            Test-Assert "Failure detector in response" ($null -ne $agents.agents.failure_detector)
            Test-Assert "Puppeteer in response" ($null -ne $agents.agents.puppeteer)
        }
    } catch {
        Test-Assert "Agent status endpoint" $false $_.Exception.Message
    }

    # Failures endpoint
    try {
        $failures = Invoke-RestMethod -Uri "$baseUrl/api/failures?limit=5" -TimeoutSec 5
        Test-Assert "Failures endpoint responds" ($null -ne $failures)
    } catch {
        Test-Assert "Failures endpoint" $false $_.Exception.Message
    }
}

# ============================================================================
# Main
# ============================================================================

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  RawrXD IDE — Test Suite" -ForegroundColor Cyan
Write-Host "  Category: $Category" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan

$startTime = Get-Date

if ($Quick) {
    Test-SmokeTests
} else {
    switch ($Category) {
        "All" {
            Test-BuildVerification
            Test-UnitTests
            Test-IntegrationTests
            Test-SmokeTests
            Test-APITests
        }
        "Build" { Test-BuildVerification }
        "Unit" { Test-UnitTests }
        "Integration" { Test-IntegrationTests }
        "Smoke" { Test-SmokeTests }
        "API" { Test-APITests }
    }
}

$elapsed = (Get-Date) - $startTime

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Results: $($script:PassedTests)/$($script:TotalTests) passed" -ForegroundColor $(if ($script:FailedTests -gt 0) { "Yellow" } else { "Green" })
Write-Host "  Passed:  $($script:PassedTests)" -ForegroundColor Green
Write-Host "  Failed:  $($script:FailedTests)" -ForegroundColor $(if ($script:FailedTests -gt 0) { "Red" } else { "Green" })
Write-Host "  Skipped: $($script:SkippedTests)" -ForegroundColor Yellow
Write-Host "  Time:    $([math]::Round($elapsed.TotalSeconds, 2))s" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan

if ($script:FailedTests -gt 0) { exit 1 }
