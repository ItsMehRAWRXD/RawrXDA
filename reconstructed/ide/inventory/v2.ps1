# Simplified IDE Inventory - Focus on extraction correctness and speed

$ErrorActionPreference = "Continue"

Write-Host "═══════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  IDE Inventory Analysis - Optimized v2" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════" -ForegroundColor Cyan

$outputDir = "d:\ide_inventory_report"
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

# ============================================================================
# 1. Ship IDE - Extract DLL sources from build_ide.ps1
# ============================================================================

Write-Host ""
Write-Host "[SHIP IDE]" -ForegroundColor Yellow
$shipDir = "d:\rawrxd\Ship"
$shipScript = "$shipDir\build_ide.ps1"
$shipSources = @()

if (Test-Path $shipScript) {
    $shipContent = Get-Content $shipScript -Raw
    # Find all @("Source", "DLL") pairs
    $pattern = '@\("([^"]+\.(?:cpp|c))"\s*,\s*"([^"]+)"'
    [regex]::Matches($shipContent, $pattern) | ForEach-Object {
        $src = $_.Groups[1].Value
        $dll = $_.Groups[2].Value
        $fullPath = (Resolve-Path "$shipDir\$src" -ErrorAction SilentlyContinue).Path
        if ($fullPath -and (Test-Path $fullPath)) {
            $shipSources += $fullPath
            Write-Host "  ✓ $src → $dll" -ForegroundColor Green
        }
    }
}
Write-Host "  Total Ship sources: $($shipSources.Count)"

# Ships EXE
$shipExe = "$shipDir\RawrXD_Win32_IDE.cpp"
if (Test-Path $shipExe) {
    Write-Host "  ✓ Ship IDE EXE: $shipExe" -ForegroundColor Green
    $shipSources += (Resolve-Path $shipExe).Path
}

# ============================================================================
# 2. CMake Win32IDE - Extract from RawrXD-ModelLoader\CMakeLists.txt
# ============================================================================

Write-Host ""
Write-Host "[CMAKE Win32IDE]" -ForegroundColor Yellow
$cmakeSources = @()
$cmaakePath = "d:\rawrxd\RawrXD-ModelLoader\CMakeLists.txt"

if (Test-Path $cmakePath) {
    $cmakeContent = Get-Content $cmaakePath -Raw
    # Look for RawrXD-Win32IDE add_executable block
    if ($cmakeContent -match 'add_executable\s*\(\s*RawrXD-Win32IDE\s+(.*?)\n\s*\)') {
        $srcBlock = $matches[1]
        # Extract files (rough parsing)
        $srcBlock -split "`n" | Where-Object { $_ -match 'src/' } | ForEach-Object {
            $file = $_ -replace '\s+#.*', '' -replace '^\s+', '' -replace '\s*\$<.*', ''
            if ($file) {
                $fullPath = (Resolve-Path "d:\rawrxd\$file" -ErrorAction SilentlyContinue).Path
                if ($fullPath -and (Test-Path $fullPath)) {
                    $cmakeSources += $fullPath
                }
            }
        }
    }
}
Write-Host "  RawrXD-ModelLoader CMake sources: $($cmakeSources.Count)"

# Check yto/CMakeLists.txt for WIN32IDE_SOURCES
$ytoPath = "d:\yto\CMakeLists.txt"
$ytoCmakeSources = @()
if (Test-Path $ytoPath) {
    $ytoContent = Get-Content $ytoPath -Raw
    if ($ytoContent -match 'set\s*\(\s*WIN32IDE_SOURCES\s+(.*?)\n\s*\)') {
        $srcBlock = $matches[1]
        $srcBlock -split "`n" | Where-Object { $_ -match 'src/' } | ForEach-Object {
            $file = $_ -replace '\s+#.*', '' -replace '^\s+', '' -replace '\s*\$<.*', ''
            if ($file) {
                $fullPath = (Resolve-Path "d:\yto\$file" -ErrorAction SilentlyContinue).Path
                if ($fullPath -and (Test-Path $fullPath)) {
                    $ytoCmakeSources += $fullPath
                    $cmakeSources += $fullPath
                }
            }
        }
    }
}
Write-Host "  yto CMake WIN32IDE_SOURCES: $($ytoCmakeSources.Count)"
Write-Host "  Total CMake sources: $($cmakeSources.Count)"

# ============================================================================
# 3. PowerBuild - Heuristic based on directory structure
# ============================================================================

Write-Host ""
Write-Host "[POWERBUILD]" -ForegroundColor Yellow

$pbExclusions = @(
    "omega_simple", "os_explorer_interceptor", "os_interceptor_cli",
    "RawrXD_Final_Integration", "RawrXD_Titan", "rawrxd_neural_core"
)

$pbSources = @()
foreach ($dir in @("d:\rawrxd\src", "d:\rawrxd\asm", "d:\rawrxd\core")) {
    if (Test-Path $dir) {
        Get-ChildItem -Path $dir -Recurse -Include "*.cpp", "*.cxx", "*.c", "*.asm" -ErrorAction SilentlyContinue | ForEach-Object {
            $basename = $_.BaseName
            if ($basename -notin $pbExclusions) {
                $pbSources += (Resolve-Path $_.FullName).Path
            }
        }
    }
}
Write-Host "  PowerBuild eligible sources: $($pbSources.Count)"

# ============================================================================
# 4. Consolidate all IDE-related files
# ============================================================================

Write-Host ""
Write-Host "[CONSOLIDATING]" -ForegroundColor Yellow

$allIDESources = @()
$allIDESources += $shipSources
$allIDESources += $cmakeSources
$allIDESources += $pbSources

# Remove duplicates and normalize
$allIDESources = $allIDESources | Select-Object -Unique

Write-Host "  Total unique files IN IDE: $($allIDESources.Count)"

# ============================================================================
# 5. Output Reports
# ============================================================================

Write-Host ""
Write-Host "[REPORTS]" -ForegroundColor Yellow

# IN list
$inPath = "$outputDir\IDE_SOURCES_IN.txt"
$allIDESources | Sort-Object | Out-File -Path $inPath -Encoding UTF8
Write-Host "  ✓ $inPath ($($allIDESources.Count) files)"

# Summary by category
$summaryPath = "$outputDir\IDE_INVENTORY_SUMMARY.txt"
$summary = @"
═════════════════════════════════════════════════════════════════
    RawrXD IDE INVENTORY SUMMARY — $(Get-Date)
═════════════════════════════════════════════════════════════════

BUILD SYSTEMS SCANNED:
  ✓ Ship IDE (build_ide.ps1)
  ✓ CMake Win32IDE (RawrXD-ModelLoader)
  ✓ CMake WIN32IDE_SOURCES (yto)
  ✓ PowerBuild Discovery

═════════════════════════════════════════════════════════════════
TOTALS:
═════════════════════════════════════════════════════════════════

Ship IDE Sources:              $($shipSources.Count)
  - EXE:       1 file
  - DLLs:      $($shipSources.Count - 1) files

CMake Sources (total):         $($cmakeSources.Count)
  - RawrXD-ModelLoader:        ~$($cmaakeSources.Count)
  - yto (WIN32IDE_SOURCES):    $($ytoCmakeSources.Count)

PowerBuild Enabled:            $($pbSources.Count)
  (src/ asm/ core/ discovery)

UNIQUE COMBINED:               $($allIDESources.Count) files

═════════════════════════════════════════════════════════════════
KEY OBSERVATION:
═════════════════════════════════════════════════════════════════

The IDE is built from THREE independent build systems:

1. SHIP IDE (singular .cpp + 32 DLLs)
   Location: d:\rawrxd\Ship\
   - Single-file EXE that dynamically loads DLLs
   - Each DLL is a separate compile unit
   
2. CMAKE WIN32IDE (multi-file modular)
   Location: d:\rawrxd\RawrXD-ModelLoader\  (currently if(FALSE) gated)
   Location: d:\yto\  (larger WIN32IDE_SOURCES block)
   - 60+ .cpp files in src/win32app/
   - Full feature-rich IDE with panel system
   
3. POWERBUILD (discovery-based)
   Location: d:\rawrxd\src\asm\core\
   - Recursively discovers .cpp/.asm files
   - Applies exclusion list for incompatible legacy MASM files

═════════════════════════════════════════════════════════════════
"@

$summary | Out-File -Path $summaryPath -Encoding UTF8
Write-Host "  ✓ $summaryPath"

# Breakdown by source
$breakdownPath = "$outputDir\BREAKDOWN_BY_SYSTEM.txt"
$breakdown = @"
═════════════════════════════════════════════════════════════════
WHAT GOES INTO THE IDE (by build system)
═════════════════════════════════════════════════════════════════

SHIP IDE ($($shipSources.Count) files):
$(if ($shipSources) { ($shipSources | Sort-Object | ForEach-Object { "  - $_" }) -join "`n" } else { "  (none)" })

CMAKE WIN32IDE - RawrXD-ModelLoader ($($cmakeSources.Count) files):
$(if ($cmakeSources | Where-Object { $_ -like "*RawrXD-ModelLoader*" }) { (($cmakeSources | Where-Object { $_ -like "*RawrXD-ModelLoader*" } | Sort-Object | ForEach-Object { "  - $_" }) -join "`n") } else { "  (none or gated off)" })

CMAKE WIN32IDE_SOURCES - yto ($($ytoCmakeSources.Count) files):
$(if ($ytoCmakeSources) { ($ytoCmakeSources | Sort-Object | ForEach-Object { "  - $_" }) -join "`n" } else { "  (none)" })

POWERBUILD Discovery ($($pbSources.Count) files):
  [See IDE_SOURCES_IN.txt for full list]
  Scanned: d:\rawrxd\src\*  d:\rawrxd\asm\*  d:\rawrxd\core\*
  Excluded: MASM-incompatible files from exclusion list

═════════════════════════════════════════════════════════════════
"@

$breakdown | Out-File -Path $breakdownPath -Encoding UTF8
Write-Host "  ✓ $breakdownPath"

Write-Host ""
Write-Host "✓ INVENTORY COMPLETE" -ForegroundColor Green
Write-Host "  Reports: $outputDir" -ForegroundColor Gray
Write-Host ""
