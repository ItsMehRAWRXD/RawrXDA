<# Storage.psm1
   Simple JSONL storage for OpenMemory
#>

function Get-StoragePath {
    param([string]$StorageFile = "$PSScriptRoot\memory.jsonl")
    return (Resolve-Path -LiteralPath $StorageFile -ErrorAction SilentlyContinue) -or $StorageFile
}

function Add-MemoryItem {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Text,
        [hashtable]$Metadata = @{},
        [double]$Salience = 1.0,
        [string]$StorageFile
    )

    if (-not $StorageFile) { $StorageFile = "$PSScriptRoot\memory.jsonl" }

    $id = [guid]::NewGuid().ToString()
    $obj = [ordered]@{
        id = $id
        text = $Text
        metadata = $Metadata
        vector = $null
        timestamp = (Get-Date).ToUniversalTime().ToString("o")
        salience = [double]$Salience
        last_access = (Get-Date).ToUniversalTime().ToString("o")
    }

    $json = $obj | ConvertTo-Json -Depth 10 -Compress
    Add-Content -Path $StorageFile -Value $json -Encoding UTF8

    return $obj
}

function Get-MemoryItem {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Id,
        [string]$StorageFile
    )
    if (-not $StorageFile) { $StorageFile = "$PSScriptRoot\memory.jsonl" }
    if (-not (Test-Path $StorageFile)) { return $null }
    Get-Content -Path $StorageFile -Encoding UTF8 | ForEach-Object {
        try { $o = $_ | ConvertFrom-Json -ErrorAction Stop } catch { return }
        if ($o.id -eq $Id) { return $o }
    }
}

function Get-MemoryItems {
    [CmdletBinding()]
    param(
        [string]$StorageFile,
        [ScriptBlock]$Filter
    )
    if (-not $StorageFile) { $StorageFile = "$PSScriptRoot\memory.jsonl" }
    if (-not (Test-Path $StorageFile)) { return @() }
    $results = @()
    Get-Content -Path $StorageFile -Encoding UTF8 | ForEach-Object {
        try { $o = $_ | ConvertFrom-Json -ErrorAction Stop } catch { return }
        if ($Filter) {
            if (& $Filter $o) { $results += $o }
        } else { $results += $o }
    }
    return $results
}

function Update-MemoryItem {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Id,
        [Parameter(Mandatory)][hashtable]$Updates,
        [string]$StorageFile
    )
    if (-not $StorageFile) { $StorageFile = "$PSScriptRoot\memory.jsonl" }
    if (-not (Test-Path $StorageFile)) { throw "Storage file not found: $StorageFile" }

    $lines = Get-Content -Path $StorageFile -Encoding UTF8
    $out = @()
    foreach ($line in $lines) {
        try { $o = $line | ConvertFrom-Json -ErrorAction Stop } catch { $out += $line; continue }
        if ($o.id -eq $Id) {
            foreach ($k in $Updates.Keys) { $o.$k = $Updates[$k] }
            $out += ($o | ConvertTo-Json -Depth 10 -Compress)
        } else { $out += $line }
    }

    $out | Set-Content -Path $StorageFile -Encoding UTF8
    return (Get-MemoryItem -Id $Id -StorageFile $StorageFile)
}

function Compact-Memory {
    [CmdletBinding()]
    param(
        [double]$MinSalience = 0.01,
        [string]$StorageFile
    )
    if (-not $StorageFile) { $StorageFile = "$PSScriptRoot\memory.jsonl" }
    if (-not (Test-Path $StorageFile)) { return }
    $kept = @()
    Get-Content -Path $StorageFile -Encoding UTF8 | ForEach-Object {
        try { $o = $_ | ConvertFrom-Json -ErrorAction Stop } catch { return }
        if (-not $o.salience) { $kept += $_; return }
        if ([double]$o.salience -ge $MinSalience) { $kept += ($_ | ConvertTo-Json -Depth 10 -Compress) }
    }
    $kept | Set-Content -Path $StorageFile -Encoding UTF8
}

Export-ModuleMember -Function Add-MemoryItem, Get-MemoryItem, Get-MemoryItems, Update-MemoryItem, Compact-Memory
