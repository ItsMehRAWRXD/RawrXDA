# Monolithic IDE Linking - Multiple Strategy Execution
# Tries different linking approaches to overcome section overflow

param(
    [string]$VCToolsDir = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64",
    [string]$SDKLibDir = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64",
    [string]$UCRTLibDir = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64",
    [string]$ObjDir = "D:\rawrxd\obj",
    [string]$OutDir = "D:\rawrxd\build",
    [string]$CMakeBuildDir = "D:\rawrxd\build"
)

$ensureVsEnv = "D:\rawrxd\tools\ensure_vsenv.ps1"
if (Test-Path $ensureVsEnv) {
    & $ensureVsEnv | Out-Null
}

$link = Join-Path $VCToolsDir "link.exe"
$lib = Join-Path $VCToolsDir "lib.exe"

if (-not (Test-Path $link)) {
    $linkFromPath = (Get-Command link.exe -ErrorAction SilentlyContinue)?.Source
    if ($linkFromPath) {
        $link = $linkFromPath
        $toolBin = Split-Path $link -Parent
        $libCandidate = Join-Path $toolBin "lib.exe"
        if (Test-Path $libCandidate) { $lib = $libCandidate }
        Write-Host "⚠️  VCToolsDir link.exe missing; using PATH: $link" -ForegroundColor Yellow
    } else {
        Write-Host "❌ link.exe not found: $link (and not on PATH)" -ForegroundColor Red
        exit 1
    }
}

# Ensure output directory exists
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Write-Host "🚀 MONOLITHIC LINKING STRATEGIES" -ForegroundColor Cyan
Write-Host "=" * 80

function Invoke-CMakeTruthLink {
    param(
        [string]$BuildDir,
        [string]$LinkExe
    )

    $objRsp = Join-Path $BuildDir "CMakeFiles\\RawrXD-Win32IDE.dir\\objects1.rsp"
    $buildMake = Join-Path $BuildDir "CMakeFiles\\RawrXD-Win32IDE.dir\\build.make"

    if (-not (Test-Path $objRsp)) { return $false }
    if (-not (Test-Path $buildMake)) { return $false }

    $lines = Get-Content -LiteralPath $buildMake

    # CMake (NMake + MSVC) uses:
    #   cmake.exe -E vs_link_exe ... link.exe ... @CMakeFiles\RawrXD-Win32IDE.dir\objects1.rsp @<<
    # then a heredoc containing the rest of the link args until a line that is exactly: <<
    #
    # NOTE: regex escaping matters here:
    # - We want to match "objects1.rsp" (dot is literal), so use \. not \\.
    $linkLineRegex = 'vs_link_exe.*link\.exe.*objects1\.rsp.*@<<'
    $targetRuleRegex = '^bin\\RawrXD-Win32IDE\.exe:'

    $startIdx = -1

    # Self-heal: prefer anchoring to the target rule so we don't accidentally grab a compile @<< block.
    $anchorIdx = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match $targetRuleRegex) { $anchorIdx = $i; break }
    }

    $searchFrom = 0
    $searchTo = $lines.Count - 1
    if ($anchorIdx -ge 0) {
        $searchFrom = $anchorIdx
        $searchTo = [math]::Min($anchorIdx + 250, $lines.Count - 1)
    }

    for ($i = $searchFrom; $i -le $searchTo; $i++) {
        if ($lines[$i] -match $linkLineRegex) {
            $startIdx = $i + 1
            break
        }
    }

    # Fallback: if anchor window didn't work, scan the whole file.
    if ($startIdx -lt 0) {
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match $linkLineRegex) {
                $startIdx = $i + 1
                break
            }
        }
    }

    if ($startIdx -lt 0) {
        Write-Host "⚠️  Could not locate CMake link args block in build.make" -ForegroundColor Yellow
        Write-Host "   Hint: expected a line matching: $linkLineRegex" -ForegroundColor DarkGray
        return $false
    }

    $argLines = New-Object System.Collections.Generic.List[string]
    for ($j = $startIdx; $j -lt $lines.Count; $j++) {
        if ($lines[$j].Trim() -eq "<<") { break }
        if ($lines[$j].Trim().Length -eq 0) { continue }
        $argLines.Add($lines[$j])
    }

    if ($argLines.Count -eq 0) {
        Write-Host "⚠️  CMake link args block was empty" -ForegroundColor Yellow
        return $false
    }

    $cmakeArgsRsp = Join-Path $BuildDir "codex_cmake_link_args.rsp"
    ($argLines -join "`n") | Out-File -FilePath $cmakeArgsRsp -Encoding ASCII

    Write-Host "   Using CMake objects: $objRsp" -ForegroundColor Gray
    Write-Host "   Using CMake args:    $cmakeArgsRsp" -ForegroundColor Gray

    # Self-heal: if the object response file references missing objects, do an incremental CMake build
    # to (re)generate them. This keeps `-SkipRebuild` usable while still allowing edited sources
    # to participate in the CMake-truth link.
    function Normalize-CMakeRspPath {
        param([string]$p)
        $t = $p.Trim()
        # objects1.rsp entries are typically quoted and use forward slashes.
        $t = $t.Trim('"')
        $t = $t -replace '/', '\'
        return $t
    }

    $missing = New-Object System.Collections.Generic.List[string]
    $tooManyMissing = $false
    $tokenRe = [regex]'"([^"]+)"'
    foreach ($line in (Get-Content -LiteralPath $objRsp)) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }

        $matches = $tokenRe.Matches($line)
        if ($matches.Count -gt 0) {
            foreach ($m in $matches) {
                $rel = Normalize-CMakeRspPath $m.Groups[1].Value
                if ([string]::IsNullOrWhiteSpace($rel)) { continue }
                $full = Join-Path $BuildDir $rel
                if (-not (Test-Path -LiteralPath $full)) {
                    $missing.Add($rel)
                    if ($missing.Count -ge 10) { $tooManyMissing = $true; break }
                }
            }
        } else {
            # Fallback: unquoted entries, whitespace separated
            foreach ($part in ($line -split '\\s+')) {
                $rel = Normalize-CMakeRspPath $part
                if ([string]::IsNullOrWhiteSpace($rel)) { continue }
                $full = Join-Path $BuildDir $rel
                if (-not (Test-Path -LiteralPath $full)) {
                    $missing.Add($rel)
                    if ($missing.Count -ge 10) { $tooManyMissing = $true; break }
                }
            }
        }

        if ($tooManyMissing) { break }
    }

    if ($missing.Count -gt 0) {
        if ($tooManyMissing) {
            Write-Host "   ⚠️  Many CMake objects are missing (showing first $($missing.Count)). Building full target object universe via NMake..." -ForegroundColor Yellow
        } else {
            Write-Host "   ⚠️  Missing objects referenced by CMake ($($missing.Count)). Building missing objects via NMake..." -ForegroundColor Yellow
        }
        Write-Host ("      " + ($missing -join "`n      ")) -ForegroundColor DarkGray

        # Use the same NMake toolchain as the generated build files.
        $toolBin = Split-Path $LinkExe -Parent
        $nmake = Join-Path $toolBin "nmake.exe"
        if (-not (Test-Path -LiteralPath $nmake)) {
            $nmake = (Get-Command nmake.exe -ErrorAction SilentlyContinue)?.Source
        }
        if (-not $nmake) {
            Write-Host "   ❌ nmake.exe not found; cannot self-heal missing objects" -ForegroundColor Red
            return $false
        }

        Push-Location $BuildDir
        try {
            $nmakeLog = Join-Path $BuildDir "codex_nmake_obj_build.log"

            if ($tooManyMissing) {
                & $nmake /nologo -f $buildMake "CMakeFiles\\RawrXD-Win32IDE.dir\\build" 2>&1 |
                    Tee-Object $nmakeLog | Out-Null
                if ($LASTEXITCODE -ne 0) {
                    Write-Host "   ❌ NMake failed while building full target objects (exit=$LASTEXITCODE)" -ForegroundColor Red
                    return $false
                }
            } else {
                foreach ($t in $missing) {
                    & $nmake /nologo -f $buildMake $t 2>&1 |
                        Tee-Object $nmakeLog | Out-Null
                    if ($LASTEXITCODE -ne 0) {
                        Write-Host "   ❌ NMake failed while building: $t (exit=$LASTEXITCODE)" -ForegroundColor Red
                        return $false
                    }
                }
            }
        } finally {
            Pop-Location
        }
    }

    $linkLog = Join-Path $BuildDir "codex_cmake_link.log"
    $exe = Join-Path $BuildDir "bin\\RawrXD-Win32IDE.exe"

    # If we still trip LNK1181 (missing .obj), build that object and retry a few times.
    for ($attempt = 1; $attempt -le 5; $attempt++) {
        Push-Location $BuildDir
        try {
            & $LinkExe /nologo "@$objRsp" "@$cmakeArgsRsp" 2>&1 |
                Tee-Object $linkLog | Out-Null
            $linkExit = $LASTEXITCODE
        } finally {
            Pop-Location
        }

        if ($linkExit -eq 0 -and (Test-Path -LiteralPath $exe)) {
            $sizeMB = [math]::Round((Get-Item $exe).Length / 1MB, 2)
            Write-Host "   ✅ CMAKE TRUTH LINK SUCCESS: $exe ($sizeMB MB)" -ForegroundColor Green
            return $true
        }

        $missingObj = $null
        if (Test-Path -LiteralPath $linkLog) {
            $m = Select-String -LiteralPath $linkLog -Pattern "LNK1181: cannot open input file '([^']+\\.obj)'" -List
            if ($m -and $m.Matches.Count -gt 0) {
                $missingObj = $m.Matches[0].Groups[1].Value
            }
        }

        if ($missingObj) {
            Write-Host "   ⚠️  Link missing object: $missingObj (attempt $attempt/5). Building it via NMake and retrying..." -ForegroundColor Yellow
            $toolBin = Split-Path $LinkExe -Parent
            $nmake = Join-Path $toolBin "nmake.exe"
            if (-not (Test-Path -LiteralPath $nmake)) { $nmake = (Get-Command nmake.exe -ErrorAction SilentlyContinue)?.Source }
            if (-not $nmake) {
                Write-Host "   ❌ nmake.exe not found; cannot self-heal LNK1181" -ForegroundColor Red
                break
            }

            Push-Location $BuildDir
            try {
                & $nmake /nologo -f $buildMake $missingObj 2>&1 |
                    Tee-Object (Join-Path $BuildDir "codex_nmake_obj_build.log") | Out-Null
            } finally {
                Pop-Location
            }
            continue
        }

        Write-Host "   ❌ CMAKE TRUTH LINK FAILED (exit=$linkExit) - see $BuildDir\\codex_cmake_link.log" -ForegroundColor Red
        return $false
    }

    Write-Host "   ❌ CMAKE TRUTH LINK FAILED - see $BuildDir\\codex_cmake_link.log" -ForegroundColor Red
    return $false
}

# Strategy 0: If a CMake build directory exists, use its exact object+lib list instead of guessing.
if (Invoke-CMakeTruthLink -BuildDir $CMakeBuildDir -LinkExe $link) {
    Write-Host "`n✅ Linking strategies complete (CMake truth link succeeded)" -ForegroundColor Cyan
    exit 0
} else {
    Write-Host "`nℹ️  Falling back to custom strategies (CMake truth link not available or failed)" -ForegroundColor Gray
}

# Common libraries
$commonLibs = @(
    "kernel32.lib", "user32.lib", "gdi32.lib", "winspool.lib",
    "comdlg32.lib", "advapi32.lib", "shell32.lib", "ole32.lib",
    "oleaut32.lib", "uuid.lib", "odbc32.lib", "odbccp32.lib",
    # Commonly-needed Win32 feature libs (driven by current unresolveds)
    "comctl32.lib",  # InitCommonControlsEx
    "bcrypt.lib",    # BCryptGenRandom
    "dwmapi.lib",    # DwmSetWindowAttribute
    "shlwapi.lib",   # PathRemoveFileSpecA
    "winhttp.lib"    # WinHttp* APIs
)

# Runtime libs must appear after /LIBPATH so link.exe can resolve them.
# Current object pool is predominantly /MD (dynamic CRT), so prefer the import libs.
# Note: we intentionally avoid forcing /MT static CRT here to prevent LNK2038 RuntimeLibrary mismatch.
$runtimeLibs = @(
    "msvcrt.lib",                    # C runtime import
    "vcruntime.lib",                 # vcruntime import
    "ucrt.lib",                      # UCRT import
    "msvcprt.lib",                   # C++ standard library import
    "legacy_stdio_definitions.lib",  # common for older/newer CRT compatibility
    # Needed to satisfy __vcrt_* / __acrt_* init/shutdown symbols referenced by msvcrt.lib.
    "libvcruntime.lib",
    "libucrt.lib"
)

# Add MSVC lib paths (CRT + ATL/MFC).
# Note: many of the C++ OBJs were compiled with /GL, so link.exe must see the matching MSVC lib dirs.
$vcInstallDir = "C:\VS2022Enterprise\VC"
$msvcVersion = "14.50.35717"

# Prefer deriving layout from the provided VCToolsDir if it matches the expected VC path pattern.
$m = [regex]::Match($VCToolsDir, '^(?<vcRoot>.+?\\VC)\\Tools\\MSVC\\(?<ver>[^\\]+)\\bin\\', 'IgnoreCase')
if ($m.Success) {
    $vcInstallDir = $m.Groups['vcRoot'].Value
    $msvcVersion = $m.Groups['ver'].Value
}

$msvcLibDir = Join-Path $vcInstallDir "Tools\MSVC\$msvcVersion\lib\x64"
$atlmfcLibDir = Join-Path $vcInstallDir "Tools\MSVC\$msvcVersion\atlmfc\lib\x64"

# Build lib paths.
# Important: put MSVC toolset libs first so we don't accidentally pick similarly-named libs from the Windows SDK.
$libPaths = @()
if (Test-Path $msvcLibDir) { $libPaths += "/LIBPATH:$msvcLibDir" } else { Write-Host "⚠️  MSVC lib dir missing: $msvcLibDir" -ForegroundColor Yellow }
if (Test-Path $atlmfcLibDir) { $libPaths += "/LIBPATH:$atlmfcLibDir" } else { Write-Host "⚠️  ATL/MFC lib dir missing: $atlmfcLibDir" -ForegroundColor Yellow }

# If the chosen MSVC lib dir doesn't actually contain the CRT libs (some installs only have clang_rt libs),
# fall back to a toolset that does (typically VS2022 BuildTools).
$msvcLibcmt = Join-Path $msvcLibDir "libcmt.lib"
if (-not (Test-Path $msvcLibcmt)) {
    $fallbackLibcmt = Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*\lib\x64\libcmt.lib" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Select-Object -First 1

    if ($fallbackLibcmt) {
        $fallbackDir = Split-Path $fallbackLibcmt.FullName -Parent
        if ($libPaths -notcontains "/LIBPATH:$fallbackDir") {
            $libPaths += "/LIBPATH:$fallbackDir"
        }
        Write-Host "   Using fallback MSVC CRT lib dir: $fallbackDir" -ForegroundColor Yellow
    } else {
        Write-Host "⚠️  libcmt.lib not found under MSVC toolsets; link will fail until CRT libs are installed" -ForegroundColor Yellow
    }
}

# Windows Kits come after MSVC toolset libs.
if (Test-Path $SDKLibDir) { $libPaths += "/LIBPATH:$SDKLibDir" }
if (Test-Path $UCRTLibDir) { $libPaths += "/LIBPATH:$UCRTLibDir" }


# Collect all object files
$monoObjs = Get-ChildItem "$ObjDir\asm_monolithic_*.obj" | ForEach-Object { $_.FullName }

# Fix 4: Handle duplicate WinMain.
# Prefer the C++ entrypoint if present; exclude ASM main.obj/asm_monolithic_main.obj from the ASM set.
$cppMainObj = Join-Path $ObjDir "win32ide_main.obj"
$asmMainObj = Join-Path $ObjDir "main.obj"
$mainObj = $cppMainObj
$excludeAsmMain = $true

if (-not (Test-Path $cppMainObj)) {
    if (Test-Path $asmMainObj) {
        $mainObj = $asmMainObj
        $excludeAsmMain = $false
        Write-Host "   Using ASM entrypoint: main.obj (win32ide_main.obj not found)" -ForegroundColor Yellow
    } else {
        Write-Host "❌ No entrypoint object found (missing win32ide_main.obj and main.obj)" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "   Using C++ entrypoint: win32ide_main.obj (excluding ASM main.obj)" -ForegroundColor Yellow
}

# Check if we have freshly rebuilt monolithic objects (no asm_ prefix)
$rebuiltObjs = @()
$rebuiltNames = @("beacon.obj", "inference.obj", "main.obj", "model_loader.obj", "swarm_coordinator.obj", "swarm.obj")
foreach ($name in $rebuiltNames) {
    if ($excludeAsmMain -and $name -eq "main.obj") {
        continue
    }
    $path = Join-Path $ObjDir $name
    if (Test-Path $path) {
        $rebuiltObjs += $path
        # Exclude corresponding asm_monolithic_* version to avoid duplicates
        $monoName = "asm_monolithic_" + [System.IO.Path]::GetFileNameWithoutExtension($name) + ".obj"
        $monoPath = Join-Path $ObjDir $monoName
        if ($monoObjs -contains $monoPath) {
            Write-Host "   Replacing $monoName with rebuilt $name" -ForegroundColor Yellow
            $monoObjs = $monoObjs | Where-Object { $_ -ne $monoPath }
        }
    }
}

# If we are using the C++ entrypoint, ensure any asm_monolithic_main.obj is not part of the ASM pool.
if ($excludeAsmMain) {
    $monoMainPath = Join-Path $ObjDir "asm_monolithic_main.obj"
    if ($monoObjs -contains $monoMainPath) {
        Write-Host "   Excluding asm_monolithic_main.obj (duplicate WinMain vs win32ide_main.obj)" -ForegroundColor Yellow
        $monoObjs = $monoObjs | Where-Object { $_ -ne $monoMainPath }
    }
}

# Combine rebuilt and remaining monolithic objects
$allAsmObjs = $rebuiltObjs + $monoObjs

$excludedCppObjNames = @(
    "mmap_loader_stub.obj"  # 6-byte text stub, not a real COFF object (LNK1107)
    "simple_stub.obj"       # Defines main; conflicts with real entrypoints (LNK2005)
)

$allCppObjs = Get-ChildItem "$ObjDir\*.obj" -Exclude asm_monolithic_*, win32ide_main.obj, beacon.obj, inference.obj, main.obj, model_loader.obj, swarm_coordinator.obj, swarm.obj |
    Where-Object { $excludedCppObjNames -notcontains $_.Name } |
    # Keep the link set closer to "real product" objects and avoid test harness artefacts.
    Where-Object { $_.Name -notlike "*__test_*" -and $_.Name -notlike "*_test*.obj" -and $_.Name -notlike "*_mock*.obj" } |
    Where-Object { $_.Length -ge 32 } |
    ForEach-Object { $_.FullName }


Write-Host "`n📊 Object Inventory:" -ForegroundColor Yellow
Write-Host "   ASM Objects (combined): $($allAsmObjs.Count) files"
Write-Host "   Rebuilt ASM: $($rebuiltObjs.Count) files"
Write-Host "   Main Entry: $(Test-Path $mainObj)"
Write-Host "   C++ Objects: $($allCppObjs.Count) files"

function New-ObjectArchive {
    param(
        [string]$Name,
        [string[]]$Objects
    )

    if (-not $Objects -or $Objects.Count -eq 0) {
        Write-Host "   Skipping $Name.lib (0 objects)" -ForegroundColor DarkGray
        return $null
    }

    $archivePath = Join-Path $OutDir "$Name.lib"
    $archiveLog = Join-Path $OutDir "deterministic_$Name.lib.log"
    $libArgs = @(
        "/OUT:$archivePath",
        "/NOLOGO",
        "/IGNORE:4006,4221"
    ) + $Objects

    Write-Host "   Building $Name.lib ($($Objects.Count) objects)..." -ForegroundColor Gray
    & $lib $libArgs 2>&1 | Tee-Object $archiveLog | Out-Null

    if (-not (Test-Path $archivePath)) {
        throw "Archive build failed: $archivePath (see $archiveLog)"
    }

    $sizeKB = [math]::Round((Get-Item $archivePath).Length / 1KB, 2)
    Write-Host "   ✅ $Name.lib ready ($sizeKB KB)" -ForegroundColor Green
    return $archivePath
}

function Split-CppObjects {
    param([string[]]$CppObjectPaths)

    $pool = @($CppObjectPaths | ForEach-Object {
            [pscustomobject]@{
                Path = $_
                Name = [System.IO.Path]::GetFileName($_).ToLowerInvariant()
            }
        })

    $take = {
        param([scriptblock]$predicate)
        $selected = @($pool | Where-Object $predicate)
        $selectedPaths = @($selected | ForEach-Object { $_.Path })
        $selectedSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
        foreach ($p in $selectedPaths) { [void]$selectedSet.Add($p) }
        $pool = @($pool | Where-Object { -not $selectedSet.Contains($_.Path) })
        return $selectedPaths
    }

    $core_cpp = & $take { $_.Name -match '(core|runtime|platform|engine|system|common|util|foundation)' }
    $inference = & $take { $_.Name -match '(infer|model|llm|token|gguf|beacon)' }
    $agents = & $take { $_.Name -match '(agent|copilot|assistant|swarm|planner|orchestrator)' }
    $ui = & $take { $_.Name -match '(ui|window|sidebar|menu|render|editor|view|dialog|panel|theme)' }
    $feature_cpp = @($pool | ForEach-Object { $_.Path })

    return @{
        core_cpp = $core_cpp
        inference = $inference
        agents = $agents
        ui = $ui
        feature_cpp = $feature_cpp
    }
}

# =============================================================================
# DETERMINISTIC CLOSURE: ARCHIVE PARTITION + SINGLE LINK
# =============================================================================
Write-Host "`n" + ("=" * 80)
Write-Host "📘 DETERMINISTIC LINK MODE (archive partitioning)" -ForegroundColor Cyan

$partitionedCpp = Split-CppObjects -CppObjectPaths $allCppObjs
$deterministicExe = Join-Path $OutDir "RawrXD_Deterministic.exe"

try {
    $archiveInputs = @(
        @{ Name = "core_asm";    Objects = $allAsmObjs },
        @{ Name = "core_cpp";    Objects = $partitionedCpp.core_cpp },
        @{ Name = "feature_cpp"; Objects = $partitionedCpp.feature_cpp },
        @{ Name = "agents";      Objects = $partitionedCpp.agents },
        @{ Name = "ui";          Objects = $partitionedCpp.ui },
        @{ Name = "inference";   Objects = $partitionedCpp.inference }
    )

    $archives = @()
    foreach ($group in $archiveInputs) {
        $archive = New-ObjectArchive -Name $group.Name -Objects $group.Objects
        if ($archive) { $archives += $archive }
    }

    Write-Host "`n   Archive set: $($archives.Count) libraries" -ForegroundColor Yellow
    foreach ($a in $archives) {
        Write-Host "   - $([System.IO.Path]::GetFileName($a))" -ForegroundColor DarkGray
    }

    $detLinkLog = Join-Path $OutDir "deterministic_link.log"
    $linkArgs = @(
        "$mainObj"
    ) + $archives + @(
        "/OUT:$deterministicExe",
        "/SUBSYSTEM:WINDOWS",
        "/ENTRY:WinMain",
        "/LARGEADDRESSAWARE",
        "/DYNAMICBASE",
        "/NXCOMPAT",
        "/LTCG:OFF",
        "/INCREMENTAL:NO",
        "/OPT:REF",
        "/OPT:ICF",
        "/FIXED:NO",
        "/STACK:8388608",
        "/HEAP:16777216",
        "/NOLOGO"
    ) + $libPaths + $runtimeLibs + $commonLibs

    Write-Host "`n   Linking deterministic executable..." -ForegroundColor Gray
    & $link $linkArgs 2>&1 | Tee-Object $detLinkLog

    Write-Host "`n" + ("=" * 80)
    Write-Host "📊 LINKING RESULTS SUMMARY" -ForegroundColor Cyan
    Write-Host "=" * 80

    if (Test-Path $deterministicExe) {
        $size = [math]::Round((Get-Item $deterministicExe).Length / 1MB, 2)
        Write-Host "✅ SUCCESS - Deterministic: $size MB" -ForegroundColor Green
        Write-Host "🏆 OUTPUT: $deterministicExe" -ForegroundColor Green
    } else {
        Write-Host "❌ FAILED - Deterministic link (see $detLinkLog)" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "❌ DETERMINISTIC LINK ERROR: $_" -ForegroundColor Red
    exit 1
}

Write-Host "`n✅ Linking strategies complete" -ForegroundColor Cyan
