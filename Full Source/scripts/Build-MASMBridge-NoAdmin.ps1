# ============================================================================
# File: D:\lazy init ide\scripts\Build-MASMBridge-NoAdmin.ps1
# Purpose: Automated MASM64 compilation (no admin/install)
# Version: 1.0.0 - Non-Admin Build Only
# ============================================================================

#Requires -Version 7.0

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('Debug', 'Release', 'Optimized')]
    [string]$Configuration = 'Release',

    [Parameter(Mandatory=$false)]
    [switch]$Clean,

    [Parameter(Mandatory=$false)]
    [string]$OutputPath = "D:\lazy init ide\bin",

    [Parameter(Mandatory=$false)]
    [string]$AssemblerPath,

    [Parameter(Mandatory=$false)]
    [string]$LinkerPath,

    [Parameter(Mandatory=$false)]
    [string[]]$IncludePaths,

    [Parameter(Mandatory=$false)]
    [string[]]$LibraryPaths,

    [Parameter(Mandatory=$false)]
    [string]$AssemblerArgs = '',

    [Parameter(Mandatory=$false)]
    [string]$LinkerArgs = '',

    [Parameter(Mandatory=$false)]
    [ValidateSet('Auto','VisualStudio','PowerShell','MASM')]
    [string]$Toolchain = 'Auto',

    [Parameter(Mandatory=$false)]
    [switch]$Install,

    [Parameter(Mandatory=$false)]
    [switch]$ForceSources,

    [Parameter(Mandatory=$false)]
    [switch]$RegenerateStubs,

    [Parameter(Mandatory=$false)]
    [ValidateSet('x64','x86')]
    [string]$Architecture = 'x64',

    [Parameter(Mandatory=$false)]
    [switch]$EnableAVX512,

    [Parameter(Mandatory=$false)]
    [string[]]$AssemblerDefines,

    [Parameter(Mandatory=$false)]
    [string[]]$LinkerLibraries,

    [Parameter(Mandatory=$false)]
    [ValidateSet('DLL','WINDOWS','CONSOLE')]
    [string]$LinkerSubsystem = 'DLL',

    [Parameter(Mandatory=$false)]
    [string]$LinkerEntryPoint,

    [Parameter(Mandatory=$false)]
    [string]$LinkerDefFile,

    [Parameter(Mandatory=$false)]
    [string[]]$Exports
)

$ErrorActionPreference = 'Stop'

#region Configuration
$BuildConfig = @{
    Debug = @{
        MASMFlags = '/c /Zi /Zd /W3 /nologo'
        LinkFlags = '/DEBUG:FULL /INCREMENTAL:NO /SUBSYSTEM:WINDOWS /DLL'
        Defines = '/DDEBUG=1 /D_DEBUG=1'
        Optimization = ''
    }
    Release = @{
        MASMFlags = '/c /W3 /nologo'
        LinkFlags = '/RELEASE /INCREMENTAL:NO /SUBSYSTEM:WINDOWS /DLL /OPT:REF /OPT:ICF'
        Defines = '/DNDEBUG=1 /DRELEASE=1'
        Optimization = ''
    }
    Optimized = @{
        MASMFlags = '/c /W3 /nologo'
        LinkFlags = '/RELEASE /INCREMENTAL:NO /SUBSYSTEM:WINDOWS /DLL /OPT:REF /OPT:ICF /LTCG'
        Defines = '/DNDEBUG=1 /DRELEASE=1 /DOPTIMIZED=1'
        Optimization = ''
    }
}

$Tools = @{
    ML64 = $null
    LINK = $null
    LIB = $null
    IncPaths = @()
    LibPaths = @()
}

$ToolchainConfig = @{
    Architecture = 'x64'
    EnableAVX512 = $false
    AssemblerDefines = @()
    LinkerLibraries = @()
    LinkerSubsystem = 'DLL'
    LinkerEntryPoint = $null
    LinkerDefFile = $null
    Exports = @()
}

$PowerShellToolchainDefaults = @{
    AssemblerPath = 'D:\tools\masm64.ps1'
    AssemblerDefines = @('NDEBUG=1', 'RELEASE=1', 'OPTIMIZED=1', 'AVX512=1')
    Architecture = 'x64'
    EnableAVX512 = $true
    LinkerPath = 'D:\tools\link64.ps1'
    LinkerLibraries = @('kernel32.lib','user32.lib','advapi32.lib','psapi.lib','ntdll.lib')
    LinkerSubsystem = 'WINDOWS'
    IncludePaths = @('D:\masm64\include64','D:\sdk\include','D:\lazy init ide\include')
    LibraryPaths = @('D:\sdk\lib\x64','D:\masm64\lib64')
    DefFile = 'RawrXD_PatternBridge.def'
    Exports = @('ClassifyPattern','InitializePatternEngine','ShutdownPatternEngine','GetPatternStats')
}

$SourceFiles = @(
    if ($env:ASM_SOURCE) { $env:ASM_SOURCE } else { 'RawrXD_PatternBridge.asm' }
)
#endregion

#region Tool Discovery
function Initialize-BuildTools {
    Write-Host "[Build] Initializing build tools..." -ForegroundColor Cyan

    $resolvedToolchain = $Toolchain
    if ($resolvedToolchain -eq 'Auto') {
        if ($AssemblerPath -and $AssemblerPath.EndsWith('.ps1')) {
            $resolvedToolchain = 'PowerShell'
        } elseif ($env:RAWRXD_ASSEMBLER -and $env:RAWRXD_ASSEMBLER.EndsWith('.ps1')) {
            $resolvedToolchain = 'PowerShell'
        } else {
            $resolvedToolchain = 'VisualStudio'
        }
    }

    if ($resolvedToolchain -eq 'MASM') {
        $resolvedToolchain = 'VisualStudio'
    }

    if ($resolvedToolchain -eq 'PowerShell') {
        if (-not $AssemblerPath) { $AssemblerPath = $PowerShellToolchainDefaults.AssemblerPath }
        if (-not $LinkerPath) { $LinkerPath = $PowerShellToolchainDefaults.LinkerPath }
        if (-not $IncludePaths -or $IncludePaths.Count -eq 0) { $IncludePaths = $PowerShellToolchainDefaults.IncludePaths }
        if (-not $LibraryPaths -or $LibraryPaths.Count -eq 0) { $LibraryPaths = $PowerShellToolchainDefaults.LibraryPaths }
        if (-not $AssemblerDefines -or $AssemblerDefines.Count -eq 0) { $AssemblerDefines = $PowerShellToolchainDefaults.AssemblerDefines }
        if (-not $LinkerLibraries -or $LinkerLibraries.Count -eq 0) { $LinkerLibraries = $PowerShellToolchainDefaults.LinkerLibraries }
        if (-not $Architecture) { $Architecture = $PowerShellToolchainDefaults.Architecture }
        if (-not $EnableAVX512) { $EnableAVX512 = [bool]$PowerShellToolchainDefaults.EnableAVX512 }
        if (-not $LinkerSubsystem) { $LinkerSubsystem = $PowerShellToolchainDefaults.LinkerSubsystem }
        if (-not $LinkerDefFile) { $LinkerDefFile = $PowerShellToolchainDefaults.DefFile }
        if (-not $Exports -or $Exports.Count -eq 0) { $Exports = $PowerShellToolchainDefaults.Exports }

        if ($Install) {
            $env:RAWRXD_ASSEMBLER = $AssemblerPath
            $env:RAWRXD_LINKER = $LinkerPath
            $env:RAWRXD_INCLUDE_PATHS = ($IncludePaths -join ';')
            $env:RAWRXD_LIB_PATHS = ($LibraryPaths -join ';')
        }
    }

    $ToolchainConfig.Architecture = $Architecture
    $ToolchainConfig.EnableAVX512 = [bool]$EnableAVX512
    if ($AssemblerDefines) { $ToolchainConfig.AssemblerDefines = $AssemblerDefines }
    if ($LinkerLibraries) { $ToolchainConfig.LinkerLibraries = $LinkerLibraries }
    if ($LinkerSubsystem) { $ToolchainConfig.LinkerSubsystem = $LinkerSubsystem }
    if ($LinkerEntryPoint) { $ToolchainConfig.LinkerEntryPoint = $LinkerEntryPoint }
    if ($LinkerDefFile) { $ToolchainConfig.LinkerDefFile = $LinkerDefFile }
    if ($Exports) { $ToolchainConfig.Exports = $Exports }

    $envAssembler = $env:RAWRXD_ASSEMBLER
    $envLinker = $env:RAWRXD_LINKER
    $envIncludePaths = $env:RAWRXD_INCLUDE_PATHS
    $envLibraryPaths = $env:RAWRXD_LIB_PATHS

    $AssemblerPath = if ($AssemblerPath) { $AssemblerPath } elseif ($envAssembler) { $envAssembler } else { $null }
    $LinkerPath = if ($LinkerPath) { $LinkerPath } elseif ($envLinker) { $envLinker } else { $null }

    $IncludePaths = if ($IncludePaths) { $IncludePaths } elseif ($envIncludePaths) { $envIncludePaths -split ';' } else { @() }
    $LibraryPaths = if ($LibraryPaths) { $LibraryPaths } elseif ($envLibraryPaths) { $envLibraryPaths -split ';' } else { @() }

    if ($AssemblerPath -or $LinkerPath) {
        if (-not $AssemblerPath) { throw "Custom toolchain selected but AssemblerPath is missing. Set -AssemblerPath or RAWRXD_ASSEMBLER." }
        if (-not $LinkerPath) { throw "Custom toolchain selected but LinkerPath is missing. Set -LinkerPath or RAWRXD_LINKER." }

        if (-not (Test-Path $AssemblerPath)) { throw "Assembler not found at: $AssemblerPath" }
        if (-not (Test-Path $LinkerPath)) { throw "Linker not found at: $LinkerPath" }

        $Tools.ML64 = $AssemblerPath
        $Tools.LINK = $LinkerPath
        $Tools.IncPaths = $IncludePaths | Where-Object { Test-Path $_ }
        $Tools.LibPaths = $LibraryPaths | Where-Object { Test-Path $_ }

        Write-Host "[Build] Using custom assembler: $($Tools.ML64)" -ForegroundColor Green
        Write-Host "[Build] Using custom linker: $($Tools.LINK)" -ForegroundColor Green
        return
    }

    # Search for Visual Studio installations
    $vsPaths = @(
        "C:\VS2022Enterprise\VC\Tools\MSVC"
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC"
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
    )

    $vsRoot = $null
    foreach ($path in $vsPaths) {
        if (Test-Path $path) {
            $version = Get-ChildItem -Path $path -Directory | Sort-Object Name -Descending | Select-Object -First 1
            if ($version) {
                $vsRoot = Join-Path $path $version.Name
                break
            }
        }
    }

    if (-not $vsRoot) {
        throw "Visual Studio C++ tools not found. Please install Visual Studio with C++ workload."
    }

    $Tools.ML64 = Join-Path $vsRoot 'bin\Hostx64\x64\ml64.exe'
    $Tools.LINK = Join-Path $vsRoot 'bin\Hostx64\x64\link.exe'

    if (-not (Test-Path $Tools.ML64)) {
        throw "ML64 not found at: $($Tools.ML64)"
    }
    if (-not (Test-Path $Tools.LINK)) {
        throw "LINK not found at: $($Tools.LINK)"
    }

    # Setup include paths
    $Tools.IncPaths = @(
        (Join-Path $vsRoot 'include')
    ) | Where-Object { Test-Path $_ }

    # Setup library paths
    $sdkBase = "${env:ProgramFiles(x86)}\Windows Kits\10\Lib"
    $sdkVersion = Get-ChildItem $sdkBase -Directory | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
    
    $Tools.LibPaths = @(
        (Join-Path $vsRoot 'lib\x64')
        "$sdkBase\$sdkVersion\um\x64"
        "$sdkBase\$sdkVersion\ucrt\x64"
    ) | Where-Object { Test-Path $_ }

    Write-Host "[Build] Found ML64: $($Tools.ML64)" -ForegroundColor Green
    Write-Host "[Build] Found LINK: $($Tools.LINK)" -ForegroundColor Green
}

function Invoke-MASMCompile {
    param([string]$SourceFile, [string]$ObjectFile)

    $config = $BuildConfig[$Configuration]
    $includeArgs = ($Tools.IncPaths | ForEach-Object { "/I`"$_`"" }) -join ' '

    $args = @(
        $config.MASMFlags
        $config.Optimization
        $config.Defines
        $includeArgs
        $AssemblerArgs
        "/Fo`"$ObjectFile`""
        "`"$SourceFile`""
    ) -join ' '

    Write-Host "[MASM] Compiling $([System.IO.Path]::GetFileName($SourceFile))..." -ForegroundColor Yellow

    if ([System.IO.Path]::GetExtension($Tools.ML64).ToLowerInvariant() -ne '.ps1') {
        Write-Host "[MASM] $($Tools.ML64) $args" -ForegroundColor DarkGray
    }

    $proc = Invoke-ExternalTool -ToolPath $Tools.ML64 -Arguments $args -SourceFile $SourceFile -ObjectFile $ObjectFile

    if ($proc.ExitCode -ne 0) {
        throw "MASM compilation failed with exit code $($proc.ExitCode)"
    }

    Write-Host "[MASM] Success: $([System.IO.Path]::GetFileName($ObjectFile))" -ForegroundColor Green
}

function Invoke-Link {
    param([array]$ObjectFiles, [string]$OutputFile)

    $config = $BuildConfig[$Configuration]
    $libPaths = ($Tools.LibPaths | ForEach-Object { "/LIBPATH:`"$_`"" }) -join ' '

    $objList = ($ObjectFiles | ForEach-Object { "`"$_`"" }) -join ' '

    $defFile = $ToolchainConfig.LinkerDefFile
    if (-not $defFile) {
        $defFile = Join-Path $PSScriptRoot '..\src\RawrXD_PatternBridge.def'
    }

    $args = @(
        $config.LinkFlags
        "/NOENTRY"
        "/DEF:`"$defFile`""
        $libPaths
        $LinkerArgs
        $objList
        "/OUT:`"$OutputFile`""
    ) -join ' '

    Write-Host "[LINK] Linking $([System.IO.Path]::GetFileName($OutputFile))..." -ForegroundColor Yellow

    if ([System.IO.Path]::GetExtension($Tools.LINK).ToLowerInvariant() -ne '.ps1') {
        Write-Host "[LINK] $($Tools.LINK) $args" -ForegroundColor DarkGray
    }

    $proc = Invoke-ExternalTool -ToolPath $Tools.LINK -Arguments $args -ObjectFiles $ObjectFiles -OutputFile $OutputFile -DefFile $defFile

    if ($proc.ExitCode -ne 0) {
        throw "Link failed with exit code $($proc.ExitCode)"
    }

    Write-Host "[LINK] Success: $OutputFile" -ForegroundColor Green
}
#endregion

#region Source Generation
function Generate-PatternBridge {
    param([string]$Path)

    $content = @"
; ============================================================================
; RawrXD Pattern Recognition Bridge - Minimal Stub
; ============================================================================

.code

; DLL entry point (stdcall convention for Windows)
DllMain PROC hinstDLL:QWORD, fdwReason:DWORD, lpvReserved:QWORD
    mov eax, 1
    ret
DllMain ENDP

ClassifyPattern PROC
    xor eax, eax
    ret
ClassifyPattern ENDP

InitializePatternEngine PROC
    xor eax, eax
    ret
InitializePatternEngine ENDP

ShutdownPatternEngine PROC
    xor eax, eax
    ret
ShutdownPatternEngine ENDP

GetPatternStats PROC
    xor rax, rax
    ret
GetPatternStats ENDP

END
"@

    New-Item -ItemType Directory -Path (Split-Path $Path) -Force | Out-Null
    $content | Set-Content -Path $Path -Encoding ASCII
    Write-Host "[Build] Generated: $Path" -ForegroundColor Green
}
#endregion

#region DEF Generation
function Ensure-DefFile {
    param(
        [string]$DefPath,
        [string[]]$Exports,
        [switch]$Force
    )

    if ((Test-Path $DefPath) -and -not $Force) {
        return
    }

    $exportLines = @()
    for ($i = 0; $i -lt $Exports.Count; $i++) {
        $ordinal = $i + 1
        $exportLines += "    $($Exports[$i]) @$ordinal"
    }
    $content = @(
        '; RawrXD Pattern Bridge Export Definitions'
        '; Auto-generated for PowerShell toolchain'
        ''
        'LIBRARY RawrXD_PatternBridge'
        'EXPORTS'
        $exportLines
        ''
    ) -join "`r`n"

    New-Item -ItemType Directory -Path (Split-Path $DefPath) -Force | Out-Null
    $content | Set-Content -Path $DefPath -Encoding ASCII
    Write-Host "[Build] Generated DEF: $DefPath" -ForegroundColor Green
}
#endregion

#region Tool Invocation
function Invoke-ExternalTool {
    param(
        [string]$ToolPath,
        [string]$Arguments,
        [string]$SourceFile,
        [string]$ObjectFile,
        [array]$ObjectFiles,
        [string]$OutputFile,
        [string]$DefFile
    )

    $extension = [System.IO.Path]::GetExtension($ToolPath).ToLowerInvariant()

    if ($extension -eq '.ps1') {
        if ($SourceFile -and $ObjectFile) {
            $defines = @()
            if ($ToolchainConfig.AssemblerDefines.Count -gt 0) {
                $defines += $ToolchainConfig.AssemblerDefines
            }
            if ($Arguments) {
                $parsed = [regex]::Matches($Arguments, '/D([^\s]+)') | ForEach-Object { $_.Groups[1].Value }
                if ($parsed) { $defines += $parsed }
            }

            $argList = @(
                '-NoProfile',
                '-ExecutionPolicy', 'Bypass',
                '-File', $ToolPath,
                '-InputFile', $SourceFile,
                '-OutputFile', $ObjectFile,
                '-Architecture', $ToolchainConfig.Architecture
            )

            if ($Tools.IncPaths.Count -gt 0) {
                $argList += '-IncludePaths'
                $argList += $Tools.IncPaths
            }

            if ($defines.Count -gt 0) {
                $argList += '-Defines'
                $argList += $defines
            }

            if ($ToolchainConfig.EnableAVX512) {
                $argList += '-EnableAVX512'
            }

            $pwsh = (Get-Command pwsh).Source
            Write-Host "[MASM] $pwsh $($argList -join ' ')" -ForegroundColor DarkGray
            return Start-Process -FilePath $pwsh -ArgumentList $argList -NoNewWindow -Wait -PassThru
        }

        if ($ObjectFiles -and $OutputFile) {
            $libPaths = @()
            if ($Tools.LibPaths.Count -gt 0) { $libPaths += $Tools.LibPaths }
            $libraries = @()
            if ($ToolchainConfig.LinkerLibraries.Count -gt 0) { $libraries += $ToolchainConfig.LinkerLibraries }

            $argList = @(
                '-NoProfile',
                '-ExecutionPolicy', 'Bypass',
                '-File', $ToolPath,
                '-ObjectFiles'
            )
            $argList += $ObjectFiles
            $argList += @('-OutputFile', $OutputFile)

            if ($libPaths.Count -gt 0) {
                $argList += '-LibraryPaths'
                $argList += $libPaths
            }

            if ($libraries.Count -gt 0) {
                $argList += '-Libraries'
                $argList += $libraries
            }

            $argList += @('-Subsystem', $ToolchainConfig.LinkerSubsystem)

            if ($ToolchainConfig.LinkerEntryPoint) {
                $argList += @('-EntryPoint', $ToolchainConfig.LinkerEntryPoint)
            }

            if ($DefFile) {
                $argList += @('-DefFile', $DefFile)
            }

            $pwsh = (Get-Command pwsh).Source
            Write-Host "[LINK] $pwsh $($argList -join ' ')" -ForegroundColor DarkGray
            return Start-Process -FilePath $pwsh -ArgumentList $argList -NoNewWindow -Wait -PassThru
        }

        $pwsh = (Get-Command pwsh).Source
        return Start-Process -FilePath $pwsh -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$ToolPath`" $Arguments" -NoNewWindow -Wait -PassThru
    }

    if ($extension -eq '.cmd' -or $extension -eq '.bat') {
        return Start-Process -FilePath "cmd.exe" -ArgumentList "/c `"$ToolPath`" $Arguments" -NoNewWindow -Wait -PassThru
    }

    return Start-Process -FilePath $ToolPath -ArgumentList $Arguments -NoNewWindow -Wait -PassThru
}
#endregion

#region Main Build Process
function Start-Build {
    if ($Clean) {
        Write-Host "[Build] Cleaning output directory..." -ForegroundColor Yellow
        Remove-Item -Path "$OutputPath\*.obj","$OutputPath\*.dll","$OutputPath\*.pdb" -Force -ErrorAction SilentlyContinue
    }

    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null

    Initialize-BuildTools

    $sourceDir = "$PSScriptRoot\..\src"
    New-Item -ItemType Directory -Path $sourceDir -Force | Out-Null

    $generatorScript = Join-Path $PSScriptRoot 'Generate-PatternSources.ps1'
    $requiredSources = @('RawrXD_PatternBridge.asm','RawrXD_PipeServer.asm','RawrXD_SIMDClassifier.asm')
    $missingSources = $requiredSources | Where-Object { -not (Test-Path (Join-Path $sourceDir $_)) }
    if (($missingSources.Count -gt 0 -or $RegenerateStubs) -and (Test-Path $generatorScript)) {
        $suffix = if ($RegenerateStubs) { 'regenerate stubs' } else { "missing: $($missingSources -join ', ')" }
        Write-Host "[Build] Generating MASM sources ($suffix)..." -ForegroundColor Cyan
        $genArgs = @('-NoProfile','-ExecutionPolicy','Bypass','-File', $generatorScript, '-OutputDir', $sourceDir)
        if ($RegenerateStubs) { $genArgs += '-Force' }
        & (Get-Command pwsh).Source @genArgs | Out-Null
    }

    $defPath = $ToolchainConfig.LinkerDefFile
    if (-not $defPath) {
        $defPath = Join-Path $PSScriptRoot '..\src\RawrXD_PatternBridge.def'
    }
    if (-not [System.IO.Path]::IsPathRooted($defPath)) {
        $defPath = Join-Path $PSScriptRoot "..\src\$defPath"
    }
    $ToolchainConfig.LinkerDefFile = $defPath

    if ($Toolchain -eq 'PowerShell' -or ($Toolchain -eq 'Auto' -and $Tools.ML64 -like '*.ps1') -or $Toolchain -eq 'MASM' -or $Toolchain -eq 'VisualStudio') {
        if ($ToolchainConfig.Exports.Count -gt 0) {
            Ensure-DefFile -DefPath $defPath -Exports $ToolchainConfig.Exports -Force:$ForceSources
        }
    }

    $objectFiles = [System.Collections.Generic.List[string]]::new()

    foreach ($source in $SourceFiles) {
        $sourcePath = Join-Path $sourceDir $source
        if (-not (Test-Path $sourcePath)) {
            Generate-PatternBridge -Path $sourcePath
        }

        $objFile = Join-Path $OutputPath ($source -replace '\.asm$', '.obj')
        Invoke-MASMCompile -SourceFile $sourcePath -ObjectFile $objFile
        $objectFiles.Add($objFile)
    }

    $dllOutput = Join-Path $OutputPath 'RawrXD_PatternBridge.dll'
    Invoke-Link -ObjectFiles $objectFiles -OutputFile $dllOutput

    Write-Host "`n[Build] Build completed successfully!" -ForegroundColor Green
    Write-Host "  Output: $dllOutput" -ForegroundColor White
    if (Test-Path $dllOutput) {
        Write-Host "  Size: $([math]::Round((Get-Item $dllOutput).Length / 1KB, 2)) KB" -ForegroundColor Gray
    }
}
#endregion

# Execute build
try {
    Start-Build
}
catch {
    Write-Error "Build failed: $_"
    exit 1
}
