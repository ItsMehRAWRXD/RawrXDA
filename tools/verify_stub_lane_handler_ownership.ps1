param(
    [string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$BuildDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-DumpbinPath() {
    $cmd = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        "C:/VS2022Enterprise/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe",
        "C:/Program Files/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe",
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"
    )

    $hits = @()
    foreach ($pattern in $candidates) {
        $hits += Get-ChildItem -Path $pattern -File -ErrorAction SilentlyContinue
    }
    if ($hits.Count -eq 0) {
        throw "dumpbin.exe not found. Run from a VS Developer shell or install MSVC build tools."
    }

    return ($hits | Sort-Object FullName -Descending | Select-Object -First 1).FullName
}

function Get-HandlerNames([string]$path, [string]$pattern) {
    if (!(Test-Path $path)) {
        throw "Missing required file: $path"
    }

    $matches = Select-String -Path $path -Pattern $pattern -AllMatches
    $names = @()
    foreach ($m in $matches) {
        foreach ($g in $m.Matches) {
            $names += $g.Groups[1].Value
        }
    }
    return $names | Sort-Object -Unique
}

$providerPath = Join-Path $RepoRoot "src/core/ssot_missing_handlers_provider.cpp"

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot "build"
}

$stubsObj = Join-Path $BuildDir "CMakeFiles/RawrXD-Win32IDE.dir/src/core/missing_handler_stubs.cpp.obj"
$providerObj = Join-Path $BuildDir "CMakeFiles/RawrXD-Win32IDE.dir/src/core/ssot_missing_handlers_provider.cpp.obj"

foreach ($p in @($stubsObj, $providerObj)) {
    if (!(Test-Path $p)) {
        throw "Missing object file: $p (build RawrXD-Win32IDE first)"
    }
}

function Get-DefinedHandlersFromObj([string]$objPath) {
    $dumpbin = Resolve-DumpbinPath
    $lines = & $dumpbin /symbols $objPath
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin failed for $objPath"
    }

    $names = @()
    foreach ($line in $lines) {
        if ($line -match "UNDEF") { continue }
        $m = [regex]::Match($line, "\?handle([A-Za-z0-9_]+)@@")
        if ($m.Success) {
            $names += "handle$($m.Groups[1].Value)"
        }
    }
    return $names | Sort-Object -Unique
}

$stubHandlers = Get-DefinedHandlersFromObj -objPath $stubsObj
$providerHandlersDefined = Get-DefinedHandlersFromObj -objPath $providerObj
$providerHandlersListed = Get-HandlerNames -path $providerPath -pattern 'X\((handle[A-Za-z0-9_]+)\)'

$providerSet = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($name in $providerHandlersDefined) { [void]$providerSet.Add($name) }

$overlap = @()
foreach ($name in $stubHandlers) {
    if ($providerSet.Contains($name)) {
        $overlap += $name
    }
}

if ($overlap.Count -gt 0) {
    throw "Stub lane overlap detected between missing_handler_stubs.cpp and ssot_missing_handlers_provider.cpp: $($overlap -join ', ')"
}

if ($providerHandlersListed.Count -ne 116) {
    throw "Expected 116 provider handlers, found $($providerHandlersListed.Count)"
}

Write-Host "SSOT stub-lane ownership verify OK: stubs_defined=$($stubHandlers.Count) provider_defined=$($providerHandlersDefined.Count) provider_listed=$($providerHandlersListed.Count) overlap=0"
