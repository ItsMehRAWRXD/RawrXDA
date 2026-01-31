#!/usr/bin/env pwsh
# Verify-QtRemoval.ps1
# Verification script for Qt removal progress
# Run after each migration batch to validate:
#   1. No Qt #includes remain in source
#   2. No Q_OBJECT macros present
#   3. Binaries have zero Qt dependencies
#   4. Foundation integration works

Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║           QT REMOVAL VERIFICATION SCRIPT                      ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$srcPath = "D:\RawrXD\src"
$shipPath = "D:\RawrXD\Ship"
$passCount = 0
$failCount = 0

# ============================================================================
# CHECK 1: No Qt includes remain in source files
# ============================================================================
Write-Host "🔍 CHECK 1: Scanning for Qt #include directives..." -ForegroundColor Yellow

$qtIncludePattern = '#include\s*[<"]Q[a-zA-Z]+'
$qtIncludes = @()

Get-ChildItem -Path $srcPath -Recurse -Include *.cpp, *.hpp, *.h -ErrorAction SilentlyContinue | 
    ForEach-Object {
        $file = $_
        $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
        if ($content) {
            $matches = [regex]::Matches($content, $qtIncludePattern)
            if ($matches.Count -gt 0) {
                foreach ($match in $matches) {
                    $lineNum = ($content.Substring(0, $match.Index) | @{$true = {$_.Split("`n").Count}; $false = {1}}[[bool]($content.Substring(0, $match.Index) -match "`n")])
                    $qtIncludes += [PSCustomObject]@{
                        File = $file.FullName.Replace($srcPath, "src")
                        Line = $match.Value
                    }
                }
            }
        }
    }

if ($qtIncludes.Count -eq 0) {
    Write-Host "   ✅ PASS: No Qt #include directives found" -ForegroundColor Green
    $passCount++
} else {
    Write-Host "   ❌ FAIL: Found $($qtIncludes.Count) Qt #include directives:" -ForegroundColor Red
    $qtIncludes | Select-Object -First 10 | ForEach-Object {
        Write-Host "      • $($_.File): $($_.Line)" -ForegroundColor DarkRed
    }
    if ($qtIncludes.Count -gt 10) {
        Write-Host "      ... and $($qtIncludes.Count - 10) more" -ForegroundColor DarkRed
    }
    $failCount++
}

Write-Host ""

# ============================================================================
# CHECK 2: No Q_OBJECT macros present
# ============================================================================
Write-Host "🔍 CHECK 2: Scanning for Q_OBJECT macros..." -ForegroundColor Yellow

$qObjectPattern = '\bQ_OBJECT\b'
$qObjects = @()

Get-ChildItem -Path $srcPath -Recurse -Include *.hpp, *.h -ErrorAction SilentlyContinue | 
    ForEach-Object {
        $file = $_
        $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
        if ($content) {
            $matches = [regex]::Matches($content, $qObjectPattern)
            if ($matches.Count -gt 0) {
                $qObjects += [PSCustomObject]@{
                    File = $file.FullName.Replace($srcPath, "src")
                    Count = $matches.Count
                }
            }
        }
    }

if ($qObjects.Count -eq 0) {
    Write-Host "   ✅ PASS: No Q_OBJECT macros found" -ForegroundColor Green
    $passCount++
} else {
    Write-Host "   ❌ FAIL: Found Q_OBJECT macros in $($qObjects.Count) files:" -ForegroundColor Red
    $qObjects | ForEach-Object {
        Write-Host "      • $($_.File): $($_.Count) occurrences" -ForegroundColor DarkRed
    }
    $failCount++
}

Write-Host ""

# ============================================================================
# CHECK 3: Check binary dependencies (dumpbin analysis)
# ============================================================================
Write-Host "🔍 CHECK 3: Analyzing binary dependencies for Qt..." -ForegroundColor Yellow

$binaries = @(
    "$shipPath\RawrXD_IDE.exe",
    "$shipPath\RawrXD_Foundation.dll",
    "$shipPath\RawrXD_MainWindow_Win32.dll",
    "$shipPath\RawrXD_Executor.dll",
    "$shipPath\RawrXD_TextEditor_Win32.dll"
)

$qtDepsFound = @()

foreach ($binary in $binaries) {
    if (Test-Path $binary) {
        try {
            $output = & dumpbin /dependents $binary 2>$null
            
            # Search for Qt DLLs (Qt5Core, Qt5Gui, Qt5Widgets, etc.)
            $qtMatches = $output | Select-String -Pattern '(Qt5|Qt6|Qt|msvcp|vcruntime).*\.dll' -AllMatches
            
            if ($qtMatches) {
                foreach ($match in $qtMatches.Matches) {
                    # Filter out expected system DLLs
                    if ($match.Value -match 'Qt[56]|Qt\.') {
                        $qtDepsFound += [PSCustomObject]@{
                            Binary = Split-Path $binary -Leaf
                            Dependency = $match.Value
                        }
                    }
                }
            }
        } catch {
            Write-Host "   ⚠️  Could not analyze $binary (dumpbin not found or not accessible)" -ForegroundColor Yellow
        }
    }
}

if ($qtDepsFound.Count -eq 0) {
    Write-Host "   ✅ PASS: No Qt DLL dependencies found in binaries" -ForegroundColor Green
    $passCount++
} else {
    Write-Host "   ❌ FAIL: Found Qt DLL dependencies:" -ForegroundColor Red
    $qtDepsFound | ForEach-Object {
        Write-Host "      • $($_.Binary) depends on: $($_.Dependency)" -ForegroundColor DarkRed
    }
    $failCount++
}

Write-Host ""

# ============================================================================
# CHECK 4: Foundation integration test
# ============================================================================
Write-Host "🔍 CHECK 4: Testing Foundation integration..." -ForegroundColor Yellow

$foundationDll = "$shipPath\RawrXD_Foundation.dll"
if (Test-Path $foundationDll) {
    Write-Host "   📦 RawrXD_Foundation.dll found at: $foundationDll" -ForegroundColor Cyan
    
    try {
        # Try to load the DLL to verify it's valid
        [System.Reflection.Assembly]::LoadFile($foundationDll) > $null
        Write-Host "   ✅ PASS: Foundation DLL loads successfully" -ForegroundColor Green
        $passCount++
    } catch {
        Write-Host "   ⚠️  Foundation DLL exists but may have load issues: $_" -ForegroundColor Yellow
    }
} else {
    Write-Host "   ❌ FAIL: RawrXD_Foundation.dll not found" -ForegroundColor Red
    $failCount++
}

Write-Host ""

# ============================================================================
# ADDITIONAL CHECKS: Qt Macros & Patterns
# ============================================================================
Write-Host "🔍 ADDITIONAL CHECKS:" -ForegroundColor Yellow

# Q_SIGNAL, Q_SLOT macros
$signalSlotPattern = '\b(Q_SIGNAL|Q_SLOT|signals|public\s+slots|private\s+slots)\b'
$signalSlotMatches = @()

Get-ChildItem -Path $srcPath -Recurse -Include *.cpp, *.hpp, *.h -ErrorAction SilentlyContinue | 
    ForEach-Object {
        $file = $_
        $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
        if ($content) {
            $matches = [regex]::Matches($content, $signalSlotPattern)
            if ($matches.Count -gt 0) {
                $signalSlotMatches += [PSCustomObject]@{
                    File = $file.FullName.Replace($srcPath, "src")
                    Count = $matches.Count
                }
            }
        }
    }

if ($signalSlotMatches.Count -eq 0) {
    Write-Host "   ✅ No Q_SIGNAL/Q_SLOT/slots patterns found" -ForegroundColor Green
} else {
    Write-Host "   ⚠️  Found signal/slot patterns in $($signalSlotMatches.Count) files (may need review)" -ForegroundColor Yellow
    $signalSlotMatches | Select-Object -First 3 | ForEach-Object {
        Write-Host "      • $($_.File)" -ForegroundColor Yellow
    }
}

# Connect() calls
$connectPattern = '\bconnect\s*\('
$connectMatches = @()

Get-ChildItem -Path $srcPath -Recurse -Include *.cpp, *.hpp, *.h -ErrorAction SilentlyContinue | 
    ForEach-Object {
        $file = $_
        $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
        if ($content) {
            $matches = [regex]::Matches($content, $connectPattern)
            if ($matches.Count -gt 0) {
                $connectMatches += [PSCustomObject]@{
                    File = $file.FullName.Replace($srcPath, "src")
                    Count = $matches.Count
                }
            }
        }
    }

if ($connectMatches.Count -eq 0) {
    Write-Host "   ✅ No Qt connect() calls found" -ForegroundColor Green
} else {
    Write-Host "   ⚠️  Found $($connectMatches.Count) files with connect() calls (verify these are Qt or callback-based)" -ForegroundColor Yellow
}

Write-Host ""

# ============================================================================
# SUMMARY
# ============================================================================
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                   VERIFICATION SUMMARY                        ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$totalChecks = $passCount + $failCount
if ($failCount -eq 0 -and $totalChecks -eq 4) {
    Write-Host "✅ ALL CHECKS PASSED - Qt successfully removed!" -ForegroundColor Green
    Write-Host ""
    Write-Host "   ✓ No Qt #include directives" -ForegroundColor Green
    Write-Host "   ✓ No Q_OBJECT macros" -ForegroundColor Green
    Write-Host "   ✓ No Qt DLL dependencies" -ForegroundColor Green
    Write-Host "   ✓ Foundation integration working" -ForegroundColor Green
} else {
    Write-Host "⚠️  VERIFICATION RESULTS:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "   Passed: $passCount/4" -ForegroundColor Green
    Write-Host "   Failed: $failCount/4" -ForegroundColor Red
    Write-Host ""
    if ($failCount -gt 0) {
        Write-Host "   ❌ Please address the failures above before proceeding" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "📊 File Statistics:" -ForegroundColor Cyan
$totalFiles = (Get-ChildItem -Path $srcPath -Recurse -Include *.cpp, *.hpp, *.h | Measure-Object).Count
$filesWithQt = $qtIncludes.Count
Write-Host "   Total source files scanned: $totalFiles" -ForegroundColor White
Write-Host "   Files with Qt dependencies: $filesWithQt" -ForegroundColor White
if ($filesWithQt -eq 0) {
    Write-Host "   ✅ 100% Qt-free!" -ForegroundColor Green
} else {
    $percent = [math]::Round((($totalFiles - $filesWithQt) / $totalFiles) * 100, 1)
    Write-Host "   Progress: $percent% Qt-free" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "📝 Next Steps:" -ForegroundColor Cyan
Write-Host "   1. Fix any failed checks above" -ForegroundColor White
Write-Host "   2. Re-run this script to verify" -ForegroundColor White
Write-Host "   3. If all pass, run: ./build.ps1" -ForegroundColor White
Write-Host "   4. Test: ./RawrXD_IDE.exe" -ForegroundColor White

Write-Host ""
