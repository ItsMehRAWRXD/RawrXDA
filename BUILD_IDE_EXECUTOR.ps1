#requires -Version 7.0
<#
.SYNOPSIS
    RawrXD IDE - REAL BUILD EXECUTOR
    
.DESCRIPTION
    Performs actual compilation and linking using real tools.
    Zero scaffolding, zero mocking.
    
    This script:
    1. Assembles all ASM → OBJ files
    2. Links OBJ files → EXE/DLL
    3. Builds IDE with CMake + MSBuild
    4. Produces real binaries
#>
param(
    [switch]$CleanFirst,
    [switch]$Verbose,
    [switch]$OnlyCompilers,
    [switch]$OnlyIDE
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = 'd:\lazy init ide'
$AsmSource = "$ProjectRoot\itsmehrawrxd-master"
$CompilerOut = "$ProjectRoot\compilers"
$BuildOut = "$ProjectRoot\build"
$DistOut = "$ProjectRoot\dist"

# Tool paths
$MASM = 'C:\masm32\bin\ml64.exe'
$NASM = 'C:\nasm\nasm.exe'
$Link = 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64\link.exe'
$CMake = 'cmake'
$MSBuild = 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe'

# ═══════════════════════════════════════════════════════════════════════════════
# HELPERS
# ═══════════════════════════════════════════════════════════════════════════════

function Log($msg, $level = 'INFO') {
    $colors = @{ INFO = 'Cyan'; OK = 'Green'; FAIL = 'Red'; WARN = 'Yellow'; BUILD = 'Magenta' }
    Write-Host "[$level] $msg" -ForegroundColor $colors[$level]
}

function Ensure-Dir($path) {
    if (-not (Test-Path $path)) {
        New-Item -Type Directory $path -Force | Out-Null
    }
}

function Has-Tool($path) {
    Test-Path $path
}

function Assemble-File {
    param([string]$Source, [string]$Output)
    
    if (-not (Test-Path $Source)) {
        Log "Source not found: $Source" WARN
        return $false
    }
    
    # Detect assembler type from file content
    $content = Get-Content $Source -Raw
    $isMASM = $content -match '\b(EXTERN|PROC|db|dw|dd|dq)\b'
    
    Log "Assembling: $(Split-Path $Source -Leaf)" BUILD
    
    if ($isMASM -and (Has-Tool $MASM)) {
        # Use MASM
        $cmd = "& `"$MASM`" /c /nologo /Fo `"$Output`" `"$Source`""
        Log "  Using MASM64" INFO
    } elseif (Has-Tool $NASM) {
        # Use NASM
        $cmd = "& `"$NASM`" -fwin64 -o `"$Output`" `"$Source`""
        Log "  Using NASM" INFO
    } else {
        Log "  No assembler available" FAIL
        return $false
    }
    
    if ($Verbose) { Log "  $cmd" INFO }
    
    $result = Invoke-Expression $cmd 2>&1
    if ($LASTEXITCODE -ne 0) {
        Log "  Compilation failed: $($result | Select-Object -First 3)" FAIL
        return $false
    }
    
    if (Test-Path $Output) {
        $size = (Get-Item $Output).Length
        Log "  → Generated $(Split-Path $Output -Leaf) ($($size / 1KB)KB)" OK
        return $true
    } else {
        Log "  Output file not created" FAIL
        return $false
    }
}

function Link-Files {
    param(
        [string[]]$Objects,
        [string]$Output,
        [string[]]$Libraries = @()
    )
    
    if (-not (Has-Tool $Link)) {
        Log "Linker not found at: $Link" FAIL
        return $false
    }
    
    Log "Linking: $(Split-Path $Output -Leaf)" BUILD
    
    $objArgs = ($Objects | ForEach-Object { "`"$_`"" }) -join ' '
    $libArgs = if ($Libraries.Count -gt 0) { ($Libraries -join ' ') } else { 'kernel32.lib' }
    
    $cmd = "& `"$Link`" /SUBSYSTEM:CONSOLE /OUT:`"$Output`" $objArgs $libArgs 2>&1"
    
    if ($Verbose) { Log "  $cmd" INFO }
    
    $result = Invoke-Expression $cmd
    if ($LASTEXITCODE -ne 0) {
        Log "  Linking failed" FAIL
        return $false
    }
    
    if (Test-Path $Output) {
        $size = (Get-Item $Output).Length
        Log "  → Generated $(Split-Path $Output -Leaf) ($($size / 1MB)MB)" OK
        return $true
    } else {
        Log "  Executable not created" FAIL
        return $false
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# PHASE 1: COMPILER BUILD
# ═══════════════════════════════════════════════════════════════════════════════

if (-not $OnlyIDE) {
    Log "════════════════════════════════════════════════════════════════" BUILD
    Log "PHASE 1: BUILD LANGUAGE COMPILERS" BUILD
    Log "════════════════════════════════════════════════════════════════" BUILD
    
    if ($CleanFirst) {
        Log "Cleaning compiler output directory..." INFO
        Remove-Item $CompilerOut -Recurse -Force -ErrorAction SilentlyContinue
    }
    
    Ensure-Dir $CompilerOut
    
    # Find all compiler ASM files
    $asmFiles = Get-ChildItem $AsmSource -Filter '*compiler*.asm' -File |
                Where-Object { $_.Name -notlike '*_fixed*' -and $_.Name -notlike '*_patched*' }
    
    Log "Found $($asmFiles.Count) compiler source files" INFO
    
    $compiledCount = 0
    foreach ($asmFile in $asmFiles) {
        $baseName = [IO.Path]::GetFileNameWithoutExtension($asmFile.Name)
        $objFile = Join-Path $CompilerOut "$baseName.obj"
        $exeFile = Join-Path $CompilerOut "$baseName.exe"
        
        # Skip if already compiled and not cleaning
        if ((Test-Path $exeFile) -and -not $CleanFirst) {
            Log "Already built: $baseName" OK
            $compiledCount++
            continue
        }
        
        # Assemble
        if (Assemble-File $asmFile.FullName $objFile) {
            # Link
            if (Link-Files @($objFile) $exeFile) {
                $compiledCount++
            }
        }
    }
    
    Log "Compiler build complete: $compiledCount/$($asmFiles.Count)" OK
    Log "" INFO
}

# ═══════════════════════════════════════════════════════════════════════════════
# PHASE 2: IDE BUILD
# ═══════════════════════════════════════════════════════════════════════════════

if (-not $OnlyCompilers) {
    Log "════════════════════════════════════════════════════════════════" BUILD
    Log "PHASE 2: BUILD IDE (C++/Qt with CMake)" BUILD
    Log "════════════════════════════════════════════════════════════════" BUILD
    
    $cmakeLists = "$ProjectRoot\CMakeLists.txt"
    
    if (-not (Test-Path $cmakeLists)) {
        Log "CMakeLists.txt not found at: $cmakeLists" WARN
        Log "Skipping IDE build" WARN
    } else {
        if ($CleanFirst) {
            Log "Cleaning build directory..." INFO
            Remove-Item $BuildOut -Recurse -Force -ErrorAction SilentlyContinue
        }
        
        Ensure-Dir $BuildOut
        
        # CMake Configure
        Log "Running CMake configure..." BUILD
        Push-Location $BuildOut
        try {
            $configCmd = @(
                $CMake,
                '..',
                '-G "Visual Studio 17 2022"',
                '-DCMAKE_BUILD_TYPE=Release',
                '-DCMAKE_CONFIGURATION_TYPES=Release'
            ) -join ' '
            
            if ($Verbose) { Log "  $configCmd" INFO }
            
            $result = Invoke-Expression $configCmd 2>&1
            if ($LASTEXITCODE -eq 0) {
                Log "CMake configuration successful" OK
            } else {
                Log "CMake configuration failed: $(($result | Select-Object -Last 5) -join ' ')" FAIL
                Pop-Location
                exit 1
            }
            
            # MSBuild Compile
            if (Test-Path $MSBuild) {
                Log "Running MSBuild compilation..." BUILD
                
                $buildCmd = @(
                    "`"$MSBuild`"",
                    'RawrXD.sln',
                    '/p:Configuration=Release',
                    '/p:Platform=x64',
                    '/m:4',
                    '/v:minimal'
                ) -join ' '
                
                if ($Verbose) { Log "  $buildCmd" INFO }
                
                $result = Invoke-Expression $buildCmd 2>&1
                if ($LASTEXITCODE -eq 0) {
                    Log "Build compilation successful" OK
                } else {
                    Log "Build compilation failed" FAIL
                    Log "Last 10 lines:" INFO
                    $result | Select-Object -Last 10 | ForEach-Object { Log "  $_" WARN }
                }
            } else {
                Log "MSBuild not found, trying cmake --build..." WARN
                
                $buildCmd = 'cmake --build . --config Release'
                if ($Verbose) { Log "  $buildCmd" INFO }
                
                $result = Invoke-Expression $buildCmd 2>&1
                if ($LASTEXITCODE -eq 0) {
                    Log "Build completed" OK
                } else {
                    Log "Build failed" FAIL
                }
            }
            
        } finally {
            Pop-Location
        }
    }
    
    Log "" INFO
}

# ═══════════════════════════════════════════════════════════════════════════════
# PHASE 3: VERIFICATION
# ═══════════════════════════════════════════════════════════════════════════════

Log "════════════════════════════════════════════════════════════════" BUILD
Log "PHASE 3: VERIFY BUILD ARTIFACTS" BUILD
Log "════════════════════════════════════════════════════════════════" BUILD

Ensure-Dir $DistOut

# Count compiler executables
$compilerExes = @(Get-ChildItem $CompilerOut -Filter '*.exe' -ErrorAction SilentlyContinue)
if ($compilerExes.Count -gt 0) {
    Log "Compiler binaries: $($compilerExes.Count)" OK
    foreach ($exe in $compilerExes | Select-Object -First 5) {
        $size = (Get-Item $exe).Length / 1MB
        Log "  ✓ $($exe.Name) ($([Math]::Round($size, 2))MB)" OK
    }
    if ($compilerExes.Count -gt 5) {
        Log "  ... and $($compilerExes.Count - 5) more" OK
    }
    
    # Copy to dist
    Copy-Item "$CompilerOut\*.exe" $DistOut -Force -ErrorAction SilentlyContinue
}

# Check IDE build
$ideExe = Get-ChildItem $BuildOut -Filter 'RawrXD.exe' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if ($ideExe) {
    Log "IDE executable found" OK
    Log "  ✓ $($ideExe.FullName)" OK
    Copy-Item $ideExe.FullName $DistOut -Force -ErrorAction SilentlyContinue
}

# Final summary
Log "" INFO
Log "════════════════════════════════════════════════════════════════" BUILD
Log "BUILD COMPLETE" BUILD
Log "════════════════════════════════════════════════════════════════" BUILD

@"
ARTIFACTS:
  Compilers: $CompilerOut
  IDE:       $BuildOut
  Dist:      $DistOut

NEXT STEPS:
  1. IDE: $($ideExe ? $ideExe.FullName : "Not built")
  2. Compilers: $(Get-ChildItem $CompilerOut -Filter '*.exe' -ErrorAction SilentlyContinue | Select-Object -First 1 | ForEach-Object { $_.FullName })
  3. Test: .\TEST_BUILD_OUTPUT.ps1
  4. Deploy: Copy $DistOut to deployment target

TROUBLESHOOTING:
  • Use -Verbose flag for detailed output
  • Use -CleanFirst to rebuild from scratch
  • Use -OnlyCompilers to skip IDE build
  • Use -OnlyIDE to skip compiler build
"@ | ForEach-Object { Log $_ INFO }
