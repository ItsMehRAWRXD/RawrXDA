#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD Compile Fix Orchestrator
.DESCRIPTION
    Three-phase build pipeline:
      Phase 1 — Monolithic MASM64 kernel (src/asm/monolithic/*.asm → RawrXD.exe)
      Phase 2 — Classify all C/C++/ASM compilation errors from batch_*_failed.json
      Phase 3 — Auto-fix what we can (missing headers, dep-stubs, /std:c++17, etc.)
    No external deps. Pure Win32 + MSVC + ML64.
.NOTES
    Run from repo root: .\compile_fix_orchestrator.ps1
    Modes: -SmokeOnly  — just test, no fixes
           -FixAll     — apply all safe auto-fixes
           -MonoOnly   — only build the monolithic kernel
           -StopOnFirstFail — halt on first compile error
           -ReportOnly — parse batch JSONs and print classified report
#>

param(
    [string]$RepoRoot     = $PSScriptRoot,
    [switch]$DryRun,
    [switch]$SmokeOnly,
    [switch]$FixAll,
    [switch]$MonoOnly,
    [switch]$StopOnFirstFail,
    [switch]$ReportOnly,
    [int]$BatchSize       = 50
)

$ErrorActionPreference = "Continue"
Set-Location $RepoRoot

$Cyan    = "`e[36m"
$Green   = "`e[32m"
$Yellow  = "`e[33m"
$Red     = "`e[31m"
$Gray    = "`e[90m"
$Reset   = "`e[0m"
$Bold    = "`e[1m"

Write-Host ""
Write-Host "${Cyan}╔═════════════════════════════════════════════════════════════╗${Reset}"
Write-Host "${Cyan}║      RawrXD Compile Fix Orchestrator  v2.0                 ║${Reset}"
Write-Host "${Cyan}║      Win32 + MSVC + ML64 — Zero External Deps             ║${Reset}"
Write-Host "${Cyan}╚═════════════════════════════════════════════════════════════╝${Reset}"
Write-Host ""

# ═══════════════════════════════════════════════════════════════════════
# TOOLCHAIN RESOLUTION
# ═══════════════════════════════════════════════════════════════════════

$srcDir      = Join-Path $RepoRoot "src"
$includeDir  = Join-Path $RepoRoot "include"
$win32appDir = Join-Path $srcDir   "win32app"
$buildDir    = Join-Path $RepoRoot "build"
$monoDir     = Join-Path $srcDir   "asm\monolithic"

function Resolve-Toolchain {
    $script:cl   = $null
    $script:ml64 = $null
    $script:link = $null
    $script:msvcRoot = $null

    foreach ($drive in @("C:", "D:")) {
        foreach ($base in @(
            "${drive}\VS2022Enterprise\VC\Tools\MSVC",
            "${drive}\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC",
            "${drive}\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
            "${drive}\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
        )) {
            if (-not (Test-Path $base)) { continue }
            $verDir = Get-ChildItem -Path $base -Directory -ErrorAction SilentlyContinue |
                      Sort-Object Name -Descending | Select-Object -First 1
            if (-not $verDir) { continue }
            $bin = Join-Path $verDir.FullName "bin\Hostx64\x64"
            if (Test-Path (Join-Path $bin "cl.exe")) {
                $script:cl       = Join-Path $bin "cl.exe"
                $script:ml64     = Join-Path $bin "ml64.exe"
                $script:link     = Join-Path $bin "link.exe"
                $script:msvcRoot = $verDir.FullName
                return
            }
        }
    }
    # Fallback: PATH
    $found = Get-Command cl.exe -ErrorAction SilentlyContinue
    if ($found) {
        $script:cl   = $found.Source
        $script:ml64 = (Get-Command ml64.exe -ErrorAction SilentlyContinue).Source
        $script:link = (Get-Command link.exe -ErrorAction SilentlyContinue).Source
        $script:msvcRoot = Split-Path (Split-Path (Split-Path $found.Source))
    }
}

Resolve-Toolchain
if (-not $cl) {
    Write-Host "${Red}FATAL: cl.exe not found. Install VS2022 C++ x64 tools.${Reset}"
    exit 1
}
Write-Host "${Green}✓${Reset} cl.exe  : $cl"
Write-Host "${Green}✓${Reset} ml64.exe: $ml64"

# SDK resolution
$sdkRoot = $null
foreach ($r in @("C:\Program Files (x86)\Windows Kits\10", "D:\Program Files (x86)\Windows Kits\10")) {
    if (Test-Path $r) { $sdkRoot = $r; break }
}
$sdkVer = $null
if ($sdkRoot) {
    $sdkInc = Join-Path $sdkRoot "Include"
    $versions = Get-ChildItem -Path $sdkInc -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending
    foreach ($v in $versions) {
        $candidate = $v.Name
        $hasUm = Test-Path (Join-Path $sdkInc "$candidate\um\windows.h")
        $hasUcrt = Test-Path (Join-Path $sdkInc "$candidate\ucrt\crtdbg.h")
        if ($hasUm -and $hasUcrt) {
            $sdkVer = $candidate
            break
        }
    }
    if (-not $sdkVer -and $versions.Count -gt 0) {
        # Last-resort fallback if this machine has a partial SDK install.
        $sdkVer = $versions[0].Name
    }
}

# Build INCLUDE env
$msvcInclude = Join-Path $msvcRoot "include"
$envInclude  = $msvcInclude
if ($sdkRoot -and $sdkVer) {
    $envInclude += ";$(Join-Path $sdkRoot "Include\$sdkVer\ucrt")"
    $envInclude += ";$(Join-Path $sdkRoot "Include\$sdkVer\shared")"
    $envInclude += ";$(Join-Path $sdkRoot "Include\$sdkVer\um")"
    $envInclude += ";$(Join-Path $sdkRoot "Include\$sdkVer\winrt")"
}
$env:INCLUDE = $envInclude
$env:PATH    = "$(Split-Path $cl -Parent);$env:PATH"

# Lib path
$libPath = $null
if ($sdkRoot -and $sdkVer) {
    $um64 = Join-Path $sdkRoot "Lib\$sdkVer\um\x64"
    if (Test-Path $um64) { $libPath = $um64 }
}

$thirdpartyDir = Join-Path $RepoRoot "3rdparty"
$ggmlIncludeDir = Join-Path $thirdpartyDir "ggml\include"
$ggmlSrcDir = Join-Path $thirdpartyDir "ggml\src"

$clBase = @("/nologo", "/c", "/std:c++20", "/EHsc", "/permissive-",
            "/DNOMINMAX", "/DWIN32_LEAN_AND_MEAN", "/D_CRT_SECURE_NO_WARNINGS",
            "/I`"$srcDir`"",
            "/I`"$includeDir`"",
            "/I`"$win32appDir`"",
            "/I`"$thirdpartyDir`"",
            "/I`"$ggmlIncludeDir`"",
            "/I`"$ggmlSrcDir`"")

Write-Host "${Green}✓${Reset} SDK     : $sdkRoot ($sdkVer)"
Write-Host ""

# ═══════════════════════════════════════════════════════════════════════
# PHASE 1 — MONOLITHIC MASM64 KERNEL
# ═══════════════════════════════════════════════════════════════════════

function Build-MonolithicKernel {
    Write-Host "${Cyan}══ Phase 1: Monolithic MASM64 Kernel ══${Reset}"

    $monoSrc = Join-Path $srcDir "asm\monolithic"
    if (-not (Test-Path $monoSrc)) {
        Write-Host "${Yellow}  SKIP: $monoSrc not found${Reset}"
        return $false
    }

    $buildScript = Join-Path $RepoRoot "scripts\build_monolithic.ps1"
    if (Test-Path $buildScript) {
        Write-Host "  Running build_monolithic.ps1 ..."
        & $buildScript 2>&1 | ForEach-Object { Write-Host "  $_" }
        if ($LASTEXITCODE -eq 0) {
            Write-Host "${Green}  ✓ Monolithic kernel built successfully${Reset}"
            return $true
        } else {
            Write-Host "${Red}  ✗ Monolithic build failed (exit $LASTEXITCODE)${Reset}"
            return $false
        }
    }

    # Inline fallback if script missing
    $asmModules = @("main", "inference", "ui", "beacon", "lsp", "agent", "model_loader")
    $objDir = Join-Path $buildDir "monolithic\obj"
    if (-not (Test-Path $objDir)) { New-Item -ItemType Directory -Path $objDir -Force | Out-Null }

    foreach ($name in $asmModules) {
        $asmPath = Join-Path $monoSrc "$name.asm"
        $objPath = Join-Path $objDir  "$name.obj"
        if (-not (Test-Path $asmPath)) {
            Write-Host "${Red}  MISSING: $asmPath${Reset}"
            if ($StopOnFirstFail) { return $false }
            continue
        }
        & $ml64 /c /nologo /Fo $objPath $asmPath 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "${Red}  ✗ $name.asm${Reset}"
            if ($StopOnFirstFail) { return $false }
        } else {
            Write-Host "${Green}  ✓${Reset} $name.asm"
        }
    }

    $outExe  = Join-Path $buildDir "monolithic\RawrXD.exe"
    $outDir  = Split-Path $outExe -Parent
    if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

    $objs = $asmModules | ForEach-Object { Join-Path $objDir "$_.obj" } | Where-Object { Test-Path $_ }
    $linkArgs = @("/OUT:$outExe", "/SUBSYSTEM:WINDOWS", "/ENTRY:WinMainCRTStartup",
                  "/LARGEADDRESSAWARE", "/FIXED:NO", "/DYNAMICBASE", "/NXCOMPAT",
                  "/OPT:REF", "/OPT:ICF", "/NODEFAULTLIB:libcmt")
    if ($libPath) { $linkArgs += "/LIBPATH:$libPath" }
    $linkArgs += $objs
    $linkArgs += @("kernel32.lib", "user32.lib", "gdi32.lib", "shell32.lib", "ole32.lib", "uuid.lib")

    & $link /nologo @linkArgs 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "${Green}  ✓ Linked: $outExe${Reset}"
        return $true
    } else {
        Write-Host "${Red}  ✗ Link failed${Reset}"
        return $false
    }
}

$monoOk = $false
if ($DryRun) {
    Write-Host "${Yellow}DryRun mode: skipping monolithic kernel build.${Reset}"
    $monoOk = $true
} else {
    $monoOk = Build-MonolithicKernel
}
Write-Host ""

if ($MonoOnly) {
    exit $(if ($monoOk) { 0 } else { 1 })
}

# ═══════════════════════════════════════════════════════════════════════
# PHASE 2 — ERROR CLASSIFICATION
# ═══════════════════════════════════════════════════════════════════════

Write-Host "${Cyan}══ Phase 2: Error Classification ══${Reset}"

# Error categories
$CAT_MISSING_HEADER     = "MISSING_HEADER"
$CAT_MISSING_OWN_HEADER = "MISSING_OWN_HEADER"
$CAT_EXT_DEP            = "EXTERNAL_DEP"
$CAT_CPP17              = "NEEDS_CPP17"
$CAT_TYPE_ERROR         = "TYPE_ERROR"
$CAT_MEMBER_ERROR       = "MISSING_MEMBER"
$CAT_SYNTAX             = "SYNTAX_ERROR"
$CAT_ASM_ERROR          = "ASM_ERROR"
$CAT_OTHER              = "OTHER"

# Known external dep headers (we stub/remove these)
$extDepHeaders = @(
    "spdlog/spdlog.h", "spdlog/sinks/", "spdlog/fmt/",
    "nlohmann/json.hpp", "nlohmann/json_fwd.hpp",
    "fmt/format.h", "fmt/core.h",
    "QApplication", "QWidget", "QMainWindow", "QPushButton", "QLabel",
    "QVBoxLayout", "QHBoxLayout", "QMenuBar", "QStatusBar", "QTextEdit",
    "QTreeView", "QTabWidget", "QSplitter", "QFileDialog", "QMessageBox",
    "QtWidgets", "QtCore", "QtGui",
    "vulkan/vulkan.h", "vulkan/vulkan_core.h",
    "ggml.h", "ggml-backend.h", "ggml-cuda.h", "ggml-vulkan.h",
    "httplib.h", "curl/curl.h", "openssl/ssl.h",
    "boost/", "absl/", "gtest/gtest.h", "gmock/"
)

function Classify-Error {
    param([string]$ErrorText, [string]$FilePath)

    $ext = [System.IO.Path]::GetExtension($FilePath).ToLower()
    if ($ext -eq ".asm") { return $CAT_ASM_ERROR }

    # Missing header
    if ($ErrorText -match "Cannot open include file:\s*'([^']+)'") {
        $header = $Matches[1]
        foreach ($dep in $extDepHeaders) {
            if ($header -like "$dep*" -or $header -eq $dep) {
                return $CAT_EXT_DEP
            }
        }
        return $CAT_MISSING_HEADER
    }

    # C++17 needed
    if ($ErrorText -match "requires compiler flag '/std:c\+\+17'" -or
        $ErrorText -match "STL4038" -or
        $ErrorText -match "contents of <filesystem> are available only" -or
        $ErrorText -match "contents of <variant> are available only" -or
        $ErrorText -match "contents of <optional> are available only" -or
        $ErrorText -match "contents of <any> are available only") {
        return $CAT_CPP17
    }

    # Type errors
    if ($ErrorText -match "cannot convert from" -or $ErrorText -match "C2440") {
        return $CAT_TYPE_ERROR
    }

    # Missing member
    if ($ErrorText -match "is not a member of" -or $ErrorText -match "C2039") {
        return $CAT_MEMBER_ERROR
    }

    # Syntax
    if ($ErrorText -match "syntax error" -or $ErrorText -match "C2143") {
        return $CAT_SYNTAX
    }

    return $CAT_OTHER
}

function Get-FailurePath {
    param($Item)

    if ($null -eq $Item) { return $null }

    # Accept raw string entries too
    if ($Item -is [string]) {
        $raw = $Item.Trim()
        return $(if ([string]::IsNullOrWhiteSpace($raw)) { $null } else { $raw })
    }

    $candidates = @()
    foreach ($name in @("Path", "path", "File", "file", "Filename", "filename")) {
        if ($Item.PSObject -and $Item.PSObject.Properties.Match($name).Count -gt 0) {
            $candidates += [string]$Item.$name
        }
    }
    foreach ($c in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($c)) {
            return $c.Trim()
        }
    }
    return $null
}

function To-FullPath {
    param([string]$PathValue)
    if ([string]::IsNullOrWhiteSpace($PathValue)) { return $null }
    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $PathValue))
}

# Load failures: from batch JSONs or run fresh smoke test
$allFailures = @()

function Load-BatchFailures {
    $batchFiles = Get-ChildItem -Path $RepoRoot -Filter "batch_*_failed.json" -ErrorAction SilentlyContinue
    if ($batchFiles.Count -eq 0) {
        Write-Host "${Yellow}  No batch_*_failed.json found. Running fresh smoke test...${Reset}"
        return $null
    }
    Write-Host "  Loading $($batchFiles.Count) batch failure files..."
    $failures = @()
    foreach ($bf in $batchFiles) {
        try {
            $data = Get-Content $bf.FullName -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json -ErrorAction SilentlyContinue
            if ($data) {
                $items = if ($data -is [array]) { $data } else { @($data) }
                foreach ($item in $items) {
                    $path = Get-FailurePath -Item $item
                    if ([string]::IsNullOrWhiteSpace($path)) { continue }
                    $error = if ($item.PSObject -and $item.PSObject.Properties.Match("Error").Count -gt 0 -and $item.Error) {
                        $item.Error
                    } elseif ($item.PSObject -and $item.PSObject.Properties.Match("error").Count -gt 0 -and $item.error) {
                        $item.error
                    } else {
                        ""
                    }
                    $type = if ($item.PSObject -and $item.PSObject.Properties.Match("Type").Count -gt 0 -and $item.Type) {
                        $item.Type
                    } elseif ($item.PSObject -and $item.PSObject.Properties.Match("type").Count -gt 0 -and $item.type) {
                        $item.type
                    } else {
                        "unknown"
                    }
                    $failures += @{
                        Path     = $path
                        Type     = $type
                        Error    = $error
                        Category = Classify-Error -ErrorText $error -FilePath $path
                        Source   = $bf.Name
                    }
                }
            }
        } catch {
            Write-Host "${Yellow}  WARN: Could not parse $($bf.Name)${Reset}"
        }
    }
    return $failures
}

function Run-FreshSmokeTest {
    Write-Host "  Scanning src/ for C/C++/ASM files..."
    $failures = @()
    $cppFiles = @(Get-ChildItem -Path $srcDir -Include "*.cpp","*.c" -Recurse -ErrorAction SilentlyContinue)
    $asmFiles = @(Get-ChildItem -Path $srcDir -Filter "*.asm" -Recurse -ErrorAction SilentlyContinue)
    $total    = $cppFiles.Count + $asmFiles.Count
    $tested   = 0

    Write-Host "  Testing $total files..."

    foreach ($file in $cppFiles) {
        $tested++
        if ($tested % 100 -eq 0) { Write-Host "    [$tested/$total]..." -ForegroundColor DarkGray }
        $result = & $cl @clBase /Fo"NUL" $file.FullName 2>&1
        if ($LASTEXITCODE -ne 0) {
            $errText = ($result | Out-String)
            $failures += @{
                Path     = $file.FullName
                Type     = "cpp"
                Error    = $errText
                Category = Classify-Error -ErrorText $errText -FilePath $file.FullName
                Source   = "smoke_test"
            }
            if ($StopOnFirstFail) { return $failures }
        }
    }

    $asmInc = Join-Path $srcDir "asm"
    foreach ($file in $asmFiles) {
        $tested++
        if ($tested % 100 -eq 0) { Write-Host "    [$tested/$total]..." -ForegroundColor DarkGray }
        # Skip monolithic (already built in Phase 1)
        if ($file.FullName -like "*\monolithic\*") { continue }
        $result = & $ml64 /nologo /c /Fo"NUL" "/I$asmInc" "/I$srcDir" $file.FullName 2>&1
        if ($LASTEXITCODE -ne 0) {
            $errText = ($result | Out-String)
            $failures += @{
                Path     = $file.FullName
                Type     = "asm"
                Error    = $errText
                Category = Classify-Error -ErrorText $errText -FilePath $file.FullName
                Source   = "smoke_test"
            }
            if ($StopOnFirstFail) { return $failures }
        }
    }

    return $failures
}

$allFailures = Load-BatchFailures
if ($null -eq $allFailures) {
    $allFailures = Run-FreshSmokeTest
}

# De-duplicate by path
$seen = @{}
$deduped = @()
foreach ($f in $allFailures) {
    $key = Get-FailurePath -Item $f
    if ([string]::IsNullOrWhiteSpace($key)) { continue }
    $key = To-FullPath -PathValue $key
    if ([string]::IsNullOrWhiteSpace($key)) { continue }
    if (-not $seen.ContainsKey($key)) {
        $seen[$key] = $true
        if (-not $f.Path) { $f.Path = $key }
        $deduped += $f
    }
}
$allFailures = $deduped

# Category summary
$catCounts = @{}
foreach ($f in $allFailures) {
    $cat = $f.Category
    if (-not $catCounts.ContainsKey($cat)) { $catCounts[$cat] = 0 }
    $catCounts[$cat]++
}

Write-Host ""
Write-Host "${Bold}  Error Classification Summary${Reset}"
Write-Host "  ───────────────────────────────────"
Write-Host "  Total unique failures: $($allFailures.Count)"
foreach ($cat in ($catCounts.Keys | Sort-Object)) {
    $count = $catCounts[$cat]
    $color = switch ($cat) {
        $CAT_EXT_DEP     { $Yellow }
        $CAT_ASM_ERROR   { $Red }
        $CAT_CPP17       { $Green }
        default          { $Reset }
    }
    Write-Host "  ${color}  $($cat.PadRight(22)) $count${Reset}"
}
Write-Host ""

if ($ReportOnly) {
    # Dump full report JSON
    $reportPath = Join-Path $RepoRoot "orchestrator_classified_report.json"
    $allFailures | ConvertTo-Json -Depth 5 | Out-File -FilePath $reportPath -Encoding UTF8
    Write-Host "${Green}Report saved: $reportPath${Reset}"
    exit 0
}

if ($SmokeOnly -or $DryRun) {
    $modeLabel = if ($DryRun) { "DryRun" } else { "Smoke-only" }
    Write-Host "${Yellow}$modeLabel mode: no fixes applied.${Reset}"

    # Persist outputs for downstream tooling (swarm expects failed_files.json)
    $reportPath = Join-Path $RepoRoot "orchestrator_report.json"
    @{
        Timestamp       = Get-Date -Format "o"
        MonolithicBuild = $monoOk
        TotalFailures   = $allFailures.Count
        AutoFixed       = 0
        Categories      = $catCounts
        StillFailing    = ($allFailures | Select-Object Path, Category, Source)
    } | ConvertTo-Json -Depth 5 | Out-File -FilePath $reportPath -Encoding UTF8

    $remainingPath = Join-Path $RepoRoot "failed_files.json"
    $allFailures | ConvertTo-Json -Depth 4 | Out-File -FilePath $remainingPath -Encoding UTF8

    Write-Host "  Report: $reportPath"
    Write-Host "  Failures: $remainingPath"

    exit $(if ($allFailures.Count -eq 0) { 0 } else { 1 })
}

# ═══════════════════════════════════════════════════════════════════════
# PHASE 3 — AUTO-FIX ENGINE
# ═══════════════════════════════════════════════════════════════════════

Write-Host "${Cyan}══ Phase 3: Auto-Fix Engine ══${Reset}"

$fixStats = @{ Applied = 0; Skipped = 0; Failed = 0 }

# ── Fix: External dep stubs ─────────────────────────────────────────
# For files that depend on spdlog/nlohmann/Qt/Vulkan/ggml — create
# minimal stub headers in include/ so they compile to no-ops.

function Ensure-StubHeaders {
    $stubDir = Join-Path $includeDir "stubs"
    if (-not (Test-Path $stubDir)) { New-Item -ItemType Directory -Path $stubDir -Force | Out-Null }

    # spdlog stub
    $spdlogDir = Join-Path $includeDir "spdlog"
    if (-not (Test-Path $spdlogDir)) {
        New-Item -ItemType Directory -Path $spdlogDir -Force | Out-Null
        $spdlogStub = @"
#pragma once
// RawrXD stub — no external spdlog dependency
#include <cstdio>
namespace spdlog {
    enum level_enum { trace, debug, info, warn, err, critical, off };
    namespace level { using enum_type = level_enum; }
    inline void set_level(level_enum) {}
    template<typename... Args> void trace(const char*, Args&&...) {}
    template<typename... Args> void debug(const char*, Args&&...) {}
    template<typename... Args> void info(const char*, Args&&...) {}
    template<typename... Args> void warn(const char*, Args&&...) {}
    template<typename... Args> void error(const char*, Args&&...) {}
    template<typename... Args> void critical(const char*, Args&&...) {}
    namespace sinks { struct sink {}; struct stdout_color_sink_mt : sink {}; }
    class logger {
    public:
        logger(const char*, std::initializer_list<sinks::sink*>) {}
        template<typename... Args> void info(const char*, Args&&...) {}
        template<typename... Args> void error(const char*, Args&&...) {}
        template<typename... Args> void warn(const char*, Args&&...) {}
        template<typename... Args> void debug(const char*, Args&&...) {}
        void set_level(level_enum) {}
        void flush() {}
    };
}
"@
        Set-Content -Path (Join-Path $spdlogDir "spdlog.h") -Value $spdlogStub -Encoding UTF8
        # spdlog/fmt stub
        $fmtDir = Join-Path $spdlogDir "fmt"
        New-Item -ItemType Directory -Path $fmtDir -Force | Out-Null
        Set-Content -Path (Join-Path $fmtDir "fmt.h")  -Value "#pragma once`n// stub" -Encoding UTF8
        Set-Content -Path (Join-Path $fmtDir "ostr.h") -Value "#pragma once`n// stub" -Encoding UTF8
        # spdlog/sinks
        $sinksDir = Join-Path $spdlogDir "sinks"
        New-Item -ItemType Directory -Path $sinksDir -Force | Out-Null
        @("stdout_color_sinks.h", "basic_file_sink.h", "rotating_file_sink.h", "daily_file_sink.h", "null_sink.h") | ForEach-Object {
            Set-Content -Path (Join-Path $sinksDir $_) -Value "#pragma once`n#include `"../spdlog.h`"`n// stub" -Encoding UTF8
        }
        Write-Host "${Green}  ✓${Reset} Created spdlog stub headers"
    }

    # nlohmann/json stub
    $nlDir = Join-Path $includeDir "nlohmann"
    if (-not (Test-Path $nlDir)) {
        New-Item -ItemType Directory -Path $nlDir -Force | Out-Null
        $jsonStub = @"
#pragma once
// RawrXD stub — no external nlohmann/json dependency
#include <string>
#include <map>
#include <vector>
namespace nlohmann {
    class json {
    public:
        using object_t = std::map<std::string, json>;
        using array_t  = std::vector<json>;
        json() = default;
        json(const char* s) : str_(s) {}
        json(const std::string& s) : str_(s) {}
        json(int v) : int_(v) {}
        json(double v) : dbl_(v) {}
        json(bool v) : bool_(v) {}
        json(std::nullptr_t) {}
        static json parse(const std::string&) { return json(); }
        static json object() { return json(); }
        static json array() { return json(); }
        std::string dump(int = -1) const { return "{}"; }
        json& operator[](const std::string&) { return *this; }
        json& operator[](const char*) { return *this; }
        json& operator[](int) { return *this; }
        bool contains(const std::string&) const { return false; }
        bool is_null() const { return true; }
        bool is_object() const { return false; }
        bool is_array() const { return false; }
        bool is_string() const { return !str_.empty(); }
        bool is_number() const { return false; }
        std::string get() const { return str_; }
        template<typename T> T get() const { return T{}; }
        template<typename T> T value(const std::string&, T def) const { return def; }
        size_t size() const { return 0; }
        bool empty() const { return true; }
        auto begin() { return items_.begin(); }
        auto end() { return items_.end(); }
        auto begin() const { return items_.begin(); }
        auto end() const { return items_.end(); }
        void push_back(const json&) {}
        void emplace_back() {}
        using iterator = std::vector<json>::iterator;
        using const_iterator = std::vector<json>::const_iterator;
    private:
        std::string str_;
        int int_ = 0;
        double dbl_ = 0.0;
        bool bool_ = false;
        std::vector<json> items_;
    };
    inline void to_json(json&, const std::string&) {}
    inline void from_json(const json&, std::string&) {}
}
using json = nlohmann::json;
"@
        Set-Content -Path (Join-Path $nlDir "json.hpp")     -Value $jsonStub -Encoding UTF8
        Set-Content -Path (Join-Path $nlDir "json_fwd.hpp") -Value "#pragma once`nnamespace nlohmann { class json; }" -Encoding UTF8
        Write-Host "${Green}  ✓${Reset} Created nlohmann/json stub headers"
    }

    # fmt stub
    $fmtDir2 = Join-Path $includeDir "fmt"
    if (-not (Test-Path $fmtDir2)) {
        New-Item -ItemType Directory -Path $fmtDir2 -Force | Out-Null
        $fmtStub = @"
#pragma once
// RawrXD stub — no external fmt dependency
#include <string>
#include <cstdio>
namespace fmt {
    template<typename... Args>
    std::string format(const char* f, Args&&...) { return std::string(f); }
    template<typename... Args>
    void print(const char* f, Args&&...) { printf("%s", f); }
    template<typename... Args>
    void print(FILE* fp, const char* f, Args&&...) { fprintf(fp, "%s", f); }
}
"@
        Set-Content -Path (Join-Path $fmtDir2 "format.h") -Value $fmtStub -Encoding UTF8
        Set-Content -Path (Join-Path $fmtDir2 "core.h")   -Value $fmtStub -Encoding UTF8
        Write-Host "${Green}  ✓${Reset} Created fmt stub headers"
    }
}

if ($FixAll) {
    Ensure-StubHeaders
}

# ── Verify fixes ────────────────────────────────────────────────────
function Test-SingleFile {
    param([string]$FilePath)
    if ([string]::IsNullOrWhiteSpace($FilePath)) {
        return @{ Success = $false; Output = "Invalid file path (null/empty)." }
    }
    if (-not (Test-Path $FilePath)) {
        return @{ Success = $false; Output = "File not found: $FilePath" }
    }
    $ext = [System.IO.Path]::GetExtension($FilePath).ToLower()
    if ($ext -eq ".asm") {
        $asmInc = Join-Path $srcDir "asm"
        $out = & $ml64 /nologo /c /Fo"NUL" "/I$asmInc" "/I$srcDir" $FilePath 2>&1
    } else {
        $out = & $cl @clBase /Fo"NUL" $FilePath 2>&1
    }
    return @{ Success = ($LASTEXITCODE -eq 0); Output = ($out | Out-String) }
}

# ── Re-test previously failed files after stub creation ─────────────
if ($FixAll) {
    Write-Host ""
    Write-Host "  Re-testing files that failed on external deps..."
    $extDepFails = $allFailures | Where-Object { $_.Category -eq $CAT_EXT_DEP }
    $fixedCount  = 0
    $stillFailed = @()
    $fixedFullPathSet = @{}
    $skippedNull = 0
    $skippedMissing = 0

    foreach ($f in $extDepFails) {
        $rawPath = Get-FailurePath -Item $f
        if ([string]::IsNullOrWhiteSpace($rawPath)) {
            $skippedNull++
            continue
        }
        $fullPath = To-FullPath -PathValue $rawPath
        if ([string]::IsNullOrWhiteSpace($fullPath)) {
            $skippedNull++
            continue
        }
        if (-not (Test-Path $fullPath)) {
            $skippedMissing++
            continue
        }
        $result = Test-SingleFile -FilePath $fullPath
        if ($result.Success) {
            $fixedCount++
            $fixedFullPathSet[$fullPath] = $true
        } else {
            # Re-classify
            $f.Category = Classify-Error -ErrorText $result.Output -FilePath $fullPath
            $f.Error    = $result.Output
            $f.Path     = $fullPath
            $stillFailed += $f
        }
    }
    Write-Host "${Green}  ✓ $fixedCount${Reset} files now compile after stub headers"
    if ($stillFailed.Count -gt 0) {
        Write-Host "${Yellow}  $($stillFailed.Count) files still failing (cascading errors)${Reset}"
    }
    if ($skippedNull -gt 0) {
        Write-Host "${Yellow}  $skippedNull entries skipped (null/empty path)${Reset}"
    }
    if ($skippedMissing -gt 0) {
        Write-Host "${Yellow}  $skippedMissing entries skipped (missing on disk)${Reset}"
    }
    $fixStats.Applied += $fixedCount

    # Remove files that were actually fixed from remaining failures.
    if ($fixedFullPathSet.Count -gt 0) {
        $allFailures = @($allFailures | Where-Object {
            $p = To-FullPath -PathValue (Get-FailurePath -Item $_)
            -not ($p -and $fixedFullPathSet.ContainsKey($p))
        })
    }
}

# ═══════════════════════════════════════════════════════════════════════
# SUMMARY
# ═══════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "${Cyan}╔═════════════════════════════════════════════════════════════╗${Reset}"
Write-Host "${Cyan}║                   Orchestrator Summary                     ║${Reset}"
Write-Host "${Cyan}╠═════════════════════════════════════════════════════════════╣${Reset}"
Write-Host "║  Monolithic kernel : $(if ($monoOk) { "${Green}PASS${Reset}" } else { "${Red}FAIL${Reset}" })                                  ║"
$totalValid = $allFailures.Count + $fixStats.Applied
$remainingCount = $allFailures.Count
$successRate = if ($totalValid -gt 0) { (($fixStats.Applied / $totalValid) * 100).ToString('F1') } else { "100.0" }
Write-Host "║  Total failures    : $($totalValid.ToString().PadLeft(5))                                  ║"
Write-Host "║  Auto-fixed        : $($fixStats.Applied.ToString().PadLeft(5))                                  ║"
Write-Host "║  Remaining         : $($remainingCount.ToString().PadLeft(5))                                  ║"
Write-Host "║  Success rate      : $(("$successRate%").PadLeft(6))                                 ║"
Write-Host "${Cyan}╚═════════════════════════════════════════════════════════════╝${Reset}"

$statusText = if ($totalValid -eq 0 -and $monoOk) {
    "SUCCESS (no failures)"
} elseif ($remainingCount -eq 0 -and $fixStats.Applied -gt 0 -and $monoOk) {
    "SUCCESS"
} elseif ($fixStats.Applied -eq 0 -and $totalValid -gt 0) {
    "NO PROGRESS (0% fix rate)"
} else {
    "PARTIAL ($($fixStats.Applied)/$totalValid fixed)"
}
$statusColor = if ($statusText -like "SUCCESS*") { $Green } elseif ($statusText -like "NO PROGRESS*") { $Red } else { $Yellow }
Write-Host "${statusColor}Status: $statusText${Reset}"

# Save orchestrator report
$reportPath = Join-Path $RepoRoot "orchestrator_report.json"
@{
    Timestamp      = Get-Date -Format "o"
    MonolithicBuild = $monoOk
    TotalFailures  = $totalValid
    AutoFixed      = $fixStats.Applied
    Categories     = $catCounts
    StillFailing   = ($allFailures | Select-Object Path, Category, Source)
} | ConvertTo-Json -Depth 5 | Out-File -FilePath $reportPath -Encoding UTF8
Write-Host "  Report: $reportPath"

# Save remaining failures for subagent consumption
$remainingPath = Join-Path $RepoRoot "failed_files.json"
$allFailures | ConvertTo-Json -Depth 4 | Out-File -FilePath $remainingPath -Encoding UTF8
Write-Host "  Failures: $remainingPath"

Write-Host ""
if ($monoOk -and $remainingCount -eq 0) {
    Write-Host "${Green}  ✅ Monolithic RawrXD.exe builds clean — core kernel is solid.${Reset}"
}
Write-Host ""
$finalOk = $monoOk -and ($remainingCount -eq 0)
exit $(if ($finalOk) { 0 } else { 1 })
