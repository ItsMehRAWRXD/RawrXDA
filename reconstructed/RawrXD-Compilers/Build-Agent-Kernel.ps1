# Build Agent Kernel - Properly set up MSVC environment in PowerShell
param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$MSVC_PATH = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64"
$CL = "$MSVC_PATH\cl.exe"
$LINK = "$MSVC_PATH\link.exe"

# Ensure obj and bin directories exist
if (!(Test-Path "obj")) { New-Item -ItemType Directory "obj" | Out-Null }
if (!(Test-Path "bin")) { New-Item -ItemType Directory "bin" | Out-Null }

if ($Clean) {
    Write-Host "[*] Cleaning..." -ForegroundColor Yellow
    Remove-Item "obj\agent_kernel*.obj" -Force -EA SilentlyContinue
    Remove-Item "bin\agent_kernel_test.exe" -Force -EA SilentlyContinue
}

Write-Host "[*] Building RawrXD Agent Kernel (C++20)" -ForegroundColor Cyan
Write-Host "[*] Compiler: MSVC 19.50" -ForegroundColor Cyan
Write-Host ""

# Set up environment for CL.exe to find headers
$env:INCLUDE = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um"
$env:LIB = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"

# Compile agent_kernel.cpp
Write-Host "[*] Compiling agent_kernel.cpp..." -ForegroundColor Yellow
& $CL `
    /std:c++20 `
    /EHsc `
    /W4 `
    /Zi `
    /O2 `
    /Fo"obj\agent_kernel.obj" `
    /c `
    agent_kernel.cpp

if ($LASTEXITCODE -ne 0) {
    Write-Host "[-] Compilation FAILED" -ForegroundColor Red
    exit 1
}
Write-Host "[+] agent_kernel.obj compiled" -ForegroundColor Green

# Compile agent_kernel_test.cpp
Write-Host "[*] Compiling agent_kernel_test.cpp..." -ForegroundColor Yellow
& $CL `
    /std:c++20 `
    /EHsc `
    /W4 `
    /Zi `
    /O2 `
    /Fo"obj\agent_kernel_test.obj" `
    /c `
    agent_kernel_test.cpp

if ($LASTEXITCODE -ne 0) {
    Write-Host "[-] Compilation FAILED" -ForegroundColor Red
    exit 1
}
Write-Host "[+] agent_kernel_test.obj compiled" -ForegroundColor Green

# Link executable
Write-Host "[*] Linking agent_kernel_test.exe..." -ForegroundColor Yellow
& $LINK `
    /NOLOGO `
    /SUBSYSTEM:CONSOLE `
    /OUT:"bin\agent_kernel_test.exe" `
    obj\agent_kernel.obj `
    obj\agent_kernel_test.obj `
    kernel32.lib `
    ntdll.lib `
    user32.lib

if ($LASTEXITCODE -ne 0) {
    Write-Host "[-] Linking FAILED" -ForegroundColor Red
    exit 1
}
Write-Host "[+] agent_kernel_test.exe created" -ForegroundColor Green

Write-Host ""
Write-Host "[+] BUILD COMPLETE" -ForegroundColor Green
Write-Host "[*] Executable: bin\agent_kernel_test.exe" -ForegroundColor Cyan
Write-Host ""
Write-Host "Run with: .\bin\agent_kernel_test.exe" -ForegroundColor Yellow
