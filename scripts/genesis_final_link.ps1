# genesis_final_link.ps1 — Monolithic RawrXD.exe linker
# Usage: .\scripts\genesis_final_link.ps1 [-ObjDir <path>] [-OutExe <path>] [-LinkExe <path>] [-LibPath <path>]
# Default ObjDir from build_monolithic: build\monolithic\obj

param(
    [string]$ObjDir = (Join-Path $PSScriptRoot "..\build\monolithic\obj"),
    [string]$OutExe = (Join-Path $PSScriptRoot "..\build\monolithic\RawrXD.exe"),
    [string]$LinkExe = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe",
    [string]$LibPath = ""
)

$ErrorActionPreference = "Stop"
$objs = @(
    "main.obj", "inference.obj", "ui.obj",
    "beacon.obj", "lsp.obj", "agent.obj", "model_loader.obj",
    "dap.obj", "testing.obj", "tasks.obj",
    "swarm.obj", "swarm_coordinator.obj",
    "stream_loader.obj", "webview2_shell.obj", "extension_host.obj"
)
$objPaths = $objs | ForEach-Object { Join-Path $ObjDir $_ }

$outDir = Split-Path -Parent $OutExe
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

$libArgs = @("/LIBPATH:C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64", "/LIBPATH:C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64")
if ($LibPath) { $libArgs += "/LIBPATH:$LibPath" }

& $LinkExe /OUT:$OutExe /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup `
    /LARGEADDRESSAWARE /FIXED:NO /DYNAMICBASE /NXCOMPAT /OPT:REF /OPT:ICF `
    $libArgs $objPaths kernel32.lib user32.lib gdi32.lib shell32.lib ole32.lib uuid.lib `
    /NODEFAULTLIB:libcmt

if ($LASTEXITCODE -ne 0) { throw "Link failed." }
Write-Host "Linked: $OutExe"
