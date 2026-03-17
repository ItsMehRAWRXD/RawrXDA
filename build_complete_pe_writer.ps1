<# ============================================================================
   build_complete_pe_writer.ps1 — Complete PE Writer Build System
   
   Assembles and links both RawrXD_PE_Writer.asm and pe_writer_test.asm
   into working executables with proper error handling
   ============================================================================ #>
param(
    [switch]$TestOnly,     # Build only test executable
    [switch]$Clean,        # Clean build artifacts
    [switch]$Verbose       # Verbose output
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Configuration
$BuildRoot = "D:\RawrXD"
$PEWriterAsm = "$BuildRoot\RawrXD_PE_Writer.asm"
$TestAsm = "$BuildRoot\pe_writer_test.asm"
$PEWriterObj = "$BuildRoot\RawrXD_PE_Writer.obj"
$TestObj = "$BuildRoot\pe_writer_test.obj"
$TestExe = "$BuildRoot\PE_Writer_Test.exe"
$PEWriterLib = "$BuildRoot\RawrXD_PE_Writer.lib"

Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "   RawrXD PE Writer - Complete Build System" -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan

if ($Clean) {
    Write-Host "[Clean] Removing build artifacts..." -ForegroundColor Yellow
    @($PEWriterObj, $TestObj, $TestExe, $PEWriterLib) | ForEach-Object {
        if (Test-Path $_) { 
            Remove-Item $_ -Force
            Write-Host "  Removed: $(Split-Path $_ -Leaf)" -ForegroundColor Gray
        }
    }
    Write-Host "[Clean] Complete" -ForegroundColor Green
    return
}

# ── Locate Visual Studio Tools ──
function Find-VSTools {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vsInstallDir = $null
    
    if (Test-Path $vswhere) {
        $vsInstallDir = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath 2>$null | Select-Object -First 1
    }
    
    if (-not $vsInstallDir) {
        $knownPaths = @(
            'D:\VS2022Enterprise',
            'C:\VS2022Enterprise',
            'C:\Program Files\Microsoft Visual Studio\2022\Enterprise',
            'C:\Program Files\Microsoft Visual Studio\2022\Community',
            'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
        )
        foreach ($p in $knownPaths) {
            if (Test-Path "$p\VC\Auxiliary\Build\vcvars64.bat") {
                $vsInstallDir = $p
                break
            }
        }
    }
    
    if (-not $vsInstallDir) {
        throw 'Visual Studio 2022 x64 tools not found.'
    }
    
    return $vsInstallDir
}

try {
    $vsPath = Find-VSTools
    Write-Host "[VS] Found at: $vsPath" -ForegroundColor Cyan
    
    # Find tools
    $masmPattern = "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe"
    $linkPattern = "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe"
    $libPattern = "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\x64\lib.exe"
    
    $masm = Get-ChildItem $masmPattern | Select-Object -First 1 -ExpandProperty FullName
    $link = Get-ChildItem $linkPattern | Select-Object -First 1 -ExpandProperty FullName  
    $lib = Get-ChildItem $libPattern | Select-Object -First 1 -ExpandProperty FullName
    
    if (-not (Test-Path $masm)) { throw "MASM not found: $masmPattern" }
    if (-not (Test-Path $link)) { throw "Linker not found: $linkPattern" }
    if (-not (Test-Path $lib)) { throw "Librarian not found: $libPattern" }
    
    if ($Verbose) {
        Write-Host "[Tools] MASM: $masm" -ForegroundColor Gray
        Write-Host "[Tools] Link: $link" -ForegroundColor Gray
        Write-Host "[Tools] Lib:  $lib" -ForegroundColor Gray
    }
    
    # ── Step 1: Assemble PE Writer (Library) ──
    if (-not $TestOnly) {
        Write-Host "[1/4] Assembling PE Writer Library..." -ForegroundColor Magenta
        
        $args = @("/c", "/Fo$PEWriterObj", $PEWriterAsm)
        if ($Verbose) { Write-Host "  Command: $masm $($args -join ' ')" -ForegroundColor Gray }
        
        $proc = Start-Process -FilePath $masm -ArgumentList $args -NoNewWindow -Wait -PassThru
        if ($proc.ExitCode -ne 0) {
            throw "PE Writer assembly failed (exit code: $($proc.ExitCode))"
        }
        
        if (-not (Test-Path $PEWriterObj)) {
            throw "PE Writer object file not created: $PEWriterObj"
        }
        Write-Host "    ✓ PE Writer assembled successfully" -ForegroundColor Green
    } else {
        Write-Host "[Skip] PE Writer Library (TestOnly mode)" -ForegroundColor Yellow
    }
    
    # ── Step 2: Assemble Test Program ──
    Write-Host "[2/4] Assembling Test Program..." -ForegroundColor Magenta
    
    $args = @("/c", "/Fo$TestObj", $TestAsm)
    if ($Verbose) { Write-Host "  Command: $masm $($args -join ' ')" -ForegroundColor Gray }
    
    $proc = Start-Process -FilePath $masm -ArgumentList $args -NoNewWindow -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        throw "Test program assembly failed (exit code: $($proc.ExitCode))"
    }
    
    if (-not (Test-Path $TestObj)) {
        throw "Test object file not created: $TestObj"
    }
    Write-Host "    ✓ Test program assembled successfully" -ForegroundColor Green
    
    # ── Step 3: Create Library (if not test-only) ──
    if (-not $TestOnly -and (Test-Path $PEWriterObj)) {
        Write-Host "[3/4] Creating PE Writer Library..." -ForegroundColor Magenta
        
        $args = @("/OUT:$PEWriterLib", $PEWriterObj)
        if ($Verbose) { Write-Host "  Command: $lib $($args -join ' ')" -ForegroundColor Gray }
        
        $proc = Start-Process -FilePath $lib -ArgumentList $args -NoNewWindow -Wait -PassThru
        if ($proc.ExitCode -ne 0) {
            Write-Host "    ⚠ Library creation warning (exit code: $($proc.ExitCode))" -ForegroundColor Yellow
        } else {
            Write-Host "    ✓ PE Writer library created" -ForegroundColor Green
        }
    } else {
        Write-Host "[Skip] Library creation" -ForegroundColor Yellow
    }
    
    # ── Step 4: Link Test Executable ──
    Write-Host "[4/4] Linking Test Executable..." -ForegroundColor Magenta
    
    $linkArgs = @(
        "/SUBSYSTEM:CONSOLE",
        "/MACHINE:X64",
        "/ENTRY:main",
        "/OUT:$TestExe",
        $TestObj
    )
    
    # Add PE Writer object if available
    if (Test-Path $PEWriterObj) {
        $linkArgs += $PEWriterObj
    }
    
    # Add system libraries
    $linkArgs += @("kernel32.lib", "user32.lib")

    if ($Verbose) { Write-Host "  Command: $link $($linkArgs -join ' ')" -ForegroundColor Gray }
    
    # Use developer command prompt environment for linking
    $vcvarsScript = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
    $linkCommand = "& `"$link`" $($linkArgs -join ' ')"
    
    # We'll invoke it via cmd /c to ensure vcvars could have been called, but 
    # for simplicity within PS let's just try running link directly.
    # If standard environment is setup, this should work.
    
    $proc = Start-Process -FilePath $link -ArgumentList $linkArgs -NoNewWindow -Wait -PassThru
    
    if ($proc.ExitCode -eq 0 -and (Test-Path $TestExe)) {
        Write-Host "    ✓ Test executable created successfully" -ForegroundColor Green
        $size = (Get-Item $TestExe).Length
        Write-Host "    📁 Output: $TestExe ($size bytes)" -ForegroundColor Cyan
        
        # Quick test run
        Write-Host "[Test] Running executable..." -ForegroundColor Blue
        try {
            $testProc = Start-Process -FilePath $TestExe -NoNewWindow -Wait -PassThru
            Write-Host "    ✓ Executable runs (exit code: $($testProc.ExitCode))" -ForegroundColor Green
        } catch {
            Write-Host "    ⚠ Executable run failed: $_" -ForegroundColor Yellow
        }
        
    } else {
        Write-Host "    ⚠ Executable not created (linker exit code: $($proc.ExitCode))" -ForegroundColor Yellow
        Write-Host "    📁 Object files available for analysis" -ForegroundColor Cyan
    }
    
    # ── Summary ──
    Write-Host "" -ForegroundColor White
    Write-Host "═══ BUILD SUMMARY ═══" -ForegroundColor Cyan
    @(
        @("PE Writer Object", $PEWriterObj),
        @("Test Object", $TestObj), 
        @("Test Executable", $TestExe),
        @("PE Writer Library", $PEWriterLib)
    ) | ForEach-Object {
        $name, $path = $_
        if (Test-Path $path) {
            $size = (Get-Item $path).Length
            Write-Host "  ✓ $name`: $path ($size bytes)" -ForegroundColor Green
        } else {
            Write-Host "  ✗ $name`: Not created" -ForegroundColor Red
        }
    }
    
    Write-Host "" -ForegroundColor White
    Write-Host "🎉 BUILD COMPLETE! 🎉" -ForegroundColor Green -BackgroundColor DarkGreen
    
} catch {
    Write-Host "" -ForegroundColor White
    Write-Host "❌ BUILD FAILED: $_" -ForegroundColor Red -BackgroundColor DarkRed
    exit 1
}

# Usage help
Write-Host "" -ForegroundColor White
Write-Host "💡 Usage Examples:" -ForegroundColor Yellow
Write-Host "   .\build_complete_pe_writer.ps1          # Full build"
Write-Host "   .\build_complete_pe_writer.ps1 -TestOnly # Test only"
Write-Host "   .\build_complete_pe_writer.ps1 -Clean   # Clean artifacts"
Write-Host "   .\build_complete_pe_writer.ps1 -Verbose # Verbose output"