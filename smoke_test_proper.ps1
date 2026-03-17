# Proper smoke test using the same flags as RawrXD-Build.ps1
# Compiles every .cpp/.c file individually with correct include paths
# Continues past failures, generates a proper report

$ErrorActionPreference = "SilentlyContinue"
$root = "d:\rawrxd"

# ── Setup vcvars64 ──
$vsPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
if (-not (Test-Path $vsPath)) {
    $vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
        -latest -products * -property installationPath 2>$null
}
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { Write-Error "vcvars64.bat not found"; exit 1 }

$tmp = [IO.Path]::GetTempFileName()
cmd /c "`"$vcvars`" && set > `"$tmp`"" 2>$null
Get-Content $tmp | ForEach-Object { if ($_ -match "^(.*?)=(.*)$") { [Environment]::SetEnvironmentVariable($matches[1], $matches[2]) } }
Remove-Item $tmp -Force

$cl = (Get-Command cl.exe -ErrorAction Stop).Source
Write-Host "[+] cl.exe: $cl" -ForegroundColor Green

# ── Build include flags (matching RawrXD-Build.ps1) ──
$iFlags = @(
    "/I$root\src",
    "/I$root\include", 
    "/I$root\3rdparty",
    "/I$root\3rdparty\ggml\include"
)

# Windows SDK includes
$sdkDir = $env:WindowsSdkDir
if (-not $sdkDir) { $sdkDir = "${env:ProgramFiles(x86)}\Windows Kits\10\" }
$sdkVer = $env:WindowsSDKVersion
if (-not $sdkVer) {
    $sdkVer = (Get-ChildItem "$sdkDir\include" -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } | Sort-Object Name -Descending | Select-Object -First 1).Name
    if ($sdkVer) { $sdkVer = "$sdkVer\" }
}
if ($sdkDir -and $sdkVer) {
    $sdkInc = Join-Path $sdkDir "Include\$sdkVer"
    foreach ($sub in @("um","shared","ucrt","winrt")) {
        $p = Join-Path $sdkInc $sub
        if (Test-Path $p) { $iFlags += "/I$p" }
    }
}

# ── Compiler flags ──
$baseFlags = @(
    "/c", "/std:c++20", "/EHsc", "/nologo", "/MP",
    "/O2", "/MD", "/DNDEBUG", "/DRELEASE",
    "/DRAWXD_BUILD", "/DUNICODE", "/D_UNICODE", "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX",
    "/w"  # suppress warnings to reduce noise — we only care about errors
)
$allFlags = $baseFlags + $iFlags

# ── Gather source files ──
$sources = @()
$sources += Get-ChildItem "$root\src" -Filter "*.cpp" -Recurse
$sources += Get-ChildItem "$root\src" -Filter "*.c" -Recurse
$sources += Get-ChildItem "$root\core" -Filter "*.cpp" -Recurse -ErrorAction SilentlyContinue
$sources += Get-ChildItem "$root\core" -Filter "*.c" -Recurse -ErrorAction SilentlyContinue

$total = $sources.Count
$passed = 0
$failed = 0
$failedList = @()

Write-Host "`n=== RawrXD Proper Smoke Test ===" -ForegroundColor Cyan
Write-Host "Files: $total | Flags: $($allFlags.Count)" -ForegroundColor Gray

$objDir = Join-Path $root "temp_smoke_obj"
$null = New-Item $objDir -ItemType Directory -Force

$i = 0
foreach ($src in $sources) {
    $i++
    $obj = Join-Path $objDir "$($src.BaseName).obj"
    
    $result = & $cl $allFlags "/Fo$obj" $src.FullName 2>&1
    $exitCode = $LASTEXITCODE
    
    # Check for actual errors (not just warnings)
    $hasError = ($result | Where-Object { $_ -match ': (fatal )?error C\d+' }) -ne $null
    
    if ($exitCode -eq 0 -and -not $hasError) {
        $passed++
    } else {
        $failed++
        $errorLines = ($result | Where-Object { $_ -match ': (fatal )?error C\d+' }) -join "`n"
        $failedList += [pscustomobject]@{
            Path = $src.FullName
            Name = $src.Name
            Error = $errorLines
        }
        Write-Host "  FAIL: $($src.Name)" -ForegroundColor Red
    }
    
    if ($i % 50 -eq 0) {
        Write-Host "  [$i/$total] pass=$passed fail=$failed" -ForegroundColor Gray
    }
}

# ── Report ──
Write-Host "`n=== RESULTS ===" -ForegroundColor Cyan
Write-Host "Total:  $total" -ForegroundColor White
Write-Host "Passed: $passed" -ForegroundColor Green  
Write-Host "Failed: $failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
Write-Host "Rate:   $(($passed / [Math]::Max(1,$total) * 100).ToString('F1'))%" -ForegroundColor Yellow

if ($failedList.Count -gt 0) {
    Write-Host "`nFailed files:" -ForegroundColor Red
    $failedList | ForEach-Object {
        Write-Host "  $($_.Name)" -ForegroundColor Red
        Write-Host "    $($_.Error)" -ForegroundColor DarkRed
    }
    
    # Save JSON report
    $failedList | ConvertTo-Json -Depth 3 | Out-File "$root\smoke_test_proper_results.json" -Encoding UTF8
    Write-Host "`nReport: $root\smoke_test_proper_results.json" -ForegroundColor Cyan
}

# Cleanup
Remove-Item $objDir -Force -Recurse -ErrorAction SilentlyContinue
