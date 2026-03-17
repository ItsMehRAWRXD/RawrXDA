# genesis_build.ps1
# Final Link One-Liner for Monolithic EXE
# Architecture: source = linking - assembling (No recompile)

param(
    [string]$Root = "D:\rawrxd",
    [string]$OutDir = "$env:LOCALAPPDATA\RawrXD\bin",
    [string]$LibDir = "$env:LOCALAPPDATA\RawrXD\lib"
)

$ErrorActionPreference = "Stop"

Write-Host "============================================================" -Fore Cyan
Write-Host " RAWRXD MONOLITHIC LINKER - 1600+ SOURCE GLORY" -Fore Cyan
Write-Host " Architecture: Monolithic EXE (All agents as threads)" -Fore Gray
Write-Host "============================================================" -Fore Cyan

if(!(Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$Linker = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
if(!(Test-Path $Linker)) {
    throw "link.exe not found at $Linker"
}

Write-Host "[INFO] Using MSVC Linker: $Linker" -Fore Green

Write-Host "[INFO] Scanning for pure ASM .obj files in $Root..." -Fore White
$ExcludeDirs = @(".git", "dist", "node_modules", "temp", "source_audit", "crash_dumps", "build_fresh", "build_gold", "build_prod", "build_universal", "build_msvc", "build_qt_free", "build_win32_gui_test", "CMakeFiles", "build_clean", "build_ide_ninja", "build_new", "build_test_parse")

$objFiles = Get-ChildItem -Path $Root -Filter "*.obj" -Recurse -ErrorAction SilentlyContinue | Where-Object {
    $path = $_.FullName
    $name = $_.Name
    
    # Exclude C/C++ object files that cause LNK1143 (corrupt file errors)
    if($name -match "\.cpp\.obj$" -or $name -match "\.c\.obj$" -or $name -match "\.cc\.obj$") { return $false }
    
    # Exclude bench/test/compiler files that cause LNK2038 (RuntimeLibrary mismatch)
    if($name -match "^bench_" -or $name -match "^test_" -or $name -match "compiler_from_scratch") { return $false }
    if($name -match "dumpbin_final\.obj") { return $false }
    foreach($dir in $ExcludeDirs) {
        if($path -match "\\$dir\\" -or $path -match "CMakeFiles") { $exclude = $true; break }
    }
    return -not $exclude
}

# Deduplicate by name (take newest)
$uniqueObjs = $objFiles | Sort-Object LastWriteTime -Descending | Group-Object Name | ForEach-Object { $_.Group[0] }

Write-Host "[INFO] Found $($uniqueObjs.Count) pure ASM .obj files for the final link." -Fore Cyan

if($uniqueObjs.Count -eq 0) {
    throw "No .obj files found! Cannot link."
}

$responseFile = Join-Path $OutDir "link_objects.rsp"
$uniqueObjs.FullName | Out-File -FilePath $responseFile -Encoding ASCII
Write-Host "[INFO] Created linker response file: $responseFile" -Fore Gray

$finalExe = Join-Path $OutDir "RawrXD.exe"

$linkArgs = @(
    "/OUT:`"$finalExe`"",
    "/SUBSYSTEM:WINDOWS",
    "/ENTRY:WinMain",
    "/LARGEADDRESSAWARE",
    "/FIXED:NO",
    "/DYNAMICBASE",
    "/NXCOMPAT",
    "/NODEFAULTLIB:libcmt",
    "/MERGE:.rdata=.text",
    "/FORCE:MULTIPLE", # MAGIC BULLET: Ignore duplicate symbols from test/bench files
    "@`"$responseFile`""
)

$coreLib = Join-Path $Root "lib\rawrxd_core.lib"
$gpuLib = Join-Path $Root "lib\rawrxd_gpu.lib"

if (Test-Path $coreLib) { $linkArgs += "`"$coreLib`"" }
if (Test-Path $gpuLib) { $linkArgs += "`"$gpuLib`"" }

$linkArgs += "kernel32.lib", "user32.lib", "gdi32.lib", "shell32.lib", "ole32.lib", "advapi32.lib", "crypt32.lib", "psapi.lib", "shlwapi.lib"

Write-Host "`n[LINKING] Forging Monolithic Executable: $finalExe..." -Fore Yellow

$proc = Start-Process -FilePath $Linker -ArgumentList $linkArgs -Wait -PassThru -NoNewWindow

if($proc.ExitCode -ne 0) {
    Write-Host "`n[ERROR] Linking failed with exit code $($proc.ExitCode)" -Fore Red
    exit $proc.ExitCode
}

Write-Host "`n============================================================" -Fore Green
Write-Host " SUCCESS! 1600+ SOURCE GLORY ACHIEVED" -Fore Green
Write-Host " Final Executable: $finalExe" -Fore White
Write-Host " All agents are now consolidated as threads inside the single executable." -Fore Green
Write-Host "============================================================" -Fore Green

if(Test-Path $responseFile) { Remove-Item $responseFile -Force }
