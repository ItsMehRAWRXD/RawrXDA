# ============================================================================
# Phase 4 Test Script - Settings Dialog
# ============================================================================
# PowerShell script to test Phase 4 settings dialog components
# ============================================================================

param(
    [switch]$Build,
    [switch]$Test,
    [switch]$All
)

# Set paths
$ML64Path = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
$LinkPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
$SDKPath = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
$CRTPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64"

# Set source directory
$SrcDir = "src\masm\final-ide"

# Create output directory
if (!(Test-Path "build\phase4")) {
    New-Item -ItemType Directory -Path "build\phase4" -Force
}

function Build-Phase4 {
    Write-Host "Building Phase 4 components..." -ForegroundColor Green
    
    # Build registry persistence layer
    Write-Host "Building registry_persistence.asm..." -ForegroundColor Yellow
    & $ML64Path /c /Fo build\phase4\registry_persistence.obj $SrcDir\registry_persistence.asm
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error building registry_persistence.asm" -ForegroundColor Red
        return $false
    }
    
    # Build settings dialog
    Write-Host "Building qt6_settings_dialog.asm..." -ForegroundColor Yellow
    & $ML64Path /c /Fo build\phase4\qt6_settings_dialog.obj $SrcDir\qt6_settings_dialog.asm
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error building qt6_settings_dialog.asm" -ForegroundColor Red
        return $false
    }
    
    # Link Phase 4 components
    Write-Host "Linking Phase 4 components..." -ForegroundColor Yellow
    & $LinkPath /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup `
        build\phase4\registry_persistence.obj `
        build\phase4\qt6_settings_dialog.obj `
        kernel32.lib user32.lib gdi32.lib comctl32.lib advapi32.lib `
        /OUT:build\phase4\phase4_test.exe
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error linking Phase 4 components" -ForegroundColor Red
        return $false
    }
    
    Write-Host "Phase 4 build completed successfully!" -ForegroundColor Green
    Write-Host "Executable: build\phase4\phase4_test.exe" -ForegroundColor Cyan
    return $true
}

function Test-Phase4 {
    Write-Host "Testing Phase 4 components..." -ForegroundColor Green
    
    # Check if executable exists
    if (!(Test-Path "build\phase4\phase4_test.exe")) {
        Write-Host "Executable not found. Run with -Build first." -ForegroundColor Red
        return $false
    }
    
    # Run the test executable
    Write-Host "Running Phase 4 test executable..." -ForegroundColor Yellow
    $process = Start-Process -FilePath "build\phase4\phase4_test.exe" -Wait -PassThru
    
    if ($process.ExitCode -eq 0) {
        Write-Host "Phase 4 test completed successfully!" -ForegroundColor Green
        return $true
    } else {
        Write-Host "Phase 4 test failed with exit code: $($process.ExitCode)" -ForegroundColor Red
        return $false
    }
}

# Main execution
if ($All -or $Build) {
    if (!(Build-Phase4)) {
        exit 1
    }
}

if ($All -or $Test) {
    if (!(Test-Phase4)) {
        exit 1
    }
}

if (!$Build -and !$Test -and !$All) {
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  .\test_phase4.ps1 -Build    # Build Phase 4 components"
    Write-Host "  .\test_phase4.ps1 -Test     # Test Phase 4 components"
    Write-Host "  .\test_phase4.ps1 -All      # Build and test Phase 4"
}