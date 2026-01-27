# validate-copilot-instructions.ps1 - Windows validation script
# Run in project root: .\scripts\validate-copilot-instructions.ps1

param(
    [switch]$Fix,
    [switch]$Verbose
)

$ErrorActionPreference = "Continue"
$script:errors = @()
$script:warnings = @()
$repoRoot = Get-Location

Write-Host "🔍 Validating Copilot Instructions..." -ForegroundColor Cyan

# Check if instructions exist
$instructionsFile = ".github/copilot-instructions.md"
if (-not (Test-Path $instructionsFile)) {
    Write-Host "❌ Error: $instructionsFile not found" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Found $instructionsFile" -ForegroundColor Green

# Read instructions
$instructions = Get-Content $instructionsFile -Raw

# Function to add error
function Add-ValidationError {
    param([string]$message)
    $script:errors += $message
    Write-Host "  ❌ $message" -ForegroundColor Red
}

# Function to add warning
function Add-ValidationWarning {
    param([string]$message)
    $script:warnings += $message
    Write-Host "  ⚠️  $message" -ForegroundColor Yellow
}

# Function to add success
function Add-ValidationSuccess {
    param([string]$message)
    Write-Host "  ✓ $message" -ForegroundColor Green
}

Write-Host ""
Write-Host "Checking file references..." -ForegroundColor Cyan

# Check file references
$filePattern = '`src/[^`]+\.(hpp|cpp|h|c)`'
$referencedFiles = [regex]::Matches($instructions, $filePattern) | ForEach-Object { $_.Value.Trim('`') }

foreach ($file in $referencedFiles) {
    if (Test-Path $file) {
        if ($Verbose) { Add-ValidationSuccess "Found: $file" }
    } else {
        Add-ValidationError "Referenced file not found: $file"
    }
}

Write-Host ""
Write-Host "Checking directory structure..." -ForegroundColor Cyan

$dirsToCheck = @("src/qtapp", "src/agent", ".github")
foreach ($dir in $dirsToCheck) {
    if (Test-Path $dir -PathType Container) {
        Add-ValidationSuccess "Found directory: $dir"
    } else {
        Add-ValidationError "Directory not found: $dir"
    }
}

Write-Host ""
Write-Host "Checking CMakeLists.txt targets..." -ForegroundColor Cyan

$cmake = Get-Content "CMakeLists.txt" -Raw
$targets = @("RawrXD-QtShell", "self_test_gate", "quant_utils")

foreach ($target in $targets) {
    if ($cmake -match "add_executable\($target\s*" -or $cmake -match "add_library\($target\s*") {
        Add-ValidationSuccess "Found target: $target"
    } else {
        Add-ValidationError "Target not found in CMakeLists.txt: $target"
    }
}

Write-Host ""
Write-Host "Checking hotpatch files..." -ForegroundColor Cyan

$hotpatchFiles = @(
    "src/qtapp/model_memory_hotpatch.hpp",
    "src/qtapp/model_memory_hotpatch.cpp",
    "src/qtapp/byte_level_hotpatcher.hpp",
    "src/qtapp/byte_level_hotpatcher.cpp",
    "src/qtapp/gguf_server_hotpatch.hpp",
    "src/qtapp/gguf_server_hotpatch.cpp",
    "src/qtapp/unified_hotpatch_manager.hpp",
    "src/qtapp/unified_hotpatch_manager.cpp",
    "src/qtapp/proxy_hotpatcher.hpp",
    "src/qtapp/proxy_hotpatcher.cpp"
)

foreach ($file in $hotpatchFiles) {
    if (Test-Path $file) {
        # Check if commented in CMakeLists
        $commentedPattern = "# $($file -replace '\\', '/')"
        if ($cmake -match [regex]::Escape($commentedPattern)) {
            Add-ValidationWarning "Hotpatch file may be disabled: $file"
        } else {
            Add-ValidationSuccess "Hotpatch enabled: $file"
        }
    } else {
        Add-ValidationError "Hotpatch file missing: $file"
    }
}

Write-Host ""
Write-Host "Checking agentic files..." -ForegroundColor Cyan

$agenticFiles = @(
    "src/agent/agentic_failure_detector.hpp",
    "src/agent/agentic_failure_detector.cpp",
    "src/agent/agentic_puppeteer.hpp",
    "src/agent/agentic_puppeteer.cpp"
)

foreach ($file in $agenticFiles) {
    if (Test-Path $file) {
        Add-ValidationSuccess "Agentic file found: $file"
    } else {
        Add-ValidationError "Agentic file missing: $file"
    }
}

Write-Host ""
Write-Host "Checking result struct patterns..." -ForegroundColor Cyan

# Check for factory method usage
$hasFactoryMethods = @(
    @{ Name = "PatchResult::ok"; File = "src/qtapp/model_memory_hotpatch.hpp" },
    @{ Name = "PatchResult::error"; File = "src/qtapp/model_memory_hotpatch.hpp" },
    @{ Name = "UnifiedResult::successResult"; File = "src/qtapp/unified_hotpatch_manager.hpp" },
    @{ Name = "CorrectionResult::ok"; File = "src/agent/agentic_puppeteer.hpp" }
)

foreach ($pattern in $hasFactoryMethods) {
    if (Test-Path $pattern.File) {
        $content = Get-Content $pattern.File -Raw
        if ($content -match [regex]::Escape($pattern.Name)) {
            Add-ValidationSuccess "Found factory method: $($pattern.Name)"
        } else {
            Add-ValidationWarning "Factory method not found: $($pattern.Name) in $($pattern.File)"
        }
    }
}

# Summary
Write-Host ""
Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host "VALIDATION SUMMARY" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Cyan

if ($script:errors.Count -eq 0 -and $script:warnings.Count -eq 0) {
    Write-Host "✅ All validations passed!" -ForegroundColor Green
    exit 0
} else {
    if ($script:errors.Count -gt 0) {
        Write-Host ""
        Write-Host "❌ ERRORS: $($script:errors.Count)" -ForegroundColor Red
        Write-Host "Please fix these issues before committing." -ForegroundColor Red
    }
    
    if ($script:warnings.Count -gt 0) {
        Write-Host ""
        Write-Host "⚠️  WARNINGS: $($script:warnings.Count)" -ForegroundColor Yellow
        Write-Host "Review these items before committing." -ForegroundColor Yellow
    }
    
    if ($script:errors.Count -gt 0) {
        exit 1
    } else {
        exit 0
    }
}
