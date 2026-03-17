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

    # Ensure Windows SDK include/lib paths are complete
    # vcvars sometimes misses um/shared includes on BuildTools installs
    $sdkDir = $env:WindowsSdkDir
    $sdkVer = $env:WindowsSDKVersion
    if (-not $sdkDir) {
        $sdkDir = "C:\Program Files (x86)\Windows Kits\10\"
    }
    if (-not $sdkVer) {
        # Detect SDK version — prefer one with complete ucrt headers
        $sdkVersions = Get-ChildItem "$sdkDir\include" -Directory -ErrorAction SilentlyContinue |
                        Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
                        Sort-Object Name -Descending
        # Pick first SDK that has ucrt dir; fallback to latest
        $sdkVer = ""
        foreach ($sv in $sdkVersions) {
            if (Test-Path "${sdkDir}include\$($sv.Name)\ucrt") {
                $sdkVer = "$($sv.Name)\"
                break
            }
        }
        if (-not $sdkVer -and $sdkVersions) { $sdkVer = "$($sdkVersions[0].Name)\" }
    }
    if ($sdkDir -and $sdkVer) {
        $sdkIncBase = "${sdkDir}include\${sdkVer}"
        $sdkLibBase = "${sdkDir}lib\${sdkVer}"
        foreach ($sub in @("um", "shared", "ucrt", "winrt")) {
            $p = "${sdkIncBase}${sub}"
            if ((Test-Path $p) -and ($env:INCLUDE -notmatch [regex]::Escape($sub))) {
                $env:INCLUDE += ";$p"
                Write-Verbose "[SDK] Added include: $p"
            }
        }
        $umLib = "${sdkLibBase}um\x64"
        if ((Test-Path $umLib) -and ($env:LIB -notmatch "um\\x64")) {
            $env:LIB += ";$umLib"
            Write-Verbose "[SDK] Added lib: $umLib"
        }
    }

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

# Legacy MASM files using MASM32 syntax (INVOKE, .386/.model, \masm32\include\)
# These are incompatible with ml64.exe and must be excluded until ported
$script:MasmExcludeList = @(
    "omega_simple",
    "os_explorer_interceptor",
    "os_explorer_interceptor_simple",
    "os_explorer_interceptor_simple_x86",
    "os_interceptor_cli",
    "os_interceptor_cli_final",
    "os_interceptor_cli_fixed",
    "os_interceptor_cli_robust",
    "os_interceptor_cli_simple",
    "os_interceptor_cli_universal",
    "os_interceptor_cli_universal_v2",
    "os_interceptor_cli_working",
    "rawrxd_compiler_masm64",
    "rawrxd_compiler_test",
    "RawrXD_Final_Integration",
    "RawrXD_GlyphAtlas",
    "RawrXD_InferenceCore",
    "RawrXD_IPC_Bridge",
    "rawrxd_lsp_bridge",
    "rawrxd_neural_core",
    "RawrXD_OmegaDeobfuscator",
    "RawrXD_SIMDClassifier",
    "RawrXD_Titan",
    "RawrXD_Titan_CORE",
    "RawrXD_Titan_Kernel",
    "RawrXD_Titan_MINIMAL",
    "RawrXD_Titan_STANDALONE"
)

function Test-MasmCompatible {
    # Auto-detect files using MASM32, GAS/NASM, or other incompatible syntax
    # Scans first 40 lines for telltale patterns
    param([string]$FilePath)
    try {
        $head = Get-Content $FilePath -TotalCount 40 -ErrorAction Stop
        $joined = $head -join "`n"
        # GAS/NASM .section directive
        if ($joined -match '(?mi)^\s*\.section\s') { return $false }
        # MASM32 directives
        if ($joined -match '(?mi)^\s*\.(386|486|586|686|model|stack)\b') { return $false }
        # INVOKE macro (MASM32 high-level)
        if ($joined -match '(?mi)\bINVOKE\b') { return $false }
        # MASM32/64 include paths
        if ($joined -match '(?i)include.*\\masm32\\') { return $false }
        if ($joined -match '(?i)include.*\\masm64\\include64\\') { return $false }
        if ($joined -match '(?i)include.*masm64rt\.inc') { return $false }
        if ($joined -match '(?i)include.*ksamd64\.inc') { return $false }
        # section keyword at line start (NASM style)
        if ($joined -match '(?mi)^section\s+\.(text|data|bss|rodata)') { return $false }
        return $true
    } catch {
        return $false
    }
}

function Get-UniqueObjName {
    param([string]$FilePath, [string]$BaseDir)
    # Create collision-free obj names using relative path with separators replaced
    $resolvedBase = (Resolve-Path $BaseDir -ErrorAction SilentlyContinue)
    if ($resolvedBase) {
        $rel = $FilePath.Replace($resolvedBase.Path, "").TrimStart("\", "/")
    } else {
        $rel = Split-Path $FilePath -Leaf
    }
    # Replace path separators with underscores to flatten into unique obj name
    $safeName = $rel -replace '[\\\/]', '_'
    $safeName = [System.IO.Path]::ChangeExtension($safeName, ".obj")
    return $safeName
}

function Get-SourceFiles {
    $files = @()
    
    foreach ($dir in $SourceDirs) {
        if (-not (Test-Path $dir)) { continue }
        $resolvedDir = (Resolve-Path $dir).Path
        
        # MASM files (skip legacy exclusions + auto-detect incompatible syntax)
        Get-ChildItem -Path $dir -Recurse -Filter "*.asm" | ForEach-Object {
            if ($script:MasmExcludeList -contains $_.BaseName) {
                Write-Verbose "[SKIP] Excluded: $($_.FullName)"
                return
            }
            if (-not (Test-MasmCompatible $_.FullName)) {
                Write-Verbose "[SKIP] Incompatible syntax: $($_.FullName)"
                return
            }
            $objName = Get-UniqueObjName -FilePath $_.FullName -BaseDir $resolvedDir
            $obj = Join-Path $script:ObjDirAbs $objName
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
            $objName = Get-UniqueObjName -FilePath $_.FullName -BaseDir $resolvedDir
            $obj = Join-Path $script:ObjDirAbs $objName
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
    
    # Windows SDK paths (hardened for partial vcvars)
    $windowsSdkDir = $env:WindowsSdkDir
    $windowsSdkVersion = $env:WindowsSDKVersion

    if (-not $windowsSdkDir) {
        # Probe registry for SDK installations
        $regPaths = @(
            "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\Windows\v10.0",
            "HKLM:\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v10.0"
        )
        foreach ($reg in $regPaths) {
            if (Test-Path $reg) {
                $windowsSdkDir = (Get-ItemProperty $reg -Name "InstallationFolder" -ErrorAction SilentlyContinue).InstallationFolder
                $windowsSdkVersion = (Get-ItemProperty $reg -Name "ProductVersion" -ErrorAction SilentlyContinue).ProductVersion
                if ($windowsSdkDir) { break }
            }
        }
    }

    # Fallback to filesystem probe
    if (-not $windowsSdkDir) {
        $kitRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
        if (Test-Path $kitRoot) {
            $windowsSdkDir = $kitRoot
            $includeDir = Join-Path $kitRoot "Include"
            $allSdks = Get-ChildItem $includeDir -Directory -ErrorAction SilentlyContinue | 
                Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } | 
                Sort-Object Name -Descending
            # Prefer SDK with complete ucrt headers
            $bestSdk = $allSdks | Where-Object { Test-Path (Join-Path $includeDir "$($_.Name)\ucrt") } | Select-Object -First 1
            if (-not $bestSdk) { $bestSdk = $allSdks | Select-Object -First 1 }
            if ($bestSdk) { $windowsSdkVersion = $bestSdk.Name }
        }
    }

    # Apply includes if found
    if ($windowsSdkDir -and $windowsSdkVersion) {
        $sdkInc = Join-Path $windowsSdkDir "Include\$windowsSdkVersion"
        @("um", "shared", "ucrt", "winrt") | ForEach-Object {
            $p = Join-Path $sdkInc $_
            if (Test-Path $p) { $flags += "/I`"$p`"" }
        }
        
        # Store lib paths for link phase
        $sdkLib = Join-Path $windowsSdkDir "Lib\$windowsSdkVersion"
        $script:SdkLibPaths = @()
        @("um\x64", "ucrt\x64", "ucrt_enclave\x64") | ForEach-Object {
            $p = Join-Path $sdkLib $_
            if (Test-Path $p) { $script:SdkLibPaths += "/LIBPATH:`"$p`"" }
        }
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
        "vulkan-1.lib",
        "shlwapi.lib",
        "comctl32.lib"
    )
    
    # Add VC CRT lib path (required for libcmt/msvcrt)
    $vcLibDir = Split-Path $script:Cl.Source -Parent
    $vcLibDir = (Split-Path $vcLibDir -Parent) -replace 'bin\\HostX64', 'lib'
    $vcLibDir = Join-Path $vcLibDir "x64" -ErrorAction SilentlyContinue
    if (-not $vcLibDir -or -not (Test-Path $vcLibDir)) {
        # Attempt direct construction from MSVC tools root
        $msvcRoot = Split-Path (Split-Path (Split-Path $script:Cl.Source -Parent) -Parent) -Parent
        $vcLibDir = Join-Path $msvcRoot "lib\x64"
    }
    $vcLibPaths = @()
    if (Test-Path $vcLibDir) {
        $vcLibPaths += "/LIBPATH:`"$vcLibDir`""
    }

    $linkArgs = $flags + $vcLibPaths + $script:SdkLibPaths + $objFiles + $libs
    
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
        # Sequential C++ compilation (PS7 -Parallel can't resolve script functions)
        $cppFiles | ForEach-Object { Invoke-CppCompile $_ }
    }
    
    # Link phase (link whatever compiled successfully)
    # Collect obj files that actually exist on disk
    $linkableObjs = $sources | Where-Object { $_.Type -ne "HEADER" -and (Test-Path $_.ObjPath) }
    if ($linkableObjs.Count -gt 0) {
        if ($script:ErrorCount -gt 0) {
            Write-Host "`n[!] $($script:ErrorCount) compile error(s) — linking $($linkableObjs.Count) successful objects" -ForegroundColor Yellow
        }
        Invoke-Link $linkableObjs
    }
    else {
        Write-Host "`n[!] No objects compiled successfully — cannot link" -ForegroundColor Red
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
