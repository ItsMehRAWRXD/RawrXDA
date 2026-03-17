<# ============================================================================
   build_pe_writer_asm.ps1 — Build RawrXD_PE_Writer.asm into executable
   
   Usage: .\build_pe_writer_asm.ps1
   Output: RawrXD_PE_Writer.exe (working executable)
   ============================================================================ #>
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Write-Host "[Build] Building RawrXD PE Writer from Assembly..." -ForegroundColor Green

# Paths
$AsmFile = "D:\RawrXD\RawrXD_PE_Writer.asm"
$ObjFile = "D:\RawrXD\RawrXD_PE_Writer.obj"
$TestAsm = "D:\RawrXD\pe_writer_test.asm"
$TestObj = "D:\RawrXD\pe_writer_test.obj"
$ExeFile = "D:\RawrXD\RawrXD_PE_Writer.exe"
$LibFile = "D:\RawrXD\RawrXD_PE_Writer.lib"

# Clean previous builds
if (Test-Path $ObjFile) { Remove-Item $ObjFile -Force }
if (Test-Path $TestObj) { Remove-Item $TestObj -Force }
if (Test-Path $ExeFile) { Remove-Item $ExeFile -Force }
if (Test-Path $LibFile) { Remove-Item $LibFile -Force }

# ── Locate MASM and MSVC ──
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallDir = $null
if (Test-Path $vswhere) {
    $vsInstallDir = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null | Select-Object -First 1
}
if (-not $vsInstallDir) {
    # Try common VS paths
    $knownPaths = @(
        'D:\VS2022Enterprise',
        'C:\VS2022Enterprise', 
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise',
        'C:\Program Files\Microsoft Visual Studio\2022\Community',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
    )
    foreach ($p in $knownPaths) {
        if (Test-Path "$p\VC\Auxiliary\Build\vcvars64.bat") {
            $vsInstallDir = $p; break
        }
    }
}

if (-not $vsInstallDir) {
    Write-Error 'Visual Studio 2022 x64 tools not found.'
    exit 1
}

Write-Host "[Build] VS found at: $vsInstallDir" -ForegroundColor Cyan

# Set up tool paths
$MasmPath = "$vsInstallDir\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe"
$LinkPath = "$vsInstallDir\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe"
$LibPath = "$vsInstallDir\VC\Tools\MSVC\*\bin\Hostx64\x64\lib.exe"

# Find actual MASM and Link paths
$MasmExe = Get-ChildItem $MasmPath | Select-Object -First 1 -ExpandProperty FullName
$LinkExe = Get-ChildItem $LinkPath | Select-Object -First 1 -ExpandProperty FullName
$LibExe = Get-ChildItem $LibPath | Select-Object -First 1 -ExpandProperty FullName

if (-not (Test-Path $MasmExe)) {
    Write-Error "MASM (ml64.exe) not found at: $MasmPath"
    exit 1
}
if (-not (Test-Path $LinkExe)) {
    Write-Error "Link (link.exe) not found at: $LinkPath"
    exit 1
}
if (-not (Test-Path $LibExe)) {
    Write-Error "Lib (lib.exe) not found at: $LibPath"
    exit 1
}

Write-Host "[Build] MASM: $MasmExe" -ForegroundColor Yellow
Write-Host "[Build] Link: $LinkExe" -ForegroundColor Yellow
Write-Host "[Build] Lib:  $LibExe" -ForegroundColor Yellow

# Resolve MSVC and Windows SDK library folders for standalone link.exe usage
$masmDir = Split-Path $MasmExe -Parent
$msvcRoot = (Resolve-Path (Join-Path $masmDir "..\\..\\..")).Path
$msvcLibX64 = Join-Path $msvcRoot "lib\x64"
$msvcLibOneCore = Join-Path $msvcRoot "lib\onecore\x64"

$sdkRoot = if (Test-Path "C:\Program Files (x86)\Windows Kits\10\Lib") {
    "C:\Program Files (x86)\Windows Kits\10\Lib"
} elseif (Test-Path "D:\Program Files (x86)\Windows Kits\10\Lib") {
    "D:\Program Files (x86)\Windows Kits\10\Lib"
} else {
    $null
}

$sdkUmX64 = $null
$sdkUcrtX64 = $null
if ($sdkRoot) {
    $versions = Get-ChildItem $sdkRoot -Directory | Where-Object { $_.Name -match '^\d+\.' } | Sort-Object Name -Descending
    foreach ($v in $versions) {
        $um = Join-Path $v.FullName "um\x64"
        $ucrt = Join-Path $v.FullName "ucrt\x64"
        if ((Test-Path $um) -and (Test-Path $ucrt)) {
            $sdkUmX64 = $um
            $sdkUcrtX64 = $ucrt
            break
        }
    }
    if (-not $sdkUmX64 -and $versions) {
        $sdkUmX64 = Join-Path $versions[0].FullName "um\x64"
    }
}

$libPaths = @()
foreach ($p in @($msvcLibX64, $msvcLibOneCore, $sdkUcrtX64, $sdkUmX64)) {
    if ($p -and (Test-Path $p)) { $libPaths += $p }
}

if ($libPaths.Count -eq 0) {
    Write-Error "No valid library paths found for MSVC/Windows SDK"
    exit 1
}

Write-Host "[Build] Library paths:" -ForegroundColor Cyan
$libPaths | ForEach-Object { Write-Host "  - $_" -ForegroundColor Gray }

try {
    # Step 1: Assemble
    Write-Host "[Step 1] Assembling..." -ForegroundColor Magenta
    $MasmArgs = @(
        "/c"           # Assemble only
        "/Fo$ObjFile"  # Output object file
        $AsmFile       # Source file
    )
    
    Write-Host "Running: $MasmExe $($MasmArgs -join ' ')" -ForegroundColor Gray
    $proc = Start-Process -FilePath $MasmExe -ArgumentList $MasmArgs -NoNewWindow -Wait -PassThru
    
    if ($proc.ExitCode -ne 0) {
        Write-Error "Assembly failed with exit code: $($proc.ExitCode)"
        exit 1
    }
    
    if (-not (Test-Path $ObjFile)) {
        Write-Error "Object file was not created: $ObjFile"
        exit 1
    }
    
    Write-Host "[OK] Assembly successful" -ForegroundColor Green

    # If the core module has no main entrypoint, use test harness when available.
    $asmText = Get-Content $AsmFile -Raw
    $hasMain = ($asmText -match '(?im)^\s*PUBLIC\s+main\s*$' -or
                $asmText -match '(?im)^\s*main\s+PROC\b' -or
                $asmText -match '(?im)^\s*WinMainCRTStartup\s+PROC\b' -or
                $asmText -match '(?im)^\s*mainCRTStartup\s+PROC\b')

    $linkInputs = @($ObjFile)
    if (-not $hasMain -and (Test-Path $TestAsm)) {
        Write-Host "[Step 2] Assembling test harness (no main in PE writer module)..." -ForegroundColor Magenta
        $testMasmArgs = @("/c", "/Fo$TestObj", $TestAsm)
        Write-Host "Running: $MasmExe $($testMasmArgs -join ' ')" -ForegroundColor Gray
        $proc = Start-Process -FilePath $MasmExe -ArgumentList $testMasmArgs -NoNewWindow -Wait -PassThru
        if ($proc.ExitCode -ne 0 -or -not (Test-Path $TestObj)) {
            Write-Error "Test harness assembly failed (exit code: $($proc.ExitCode))"
            exit 1
        }
        $linkInputs = @($TestObj, $ObjFile)
        Write-Host "[OK] Test harness assembled" -ForegroundColor Green
    }

    # Step 3: Link
    Write-Host "[Step 3] Linking..." -ForegroundColor Magenta
    $LinkArgs = @(
        "/SUBSYSTEM:CONSOLE"
        "/MACHINE:X64"
        "/ENTRY:main"
        "/OUT:$ExeFile"         # Output executable
    )
    $LinkArgs += $linkInputs
    foreach ($lp in $libPaths) {
        $LinkArgs += "/LIBPATH:`"$lp`""
    }
    $LinkArgs += @(
        "kernel32.lib"
        "user32.lib"
        "libcmt.lib"
    )
    
    Write-Host "Running: $LinkExe $($LinkArgs -join ' ')" -ForegroundColor Gray
    $proc = Start-Process -FilePath $LinkExe -ArgumentList $LinkArgs -NoNewWindow -Wait -PassThru
    
    if ($proc.ExitCode -ne 0) {
        # If there's no viable entrypoint, fall back to generating a static library.
        if (-not $hasMain -and -not (Test-Path $TestAsm)) {
            Write-Host "[Info] No executable entrypoint available; generating static library instead..." -ForegroundColor Yellow
            $libArgs = @("/OUT:$LibFile", $ObjFile)
            $libProc = Start-Process -FilePath $LibExe -ArgumentList $libArgs -NoNewWindow -Wait -PassThru
            if ($libProc.ExitCode -ne 0 -or -not (Test-Path $LibFile)) {
                Write-Error "Library generation failed (exit code: $($libProc.ExitCode))"
                exit 1
            }
            Write-Host "[SUCCESS] Library created: $LibFile" -ForegroundColor Green
            exit 0
        }
        Write-Error "Link failed with exit code: $($proc.ExitCode)"
        exit 1
    }
    
    if (Test-Path $ExeFile) {
        Write-Host "[SUCCESS] Executable created: $ExeFile" -ForegroundColor Green
        $size = (Get-Item $ExeFile).Length
        Write-Host "[Info] File size: $size bytes" -ForegroundColor Cyan
    } else {
        Write-Host "[Warning] Executable not found, but object file exists" -ForegroundColor Yellow
        Write-Host "[Info] Object file: $ObjFile" -ForegroundColor Cyan
    }
    
} catch {
    Write-Error "Build failed: $_"
    exit 1
}

Write-Host "[Build Complete]" -ForegroundColor Green
