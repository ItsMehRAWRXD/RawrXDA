#================================================================================
# BUILD_WEEK4.PS1 - Week 4 Test Suite Build Script
# Automated compilation, linking, and verification
#================================================================================

param(
    [switch]$Rebuild,
    [switch]$Verbose,
    [switch]$RunTests
)

$ErrorActionPreference = "Stop"

#================================================================================
# CONFIGURATION
#================================================================================

$WEEK4_DIR = "D:\rawrxd\src\agentic\week4"
$BUILD_DIR = "D:\rawrxd\build\week4"
$OUTPUT_DIR = "D:\rawrxd\bin"
$TEST_RESULTS_DIR = "D:\rawrxd\test_results"

$ASM_FILE = "$WEEK4_DIR\WEEK4_DELIVERABLE.asm"
$OBJ_FILE = "$BUILD_DIR\WEEK4_DELIVERABLE.obj"
$EXE_FILE = "$OUTPUT_DIR\Week4_TestSuite.exe"

# Assembler configuration
$ML64 = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\ml64.exe"
$LINK = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\link.exe"

$ASM_FLAGS = @(
    "/c"                              # Compile only
    "/O2"                             # Optimize for speed
    "/Zi"                             # Debug info
    "/W3"                             # Warning level 3
    "/nologo"                         # No banner
    "/Fo$OBJ_FILE"                    # Output file
)

$LINK_FLAGS = @(
    "/SUBSYSTEM:CONSOLE"              # Console application
    "/MACHINE:X64"                    # x64 target
    "/NOLOGO"                         # No banner
    "/DEBUG"                          # Debug info
    "/OUT:$EXE_FILE"                  # Output executable
)

# Libraries to link
$LIBS = @(
    "kernel32.lib"
    "user32.lib"
    "msvcrt.lib"
    "ucrt.lib"
    "vcruntime.lib"
)

#================================================================================
# HELPER FUNCTIONS
#================================================================================

function Write-Header {
    param([string]$Message)
    Write-Host "`n╔══════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║ $($Message.PadRight(68)) ║" -ForegroundColor Cyan
    Write-Host "╚══════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "✓ " -ForegroundColor Green -NoNewline
    Write-Host $Message
}

function Write-Error {
    param([string]$Message)
    Write-Host "✗ " -ForegroundColor Red -NoNewline
    Write-Host $Message -ForegroundColor Red
}

function Write-Info {
    param([string]$Message)
    Write-Host "ℹ " -ForegroundColor Yellow -NoNewline
    Write-Host $Message
}

function Test-FileExists {
    param([string]$Path, [string]$Description)
    
    if (Test-Path $Path) {
        $size = (Get-Item $Path).Length
        Write-Success "$Description ($([math]::Round($size/1KB, 2)) KB)"
        return $true
    } else {
        Write-Error "$Description not found: $Path"
        return $false
    }
}

#================================================================================
# BUILD STEPS
#================================================================================

Write-Header "WEEK 4 TEST SUITE BUILD"

# Step 1: Validate environment
Write-Host "`n[1/6] Validating build environment..." -ForegroundColor Cyan

if (-not (Test-FileExists $ML64 "ML64 Assembler")) { exit 1 }
if (-not (Test-FileExists $LINK "Linker")) { exit 1 }
if (-not (Test-FileExists $ASM_FILE "Week 4 Source")) { exit 1 }

# Step 2: Create directories
Write-Host "`n[2/6] Creating build directories..." -ForegroundColor Cyan

@($BUILD_DIR, $OUTPUT_DIR, $TEST_RESULTS_DIR) | ForEach-Object {
    if (-not (Test-Path $_)) {
        New-Item -ItemType Directory -Path $_ -Force | Out-Null
        Write-Success "Created $_"
    } else {
        Write-Info "Directory exists: $_"
    }
}

# Step 3: Clean previous build
if ($Rebuild) {
    Write-Host "`n[3/6] Cleaning previous build..." -ForegroundColor Cyan
    
    if (Test-Path $OBJ_FILE) {
        Remove-Item $OBJ_FILE -Force
        Write-Success "Removed $OBJ_FILE"
    }
    if (Test-Path $EXE_FILE) {
        Remove-Item $EXE_FILE -Force
        Write-Success "Removed $EXE_FILE"
    }
} else {
    Write-Host "`n[3/6] Skipping clean (use -Rebuild to clean)" -ForegroundColor Cyan
}

# Step 4: Assemble source
Write-Host "`n[4/6] Assembling Week 4 test suite..." -ForegroundColor Cyan

$asmArgs = $ASM_FLAGS + $ASM_FILE

if ($Verbose) {
    Write-Info "Command: $ML64 $($asmArgs -join ' ')"
}

$asmProcess = Start-Process -FilePath $ML64 -ArgumentList $asmArgs -WorkingDirectory $WEEK4_DIR -Wait -PassThru -NoNewWindow

if ($asmProcess.ExitCode -ne 0) {
    Write-Error "Assembly failed with exit code $($asmProcess.ExitCode)"
    exit 1
}

if (-not (Test-FileExists $OBJ_FILE "Object file")) { exit 1 }

$objSize = (Get-Item $OBJ_FILE).Length
Write-Success "Assembly complete: $([math]::Round($objSize/1KB, 2)) KB object file"

# Step 5: Link executable
Write-Host "`n[5/6] Linking test executable..." -ForegroundColor Cyan

$linkArgs = $LINK_FLAGS + $OBJ_FILE + $LIBS

if ($Verbose) {
    Write-Info "Command: $LINK $($linkArgs -join ' ')"
}

$linkProcess = Start-Process -FilePath $LINK -ArgumentList $linkArgs -WorkingDirectory $BUILD_DIR -Wait -PassThru -NoNewWindow

if ($linkProcess.ExitCode -ne 0) {
    Write-Error "Linking failed with exit code $($linkProcess.ExitCode)"
    exit 1
}

if (-not (Test-FileExists $EXE_FILE "Test executable")) { exit 1 }

$exeSize = (Get-Item $EXE_FILE).Length
Write-Success "Linking complete: $([math]::Round($exeSize/1KB, 2)) KB executable"

# Step 6: Verify build
Write-Host "`n[6/6] Verifying build artifacts..." -ForegroundColor Cyan

$allGood = $true

# Check object file size (expected: 100-200 KB)
if ($objSize -lt 50KB -or $objSize -gt 500KB) {
    Write-Error "Object file size unexpected: $([math]::Round($objSize/1KB, 2)) KB"
    $allGood = $false
} else {
    Write-Success "Object file size OK"
}

# Check executable size (expected: 50-150 KB)
if ($exeSize -lt 20KB -or $exeSize -gt 300KB) {
    Write-Error "Executable size unexpected: $([math]::Round($exeSize/1KB, 2)) KB"
    $allGood = $false
} else {
    Write-Success "Executable size OK"
}

#================================================================================
# RUN TESTS (OPTIONAL)
#================================================================================

if ($RunTests) {
    Write-Host "`n[OPTIONAL] Running test suite..." -ForegroundColor Cyan
    
    Write-Info "Executing: $EXE_FILE"
    
    $testProcess = Start-Process -FilePath $EXE_FILE -WorkingDirectory $OUTPUT_DIR -Wait -PassThru -NoNewWindow
    
    $testExitCode = $testProcess.ExitCode
    
    if ($testExitCode -eq 0) {
        Write-Success "All tests passed!"
    } elseif ($testExitCode -eq 1) {
        Write-Error "Some tests failed (see test_results/ for details)"
    } elseif ($testExitCode -eq 2) {
        Write-Error "Test framework initialization failed"
    } else {
        Write-Error "Test suite exited with code $testExitCode"
    }
    
    # Show test results if available
    $xmlReport = "$TEST_RESULTS_DIR\test_results.xml"
    $jsonReport = "$TEST_RESULTS_DIR\test_results.json"
    
    if (Test-Path $xmlReport) {
        Write-Success "XML report: $xmlReport"
    }
    if (Test-Path $jsonReport) {
        Write-Success "JSON report: $jsonReport"
    }
}

#================================================================================
# BUILD SUMMARY
#================================================================================

Write-Header "BUILD COMPLETE"

Write-Host "`nBuild artifacts:" -ForegroundColor Cyan
Write-Host "  • Object file:  $OBJ_FILE" -ForegroundColor White
Write-Host "  • Executable:   $EXE_FILE" -ForegroundColor White
Write-Host "`nTo run tests manually:" -ForegroundColor Cyan
Write-Host "  cd $OUTPUT_DIR" -ForegroundColor White
Write-Host "  .\Week4_TestSuite.exe" -ForegroundColor White

if ($allGood) {
    Write-Host "`n✓ BUILD SUCCESSFUL" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n✗ BUILD COMPLETED WITH WARNINGS" -ForegroundColor Yellow
    exit 0
}
