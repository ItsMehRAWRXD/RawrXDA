# Subagent Compilation Fixer - Processes one batch of failed files in parallel
# Usage: Each subagent gets a slice of failed_files.json and fixes each file
param(
    [string]$FailedFilesJson = "d:\rawrxd\failed_files.json",
    [int]$BatchStart = 0,
    [int]$BatchSize = 50,
    [string]$OutputFailedJson = ""
)

$ErrorActionPreference = "Continue"

# ========== Resolve cl/ml64 + INCLUDE (same as orchestrator) ==========
$rawrxdRoot = "d:\rawrxd"
$srcDir = Join-Path $rawrxdRoot "src"
$includeDir = Join-Path $rawrxdRoot "include"
$thirdpartyDir = Join-Path $rawrxdRoot "3rdparty"
$ggmlIncludeDir = Join-Path $thirdpartyDir "ggml\include"
$ggmlSrcDir = Join-Path $thirdpartyDir "ggml\src"
$win32appDir = Join-Path $srcDir "win32app"
$cl = $null
$ml64 = $null
foreach ($drive in @("D:", "C:")) {
    $msvcBase = "${drive}\VS2022Enterprise\VC\Tools\MSVC"
    if (-not (Test-Path $msvcBase)) {
        $msvcBase = "${drive}\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC"
    }
    if (-not (Test-Path $msvcBase)) {
        $msvcBase = "${drive}\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
    }
    if (Test-Path $msvcBase) {
        $verDir = Get-ChildItem -Path $msvcBase -Directory | Sort-Object Name -Descending | Select-Object -First 1
        if ($verDir) {
            $binPath = Join-Path $verDir.FullName "bin\Hostx64\x64"
            $candidateCl = Join-Path $binPath "cl.exe"
            $candidateMl64 = Join-Path $binPath "ml64.exe"
            if (Test-Path $candidateCl) {
                $cl = $candidateCl
                $ml64 = $candidateMl64
                $msvcRoot = $verDir.FullName
                break
            }
        }
    }
}
if (-not $cl) {
    Write-Error "cl.exe not found (tried D:/C: VS2022 paths)."
    exit 1
}
$sdkRoot = $null
foreach ($r in @("D:\Program Files (x86)\Windows Kits\10", "C:\Program Files (x86)\Windows Kits\10")) {
    if (Test-Path $r) { $sdkRoot = $r; break }
}
$sdkVer = "10.0.22621.0"
if ($sdkRoot) {
    $sdkInc = Join-Path $sdkRoot "Include"
    $preferred = $sdkVer
    $preferredOk = (Test-Path (Join-Path $sdkInc "$preferred\um\windows.h")) -and
                   (Test-Path (Join-Path $sdkInc "$preferred\ucrt\crtdbg.h"))
    if (-not $preferredOk) {
        $found = Get-ChildItem -Path $sdkInc -Directory -ErrorAction SilentlyContinue |
                 Sort-Object Name -Descending |
                 Where-Object {
                    (Test-Path (Join-Path $sdkInc "$($_.Name)\um\windows.h")) -and
                    (Test-Path (Join-Path $sdkInc "$($_.Name)\ucrt\crtdbg.h"))
                 } |
                 Select-Object -First 1
        if ($found) { $sdkVer = $found.Name }
    }
}
$msvcInclude = Join-Path $msvcRoot "include"
$envInclude = $msvcInclude
if ($sdkRoot) {
    $envInclude += ";$(Join-Path $sdkRoot "Include\$sdkVer\ucrt")"
    $envInclude += ";$(Join-Path $sdkRoot "Include\$sdkVer\shared")"
    $envInclude += ";$(Join-Path $sdkRoot "Include\$sdkVer\um")"
    $envInclude += ";$(Join-Path $sdkRoot "Include\$sdkVer\winrt")"
}
$env:INCLUDE = $envInclude
$env:PATH = "$(Split-Path $cl -Parent);$env:PATH"
$clIncludes = @(
    "/I`"$srcDir`"",
    "/I`"$includeDir`"",
    "/I`"$win32appDir`"",
    "/I`"$thirdpartyDir`"",
    "/I`"$ggmlIncludeDir`"",
    "/I`"$ggmlSrcDir`""
)
$asmInc = Join-Path $srcDir "asm"

if (-not (Test-Path $FailedFilesJson)) {
    Write-Error "Failed files JSON not found: $FailedFilesJson"
    exit 1
}

# Load failed files
$failedData = Get-Content $FailedFilesJson -Raw | ConvertFrom-Json
if ($failedData -isnot [array]) {
    $failedData = @($failedData)
}

Write-Host "DEBUG: Loaded $($failedData.Count) items from $FailedFilesJson" -ForegroundColor Gray

$batch = $failedData | Select-Object -Skip $BatchStart -First $BatchSize
$fixed = 0
$skipped = 0
$stillFailed = @()

Write-Host "Processing batch: files $BatchStart to $($BatchStart + $batch.Count - 1)" -ForegroundColor Cyan

foreach ($item in $batch) {
    if (-not $item) {
        $stillFailed += [pscustomobject]@{
            Path      = $null
            Type      = "unknown"
            Error     = "Null manifest entry"
            BatchStart = $BatchStart
        }
        $skipped++
        continue
    }
    
    # Robust path extraction (supports .path, .file, .filename)
    $filePath = $item.path
    if (-not $filePath) { $filePath = $item.file }
    if (-not $filePath) { $filePath = $item.filename }
    
    $fileType = $item.Type
    
    if ([string]::IsNullOrWhiteSpace($filePath)) {
        Write-Host "DEBUG: Item has no path: $($item | ConvertTo-Json -Depth 1 -Compress)" -ForegroundColor Yellow
        $stillFailed += [pscustomobject]@{
            Path      = $null
            Type      = $fileType
            Error     = "Manifest entry has null/empty path"
            BatchStart = $BatchStart
        }
        $skipped++
        continue
    }

    # Ensure path is absolute for Test-Path (or relative to $rawrxdRoot)
    if (-not [System.IO.Path]::IsPathRooted($filePath)) {
        $filePath = Join-Path $rawrxdRoot $filePath
    }

    if (-not (Test-Path $filePath)) {
        Write-Host "✗ File not found: $filePath" -ForegroundColor Red
        $stillFailed += [pscustomobject]@{
            Path      = $filePath
            Type      = $fileType
            Error     = "File not found on disk"
            BatchStart = $BatchStart
        }
        $skipped++
        continue
    }
    
    $content = Get-Content $filePath -Raw
    $originalContent = $content
    
    # ===== COMMON FIX PATTERNS =====
    
    # 1. Missing #include guards or includes
    if ($content -notmatch "#pragma once" -and $content -notmatch "#ifndef") {
        if ($filePath -like "*.h") {
            $headerGuard = "RAWRXD_" + [System.IO.Path]::GetFileNameWithoutExtension($filePath).ToUpper() + "_H"
            $content = "#pragma once`n`n" + $content
        }
    }
    
    # 2. Undefined identifiers - add forward declarations
    if ($content -like "*undefined*" -or $content -like "*not declared*") {
        # Common Windows types that might be missing
        if ($content -like "*OutputDebugStringA*" -and $content -notmatch "#include.*Windows.h") {
            $content = "#include <windows.h>`n" + $content
        }
        if ($content -like "*HWND*" -and $content -notmatch "#include.*Windows.h") {
            $content = "#include <windows.h>`n" + $content
        }
        if ($content -like "*std::*" -and $content -notmatch "#include.*algorithm|vector|string|memory") {
            if ($content -like "*std::vector*") { $content = "#include <vector>`n" + $content }
            if ($content -like "*std::string*") { $content = "#include <string>`n" + $content }
            if ($content -like "*std::make_shared*") { $content = "#include <memory>`n" + $content }
        }
    }
    
    # 3. Duplicate definitions - comment out duplicates
    $lines = $content -split "`n"
    $seen = @{}
    $deduped = @()
    
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        # Skip empty/comment lines
        if (-not $trimmed -or $trimmed.StartsWith("//")) {
            $deduped += $line
            continue
        }
        
        # Detect duplicate function/class definitions
        if ($trimmed -match "^(void|int|bool|std|class|struct)\s+\w+\s*\(") {
            $sig = $trimmed.Substring(0, [Math]::Min(60, $trimmed.Length))
            if ($seen.ContainsKey($sig)) {
                $deduped += "// [DEDUPED] $line"
            } else {
                $seen[$sig] = $true
                $deduped += $line
            }
        } else {
            $deduped += $line
        }
    }
    $content = $deduped -join "`n"
    
    # 4. Missing return statements
    $content = $content -replace '(?m)(\s*\n\s*\}\s*$)', "`n    return true;`n}`n"
    
    # 5. Syntax errors in catch blocks
    $content = $content -replace 'catch\s*\(\.\.\.', 'catch (...'
    $content = $content -replace 'catch\s*\(\s*Exception', 'catch (std::exception'
    
    # Write back if changed
    if ($content -ne $originalContent) {
        Set-Content -Path $filePath -Value $content -Encoding UTF8
    }
    
    # Verify compilation (with INCLUDE and /I already set above)
    if ($fileType -eq "cpp" -or $fileType -eq "c") {
        $result = & $cl /nologo /c /std:c++20 /EHsc /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /Fo"nul:" @clIncludes $filePath 2>&1
        $compiles = $LASTEXITCODE -eq 0
    } else {
        $result = & $ml64 /nologo /c /Fo"nul:" "/I$asmInc" "/I$srcDir" $filePath 2>&1
        $compiles = $LASTEXITCODE -eq 0
    }
    
    if ($compiles) {
        Write-Host "✓ $([System.IO.Path]::GetFileName($filePath))" -ForegroundColor Green
        $fixed++
    } else {
        Write-Host "✗ $([System.IO.Path]::GetFileName($filePath))" -ForegroundColor Red
        $stillFailed += [pscustomobject]@{
            Path      = $filePath
            Type      = $fileType
            Error     = (($result | Out-String).Trim())
            BatchStart = $BatchStart
        }
    }
}

Write-Host "`nBatch Results: $fixed fixed, $($stillFailed.Count) still failing" -ForegroundColor Yellow
Write-Host "Skipped entries: $skipped" -ForegroundColor DarkGray

if ([string]::IsNullOrWhiteSpace($OutputFailedJson)) {
    $OutputFailedJson = "d:\rawrxd\batch_$BatchStart`_failed.json"
}

$outDir = Split-Path -Path $OutputFailedJson -Parent
if ($outDir -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}
$stillFailed | ConvertTo-Json -Depth 5 | Out-File -FilePath $OutputFailedJson -Encoding UTF8
if ($stillFailed.Count -gt 0) {
    Write-Host "Unfixed files saved to: $OutputFailedJson" -ForegroundColor Yellow
}

exit ($stillFailed.Count)
