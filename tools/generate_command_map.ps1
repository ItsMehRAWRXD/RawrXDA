#requires -Version 7.0
param(
    [string]$Root = "",
    [string]$OutFile = "",
    [string]$UsageJson = ""
)

$ErrorActionPreference = "Stop"

function Resolve-Root {
    param([string]$Candidate)
    if ($Candidate -and (Test-Path (Join-Path $Candidate "src\core\command_registry.hpp"))) {
        return (Resolve-Path $Candidate).Path
    }
    if ($PSScriptRoot) {
        $parent = Split-Path -Parent $PSScriptRoot
        if (Test-Path (Join-Path $parent "src\core\command_registry.hpp")) {
            return (Resolve-Path $parent).Path
        }
    }
    if (Test-Path "D:\rawrxd\src\core\command_registry.hpp") {
        return "D:\rawrxd"
    }
    throw "Could not resolve repo root."
}

$Root = Resolve-Root -Candidate $Root
if (-not $OutFile) { $OutFile = Join-Path $Root "docs\COMMAND_MAP.md" }
if (-not $UsageJson) { $UsageJson = Join-Path $Root "logs\command_usage_runtime.json" }

$registryPath = Join-Path $Root "src\core\command_registry.hpp"
$lines = Get-Content $registryPath

$rx = '^\s*X\((\d+),\s*([A-Z0-9_]+),\s*"([^"]+)",\s*"([^"]*)",\s*(GUI_ONLY|CLI_ONLY|BOTH|INTERNAL),\s*"([^"]+)",\s*([A-Za-z0-9_]+),\s*([^\)]+)\)\s*\\'
$rows = @()

foreach ($line in $lines) {
    $m = [regex]::Match($line, $rx)
    if (-not $m.Success) { continue }

    $flags = $m.Groups[8].Value.Trim()

    $rows += [pscustomobject]@{
        Id = [int]$m.Groups[1].Value
        Symbol = $m.Groups[2].Value
        Canonical = $m.Groups[3].Value
        CliAlias = $m.Groups[4].Value
        Exposure = $m.Groups[5].Value
        Category = $m.Groups[6].Value
        Handler = $m.Groups[7].Value
        Flags = $flags
    }
}

$disableIncomplete = $true
$disabledCommands = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$incompleteCommands = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)

$cfgPath = Join-Path $Root "config\command_feature_flags.ini"
if (Test-Path $cfgPath) {
    foreach ($line in (Get-Content $cfgPath)) {
        $t = $line.Trim()
        if (-not $t -or $t.StartsWith("#") -or $t.StartsWith(";")) { continue }
        $eq = $t.IndexOf('=')
        if ($eq -lt 0) { continue }
        $k = $t.Substring(0, $eq).Trim().ToLowerInvariant()
        $v = $t.Substring($eq + 1).Trim()
        switch ($k) {
            "disable_incomplete" {
                $lv = $v.ToLowerInvariant()
                $disableIncomplete = -not ($lv -eq "0" -or $lv -eq "false" -or $lv -eq "off")
            }
            "disable_command" {
                if ($v) { [void]$disabledCommands.Add($v) }
            }
            "incomplete_command" {
                if ($v) { [void]$incompleteCommands.Add($v) }
            }
        }
    }
}

$usageMap = @{}
if (Test-Path $UsageJson) {
    try {
        $usage = Get-Content $UsageJson -Raw | ConvertFrom-Json
        if ($usage -and $usage.usage) {
            foreach ($u in $usage.usage) {
                $usageMap[[string]$u.canonical] = [int64]$u.attempts
            }
        }
    }
    catch {
        # ignore malformed telemetry file
    }
}

$selftestCovered = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
@("file.open","view.sidebar","view.transparency100","terminal.splitCode","vscext.status","vscext.listCommands") | ForEach-Object { [void]$selftestCovered.Add($_) }

$docsDir = Split-Path -Parent $OutFile
if ($docsDir -and -not (Test-Path $docsDir)) {
    New-Item -ItemType Directory -Path $docsDir -Force | Out-Null
}

$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine("# RawrXD Command Map")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("| cmdId | canonical | handler | category | exposure | enabled | flags | attempts | proof note |")
[void]$sb.AppendLine("|---:|---|---|---|---|---|---|---:|---|")

foreach ($r in ($rows | Sort-Object Id, Canonical)) {
    $attempts = 0
    if ($usageMap.ContainsKey($r.Canonical)) {
        $attempts = $usageMap[$r.Canonical]
    }

    $proof = "Registry wired + unified dispatch path"
    $enabled = $true
    if ($disabledCommands.Contains($r.Canonical)) {
        $enabled = $false
        $proof = "Disabled by runtime feature flag config"
    }
    elseif ($disableIncomplete -and $incompleteCommands.Contains($r.Canonical)) {
        $enabled = $false
        $proof = "Flagged incomplete by runtime feature flag config"
    }

    if ($attempts -gt 0) {
        $proof = "Observed in runtime telemetry"
    }
    elseif ($selftestCovered.Contains($r.Canonical)) {
        $proof = "Covered by --selftest representative probe"
    }

    [void]$sb.AppendLine("| $($r.Id) | $($r.Canonical) | $($r.Handler) | $($r.Category) | $($r.Exposure) | $(if($enabled){'yes'}else{'no'}) | $($r.Flags) | $attempts | $proof |")
}

Set-Content -Path $OutFile -Value $sb.ToString() -Encoding UTF8
Write-Host "Generated command map: $OutFile"
Write-Host "Rows: $($rows.Count)"
