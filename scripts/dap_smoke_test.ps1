param(
    [string]$WorkspaceRoot = "d:\rawrxd",
    [string]$AdapterPath = "",
    [switch]$Handshake
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-LaunchPath {
    param([string]$Root)

    $primary = Join-Path $Root '.vscode\launch.json'
    if (Test-Path $primary) { return $primary }

    $fallback = Join-Path $Root '.rawrxd\launch.json'
    if (Test-Path $fallback) { return $fallback }

    throw "launch.json not found under $Root"
}

function Resolve-AdapterPath {
    param(
        [pscustomobject]$Config,
        [string]$ExplicitPath
    )

    if ($ExplicitPath -and (Test-Path $ExplicitPath)) {
        return $ExplicitPath
    }

    foreach ($field in 'adapterExecutable', 'debugAdapter', 'debugAdapterPath') {
        if ($Config.PSObject.Properties.Name -contains $field) {
            $candidate = [string]$Config.$field
            if ($candidate -and (Test-Path $candidate)) {
                return $candidate
            }
        }
    }

    $extensionRoots = @(
        (Join-Path $env:USERPROFILE '.vscode\extensions'),
        'C:\Users\HiH8e\.vscode\extensions'
    ) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique

    foreach ($root in $extensionRoots) {
        $adapter = Get-ChildItem -Path $root -Directory -Filter 'ms-vscode.cpptools*' -ErrorAction SilentlyContinue |
            ForEach-Object { Join-Path $_.FullName 'debugAdapters\bin\OpenDebugAD7.exe' } |
            Where-Object { Test-Path $_ } |
            Select-Object -First 1
        if ($adapter) {
            return $adapter
        }
    }

    return ''
}

function Send-DapMessage {
    param(
        [System.IO.StreamWriter]$Writer,
        [hashtable]$Payload
    )

    $json = ($Payload | ConvertTo-Json -Depth 10 -Compress)
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    $Writer.Write("Content-Length: $($bytes.Length)`r`n`r`n$json")
    $Writer.Flush()
}

function Read-DapMessage {
    param([System.IO.StreamReader]$Reader)

    $contentLength = 0
    while ($true) {
        $line = $Reader.ReadLine()
        if ($null -eq $line) {
            throw 'DAP stream closed before header completed'
        }
        if ($line -eq '') {
            break
        }
        if ($line -like 'Content-Length:*') {
            $contentLength = [int]($line.Substring('Content-Length:'.Length).Trim())
        }
    }

    if ($contentLength -le 0) {
        throw 'Invalid DAP content length'
    }

    $buffer = New-Object char[] $contentLength
    $offset = 0
    while ($offset -lt $contentLength) {
        $read = $Reader.Read($buffer, $offset, $contentLength - $offset)
        if ($read -le 0) {
            throw 'DAP stream closed while reading payload'
        }
        $offset += $read
    }

    return ($buffer -join '' | ConvertFrom-Json)
}

function Get-LaunchConfigurations {
    param([string]$LaunchPath)

    $raw = Get-Content $LaunchPath -Raw

    try {
        $parsed = $raw | ConvertFrom-Json
        if ($parsed -and $parsed.PSObject.Properties.Name -contains 'configurations') {
            return @($parsed.configurations)
        }
    }
    catch {
    }

    $configs = New-Object System.Collections.Generic.List[object]
    $depth = 0
    $buffer = New-Object System.Collections.Generic.List[string]

    foreach ($line in (Get-Content $LaunchPath)) {
        $sanitized = [regex]::Replace($line, '"(?:\\.|[^"\\])*"', '""')
        $openCount = ([regex]::Matches($sanitized, '\{')).Count
        $closeCount = ([regex]::Matches($sanitized, '\}')).Count

        if ($depth -gt 0 -or $openCount -gt 0) {
            $buffer.Add($line)
        }

        $depth += $openCount
        $depth -= $closeCount

        if ($depth -eq 0 -and $buffer.Count -gt 0) {
            $jsonText = ($buffer -join "`n")
            try {
                $obj = $jsonText | ConvertFrom-Json
                if ($obj -and ($obj.PSObject.Properties.Name -contains 'type') -and ($obj.PSObject.Properties.Name -contains 'request')) {
                    $configs.Add($obj)
                }
            }
            catch {
            }
            $buffer.Clear()
        }
    }

    return ,($configs.ToArray())
}

$launchPath = Resolve-LaunchPath -Root $WorkspaceRoot
$configs = @(Get-LaunchConfigurations -LaunchPath $launchPath)
if (-not $configs -or $configs.Count -eq 0) {
    throw "No configurations found in $launchPath"
}

$config = $configs | Where-Object { $_.type -in @('cppdbg', 'cppvsdbg') -and $_.request -eq 'launch' } | Select-Object -First 1
if (-not $config) {
    $config = $configs | Where-Object { $_.type -in @('cppdbg', 'cppvsdbg') } | Select-Object -First 1
}
if (-not $config) {
    throw 'No cppdbg/cppvsdbg configuration found'
}

$resolvedAdapter = Resolve-AdapterPath -Config $config -ExplicitPath $AdapterPath
$programValue = ''
if ($config.PSObject.Properties.Name -contains 'program') {
    $programValue = [string]$config.program
}

Write-Host "Launch file : $launchPath"
Write-Host "Config name  : $($config.name)"
Write-Host "Type/request : $($config.type) / $($config.request)"
Write-Host "Program      : $programValue"
Write-Host "Adapter path : $(if ($resolvedAdapter) { $resolvedAdapter } else { '<not found>' })"

if (-not $Handshake) {
    exit 0
}

if (-not $resolvedAdapter) {
    throw 'Handshake requested but no DAP adapter could be resolved'
}

$process = New-Object System.Diagnostics.Process
$process.StartInfo.FileName = $resolvedAdapter
$process.StartInfo.RedirectStandardInput = $true
$process.StartInfo.RedirectStandardOutput = $true
$process.StartInfo.RedirectStandardError = $true
$process.StartInfo.UseShellExecute = $false
$process.StartInfo.CreateNoWindow = $true

if (-not $process.Start()) {
    throw "Failed to start adapter: $resolvedAdapter"
}

try {
    $writer = $process.StandardInput
    $reader = $process.StandardOutput

    Send-DapMessage -Writer $writer -Payload @{
        seq = 1
        type = 'request'
        command = 'initialize'
        arguments = @{
            clientID = 'rawrxd-smoke'
            clientName = 'RawrXD Smoke Test'
            adapterID = $config.type
            pathFormat = 'path'
            linesStartAt1 = $true
            columnsStartAt1 = $true
        }
    }

    $response = Read-DapMessage -Reader $reader
    Write-Host "Handshake response type : $($response.type)"
    Write-Host "Handshake success       : $($response.success)"

    if (-not $response.success) {
        throw "Initialize failed: $($response.message)"
    }
}
finally {
    if (-not $process.HasExited) {
        $process.Kill()
        $process.WaitForExit()
    }
}

Write-Host 'DAP smoke test passed.'