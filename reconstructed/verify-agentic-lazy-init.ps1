# Agentic Lazy Init - Integration Verification Script
# Checks that all components are properly wired together

Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  Agentic Lazy Init Integration Verification               ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$ProjectRoot = "D:\RawrXD-production-lazy-init"
$errors = @()
$warnings = @()
$successes = @()

function Test-FileContains {
    param($FilePath, $Pattern, $Description)
    
    if (-not (Test-Path $FilePath)) {
        return $false, "File not found: $FilePath"
    }
    
    $content = Get-Content $FilePath -Raw
    if ($content -match $Pattern) {
        return $true, "✓ $Description"
    } else {
        return $false, "✗ $Description - Pattern not found"
    }
}

Write-Host "[1/6] Verifying source file modifications..." -ForegroundColor Yellow

# Check CLI main
$result = Test-FileContains -FilePath "$ProjectRoot\src\cli\cli_main.cpp" `
    -Pattern "auto_model_loader\.h" `
    -Description "CLI includes auto_model_loader.h"

if ($result[0]) { $successes += $result[1] } else { $errors += $result[1] }

$result = Test-FileContains -FilePath "$ProjectRoot\src\cli\cli_main.cpp" `
    -Pattern "lazy_directory_loader\.h" `
    -Description "CLI includes lazy_directory_loader.h"

if ($result[0]) { $successes += $result[1] } else { $errors += $result[1] }

$result = Test-FileContains -FilePath "$ProjectRoot\src\cli\cli_main.cpp" `
    -Pattern "AutoModelLoader::QtIDEAutoLoader::initialize" `
    -Description "CLI initializes AutoModelLoader"

if ($result[0]) { $successes += $result[1] } else { $errors += $result[1] }

$result = Test-FileContains -FilePath "$ProjectRoot\src\cli\cli_main.cpp" `
    -Pattern "LazyDirectoryLoader::instance\(\)\.initialize" `
    -Description "CLI initializes LazyDirectoryLoader"

if ($result[0]) { $successes += $result[1] } else { $errors += $result[1] }

Write-Host ""
Write-Host "[2/6] Verifying GUI modifications..." -ForegroundColor Yellow

# Check GUI MainWindow
$result = Test-FileContains -FilePath "$ProjectRoot\src\qtapp\MainWindow_v5.cpp" `
    -Pattern "lazy_directory_loader\.h" `
    -Description "GUI includes lazy_directory_loader.h"

if ($result[0]) { $successes += $result[1] } else { $errors += $result[1] }

$result = Test-FileContains -FilePath "$ProjectRoot\src\qtapp\MainWindow_v5.cpp" `
    -Pattern "AutoModelLoader::QtIDEAutoLoader::initialize" `
    -Description "GUI initializes AutoModelLoader"

if ($result[0]) { $successes += $result[1] } else { $errors += $result[1] }

$result = Test-FileContains -FilePath "$ProjectRoot\src\qtapp\MainWindow_v5.cpp" `
    -Pattern "LazyDirectoryLoader::instance\(\)\.initialize" `
    -Description "GUI initializes LazyDirectoryLoader"

if ($result[0]) { $successes += $result[1] } else { $errors += $result[1] }

Write-Host ""
Write-Host "[3/6] Verifying CMake configuration..." -ForegroundColor Yellow

# Check Core CMakeLists
$result = Test-FileContains -FilePath "$ProjectRoot\src\core\CMakeLists.txt" `
    -Pattern "lazy_directory_loader\.cpp" `
    -Description "Core CMake includes lazy_directory_loader.cpp"

if ($result[0]) { $successes += $result[1] } else { $errors += $result[1] }

$result = Test-FileContains -FilePath "$ProjectRoot\src\core\CMakeLists.txt" `
    -Pattern "auto_model_loader\.cpp" `
    -Description "Core CMake includes auto_model_loader.cpp"

if ($result[0]) { $successes += $result[1] } else { $errors += $result[1] }

$result = Test-FileContains -FilePath "$ProjectRoot\src\core\CMakeLists.txt" `
    -Pattern "Qt6::Concurrent" `
    -Description "Core CMake links Qt6::Concurrent"

if ($result[0]) { $successes += $result[1] } else { $errors += $result[1] }

Write-Host ""
Write-Host "[4/6] Verifying implementation files exist..." -ForegroundColor Yellow

$files = @(
    "$ProjectRoot\src\auto_model_loader.cpp",
    "$ProjectRoot\src\auto_model_loader.h",
    "$ProjectRoot\src\lazy_directory_loader.cpp",
    "$ProjectRoot\src\lazy_directory_loader.h",
    "$ProjectRoot\src\benchmark_lazy_init.cpp",
    "$ProjectRoot\src\file_browser.cpp",
    "$ProjectRoot\src\file_browser.h"
)

foreach ($file in $files) {
    if (Test-Path $file) {
        $fileName = Split-Path $file -Leaf
        $successes += "✓ Found: $fileName"
    } else {
        $errors += "✗ Missing: $file"
    }
}

Write-Host ""
Write-Host "[5/6] Checking for potential conflicts..." -ForegroundColor Yellow

# Check for duplicate includes
$cliContent = Get-Content "$ProjectRoot\src\cli\cli_main.cpp" -Raw
$includeCount = ([regex]::Matches($cliContent, "auto_model_loader\.h")).Count
if ($includeCount -eq 1) {
    $successes += "✓ CLI has exactly one auto_model_loader include"
} elseif ($includeCount -gt 1) {
    $warnings += "⚠ CLI has duplicate auto_model_loader includes ($includeCount)"
}

$includeCount = ([regex]::Matches($cliContent, "lazy_directory_loader\.h")).Count
if ($includeCount -eq 1) {
    $successes += "✓ CLI has exactly one lazy_directory_loader include"
} elseif ($includeCount -gt 1) {
    $warnings += "⚠ CLI has duplicate lazy_directory_loader includes ($includeCount)"
}

Write-Host ""
Write-Host "[6/6] Verifying documentation..." -ForegroundColor Yellow

if (Test-Path "D:\AGENTIC_LAZY_INIT_COMPLETION_REPORT.md") {
    $successes += "✓ Completion report exists"
} else {
    $warnings += "⚠ Completion report not found"
}

if (Test-Path "$ProjectRoot\LAZY_INITIALIZATION_IMPLEMENTATION.md") {
    $successes += "✓ Implementation documentation exists"
} else {
    $warnings += "⚠ Implementation documentation not found"
}

# Display results
Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                 Verification Results                      ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

Write-Host "Successes: $($successes.Count)" -ForegroundColor Green
$successes | ForEach-Object { Write-Host "  $_" -ForegroundColor Green }

if ($warnings.Count -gt 0) {
    Write-Host ""
    Write-Host "Warnings: $($warnings.Count)" -ForegroundColor Yellow
    $warnings | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
}

if ($errors.Count -gt 0) {
    Write-Host ""
    Write-Host "Errors: $($errors.Count)" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
}

Write-Host ""

if ($errors.Count -eq 0 -and $warnings.Count -eq 0) {
    Write-Host "✅ VERIFICATION PASSED: All checks successful!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Ready to build with:" -ForegroundColor Cyan
    Write-Host "  powershell D:\build-agentic-lazy-init.ps1" -ForegroundColor White
    exit 0
} elseif ($errors.Count -eq 0) {
    Write-Host "⚠ VERIFICATION PASSED WITH WARNINGS" -ForegroundColor Yellow
    Write-Host "Review warnings above before building" -ForegroundColor Yellow
    exit 0
} else {
    Write-Host "❌ VERIFICATION FAILED: $($errors.Count) error(s) found" -ForegroundColor Red
    Write-Host "Fix errors before building" -ForegroundColor Red
    exit 1
}
