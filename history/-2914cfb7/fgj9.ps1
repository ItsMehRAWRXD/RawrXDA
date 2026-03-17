# ============================================================================
# File: D:\lazy init ide\scripts\Build-PowerShellBridge.ps1
# Purpose: Multi-toolchain build system for RawrXD Pattern Bridge
# ============================================================================

[CmdletBinding()]
param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('PowerShell', 'MASM', 'NASM', 'GCC', 'Custom', 'Auto')]
    [string]$Toolchain = 'Auto',
    
    [Parameter(Mandatory=$false)]
    [ValidateSet('Debug', 'Release', 'Optimized')]
    [string]$Configuration = 'Optimized',
    
    [Parameter(Mandatory=$false)]
    [switch]$Install,
    
    [Parameter(Mandatory=$false)]
    [switch]$Force,
    
    [Parameter(Mandatory=$false)]
    [string]$ConfigPath = "D:\lazy init ide\config\BuildToolchains.psd1"
)

$ErrorActionPreference = 'Stop'

# ============================================================================
# CONFIGURATION LOADER
# ============================================================================

function Import-BuildConfig {
    param([string]$Path)
    
    if (-not (Test-Path $Path)) {
        Write-Warning "Config not found: $Path (using defaults)"
        return $null
    }
    
    try {
        return Import-PowerShellDataFile -Path $Path
    } catch {
        Write-Error "Failed to load config: $_"
        return $null
    }
}

$config = Import-BuildConfig -Path $ConfigPath
if (-not $config) {
    Write-Error "Build configuration required. Create $ConfigPath first."
    exit 1
}

# ============================================================================
# TOOLCHAIN AUTO-DETECTION
# ============================================================================

function Test-ToolchainAvailable {
    param(
        [string]$ToolchainName,
        [hashtable]$ToolchainConfig
    )
    
    $isEnabled = $ToolchainConfig.Enabled -eq 'true' -or $ToolchainConfig.Enabled -eq $true
    if (-not $isEnabled -and -not $Force) {
        Write-Verbose "Toolchain '$ToolchainName' is disabled in config"
        return $false
    }
    
    foreach ($tool in $ToolchainConfig.RequiredTools) {
        $found = Get-Command $tool -ErrorAction SilentlyContinue
        if (-not $found) {
            # Try explicit path
            if ($ToolchainConfig.CompilerPath) {
                $paths = Resolve-Path $ToolchainConfig.CompilerPath -ErrorAction SilentlyContinue
                if (-not $paths) {
                    Write-Verbose "Tool not found: $tool (checked: $($ToolchainConfig.CompilerPath))"
                    return $false
                }
            } else {
                Write-Verbose "Tool not found: $tool"
                return $false
            }
        }
    }
    
    return $true
}

function Select-Toolchain {
    param([string]$Requested)
    
    if ($Requested -ne 'Auto') {
        $tc = $config.Toolchains[$Requested]
        if (Test-ToolchainAvailable -ToolchainName $Requested -ToolchainConfig $tc) {
            Write-Host "[Toolchain] Selected: $Requested" -ForegroundColor Cyan
            Write-Host "  Description: $($tc.Description)" -ForegroundColor Gray
            Write-Host "  Performance: $($tc.Performance)" -ForegroundColor Gray
            return $Requested
        } else {
            Write-Warning "Requested toolchain '$Requested' not available"
            if (-not $Force) {
                Write-Host "[Toolchain] Trying fallback chain..." -ForegroundColor Yellow
                $Requested = 'Auto'
            } else {
                Write-Error "Toolchain '$Requested' required but not available"
                exit 1
            }
        }
    }
    
    if ($Requested -eq 'Auto') {
        foreach ($tc in $config.FallbackChain) {
            $tcConfig = $config.Toolchains[$tc]
            if (Test-ToolchainAvailable -ToolchainName $tc -ToolchainConfig $tcConfig) {
                Write-Host "[Toolchain] Auto-selected: $tc" -ForegroundColor Cyan
                Write-Host "  Description: $($tcConfig.Description)" -ForegroundColor Gray
                Write-Host "  Performance: $($tcConfig.Performance)" -ForegroundColor Gray
                return $tc
            }
        }
        
        Write-Error "No available toolchain found. Install one or enable PowerShell backend."
        exit 1
    }
}

$selectedToolchain = Select-Toolchain -Requested $Toolchain
$toolchainConfig = $config.Toolchains[$selectedToolchain]

# ============================================================================
# BUILD BACKENDS
# ============================================================================

function Build-PowerShellBackend {
    param([hashtable]$Config)
    
    Write-Host "[Build] Using PowerShell/C# backend..." -ForegroundColor Cyan
    
    # Load C# source
    $csharpSource = Get-Content "D:\lazy init ide\src\RawrXD_PatternEngine.cs" -Raw -ErrorAction Stop
    
    # Compilation parameters
    $compileParams = @{
        TypeDefinition = $csharpSource
        Language = 'CSharp'
        WarningAction = 'SilentlyContinue'
    }
    
    if ($Configuration -eq 'Optimized') {
        $compilerParams = New-Object System.CodeDom.Compiler.CompilerParameters
        $compilerParams.CompilerOptions = '/optimize+ /unsafe'
        $compilerParams.GenerateInMemory = $true
        $compileParams['CompilerParameters'] = $compilerParams
    }
    
    try {
        Add-Type @compileParams -ErrorAction Stop
        Write-Host "[Build] C# compilation successful" -ForegroundColor Green
        return $true
    } catch {
        Write-Error "C# compilation failed: $_"
        return $false
    }
}

function Build-MASMBackend {
    param([hashtable]$Config)
    
    Write-Host "[Build] Using MASM backend..." -ForegroundColor Cyan
    
    # Resolve ml64.exe path
    $ml64Path = Resolve-Path $Config.CompilerPath -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Path
    $linkPath = Resolve-Path $Config.LinkerPath -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Path
    
    if (-not $ml64Path -or -not $linkPath) {
        Write-Error "MASM tools not found. Check paths in config."
        return $false
    }
    
    $asmSource = "D:\lazy init ide\src\RawrXD_PatternEngine.asm"
    $objFile = Join-Path $config.Output.IntermediatePath "RawrXD_PatternEngine.obj"
    $dllFile = Join-Path $config.Output.BinPath "RawrXD_PatternEngine.dll"
    
    # Assemble
    $asmArgs = @("/c", "/Cp", "/Cx", "/Fo$objFile")
    if ($Config.CompilerOptions.EnableAVX512) { $asmArgs += "/arch:AVX512" }
    $asmArgs += $asmSource
    
    Write-Verbose "Assembling: $ml64Path $($asmArgs -join ' ')"
    & $ml64Path $asmArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "MASM assembly failed (exit code: $LASTEXITCODE)"
        return $false
    }
    
    # Link
    $linkArgs = @("/DLL", "/OUT:$dllFile", "/MACHINE:X64", $objFile)
    Write-Verbose "Linking: $linkPath $($linkArgs -join ' ')"
    & $linkPath $linkArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Linking failed (exit code: $LASTEXITCODE)"
        return $false
    }
    
    Write-Host "[Build] MASM compilation successful: $dllFile" -ForegroundColor Green
    return $true
}

function Build-NASMBackend {
    param([hashtable]$Config)
    
    Write-Host "[Build] Using NASM backend..." -ForegroundColor Cyan
    
    $nasmPath = $Config.CompilerPath
    $linkPath = Resolve-Path $Config.LinkerPath -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Path
    
    $asmSource = "D:\lazy init ide\src\RawrXD_PatternEngine_nasm.asm"
    $objFile = Join-Path $config.Output.IntermediatePath "RawrXD_PatternEngine.obj"
    $dllFile = Join-Path $config.Output.BinPath "RawrXD_PatternEngine.dll"
    
    # Assemble
    $nasmArgs = @("-f", $Config.CompilerOptions.Format, "-o", $objFile, $asmSource)
    Write-Verbose "Assembling: $nasmPath $($nasmArgs -join ' ')"
    & $nasmPath $nasmArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "NASM assembly failed"
        return $false
    }
    
    # Link
    $linkArgs = @("/DLL", "/OUT:$dllFile", "/MACHINE:X64", $objFile)
    & $linkPath $linkArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Linking failed"
        return $false
    }
    
    Write-Host "[Build] NASM compilation successful: $dllFile" -ForegroundColor Green
    return $true
}

function Build-GCCBackend {
    param([hashtable]$Config)
    
    Write-Host "[Build] Using GCC backend..." -ForegroundColor Cyan
    
    $gccPath = $Config.CompilerPath
    $cppSource = "D:\lazy init ide\src\RawrXD_PatternEngine.cpp"
    $objFile = Join-Path $config.Output.IntermediatePath "RawrXD_PatternEngine.o"
    $dllFile = Join-Path $config.Output.BinPath "RawrXD_PatternEngine.dll"
    
    # Compile
    $gccArgs = @("-c", $cppSource, "-o", $objFile, "-std=c++17", "-march=native")
    if ($Config.CompilerOptions.OptimizationLevel) {
        $gccArgs += "-$($Config.CompilerOptions.OptimizationLevel)"
    }
    if ($Config.CompilerOptions.EnableAVX512) {
        $gccArgs += "-mavx512f"
    }
    
    Write-Verbose "Compiling: $gccPath $($gccArgs -join ' ')"
    & $gccPath $gccArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "GCC compilation failed"
        return $false
    }
    
    # Link
    $linkArgs = @("-shared", "-o", $dllFile, $objFile)
    if ($Config.CompilerOptions.StaticLink) {
        $linkArgs += "-static-libgcc", "-static-libstdc++"
    }
    
    & $Config.LinkerPath $linkArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "GCC linking failed"
        return $false
    }
    
    Write-Host "[Build] GCC compilation successful: $dllFile" -ForegroundColor Green
    return $true
}

function Build-CustomBackend {
    param([hashtable]$Config)
    
    Write-Host "[Build] Using custom toolchain..." -ForegroundColor Cyan
    
    if (-not $Config.CompilerPath -or -not $Config.CompilerOptions.CommandTemplate) {
        Write-Error "Custom toolchain not configured. Set CompilerPath and CommandTemplate in config."
        return $false
    }
    
    # User provides template like: "{compiler} {inputFile} {options} -o {outputFile}"
    $template = $Config.CompilerOptions.CommandTemplate
    $inputFile = "D:\lazy init ide\src\RawrXD_PatternEngine_custom.src"
    $outputFile = Join-Path $config.Output.BinPath "RawrXD_PatternEngine.dll"
    $options = $Config.CompilerOptions.CustomFlags -join ' '
    
    $command = $template -replace '{compiler}', $Config.CompilerPath `
                         -replace '{inputFile}', $inputFile `
                         -replace '{options}', $options `
                         -replace '{outputFile}', $outputFile
    
    Write-Host "[Build] Executing custom command: $command" -ForegroundColor Yellow
    Invoke-Expression $command
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Custom toolchain build failed"
        return $false
    }
    
    Write-Host "[Build] Custom build successful: $outputFile" -ForegroundColor Green
    return $true
}

# ============================================================================
# BUILD ORCHESTRATION
# ============================================================================

Write-Host "`n=== RawrXD Pattern Bridge Build ===" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration" -ForegroundColor White
Write-Host "Toolchain: $selectedToolchain" -ForegroundColor White
Write-Host ""

# Create output directories
New-Item -ItemType Directory -Path $config.Output.BinPath -Force | Out-Null
New-Item -ItemType Directory -Path $config.Output.IntermediatePath -Force | Out-Null
New-Item -ItemType Directory -Path $config.Output.LogPath -Force | Out-Null

# Execute appropriate build backend
$buildSuccess = switch ($selectedToolchain) {
    'PowerShell' { Build-PowerShellBackend -Config $toolchainConfig }
    'MASM'       { Build-MASMBackend -Config $toolchainConfig }
    'NASM'       { Build-NASMBackend -Config $toolchainConfig }
    'GCC'        { Build-GCCBackend -Config $toolchainConfig }
    'Custom'     { Build-CustomBackend -Config $toolchainConfig }
    default      { Write-Error "Unknown toolchain: $selectedToolchain"; $false }
}

if (-not $buildSuccess) {
    Write-Error "Build failed"
    exit 1
}

# ============================================================================
# MODULE GENERATION
# ============================================================================

Write-Host "[Build] Generating PowerShell module wrapper..." -ForegroundColor Cyan

$modulePath = Join-Path $config.Output.BinPath "$($config.Output.ModuleName).psm1"

$moduleContent = @"
# RawrXD Pattern Bridge - Generated Module
# Toolchain: $selectedToolchain
# Generated: $(Get-Date -Format 'o')
# Configuration: $Configuration

`$script:ToolchainBackend = '$selectedToolchain'

function Initialize-RawrXDPatternEngine {
    [CmdletBinding()]
    param()
    
    Write-Verbose "[RawrXD] Initializing pattern engine (Backend: `$script:ToolchainBackend)"
    
    switch (`$script:ToolchainBackend) {
        'PowerShell' {
            # C# types already loaded via Add-Type
            `$null = [RawrXD.PatternBridge.PatternEngine]
        }
        default {
            # Load native DLL
            `$dllPath = Join-Path `$PSScriptRoot 'RawrXD_PatternEngine.dll'
            if (Test-Path `$dllPath) {
                Add-Type -Path `$dllPath
            } else {
                throw "Backend DLL not found: `$dllPath"
            }
        }
    }
    
    Write-Verbose "[RawrXD] Pattern engine ready"
    return `$true
}

function Invoke-RawrXDClassification {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=`$true)]
        [string]`$Code,
        
        [Parameter(Mandatory=`$false)]
        [string]`$Context = ""
    )
    
    if (`$script:ToolchainBackend -eq 'PowerShell') {
        `$confidence = 0.0
        `$type = [RawrXD.PatternBridge.PatternEngine]::ClassifyPattern(`$Code, `$Context, [ref]`$confidence)
        
        return [PSCustomObject]@{
            Type = [int]`$type
            TypeName = `$type.ToString()
            Confidence = `$confidence
            IsPattern = (`$type -in @(1, 3))  # Template or Learned
            RequiresManualReview = (`$type -eq 2)  # NonPattern
            Backend = `$script:ToolchainBackend
        }
    } else {
        # Call native DLL function
        throw "Native backend invocation not yet implemented"
    }
}

function Get-RawrXDPatternStats {
    [CmdletBinding()]
    param()
    
    if (`$script:ToolchainBackend -eq 'PowerShell') {
        `$stats = [RawrXD.PatternBridge.PatternEngine]::GetStats()
        return [PSCustomObject]@{
            TotalClassifications = `$stats.TotalClassifications
            TemplateMatches = `$stats.TemplateMatches
            NonPatternMatches = `$stats.NonPatternMatches
            LearnedMatches = `$stats.LearnedMatches
            AvgConfidence = `$stats.AvgConfidence
            Backend = `$script:ToolchainBackend
        }
    } else {
        throw "Native backend stats not yet implemented"
    }
}

function Get-RawrXDToolchainInfo {
    [CmdletBinding()]
    param()
    
    return [PSCustomObject]@{
        Backend = `$script:ToolchainBackend
        Configuration = '$Configuration'
        ModulePath = `$PSScriptRoot
        BuildDate = '$(Get-Date -Format 'o')'
    }
}

# Auto-initialize
try {
    Initialize-RawrXDPatternEngine | Out-Null
} catch {
    Write-Warning "[RawrXD] Initialization failed: `$_"
}

Export-ModuleMember -Function @(
    'Initialize-RawrXDPatternEngine',
    'Invoke-RawrXDClassification',
    'Get-RawrXDPatternStats',
    'Get-RawrXDToolchainInfo'
)
"@

$moduleContent | Set-Content -Path $modulePath -Encoding UTF8
Write-Host "[Build] Module created: $modulePath" -ForegroundColor Green

# ============================================================================
# INSTALLATION
# ============================================================================

if ($Install) {
    Write-Host "[Install] Installing module..." -ForegroundColor Cyan
    
    $installDir = "$env:USERPROFILE\Documents\PowerShell\Modules\$($config.Output.ModuleName)"
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
    
    Copy-Item -Path $modulePath -Destination "$installDir\$($config.Output.ModuleName).psm1" -Force
    
    # Copy DLL if exists
    $dllPath = Join-Path $config.Output.BinPath "RawrXD_PatternEngine.dll"
    if (Test-Path $dllPath) {
        Copy-Item -Path $dllPath -Destination $installDir -Force
    }
    
    # Create manifest
    $manifestPath = "$installDir\$($config.Output.ModuleName).psd1"
    @{
        RootModule = "$($config.Output.ModuleName).psm1"
        ModuleVersion = '1.0.0'
        GUID = [Guid]::NewGuid().ToString()
        Author = 'RawrXD'
        Description = "High-performance pattern recognition bridge (Backend: $selectedToolchain)"
        PowerShellVersion = '5.1'
        FunctionsToExport = @('Initialize-RawrXDPatternEngine', 'Invoke-RawrXDClassification', 'Get-RawrXDPatternStats', 'Get-RawrXDToolchainInfo')
        PrivateData = @{
            PSData = @{
                Tags = @('RawrXD', 'PatternRecognition', 'AI', $selectedToolchain)
            }
        }
    } | Export-Clixml -Path $manifestPath
    
    Write-Host "[Install] Installed to: $installDir" -ForegroundColor Green
}

# ============================================================================
# SUMMARY
# ============================================================================

Write-Host "`n=== Build Complete ===" -ForegroundColor Green
Write-Host "Toolchain:     $selectedToolchain" -ForegroundColor White
Write-Host "Configuration: $Configuration" -ForegroundColor White
Write-Host "Output:        $modulePath" -ForegroundColor White
Write-Host "Performance:   $($toolchainConfig.Performance)" -ForegroundColor Gray

if ($Install) {
    Write-Host "`nModule installed. Import with:" -ForegroundColor Cyan
    Write-Host "  Import-Module $($config.Output.ModuleName)" -ForegroundColor White
}

Write-Host "`nTo switch toolchains, edit: $ConfigPath" -ForegroundColor Gray
Write-Host "Or use: .\Build-PowerShellBridge.ps1 -Toolchain <name>" -ForegroundColor Gray
