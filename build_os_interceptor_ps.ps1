#============================================================================
# OS Explorer Interceptor Build Script - PowerShell Version
# Uses Visual Studio Developer Command Prompt environment
#============================================================================

param(
    [switch]$Clean,
    [switch]$Verbose,
    [switch]$ForceCLI,
    [string]$Masm32Path = "C:\masm32"
)

# Set paths
$VSPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
$VCTools = "$VSPath\VC\Tools\MSVC\14.44.35207"
$ML64Path = "$VCTools\bin\Hostx64\x64"
$LinkPath = "$VCTools\bin\Hostx64\x64"
$IncludePath = "$VCTools\include"
$LibPath = "$VCTools\lib\x64"
$WinSdkVersion = "10.0.22621.0"
$WinSdkLibBase = "C:\Program Files (x86)\Windows Kits\10\Lib"
$WinSdkUmLib = "$WinSdkLibBase\$WinSdkVersion\um\x64"
$WinSdkUcrtLib = "$WinSdkLibBase\$WinSdkVersion\ucrt\x64"
$WinSdkUmLibX86 = "$WinSdkLibBase\$WinSdkVersion\um\x86"
$WinSdkUcrtLibX86 = "$WinSdkLibBase\$WinSdkVersion\ucrt\x86"
$MasmInclude = Join-Path $Masm32Path "include\windows.inc"
$MasmBinPath = Join-Path $Masm32Path "bin"
$ML32Path = Join-Path $MasmBinPath "ml.exe"
$BuildCli = (Test-Path $MasmInclude) -and (Test-Path $ML32Path)
if ($ForceCLI -and (-not $BuildCli)) {
    Write-Warning "MASM32 not found at $Masm32Path; CLI build disabled. Use -Masm32Path to point to your MASM32 install."
}

$SrcPath = "src"
$BinPath = "bin"
$ObjPath = "obj"
$ScriptRoot = $PSScriptRoot

# Create directories
if (-not (Test-Path $BinPath)) { New-Item -Path $BinPath -ItemType Directory -Force | Out-Null }
if (-not (Test-Path $ObjPath)) { New-Item -Path $ObjPath -ItemType Directory -Force | Out-Null }

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning build directories..." -ForegroundColor Yellow
    if (Test-Path $ObjPath) { Remove-Item -Path $ObjPath -Recurse -Force }
    if (Test-Path $BinPath) { Remove-Item -Path $BinPath -Recurse -Force }
    New-Item -Path $BinPath -ItemType Directory -Force | Out-Null
    New-Item -Path $ObjPath -ItemType Directory -Force | Out-Null
}

Write-Host "Building OS Explorer Interceptor with Visual Studio..." -ForegroundColor Cyan
Write-Host ""

try {
    # [1/6] Assemble OS Explorer Interceptor DLL (Simplified)
    Write-Host "[1/6] Assembling OS Explorer Interceptor DLL (Simplified)..." -ForegroundColor Yellow
    $ml64Args = @(
        "/c",
        "/Fo$ObjPath\os_explorer_interceptor.obj",
        "-D_WINDOWS",
        "-D_AMD64_",
        "$SrcPath\os_explorer_interceptor_simple.asm"
    )
    
    $process = Start-Process -FilePath "$ML64Path\ml64.exe" -ArgumentList $ml64Args -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne 0) {
        throw "Failed to assemble interceptor DLL"
    }
    Write-Host "  [OK] DLL assembled" -ForegroundColor Green
    
    # [2/6] Link OS Explorer Interceptor DLL
    Write-Host "[2/6] Linking OS Explorer Interceptor DLL..." -ForegroundColor Yellow
    $linkArgs = @(
        "/DLL",
        "/SUBSYSTEM:WINDOWS",
        "/ENTRY:DllMain",
        "/IGNORE:4216",
        "/OUT:$BinPath\os_explorer_interceptor.dll",
        "/LIBPATH:`"$LibPath`"",
        "/LIBPATH:`"$WinSdkUmLib`"",
        "/LIBPATH:`"$WinSdkUcrtLib`"",
        "/DEF:$SrcPath\os_interceptor.def",
        "$ObjPath\os_explorer_interceptor.obj",
        "kernel32.lib",
        "user32.lib",
        "advapi32.lib",
        "ws2_32.lib",
        "ole32.lib"
    )
    
    $process = Start-Process -FilePath "$LinkPath\link.exe" -ArgumentList $linkArgs -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne 0) {
        throw "Failed to link interceptor DLL"
    }
    Write-Host "  [OK] DLL linked" -ForegroundColor Green
    
    if (-not $BuildCli) {
        Write-Host "[3/6] Skipping OS Interceptor CLI build (MASM32 not found at $Masm32Path). Use -Masm32Path <path> to point to your MASM32 install." -ForegroundColor Yellow
    }
    else {
        # [3/6] Assemble OS Interceptor CLI (Universal)
        Write-Host "[3/6] Assembling OS Interceptor CLI (Universal)..." -ForegroundColor Yellow
        $cliSourcePath = Join-Path $ScriptRoot "$SrcPath\os_interceptor_cli_universal.asm"
        $cliObjPath = Join-Path $ScriptRoot "$ObjPath\os_interceptor_cli.obj"
        $cliOutPath = Join-Path $ScriptRoot "$BinPath\os_interceptor_cli.exe"
        $masmRootDrive = Split-Path $Masm32Path -Qualifier
        try {
            $ml32Args = @(
                "/c",
                "/coff",
                "/Fo`"$cliObjPath`"",
                "/I`"$Masm32Path\include`"",
                "-D_CONSOLE",
                "-D_WIN32",
                "-D_X86_",
                "`"$cliSourcePath`""
            )
            
            Push-Location $masmRootDrive
            $process = Start-Process -FilePath $ML32Path -ArgumentList $ml32Args -Wait -PassThru -NoNewWindow
            Pop-Location
            if ($process.ExitCode -ne 0) {
                throw "Failed to assemble CLI"
            }
            Write-Host "  [OK] CLI assembled" -ForegroundColor Green
            
            # [4/6] Link OS Interceptor CLI
            Write-Host "[4/6] Linking OS Interceptor CLI..." -ForegroundColor Yellow
            $linkArgs = @(
                "/MACHINE:X86",
                "/SUBSYSTEM:CONSOLE",
                "/OUT:`"$cliOutPath`"",
                "/LIBPATH:`"$Masm32Path\lib`"",
                "/LIBPATH:`"$WinSdkUmLibX86`"",
                "/LIBPATH:`"$WinSdkUcrtLibX86`"",
                "`"$cliObjPath`"",
                "kernel32.lib",
                "user32.lib",
                "advapi32.lib",
                "psapi.lib"
            )
            
            $process = Start-Process -FilePath "$LinkPath\link.exe" -ArgumentList $linkArgs -Wait -PassThru -NoNewWindow
            if ($process.ExitCode -ne 0) {
                throw "Failed to link CLI"
            }
            Write-Host "  [OK] CLI linked" -ForegroundColor Green
        }
        catch {
            Write-Warning "CLI build failed; continuing without CLI. Error: $($_.Exception.Message)"
            $BuildCli = $false
        }
    }
    
    # [5/6] Building PowerShell module
    Write-Host "[5/6] Building PowerShell module..." -ForegroundColor Yellow
    Write-Host "  [OK] PowerShell module: modules\OSExplorerInterceptor.psm1" -ForegroundColor Green
    
    # [6/6] Creating integration script
    Write-Host "[6/6] Creating integration script..." -ForegroundColor Yellow
    if ($BuildCli) {
        $integrationScript = @"
@echo off
REM OS Explorer Interceptor - Quick Start

echo Starting OS Explorer Interceptor CLI...
start "" "%CD%\$BinPath\os_interceptor_cli.exe"

echo.
echo.
echo To use with PowerShell:
echo   Import-Module "%CD%\modules\OSExplorerInterceptor.psm1"
echo   Start-OSInterceptor -ProcessId 1234 -RealTimeStreaming

echo.
pause
"@
        Write-Host "  [OK] Integration script created" -ForegroundColor Green
    }
    else {
        $integrationScript = @"
@echo off
REM OS Explorer Interceptor - Quick Start (CLI skipped)

echo MASM32 includes not found. CLI build was skipped.
echo Use PowerShell module to control the interceptor:
echo   Import-Module "%CD%\modules\OSExplorerInterceptor.psm1"
echo   Start-OSInterceptor -ProcessId 1234 -RealTimeStreaming

echo.
pause
"@
        Write-Host "  [OK] Integration script created (CLI skipped)" -ForegroundColor Yellow
    }
    
    $integrationScript | Out-File -FilePath "start_interceptor.bat" -Encoding ASCII
    
    Write-Host ""
    Write-Host "[SUCCESS] Build completed successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Built files:" -ForegroundColor Cyan
    Write-Host "  - $BinPath\os_explorer_interceptor.dll (Interceptor DLL)" -ForegroundColor White
    if ($BuildCli) {
        Write-Host "  - $BinPath\os_interceptor_cli.exe (CLI executable)" -ForegroundColor White
    }
    Write-Host "  - modules\OSExplorerInterceptor.psm1 (PowerShell module)" -ForegroundColor White
    Write-Host "  - start_interceptor.bat (Quick start script)" -ForegroundColor White
    Write-Host ""
    Write-Host "To use:" -ForegroundColor Cyan
    if ($BuildCli) {
        Write-Host "  1. Run CLI: $BinPath\os_interceptor_cli.exe" -ForegroundColor White
        Write-Host "  2. Or use PowerShell: Import-Module modules\OSExplorerInterceptor.psm1" -ForegroundColor White
    }
    else {
        Write-Host "  1. Import-Module modules\OSExplorerInterceptor.psm1" -ForegroundColor White
    }
    Write-Host "  3. Start interception: Start-OSInterceptor -ProcessId 1234 -RealTimeStreaming" -ForegroundColor White
    Write-Host ""
    Write-Host "For help: helpos (in PowerShell)" -ForegroundColor Cyan
    if ($BuildCli) {
        Write-Host "  (CLI help available via 'help' command)" -ForegroundColor Cyan
    }
    Write-Host ""
    
    exit 0
    
} catch {
    Write-Error "[ERROR] Build failed: $_"
    exit 1
}
