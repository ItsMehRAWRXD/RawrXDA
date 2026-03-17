# RawrXD-Build.ps1
# Pure PowerShell build system for MASM64 + C++ hybrid projects
# Usage: .\RawrXD-Build.ps1 [-Config Release|Debug] [-Clean] [-Verbose]

[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    
    [switch]$Clean,
    [switch]$NoLogo,
    [string]$OutputDir = ".\bin",
    [string]$ObjDir = ".\obj",
    [string[]]$SourceDirs = @(".\src", ".\asm", ".\core"),
    [string]$Target = "RawrXD-Win32IDE.exe",
    [ValidateSet("WINDOWS", "CONSOLE")]
    [string]$Subsystem = "WINDOWS",
    [string]$EntryPoint = ""
)

# Error handling: Stop on any error
$ErrorActionPreference = "Stop"

# =============================================================================
# 1. ENVIRONMENT DETECTION & TOOLCHAIN SETUP
# =============================================================================

function Initialize-Toolchain {
    if (-not $NoLogo) {
        Write-Host @"
╔══════════════════════════════════════════════════════════════╗
║           RAWRXD POWERBUILD v14.2.0                          ║
║     MASM64 + C++ Hybrid Build System                         ║
╚══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan
    }

    # Locate VS2022/2019 installation via vswhere
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWhere)) {
        throw "Visual Studio not found. Install VS2022 Build Tools."
    }

    $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        throw "VC++ Tools not found. Install 'Desktop development with C++' workload."
    }

    # Import VC vars
    $vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvarsPath)) {
        throw "vcvars64.bat not found at $vcvarsPath"
    }

    # Extract environment variables from vcvars
    $tempFile = [System.IO.Path]::GetTempFileName()
    cmd /c " `"$vcvarsPath`" && set > `"$tempFile`" "
    Get-Content $tempFile | ForEach-Object {
        if ($_ -match "^(.*?)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
        }
    }
    Remove-Item $tempFile

    # Verify tools
    $script:Ml64 = Get-Command ml64.exe -ErrorAction Stop
    $script:Cl = Get-Command cl.exe -ErrorAction Stop
    $script:Link = Get-Command link.exe -ErrorAction Stop
    $script:Lib = Get-Command lib.exe -ErrorAction Stop

    Write-Host "[+] Toolchain ready:" -ForegroundColor Green
    Write-Host "    ml64: $($script:Ml64.Source)"
    Write-Host "    cl:   $($script:Cl.Source)"
    Write-Host "    link: $($script:Link.Source)"
}

# =============================================================================
# 2. SOURCE DISCOVERY & INCREMENTAL BUILD LOGIC
# =============================================================================

class SourceFile {
    [string]$Path
    [string]$Type        # "ASM" or "CPP" or "C"
    [string]$ObjPath
    [datetime]$LastWrite
    [bool]$NeedsBuild
}

function Get-SourceFiles {
    $files = @()
    
    foreach ($dir in $SourceDirs) {
        if (-not (Test-Path $dir)) { continue }
        
        # MASM files
        Get-ChildItem -Path $dir -Recurse -Filter "*.asm" | ForEach-Object {
            $obj = Join-Path $script:ObjDirAbs ($_.BaseName + ".obj")
            $files += [SourceFile]@{
                Path = $_.FullName
                Type = "ASM"
                ObjPath = $obj
                LastWrite = $_.LastWriteTime
                NeedsBuild = $true
            }
        }
        
        # C++ files
        Get-ChildItem -Path $dir -Recurse -Include "*.cpp","*.cxx" | ForEach-Object {
            $obj = Join-Path $script:ObjDirAbs ($_.BaseName + ".obj")
            $files += [SourceFile]@{
                Path = $_.FullName
                Type = "CPP"
                ObjPath = $obj
                LastWrite = $_.LastWriteTime
                NeedsBuild = $true
            }
        }
        
        # C headers (for dependency checking)
        Get-ChildItem -Path $dir -Recurse -Include "*.h","*.hpp" | ForEach-Object {
            $files += [SourceFile]@{
                Path = $_.FullName
                Type = "HEADER"
                ObjPath = $null
                LastWrite = $_.LastWriteTime
                NeedsBuild = $false
            }
        }
    }
    
    return $files
}

function Test-IncrementalBuild {
    param([SourceFile[]]$Files)
    
    $headerLatest = ($Files | Where-Object { $_.Type -eq "HEADER" } | 
                     Measure-Object -Property LastWrite -Maximum).Maximum
    
    foreach ($file in $Files | Where-Object { $_.Type -ne "HEADER" }) {
        if (Test-Path $file.ObjPath) {
            $objTime = (Get-Item $file.ObjPath).LastWriteTime
            
            # Rebuild if source newer than obj
            if ($file.LastWrite -gt $objTime) {
                $file.NeedsBuild = $true
            }
            # Rebuild if any header newer than obj
            elseif ($headerLatest -gt $objTime) {
                $file.NeedsBuild = $true
                Write-Verbose "[DEP] $($file.Path) triggered by header change"
            }
            else {
                $file.NeedsBuild = $false
            }
        }
        else {
            $file.NeedsBuild = $true
        }
    }
    
    return $Files
}

# =============================================================================
# 3. COMPILATION FUNCTIONS
# =============================================================================

$script:BuildLog = @()
$script:ErrorCount = 0

function Invoke-MasmCompile {
    param([SourceFile]$File)
    
    $log = "[ASM] $($File.Path)"
    Write-Host $log -ForegroundColor Yellow -NoNewline
    
    # Resolve obj path to absolute
    $absObj = Join-Path $script:ObjDirAbs (Split-Path $File.ObjPath -Leaf)
    
    # MASM flags — NOTE: /Od /Zd /Zi are C++ flags only, NOT valid for ml64
    # ml64 does NOT support /Od, /Zd, or /Zi — debug info comes from linker /DEBUG
    $flags = @(
        "/c",
        "/nologo",
        "/W3"
    )
    
    if ($Config -eq "Release") {
        # ml64 doesn't have many optimization flags; /c is standard
    }
    # Debug mode: no /Zi for ml64 — linker /DEBUG handles debug symbols
    
    # MASM include paths  
    $flags += "/I", ".\src"
    $flags += "/I", ".\include"
    $flags += "/I", ".\asm"
    
    # Output object (ml64 requires /Fo immediately followed by path)
    $flags += "/Fo$absObj"
    
    try {
        $output = & $script:Ml64.Source $flags $File.Path 2>&1
        $exitCode = $LASTEXITCODE
        
        if ($exitCode -eq 0) {
            Write-Host " [OK]" -ForegroundColor Green
            $script:BuildLog += "[PASS] $log"
        }
        else {
            Write-Host " [FAIL]" -ForegroundColor Red
            $script:ErrorCount++
            $script:BuildLog += "[FAIL] $log`n$output"
            Write-Host $output -ForegroundColor Red
        }
    }
    catch {
        Write-Host " [ERROR] $_" -ForegroundColor Red
        $script:ErrorCount++
    }
}

function Invoke-CppCompile {
    param([SourceFile]$File)
    
    $log = "[C++] $($File.Path)"
    Write-Host $log -ForegroundColor Cyan -NoNewline
    
    # Resolve obj path to absolute
    $absObj = Join-Path $script:ObjDirAbs (Split-Path $File.ObjPath -Leaf)
    
    # C++ flags
    $flags = @(
        "/c",                    # Compile only
        "/std:c++20",            # C++20 standard
        "/EHsc",                 # Exception handling
        "/nologo",
        "/MP",                   # Multi-processor compilation
        "/Fo$absObj"
    )
    
    if ($Config -eq "Release") {
        $flags += "/O2"          # Maximize speed
        $flags += "/Ob2"         # Inline any suitable
        $flags += "/Oi"          # Enable intrinsics
        $flags += "/Ot"          # Favor fast code
        $flags += "/GL"          # Whole program optimization
        $flags += "/DNDEBUG"
        $flags += "/DRELEASE"
    }
    else {
        $flags += "/Od"          # Disable optimization
        $flags += "/Zi"          # Full debug info
        $flags += "/RTC1"        # Run-time checks
        $flags += "/DDEBUG"
        $flags += "/D_DEBUG"
    }
    
    # Includes
    $flags += "/I`".\src`""
    $flags += "/I`".\include`""
    $flags += "/I`".\3rdparty`""
    
    # Windows SDK paths (auto-detected from env, with fallbacks)
    $windowsSdkDir = $env:WindowsSdkDir
    $windowsSdkVersion = $env:WindowsSDKVersion

    if (-not $windowsSdkDir -and $env:WindowsSdkDir) {
        $windowsSdkDir = $env:WindowsSdkDir.TrimEnd('\\')
    }
    if (-not $windowsSdkVersion -and $env:WindowsSDKVersion) {
        $windowsSdkVersion = $env:WindowsSDKVersion.TrimEnd('\\')
    }

    # Fallback: probe typical install locations if vcvars didn't set them
    if (-not $windowsSdkDir) {
        $sdkPaths = @(
            "C:\Program Files (x86)\Windows Kits\10",
            "C:\Program Files\Windows Kits\10",
            "${env:ProgramFiles(x86)}\Windows Kits\10"
        )
        foreach ($path in $sdkPaths) {
            if (Test-Path $path) {
                $windowsSdkDir = $path
                # Find latest SDK version
                $includeDir = Join-Path $path "Include"
                if (Test-Path $includeDir) {
                    $latestSdk = Get-ChildItem $includeDir -Directory | 
                        Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } | 
                        Sort-Object Name -Descending | 
                        Select-Object -First 1
                    if ($latestSdk) {
                        $windowsSdkVersion = $latestSdk.Name
                        break
                    }
                }
            }
        }
    }

    if ($windowsSdkDir -and $windowsSdkVersion) {
        $umPath = Join-Path $windowsSdkDir "Include\$windowsSdkVersion\um"
        $sharedPath = Join-Path $windowsSdkDir "Include\$windowsSdkVersion\shared"
        $ucrtPath = Join-Path $windowsSdkDir "Include\$windowsSdkVersion\ucrt"
        
        if (Test-Path $umPath) { $flags += "/I`"$umPath`"" }
        if (Test-Path $sharedPath) { $flags += "/I`"$sharedPath`"" }
        if (Test-Path $ucrtPath) { $flags += "/I`"$ucrtPath`"" }
        
        # Library paths for link phase
        $libUmPath = Join-Path $windowsSdkDir "Lib\$windowsSdkVersion\um\x64"
        $libUcrtPath = Join-Path $windowsSdkDir "Lib\$windowsSdkVersion\ucrt\x64"
        
        $script:SdkLibPaths = @()
        if (Test-Path $libUmPath) { $script:SdkLibPaths += "/LIBPATH:`"$libUmPath`"" }
        if (Test-Path $libUcrtPath) { $script:SdkLibPaths += "/LIBPATH:`"$libUcrtPath`"" }
    }
    
    # Preprocessor defs
    $flags += "/DRAWXD_BUILD"
    $flags += "/DUNICODE"
    $flags += "/D_UNICODE"
    $flags += "/DWIN32_LEAN_AND_MEAN"
    $flags += "/DNOMINMAX"
    
    try {
        $output = & $script:Cl.Source $flags $File.Path 2>&1
        $exitCode = $LASTEXITCODE
        
        if ($exitCode -eq 0) {
            Write-Host " [OK]" -ForegroundColor Green
            $script:BuildLog += "[PASS] $log"
        }
        else {
            Write-Host " [FAIL]" -ForegroundColor Red
            $script:ErrorCount++
            $script:BuildLog += "[FAIL] $log`n$output"
            Write-Host $output -ForegroundColor Red
        }
    }
    catch {
        Write-Host " [ERROR] $_" -ForegroundColor Red
        $script:ErrorCount++
    }
}

# =============================================================================
# 4. LINKING PHASE
# =============================================================================

function Invoke-Link {
    param([SourceFile[]]$Files)
    
    Write-Host "`n[LINK] Starting linker..." -ForegroundColor Magenta
    
    $objFiles = $Files | Where-Object { $_.Type -ne "HEADER" } | 
                ForEach-Object { "`"$($_.ObjPath)`"" }
    
    $outPath = Join-Path $OutputDir $Target
    
    # Auto-detect entry point
    $entry = if ($EntryPoint) { $EntryPoint } 
             elseif ($Subsystem -eq "CONSOLE") { "mainCRTStartup" }
             else { "WinMainCRTStartup" }
    
    $flags = @(
        "/OUT:`"$outPath`"",
        "/NOLOGO",
        "/SUBSYSTEM:$Subsystem",
        "/ENTRY:$entry",
        "/MACHINE:X64",
        "/DYNAMICBASE",
        "/NXCOMPAT",
        "/LARGEADDRESSAWARE"
    )
    
    if ($Config -eq "Release") {
        $flags += "/OPT:REF"     # Remove unreferenced functions
        $flags += "/OPT:ICF"     # Identical COMDAT folding
        $flags += "/LTCG"        # Link-time code gen
    }
    else {
        $flags += "/DEBUG"       # Generate PDB
        $flags += "/INCREMENTAL"
    }
    
    # Libraries
    $libs = @(
        "kernel32.lib",
        "user32.lib",
        "gdi32.lib",
        "winspool.lib",
        "comdlg32.lib",
        "advapi32.lib",
        "shell32.lib",
        "ole32.lib",
        "oleaut32.lib",
        "uuid.lib",
        "odbc32.lib",
        "odbccp32.lib",
        "vulkan-1.lib"
    )
    
    $linkArgs = $flags + $script:SdkLibPaths + $objFiles + $libs
    
    try {
        $output = & $script:Link.Source $linkArgs 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[+] Link successful: $outPath" -ForegroundColor Green
            $size = (Get-Item $outPath).Length / 1MB
            Write-Host "    Size: $([math]::Round($size, 2)) MB" -ForegroundColor Gray
        }
        else {
            Write-Host "[-] Link failed:" -ForegroundColor Red
            Write-Host $output -ForegroundColor Red
            $script:ErrorCount++
        }
    }
    catch {
        Write-Host "[-] Link error: $_" -ForegroundColor Red
        $script:ErrorCount++
    }
}

# =============================================================================
# 5. MAIN BUILD PIPELINE
# =============================================================================

function Start-Build {
    # Setup directories (resolve to absolute paths)
    if (-not (Test-Path $ObjDir)) { New-Item -ItemType Directory -Path $ObjDir -Force | Out-Null }
    if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null }
    $script:ObjDirAbs = (Resolve-Path $ObjDir).Path
    $script:OutputDirAbs = (Resolve-Path $OutputDir).Path
    
    if ($Clean) {
        Write-Host "[*] Cleaning build artifacts..." -ForegroundColor Yellow
        Get-ChildItem $ObjDir -Filter "*.obj" | Remove-Item -Force
        Get-ChildItem $OutputDir -Filter "*.exe" | Remove-Item -Force
        Get-ChildItem $OutputDir -Filter "*.pdb" | Remove-Item -Force
    }
    
    # Initialize
    Initialize-Toolchain
    
    # Discover sources
    Write-Host "`n[*] Scanning source files..." -ForegroundColor White
    $sources = Get-SourceFiles
    $sources = Test-IncrementalBuild $sources
    
    $toBuild = $sources | Where-Object { $_.NeedsBuild }
    $skipped = $sources | Where-Object { -not $_.NeedsBuild }
    
    Write-Host "    Found: $($sources.Count) files"
    Write-Host "    Build: $($toBuild.Count) files"
    Write-Host "    Skip:  $($skipped.Count) files (up to date)`n"
    
    if ($toBuild.Count -eq 0) {
        Write-Host "[*] Nothing to build. Target is up to date." -ForegroundColor Green
        return
    }
    
    # Compile phase
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    # Build MASM files first (parallel)
    $asmFiles = $toBuild | Where-Object { $_.Type -eq "ASM" }
    $cppFiles = $toBuild | Where-Object { $_.Type -eq "CPP" }
    
    if ($asmFiles) {
        Write-Host "=== MASM64 Compilation ===" -ForegroundColor Yellow
        $asmFiles | ForEach-Object { Invoke-MasmCompile $_ }
    }
    
    if ($cppFiles) {
        Write-Host "`n=== C++ Compilation ===" -ForegroundColor Cyan
        # Parallel C++ compilation using ForEach-Object -Parallel if PS7+
        if ($PSVersionTable.PSVersion.Major -ge 7) {
            $cppFiles | ForEach-Object -Parallel {
                # Note: Requires script scope variables to be passed explicitly in PS7
                Invoke-CppCompile $_
            } -ThrottleLimit $env:NUMBER_OF_PROCESSORS
        }
        else {
            $cppFiles | ForEach-Object { Invoke-CppCompile $_ }
        }
    }
    
    # Link phase (only if compiles succeeded)
    if ($script:ErrorCount -eq 0) {
        Invoke-Link $sources
    }
    else {
        Write-Host "`n[!] Skipping link due to $($script:ErrorCount) error(s)" -ForegroundColor Red
    }
    
    $stopwatch.Stop()
    Write-Host "`nBuild completed in $($stopwatch.Elapsed.TotalSeconds.ToString('F2'))s" -ForegroundColor White
    
    # Summary
    if ($script:ErrorCount -eq 0) {
        Write-Host "STATUS: SUCCESS" -ForegroundColor Green -BackgroundColor Black
        exit 0
    }
    else {
        Write-Host "STATUS: FAILED ($($script:ErrorCount) errors)" -ForegroundColor Red -BackgroundColor Black
        exit 1
    }
}

# =============================================================================
# 6. ENTRY POINT
# =============================================================================

try {
    Start-Build
}
catch {
    Write-Host "`n[FATAL] $_" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor DarkGray
    exit 1
}
