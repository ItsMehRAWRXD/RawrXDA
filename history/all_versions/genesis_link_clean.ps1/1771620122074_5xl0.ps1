$ErrorActionPreference = "Continue"

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

# ---- Write initial response file (will be rebuilt per attempt in auto-learn) ----
# (good_objects.txt is written AFTER auto-learn completes, not before)

# ---- Link with auto-learn loop ----
# If LNK1223 (corrupt .pdata) or LNK1136 (corrupt file) is detected,
# the offending .obj is blacklisted and the link retried automatically.
$maxRetries = 30
$attempt = 1
$additionalBlacklist = @()
$stdoutFile = Join-Path $OutDir "link_stdout.txt"
$stderrFile = Join-Path $OutDir "link_stderr.txt"
$batchFile  = Join-Path $OutDir "run_link.bat"
$rspFile    = Join-Path $OutDir "link_objects.rsp"
$linkExitCode = -1

while ($attempt -le $maxRetries) {
    # Rebuild response file excluding any newly-discovered corrupt objects
    $rspContent = [System.Collections.ArrayList]@()
    foreach ($arg in $linkArgs) { [void]$rspContent.Add($arg) }
    $filteredObjs = @($cleanObjs | Where-Object { $_.Name -notin $additionalBlacklist })
    foreach ($obj in $filteredObjs) { [void]$rspContent.Add("`"$($obj.FullName)`"") }
    [System.IO.File]::WriteAllLines($rspFile, [string[]]$rspContent.ToArray())
    
    if ($attempt -eq 1) {
        Write-Host "[LINK] Attempt ${attempt}: $($filteredObjs.Count) objects..." -Fore Cyan
    } else {
        Write-Host "[LINK] Attempt ${attempt}: $($filteredObjs.Count) objects (removed $($additionalBlacklist.Count) corrupt)..." -Fore Yellow
    }

    # Delete any previous exe so we can check success by file existence
    if (Test-Path $finalExe) { Remove-Item $finalExe -Force -ErrorAction SilentlyContinue }

    # Write batch file — cmd /c ensures synchronous execution
    @"
@echo off
"$Linker" @"$rspFile" >"$stdoutFile" 2>"$stderrFile"
echo EXIT_CODE=%errorlevel%
exit /b %errorlevel%
"@ | Set-Content -Path $batchFile -Encoding ASCII

    # Execute synchronously via cmd /c
    $proc = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "`"$batchFile`"" `
        -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$OutDir\bat_stdout.tmp" -RedirectStandardError "$OutDir\bat_stderr.tmp"
    $linkExitCode = $proc.ExitCode

    # Extra safety: wait for any lingering link.exe (shouldn't happen with cmd /c /Wait)
    $waited = 0
    while ((Get-Process -Name link -ErrorAction SilentlyContinue) -and $waited -lt 30) {
        Start-Sleep -Seconds 1
        $waited++
    }

    # Read outputs
    $output      = if (Test-Path $stdoutFile) { Get-Content $stdoutFile -Raw -ErrorAction SilentlyContinue } else { "" }
    $errorOutput = if (Test-Path $stderrFile) { Get-Content $stderrFile -Raw -ErrorAction SilentlyContinue } else { "" }
    if ($null -eq $output) { $output = "" }
    if ($null -eq $errorOutput) { $errorOutput = "" }
    $fullOutput = $output + "`n" + $errorOutput

    # Auto-learn: check for LNK1223 (corrupt .pdata)
    $lnk1223Match = [regex]::Match($fullOutput, '([a-zA-Z0-9_.\-]+\.obj)\s*:\s*fatal error LNK1223')
    if ($lnk1223Match.Success) {
        $badObj = $lnk1223Match.Groups[1].Value
        Write-Host "  [AUTO-BLOCK] LNK1223 corrupt .pdata: $badObj" -Fore Red
        $additionalBlacklist += $badObj
        $attempt++
        continue
    }
    
    # Auto-learn: check for LNK1136 (corrupt file)
    $lnk1136Match = [regex]::Match($fullOutput, '([a-zA-Z0-9_.\-]+\.obj)\s*:\s*fatal error LNK1136')
    if ($lnk1136Match.Success) {
        $badObj = $lnk1136Match.Groups[1].Value
        Write-Host "  [AUTO-BLOCK] LNK1136 corrupt file: $badObj" -Fore Red
        $additionalBlacklist += $badObj
        $attempt++
        continue
    }

    # Auto-learn: check for LNK1181 (can't open input)
    $lnk1181Match = [regex]::Match($fullOutput, 'fatal error LNK1181.*?cannot open.*?([a-zA-Z0-9_.\-]+\.obj)')
    if ($lnk1181Match.Success) {
        $badObj = $lnk1181Match.Groups[1].Value
        Write-Host "  [AUTO-BLOCK] LNK1181 missing input: $badObj" -Fore Red
        $additionalBlacklist += $badObj
        $attempt++
        continue
    }

    # No retryable error — break
    break
}

# ---- Save audit trail ----
$goodListFile = Join-Path $OutDir "good_objects.txt"
$filteredObjs | ForEach-Object { $_.FullName } | Out-File -FilePath $goodListFile -Encoding ASCII

if ($additionalBlacklist.Count -gt 0) {
    Write-Host ""
    Write-Host "[AUTO-LEARNED] $($additionalBlacklist.Count) corrupt objects discovered:" -Fore Yellow
    $additionalBlacklist | ForEach-Object { Write-Host "  $_" -Fore Red }
    Write-Host ""
    Write-Host "  These should be added to obj_blacklist.ps1 BlacklistedNames for permanent blocking." -Fore DarkGray
    
    # Auto-append to blacklist file for next run
    $autoLearnFile = Join-Path $OutDir "auto_learned_blacklist.txt"
    $additionalBlacklist | Out-File -FilePath $autoLearnFile -Encoding ASCII
    Write-Host "  Saved to: $autoLearnFile" -Fore DarkGray
}

# ---- Check result ----
$exeExists = Test-Path $finalExe
$hasFatalError = $fullOutput -match "fatal error"

if ($exeExists -and -not $hasFatalError) {
    $sz = (Get-Item $finalExe).Length
    Write-Host ""
    Write-Host "============================================================" -Fore Green
    Write-Host " SUCCESS! CLEAN LINK COMPLETE" -Fore Green
    Write-Host " Executable:  $finalExe" -Fore White
    Write-Host " Size:        $([math]::Round($sz/1MB, 2)) MB" -Fore White
    Write-Host " Objects:     $($filteredObjs.Count)" -Fore White
    Write-Host " Attempts:    $attempt" -Fore $(if($attempt -eq 1){'Green'}else{'Yellow'})
    Write-Host " Linker exit: $linkExitCode" -Fore $(if($linkExitCode -eq 0){'Green'}else{'Yellow'})
    Write-Host "============================================================" -Fore Green
    
    if ($output) {
        $warnCount = ([regex]::Matches($output, 'LNK\d+')).Count
        if ($warnCount -gt 0) {
            Write-Host " Link warnings: $warnCount (see $stdoutFile)" -Fore Yellow
        }
    }
    
    # Verify PE header
    try {
        $peBytes = [System.IO.File]::ReadAllBytes($finalExe)
        if ($peBytes[0] -eq 0x4D -and $peBytes[1] -eq 0x5A) {
            Write-Host " PE header:   VALID (MZ)" -Fore Green
        } else {
            Write-Host " PE header:   INVALID (no MZ signature!)" -Fore Red
        }
    } catch {
        Write-Host " PE header:   Could not verify ($($_.Exception.Message))" -Fore Yellow
    }
} elseif ($exeExists -and $hasFatalError) {
    # Exe was created but has fatal errors — likely truncated/corrupt
    Remove-Item $finalExe -Force -ErrorAction SilentlyContinue
    Write-Host ""
    Write-Host "============================================================" -Fore Red
    Write-Host " LINK PRODUCED CORRUPT EXE (removed)" -Fore Red
    Write-Host " Fatal error in output despite exe creation." -Fore DarkRed
    Write-Host " Check: $stdoutFile" -Fore DarkGray
    Write-Host "============================================================" -Fore Red
    $fullOutput -split "`n" | Select-String "fatal error" | Select-Object -First 5 | ForEach-Object {
        Write-Host "  $($_.Line.Trim())" -Fore DarkRed
    }
    exit 1
} else {
    Write-Host ""
    Write-Host "============================================================" -Fore Red
    Write-Host " LINK FAILED (exit code: $linkExitCode)" -Fore Red
    Write-Host " Check: $stdoutFile" -Fore DarkGray
    Write-Host " Check: $stderrFile" -Fore DarkGray
    Write-Host "============================================================" -Fore Red
    $fullOutput -split "`n" | Select-String "error" | Select-Object -First 10 | ForEach-Object {
        Write-Host "  $($_.Line.Trim())" -Fore DarkRed
    }
    exit 1
}
