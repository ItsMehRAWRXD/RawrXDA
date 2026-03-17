# ============================================================================
# VS 2022 Cross Tools Developer Command Prompt - Launch & Build
# ============================================================================
# This script uses the VS Developer Command Prompt shortcut to build BotBuilder
# Location of shortcut: D:\~dev\sdk\x86_x64 Cross Tools Command Prompt for VS 2022 (2).lnk

param(
    [switch]$LaunchApp = $false,
    [switch]$RunTests = $false
)

Write-Host "🔨 Visual Studio 2022 Developer Tools - BotBuilder Builder`n" -ForegroundColor Green

# Paths
$DEV_CMDPROMPT = "D:\~dev\sdk\x86_x64 Cross Tools Command Prompt for VS 2022 (2).lnk"
$BOTBUILDER_PATH = "c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder"
$BUILD_SCRIPT = "$PSScriptRoot\BUILD-BOTBUILDER-DEVTOOLS.bat"
$EXE_PATH = "$BOTBUILDER_PATH\bin\Debug\BotBuilder.exe"

# Verify files exist
Write-Host "📋 Verifying paths..." -ForegroundColor Cyan
if (-not (Test-Path $BUILD_SCRIPT)) {
    Write-Host "❌ Build script not found: $BUILD_SCRIPT" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $BOTBUILDER_PATH)) {
    Write-Host "❌ BotBuilder path not found: $BOTBUILDER_PATH" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Paths verified`n" -ForegroundColor Green

# Method 1: Direct batch execution (simpler, no GUI needed)
Write-Host "🛠️  Building BotBuilder using MSBuild..." -ForegroundColor Cyan
Write-Host "   (This uses your VS 2022 Developer Command Prompt environment)`n" -ForegroundColor Gray

# Execute build script in CMD
cmd /c "%BUILD_SCRIPT%"
$buildResult = $LastExitCode

if ($buildResult -ne 0) {
    Write-Host "❌ Build failed with exit code: $buildResult" -ForegroundColor Red
    Write-Host "`nTroubleshooting:" -ForegroundColor Yellow
    Write-Host "1. Check if Developer Command Prompt has msbuild in PATH"
    Write-Host "2. Verify all NuGet packages restored"
    Write-Host "3. Check if bin/obj folders were deleted before rebuild"
    exit 1
}

# Verify executable exists
if (Test-Path $EXE_PATH) {
    Write-Host "✅ BotBuilder.exe created successfully!`n" -ForegroundColor Green
    Write-Host "Location: $EXE_PATH`n" -ForegroundColor Gray
    
    # Option to launch
    if ($LaunchApp) {
        Write-Host "🚀 Launching BotBuilder..." -ForegroundColor Cyan
        & $EXE_PATH
    } else {
        Write-Host "To run the application:" -ForegroundColor Yellow
        Write-Host "  - Option 1: $EXE_PATH" -ForegroundColor Gray
        Write-Host "  - Option 2: .\RUN-BOTBUILDER.ps1 -LaunchApp" -ForegroundColor Gray
        Write-Host "  - Option 3: .\RUN-BOTBUILDER.ps1" -ForegroundColor Gray
        Write-Host "`nOr double-click the .exe file in File Explorer" -ForegroundColor Gray
    }
} else {
    Write-Host "❌ Executable not found at: $EXE_PATH" -ForegroundColor Red
    Write-Host "This usually means the build succeeded but output path is different." -ForegroundColor Yellow
    
    # Search for any exe
    Write-Host "`nSearching for built executables..." -ForegroundColor Cyan
    $exes = Get-ChildItem "$BOTBUILDER_PATH\bin" -Recurse -Filter "*.exe" -ErrorAction SilentlyContinue
    if ($exes) {
        Write-Host "Found:" -ForegroundColor Green
        foreach ($exe in $exes) {
            Write-Host "  - $($exe.FullName)" -ForegroundColor Gray
        }
    } else {
        Write-Host "No executables found. Build may have failed silently." -ForegroundColor Red
    }
}

Write-Host "`n" -ForegroundColor Green
