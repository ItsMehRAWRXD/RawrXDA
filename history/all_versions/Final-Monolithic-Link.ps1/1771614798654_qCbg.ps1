# Final-Monolithic-Link.ps1
# Links all 1600+ .obj files into a single PE32+ executable (RawrXD.exe)
# Architecture: source = linking - assembling (No recompile)

param(
    [string]$Root = "D:\rawrxd",
    [string]$OutDir = "D:\rawrxd\bin",
    [string]$ObjDir = "D:\rawrxd", # We will search for .obj files here
    [string[]]$ExcludeDirs = @(".git", "dist", "node_modules", "temp", "source_audit", "crash_dumps", "build_fresh", "build_gold", "build_prod", "build_universal", "build_msvc", "build_qt_free", "build_win32_gui_test")
)

$ErrorActionPreference = "Stop"

Write-Host "============================================================" -Fore Cyan
Write-Host " RAWRXD MONOLITHIC LINKER - 1600+ SOURCE GLORY" -Fore Cyan
Write-Host "============================================================" -Fore Cyan

# Ensure output directory exists
if(!(Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

# 1. Find the MSVC Linker
$Linker = $null
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if(Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if($vsPath) {
        $toolsPath = Get-ChildItem "$vsPath\VC\Tools\MSVC" | Sort-Object Name -Descending | Select-Object -First 1
        if($toolsPath) {
            $Linker = "$($toolsPath.FullName)\bin\Hostx64\x64\link.exe"
        }
    }
}

if(!$Linker -or !(Test-Path $Linker)) {
    # Fallback to environment variable if vswhere fails
    $Linker = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
    if(!(Test-Path $Linker)) {
        throw "link.exe not found. Please run from a Developer Command Prompt."
    }
}

Write-Host "[INFO] Using Linker: $Linker" -Fore Green

# 2. Gather all .obj files (excluding backup/test build dirs to avoid duplicates)
Write-Host "[INFO] Scanning for .obj files in $ObjDir..." -Fore White
$allObjs = Get-ChildItem -Path $ObjDir -Filter "*.obj" -Recurse -ErrorAction SilentlyContinue | Where-Object {
    $path = $_.FullName
    $exclude = $false
    foreach($dir in $ExcludeDirs) {
        if($path -match "\\$dir\\" -or $path -match "CMakeFiles") { $exclude = $true; break }
    }
    return -not $exclude
}

# Deduplicate by name just in case (taking the newest one if duplicates exist)
$uniqueObjs = $allObjs | Sort-Object LastWriteTime -Descending | Group-Object Name | ForEach-Object { $_.Group[0] }

Write-Host "[INFO] Found $($uniqueObjs.Count) unique .obj files for the final link." -Fore Cyan

if($uniqueObjs.Count -eq 0) {
    throw "No .obj files found! Cannot link."
}

# 3. Create a response file for the linker (to bypass command-line length limits)
$responseFile = Join-Path $OutDir "link_objects.rsp"
$uniqueObjs.FullName | Out-File -FilePath $responseFile -Encoding ASCII
Write-Host "[INFO] Created linker response file: $responseFile" -Fore Gray

# 4. Define the final executable path
$finalExe = Join-Path $OutDir "RawrXD.exe"

# 5. Construct linker arguments
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
    "@`"$responseFile`""
)

# Add static libraries if they exist
$coreLib = Join-Path $Root "lib\rawrxd_core.lib"
$gpuLib = Join-Path $Root "lib\rawrxd_gpu.lib"

if (Test-Path $coreLib) { $linkArgs += "`"$coreLib`"" }
if (Test-Path $gpuLib) { $linkArgs += "`"$gpuLib`"" }

# Add system libraries
$linkArgs += "kernel32.lib", "user32.lib", "gdi32.lib", "shell32.lib", "ole32.lib", "advapi32.lib", "crypt32.lib", "psapi.lib", "shlwapi.lib"

Write-Host "`n[LINKING] Forging Monolithic Executable: $finalExe..." -Fore Yellow
Write-Host "Command: link.exe $($linkArgs -join ' ')" -Fore DarkGray

# 6. Execute the linker
$proc = Start-Process -FilePath $Linker -ArgumentList $linkArgs -Wait -PassThru -NoNewWindow

if($proc.ExitCode -ne 0) {
    Write-Host "`n[ERROR] Linking failed with exit code $($proc.ExitCode)" -Fore Red
    exit $proc.ExitCode
}

Write-Host "`n============================================================" -Fore Green
Write-Host " SUCCESS! 1600+ SOURCE GLORY ACHIEVED" -Fore Green
Write-Host " Final Executable: $finalExe" -Fore White
Write-Host "============================================================" -Fore Green

# Cleanup response file
if(Test-Path $responseFile) { Remove-Item $responseFile -Force }
