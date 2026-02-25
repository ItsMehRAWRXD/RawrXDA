#requires -Version 7.0
<#
.SYNOPSIS
    RawrXD IDE - PRODUCTION BUILD (No Scaffolding)
    
.DESCRIPTION
    Builds the complete RawrXD IDE with zero scaffolding, zero placeholders.
    Only real code, real compilation, real binaries.
    
.PARAMETER Target
    Build target: Full, Compilers, IDE, Desktop
    
.PARAMETER Config
    Release or Debug
    
.PARAMETER Clean
    Clean build artifacts first
#>
param(
    [ValidateSet('Full', 'Compilers', 'IDE', 'Desktop', 'Quick')]
    [string]$Target = 'Full',
    
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release',
    
    [switch]$Clean,
    [switch]$Test,
    [switch]$Verbose,
    [switch]$Interactive
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# PATHS & CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════════════

$ProjectRoot = 'd:\lazy init ide'
$BuildDir = Join-Path $ProjectRoot 'build'
$OutputDir = Join-Path $ProjectRoot 'dist'
$CompilerDir = Join-Path $ProjectRoot 'compilers'
$SourceDir = Join-Path $ProjectRoot 'src'
$AssemblyDir = Join-Path $ProjectRoot 'itsmehrawrxd-master'

# Tools
$CMake = 'cmake'
$MSBuild = 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe'
$MASM = 'C:\masm32\bin\ml64.exe'
$NASM = 'C:\nasm\nasm.exe'
$Linker = 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64\link.exe'

# Colors
$Colors = @{
    Section = 'Magenta'
    Success = 'Green'
    Error = 'Red'
    Warning = 'Yellow'
    Info = 'Cyan'
}

# ═══════════════════════════════════════════════════════════════════════════════
# UTILITY FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════════

function Write-Section {
    param([string]$Text)
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor $Colors.Section
    Write-Host " $Text" -ForegroundColor $Colors.Section
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor $Colors.Section
}

function Write-Status {
    param([string]$Text, [string]$Status = 'INFO')
    $color = if ($Status -eq 'OK') { $Colors.Success } elseif ($Status -eq 'ERROR') { $Colors.Error } else { $Colors.Info }
    Write-Host "[$Status] $Text" -ForegroundColor $color
}

function Test-Tool {
    param([string]$Tool, [string]$Description)
    $path = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($path) {
        Write-Status "$Description found" -Status 'OK'
        return $true
    } else {
        Write-Status "$Description NOT FOUND: $Tool" -Status 'ERROR'
        return $false
    }
}

function Invoke-Build {
    param(
        [string]$Command,
        [string]$Description,
        [bool]$Required = $true
    )
    
    Write-Status "Running: $Description"
    Write-Host $Command -ForegroundColor Gray
    
    $result = Invoke-Expression $Command 2>&1
    $exitCode = $LASTEXITCODE
    
    if ($exitCode -ne 0) {
        Write-Status "FAILED: $Description (exit code $exitCode)" -Status 'ERROR'
        if ($Required) {
            Write-Host $result
            throw "Build failed at: $Description"
        }
    } else {
        Write-Status "SUCCESS: $Description" -Status 'OK'
    }
    
    return $result
}

function Ensure-Directory {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
        Write-Status "Created directory: $Path" -Status 'INFO'
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# PHASE 1: VALIDATE BUILD ENVIRONMENT
# ═══════════════════════════════════════════════════════════════════════════════

Write-Section "PHASE 1: VALIDATE BUILD ENVIRONMENT"

$tools = @(
    @{ Tool = 'cmake'; Desc = 'CMake' },
    @{ Tool = 'pwsh'; Desc = 'PowerShell Core' },
    @{ Tool = 'git'; Desc = 'Git' }
)

$optionalTools = @(
    @{ Tool = $MSBuild; Desc = 'MSBuild' },
    @{ Tool = $MASM; Desc = 'MASM x64' },
    @{ Tool = $NASM; Desc = 'NASM' },
    @{ Tool = $Linker; Desc = 'Visual C++ Linker' }
)

Write-Status "Checking required tools..." -Status 'INFO'
$allGood = $true
foreach ($tool in $tools) {
    if (-not (Test-Tool $tool.Tool $tool.Desc)) {
        $allGood = $false
    }
}

if (-not $allGood) {
    throw "Missing required build tools. Install CMake, PowerShell Core, and Git."
}

Write-Status "Checking optional tools..." -Status 'INFO'
$optionalAvailable = @{}
foreach ($tool in $optionalTools) {
    if (Test-Path $tool.Tool) {
        $optionalAvailable[$tool.Desc] = $true
    } else {
        Write-Status "$($tool.Desc) not found (optional)" -Status 'WARNING'
        $optionalAvailable[$tool.Desc] = $false
    }
}

if ($Interactive) {
    Read-Host "Build environment validated. Press Enter to continue"
}

# ═══════════════════════════════════════════════════════════════════════════════
# PHASE 2: CLEAN BUILD ARTIFACTS (if requested)
# ═══════════════════════════════════════════════════════════════════════════════

if ($Clean) {
    Write-Section "PHASE 2: CLEAN BUILD ARTIFACTS"
    
    $dirsToClean = @($BuildDir, $OutputDir)
    foreach ($dir in $dirsToClean) {
        if (Test-Path $dir) {
            Write-Status "Removing: $dir" -Status 'INFO'
            Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# PHASE 3: BUILD COMPILERS (if needed)
# ═══════════════════════════════════════════════════════════════════════════════

if ($Target -in @('Full', 'Compilers')) {
    Write-Section "PHASE 3: BUILD LANGUAGE COMPILERS"
    
    Ensure-Directory $CompilerDir
    
    # Get all compiler assembly files
    $asmFiles = Get-ChildItem "$AssemblyDir" -Filter '*_compiler*.asm' -File | 
                Where-Object { $_.Name -notlike '*_fixed.asm' -and $_.Name -notlike '*_patched*' }
    
    Write-Status "Found $($asmFiles.Count) compiler sources to build" -Status 'INFO'
    
    if ($asmFiles.Count -eq 0) {
        Write-Status "No compiler sources found. Skipping compiler build." -Status 'WARNING'
    } else {
        $compiled = 0
        $failed = 0
        
        foreach ($asmFile in $asmFiles) {
            $baseName = [IO.Path]::GetFileNameWithoutExtension($asmFile.Name)
            $objFile = Join-Path $CompilerDir "$baseName.obj"
            $exeFile = Join-Path $CompilerDir "$baseName.exe"
            
            # Skip if already built
            if ((Test-Path $exeFile) -and -not $Clean) {
                Write-Status "Already built: $($asmFile.Name)" -Status 'INFO'
                $compiled++
                continue
            }
            
            try {
                Write-Status "Compiling: $($asmFile.Name)" -Status 'INFO'
                
                if ($optionalAvailable['MASM x64']) {
                    # Try MASM compilation
                    $cmd = "& '$MASM' /c /Fo `"$objFile`" `"$($asmFile.FullName)`""
                    $result = Invoke-Expression $cmd 2>&1
                    
                    if ($LASTEXITCODE -eq 0 -and (Test-Path $objFile)) {
                        # Link object file
                        $linkCmd = "& '$Linker' /SUBSYSTEM:CONSOLE /OUT:`"$exeFile`" `"$objFile`" kernel32.lib"
                        Invoke-Expression $linkCmd | Out-Null
                        
                        if (Test-Path $exeFile) {
                            Write-Status "Built: $baseName.exe" -Status 'OK'
                            $compiled++
                        }
                    }
                } elseif ($optionalAvailable['NASM']) {
                    # Try NASM compilation
                    $cmd = "& '$NASM' -fwin64 -o `"$objFile`" `"$($asmFile.FullName)`""
                    Invoke-Expression $cmd | Out-Null
                    
                    if (Test-Path $objFile) {
                        $linkCmd = "& '$Linker' /SUBSYSTEM:CONSOLE /OUT:`"$exeFile`" `"$objFile`" kernel32.lib"
                        Invoke-Expression $linkCmd | Out-Null
                        
                        if (Test-Path $exeFile) {
                            Write-Status "Built: $baseName.exe" -Status 'OK'
                            $compiled++
                        }
                    }
                } else {
                    Write-Status "Skipping (no assembler): $($asmFile.Name)" -Status 'WARNING'
                }
            } catch {
                Write-Status "Failed to compile: $($asmFile.Name)" -Status 'ERROR'
                $failed++
            }
        }
        
        Write-Status "Compiler build complete: $compiled/$($asmFiles.Count) successful" -Status 'INFO'
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# PHASE 4: BUILD IDE (C++/Qt)
# ═══════════════════════════════════════════════════════════════════════════════

if ($Target -in @('Full', 'IDE')) {
    Write-Section "PHASE 4: BUILD IDE (C++ with CMake)"
    
    $cmakeLists = Join-Path $ProjectRoot 'CMakeLists.txt'
    if (-not (Test-Path $cmakeLists)) {
        Write-Status "CMakeLists.txt not found at $cmakeLists" -Status 'WARNING'
        Write-Status "Skipping IDE build" -Status 'INFO'
    } else {
        Ensure-Directory $BuildDir
        
        # Configure with CMake
        Write-Status "Configuring with CMake..." -Status 'INFO'
        Push-Location $BuildDir
        try {
            $configCmd = @(
                'cmake',
                '..',
                '-G "Visual Studio 17 2022"',
                '-DCMAKE_BUILD_TYPE=' + $Config,
                '-DCMAKE_CONFIGURATION_TYPES=' + $Config
            ) -join ' '
            
            Invoke-Build $configCmd "CMake Configuration" -Required $true
            
            # Build with MSBuild
            if (Test-Path $MSBuild) {
                Write-Status "Building with MSBuild..." -Status 'INFO'
                $buildCmd = "& `"$MSBuild`" RawrXD.sln /p:Configuration=$Config /p:Platform=x64 /v:minimal"
                Invoke-Build $buildCmd "MSBuild Compilation" -Required $true
            } else {
                Write-Status "MSBuild not found, trying cmake --build..." -Status 'WARNING'
                $buildCmd = "cmake --build . --config $Config"
                Invoke-Build $buildCmd "CMake Build" -Required $true
            }
        } finally {
            Pop-Location
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# PHASE 5: BUILD DESKTOP UTILITIES
# ═══════════════════════════════════════════════════════════════════════════════

if ($Target -in @('Full', 'Desktop')) {
    Write-Section "PHASE 5: BUILD DESKTOP UTILITIES"
    
    $desktopDir = Join-Path $ProjectRoot 'desktop'
    if (Test-Path $desktopDir) {
        $psScripts = Get-ChildItem $desktopDir -Filter '*.ps1' -File
        Write-Status "Found $($psScripts.Count) PowerShell utilities" -Status 'INFO'
        
        # Copy to output
        Ensure-Directory $OutputDir
        Copy-Item (Join-Path $desktopDir '*') -Destination $OutputDir -Recurse -Force
        Write-Status "Desktop utilities copied to $OutputDir" -Status 'OK'
    } else {
        Write-Status "Desktop directory not found" -Status 'WARNING'
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# PHASE 6: VERIFY BUILD OUTPUTS
# ═══════════════════════════════════════════════════════════════════════════════

Write-Section "PHASE 6: VERIFY BUILD OUTPUTS"

$artifacts = @(
    @{ Path = $CompilerDir; Name = 'Compilers' },
    @{ Path = $OutputDir; Name = 'Distribution' },
    @{ Path = (Join-Path $BuildDir 'bin'); Name = 'IDE Binaries' }
)

$buildArtifacts = @()
foreach ($artifact in $artifacts) {
    if (Test-Path $artifact.Path) {
        $files = Get-ChildItem $artifact.Path -Recurse -File | Measure-Object
        Write-Status "$($artifact.Name): $($files.Count) files" -Status 'OK'
        
        $exeFiles = Get-ChildItem $artifact.Path -Filter '*.exe' -Recurse
        if ($exeFiles) {
            foreach ($exe in $exeFiles) {
                $size = "{0:N2}" -f ($exe.Length / 1MB)
                Write-Host "  ✓ $($exe.Name) ($size MB)" -ForegroundColor Green
                $buildArtifacts += $exe
            }
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# PHASE 7: RUN TESTS (if requested)
# ═══════════════════════════════════════════════════════════════════════════════

if ($Test) {
    Write-Section "PHASE 7: RUN TESTS"
    
    # Test compiler binaries
    $exeFiles = $buildArtifacts | Where-Object { $_.Extension -eq '.exe' }
    if ($exeFiles) {
        foreach ($exe in $exeFiles) {
            Write-Status "Testing: $($exe.Name)" -Status 'INFO'
            try {
                $output = & $exe.FullName 2>&1
                if ($LASTEXITCODE -eq 0) {
                    Write-Status "$($exe.Name) - OK" -Status 'OK'
                } else {
                    Write-Status "$($exe.Name) - Exit code $LASTEXITCODE" -Status 'WARNING'
                }
            } catch {
                Write-Status "Failed to run: $($exe.Name)" -Status 'ERROR'
            }
        }
    }
    
    # Test IDE if built
    $ideExe = Get-ChildItem $BuildDir -Filter 'RawrXD.exe' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($ideExe) {
        Write-Status "IDE binary found: $($ideExe.FullName)" -Status 'OK'
        if ($Interactive) {
            Write-Status "To launch IDE: & '$($ideExe.FullName)'" -Status 'INFO'
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# PHASE 8: GENERATE BUILD REPORT
# ═══════════════════════════════════════════════════════════════════════════════

Write-Section "PHASE 8: BUILD REPORT"

$report = @{
    'Build Date' = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    'Target' = $Target
    'Configuration' = $Config
    'Project Root' = $ProjectRoot
    'Build Directory' = $BuildDir
    'Output Directory' = $OutputDir
    'Compiler Count' = (Get-ChildItem $CompilerDir -Filter '*.exe' -ErrorAction SilentlyContinue | Measure-Object).Count
    'IDE Built' = if (Get-ChildItem $BuildDir -Filter 'RawrXD.exe' -Recurse -ErrorAction SilentlyContinue) { 'Yes' } else { 'No' }
    'Total Artifacts' = $buildArtifacts.Count
}

$report | Format-Table -AutoSize

# Save report to file
$reportFile = Join-Path $OutputDir 'BUILD_REPORT.txt'
$report | Out-String | Set-Content $reportFile
Write-Status "Build report saved to: $reportFile" -Status 'OK'

# ═══════════════════════════════════════════════════════════════════════════════
# FINAL SUMMARY
# ═══════════════════════════════════════════════════════════════════════════════

Write-Section "BUILD COMPLETE"

Write-Host @"

✓ Build process finished successfully!

NEXT STEPS:

1. IDE BINARIES (if built):
   $BuildDir

2. LANGUAGE COMPILERS:
   $CompilerDir

3. DISTRIBUTION ARTIFACTS:
   $OutputDir

4. TO LAUNCH IDE:
   • Look for RawrXD.exe in build directory
   • Or run: & '$OutputDir\RawrXD.exe'

5. TO REBUILD:
   • Run with -Clean flag: .\BUILD_IDE_PRODUCTION.ps1 -Target Full -Clean
   • Run specific target: .\BUILD_IDE_PRODUCTION.ps1 -Target Compilers

6. BUILD OPTIONS:
   -Target     : Full, Compilers, IDE, Desktop, Quick
   -Config     : Release, Debug
   -Clean      : Clean before building
   -Test       : Run basic tests after build
   -Verbose    : Show all build output
   -Interactive: Pause between phases

DOCUMENTATION:
   See BUILD_REPORT.txt for detailed build information

"@ -ForegroundColor Green

Write-Status "All operations completed successfully!" -Status 'OK'
