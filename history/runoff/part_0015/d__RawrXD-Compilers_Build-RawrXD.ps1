# RawrXD Unified Build System
# Builds all core components: assembler, encoder, hot-patch engine
# Zero external dependencies - pure MASM64

param(
    [switch]$Clean,
    [switch]$Rebuild,
    [switch]$Verbose
)

$ErrorActionPreference = "Continue"

# Paths
$CompilerDir = "D:\RawrXD-Compilers"
$ML64 = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
$LINK = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
$OutputDir = "$CompilerDir\bin"

# Build components
$Components = @(
    @{
        Name = "MASM/NASM Universal Assembler"
        Source = "$CompilerDir\masm_nasm_universal.asm"
        Object = "$CompilerDir\masm_nasm_universal.obj"
        Output = "$OutputDir\rawrxd_asm.exe"
        ML64Flags = "/c /nologo /Zi"
        LinkFlags = "/SUBSYSTEM:CONSOLE /ENTRY:main kernel32.lib user32.lib"
        Description = "Self-hosting assembler with token-level macro substitution"
    },
    @{
        Name = "x64 Instruction Encoder"
        Source = "$CompilerDir\x64_encoder_corrected.asm"
        Object = "$CompilerDir\x64_encoder_corrected.obj"
        Output = "$OutputDir\x64_encoder.lib"
        ML64Flags = "/c /nologo /Zi"
        LinkFlags = "/LIB"
        Description = "Pure assembly x64 instruction encoder (REX/ModRM/SIB)"
    },
    @{
        Name = "RoslynBox Hot-Patch Engine"
        Source = "$CompilerDir\RoslynBox.asm"
        Object = "$CompilerDir\RoslynBox.obj"
        Output = "$OutputDir\roslynbox.exe"
        ML64Flags = "/c /nologo /Zi"
        LinkFlags = "/SUBSYSTEM:CONSOLE /ENTRY:RoslynBox_Main kernel32.lib"
        Description = "Native C# compiler/IL patcher for IDE hot-reload"
    }
)

# Colors for output
function Write-BuildStep {
    param([string]$Message, [string]$Color = "Cyan")
    Write-Host "`n$Message" -ForegroundColor $Color
}

function Write-BuildSuccess {
    param([string]$Message)
    Write-Host "✓ $Message" -ForegroundColor Green
}

function Write-BuildError {
    param([string]$Message)
    Write-Host "✗ $Message" -ForegroundColor Red
}

function Write-BuildWarning {
    param([string]$Message)
    Write-Host "⚠ $Message" -ForegroundColor Yellow
}

# Clean function
function Clean-Build {
    Write-BuildStep "Cleaning build artifacts..."
    
    foreach ($comp in $Components) {
        if (Test-Path $comp.Object) {
            Remove-Item $comp.Object -Force
            Write-Host "  Removed: $($comp.Object)"
        }
    }
    
    if (Test-Path $OutputDir) {
        Remove-Item "$OutputDir\*" -Force -ErrorAction SilentlyContinue
        Write-Host "  Cleaned: $OutputDir"
    }
    
    Write-BuildSuccess "Clean complete"
}

# Compile function
function Compile-Component {
    param($Component)
    
    Write-BuildStep "Building: $($Component.Name)"
    Write-Host "  $($Component.Description)" -ForegroundColor Gray
    
    # Check source exists
    if (!(Test-Path $Component.Source)) {
        Write-BuildError "Source not found: $($Component.Source)"
        return $false
    }
    
    # Assemble
    $ml64Args = "$($Component.ML64Flags) `"$($Component.Source)`""
    Write-Host "  ml64 $ml64Args" -ForegroundColor DarkGray
    
    Push-Location $CompilerDir
    $output = & $ML64 $Component.ML64Flags $Component.Source 2>&1
    $exitCode = $LASTEXITCODE
    Pop-Location
    
    if ($Verbose) {
        $output | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
    }
    
    if ($exitCode -ne 0) {
        Write-BuildError "Assembly failed with exit code $exitCode"
        $output | Where-Object { $_ -match "error" } | ForEach-Object {
            Write-Host "    $_" -ForegroundColor Red
        }
        return $false
    }
    
    # Check object file
    if (!(Test-Path $Component.Object)) {
        Write-BuildError "Object file not created: $($Component.Object)"
        return $false
    }
    
    $objSize = (Get-Item $Component.Object).Length
    Write-Host "  Generated: $objSize bytes" -ForegroundColor Gray
    
    Write-BuildSuccess "Assembly complete: $($Component.Name)"
    return $true
}

# Link function
function Link-Component {
    param($Component)
    
    # Create output directory
    if (!(Test-Path $OutputDir)) {
        New-Item -ItemType Directory -Path $OutputDir | Out-Null
    }
    
    # Determine link mode
    if ($Component.LinkFlags -match "/LIB") {
        Write-Host "  Creating static library..." -ForegroundColor Gray
        $linkArgs = "/LIB /OUT:`"$($Component.Output)`" `"$($Component.Object)`""
    } else {
        Write-Host "  Linking executable..." -ForegroundColor Gray
        $linkArgs = "$($Component.LinkFlags) /OUT:`"$($Component.Output)`" `"$($Component.Object)`""
    }
    
    Write-Host "  link $linkArgs" -ForegroundColor DarkGray
    
    $output = & $LINK $linkArgs.Split(' ') 2>&1
    $exitCode = $LASTEXITCODE
    
    if ($Verbose) {
        $output | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
    }
    
    if ($exitCode -ne 0) {
        Write-BuildError "Linking failed with exit code $exitCode"
        $output | Where-Object { $_ -match "error" } | ForEach-Object {
            Write-Host "    $_" -ForegroundColor Red
        }
        return $false
    }
    
    if (!(Test-Path $Component.Output)) {
        Write-BuildError "Output file not created: $($Component.Output)"
        return $false
    }
    
    $outSize = (Get-Item $Component.Output).Length
    Write-BuildSuccess "Linked: $($Component.Output) ($outSize bytes)"
    return $true
}

# Main build sequence
function Build-All {
    Write-BuildStep "═══════════════════════════════════════════════════════════" "Magenta"
    Write-BuildStep "  RawrXD Unified Build System" "Magenta"
    Write-BuildStep "  Pure MASM64 | Zero Dependencies" "Magenta"
    Write-BuildStep "═══════════════════════════════════════════════════════════" "Magenta"
    
    $buildStart = Get-Date
    $successCount = 0
    $failCount = 0
    
    foreach ($comp in $Components) {
        $compileSuccess = Compile-Component $comp
        
        if ($compileSuccess) {
            $linkSuccess = Link-Component $comp
            
            if ($linkSuccess) {
                $successCount++
            } else {
                $failCount++
            }
        } else {
            $failCount++
        }
    }
    
    $buildEnd = Get-Date
    $duration = ($buildEnd - $buildStart).TotalSeconds
    
    Write-BuildStep "═══════════════════════════════════════════════════════════" "Magenta"
    Write-Host "`nBuild Summary:" -ForegroundColor White
    Write-Host "  Success: $successCount / $($Components.Count)" -ForegroundColor $(if ($successCount -eq $Components.Count) { "Green" } else { "Yellow" })
    Write-Host "  Failed:  $failCount / $($Components.Count)" -ForegroundColor $(if ($failCount -eq 0) { "Green" } else { "Red" })
    Write-Host "  Time:    $([Math]::Round($duration, 2)) seconds" -ForegroundColor Gray
    Write-BuildStep "═══════════════════════════════════════════════════════════" "Magenta"
    
    if ($failCount -eq 0) {
        Write-BuildSuccess "`nAll components built successfully!"
        Write-Host "`nOutput directory: $OutputDir" -ForegroundColor Cyan
        
        # List outputs
        Get-ChildItem $OutputDir | ForEach-Object {
            Write-Host "  $($_.Name) - $($_.Length) bytes" -ForegroundColor Gray
        }
        
        return $true
    } else {
        Write-BuildError "`nBuild completed with $failCount error(s)"
        return $false
    }
}

# Entry point
if ($Clean -or $Rebuild) {
    Clean-Build
}

if (!$Clean) {
    $success = Build-All
    exit $(if ($success) { 0 } else { 1 })
}
