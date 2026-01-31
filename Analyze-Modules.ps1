#Requires -Version 7.0
<#
.SYNOPSIS
    Comprehensive analysis of modules in lazy init directory
.DESCRIPTION
    Identifies original vs auto-generated modules, finds duplicates, and compares content
#>

param(
    [string]$Path = "d:\lazy init ide"
)

Write-Host "=== MODULE ANALYSIS FOR: $Path ===" -ForegroundColor Cyan
Write-Host ""

# Get all modules
$modules = Get-ChildItem -Path $Path -Recurse -Filter "*.psm1" -ErrorAction SilentlyContinue

Write-Host "Total modules found: $($modules.Count)" -ForegroundColor Green
Write-Host ""

# Group by directory
Write-Host "=== MODULES BY LOCATION ===" -ForegroundColor Yellow
$byDir = $modules | Group-Object -Property {$_.Directory.FullName}
foreach ($group in $byDir | Sort-Object Name) {
    $dirName = $group.Name -replace [regex]::Escape($Path), ''
    if ($dirName -eq '' -or $dirName -eq '\') { $dirName = '(root)' }
    else { $dirName = $dirName.TrimStart('\') }
    
    Write-Host "Location: $dirName" -ForegroundColor Cyan
    Write-Host "File count: $($group.Count)" -ForegroundColor White
    
    foreach ($file in $group.Group | Sort-Object Name) {
        $sizeKB = [math]::Round($file.Length / 1KB, 1)
        $modified = $file.LastWriteTime.ToString("yyyy-MM-dd HH:mm")
        Write-Host "  - $($file.Name)" -ForegroundColor White -NoNewline
        Write-Host " ($sizeKB KB, modified: $modified)" -ForegroundColor Gray
    }
    Write-Host ""
}

# Find duplicates by name
Write-Host "=== DUPLICATE MODULE NAMES ===" -ForegroundColor Red
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
    
    Write-Host "Module: $($dup.Key)" -ForegroundColor Magenta
    Write-Host "  Appearances: $($dup.Value.Count)" -ForegroundColor Yellow
    
    foreach ($file in $dup.Value | Sort-Object FullName) {
        $relPath = $file.FullName -replace [regex]::Escape($Path), ''
        $relPath = $relPath.TrimStart('\')
        $sizeKB = [math]::Round($file.Length / 1KB, 1)
        Write-Host "    $relPath ($sizeKB KB)" -ForegroundColor White
    }
    Write-Host ""
}

Write-Host "Summary: $duplicateCount module names have duplicates" -ForegroundColor Cyan
Write-Host "Total duplicate files: $totalDuplicateFiles" -ForegroundColor Cyan
Write-Host ""

# Categorize likely originals vs auto-generated
Write-Host "=== CATEGORIZATION ANALYSIS ===" -ForegroundColor Green

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

Write-Host "Root directory (likely originals): $($rootModules.Count) modules" -ForegroundColor Cyan
foreach ($mod in $rootModules) {
    $sizeKB = [math]::Round($mod.Length / 1KB, 1)
    Write-Host "  - $($mod.Name) ($sizeKB KB)" -ForegroundColor White
}

Write-Host ""
Write-Host "Auto-generated directory: $($autoGenModules.Count) modules" -ForegroundColor Yellow
foreach ($mod in $autoGenModules | Sort-Object FullName) {
    $relPath = $mod.FullName -replace [regex]::Escape($autoGenDir), ''
    $relPath = $relPath.TrimStart('\')
    $sizeKB = [math]::Round($mod.Length / 1KB, 1)
    Write-Host "  - $relPath ($sizeKB KB)" -ForegroundColor White
}

Write-Host ""
Write-Host "Extensions directory: $($extensionModules.Count) modules" -ForegroundColor Magenta
foreach ($mod in $extensionModules | Sort-Object FullName) {
    $relPath = $mod.FullName -replace [regex]::Escape($extensionsDir), ''
    $relPath = $relPath.TrimStart('\')
    $sizeKB = [math]::Round($mod.Length / 1KB, 1)
    Write-Host "  - $relPath ($sizeKB KB)" -ForegroundColor White
}

Write-Host ""
Write-Host "Modules subdirectory: $($modulesSubdir.Count) modules" -ForegroundColor Blue
foreach ($mod in $modulesSubdir | Sort-Object FullName) {
    $relPath = $mod.FullName -replace [regex]::Escape($modulesDir), ''
    $relPath = $relPath.TrimStart('\')
    $sizeKB = [math]::Round($mod.Length / 1KB, 1)
    Write-Host "  - $relPath ($sizeKB KB)" -ForegroundColor White
}

# Detailed comparison for duplicates
if ($duplicates.Count -gt 0) {
    Write-Host ""
    Write-Host "=== DETAILED CONTENT COMPARISON ===" -ForegroundColor Green
    
    foreach ($dup in $duplicates | Sort-Object Name) {
        $files = $dup.Value | Sort-Object FullName
        
        if ($files.Count -eq 2) {
            Write-Host "Comparing: $($dup.Key)" -ForegroundColor Cyan
            
            try {
                $content1 = Get-Content -Path $files[0].FullName -Raw
                $content2 = Get-Content -Path $files[1].FullName -Raw
                
                if ($content1 -eq $content2) {
                    Write-Host "  Result: IDENTICAL CONTENT" -ForegroundColor Green
                } else {
                    Write-Host "  Result: DIFFERENT CONTENT" -ForegroundColor Red
                    
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
                    
                    Write-Host "  Differences found: $diffCount lines" -ForegroundColor Yellow
                    
                    # Show file sizes
                    $size1 = [math]::Round($files[0].Length / 1KB, 1)
                    $size2 = [math]::Round($files[1].Length / 1KB, 1)
                    Write-Host "  File 1: $($files[0].FullName) ($size1 KB)" -ForegroundColor White
                    Write-Host "  File 2: $($files[1].FullName) ($size2 KB)" -ForegroundColor White
                }
            } catch {
                Write-Host "  Error reading files: $_" -ForegroundColor Red
            }
            Write-Host ""
        }
    }
}

# Summary
Write-Host "=== ANALYSIS SUMMARY ===" -ForegroundColor Cyan
Write-Host "Total modules analyzed: $($modules.Count)" -ForegroundColor White
Write-Host "Unique module names: $($byName.Count)" -ForegroundColor White
Write-Host "Modules with duplicates: $($duplicates.Count)" -ForegroundColor Yellow
Write-Host "Likely original modules (root): $($rootModules.Count)" -ForegroundColor Green
Write-Host "Auto-generated modules: $($autoGenModules.Count)" -ForegroundColor Yellow
Write-Host "Extension modules: $($extensionModules.Count)" -ForegroundColor Magenta
Write-Host "Modules from subdirectories: $($modulesSubdir.Count)" -ForegroundColor Blue

Write-Host ""
Write-Host "Analysis complete!" -ForegroundColor Green
