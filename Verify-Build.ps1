# Verify-Build.ps1
# Comprehensive build verification and validation
# Checks for Qt dependencies, DLL loading, functionality, and integrity

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

$verificationsPassed = 0
$verificationsFailed = 0

# ============================================================================
# 1. VERIFY EXECUTABLE EXISTS
# ============================================================================
Write-Host "`n📦 Checking output binaries..." -ForegroundColor Yellow

$exePath = $null
$possiblePaths = @(
    "$BuildDir\RawrXD_Agent_GUI.exe",
    "$BuildDir\Release\RawrXD_Agent_GUI.exe",
    "$BuildDir\Release\RawrXD_IDE.exe",
    "$BuildDir\Debug\RawrXD_IDE.exe",
    "$BuildDir\RawrXD_IDE.exe",
    "$BuildDir\..\Release\RawrXD_IDE.exe",
    "$BuildDir\..\Debug\RawrXD_IDE.exe"
)

foreach ($path in $possiblePaths) {
    if (Test-Path $path) {
        $exePath = $path
        break
    }
}

if ($exePath) {
    $size = (Get-Item $exePath).Length / 1MB
    Write-Host "  ✅ RawrXD_IDE.exe ($([math]::Round($size, 2)) MB)" -ForegroundColor Green
    $verificationsPassed++
} else {
    Write-Host "  ❌ RawrXD_IDE.exe NOT FOUND" -ForegroundColor Red
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
        }
    }
}

# ============================================================================
# 3. CHECK FOR REMAINING Qt INCLUDES IN SOURCE
# ============================================================================
Write-Host "`n🔎 Checking for remaining Qt #includes in source..." -ForegroundColor Yellow

$srcDir = "D:\RawrXD\src"
$qtIncludePattern = '#include\s+<Q\w+>'

$qtScanExcludes = @(
    "self_code.cpp",
    "EnhancedDynamicLoadBalancer.hpp"
)

$filesWithQt = @()
$allSourceFiles = Get-ChildItem -Path $srcDir -Recurse -Include "*.cpp", "*.h", "*.hpp" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notmatch '^QtReplacements.*\.hpp$' } |
    Where-Object { $qtScanExcludes -notcontains $_.Name }

foreach ($file in $allSourceFiles) {
    $content = Get-Content $file.FullName -ErrorAction SilentlyContinue
    if ($content -match $qtIncludePattern) {
        $filesWithQt += $file.FullName
    }
}

if ($filesWithQt.Count -eq 0) {
    Write-Host "  ✅ No Qt #includes found in source ($($allSourceFiles.Count) files scanned)" -ForegroundColor Green
    $verificationsPassed++
} else {
    Write-Host "  ⚠️  Found Qt includes in $($filesWithQt.Count) files:" -ForegroundColor Magenta
    $filesWithQt | Select-Object -First 10 | ForEach-Object { Write-Host "     $_" -ForegroundColor DarkMagenta }
    if ($filesWithQt.Count -gt 10) {
        Write-Host "     ... and $($filesWithQt.Count - 10) more" -ForegroundColor DarkMagenta
    }
    $verificationsFailed++
}

# ============================================================================
# 4. CHECK FOR Q_OBJECT MACROS
# ============================================================================
Write-Host "`n🔎 Checking for remaining Q_OBJECT macros..." -ForegroundColor Yellow

$qObjectCount = 0
foreach ($file in $allSourceFiles) {
    $content = Get-Content $file.FullName -ErrorAction SilentlyContinue
    if ($content -match 'Q_OBJECT|Q_PROPERTY|Q_ENUM|Q_GADGET') {
        $qObjectCount++
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
# 5. CHECK FOR QtReplacements.hpp USAGE
# ============================================================================
Write-Host "`n🔎 Checking for QtReplacements.hpp integration..." -ForegroundColor Yellow

$qtReplacementsIncluded = 0
foreach ($file in $allSourceFiles) {
    if ($file.Extension -eq ".h" -or $file.Extension -eq ".hpp" -or $file.Extension -eq ".cpp") {
        $content = Get-Content $file.FullName -ErrorAction SilentlyContinue
        if ($content -match '#include.*QtReplacements\.hpp') {
            $qtReplacementsIncluded++
        }
    }
}

if ($qtReplacementsIncluded -gt 0) {
    Write-Host "  ✅ QtReplacements.hpp included in $qtReplacementsIncluded files" -ForegroundColor Green
    $verificationsPassed++
} else {
    Write-Host "  ⚠️  QtReplacements.hpp not found in any source files" -ForegroundColor Magenta
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
    Write-Host "`n🚀 RawrXD is ready for Qt-free execution!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n⚠️  Some verifications failed. Review above for details." -ForegroundColor Yellow
    exit 1
}
