#============================================================================
# Universal Build Script for OS Explorer Interceptor
# Builds both x86 and x64 versions
#============================================================================

param(
    [switch]$Clean,
    [switch]$Verbose,
    [string]$Masm32Path = "C:\masm32",
    [string]$Configuration = "Release"
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

$SrcPath = "src"
$BinPath = "bin"
$ObjPath = "obj"

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

Write-Host "Building OS Explorer Interceptor (Universal)..." -ForegroundColor Cyan
Write-Host ""

try {
    # Build x64 DLL (simplified version)
    Write-Host "[1/6] Assembling x64 Interceptor DLL..." -ForegroundColor Yellow
    $ml64Args = @(
        "/c",
        "/Fo$ObjPath\os_explorer_interceptor_x64.obj",
        "-D_WINDOWS",
        "-D_AMD64_",
        "$SrcPath\os_explorer_interceptor_simple.asm"
    )
    
    $process = Start-Process -FilePath "$ML64Path\ml64.exe" -ArgumentList $ml64Args -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne 0) {
        throw "Failed to assemble x64 DLL"
    }
    Write-Host "  [OK] x64 DLL assembled" -ForegroundColor Green
    
    # Link x64 DLL
    Write-Host "[2/6] Linking x64 Interceptor DLL..." -ForegroundColor Yellow
    $linkArgs = @(
        "/DLL",
        "/SUBSYSTEM:WINDOWS",
        "/ENTRY:DllMain",
        "/IGNORE:4216",
        "/OUT:$BinPath\os_explorer_interceptor_x64.dll",
        "/LIBPATH:`"$LibPath`"",
        "/LIBPATH:`"$WinSdkUmLib`"",
        "/LIBPATH:`"$WinSdkUcrtLib`"",
        "/DEF:$SrcPath\os_interceptor.def",
        "$ObjPath\os_explorer_interceptor_x64.obj",
        "kernel32.lib",
        "user32.lib",
        "advapi32.lib",
        "ws2_32.lib",
        "ole32.lib"
    )
    
    $process = Start-Process -FilePath "$LinkPath\link.exe" -ArgumentList $linkArgs -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne 0) {
        throw "Failed to link x64 DLL"
    }
    Write-Host "  [OK] x64 DLL linked" -ForegroundColor Green
    
    # Build x86 DLL (simplified version)
    Write-Host "[3/6] Assembling x86 Interceptor DLL..." -ForegroundColor Yellow
    $x86SourcePath = Join-Path $ScriptRoot "$SrcPath\os_explorer_interceptor_simple_x86.asm"
    $x86ObjPath = Join-Path $ScriptRoot "$ObjPath\os_explorer_interceptor_x86.obj"
    
    # Use relative paths from MASM32 directory
    $relativeSourcePath = $x86SourcePath.Substring($ScriptRoot.Length + 1)
    $relativeObjPath = $x86ObjPath.Substring($ScriptRoot.Length + 1)
    
    $ml32Args = @(
        "/c",
        "/coff",
        "/Fo`"$relativeObjPath`"",
        "/I`"$Masm32Path\include`"",
        "-D_WINDOWS",
        "-D_WIN32",
        "-D_X86_",
        "`"$relativeSourcePath`""
    )
    
    # Run from the project root directory
    Push-Location $ScriptRoot
    $process = Start-Process -FilePath $ML32Path -ArgumentList $ml32Args -Wait -PassThru -NoNewWindow
    Pop-Location
    if ($process.ExitCode -ne 0) {
        throw "Failed to assemble x86 DLL"
    }
    Write-Host "  [OK] x86 DLL assembled" -ForegroundColor Green
    
    # Link x86 DLL
    Write-Host "[4/6] Linking x86 Interceptor DLL..." -ForegroundColor Yellow
    $linkArgs = @(
        "/DLL",
        "/SUBSYSTEM:WINDOWS",
        "/ENTRY:DllMain",
        "/MACHINE:X86",
        "/OUT:$BinPath\os_explorer_interceptor_x86.dll",
        "/LIBPATH:`"$Masm32Path\lib`"",
        "/LIBPATH:`"$WinSdkUmLibX86`"",
        "/LIBPATH:`"$WinSdkUcrtLibX86`"",
        "/DEF:$SrcPath\os_interceptor.def",
        "$ObjPath\os_explorer_interceptor_x86.obj",
        "kernel32.lib",
        "user32.lib",
        "advapi32.lib",
        "ws2_32.lib",
        "ole32.lib"
    )
    
    $process = Start-Process -FilePath "$LinkPath\link.exe" -ArgumentList $linkArgs -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne 0) {
        throw "Failed to link x86 DLL"
    }
    Write-Host "  [OK] x86 DLL linked" -ForegroundColor Green
    
    # Build Universal CLI (x86 executable that can handle both)
    Write-Host "[5/6] Assembling Universal CLI..." -ForegroundColor Yellow
    $cliSourcePath = Join-Path $ScriptRoot "$SrcPath\os_interceptor_cli_universal.asm"
    $cliObjPath = Join-Path $ScriptRoot "$ObjPath\os_interceptor_cli.obj"
    $cliOutPath = Join-Path $ScriptRoot "$BinPath\os_interceptor_cli.exe"
    
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
    
    # Link Universal CLI
    Write-Host "[6/6] Linking Universal CLI..." -ForegroundColor Yellow
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
    
    Write-Host ""
    Write-Host "[SUCCESS] Universal build completed successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Built files:" -ForegroundColor Cyan
    Write-Host "  - $BinPath\os_explorer_interceptor_x64.dll (x64 Interceptor DLL)" -ForegroundColor White
    Write-Host "  - $BinPath\os_explorer_interceptor_x86.dll (x86 Interceptor DLL)" -ForegroundColor White
    Write-Host "  - $BinPath\os_interceptor_cli.exe (Universal CLI - x86)" -ForegroundColor White
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Cyan
    Write-Host "  1. Run CLI: $BinPath\os_interceptor_cli.exe" -ForegroundColor White
    Write-Host "  2. It will automatically detect and use the correct DLL" -ForegroundColor White
    Write-Host ""
    Write-Host "Features:" -ForegroundColor Cyan
    Write-Host "  - Automatically finds existing Cursor processes" -ForegroundColor White
    Write-Host "  - Can launch new Cursor instances" -ForegroundColor White
    Write-Host "  - Injects the appropriate DLL (x86 or x64)" -ForegroundColor White
    Write-Host "  - Provides detailed status messages" -ForegroundColor White
    Write-Host ""
    
    exit 0
    
} catch {
    Write-Error "[ERROR] Build failed: $_"
    exit 1
}
