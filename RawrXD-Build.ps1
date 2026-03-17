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
    [string[]]$SourceDirs = @(".\src", ".\asm", ".\core", ".\src\reverse_engineering\re_modules", ".\src\reverse_engineering\reverser_compiler"),
    [string]$Target = "RawrXD-Win32IDE.exe",
    [ValidateSet("WINDOWS", "CONSOLE")]
    [string]$Subsystem = "WINDOWS",
    [string]$EntryPoint = ""
)

# Error handling: Continue on error for troubleshooting
$ErrorActionPreference = "Continue"

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

    $vsPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
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
    # Sanitize: vcvars on some BuildTools installs sets WindowsSDKVersion to
    # just "\" or empty; detect and clear so the filesystem probe fires
    if ($sdkVer -and -not ($sdkVer -match '\d+\.\d+\.\d+\.\d+')) {
        $sdkVer = $null
    }
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

# Directories whose .asm files are excluded (legacy/reference only; no per-file list needed)
# re_modules + reverser_compiler: built (gpu_dma MASM port + reverser stubs). Rest of reverse_engineering excluded by subdir.
$script:MasmExcludeDirs = @(
    "orchestrator",
    "masm",
    "gpu_masm",
    "gpu",
    "loader",
    "net",
    "deobfuscator",
    "omega_suite",
    "model_reverse",
    "pe_tools",
    "security_toolkit"
)

# Legacy MASM files (MASM32 / NASM / doc-only) in build tree — by base name
# Files under MasmExcludeDirs are skipped by path; list only those in src\, src\asm, src\win32app
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
    "RawrXD_SIMDClassifier",
    "RawrXD_Titan",
    "RawrXD_Titan_CORE",
    "RawrXD_Titan_Kernel",
    "RawrXD_Titan_MINIMAL",
    "RawrXD_Titan_STANDALONE",
    # Reverser NASM/binary variants that are not ml64-compatible
    "reverser_compiler_from_scratch",
    "reverser_compiler_from_scratch_patched",
    "reverser_compiler_patched",
    "Win32IDE_Sidebar_Pure_Minimal",
    "Win32IDE_Sidebar_Pure_Simple",
    "Win32IDE_Sidebar_Pure",
    "mcp_hooks",
    "Win32IDE_Sidebar",
    "Win32IDE_Sidebar_Core"
)

# Directories containing platform-incompatible sources (ggml cross-platform backends)
# These require a full ggml build environment and/or non-x64 headers (ARM NEON, RISC-V, etc.)
$script:CppExcludeDirs = @(
    "Full Source",       # Duplicate source tree - exclude to prevent redefinitions
    "reconstructed",     # Archived source snapshots
    "history",           # Version history directory
    "Ship",              # Old monolithic file
    "ggml-blas",
    "ggml-cann",
    "ggml-cpu",
    "ggml-cuda",
    "ggml-hip",
    "ggml-kompute",
    "ggml-metal",
    "ggml-musa",
    "ggml-rpc",
    "ggml-sycl",
    "ggml-vulkan",
    "ggml-webgpu",
    "ggml-zdnn",
    "ggml-opencl",
    "ggml-hexagon"
)

# Specific C++ translation units to exclude (duplicates, test mains, or conflicts)
$script:CppExcludeFiles = @(
    "IDEDiagnosticAutoHealer.cpp",
    "RawrXD_FileManager_Win32.cpp",
    "simple_test.cpp",
    "auto_feature_registry.cpp",
    # Corrupted / duplicate UI samples
    "MainWindowSimple.cpp",
    "Win32IDE_CircularBeaconIntegration.cpp",
    # Deferred feature surfaces under active reconstruction
    "Win32IDE_ReverseEngineering.cpp",
    "Win32IDE_VoiceAutomation.cpp",
    "test_runner.cpp",
    "OSExplorerInterceptor.cpp",
    "Win32IDE_Sidebar_PathOps.cpp",
    "Win32IDE_AIBackend.cpp",
    "Win32IDE_BackendSwitcher.cpp",
    "Win32IDE_Build.cpp",
    "Win32IDE_CopilotGapPanel.cpp",
    "Win32IDE_LSPClient.cpp",
    "Win32IDE_TaskRunner.cpp",
    "Win32IDE_TranscendencePanel.cpp",
    "FileRegistry_Generated.cpp",
    # Renderer mismatch backlog
    "TransparentRenderer.cpp",
    "VulkanRenderer.cpp"
)

# Pattern-based excludes for advanced subsystems (keep core IDE build focused)
$script:CppExcludeNamePatterns = @(
    "*GapFuzz*",
    "*EncryptMode*",
    "*AgenticMode*",
    "*_new.cpp",
    "*\\src\\ggml*.c",
    "*\\src\\ggml*.cpp"
)

# Ensure critical ASM hotpatch objects are linked first
$script:PreferredAsmOrder = @(
    (Join-Path $PSScriptRoot "src\asm\monolithic\main.asm"),
    (Join-Path $PSScriptRoot "src\asm\monolithic\beacon.asm"),
    (Join-Path $PSScriptRoot "src\asm\monolithic\inference.asm"),
    (Join-Path $PSScriptRoot "src\asm\monolithic\model_loader.asm")
)

# Case-insensitive lookup for duplicate/conflicting C++ TUs.
$script:CppExcludeFileSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($f in $script:CppExcludeFiles) {
    [void]$script:CppExcludeFileSet.Add($f)
}

function Test-MasmCompatible {
    # Auto-detect files using MASM32, GAS/NASM, or other incompatible syntax
    # Scans first 40 lines for telltale patterns
    param([string]$FilePath)
    try {
        # Reject binary/corrupt files (NUL bytes) early.
        $bytes = [System.IO.File]::ReadAllBytes($FilePath)
        $scanLen = [Math]::Min($bytes.Length, 4096)
        for ($i = 0; $i -lt $scanLen; $i++) {
            if ($bytes[$i] -eq 0) { return $false }
        }

        $head = Get-Content $FilePath -TotalCount 40 -ErrorAction Stop
        $joined = $head -join "`n"
        # NASM global directive
        if ($joined -match '(?mi)^\s*global\s+') { Write-Verbose "[DEBUG] Match global: $FilePath"; return $false }
        # GAS/NASM .section directive
        if ($joined -match '(?mi)^\s*\.section\s') { Write-Verbose "[DEBUG] Match .section: $FilePath"; return $false }
        # MASM32 directives
        if ($joined -match '(?mi)^\s*\.(386|486|586|686|model|stack)\b') { Write-Verbose "[DEBUG] Match model: $FilePath"; return $false }
        # INVOKE macro (MASM32 high-level)
        if ($joined -match '(?mi)\bINVOKE\b') { Write-Verbose "[DEBUG] Match INVOKE: $FilePath"; return $false }
        # MASM32/64 include paths
        if ($joined -match '(?i)include.*\\masm32\\') { Write-Verbose "[DEBUG] Match masm32: $FilePath"; return $false }
        if ($joined -match '(?i)include.*\\masm64\\include64\\') { Write-Verbose "[DEBUG] Match masm64: $FilePath"; return $false }
        if ($joined -match '(?i)include.*masm64rt\.inc') { Write-Verbose "[DEBUG] Match masm64rt: $FilePath"; return $false }
        if ($joined -match '(?i)include.*ksamd64\.inc') { Write-Verbose "[DEBUG] Match ksamd64: $FilePath"; return $false }
        # section keyword at line start (NASM style)
        if ($joined -match '(?mi)^section\s+\.(text|data|bss|rodata)') { Write-Verbose "[DEBUG] Match section: $FilePath"; return $false }
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
    
    Write-Host "[DEBUG] SourceDirs: $($SourceDirs -join ', ')" -ForegroundColor Yellow
    foreach ($dir in $SourceDirs) {
        $fullPath = Resolve-Path $dir -ErrorAction SilentlyContinue
        if (-not $fullPath) { 
             Write-Host "[DEBUG] Directory not found: $dir" -ForegroundColor Red
             continue 
        }
        $resolvedDir = $fullPath.Path
        Write-Host "[DEBUG] Scanning directory: $resolvedDir" -ForegroundColor Cyan
        
        # MASM files
        $asmFiles = Get-ChildItem -Path $resolvedDir -Recurse -Filter "*.asm"
        Write-Host "[DEBUG] Found $($asmFiles.Count) ASM potential files in $resolvedDir" -ForegroundColor Gray
        foreach ($entry in $asmFiles) {
             # $_ is often overwritten by inner loops, use $entry
             $_ = $entry
             Write-Verbose "[DEBUG] Processing file: $($_.FullName)"
             $fullFilePath = $_.FullName
            if ($_.FullName -like "*\src\reverse_engineering\reverser_compiler\tests\reverser_test_suite.asm") {
                Write-Verbose "[SKIP] Non-ml64 reverser test suite: $($_.FullName)"
                return
            }
            $underExcludedDir = $false
            foreach ($d in $script:MasmExcludeDirs) {
                if ($_.FullName -like "*\$d\*") { $underExcludedDir = $true; break }
            }
            if ($underExcludedDir) {
                Write-Verbose "[SKIP] Excluded dir: $($_.FullName)"
                return
            }
            if ($script:MasmExcludeList -contains $_.BaseName) {
                Write-Verbose "[SKIP] Excluded: $($_.FullName)"
                return
            }
            Write-Verbose "[CHECKING] Masm Compatibility: $($_.FullName)"
            if (-not (Test-MasmCompatible $_.FullName)) {
                Write-Verbose "[SKIP] Incompatible syntax: $($_.FullName)"
                return
            }
            Write-Verbose "[ADDED] Candidate: $($_.FullName)"
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
        
        # C++ files (skip platform-incompatible directories)
        Get-ChildItem -Path $dir -Recurse -Include "*.cpp","*.cxx" | ForEach-Object {
            $leafName = [System.IO.Path]::GetFileName($_.FullName)
            if ($script:CppExcludeFileSet.Contains($leafName)) {
                Write-Verbose "[SKIP] Excluded file: $($_.FullName)"
                return
            }

            # Check if file is under an excluded directory
            $excluded = $false
            foreach ($exDir in $script:CppExcludeDirs) {
                if ($_.FullName -match "[\\/]$exDir[\\/]") {
                    Write-Verbose "[SKIP] Excluded dir ($exDir): $($_.FullName)"
                    $excluded = $true
                    break
                }
            }
            if ($excluded) { return }

            # Pattern-based excludes (advanced subsystems)
            foreach ($pat in $script:CppExcludeNamePatterns) {
                if ($_.FullName -like $pat) {
                    Write-Verbose "[SKIP] Pattern excluded ($pat): $($_.FullName)"
                    return
                }
            }

                # Skip duplicate or conflicting translation units
                if ($script:CppExcludeFileSet.Contains($_.Name) -or $_.Name -in @(
                    "simple_test.cpp",
                    "IDEDiagnosticAutoHealer.cpp",
                    "RawrXD_FileManager_Win32.cpp"
                )) {
                    Write-Verbose "[SKIP] Duplicate source: $($_.FullName)"
                    return
                }

            # Skip ALL stub files. This build must link real implementations only.
            if (($_.Name -like "link_stubs_*.cpp" -or 
                 $_.Name -like "*_stub.cpp" -or 
                 $_.Name -like "*_stubs.cpp" -or
                 $_.Name -like "*stub*.cpp")) {
                Write-Verbose "[SKIP] Stub file: $($_.FullName)"
                return
            }

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

        # C files (compile sqlite3.c and other C sources)
        Get-ChildItem -Path $dir -Recurse -Include "*.c" | ForEach-Object {
            $leafName = [System.IO.Path]::GetFileName($_.FullName)

            # Check if file is under an excluded directory (shared with C++).
            $excluded = $false
            foreach ($exDir in $script:CppExcludeDirs) {
                if ($_.FullName -match "[\\/]$exDir[\\/]") {
                    Write-Verbose "[SKIP] Excluded dir ($exDir): $($_.FullName)"
                    $excluded = $true
                    break
                }
            }
            if ($excluded) { return }

            foreach ($pat in $script:CppExcludeNamePatterns) {
                if ($_.FullName -like $pat) {
                    Write-Verbose "[SKIP] Pattern excluded ($pat): $($_.FullName)"
                    return
                }
            }

            # Skip stub C files
            if ($_.Name -like "*stub*.c") {
                Write-Verbose "[SKIP] Stub file: $($_.FullName)"
                return
            }

            $objName = Get-UniqueObjName -FilePath $_.FullName -BaseDir $resolvedDir
            $obj = Join-Path $script:ObjDirAbs $objName
            $files += [SourceFile]@{
                Path = $_.FullName
                Type = "C"
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
        if (-not $file.ObjPath) {
            # Skip entries without an object path (e.g., malformed or non-compile items)
            continue
        }

        if (Test-Path $file.ObjPath) {
            $objTime = (Get-Item $file.ObjPath).LastWriteTime
            
            # Rebuild if source newer than obj
            if ($file.LastWrite -gt $objTime) {
                $file.NeedsBuild = $true
            }
            # Rebuild if any header newer than obj
            elseif ($headerLatest -and ($headerLatest -gt $objTime)) {
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
    $flags += "/I", (Join-Path $PSScriptRoot "src")
    $flags += "/I", (Join-Path $PSScriptRoot "include")
    $flags += "/I", (Join-Path $PSScriptRoot "asm")
    
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
    $sharedPdb = Join-Path $script:ObjDirAbs "rawrxd-$Config.pdb"
    $flags = @(
        "/c",                    # Compile only
        "/std:c++20",            # C++20 standard
        "/EHsc",                 # Exception handling
        "/nologo",
        "/MP",                   # Multi-processor compilation
        "/FS",                   # Serialize PDB writes (fixes C1041)
        "/Fd$sharedPdb",         # Explicit per-config compiler PDB
        "/Fo$absObj"
    )
    
    if ($Config -eq "Release") {
        $flags += "/O2"          # Maximize speed
        $flags += "/Ob2"         # Inline any suitable
        $flags += "/Oi"          # Enable intrinsics
        $flags += "/Ot"          # Favor fast code
        $flags += "/GL"          # Whole program optimization
        $flags += "/MD"          # Dynamic Release CRT
        $flags += "/DNDEBUG"
        $flags += "/DRELEASE"
    }
    else {
        $flags += "/Od"          # Disable optimization
        $flags += "/Zi"          # Full debug info
        $flags += "/RTC1"        # Run-time checks
        $flags += "/MDd"         # Dynamic Debug CRT
        $flags += "/DDEBUG"
        $flags += "/D_DEBUG"
    }
    
    # Includes
    $flags += "/I$PSScriptRoot\src"
    $flags += "/I$PSScriptRoot\3rdparty\ggml\include"
    $flags += "/I$PSScriptRoot\include"
    $flags += "/I$PSScriptRoot\3rdparty"
    
    # Windows SDK paths (hardened for partial vcvars)
    $windowsSdkDir = $env:WindowsSdkDir
    $windowsSdkVersion = $env:WindowsSDKVersion
    # Sanitize: vcvars on some BuildTools installs sets version to just "\"
    if ($windowsSdkVersion -and -not ($windowsSdkVersion -match '\d+\.\d+\.\d+\.\d+')) {
        $windowsSdkVersion = $null
    }

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
            if (Test-Path $p) { $flags += "/I$p" }
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
        if ($File.Path -match "response_parser.cpp") {
            Write-Host "FLAGS: $flags"
        }
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

function Invoke-CCompile {
    param([SourceFile]$File)

    $log = "[C] $($File.Path)"
    Write-Host $log -ForegroundColor DarkCyan -NoNewline

    # Resolve obj path to absolute
    $absObj = Join-Path $script:ObjDirAbs (Split-Path $File.ObjPath -Leaf)

    # C flags
    $sharedPdb = Join-Path $script:ObjDirAbs "rawrxd-$Config.pdb"
    $flags = @(
        "/c",
        "/TC",
        "/nologo",
        "/MP",
        "/FS",
        "/Fd$sharedPdb",
        "/Fo$absObj"
    )

    if ($Config -eq "Release") {
        $flags += "/O2"
        $flags += "/MD"
        $flags += "/DNDEBUG"
    }
    else {
        $flags += "/Od"
        $flags += "/Zi"
        $flags += "/RTC1"
        $flags += "/MDd"
        $flags += "/DDEBUG"
        $flags += "/D_DEBUG"
    }

    # Includes
    $flags += "/I$PSScriptRoot\src"
    $flags += "/I$PSScriptRoot\3rdparty\ggml\include"
    $flags += "/I$PSScriptRoot\include"
    $flags += "/I$PSScriptRoot\3rdparty"

    # Windows SDK paths (same probe as C++ compile)
    $cWindowsSdkDir = $env:WindowsSdkDir
    $cWindowsSdkVersion = $env:WindowsSDKVersion
    # Sanitize: vcvars on some BuildTools installs sets version to just "\"
    if ($cWindowsSdkVersion -and -not ($cWindowsSdkVersion -match '\d+\.\d+\.\d+\.\d+')) {
        $cWindowsSdkVersion = $null
    }
    if (-not $cWindowsSdkDir) {
        $cRegPaths = @(
            "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\Windows\v10.0",
            "HKLM:\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v10.0"
        )
        foreach ($reg in $cRegPaths) {
            if (Test-Path $reg) {
                $cWindowsSdkDir = (Get-ItemProperty $reg -Name "InstallationFolder" -ErrorAction SilentlyContinue).InstallationFolder
                $cWindowsSdkVersion = (Get-ItemProperty $reg -Name "ProductVersion" -ErrorAction SilentlyContinue).ProductVersion
                if ($cWindowsSdkDir) { break }
            }
        }
    }
    if (-not $cWindowsSdkDir) {
        $cKitRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
        if (Test-Path $cKitRoot) {
            $cWindowsSdkDir = $cKitRoot
            $cIncDir = Join-Path $cKitRoot "Include"
            $cAllSdks = Get-ChildItem $cIncDir -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
                Sort-Object Name -Descending
            $cBest = $cAllSdks | Where-Object { Test-Path (Join-Path $cIncDir "$($_.Name)\ucrt") } | Select-Object -First 1
            if (-not $cBest) { $cBest = $cAllSdks | Select-Object -First 1 }
            if ($cBest) { $cWindowsSdkVersion = $cBest.Name }
        }
    }
    if ($cWindowsSdkDir -and $cWindowsSdkVersion) {
        $cSdkInc = Join-Path $cWindowsSdkDir "Include\$cWindowsSdkVersion"
        @("um", "shared", "ucrt") | ForEach-Object {
            $p = Join-Path $cSdkInc $_
            if (Test-Path $p) { $flags += "/I$p" }
        }
    }

    # MSVC CRT include path
    $vcToolsDir = $env:VCToolsInstallDir
    if ($vcToolsDir) {
        $vcInc = Join-Path $vcToolsDir "include"
        if (Test-Path $vcInc) { $flags += "/I$vcInc" }
    }

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

    # Hard filter duplicate/conflicting objects at link time too, so stale maps
    # or alternate source discovery paths cannot re-introduce them.
    $objExcludePatterns = @(
        '(?i)[\\/]win32app_IDEDiagnosticAutoHealer\.obj$',
        '(?i)[\\/]win32app_RawrXD_FileManager_Win32\.obj$',
        '(?i)[\\/]win32app_simple_test\.obj$'
    )
    $filteredObjs = $Files | Where-Object { $_.Type -ne "HEADER" } | Where-Object {
        $objPath = $_.ObjPath
        foreach ($pat in $objExcludePatterns) {
            if ($objPath -match $pat) {
                Write-Verbose "[SKIP] Link-excluded obj: $objPath"
                return $false
            }
        }
        return $true
    }
    $preferredAsmMap = @{}
    for ($i = 0; $i -lt $script:PreferredAsmOrder.Count; $i++) {
        $preferredAsmMap[$script:PreferredAsmOrder[$i].ToLowerInvariant()] = $i
    }

    $objFiles = $filteredObjs |
        Sort-Object -Property @(
            @{ Expression = {
                $p = $_.Path.ToLowerInvariant()
                if ($preferredAsmMap.ContainsKey($p)) { 0 }
                elseif ($_.Type -eq "ASM") { 1 }
                else { 2 }
            } },
            @{ Expression = {
                $p = $_.Path.ToLowerInvariant()
                if ($preferredAsmMap.ContainsKey($p)) { $preferredAsmMap[$p] } else { 0 }
            } },
            "Path"
        ) |
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
        $flags += "/NODEFAULTLIB:libcmt"
        $flags += "/NODEFAULTLIB:libcmtd"
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
        "shlwapi.lib",
        "comctl32.lib",
        "legacy_stdio_definitions.lib"
    )

    $sqliteLibCandidates = @(
        (Join-Path $PSScriptRoot "lib\sqlite3.lib"),
        (Join-Path $PSScriptRoot "3rdparty\sqlite3.lib")
    )
    foreach ($candidate in $sqliteLibCandidates) {
        if (Test-Path $candidate) {
            $libs += "`"$candidate`""
            break
        }
    }
    # CRT import libraries for /MD (dynamic CRT)
    # Note: /MD injects msvcrt.lib (or msvcrtd.lib in Debug) via defaultlib pragma.
    # We explicitly add ucrt + vcruntime for compatibility with modern C runtime.
    if ($Config -eq "Debug") {
        # Keep debug CRT family grouped and ordered by dependency.
        $libs += "msvcrtd.lib"
        $libs += "vcruntimed.lib"
        $libs += "libcpmtd.lib"
        $libs += "ucrtd.lib"
        # Fallback imports for mixed-object scenarios.
        $libs += "vcruntime.lib"
        $libs += "ucrt.lib"
    } else {
        $libs += "ucrt.lib"
        $libs += "vcruntime.lib"
        $libs += "msvcrt.lib"
    }
    
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
    $rspFile = Join-Path $OutputDir "$Target-link.rsp"
    $rspContent = $linkArgs -join "`n"
    Set-Content -Path $rspFile -Value $rspContent -Encoding ASCII

    try {
        Write-Host "[*] Invoking link.exe via response file: $rspFile" -ForegroundColor Yellow
        $output = & $script:Link.Source "@$rspFile" 2>&1
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

    # Hard-reset inherited compiler/linker environment knobs so hidden global
    # flags (for example /GL in _CL_) cannot poison this build configuration.
    Remove-Item Env:CL -ErrorAction SilentlyContinue
    Remove-Item Env:_CL_ -ErrorAction SilentlyContinue
    Remove-Item Env:LINK -ErrorAction SilentlyContinue
    if ($Config -eq "Debug") {
        $env:CL = "/MDd"
    } else {
        $env:CL = "/MD"
    }
    
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
    $cFiles = $toBuild | Where-Object { $_.Type -eq "C" }
    $cppFiles = $toBuild | Where-Object { $_.Type -eq "CPP" }
    
    if ($asmFiles) {
        Write-Host "=== MASM64 Compilation ===" -ForegroundColor Yellow
        $asmFiles | ForEach-Object { Invoke-MasmCompile $_ }
    }

    if ($cFiles) {
        Write-Host "`n=== C Compilation ===" -ForegroundColor DarkCyan
        $cFiles | ForEach-Object { Invoke-CCompile $_ }
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
