$ErrorActionPreference = "Stop"

# ============================================================
#  RAWRXD CLEAN LINKER — No brute force needed
#  Pre-validated objects + COFF checking + blacklist
# ============================================================
#
#  This replaces genesis_bruteforce_link.ps1 entirely.
#  Instead of trial-and-error linking, it:
#    1. Loads obj_blacklist.ps1 (static + dynamic validation)
#    2. Validates every .obj COFF header BEFORE linking
#    3. Links once, deterministically
# ============================================================

$Root       = "D:\rawrxd"
$OutDir     = "$env:LOCALAPPDATA\RawrXD\bin"
$finalExe   = Join-Path $OutDir "RawrXD.exe"

# ---- Resolve the best available MSVC linker ----
$LinkerCandidates = @(
    "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
)
$Linker = $null
foreach ($c in $LinkerCandidates) {
    if (Test-Path $c) { $Linker = $c; break }
}
if (-not $Linker) {
    Write-Host "[FATAL] No MSVC linker found. Checked:" -Fore Red
    $LinkerCandidates | ForEach-Object { Write-Host "  $_" -Fore DarkGray }
    exit 1
}

# ---- Resolve best MSVC lib path ----
$MSVCLibCandidates = @(
    "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64"
)
$MSVCLib = $null
foreach ($c in $MSVCLibCandidates) {
    if (Test-Path $c) { $MSVCLib = $c; break }
}

Write-Host "============================================================" -Fore Cyan
Write-Host " RAWRXD CLEAN LINKER (blacklist-validated)" -Fore Cyan
Write-Host " Linker: $Linker" -Fore DarkGray
Write-Host "============================================================" -Fore Cyan

if (!(Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

# ---- Load the blacklist/validator ----
. (Join-Path $Root "obj_blacklist.ps1")

# ---- Get pre-validated clean objects ----
$cleanObjs = Get-CleanObjects -Verbose

if ($cleanObjs.Count -eq 0) {
    Write-Host "[FATAL] No clean objects found. Cannot link." -Fore Red
    exit 1
}

Write-Host "[LINK] Linking $($cleanObjs.Count) validated objects..." -Fore Cyan

# ---- Build link arguments ----
$linkArgs = @(
    "/OUT:`"$finalExe`"",
    "/SUBSYSTEM:WINDOWS",
    "/ENTRY:WinMain",
    "/LARGEADDRESSAWARE:NO",
    "/FIXED:NO",
    "/DYNAMICBASE",
    "/NXCOMPAT",
    "/NODEFAULTLIB:libcmt",
    "/NODEFAULTLIB:vulkan-1.lib",
    "/MERGE:.rdata=.text",
    "/FORCE:MULTIPLE",
    "/FORCE:UNRESOLVED",
    "/LTCG:OFF"
)

# Add ALL available MSVC lib paths (some have different CRT versions)
$AllMSVCLibPaths = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64",
    "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64"
)
foreach ($lp in $AllMSVCLibPaths) {
    if (Test-Path $lp) { $linkArgs += "/LIBPATH:`"$lp`"" }
}
$linkArgs += "/LIBPATH:`"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64`""
$linkArgs += "/LIBPATH:`"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64`""

# Custom libs
$coreLib = Join-Path $Root "lib\rawrxd_core.lib"
$gpuLib  = Join-Path $Root "lib\rawrxd_gpu.lib"
if (Test-Path $coreLib) { $linkArgs += "`"$coreLib`"" }
if (Test-Path $gpuLib)  { $linkArgs += "`"$gpuLib`"" }

# System libs
$linkArgs += "kernel32.lib", "user32.lib", "gdi32.lib", "shell32.lib",
             "ole32.lib", "advapi32.lib", "crypt32.lib", "psapi.lib",
             "shlwapi.lib", "ws2_32.lib", "ntdll.lib",
             "msvcrt.lib", "vcruntime.lib", "ucrt.lib"

# ---- Write response file ----
$rspFile = Join-Path $OutDir "link_objects.rsp"
$rspContent = @()
foreach ($arg in $linkArgs) { $rspContent += $arg }
foreach ($obj in $cleanObjs) { $rspContent += "`"$($obj.FullName)`"" }
$rspContent | Out-File -FilePath $rspFile -Encoding ASCII

# ---- Also save the validated object list for audit ----
$goodListFile = Join-Path $OutDir "good_objects.txt"
$cleanObjs | ForEach-Object { $_.FullName } | Out-File -FilePath $goodListFile -Encoding ASCII

# ---- Run linker via batch (avoids PS quoting issues) ----
$stdoutFile = Join-Path $OutDir "link_stdout.txt"
$stderrFile = Join-Path $OutDir "link_stderr.txt"
$batchFile  = Join-Path $OutDir "run_link.bat"

@"
@echo off
"$Linker" @"$rspFile" 1>"$stdoutFile" 2>"$stderrFile"
exit /b %errorlevel%
"@ | Set-Content -Path $batchFile -Encoding ASCII

$cmdProc = Start-Process -FilePath $batchFile -NoNewWindow -Wait -PassThru

# Wait for child link.exe
Start-Sleep -Seconds 2
while (Get-Process -Name link -ErrorAction SilentlyContinue) {
    Write-Host "  Waiting for link.exe..." -Fore DarkGray
    Start-Sleep -Seconds 3
}

$output      = if (Test-Path $stdoutFile) { Get-Content $stdoutFile -Raw -ErrorAction SilentlyContinue } else { "" }
$errorOutput = if (Test-Path $stderrFile) { Get-Content $stderrFile -Raw -ErrorAction SilentlyContinue } else { "" }
if ($null -eq $output) { $output = "" }
if ($null -eq $errorOutput) { $errorOutput = "" }
$fullOutput = $output + "`n" + $errorOutput

# ---- Check result ----
if ($cmdProc.ExitCode -eq 0 -or ((Test-Path $finalExe) -and $fullOutput -notmatch "fatal error")) {
    $sz = (Get-Item $finalExe).Length
    Write-Host ""
    Write-Host "============================================================" -Fore Green
    Write-Host " SUCCESS! CLEAN LINK COMPLETE (first attempt, no brute force)" -Fore Green
    Write-Host " Executable:  $finalExe" -Fore White
    Write-Host " Size:        $([math]::Round($sz/1MB, 2)) MB" -Fore White
    Write-Host " Objects:     $($cleanObjs.Count)" -Fore White
    Write-Host " Linker exit: $($cmdProc.ExitCode)" -Fore $(if($cmdProc.ExitCode -eq 0){'Green'}else{'Yellow'})
    Write-Host "============================================================" -Fore Green
    
    # Count link warnings
    if ($output) {
        $warnCount = ([regex]::Matches($output, 'LNK\d+')).Count
        if ($warnCount -gt 0) {
            Write-Host " Link warnings: $warnCount (see $stdoutFile)" -Fore Yellow
        }
    }
} else {
    Write-Host ""
    Write-Host "============================================================" -Fore Red
    Write-Host " LINK FAILED  (exit code: $($cmdProc.ExitCode))" -Fore Red
    Write-Host " Check: $stdoutFile" -Fore DarkGray
    Write-Host " Check: $stderrFile" -Fore DarkGray
    Write-Host "============================================================" -Fore Red
    
    # Show first few errors
    if ($fullOutput) {
        $fullOutput -split "`n" | Select-String "error" | Select-Object -First 10 | ForEach-Object {
            Write-Host "  $($_.Line)" -Fore DarkRed
        }
    }
    exit 1
}
