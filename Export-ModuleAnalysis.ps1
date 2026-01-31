#Requires -Version 7.0
<#
.SYNOPSIS
    Export comprehensive module analysis to file
.DESCRIPTION
    Identifies original vs auto-generated modules, finds duplicates, and compares content
    Results are saved to a timestamped report file
.PARAMETER Path
    Directory to analyze
.PARAMETER OutputPath
    Path for the output report (optional)
#>

param(
    [string]$Path = "d:\lazy init ide",
    [string]$OutputPath = ""
)

# Generate timestamped filename if not specified
if (-not $OutputPath) {
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputPath = Join-Path $Path "ModuleAnalysis_$timestamp.txt"
}

# Create StringBuilder for efficient string building
$report = [System.Text.StringBuilder]::new()

# Helper function to add content to report
function Add-ReportContent {
    param([string]$Content, [string]$Color = "White")
    $null = $report.AppendLine($Content)
    # Also write to console with color
    Write-Host $Content -ForegroundColor $Color
}

# Get all modules
$modules = Get-ChildItem -Path $Path -Recurse -Filter "*.psm1" -ErrorAction SilentlyContinue

Add-ReportContent "=== MODULE ANALYSIS FOR: $Path ===" "Cyan"
Add-ReportContent ""
Add-ReportContent "Total modules found: $($modules.Count)" "Green"
Add-ReportContent ""

# Group by directory
Add-ReportContent "=== MODULES BY LOCATION ===" "Yellow"
$byDir = $modules | Group-Object -Property {$_.Directory.FullName}
foreach ($group in $byDir | Sort-Object Name) {
    $dirName = $group.Name -replace [regex]::Escape($Path), ''
    if ($dirName -eq '' -or $dirName -eq '\') { $dirName = '(root)' }
    else { $dirName = $dirName.TrimStart('\') }
    
    Add-ReportContent "Directory: $dirName" "Cyan"
    Add-ReportContent "File count: $($group.Count)" "White"
    
    foreach ($file in $group.Group | Sort-Object Name) {
        $sizeKB = [math]::Round($file.Length / 1KB, 1)
        $modified = $file.LastWriteTime.ToString("yyyy-MM-dd HH:mm")
        Add-ReportContent "  - $($file.Name) ($sizeKB KB, modified: $modified)" "White"
    }
    Add-ReportContent ""
}

# Find duplicates by name
Add-ReportContent "=== DUPLICATE MODULE NAMES ===" "Red"
$byName = @{};
foreach ($m in $modules) {
    $baseName = $m.Name
    if (-not $byName.ContainsKey($baseName)) {
        $byName[$baseName] = @()
    }
    $byName[$baseName] += $m
}

$duplicates = $byName.GetEnumerator() | Where-Object { $_.Value.Count -gt 1 }
$duplicateCount = 0
$totalDuplicateFiles = 0

foreach ($dup in $duplicates | Sort-Object Name) {
    $duplicateCount++
    $totalDuplicateFiles += $dup.Value.Count
    
    Add-ReportContent "Module: $($dup.Key)" "Magenta"
    Add-ReportContent "  Appearances: $($dup.Value.Count)" "Yellow"
    
    foreach ($file in $dup.Value | Sort-Object FullName) {
        $relPath = $file.FullName -replace [regex]::Escape($Path), ''
        $relPath = $relPath.TrimStart('\')
        $sizeKB = [math]::Round($file.Length / 1KB, 1)
        Add-ReportContent "    $relPath ($sizeKB KB)" "White"
    }
    Add-ReportContent ""
}

Add-ReportContent "Summary: $duplicateCount module names have duplicates" "Cyan"
Add-ReportContent "Total duplicate files: $totalDuplicateFiles" "Cyan"
Add-ReportContent ""

# Categorize likely originals vs auto-generated
Add-ReportContent "=== CATEGORIZATION ANALYSIS ===" "Green"

$rootModules = Get-ChildItem -Path $Path -Filter "*.psm1" -ErrorAction SilentlyContinue | Sort-Object Name
$autoGenDir = Join-Path $Path "auto_generated_methods"
$autoGenModules = @()
if (Test-Path $autoGenDir) {
    $autoGenModules = Get-ChildItem -Path $autoGenDir -Recurse -Filter "*.psm1" -ErrorAction SilentlyContinue
}

$extensionsDir = Join-Path $Path "extensions"
$extensionModules = @()
if (Test-Path $extensionsDir) {
    $extensionModules = Get-ChildItem -Path $extensionsDir -Recurse -Filter "*.psm1" -ErrorAction SilentlyContinue
}

$modulesDir = Join-Path $Path "modules"
$modulesSubdir = @()
if (Test-Path $modulesDir) {
    $modulesSubdir = Get-ChildItem -Path $modulesDir -Recurse -Filter "*.psm1" -ErrorAction SilentlyContinue
}

Add-ReportContent "Root directory (likely originals): $($rootModules.Count) modules" "Cyan"
foreach ($mod in $rootModules) {
    $sizeKB = [math]::Round($mod.Length / 1KB, 1)
    Add-ReportContent "  - $($mod.Name) ($sizeKB KB)" "White"
}

Add-ReportContent ""
Add-ReportContent "Auto-generated directory: $($autoGenModules.Count) modules" "Yellow"
foreach ($mod in $autoGenModules | Sort-Object FullName) {
    $relPath = $mod.FullName -replace [regex]::Escape($autoGenDir), ''
    $relPath = $relPath.TrimStart('\')
    $sizeKB = [math]::Round($mod.Length / 1KB, 1)
    Add-ReportContent "  - $relPath ($sizeKB KB)" "White"
}

Add-ReportContent ""
Add-ReportContent "Extensions directory: $($extensionModules.Count) modules" "Magenta"
foreach ($mod in $extensionModules | Sort-Object FullName) {
    $relPath = $mod.FullName -replace [regex]::Escape($extensionsDir), ''
    $relPath = $relPath.TrimStart('\')
    $sizeKB = [math]::Round($mod.Length / 1KB, 1)
    Add-ReportContent "  - $relPath ($sizeKB KB)" "White"
}

Add-ReportContent ""
Add-ReportContent "Modules subdirectory: $($modulesSubdir.Count) modules" "Blue"
foreach ($mod in $modulesSubdir | Sort-Object FullName) {
    $relPath = $mod.FullName -replace [regex]::Escape($modulesDir), ''
    $relPath = $relPath.TrimStart('\')
    $sizeKB = [math]::Round($mod.Length / 1KB, 1)
    Add-ReportContent "  - $relPath ($sizeKB KB)" "White"
}

# Detailed comparison for duplicates
if ($duplicates.Count -gt 0) {
    Add-ReportContent ""
    Add-ReportContent "=== DETAILED CONTENT COMPARISON ===" "Green"
    
    foreach ($dup in $duplicates | Sort-Object Name) {
        $files = $dup.Value | Sort-Object FullName
        
        if ($files.Count -eq 2) {
            Add-ReportContent "Comparing: $($dup.Key)" "Cyan"
            
            try {
                $content1 = Get-Content -Path $files[0].FullName -Raw
                $content2 = Get-Content -Path $files[1].FullName -Raw
                
                if ($content1 -eq $content2) {
                    Add-ReportContent "  Result: IDENTICAL CONTENT" "Green"
                } else {
                    Add-ReportContent "  Result: DIFFERENT CONTENT" "Red"
                    
                    # Count differences
                    $lines1 = $content1 -split "`n"
                    $lines2 = $content2 -split "`n"
                    $maxLines = [Math]::Max($lines1.Count, $lines2.Count)
                    $diffCount = 0
                    
                    for ($i = 0; $i -lt $maxLines; $i++) {
                        $line1 = if ($i -lt $lines1.Count) { $lines1[$i] } else { "" }
                        $line2 = if ($i -lt $lines2.Count) { $lines2[$i] } else { "" }
                        
                        if ($line1 -ne $line2) {
                            $diffCount++
                        }
                    }
                    
                    Add-ReportContent "  Differences found: $diffCount lines" "Yellow"
                    
                    # Show file sizes
                    $size1 = [math]::Round($files[0].Length / 1KB, 1)
                    $size2 = [math]::Round($files[1].Length / 1KB, 1)
                    Add-ReportContent "  File 1: $($files[0].FullName) ($size1 KB)" "White"
                    Add-ReportContent "  File 2: $($files[1].FullName) ($size2 KB)" "White"
                }
            } catch {
                Add-ReportContent "  Error reading files: $_" "Red"
            }
            Add-ReportContent ""
        }
    }
}

# Summary
Add-ReportContent "=== ANALYSIS SUMMARY ===" "Cyan"
Add-ReportContent "Total modules analyzed: $($modules.Count)" "White"
Add-ReportContent "Unique module names: $($byName.Count)" "White"
Add-ReportContent "Modules with duplicates: $($duplicates.Count)" "Yellow"
Add-ReportContent "Likely original modules (root): $($rootModules.Count)" "Green"
Add-ReportContent "Auto-generated modules: $($autoGenModules.Count)" "Yellow"
Add-ReportContent "Extension modules: $($extensionModules.Count)" "Magenta"
Add-ReportContent "Modules from subdirectories: $($modulesSubdir.Count)" "Blue"

Add-ReportContent ""
Add-ReportContent "Analysis complete!" "Green"

# Save to file
$reportContent = $report.ToString()
$reportContent | Set-Content -Path $OutputPath -Encoding UTF8

Write-Host ""
Write-Host "Report saved to: $OutputPath" -ForegroundColor Green
Write-Host "File size: $([math]::Round((Get-Item $OutputPath).Length / 1KB, 1)) KB" -ForegroundColor White
