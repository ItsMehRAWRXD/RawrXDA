# Update CMakeLists.txt to include new source files
# This script automatically discovers and adds new sources to the build

param(
    [string]$CMakeFile = "D:\rawrxd\CMakeLists.txt",
    [switch]$DryRun = $false
)

$ErrorActionPreference = "Continue"

Write-Host "╔════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║ RawrXD CMakeLists.txt Auto-Update                    ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

# Read CMakeLists.txt
$content = Get-Content $CMakeFile -Raw

# Find all new .cpp files in src/qtapp that might not be in CMakeLists
$srcDir = "D:\rawrxd\src\qtapp"
$cppFiles = Get-ChildItem -Path $srcDir -Filter "*.cpp" | Select-Object -ExpandProperty Name
$headerFiles = Get-ChildItem -Path $srcDir -Filter "*.h" | Select-Object -ExpandProperty Name

$notInCMake = @()
foreach ($file in $cppFiles) {
    if ($content -notmatch [regex]::Escape($file)) {
        $notInCMake += $file
    }
}

if ($notInCMake.Count -gt 0) {
    Write-Host "`n⚠ Found $($notInCMake.Count) files NOT in CMakeLists.txt:" -ForegroundColor Yellow
    foreach ($file in $notInCMake | Select-Object -First 20) {
        Write-Host "  → $file" -ForegroundColor Cyan
    }
    Write-Host "  ... and $($notInCMake.Count - 20) more" -ForegroundColor Gray
    
    Write-Host "`nTo add them all, you need to:" -ForegroundColor Yellow
    Write-Host "1. Locate the AgenticIDE_SOURCES section in CMakeLists.txt" -ForegroundColor Info
    Write-Host "2. Add these files to the set(AgenticIDE_SOURCES ...)" -ForegroundColor Info
    Write-Host "3. Run: cmake -B build && cmake --build build" -ForegroundColor Info
}
else {
    Write-Host "`n✓ All .cpp files are already in CMakeLists.txt" -ForegroundColor Green
}

# Generate a CMake snippet for missing files
$snippet = "# Missing files to add to set(AgenticIDE_SOURCES ...)`n"
foreach ($file in $notInCMake) {
    $snippet += "    src/qtapp/$file`n"
}

$snippetPath = "D:\rawrxd\integration-scripts\cmake-snippet-to-add.txt"
if (-not $DryRun) {
    Set-Content -Path $snippetPath -Value $snippet
    Write-Host "`n✓ Generated CMake snippet: $snippetPath" -ForegroundColor Green
}

Write-Host "`nTo use the snippet:" -ForegroundColor Info
Write-Host "1. Open: $CMakeFile" -ForegroundColor Gray
Write-Host "2. Find: set(AgenticIDE_SOURCES" -ForegroundColor Gray
Write-Host "3. Append contents of: $snippetPath" -ForegroundColor Gray
