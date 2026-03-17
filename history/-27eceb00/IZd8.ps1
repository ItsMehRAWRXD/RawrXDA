#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD Build Orchestrator — Zero-dependency PowerShell build system
.DESCRIPTION
    Replaces CMake/MSBuild with a pure PowerShell build pipeline.
    Auto-detects Visual Studio, compiles MASM64 + C++, links to .exe.
.PARAMETER Config
    Build configuration: Release or Debug (default: Release)
.PARAMETER Clean
    Remove all build artifacts before building
.PARAMETER NoLogo
    Suppress banner output
.PARAMETER OutputDir
    Output directory for final binaries (default: bin)
.PARAMETER ObjDir
    Intermediate object directory (default: obj)
.PARAMETER SourceDirs
    Array of source directories to scan (default: current directory)
.PARAMETER Target
    Output executable name (default: terraform.exe)
#>
[CmdletBinding()]
param(
    [ValidateSet('Release','Debug')]
    [string]$Config = 'Release',
    [switch]$Clean,
    [switch]$NoLogo,
    [string]$OutputDir = 'bin',
    [string]$ObjDir = 'obj',
    [string[]]$SourceDirs = @('.'),
    [string]$Target = 'terraform.exe'
)

$ErrorActionPreference = 'Stop'
$script:Errors = @()
$script:StartTime = Get-Date

# ─── Banner ──────────────────────────────────────────────────────────────
function Show-Banner {
    if ($NoLogo) { return }
    Write-Host @"
╔══════════════════════════════════════════════════════╗
║       RawrXD Build Orchestrator v1.0                ║
║       Pure PowerShell — Zero Dependencies           ║
╚══════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan
    Write-Host "  Config:  $Config" -ForegroundColor Gray
    Write-Host "  Target:  $Target" -ForegroundColor Gray
    Write-Host ""
}

# ─── VS Detection ────────────────────────────────────────────────────────
function Find-VisualStudio {
    Write-Host "[*] Detecting Visual Studio..." -ForegroundColor Yellow

    # Try vswhere first
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -property installationPath 2>$null
        if ($vsPath -and (Test-Path $vsPath)) {
            Write-Host "    Found via vswhere: $vsPath" -ForegroundColor Green
            return $vsPath
        }
    }

    # Fallback: check known paths
    $knownPaths = @(
        'C:\VS2022Enterprise',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional',
        'C:\Program Files\Microsoft Visual Studio\2022\Community',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools'
    )

    foreach ($p in $knownPaths) {
        if (Test-Path $p) {
            Write-Host "    Found at: $p" -ForegroundColor Green
            return $p
        }
    }

    throw "Visual Studio not found. Install VS2022 or Build Tools."
}

function Find-MSVC-Tools {
    param([string]$vsPath)

    $msvcRoot = Join-Path $vsPath 'VC\Tools\MSVC'
    if (-not (Test-Path $msvcRoot)) {
        throw "MSVC tools not found at $msvcRoot"
    }

    # Get latest version
    $versions = Get-ChildItem $msvcRoot -Directory | Sort-Object Name -Descending
    if ($versions.Count -eq 0) {
        throw "No MSVC versions found in $msvcRoot"
    }

    $latest = $versions[0].FullName
    $binDir = Join-Path $latest 'bin\Hostx64\x64'

    $ml64  = Join-Path $binDir 'ml64.exe'
    $cl    = Join-Path $binDir 'cl.exe'
    $link  = Join-Path $binDir 'link.exe'
    $lib   = Join-Path $binDir 'lib.exe'

    if (-not (Test-Path $ml64)) { throw "ml64.exe not found at $ml64" }
    if (-not (Test-Path $link)) { throw "link.exe not found at $link" }

    $includeDir = Join-Path $latest 'include'
    $libDir     = Join-Path $latest 'lib\x64'

    return @{
        ML64       = $ml64
        CL         = $cl
        LINK       = $link
        LIB        = $lib
        IncludeDir = $includeDir
        LibDir     = $libDir
        BinDir     = $binDir
        Version    = $versions[0].Name
    }
}

function Find-WindowsSDK {
    $sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
    if (-not (Test-Path $sdkRoot)) {
        $sdkRoot = "C:\Program Files (x86)\Windows Kits\10"
    }
    if (-not (Test-Path $sdkRoot)) {
        Write-Host "    [!] Windows SDK not found — linking may need manual paths" -ForegroundColor Yellow
        return $null
    }

    $versions = Get-ChildItem (Join-Path $sdkRoot 'Include') -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d+\.' } |
        Sort-Object Name -Descending

    if ($versions.Count -eq 0) {
        Write-Host "    [!] No SDK versions found" -ForegroundColor Yellow
        return $null
    }

    $sdkVer = $versions[0].Name
    Write-Host "    Windows SDK: $sdkVer" -ForegroundColor Green

    return @{
        Root       = $sdkRoot
        Version    = $sdkVer
        IncludeUCRT = Join-Path $sdkRoot "Include\$sdkVer\ucrt"
        IncludeUM   = Join-Path $sdkRoot "Include\$sdkVer\um"
        IncludeShared = Join-Path $sdkRoot "Include\$sdkVer\shared"
        LibUCRT    = Join-Path $sdkRoot "Lib\$sdkVer\ucrt\x64"
        LibUM      = Join-Path $sdkRoot "Lib\$sdkVer\um\x64"
    }
}

# ─── Source Discovery ────────────────────────────────────────────────────
function Find-SourceFiles {
    $asmFiles = @()
    $cppFiles = @()

    foreach ($dir in $SourceDirs) {
        $resolved = Resolve-Path $dir -ErrorAction SilentlyContinue
        if (-not $resolved) { continue }

        $asmFiles += Get-ChildItem $resolved -Filter '*.asm' -File -ErrorAction SilentlyContinue
        $cppFiles += Get-ChildItem $resolved -Filter '*.cpp' -File -ErrorAction SilentlyContinue
        $cppFiles += Get-ChildItem $resolved -Filter '*.c' -File -ErrorAction SilentlyContinue
    }

    return @{
        ASM = $asmFiles
        CPP = $cppFiles
    }
}

# ─── Incremental Build Check ────────────────────────────────────────────
function Test-NeedsBuild {
    param(
        [string]$Source,
        [string]$Object
    )

    if (-not (Test-Path $Object)) { return $true }

    $srcTime = (Get-Item $Source).LastWriteTime
    $objTime = (Get-Item $Object).LastWriteTime

    return ($srcTime -gt $objTime)
}

# ─── Compile MASM ────────────────────────────────────────────────────────
function Invoke-MASM {
    param(
        [hashtable]$Tools,
        [System.IO.FileInfo[]]$Files
    )

    if ($Files.Count -eq 0) {
        Write-Host "    No .asm files to compile" -ForegroundColor Gray
        return @()
    }

    $objects = @()
    foreach ($f in $Files) {
        $objFile = Join-Path $ObjDir ($f.BaseName + '.obj')
        $objects += $objFile

        if (-not (Test-NeedsBuild $f.FullName $objFile)) {
            Write-Host "    [skip] $($f.Name) (up to date)" -ForegroundColor DarkGray
            continue
        }

        Write-Host "    [asm]  $($f.Name)" -ForegroundColor White
        $debugFlag = if ($Config -eq 'Debug') { '/Zi' } else { '' }

        $args = @('/c', '/nologo', '/Fo', $objFile)
        if ($debugFlag) { $args += $debugFlag }
        $args += $f.FullName

        $proc = Start-Process -FilePath $Tools.ML64 -ArgumentList $args `
            -NoNewWindow -Wait -PassThru -RedirectStandardError "$ObjDir\$($f.BaseName)_asm.log"

        if ($proc.ExitCode -ne 0) {
            $errLog = Get-Content "$ObjDir\$($f.BaseName)_asm.log" -Raw -ErrorAction SilentlyContinue
            $script:Errors += "MASM error in $($f.Name): $errLog"
            Write-Host "    [FAIL] $($f.Name)" -ForegroundColor Red
            if ($errLog) { Write-Host $errLog -ForegroundColor Red }
        }
    }
    return $objects
}

# ─── Compile C++ ─────────────────────────────────────────────────────────
function Invoke-CPP {
    param(
        [hashtable]$Tools,
        [hashtable]$SDK,
        [System.IO.FileInfo[]]$Files
    )

    if ($Files.Count -eq 0) {
        Write-Host "    No C/C++ files to compile" -ForegroundColor Gray
        return @()
    }

    $objects = @()
    foreach ($f in $Files) {
        $objFile = Join-Path $ObjDir ($f.BaseName + '.obj')
        $objects += $objFile

        if (-not (Test-NeedsBuild $f.FullName $objFile)) {
            Write-Host "    [skip] $($f.Name) (up to date)" -ForegroundColor DarkGray
            continue
        }

        Write-Host "    [c++]  $($f.Name)" -ForegroundColor White

        $args = @('/c', '/nologo', '/EHsc', '/std:c++17', "/Fo$objFile")

        if ($Config -eq 'Debug') {
            $args += @('/Od', '/Zi', '/MDd')
        } else {
            $args += @('/O2', '/MD', '/DNDEBUG')
        }

        # Include paths
        $args += "/I`"$($Tools.IncludeDir)`""
        if ($SDK) {
            $args += "/I`"$($SDK.IncludeUCRT)`""
            $args += "/I`"$($SDK.IncludeUM)`""
            $args += "/I`"$($SDK.IncludeShared)`""
        }

        $args += $f.FullName

        $proc = Start-Process -FilePath $Tools.CL -ArgumentList $args `
            -NoNewWindow -Wait -PassThru -RedirectStandardError "$ObjDir\$($f.BaseName)_cpp.log"

        if ($proc.ExitCode -ne 0) {
            $errLog = Get-Content "$ObjDir\$($f.BaseName)_cpp.log" -Raw -ErrorAction SilentlyContinue
            $script:Errors += "C++ error in $($f.Name): $errLog"
            Write-Host "    [FAIL] $($f.Name)" -ForegroundColor Red
            if ($errLog) { Write-Host $errLog -ForegroundColor Red }
        }
    }
    return $objects
}

# ─── Link ────────────────────────────────────────────────────────────────
function Invoke-Link {
    param(
        [hashtable]$Tools,
        [hashtable]$SDK,
        [string[]]$Objects
    )

    if ($Objects.Count -eq 0) {
        Write-Host "    [!] No objects to link" -ForegroundColor Yellow
        return
    }

    $outFile = Join-Path $OutputDir $Target
    Write-Host "    [link] -> $outFile" -ForegroundColor Cyan

    $args = @('/NOLOGO', "/OUT:$outFile", '/ENTRY:_start', '/SUBSYSTEM:CONSOLE')

    if ($Config -eq 'Debug') {
        $args += '/DEBUG'
    } else {
        $args += @('/OPT:REF', '/OPT:ICF')
    }

    # Library paths
    $args += "/LIBPATH:`"$($Tools.LibDir)`""
    if ($SDK) {
        $args += "/LIBPATH:`"$($SDK.LibUCRT)`""
        $args += "/LIBPATH:`"$($SDK.LibUM)`""
    }

    # Default libs for standalone MASM
    $args += @('kernel32.lib')

    # Object files
    $args += $Objects

    $proc = Start-Process -FilePath $Tools.LINK -ArgumentList $args `
        -NoNewWindow -Wait -PassThru -RedirectStandardError "$ObjDir\link.log"

    if ($proc.ExitCode -ne 0) {
        $errLog = Get-Content "$ObjDir\link.log" -Raw -ErrorAction SilentlyContinue
        $script:Errors += "Link error: $errLog"
        Write-Host "    [FAIL] Linking failed" -ForegroundColor Red
        if ($errLog) { Write-Host $errLog -ForegroundColor Red }
    } else {
        $size = (Get-Item $outFile).Length
        Write-Host "    [OK]   $outFile ($size bytes)" -ForegroundColor Green
    }
}

# ─── Clean ───────────────────────────────────────────────────────────────
function Invoke-Clean {
    Write-Host "[*] Cleaning build artifacts..." -ForegroundColor Yellow
    if (Test-Path $ObjDir) { Remove-Item $ObjDir -Recurse -Force }
    if (Test-Path $OutputDir) { Remove-Item $OutputDir -Recurse -Force }
    Write-Host "    Done." -ForegroundColor Green
}

# ─── Main ────────────────────────────────────────────────────────────────
function Main {
    Show-Banner

    if ($Clean) {
        Invoke-Clean
    }

    # Create directories
    if (-not (Test-Path $ObjDir))    { New-Item $ObjDir -ItemType Directory -Force | Out-Null }
    if (-not (Test-Path $OutputDir)) { New-Item $OutputDir -ItemType Directory -Force | Out-Null }

    # Detect toolchain
    $vsPath = Find-VisualStudio
    $tools  = Find-MSVC-Tools -vsPath $vsPath
    $sdk    = Find-WindowsSDK

    Write-Host "    MSVC:  $($tools.Version)" -ForegroundColor Green
    Write-Host "    ml64:  $($tools.ML64)" -ForegroundColor Green
    Write-Host ""

    # Discover sources
    Write-Host "[*] Scanning sources..." -ForegroundColor Yellow
    $sources = Find-SourceFiles
    Write-Host "    Found: $($sources.ASM.Count) .asm, $($sources.CPP.Count) .cpp/.c" -ForegroundColor Gray
    Write-Host ""

    # Compile
    Write-Host "[*] Compiling MASM..." -ForegroundColor Yellow
    $asmObjects = Invoke-MASM -Tools $tools -Files $sources.ASM
    Write-Host ""

    if ($sources.CPP.Count -gt 0) {
        Write-Host "[*] Compiling C/C++..." -ForegroundColor Yellow
        $cppObjects = Invoke-CPP -Tools $tools -SDK $sdk -Files $sources.CPP
        Write-Host ""
    } else {
        $cppObjects = @()
    }

    $allObjects = @($asmObjects) + @($cppObjects) | Where-Object { $_ }

    # Link
    Write-Host "[*] Linking..." -ForegroundColor Yellow
    Invoke-Link -Tools $tools -SDK $sdk -Objects $allObjects
    Write-Host ""

    # Summary
    $elapsed = (Get-Date) - $script:StartTime

    if ($script:Errors.Count -gt 0) {
        Write-Host "╔══════════════════════════════════════════════════════╗" -ForegroundColor Red
        Write-Host "║  BUILD FAILED — $($script:Errors.Count) error(s)                        ║" -ForegroundColor Red
        Write-Host "╚══════════════════════════════════════════════════════╝" -ForegroundColor Red
        foreach ($e in $script:Errors) {
            Write-Host "  ! $e" -ForegroundColor Red
        }
        exit 1
    } else {
        Write-Host "╔══════════════════════════════════════════════════════╗" -ForegroundColor Green
        Write-Host "║  BUILD SUCCESS                                      ║" -ForegroundColor Green
        Write-Host "╚══════════════════════════════════════════════════════╝" -ForegroundColor Green
        Write-Host "  Time: $([math]::Round($elapsed.TotalSeconds, 2))s" -ForegroundColor Gray
    }
}

Main
