# =============================================================================
# CHAT EXTRACTOR - GitHub Copilot + Cursor Agent Panes
# Extracts everything from file system, SQLite, JSON, workspace state.
# No memory injection - file/system/API only.
# =============================================================================
param([string[]]$Targets = @("cursor","github","vscode"))
$ErrorActionPreference = "SilentlyContinue"

$OutDir = "D:\rawrxd\extracted_chats"
New-Item $OutDir -ItemType Directory -Force | Out-Null

$Paths = @{
    Cursor = @{
        State = "$env:APPDATA\Cursor\User\globalStorage"
        WorkspaceState = "$env:APPDATA\Cursor\User\workspaceStorage"
        History = "$env:APPDATA\Cursor\User\History"
        Extensions = "$env:USERPROFILE\.cursor\extensions"
        Cache = "$env:APPDATA\Cursor\Cache"
        CachedData = "$env:APPDATA\Cursor\CachedData"
        GPUCache = "$env:APPDATA\Cursor\GPUCache"
        CodeCache = "$env:APPDATA\Cursor\Code Cache"
        LocalStorage = "$env:APPDATA\Cursor\Local Storage"
        SessionStorage = "$env:APPDATA\Cursor\Session Storage"
        IndexedDB = "$env:APPDATA\Cursor\IndexedDB"
        blob_storage = "$env:APPDATA\Cursor\blob_storage"
        ServiceWorker = "$env:APPDATA\Cursor\Service Worker"
        Logs = "$env:APPDATA\Cursor\logs"
    }
    VSCode = @{
        State = "$env:APPDATA\Code\User\globalStorage"
        WorkspaceState = "$env:APPDATA\Code\User\workspaceStorage"
        History = "$env:APPDATA\Code\User\History"
        Extensions = "$env:USERPROFILE\.vscode\extensions"
    }
    GitHub = @{
        Copilot = "$env:USERPROFILE\.copilot"
        CopilotChat = "$env:APPDATA\GitHub Copilot"
        GlobalStorage = "$env:APPDATA\Code\User\globalStorage\github.copilot*"
    }
}

function Dump-All {
    param([string]$Base, [string]$Out)
    if (-not (Test-Path $Base)) { return }
    Get-ChildItem $Base -Recurse -Force -EA 0 | ForEach-Object {
        $rel = $_.FullName.Replace($Base, "").TrimStart("\")
        $dest = Join-Path $Out $rel
        if ($_.PSIsContainer) { New-Item $dest -ItemType Directory -Force | Out-Null }
        else {
            try {
                New-Item (Split-Path $dest) -ItemType Directory -Force | Out-Null
                Copy-Item $_.FullName $dest -Force -EA 0
            } catch {}
        }
    }
}

function Extract-SQLite {
    param([string]$DbPath, [string]$OutPrefix)
    if (-not (Test-Path $DbPath)) { return }
    try {
        Add-Type -Path "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\System.Data.dll" -EA 0
        $conn = New-Object System.Data.SQLite.SQLiteConnection("Data Source=$DbPath;Version=3;Read Only=True") -EA 0
        if (-not $conn) {
            $tables = & sqlite3 $DbPath ".tables" 2>$null
            $tables -split "\s+" | Where-Object { $_ } | ForEach-Object {
                & sqlite3 $DbPath "SELECT * FROM $_" 2>$null | Out-File "$OutPrefix`_$_.txt" -Force
            }
            return
        }
        $conn.Open()
        $cmd = $conn.CreateCommand()
        $cmd.CommandText = "SELECT name FROM sqlite_master WHERE type='table'"
        $r = $cmd.ExecuteReader()
        while ($r.Read()) {
            $t = $r["name"]
            $cmd2 = $conn.CreateCommand()
            $cmd2.CommandText = "SELECT * FROM [$t]"
            $adp = New-Object System.Data.SQLite.SQLiteDataAdapter($cmd2)
            $dt = New-Object System.Data.DataTable
            $adp.Fill($dt) | Out-Null
            $dt | Export-Csv "$OutPrefix`_$t.csv" -NoTypeInformation -Force
        }
        $conn.Close()
    } catch {
        Get-Content $DbPath -Raw -Encoding Byte -EA 0 | ForEach-Object { [System.Text.Encoding]::UTF8.GetString($_) } | Out-File "$OutPrefix`_raw.txt" -Force
    }
}

function Extract-JSON {
    param([string]$Path, [string]$Out)
    Get-ChildItem $Path -Filter "*.json" -Recurse -EA 0 | ForEach-Object {
        $j = Get-Content $_.FullName -Raw -EA 0
        if ($j) { $j | Out-File (Join-Path $Out ($_.Name -replace "[\\/:*?`"<>|]","_")) -Force }
    }
}

function Extract-Storage {
    param([string]$Root, [string]$Out)
    $state = Join-Path $Root "state.vscdb"
    if (Test-Path $state) { Extract-SQLite $state "$Out\state" }
    Get-ChildItem $Root -Directory -EA 0 | ForEach-Object {
        $ws = Join-Path $_.FullName "workspace.json"
        $db = Join-Path $_.FullName "state.vscdb"
        if (Test-Path $ws) { Copy-Item $ws "$Out\ws_$($_.Name).json" -Force }
        if (Test-Path $db) { Extract-SQLite $db "$Out\ws_$($_.Name)" }
    }
}

# Cursor
if ($Targets -contains "cursor") {
    $cOut = "$OutDir\cursor"
    New-Item $cOut -ItemType Directory -Force | Out-Null
    foreach ($k in $Paths.Cursor.Keys) {
        $p = $Paths.Cursor[$k]
        if (Test-Path $p) {
            Dump-All $p "$cOut\$k"
            Get-ChildItem $p -Filter "*.db" -Recurse -EA 0 | ForEach-Object { Extract-SQLite $_.FullName "$cOut\$k`_$($_.BaseName)" }
        }
    }
    $gs = "$env:APPDATA\Cursor\User\globalStorage"
    Extract-Storage $gs "$cOut\globalStorage"
    $wsRoot = "$env:APPDATA\Cursor\User\workspaceStorage"
    Extract-Storage $wsRoot "$cOut\workspaceStorage"
    Get-ChildItem "$env:APPDATA\Cursor" -Filter "*.json" -Recurse -EA 0 | ForEach-Object {
        (Get-Content $_.FullName -Raw) | Out-File "$cOut\json_$($_.Name)" -Force
    }
}

# GitHub Copilot / VSCode
if ($Targets -contains "github" -or $Targets -contains "vscode") {
    $gOut = "$OutDir\github_copilot"
    New-Item $gOut -ItemType Directory -Force | Out-Null
    Dump-All "$env:USERPROFILE\.copilot" $gOut
    Dump-All "$env:APPDATA\GitHub Copilot" $gOut 2>$null
    Get-ChildItem "$env:APPDATA\Code\User\globalStorage" -Filter "*copilot*" -Directory -EA 0 | ForEach-Object {
        Dump-All $_.FullName "$gOut\copilot_$($_.Name)"
        Extract-Storage $_.FullName "$gOut\copilot_$($_.Name)"
    }
}

# Extension API surface (package.json manifests)
$extOut = "$OutDir\extensions_api"
New-Item $extOut -ItemType Directory -Force | Out-Null
@("$env:USERPROFILE\.cursor\extensions", "$env:USERPROFILE\.vscode\extensions") | ForEach-Object {
    if (Test-Path $_) {
        Get-ChildItem $_ -Filter "package.json" -Recurse -Depth 2 -EA 0 | ForEach-Object {
            $p = Get-Content $_.FullName -Raw | ConvertFrom-Json -EA 0
            if ($p) {
                $name = $p.name + "_" + $p.version
                @{ contributes = $p.contributes; main = $p.main; activationEvents = $p.activationEvents } | ConvertTo-Json -Depth 10 | Out-File "$extOut\$name.json" -Force
            }
        }
    }
}

# Reverse install order (by mtime)
$installOrder = @()
Get-ChildItem "$env:USERPROFILE\.cursor\extensions" -Directory -EA 0 | ForEach-Object {
    $installOrder += [PSCustomObject]@{ Name = $_.Name; Installed = $_.LastWriteTime }
}
$installOrder | Sort-Object Installed -Descending | Export-Csv "$OutDir\extension_install_order.csv" -NoTypeInformation -Force

# IndexedDB / LevelDB (chat blobs)
$idb = "$env:APPDATA\Cursor\IndexedDB"
if (Test-Path $idb) {
    Get-ChildItem $idb -Recurse -File -EA 0 | ForEach-Object {
        $hex = [BitConverter]::ToString([IO.File]::ReadAllBytes($_.FullName)).Replace("-","")
        $hex.Substring(0, [Math]::Min(5000, $hex.Length)) | Out-File "$OutDir\indexeddb_$($_.Name).hex" -Force
    }
}

# Local Storage (chat-related)
$ls = "$env:APPDATA\Cursor\Local Storage\leveldb"
if (Test-Path $ls) {
    Get-ChildItem $ls -Filter "*.log" -EA 0 | ForEach-Object {
        Get-Content $_.FullName -Raw -Encoding Byte | ForEach-Object { [System.Text.Encoding]::UTF8.GetString($_) } | Out-File "$OutDir\localstorage_$($_.Name).txt" -Force
    }
}

Write-Host "Extracted to $OutDir" -ForegroundColor Green
Get-ChildItem $OutDir -Recurse -File | Measure-Object | Select-Object -ExpandProperty Count
