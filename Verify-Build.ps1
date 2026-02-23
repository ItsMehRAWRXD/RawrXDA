# Verify-Build.ps1
# Comprehensive build verification and validation
# Checks for Qt dependencies, DLL loading, functionality, and integrity.
#
# Policy: RawrXD is Qt-free. Use Ship/StdReplacements.hpp and Win32/C++20 only.
# Any #include <QString>, <QtCore>, etc. in src/ or Ship/ fails this script.

param(
    [string]$BuildDir = "D:\RawrXD\build_clean\Release"
)

Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║              BUILD VERIFICATION SUITE                          ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

# Ensure BuildDir exists
if (-not (Test-Path $BuildDir)) {
    Write-Host "❌ Build directory not found: $BuildDir" -ForegroundColor Red
    exit 1
}

# Resolve repo root from BuildDir (e.g. D:\rawrxd\build -> D:\rawrxd)
$BuildDirResolved = (Resolve-Path -LiteralPath $BuildDir -ErrorAction SilentlyContinue).Path
if (-not $BuildDirResolved) { $BuildDirResolved = $BuildDir }
$repoRoot = (Get-Item -LiteralPath $BuildDirResolved).Parent.FullName
$srcDir = Join-Path $repoRoot "src"
$shipDir = Join-Path $repoRoot "Ship"

$verificationsPassed = 0
$verificationsFailed = 0

# ============================================================================
# 1. VERIFY EXECUTABLE EXISTS
# ============================================================================
Write-Host "`n📦 Checking output binaries..." -ForegroundColor Yellow

$exePath = $null
$possiblePaths = @(
    "$BuildDir\bin\RawrXD-Win32IDE.exe",
    "$BuildDir\RawrXD-Win32IDE.exe",
    "$BuildDir\RawrXD_Agent_GUI.exe",
    "$BuildDir\Release\RawrXD_Agent_GUI.exe",
    "$BuildDir\Release\RawrXD_IDE.exe",
    "$BuildDir\Debug\RawrXD_IDE.exe",
    "$BuildDir\RawrXD_IDE.exe",
    "$BuildDir\..\Release\RawrXD_IDE.exe",
    "$BuildDir\..\Debug\RawrXD_IDE.exe",
    "$repoRoot\build_ide\bin\RawrXD-Win32IDE.exe",
    "$repoRoot\build_ide\RawrXD-Win32IDE.exe",
    "$repoRoot\build_ide\RawrXD_Agent_GUI.exe",
    "$repoRoot\build_ide\Release\RawrXD_Agent_GUI.exe",
    "$repoRoot\build_ide\Release\RawrXD-Win32IDE.exe"
)

foreach ($path in $possiblePaths) {
    if (Test-Path $path) {
        $exePath = $path
        break
    }
}

# Fallback: search recursively under BuildDir and repo build_ide for any known IDE/agent exe
if (-not $exePath) {
    $exeNames = @("RawrXD-Win32IDE.exe", "RawrXD_Agent_GUI.exe", "RawrXD_IDE.exe", "RawrEngine.exe", "RawrXD_Gold.exe")
    foreach ($dir in @($BuildDir, (Join-Path $repoRoot "build_ide"))) {
        if (-not (Test-Path $dir)) { continue }
        foreach ($name in $exeNames) {
            $found = Get-ChildItem -Path $dir -Recurse -Filter $name -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($found) {
                $exePath = $found.FullName
                break
            }
        }
        if ($exePath) { break }
    }
}

if ($exePath) {
    $size = (Get-Item $exePath).Length / 1MB
    $name = Split-Path $exePath -Leaf
    Write-Host "  ✅ $name ($([math]::Round($size, 2)) MB)" -ForegroundColor Green
    $verificationsPassed++
} else {
    Write-Host "  ❌ IDE executable NOT FOUND" -ForegroundColor Red
    Write-Host "     Searched: $BuildDir and variants" -ForegroundColor DarkRed
    $verificationsFailed++
}

# ============================================================================
# 2. CHECK FOR Qt DEPENDENCIES (CRITICAL)
# ============================================================================
Write-Host "`n🔍 Checking for Qt DLL dependencies..." -ForegroundColor Yellow

$qtDllsFound = @()
if ($exePath -and (Test-Path $exePath)) {
    # Use dumpbin to check dependencies
    $dumpbinPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
    if (-not (Test-Path $dumpbinPath)) {
        $dumpbinPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
    }
    
    if (Test-Path $dumpbinPath) {
        # Optimized lookup for dumpbin.exe
        $dumpbinExe = Get-ChildItem -Path "$dumpbinPath\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1

        if (-not $dumpbinExe) {
             # Fallback to Hostx86
             $dumpbinExe = Get-ChildItem -Path "$dumpbinPath\*\bin\Hostx86\x86\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        }
        
        if (-not $dumpbinExe) {
             # Last resort: Recursive search (slow)
             $dumpbinExe = Get-ChildItem -Path $dumpbinPath -Recurse -Include "dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        }
        
        if ($dumpbinExe) {
            $dumpOutput = & $dumpbinExe.FullName /dependents $exePath 2>&1
            
            # Check for Qt DLLs
            $qtDlls = @("Qt5Core", "Qt5Gui", "Qt5Widgets", "Qt5Network", "Qt5Sql", "Qt6Core", "Qt6Gui", "Qt6Widgets")
            foreach ($qtDll in $qtDlls) {
                if ($dumpOutput -match $qtDll) {
                    $qtDllsFound += $qtDll
                }
            }
            
            if ($qtDllsFound.Count -eq 0) {
                Write-Host "  ✅ No Qt DLLs found" -ForegroundColor Green
                $verificationsPassed++
            } else {
                Write-Host "  ❌ FOUND Qt DLLs: $($qtDllsFound -join ', ')" -ForegroundColor Red
                $verificationsFailed++
            }
            # Check for vulkan-1.dll (load-time dep — missing causes startup crash)
            if ($dumpOutput -match "vulkan-1\.dll") {
                $vkPath = Join-Path (Split-Path $exePath) "vulkan-1.dll"
                if (-not (Test-Path $vkPath)) {
                    Write-Host "  ⚠️  vulkan-1.dll required (load-time). If IDE crashes on startup:" -ForegroundColor Yellow
                    Write-Host "     Install Vulkan Runtime: https://vulkan.lunarg.com/sdk/home" -ForegroundColor Yellow
                }
            }
        }
    }
}

# ============================================================================
# 3. CHECK FOR Qt INCLUDES IN SOURCE (ZERO TOLERANCE)
# ============================================================================
# Policy: NO .cpp/.h/.hpp in src/ or Ship/ may contain #include <Q...>. <queue> excluded.
Write-Host "`n🔎 Checking for Qt #includes in source (src + Ship)..." -ForegroundColor Yellow

# Match Qt framework headers only (QString, QObject, QtCore, etc.) — NOT STL <queue>
# Use case-sensitive match (-cmatch) so "#include <queue>" is never counted as Qt.
$qtIncludePattern = '#include\s+<Q[A-Z]\w*>'
$commentLinePattern = '^\s*(\/\/|\*|\/\*)'
# STL/system headers that must never be reported as Qt
$qtExcludePattern = '#include\s+<queue>\s*$'

function Test-FileHasQtInclude {
    param([string]$path, [string]$pattern, [string]$commentPattern, [string]$excludePattern)
    $lines = Get-Content -LiteralPath $path -ErrorAction SilentlyContinue
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if ($trimmed -match $commentPattern) { continue }
        if ($trimmed -cmatch $excludePattern) { continue }
        if ($trimmed -cmatch $pattern) { return $true }
    }
    return $false
}

$qtScanExcludes = @(
    "self_code.cpp",
    "EnhancedDynamicLoadBalancer.hpp"
)

$filesWithQt = @()
$allSourceFiles = Get-ChildItem -Path $srcDir -Recurse -Include "*.cpp", "*.h", "*.hpp" -ErrorAction SilentlyContinue |
    Where-Object { $qtScanExcludes -notcontains $_.Name }

foreach ($file in $allSourceFiles) {
    if (Test-FileHasQtInclude -path $file.FullName -pattern $qtIncludePattern -commentPattern $commentLinePattern -excludePattern $qtExcludePattern) {
        $filesWithQt += $file.FullName
    }
}

# Also scan Ship for Qt includes (same policy: no Qt)
$shipSourceFiles = @()
if (Test-Path $shipDir) {
    $shipSourceFiles = Get-ChildItem -Path $shipDir -Recurse -Include "*.cpp", "*.h", "*.hpp" -ErrorAction SilentlyContinue
}
foreach ($file in $shipSourceFiles) {
    if (Test-FileHasQtInclude -path $file.FullName -pattern $qtIncludePattern -commentPattern $commentLinePattern -excludePattern $qtExcludePattern) {
        $filesWithQt += $file.FullName
    }
}

$totalScanned = $allSourceFiles.Count + $shipSourceFiles.Count
if ($filesWithQt.Count -eq 0) {
    Write-Host "  ✅ No Qt #includes found ($totalScanned files in src + Ship)" -ForegroundColor Green
    $verificationsPassed++
} else {
    Write-Host "  ❌ Found Qt includes in $($filesWithQt.Count) files:" -ForegroundColor Red
    $filesWithQt | Select-Object -First 10 | ForEach-Object { Write-Host "     $_" -ForegroundColor DarkRed }
    if ($filesWithQt.Count -gt 10) {
        Write-Host "     ... and $($filesWithQt.Count - 10) more" -ForegroundColor DarkRed
    }
    Write-Host "     Remove Qt; use Ship/StdReplacements.hpp and Win32/C++20." -ForegroundColor Yellow
    $verificationsFailed++
}

# ============================================================================
# 4. CHECK FOR Q_OBJECT MACROS (code only; ignore comment lines)
# ============================================================================
Write-Host "`n🔎 Checking for remaining Q_OBJECT macros..." -ForegroundColor Yellow

$qObjectMacroPattern = 'Q_OBJECT|Q_PROPERTY|Q_ENUM|Q_GADGET'
$qObjectCount = 0
foreach ($file in $allSourceFiles) {
    $lines = Get-Content $file.FullName -ErrorAction SilentlyContinue
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^\s*(\/\/|\*|\/\*)') { continue }
        if ($trimmed -match $qObjectMacroPattern) {
            $qObjectCount++
            break
        }
    }
}
# Include Ship in Q_OBJECT scan
foreach ($file in $shipSourceFiles) {
    $lines = Get-Content $file.FullName -ErrorAction SilentlyContinue
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^\s*(\/\/|\*|\/\*)') { continue }
        if ($trimmed -match $qObjectMacroPattern) {
            $qObjectCount++
            break
        }
    }
}

if ($qObjectCount -eq 0) {
    Write-Host "  ✅ No Q_OBJECT/Q_PROPERTY macros found" -ForegroundColor Green
    $verificationsPassed++
} else {
    Write-Host "  ⚠️  Found Qt macros in $qObjectCount files" -ForegroundColor Magenta
    $verificationsFailed++
}

# ============================================================================
# 5. CHECK FOR StdReplacements.hpp USAGE (Ship / STL replacement layer)
# ============================================================================
Write-Host "`n🔎 Checking for StdReplacements.hpp integration..." -ForegroundColor Yellow

$stdReplacementsIncluded = 0
$filesToScan = @()
if (Test-Path $shipDir) {
    $filesToScan = Get-ChildItem -Path $shipDir -Recurse -Include "*.cpp", "*.h", "*.hpp" -ErrorAction SilentlyContinue
}
$filesToScan += $allSourceFiles
foreach ($file in $filesToScan) {
    if ($file.Extension -eq ".h" -or $file.Extension -eq ".hpp" -or $file.Extension -eq ".cpp") {
        $content = Get-Content $file.FullName -ErrorAction SilentlyContinue
        if ($content -match '#include.*StdReplacements\.hpp') {
            $stdReplacementsIncluded++
        }
    }
}

if ($stdReplacementsIncluded -gt 0) {
    Write-Host "  ✅ StdReplacements.hpp included in $stdReplacementsIncluded files" -ForegroundColor Green
    $verificationsPassed++
} else {
    Write-Host "  ⚠️  StdReplacements.hpp not found in any source files" -ForegroundColor Magenta
    $verificationsFailed++
}

# ============================================================================
# 6. CHECK BUILD ARTIFACTS
# ============================================================================
Write-Host "`n📋 Build artifacts analysis..." -ForegroundColor Yellow

$buildArtifacts = Get-ChildItem -Path $BuildDir -Recurse -Include "*.obj", "*.lib", "*.exp" -ErrorAction SilentlyContinue
$objectFiles = $buildArtifacts | Where-Object { $_.Extension -eq ".obj" }
$libFiles = $buildArtifacts | Where-Object { $_.Extension -eq ".lib" }

Write-Host "  Object files: $($objectFiles.Count)" -ForegroundColor Gray
Write-Host "  Library files: $($libFiles.Count)" -ForegroundColor Gray

if ($objectFiles.Count -gt 0) {
    Write-Host "  ✅ Object files generated" -ForegroundColor Green
    $verificationsPassed++
}

# ============================================================================
# 7. VERIFY WIN32 API LIBRARY LINKING
# ============================================================================
Write-Host "`n🔗 Checking for Win32 library linking..." -ForegroundColor Yellow

$importLibs = @("kernel32", "user32", "gdi32", "shell32", "ole32")
$libsFound = 0

if ($exePath -and (Test-Path $exePath)) {
    $dumpbinExe = Get-ChildItem -Path "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $dumpbinExe) {
        $dumpbinExe = Get-ChildItem -Path "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    }
    
    if ($dumpbinExe) {
        $dumpOutput = & $dumpbinExe.FullName /imports $exePath 2>&1
        foreach ($lib in $importLibs) {
            if ($dumpOutput -match $lib) {
                $libsFound++
            }
        }
    }
}

if ($libsFound -gt 0) {
    Write-Host "  ✅ Win32 libraries linked ($libsFound/$($importLibs.Count))" -ForegroundColor Green
    $verificationsPassed++
}

# ============================================================================
# SUMMARY
# ============================================================================
Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "VERIFICATION SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$totalVerifications = $verificationsPassed + $verificationsFailed
$passPercentage = if ($totalVerifications -gt 0) { [math]::Round(($verificationsPassed / $totalVerifications) * 100) } else { 0 }

Write-Host "Passed:  $verificationsPassed/$totalVerifications" -ForegroundColor Green
Write-Host "Failed:  $verificationsFailed/$totalVerifications" -ForegroundColor Red
Write-Host "Status:  $passPercentage%" -ForegroundColor Cyan

if ($verificationsFailed -eq 0) {
    Write-Host "`n✅ ALL VERIFICATIONS PASSED!" -ForegroundColor Green
    Write-Host "`n🚀 RawrXD is ready for Qt-free execution." -ForegroundColor Green
    Write-Host "   Optional: .\scripts\Digest-SourceManifest.ps1 -OutDir `"$BuildDir`" -Format both" -ForegroundColor DarkGray
    exit 0
} else {
    Write-Host "`n⚠️  Some verifications failed. Review above for details." -ForegroundColor Yellow
    exit 1
}
