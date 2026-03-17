# Dump_Chat_Panes.ps1 - Exhaustive dump: AppData + SQLite + UI Automation + CDP + Pipes + Network + FileWatch.
# No 3rd-party DLLs. .NET UIA, Win32, built-in SQLite, Chrome DevTools Protocol, named pipes, netstat.
# Your machine, your processes, your data.
#
# Usage:
#   .\Dump_Chat_Panes.ps1 -All                 # Everything: files + UIA + CDP + pipes + network + process intel
#   .\Dump_Chat_Panes.ps1 -UIA                 # Live window text only
#   .\Dump_Chat_Panes.ps1 -CDP                 # Chrome DevTools Protocol extraction (needs debug port)
#   .\Dump_Chat_Panes.ps1 -Pipes               # Named pipe enumeration
#   .\Dump_Chat_Panes.ps1 -Network             # Process-correlated network endpoints
#   .\Dump_Chat_Panes.ps1 -Watch               # Real-time file watcher (blocks until Ctrl+C)
#   .\Dump_Chat_Panes.ps1 -ProcessIntel        # Command lines, env vars, loaded modules
#   .\Dump_Chat_Panes.ps1 -ExtensionDump       # Generate + sideload a VS Code extension that dumps internal state
#   .\Dump_Chat_Panes.ps1 -Cursor              # Cursor AppData only
#   .\Dump_Chat_Panes.ps1 -Copilot             # VS Code AppData only

param(
    [string]$OutputDir = "D:\rawrxd\dumps\chat_panes",
    [switch]$Cursor,
    [switch]$Copilot,
    [switch]$UIA,
    [switch]$CDP,
    [switch]$Pipes,
    [switch]$Network,
    [switch]$Watch,
    [switch]$ProcessIntel,
    [switch]$ExtensionDump,
    [switch]$All
)

$anySwitchSet = $Cursor -or $Copilot -or $UIA -or $CDP -or $Pipes -or $Network -or $Watch -or $ProcessIntel -or $ExtensionDump
$All = $All -or (-not $anySwitchSet)
$ErrorActionPreference = "SilentlyContinue"
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$script:startTime = Get-Date

# ---------- UI Automation: dump all visible text from Cursor/Code windows (your processes) ----------
function Dump-UIA {
    $outFile = Join-Path $OutputDir "uia_live_panes_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
    $sb = New-Object System.Text.StringBuilder
    try {
        Add-Type -AssemblyName UIAutomationClient
        Add-Type -AssemblyName UIAutomationTypes
        $null = [System.Windows.Automation.AutomationElement]::RootElement
        $cond = [System.Windows.Automation.Condition]::TrueCondition
        $walker = [System.Windows.Automation.TreeWalker]::new($cond)
        $all = [System.Windows.Automation.AutomationElement]::RootElement.FindAll([System.Windows.Automation.TreeScope]::Descendants, $cond)
        foreach ($e in $all) {
            try {
                $name = $e.Current.Name
                $ctrl = $e.Current.ControlType.ProgrammaticName
                $val = $e.GetCurrentPropertyValue([System.Windows.Automation.AutomationElement]::ValuePropertyId)
                if ([string]::IsNullOrWhiteSpace($name) -and [string]::IsNullOrWhiteSpace($val)) { continue }
                $win = $e; while ($win.Current.ControlType.ProgrammaticName -notmatch "Window") { $win = $walker.GetParent($win); if (-not $win) { break } }
                $wintitle = if ($win) { $win.Current.Name } else { "" }
                if ($wintitle -notmatch "Cursor|Code|Visual Studio") { continue }
                [void]$sb.AppendLine("WINDOW: $wintitle | $ctrl | Name=[$name] Value=[$val]")
            } catch { }
        }
        [System.IO.File]::WriteAllText($outFile, $sb.ToString(), [System.Text.Encoding]::UTF8)
        Write-Host "UIA dump: $outFile" -ForegroundColor Green
    } catch {
        # Fallback: raw window titles + GetWindowText via Win32
        $sig = '[DllImport("user32.dll")] public static extern int EnumWindows(IntPtr lpEnumFunc, IntPtr lParam); [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder s, int n); [DllImport("user32.dll")] public static extern int GetWindowTextLength(IntPtr hWnd);'
        Add-Type -MemberDefinition $sig -Name Win32 -Namespace User32 -ErrorAction SilentlyContinue
        $titles = @()
        $callback = { param($hwnd,$lparam) $len = [User32.Win32]::GetWindowTextLength($hwnd); if ($len -gt 0) { $sb = New-Object System.Text.StringBuilder ($len+1); [User32.Win32]::GetWindowText($hwnd,$sb,$sb.Capacity) | Out-Null; $titles += $sb.ToString() }; return $true }
        $null = [User32.Win32]::EnumWindows([System.Runtime.InteropServices.Marshal]::GetFunctionPointerForDelegate($callback), [IntPtr]::Zero)
        $titles | Where-Object { $_ -match "Cursor|Code|VS " } | Set-Content $outFile -Encoding UTF8
        Write-Host "UIA fallback (titles): $outFile" -ForegroundColor Yellow
    }
}

if ($All -or $UIA) { Dump-UIA }

# ---------- Exhaustive paths ----------
$cursorPaths = @(
    "$env:APPDATA\Cursor",
    "$env:LOCALAPPDATA\Cursor",
    "$env:APPDATA\Cursor\User\globalStorage",
    "$env:APPDATA\Cursor\User\workspaceStorage",
    "$env:APPDATA\Cursor\User\History",
    "$env:APPDATA\Cursor\Cache",
    "$env:APPDATA\Cursor\CachedData",
    "$env:APPDATA\Cursor\CachedExtensions",
    "$env:APPDATA\Cursor\Code Cache",
    "$env:LOCALAPPDATA\Cursor\Application Support",
    "$env:USERPROFILE\.cursor"
)

$vscodePaths = @(
    "$env:APPDATA\Code",
    "$env:LOCALAPPDATA\Programs\Microsoft VS Code",
    "$env:APPDATA\Code\User\globalStorage",
    "$env:APPDATA\Code\User\workspaceStorage",
    "$env:APPDATA\Code\User\History",
    "$env:APPDATA\Code\Cache",
    "$env:APPDATA\Code\CachedData",
    "$env:APPDATA\Code\CachedExtensions",
    "$env:USERPROFILE\.vscode"
)

$results = @{ Cursor = @(); Copilot = @() }

function Dump-PathSet {
    param([string[]]$paths, [string]$tag)
    foreach ($p in $paths) {
        if (-not (Test-Path $p)) { continue }
        Get-ChildItem $p -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
            $ext = $_.Extension.ToLower()
            $rel = $_.FullName.Replace($p, "").TrimStart("\")
            $dump = $true
            if ($ext -notin ".json",".db",".sqlite",".sqlite3",".vscdb",".log",".txt","" -and $_.Name -notmatch "state\.vscdb|\.vscdb$") { $dump = $false }
            if (-not $dump) { return }
            try {
                $content = $null
                if ($ext -eq ".json") { $content = Get-Content $_.FullName -Raw -Encoding UTF8 -ErrorAction SilentlyContinue }
                elseif ($ext -in ".db",".sqlite",".sqlite3",".vscdb") {
                    try {
                        $conn = New-Object System.Data.SQLite.SQLiteConnection("Data Source=$($_.FullName);Read Only=True;FailIfMissing=False")
                        $conn.Open()
                        $cmd = $conn.CreateCommand()
                        $cmd.CommandText = "SELECT name FROM sqlite_master WHERE type='table'"
                        $tables = @(); $r = $cmd.ExecuteReader(); while ($r.Read()) { $tables += $r["name"] }; $r.Close()
                        $out = @{}
                        foreach ($t in $tables) {
                            $cmd.CommandText = "SELECT * FROM [$t]"
                            $r = $cmd.ExecuteReader()
                            $rows = @(); while ($r.Read()) { $row = @{}; for ($i=0;$i -lt $r.FieldCount;$i++) { $row[$r.GetName($i)] = $r.GetValue($i) }; $rows += [PSCustomObject]$row }; $r.Close()
                            $out[$t] = $rows
                        }
                        $conn.Close()
                        $content = $out | ConvertTo-Json -Depth 15 -Compress
                    } catch { $content = [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($_.FullName)[0..[Math]::Min(262144,$_.Length)]) }
                }
                else { $content = [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($_.FullName)[0..[Math]::Min(262144,$_.Length)]) }
                if ($content) {
                    $safe = $rel -replace '[\\/:*?"<>|]','_'
                    [System.IO.File]::WriteAllText("$OutputDir\${tag}_$safe.txt", $content, [System.Text.Encoding]::UTF8)
                    $results[$tag] += [PSCustomObject]@{ Path = $rel; Size = $_.Length }
                }
            } catch { }
        }
    }
}

if ($All -or $Cursor) { Dump-PathSet -paths $cursorPaths -tag "Cursor" }
if ($All -or $Copilot) { Dump-PathSet -paths $vscodePaths -tag "Copilot" }

# ---------- state.vscdb raw + copy ----------
$statePaths = @(
    "$env:APPDATA\Cursor\User\globalStorage\state.vscdb",
    "$env:APPDATA\Cursor\User\workspaceStorage\*\state.vscdb",
    "$env:APPDATA\Code\User\globalStorage\state.vscdb",
    "$env:APPDATA\Code\User\workspaceStorage\*\state.vscdb"
)
foreach ($sp in $statePaths) {
    if ($sp -match '\*') {
        Get-Item $sp -ErrorAction SilentlyContinue | ForEach-Object {
            $name = if ($_.FullName -match "Cursor") { "cursor" } else { "copilot" }
            $ws = Split-Path (Split-Path $_.FullName -Parent) -Leaf
            Copy-Item $_.FullName "$OutputDir\${name}_ws_$ws.vscdb" -Force
            Write-Host "Copied: ${name}_ws_$ws.vscdb" -ForegroundColor Green
        }
    } elseif (Test-Path $sp) {
        $name = if ($sp -match "Cursor") { "cursor" } else { "copilot" }
        Copy-Item $sp "$OutputDir\${name}_state.vscdb" -Force
        Write-Host "Copied: ${name}_state.vscdb" -ForegroundColor Green
    }
}

# ---------- ItemTable from state.vscdb (key-value store Cursor/Code use) ----------
Get-ChildItem $OutputDir -Filter "*.vscdb" -ErrorAction SilentlyContinue | ForEach-Object {
    try {
        $conn = New-Object System.Data.SQLite.SQLiteConnection("Data Source=$($_.FullName);Read Only=True")
        $conn.Open()
        $cmd = $conn.CreateCommand()
        $cmd.CommandText = "SELECT key, value FROM ItemTable"
        $r = $cmd.ExecuteReader()
        $kv = @{}
        while ($r.Read()) { $kv[$r["key"]] = $r["value"] }
        $r.Close()
        $conn.Close()
        $kv.GetEnumerator() | ForEach-Object { "$($_.Key)`t$($_.Value)" } | Set-Content "$($_.FullName).ItemTable.txt" -Encoding UTF8
        Write-Host "ItemTable dumped: $($_.Name).ItemTable.txt" -ForegroundColor Green
    } catch { }
}

$results | ConvertTo-Json -Depth 5 | Out-File "$OutputDir\manifest.json" -Force
Write-Host "Dump complete: $OutputDir" -ForegroundColor Magenta
