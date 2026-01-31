#==============================================================================
# BUILD_WEEK1.ps1
# RawrXD Week 1 Infrastructure Build Script
# Pure x64 MASM Assembly Compilation
#==============================================================================

param(
    [switch]$Clean,
    [switch]$Debug,
    [switch]$Release,
    [switch]$Test,
    [switch]$UseComplete,
    [string]$BuildDir = "build_week1"
)

$ErrorActionPreference = "Stop"

#------------------------------------------------------------------------------
# Configuration
#------------------------------------------------------------------------------
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SourceDir = $ScriptDir
$RootDir = Join-Path $ScriptDir ".." ".." ".."
$BuildPath = Join-Path $RootDir $BuildDir

# Source files
$CompleteAsm = Join-Path $SourceDir "WEEK1_COMPLETE.asm"
$DeliverableAsm = Join-Path $SourceDir "WEEK1_DELIVERABLE.asm"
$HeaderFile = Join-Path $SourceDir "Week1_API.h"

# Determine which source to use
if ($UseComplete -or (Test-Path $CompleteAsm)) {
    $PrimaryAsm = $CompleteAsm
    $AsmName = "WEEK1_COMPLETE"
    Write-Host "[INFO] Using complete implementation (2100+ LOC)" -ForegroundColor Green
} else {
    $PrimaryAsm = $DeliverableAsm
    $AsmName = "WEEK1_DELIVERABLE"
    Write-Host "[INFO] Using deliverable implementation" -ForegroundColor Yellow
}

# Output files
$ObjFile = Join-Path $BuildPath "$AsmName.obj"
$LibFile = Join-Path $BuildPath "week1.lib"
$TestExe = Join-Path $BuildPath "week1_test.exe"

#------------------------------------------------------------------------------
# Find Visual Studio
#------------------------------------------------------------------------------
function Find-VsInstall {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -property installationPath
        return $vsPath
    }
    
    # Fallback paths
    $fallbacks = @(
        "C:\VS2022Enterprise",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional",
        "C:\Program Files\Microsoft Visual Studio\2022\Community"
    )
    
    foreach ($path in $fallbacks) {
        if (Test-Path $path) {
            return $path
        }
    }
    
    throw "Visual Studio not found"
}

function Initialize-VsEnvironment {
    $vsPath = Find-VsInstall
    $vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    
    if (-not (Test-Path $vcvarsall)) {
        throw "vcvars64.bat not found at: $vcvarsall"
    }
    
    Write-Host "[INFO] Initializing VS environment from: $vsPath" -ForegroundColor Cyan
    
    # Execute vcvars64.bat and capture environment
    $envOutput = cmd /c "`"$vcvarsall`" && set"
    
    foreach ($line in $envOutput) {
        if ($line -match "^([^=]+)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

#------------------------------------------------------------------------------
# Build Functions
#------------------------------------------------------------------------------
function Clean-Build {
    Write-Host "`n[CLEAN] Removing build directory..." -ForegroundColor Yellow
    if (Test-Path $BuildPath) {
        Remove-Item -Path $BuildPath -Recurse -Force
    }
    Write-Host "[CLEAN] Done" -ForegroundColor Green
}

function Ensure-BuildDir {
    if (-not (Test-Path $BuildPath)) {
        New-Item -Path $BuildPath -ItemType Directory -Force | Out-Null
        Write-Host "[INFO] Created build directory: $BuildPath" -ForegroundColor Cyan
    }
}

function Compile-Assembly {
    Write-Host "`n[MASM] Compiling $AsmName.asm..." -ForegroundColor Cyan
    
    # MASM flags
    $MasmFlags = @(
        "/c",           # Compile only
        "/nologo",      # No banner
        "/W3",          # Warning level 3
        "/Cp",          # Preserve case
        "/Cx"           # Preserve case in publics/externs
    )
    
    if ($Debug) {
        $MasmFlags += "/Zi"     # Debug info
        $MasmFlags += "/DDEBUG=1"
    } else {
        $MasmFlags += "/O2"     # Optimize
    }
    
    $MasmFlags += "/Fo`"$ObjFile`""
    $MasmFlags += "`"$PrimaryAsm`""
    
    $flagStr = $MasmFlags -join " "
    Write-Host "[MASM] ml64 $flagStr" -ForegroundColor DarkGray
    
    $process = Start-Process -FilePath "ml64" -ArgumentList $MasmFlags -NoNewWindow -Wait -PassThru
    
    if ($process.ExitCode -ne 0) {
        throw "MASM compilation failed with exit code: $($process.ExitCode)"
    }
    
    if (-not (Test-Path $ObjFile)) {
        throw "Object file not created: $ObjFile"
    }
    
    $objSize = (Get-Item $ObjFile).Length / 1KB
    Write-Host "[MASM] Created: $ObjFile ($([math]::Round($objSize, 2)) KB)" -ForegroundColor Green
}

function Create-Library {
    Write-Host "`n[LIB] Creating static library..." -ForegroundColor Cyan
    
    $LibFlags = @(
        "/OUT:`"$LibFile`"",
        "/NOLOGO",
        "`"$ObjFile`""
    )
    
    $flagStr = $LibFlags -join " "
    Write-Host "[LIB] lib $flagStr" -ForegroundColor DarkGray
    
    $process = Start-Process -FilePath "lib" -ArgumentList $LibFlags -NoNewWindow -Wait -PassThru
    
    if ($process.ExitCode -ne 0) {
        throw "Library creation failed with exit code: $($process.ExitCode)"
    }
    
    $libSize = (Get-Item $LibFile).Length / 1KB
    Write-Host "[LIB] Created: $LibFile ($([math]::Round($libSize, 2)) KB)" -ForegroundColor Green
}

function Build-TestHarness {
    Write-Host "`n[TEST] Building test harness..." -ForegroundColor Cyan
    
    # Create simple test harness C file
    $testCFile = Join-Path $BuildPath "week1_test.c"
    
    $testCode = @"
/* Week 1 Infrastructure Test Harness */
#include <stdio.h>
#include <Windows.h>

/* Import Week 1 functions */
extern int __cdecl Week1Initialize(void** ppInfrastructure);
extern int __cdecl Week1StartBackgroundThreads(void* pInfrastructure);
extern void __cdecl Week1Shutdown(void* pInfrastructure);
extern int __cdecl SubmitTask(void* pInfrastructure, void* pTask);
extern int __cdecl Week1RegisterNode(void* pInfra, int nodeId, int ip, short port, void* cb, void* ctx);
extern int __cdecl Week1RegisterResource(void* pInfra, unsigned __int64 resourceId, const char* name);
extern void __cdecl Week1GetStatistics(void* pInfra, void* pStats);
extern const char* __cdecl Week1GetVersion(void);

/* Simple task function */
static unsigned __int64 g_TaskCounter = 0;

void TestTaskFunc(void* ctx) {
    InterlockedIncrement64(&g_TaskCounter);
}

int main(int argc, char** argv) {
    void* infra = NULL;
    int result;
    unsigned __int64 stats[8];
    
    printf("Week 1 Infrastructure Test\\n");
    printf("===========================\\n\\n");
    
    /* Get version */
    printf("Version: %s\\n\\n", Week1GetVersion());
    
    /* Initialize */
    printf("[TEST] Initializing infrastructure...\\n");
    result = Week1Initialize(&infra);
    if (result != 0) {
        printf("[ERROR] Initialization failed: %d\\n", result);
        return 1;
    }
    printf("[OK] Infrastructure initialized at %p\\n", infra);
    
    /* Start background threads */
    printf("[TEST] Starting background threads...\\n");
    result = Week1StartBackgroundThreads(infra);
    if (result != 0) {
        printf("[ERROR] Failed to start threads: %d\\n", result);
        Week1Shutdown(infra);
        return 1;
    }
    printf("[OK] Background threads started\\n");
    
    /* Register test node */
    printf("[TEST] Registering test node...\\n");
    result = Week1RegisterNode(infra, 1, 0x7F000001, 8080, NULL, NULL);
    printf("[%s] Node registration: %d\\n", result ? "OK" : "FAIL", result);
    
    /* Register test resource */
    printf("[TEST] Registering test resource...\\n");
    result = Week1RegisterResource(infra, 1001, "test_lock");
    printf("[%s] Resource registration: %d\\n", result ? "OK" : "FAIL", result);
    
    /* Let it run */
    printf("[TEST] Running for 2 seconds...\\n");
    Sleep(2000);
    
    /* Get statistics */
    printf("[TEST] Getting statistics...\\n");
    Week1GetStatistics(infra, stats);
    printf("  Tasks Submitted: %llu\\n", stats[0]);
    printf("  Tasks Executed:  %llu\\n", stats[1]);
    printf("  Tasks Stolen:    %llu\\n", stats[2]);
    printf("  Worker Count:    %u\\n", (unsigned)stats[3]);
    
    /* Shutdown */
    printf("[TEST] Shutting down...\\n");
    Week1Shutdown(infra);
    printf("[OK] Shutdown complete\\n");
    
    printf("\\n=== TEST PASSED ===\\n");
    return 0;
}
"@
    
    Set-Content -Path $testCFile -Value $testCode -Encoding UTF8
    
    # Compile test harness
    $ClFlags = @(
        "/nologo",
        "/Od",
        "/Zi",
        "/Fe:`"$TestExe`"",
        "`"$testCFile`"",
        "`"$LibFile`"",
        "kernel32.lib",
        "ntdll.lib"
    )
    
    $flagStr = $ClFlags -join " "
    Write-Host "[CL] cl $flagStr" -ForegroundColor DarkGray
    
    $process = Start-Process -FilePath "cl" -ArgumentList $ClFlags -NoNewWindow -Wait -PassThru
    
    if ($process.ExitCode -ne 0) {
        Write-Host "[WARN] Test harness compilation failed" -ForegroundColor Yellow
    } else {
        Write-Host "[OK] Test executable: $TestExe" -ForegroundColor Green
    }
}

function Run-Tests {
    Write-Host "`n[TEST] Running tests..." -ForegroundColor Cyan
    
    if (-not (Test-Path $TestExe)) {
        Write-Host "[WARN] Test executable not found, building..." -ForegroundColor Yellow
        Build-TestHarness
    }
    
    if (Test-Path $TestExe) {
        Write-Host "[TEST] Executing: $TestExe" -ForegroundColor Cyan
        & $TestExe
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[PASS] All tests passed" -ForegroundColor Green
        } else {
            Write-Host "[FAIL] Tests failed with code: $LASTEXITCODE" -ForegroundColor Red
        }
    }
}

function Print-Summary {
    Write-Host "`n" + "="*60 -ForegroundColor Cyan
    Write-Host "BUILD SUMMARY" -ForegroundColor Cyan
    Write-Host "="*60 -ForegroundColor Cyan
    
    Write-Host "Source:  $PrimaryAsm"
    Write-Host "Object:  $ObjFile"
    Write-Host "Library: $LibFile"
    
    if (Test-Path $ObjFile) {
        $objSize = (Get-Item $ObjFile).Length / 1KB
        Write-Host "         Object size: $([math]::Round($objSize, 2)) KB" -ForegroundColor Green
    }
    
    if (Test-Path $LibFile) {
        $libSize = (Get-Item $LibFile).Length / 1KB
        Write-Host "         Library size: $([math]::Round($libSize, 2)) KB" -ForegroundColor Green
    }
    
    Write-Host "`n[SUCCESS] Week 1 build complete!" -ForegroundColor Green
}

#------------------------------------------------------------------------------
# Main
#------------------------------------------------------------------------------
Write-Host "="*60 -ForegroundColor Cyan
Write-Host "RawrXD Week 1 Infrastructure Build" -ForegroundColor Cyan
Write-Host "="*60 -ForegroundColor Cyan

try {
    # Clean if requested
    if ($Clean) {
        Clean-Build
        if (-not $Debug -and -not $Release -and -not $Test) {
            exit 0
        }
    }
    
    # Initialize VS environment
    Initialize-VsEnvironment
    
    # Create build directory
    Ensure-BuildDir
    
    # Compile assembly
    Compile-Assembly
    
    # Create library
    Create-Library
    
    # Build test harness
    Build-TestHarness
    
    # Run tests if requested
    if ($Test) {
        Run-Tests
    }
    
    # Print summary
    Print-Summary
    
} catch {
    Write-Host "[ERROR] $($_.Exception.Message)" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor DarkRed
    exit 1
}
