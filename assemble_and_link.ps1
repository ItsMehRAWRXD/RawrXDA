# assemble_and_link.ps1
# Reassemble all MASM sources into fresh .obj files, then link monolithic EXE.
# No CMake, no in-house compiler — just ml64.exe + link.exe.

param(
    [string]$OutDir = "D:\rawrxd\build_genesis",
    [switch]$LinkOnly,
    [switch]$AssembleOnly
)

$ErrorActionPreference = "Continue"
$ml64 = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
$link = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"

if (!(Test-Path $ml64)) { throw "ml64.exe not found" }
if (!(Test-Path $link)) { throw "link.exe not found" }
if (!(Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$root = "D:\rawrxd"

Write-Host "============================================" -Fore Cyan
Write-Host " GENESIS: Assemble + Link from MASM Source" -Fore Cyan
Write-Host "============================================" -Fore Cyan

# ---------------------------------------------------------------
# Phase 1: Assemble all .asm sources with ml64.exe
# ---------------------------------------------------------------
if (!$LinkOnly) {
    Write-Host "`n[PHASE 1] Assembling MASM sources..." -Fore Yellow

    # Collect all .asm files from src/asm/ and src/
    $asmFiles = @()
    $asmFiles += Get-ChildItem "$root\src\asm\*.asm" -ErrorAction SilentlyContinue
    $asmFiles += Get-ChildItem "$root\src\*.asm" -ErrorAction SilentlyContinue

    # Exclude known-bad files
    $excludeNames = @(
        "MASM_STACK_ALIGNMENT_AUDIT_REPORT",  # Not real code
        "RawrXD_NanoQuant_Streaming"           # 0 bytes
    )

    $asmFiles = $asmFiles | Where-Object {
        $base = $_.BaseName
        $_.Length -gt 0 -and $excludeNames -notcontains $base
    }

    Write-Host "  Found $($asmFiles.Count) .asm source files" -Fore White

    $ok = 0; $fail = 0; $skip = 0
    $failList = @()

    foreach ($asm in $asmFiles) {
        $objName = $asm.BaseName + ".obj"
        $objPath = Join-Path $OutDir $objName

        # Skip if obj is newer than source
        if ((Test-Path $objPath) -and ((Get-Item $objPath).LastWriteTime -ge $asm.LastWriteTime)) {
            $skip++
            continue
        }

        $errLog = Join-Path $OutDir "$($asm.BaseName).err"
        $proc = Start-Process -FilePath $ml64 -ArgumentList "/c /nologo /W0 /Fo`"$objPath`" `"$($asm.FullName)`"" `
            -Wait -PassThru -NoNewWindow -RedirectStandardError $errLog -RedirectStandardOutput "$OutDir\$($asm.BaseName).log"

        if ($proc.ExitCode -eq 0) {
            $ok++
            Remove-Item $errLog, "$OutDir\$($asm.BaseName).log" -Force -ErrorAction SilentlyContinue
        } else {
            $fail++
            $errText = Get-Content $errLog -Raw -ErrorAction SilentlyContinue
            $failList += "$($asm.Name): $errText"
        }
    }

    Write-Host "  Assembled: $ok  Skipped (up-to-date): $skip  Failed: $fail" -Fore $(if ($fail -gt 0) { "Yellow" } else { "Green" })
    if ($fail -gt 0) {
        Write-Host "`n  Failed assemblies:" -Fore Red
        foreach ($f in $failList) {
            $firstLine = ($f -split "`n")[0]
            Write-Host "    $firstLine" -Fore Red
        }
    }
}

if ($AssembleOnly) {
    Write-Host "`n[DONE] Assemble-only mode. Objects in: $OutDir" -Fore Cyan
    exit 0
}

# ---------------------------------------------------------------
# Phase 2: Collect all linkable .obj files
# ---------------------------------------------------------------
Write-Host "`n[PHASE 2] Collecting linkable objects..." -Fore Yellow

# Freshly assembled objects
$freshObjs = Get-ChildItem "$OutDir\*.obj" -ErrorAction SilentlyContinue

# Also include existing good objects from obj/ (the ones that passed before)
$existingObjs = Get-ChildItem "$root\obj\*.obj" -ErrorAction SilentlyContinue | Where-Object {
    $n = $_.Name
    # Exclude known corrupt, LTCG, bench/test, compiler, CMake
    if ($n -match "\.cpp\.obj$|\.c\.obj$|\.cc\.obj$") { return $false }
    if ($n -match "^bench_|^test_|compiler_from_scratch|omega_pro|OmegaPolyglot") { return $false }
    if ($n -match "^CMakeC") { return $false }

    # Check for LTCG (anonymous object format — first 2 bytes are 0x0000 not 0x8664)
    try {
        $bytes = [System.IO.File]::ReadAllBytes($_.FullName)
        if ($bytes.Length -lt 2) { return $false }
        # AMD64 COFF magic = 0x8664, LTCG anonymous = 0x0000
        $magic = [BitConverter]::ToUInt16($bytes, 0)
        if ($magic -ne 0x8664) { return $false }
    } catch { return $false }

    return $true
}

# Also pick up root-level .obj if they pass the same checks
$rootObjs = Get-ChildItem "$root\*.obj" -ErrorAction SilentlyContinue | Where-Object {
    $n = $_.Name
    if ($n -match "\.cpp\.obj$|\.c\.obj$|\.cc\.obj$") { return $false }
    if ($n -match "^bench_|^test_|compiler_from_scratch|omega_pro|OmegaPolyglot|^NUL\.|^temp_test\.|^proof\.") { return $false }
    if ($n -match "^CMakeC") { return $false }
    try {
        $bytes = [System.IO.File]::ReadAllBytes($_.FullName)
        if ($bytes.Length -lt 2) { return $false }
        $magic = [BitConverter]::ToUInt16($bytes, 0)
        if ($magic -ne 0x8664) { return $false }
    } catch { return $false }
    return $true
}

# Merge all, deduplicate by name (prefer freshly assembled)
$allObjs = @{}
foreach ($o in $existingObjs) { $allObjs[$o.Name] = $o.FullName }
foreach ($o in $rootObjs) { $allObjs[$o.Name] = $o.FullName }
foreach ($o in $freshObjs) { $allObjs[$o.Name] = $o.FullName }  # Fresh wins

Write-Host "  Fresh assembled: $($freshObjs.Count)" -Fore White
Write-Host "  Existing (obj/): $($existingObjs.Count)" -Fore White
Write-Host "  Root level:      $($rootObjs.Count)" -Fore White
Write-Host "  Total unique:    $($allObjs.Count)" -Fore Cyan

# Remove known-corrupt objects (invalid .pdata → LNK1223)
$corruptNames = @(
    "mmap_loader.obj", "lsp_jsonrpc.obj", "kv_cache_mgr.obj",
    "dequant_simd.obj", "NUL.obj", "temp_test.obj",
    "RawrXD_P2PRelay_New.obj", "win32ide_main.obj"
)
foreach ($bad in $corruptNames) {
    if ($allObjs.ContainsKey($bad)) {
        $allObjs.Remove($bad)
        Write-Host "  Excluded corrupt: $bad" -Fore DarkYellow
    }
}
Write-Host "  After exclusions: $($allObjs.Count)" -Fore Cyan

# ---------------------------------------------------------------
# Phase 3: Link (with auto-retry for corrupt .pdata objects)
# ---------------------------------------------------------------
Write-Host "`n[PHASE 3] Linking monolithic RawrXD.exe..." -Fore Yellow

$finalExe = Join-Path $OutDir "RawrXD.exe"
$rspFile = Join-Path $OutDir "link.rsp"
$linkOut = Join-Path $OutDir "link_output.log"
$linkErr = Join-Path $OutDir "link_errors.log"

$maxRetries = 20
for ($attempt = 1; $attempt -le $maxRetries; $attempt++) {
    # Write RSP
    $allObjs.Values | Sort-Object | ForEach-Object { "`"$_`"" } | Out-File -FilePath $rspFile -Encoding ASCII

    $linkArgs = @(
        "/NOLOGO",
        "/OUT:`"$finalExe`"",
        "/SUBSYSTEM:WINDOWS",
        "/ENTRY:WinMain",
        "/LARGEADDRESSAWARE",
        "/FIXED:NO",
        "/DYNAMICBASE",
        "/NXCOMPAT",
        "/MACHINE:X64",
        "/INCREMENTAL:NO",
        "/FORCE:MULTIPLE",
        "/FORCE:UNRESOLVED",
        "/NODEFAULTLIB:msvcrt",
        "/NODEFAULTLIB:msvcrtd",
        "/NODEFAULTLIB:vulkan-1.lib",
        "/MERGE:.rdata=.text",
        "/IGNORE:4006",
        "/IGNORE:4088",
        "/IGNORE:4078",
        "/LIBPATH:`"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64`"",
        "/LIBPATH:`"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64`"",
        "/LIBPATH:`"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64`"",
        "@`"$rspFile`"",
        "libcmt.lib", "libvcruntime.lib", "libucrt.lib", "legacy_stdio_definitions.lib",
        "kernel32.lib", "user32.lib", "gdi32.lib", "shell32.lib", "advapi32.lib",
        "ole32.lib", "oleaut32.lib", "uuid.lib", "comctl32.lib", "comdlg32.lib",
        "shlwapi.lib", "ws2_32.lib", "ntdll.lib", "psapi.lib", "version.lib",
        "crypt32.lib", "bcrypt.lib", "wininet.lib", "winhttp.lib",
        "opengl32.lib", "dwmapi.lib", "uxtheme.lib"
    )

    $proc = Start-Process -FilePath $link -ArgumentList $linkArgs -Wait -PassThru -NoNewWindow `
        -RedirectStandardOutput $linkOut -RedirectStandardError $linkErr

    if ($proc.ExitCode -eq 1223) {
        # Parse the corrupt object name from the log
        $logText = Get-Content $linkOut -Raw -ErrorAction SilentlyContinue
        if ($logText -match '(\S+\.obj)\s*:\s*fatal error LNK1223') {
            $badObj = $Matches[1]
            Write-Host "  [Attempt $attempt] Excluding corrupt: $badObj" -Fore DarkYellow
            $allObjs.Remove($badObj)
            continue
        }
    }
    break  # Exit loop on success or non-LNK1223 failure
}

# Show results
$errors = Get-Content $linkOut -Raw -ErrorAction SilentlyContinue
$fatals = ($errors -split "`n") | Where-Object { $_ -match "fatal error" }
$warnings = ($errors -split "`n") | Where-Object { $_ -match "warning LNK" }
$unresolvedCount = ($errors -split "`n" | Where-Object { $_ -match "LNK2001|LNK2019" }).Count

if ($proc.ExitCode -eq 0 -and (Test-Path $finalExe)) {
    $sizeMB = [math]::Round((Get-Item $finalExe).Length / 1MB, 2)
    Write-Host "`n============================================" -Fore Green
    Write-Host " GENESIS BUILD COMPLETE" -Fore Green
    Write-Host " Output:   $finalExe" -Fore White
    Write-Host " Size:     $sizeMB MB" -Fore White
    Write-Host " Objects:  $($allObjs.Count)" -Fore White
    Write-Host " Warnings: $($warnings.Count)" -Fore $(if ($warnings.Count -gt 0) { "Yellow" } else { "Green" })
    Write-Host " Unresolved (forced): $unresolvedCount" -Fore $(if ($unresolvedCount -gt 0) { "Yellow" } else { "Green" })
    Write-Host "============================================" -Fore Green
} else {
    Write-Host "`n[FAILED] Exit code: $($proc.ExitCode)" -Fore Red
    if ($fatals) {
        Write-Host "Fatal errors:" -Fore Red
        $fatals | ForEach-Object { Write-Host "  $_" -Fore Red }
    }
    Write-Host "Full log: $linkOut" -Fore Gray
}
