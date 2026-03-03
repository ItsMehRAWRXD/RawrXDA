param(
    [string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$BuildDir = "",
    [string[]]$Objects = @()
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

function Resolve-ObjectPath([string[]]$candidates, [string]$leafName, [string]$missingMessage) {
    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        $expanded = [Environment]::ExpandEnvironmentVariables($candidate)
        $fullPath = $null
        try {
            $fullPath = (Resolve-Path -Path $expanded -ErrorAction Stop).Path
        } catch {
            continue
        }
        if ([IO.Path]::GetFileName($fullPath) -ieq $leafName) {
            return $fullPath
        }
    }
    throw $missingMessage
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

$providerPath = Join-Path $RepoRoot "src/core/ssot_missing_handlers_provider.cpp"
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot "build"
}

$fallbackStubsObjCandidates = @(
    (Join-Path $BuildDir "CMakeFiles/ssot_stub_lane_objs.dir/src/core/missing_handler_stubs.cpp.obj"),
    (Join-Path $BuildDir "CMakeFiles/ssot_stub_lane_objs.dir/./src/core/missing_handler_stubs.cpp.obj"),
    (Join-Path $BuildDir "CMakeFiles/RawrXD-Win32IDE.dir/src/core/missing_handler_stubs.cpp.obj"),
    (Join-Path $BuildDir "CMakeFiles/RawrXD-Win32IDE.dir/./src/core/missing_handler_stubs.cpp.obj")
)
$fallbackProviderObjCandidates = @(
    (Join-Path $BuildDir "CMakeFiles/ssot_stub_lane_objs.dir/src/core/ssot_missing_handlers_provider.cpp.obj"),
    (Join-Path $BuildDir "CMakeFiles/ssot_stub_lane_objs.dir/./src/core/ssot_missing_handlers_provider.cpp.obj"),
    (Join-Path $BuildDir "CMakeFiles/RawrXD-Win32IDE.dir/src/core/ssot_missing_handlers_provider.cpp.obj"),
    (Join-Path $BuildDir "CMakeFiles/RawrXD-Win32IDE.dir/./src/core/ssot_missing_handlers_provider.cpp.obj")
)
$resolvedObjects = @()
foreach ($obj in $Objects) {
    if ([string]::IsNullOrWhiteSpace($obj)) { continue }
    $expanded = [Environment]::ExpandEnvironmentVariables($obj)
    if (Test-Path -Path $expanded) {
        $resolvedObjects += (Resolve-Path -Path $expanded).Path
    }
}
if ($resolvedObjects.Count -eq 0) {
    $resolvedObjects = @($fallbackStubsObjCandidates + $fallbackProviderObjCandidates)
}

try {
    $stubsObj = Resolve-ObjectPath -candidates $resolvedObjects -leafName "missing_handler_stubs.cpp.obj" -missingMessage "Missing object file: missing_handler_stubs.cpp.obj (build ssot_stub_lane_objs or RawrXD-Win32IDE first)"
    $providerObj = Resolve-ObjectPath -candidates $resolvedObjects -leafName "ssot_missing_handlers_provider.cpp.obj" -missingMessage "Missing object file: ssot_missing_handlers_provider.cpp.obj (build ssot_stub_lane_objs or RawrXD-Win32IDE first)"

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

    Write-Host "SSOT_STUB_LANE_GUARD: build=$BuildDir scanned_objs=$($resolvedObjects.Count) stubs_defined=$($stubHandlers.Count) provider_defined=$($providerHandlersDefined.Count) provider_listed=$($providerHandlersListed.Count) overlap=0 RESULT=PASS"
    exit 0
} catch {
    $reason = $_.Exception.Message
    Write-Host "SSOT_STUB_LANE_GUARD: build=$BuildDir scanned_objs=$($resolvedObjects.Count) RESULT=FAIL reason=$reason"
    exit 1
}
