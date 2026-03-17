# IDE Inventory Script - Comprehensive "In vs Out" Analysis
# Scans D:\ to identify all files compiled into/bundled with the RawrXD IDE
# across ALL build systems (Ship, CMake, PowerBuild)

[CmdletBinding()]
param(
    [string[]]$RootDirs = @("d:\"),
    [string[]]$ExcludeDirs = @("*\build*", "*\bin\", "*\obj\", "*\dist\", "*\out\", "*\node_modules\", "*\.git\", "*\OneDrive\", "*\System Volume Information\", "*\$RECYCLE.BIN"),
    [string]$OutputDir = "d:\ide_inventory_report"
)

$ErrorActionPreference = "Continue"

# ============================================================================
# Section 1: Extract Ship IDE Definitions
# ============================================================================

function Get-ShipIDEDefinitions {
    Write-Host "[SHIP] Extracting IDE definitions..." -ForegroundColor Cyan
    
    $shipDir = "d:\rawrxd\Ship"
    $ideBuildScript = "$shipDir\build_ide.ps1"
    $ideExeSource = "$shipDir\RawrXD_Win32_IDE.cpp"
    
    $shipDefs = @{
        ExeSource = $ideExeSource
        DllSources = @()
        RuntimeDlls = @()
    }
    
    # Parse build_ide.ps1 for DLL source lists
    if (Test-Path $ideBuildScript) {
        try {
            $content = Get-Content $ideBuildScript -Raw
            
            # Extract phase arrays: @("Source.cpp", "DllName")
            $phases = @("phase1", "phase2", "phase3", "phase4", "phase5")
            foreach ($phase in $phases) {
                $pattern = '\$(' + $phase + ')\s*=\s*@\((.*?)\)' 
                if ($content -match $pattern) {
                    $matches[1] -split '\),\s*@\(' | ForEach-Object {
                        if ($_ -match '@\("([^"]+)"\s*,\s*"([^"]+)"') {
                            $sourceFile = $matches[1]
                            $dllName = $matches[2]
                            $fullPath = Join-Path $shipDir $sourceFile
                            $shipDefs.DllSources += @{Source = $sourceFile; FullPath = $fullPath; DLL = $dllName}
                        }
                    }
                }
            }
            
            # Manual extraction of specific lines (more robust)
            $lines = $content -split "`n"
            $inPhase = $false
            $currentPhase = ""
            
            foreach ($line in $lines) {
                if ($line -match '\$phase\d+\s*=\s*@\(') {
                    $inPhase = $true
                    $currentPhase = $line
                }
                if ($inPhase -and $line -match '@\("([^"]+)"\s*,\s*"([^"]*)"') {
                    $source = $matches[1]
                    $dll = if ($matches[2]) { $matches[2] } else { "Unknown" }
                    $fullPath = Join-Path $shipDir $source
                    
                    # Avoid dupes
                    if (-not ($shipDefs.DllSources | Where-Object { $_.FullPath -eq $fullPath })) {
                        $shipDefs.DllSources += @{Source = $source; FullPath = $fullPath; DLL = $dll}
                    }
                }
                if ($inPhase -and $line -match '^\)') {
                    $inPhase = $false
                }
            }
        } catch {
            Write-Warning "Error parsing $ideBuildScript : $_"
        }
    }
    
    # Extract runtime DLLs from RawrXD_Win32_IDE.cpp
    if (Test-Path $ideExeSource) {
        try {
            $content = Get-Content $ideExeSource -Raw
            # Match LoadLibraryW("RawrXD_*.dll") pattern
            $pattern = 'LoadLibraryW?\s*\(\s*[L"]([^"]*\.dll)'
            [regex]::Matches($content, $pattern, 'IgnoreCase') | ForEach-Object {
                $dllName = $_.Groups[1].Value
                if ($dllName -match 'RawrXD') {
                    if ($dllName -notin $shipDefs.RuntimeDlls) {
                        $shipDefs.RuntimeDlls += $dllName
                    }
                }
            }
        } catch {
            Write-Warning "Error parsing $ideExeSource : $_"
        }
    }
    
    return $shipDefs
}

# ============================================================================
# Section 2: Extract CMake IDE Definitions
# ============================================================================

function Get-CMakeIDEDefinitions {
    Write-Host "[CMAKE] Extracting IDE target definitions..." -ForegroundColor Cyan
    
    $cmakeDefs = @{
        Win32IDESources = @()
        ConditionalSources = @()
        OtherIDETargets = @()
    }
    
    # Find all CMakeLists.txt files
    Get-ChildItem -Path "d:\" -Recurse -Filter "CMakeLists.txt" -ErrorAction SilentlyContinue | ForEach-Object {
        $cmaakePath = $_.FullName
        
        try {
            $content = Get-Content $cmaakePath -Raw
            
            # Look for WIN32IDE_SOURCES or add_executable(*IDE*)
            if ($content -match 'set\s*\(\s*WIN32IDE_SOURCES') {
                # Extract the set block
                $pattern = 'set\s*\(\s*WIN32IDE_SOURCES\s+(.*?)\s*\)'
                if ($content -match $pattern) {
                    $srcBlock = $matches[1]
                    # Parse each src/... line
                    $srcLines = $srcBlock -split "`n" | Where-Object { $_ -match 'src/' }
                    foreach ($line in $srcLines) {
                        $file = $line.Trim() -replace '\s*#.*$', '' -replace '^\s+', ''
                        if ($file -and -not $file.StartsWith('$<')) {
                            $fullPath = Join-Path (Split-Path $cmaakePath) $file
                            $cmakeDefs.Win32IDESources += @{File = $file; FullPath = $fullPath; CMakePath = $cmaakePath; IsConditional = $false}
                        }
                    }
                    
                    # Also extract conditional sources ($<...>)
                    $condPattern = '\$<[^>]+>:([^>]+)\>'
                    [regex]::Matches($srcBlock, $condPattern) | ForEach-Object {
                        $condFile = $_.Groups[1].Value
                        $fullPath = Join-Path (Split-Path $cmaakePath) $condFile
                        $cmakeDefs.ConditionalSources += @{File = $condFile; FullPath = $fullPath; CMakePath = $cmaakePath; Condition = "Generator-conditional"}
                    }
                }
            }
            
            # Look for add_executable with IDE in name
            $addExecPattern = 'add_executable\s*\(\s*([^\s]+IDE[^\s]*)\s+'
            [regex]::Matches($content, $addExecPattern, 'IgnoreCase') | ForEach-Object {
                $targetName = $_.Groups[1].Value
                $cmakeDefs.OtherIDETargets += @{Target = $targetName; CMakePath = $cmaakePath}
            }
        } catch {
            Write-Verbose "Error parsing $cmaakePath : $_"
        }
    }
    
    return $cmakeDefs
}

# ============================================================================
# Section 3: Extract PowerBuild Discovery Rules
# ============================================================================

function Get-PowerBuildRules {
    Write-Host "[POWERBUILD] Extracting discovery rules..." -ForegroundColor Cyan
    
    $pbRules = @{
        SourceDirs = @(".\src", ".\asm", ".\core")
        MasmExcludeList = @()
        InclusionPattern = @("*.cpp", "*.cxx", "*.c", "*.asm")
    }
    
    $pbScript = "d:\rawrxd\RawrXD-Build.ps1"
    if (Test-Path $pbScript) {
        try {
            $content = Get-Content $pbScript -Raw
            
            # Extract MasmExcludeList
            $pattern = '\$script:MasmExcludeList\s*=\s*@\((.*?)\)'
            if ($content -match $pattern) {
                $listBlock = $matches[1]
                $listBlock -split ',' | ForEach-Object {
                    $item = $_ -replace '["\s]', ''
                    if ($item) {
                        $pbRules.MasmExcludeList += $item
                    }
                }
            }
        } catch {
            Write-Warning "Error parsing $pbScript : $_"
        }
    }
    
    return $pbRules
}

# ============================================================================
# Section 4: Main Filesystem Scan & Tagging
# ============================================================================

function Invoke-IDEInventoryScan {
    param(
        $ShipDefs,
        $CmakeDefs,
        $PbRules
    )
    
    Write-Host "[SCAN] Walking filesystem and tagging files..." -ForegroundColor Cyan
    
    $inventory = @()
    $fileMap = @{}  # For deduplication
    
    # Helper: Check if file matches exclude patterns
    function Test-IsExcluded {
        param([string]$Path)
        foreach ($pattern in $ExcludeDirs) {
            if ($Path -like $pattern) {
                return $true
            }
        }
        return $false
    }
    
    # Helper: Normalize path for comparison
    function Get-NormalizedPath {
        param([string]$Path)
        return $Path.ToLower() -replace '\\', '/' -replace '^[a-z]:/', 'd:/'
    }
    
    # Scan each root dir
    foreach ($root in $RootDirs) {
        if (-not (Test-Path $root)) { continue }
        
        Get-ChildItem -Path $root -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
            $filePath = $_.FullName
            $normPath = Get-NormalizedPath $filePath
            
            # Skip excluded dirs
            if (Test-IsExcluded $filePath) { return }
            
            # Skip duplicates
            if ($fileMap.ContainsKey($normPath)) { return }
            $fileMap[$normPath] = $true
            
            # Initialize tagging record
            $record = @{
                FilePath = $filePath
                NormPath = $normPath
                Filename = $_.Name
                Extension = $_.Extension
                Size = $_.Length
                InShipExeSrc = $false
                InShipDllSrc = $false
                InShipRuntimeDll = $false
                InCMakeWin32IDE = $false
                InCMakeConditional = $false
                InPowerBuild = $false
                InAnyIDE = $false
                Tags = @()
            }
            
            # --- Check Ship IDE EXE source
            if ($normPath -eq (Get-NormalizedPath $ShipDefs.ExeSource)) {
                $record.InShipExeSrc = $true
                $record.Tags += "Ship_EXE_Source"
            }
            
            # --- Check Ship IDE DLL sources
            foreach ($dll in $ShipDefs.DllSources) {
                if ($normPath -eq (Get-NormalizedPath $dll.FullPath)) {
                    $record.InShipDllSrc = $true
                    $record.Tags += "Ship_DLL_Source:$($dll.DLL)"
                    break
                }
            }
            
            # --- Check if this is a Ship runtime DLL (by name)
            if ($_.Name -match '^RawrXD_.*\.dll$') {
                if ($_.Name -in $ShipDefs.RuntimeDlls) {
                    $record.InShipRuntimeDll = $true
                    $record.Tags += "Ship_Runtime_DLL"
                }
            }
            
            # --- Check CMake Win32IDE sources
            foreach ($src in $CmakeDefs.Win32IDESources) {
                if ($normPath -eq (Get-NormalizedPath $src.FullPath)) {
                    $record.InCMakeWin32IDE = $true
                    $record.Tags += "CMake_Win32IDE_Source"
                    break
                }
            }
            
            # --- Check CMake conditional sources
            foreach ($src in $CmakeDefs.ConditionalSources) {
                if ($normPath -eq (Get-NormalizedPath $src.FullPath)) {
                    $record.InCMakeConditional = $true
                    $record.Tags += "CMake_Conditional:$($src.Condition)"
                    break
                }
            }
            
            # --- Check PowerBuild inclusion (heuristic)
            # Files in ./src./asm, ./core that match *.cpp/*.cxx/*.c/*.asm and not in MASM exclude list
            $baseName = $_.BaseName
            if (($_.Extension -in ('.cpp', '.cxx', '.c', '.asm')) -and 
                ($filePath -like "*\src\*" -or $filePath -like "*\asm\*" -or $filePath -like "*\core\*")) {
                
                if ($baseName -notin $PbRules.MasmExcludeList) {
                    # Assume included in PowerBuild discovery
                    if ($_.Extension -ne '.asm' -or (Test-PowerBuildMasmCompat $filePath)) {
                        $record.InPowerBuild = $true
                        $record.Tags += "PowerBuild_Source"
                    }
                }
            }
            
            # --- Compute InAnyIDE
            $record.InAnyIDE = $record.InShipExeSrc -or $record.InShipDllSrc -or $record.InShipRuntimeDll -or `
                              $record.InCMakeWin32IDE -or $record.InCMakeConditional -or $record.InPowerBuild
            
            $inventory += $record
        }
    }
    
    return $inventory
}

# Helper: Test MASM compatibility (simplified)
function Test-PowerBuildMasmCompat {
    param([string]$FilePath)
    if ($_ -notmatch '\.asm$') { return $true }
    
    try {
        $head = Get-Content $FilePath -TotalCount 40 -ErrorAction Stop
        $joined = $head -join "`n"
        
        # Incompatible patterns
        if ($joined -match '(?mi)^\s*\.section\s') { return $false }
        if ($joined -match '(?mi)^\s*\.(386|486|model|stack)\b') { return $false }
        if ($joined -match '(?mi)\bINVOKE\b') { return $false }
        if ($joined -match '(?i)include.*\\masm32\\') { return $false }
        if ($joined -match '(?mi)^section\s+\.(text|data)') { return $false }
        
        return $true
    } catch {
        return $false
    }
}

# ============================================================================
# Section 5: Generate Reports
# ============================================================================

function Export-InventoryReports {
    param($Inventory)
    
    Write-Host "[REPORT] Generating output reports..." -ForegroundColor Cyan
    
    if (-not (Test-Path $OutputDir)) {
        New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    }
    
    # --- CSV Report
    $csvPath = "$OutputDir\ide_inventory_full.csv"
    $csvRecords = $Inventory | Select-Object `
        FilePath, `
        Filename, `
        Extension, `
        Size, `
        InShipExeSrc, `
        InShipDllSrc, `
        InShipRuntimeDll, `
        InCMakeWin32IDE, `
        InCMakeConditional, `
        InPowerBuild, `
        InAnyIDE, `
        @{Name='Tags'; Expression={$_.Tags -join '|'}}
    
    $csvRecords | Export-Csv -Path $csvPath -NoTypeInformation -Encoding UTF8
    Write-Host "  ✓ $csvPath ($(($csvRecords | Measure-Object).Count) files)" -ForegroundColor Green
    
    # --- IN List
    $inPath = "$OutputDir\ide_inventory_in.txt"
    $inFiles = $Inventory | Where-Object { $_.InAnyIDE } | Sort-Object FilePath
    $inFiles | ForEach-Object { $_.FilePath } | Out-File -Path $inPath -Encoding UTF8
    Write-Host "  ✓ $inPath ($(($inFiles | Measure-Object).Count) files)" -ForegroundColor Green
    
    # --- OUT List
    $outPath = "$OutputDir\ide_inventory_out.txt"
    $outFiles = $Inventory | Where-Object { -not $_.InAnyIDE } | Sort-Object FilePath
    $outFiles | ForEach-Object { $_.FilePath } | Out-File -Path $outPath -Encoding UTF8
    Write-Host "  ✓ $outPath ($(($outFiles | Measure-Object).Count) files)" -ForegroundColor Green
    
    # --- Summary
    $summaryPath = "$OutputDir\ide_inventory_summary.txt"
    $summary = @"
===== IDE INVENTORY SUMMARY =====
Scan Date: $(Get-Date)
Root Directories: $($RootDirs -join ', ')

=== TOTALS ===
Total Files Scanned: $(($Inventory | Measure-Object).Count)
Files IN Any IDE Build: $(($inFiles | Measure-Object).Count)
Files NOT IN Any IDE: $(($outFiles | Measure-Object).Count)

=== BREAKDOWN BY BUILD SYSTEM ===
Ship IDE EXE Source: $(($Inventory | Where-Object { $_.InShipExeSrc } | Measure-Object).Count)
Ship IDE DLL Sources: $(($Inventory | Where-Object { $_.InShipDllSrc } | Measure-Object).Count)
Ship Runtime DLLs: $(($Inventory | Where-Object { $_.InShipRuntimeDll } | Measure-Object).Count)
CMake Win32IDE Direct: $(($Inventory | Where-Object { $_.InCMakeWin32IDE } | Measure-Object).Count)
CMake Conditional: $(($Inventory | Where-Object { $_.InCMakeConditional } | Measure-Object).Count)
PowerBuild Discovery: $(($Inventory | Where-Object { $_.InPowerBuild } | Measure-Object).Count)

=== TOP 10 DIRECTORIES BY FILE COUNT (IN IDE) ===
"@
    $inFiles | ForEach-Object { Split-Path $_.FilePath } | Group-Object | Sort-Object Count -Descending | Select-Object -First 10 | ForEach-Object {
        $summary += "`n$($_.Count)  $($_.Name)"
    }
    
    $summary | Out-File -Path $summaryPath -Encoding UTF8
    Write-Host "  ✓ $summaryPath" -ForegroundColor Green
    
    return @{ CSV = $csvPath; IN = $inPath; OUT = $outPath; Summary = $summaryPath }
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

Write-Host ""
Write-Host "╔════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   RawrXD IDE Inventory Scanner v1.0       ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

try {
    # Step 1: Extract definitions
    $shipDefs = Get-ShipIDEDefinitions
    Write-Host "  Found: EXE source=$($shipDefs.ExeSource), $($shipDefs.DllSources.Count) DLL sources, $($shipDefs.RuntimeDlls.Count) runtime DLLs" -ForegroundColor Green
    
    # Step 2: Extract CMake
    $cmakeDefs = Get-CMakeIDEDefinitions
    Write-Host "  Found: $($cmakeDefs.Win32IDESources.Count) Win32IDE sources, $($cmakeDefs.ConditionalSources.Count) conditional, $($cmakeDefs.OtherIDETargets.Count) other IDE targets" -ForegroundColor Green
    
    # Step 3: Extract PowerBuild
    $pbRules = Get-PowerBuildRules
    Write-Host "  Found: $($pbRules.MasmExcludeList.Count) MASM exclusions" -ForegroundColor Green
    
    Write-Host ""
    
    # Step 4: Scan filesystem
    $inventory = Invoke-IDEInventoryScan -ShipDefs $shipDefs -CmakeDefs $cmakeDefs -PbRules $pbRules
    Write-Host "  Completed scanning $(($inventory | Measure-Object).Count) files" -ForegroundColor Green
    
    Write-Host ""
    
    # Step 5: Export reports
    $reports = Export-InventoryReports -Inventory $inventory
    
    Write-Host ""
    Write-Host "✓ Inventory complete! Reports saved to: $OutputDir" -ForegroundColor Green
    Write-Host ""
    
} catch {
    Write-Host "✗ ERROR: $_" -ForegroundColor Red
    throw
}
