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

# ============================================================================
# HELPER: Write output with timestamp
# ============================================================================
function Write-Dump {
    param([string]$Message, [string]$Color = "Green")
    $ts = (Get-Date).ToString("HH:mm:ss.fff")
    Write-Host "[$ts] $Message" -ForegroundColor $Color
}

# ============================================================================
# 1. UI AUTOMATION — walk all visible controls + TextPattern for editor text
# ============================================================================
function Dump-UIA {
    $outFile = Join-Path $OutputDir "uia_live_panes_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
    $textFile = Join-Path $OutputDir "uia_textpattern_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
    $sb = New-Object System.Text.StringBuilder
    $tbSb = New-Object System.Text.StringBuilder
    try {
        Add-Type -AssemblyName UIAutomationClient
        Add-Type -AssemblyName UIAutomationTypes
        $root = [System.Windows.Automation.AutomationElement]::RootElement
        $cond = [System.Windows.Automation.Condition]::TrueCondition
        $walker = [System.Windows.Automation.TreeWalker]::new($cond)

        # Find top-level Cursor/Code/VS windows first to scope the walk
        $winCond = New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
            [System.Windows.Automation.ControlType]::Window)
        $topWindows = $root.FindAll([System.Windows.Automation.TreeScope]::Children, $winCond)
        $targetWindows = @()
        foreach ($tw in $topWindows) {
            try {
                if ($tw.Current.Name -match "Cursor|Code|Visual Studio") {
                    $targetWindows += $tw
                }
            } catch { }
        }

        if ($targetWindows.Count -eq 0) {
            Write-Dump "No Cursor/Code/VS windows found for UIA" Yellow
            throw "NoTargetWindows"
        }

        foreach ($tw in $targetWindows) {
            $wintitle = $tw.Current.Name
            [void]$sb.AppendLine("=" * 120)
            [void]$sb.AppendLine("WINDOW: $wintitle  (PID: $($tw.Current.ProcessId))")
            [void]$sb.AppendLine("=" * 120)

            # Walk descendants of this window only (much faster than walking entire desktop)
            $descendants = $tw.FindAll([System.Windows.Automation.TreeScope]::Descendants, $cond)
            foreach ($e in $descendants) {
                try {
                    $name = $e.Current.Name
                    $ctrl = $e.Current.ControlType.ProgrammaticName
                    $autoId = $e.Current.AutomationId
                    $className = $e.Current.ClassName

                    # Try ValuePattern
                    $val = ""
                    try {
                        $valPattern = $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
                        if ($valPattern) { $val = $valPattern.Current.Value }
                    } catch { }

                    if ([string]::IsNullOrWhiteSpace($name) -and [string]::IsNullOrWhiteSpace($val) -and [string]::IsNullOrWhiteSpace($autoId)) { continue }

                    [void]$sb.AppendLine("  $ctrl | AutoId=[$autoId] Class=[$className] Name=[$name] Value=[$val]")

                    # --- TextPattern: extract actual text content from document/edit controls ---
                    try {
                        $textPattern = $e.GetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern)
                        if ($textPattern) {
                            $docRange = $textPattern.DocumentRange
                            $text = $docRange.GetText(65536)  # up to 64KB per control
                            if (-not [string]::IsNullOrWhiteSpace($text)) {
                                [void]$tbSb.AppendLine("=" * 80)
                                [void]$tbSb.AppendLine("SOURCE: $wintitle | $ctrl | AutoId=[$autoId] Name=[$name]")
                                [void]$tbSb.AppendLine("-" * 80)
                                [void]$tbSb.AppendLine($text)
                                [void]$tbSb.AppendLine("")
                            }
                        }
                    } catch { }

                    # --- ScrollPattern: get scroll position (useful for knowing what's visible) ---
                    try {
                        $scrollPattern = $e.GetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern)
                        if ($scrollPattern) {
                            $hPct = $scrollPattern.Current.HorizontalScrollPercent
                            $vPct = $scrollPattern.Current.VerticalScrollPercent
                            [void]$sb.AppendLine("    [Scroll] H=$hPct% V=$vPct%")
                        }
                    } catch { }

                } catch { }
            }
        }

        [System.IO.File]::WriteAllText($outFile, $sb.ToString(), [System.Text.Encoding]::UTF8)
        Write-Dump "UIA control tree: $outFile"

        if ($tbSb.Length -gt 0) {
            [System.IO.File]::WriteAllText($textFile, $tbSb.ToString(), [System.Text.Encoding]::UTF8)
            Write-Dump "UIA TextPattern content: $textFile"
        }

    } catch {
        if ($_.Exception.Message -eq "NoTargetWindows") { return }
        # Fallback: raw window titles via Win32 EnumWindows
        Write-Dump "UIA failed, falling back to Win32 EnumWindows" Yellow
        $sig = @'
[DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
[DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder s, int n);
[DllImport("user32.dll")] public static extern int GetWindowTextLength(IntPtr hWnd);
[DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
'@
        Add-Type -MemberDefinition $sig -Name Win32UIA -Namespace User32 -ErrorAction SilentlyContinue
        $script:windowData = @()
        $cb = [User32.Win32UIA+EnumWindowsProc]{
            param($hwnd, $lparam)
            if ([User32.Win32UIA]::IsWindowVisible($hwnd)) {
                $len = [User32.Win32UIA]::GetWindowTextLength($hwnd)
                if ($len -gt 0) {
                    $sb2 = New-Object System.Text.StringBuilder ($len + 1)
                    [User32.Win32UIA]::GetWindowText($hwnd, $sb2, $sb2.Capacity) | Out-Null
                    $pid = [uint32]0
                    [User32.Win32UIA]::GetWindowThreadProcessId($hwnd, [ref]$pid) | Out-Null
                    $title = $sb2.ToString()
                    if ($title -match "Cursor|Code|Visual Studio") {
                        $script:windowData += "HWND=0x$($hwnd.ToString('X')) PID=$pid TITLE=$title"
                    }
                }
            }
            return $true
        }
        [User32.Win32UIA]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
        $script:windowData | Set-Content $outFile -Encoding UTF8
        Write-Dump "UIA fallback (Win32 titles): $outFile" Yellow
    }
}

if ($All -or $UIA) { Dump-UIA }

# ============================================================================
# 2. CHROME DEVTOOLS PROTOCOL — connect to Electron's inspector
# ============================================================================
function Dump-CDP {
    $outFile = Join-Path $OutputDir "cdp_extraction_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
    $sb = New-Object System.Text.StringBuilder

    # Discover debug ports from running processes
    $debugPorts = @()
    $procs = Get-Process -Name "Cursor","Code","electron" -ErrorAction SilentlyContinue
    foreach ($proc in $procs) {
        try {
            $cmdline = (Get-CimInstance Win32_Process -Filter "ProcessId=$($proc.Id)" -ErrorAction SilentlyContinue).CommandLine
            if ($cmdline -match "--remote-debugging-port=(\d+)") {
                $debugPorts += @{ Port = [int]$Matches[1]; Name = $proc.Name; PID = $proc.Id; CmdLine = $cmdline }
            }
            # Also check for --inspect
            if ($cmdline -match "--inspect=(?:[\d\.]+:)?(\d+)") {
                $debugPorts += @{ Port = [int]$Matches[1]; Name = $proc.Name; PID = $proc.Id; CmdLine = $cmdline }
            }
        } catch { }
    }

    # Also probe common Electron debug ports
    $commonPorts = @(9222, 9229, 9230, 5858, 9333, 9515)
    foreach ($port in $commonPorts) {
        try {
            $tcp = New-Object System.Net.Sockets.TcpClient
            $tcp.ConnectAsync("127.0.0.1", $port).Wait(500) | Out-Null
            if ($tcp.Connected) {
                $already = $debugPorts | Where-Object { $_.Port -eq $port }
                if (-not $already) {
                    $debugPorts += @{ Port = $port; Name = "unknown"; PID = 0; CmdLine = "probed" }
                }
            }
            $tcp.Close()
        } catch { }
    }

    if ($debugPorts.Count -eq 0) {
        [void]$sb.AppendLine("No active debug ports found.")
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("To enable CDP, launch Cursor/Code with:")
        [void]$sb.AppendLine('  cursor.exe --remote-debugging-port=9222')
        [void]$sb.AppendLine('  code.exe --remote-debugging-port=9222')
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("Or set environment variable before launching:")
        [void]$sb.AppendLine('  $env:ELECTRON_ENABLE_LOGGING = "1"')
        [void]$sb.AppendLine('  $env:ELECTRON_DEBUG_PORT = "9222"')
        [System.IO.File]::WriteAllText($outFile, $sb.ToString(), [System.Text.Encoding]::UTF8)
        Write-Dump "CDP: No debug ports active. Instructions written to $outFile" Yellow
        return
    }

    foreach ($dp in $debugPorts) {
        $port = $dp.Port
        [void]$sb.AppendLine("=" * 100)
        [void]$sb.AppendLine("CDP TARGET: $($dp.Name) PID=$($dp.PID) PORT=$port")
        [void]$sb.AppendLine("=" * 100)

        # Get list of debuggable targets
        try {
            $targets = Invoke-RestMethod -Uri "http://127.0.0.1:$port/json" -TimeoutSec 5 -ErrorAction Stop
            foreach ($target in $targets) {
                [void]$sb.AppendLine("")
                [void]$sb.AppendLine("--- Target: $($target.title) ---")
                [void]$sb.AppendLine("  Type:        $($target.type)")
                [void]$sb.AppendLine("  URL:         $($target.url)")
                [void]$sb.AppendLine("  WebSocket:   $($target.webSocketDebuggerUrl)")
                [void]$sb.AppendLine("  DevToolsURL: $($target.devtoolsFrontendUrl)")
            }
        } catch {
            [void]$sb.AppendLine("  Failed to enumerate targets: $($_.Exception.Message)")
        }

        # Get version info
        try {
            $ver = Invoke-RestMethod -Uri "http://127.0.0.1:$port/json/version" -TimeoutSec 5 -ErrorAction Stop
            [void]$sb.AppendLine("")
            [void]$sb.AppendLine("--- Version Info ---")
            $ver.PSObject.Properties | ForEach-Object { [void]$sb.AppendLine("  $($_.Name): $($_.Value)") }
        } catch { }

        # Try to extract page content via CDP WebSocket
        try {
            $targets = Invoke-RestMethod -Uri "http://127.0.0.1:$port/json" -TimeoutSec 5 -ErrorAction Stop
            foreach ($target in $targets) {
                if (-not $target.webSocketDebuggerUrl) { continue }
                $wsUrl = $target.webSocketDebuggerUrl

                # Use .NET WebSocket to send CDP commands
                $ws = New-Object System.Net.WebSockets.ClientWebSocket
                $cts = New-Object System.Threading.CancellationTokenSource(10000)
                try {
                    $ws.ConnectAsync([Uri]$wsUrl, $cts.Token).Wait()

                    # CDP commands to extract data
                    $cdpCommands = @(
                        @{ id = 1; method = "Runtime.evaluate"; params = @{
                            expression = "JSON.stringify({title:document.title, url:location.href, bodyText:document.body?.innerText?.substring(0,32768)||''})"
                            returnByValue = $true
                        }},
                        @{ id = 2; method = "Runtime.evaluate"; params = @{
                            expression = "JSON.stringify(Array.from(document.querySelectorAll('[class*=chat],[class*=message],[class*=conversation],[class*=copilot],[class*=inline-chat],[role=log],[role=dialog]')).map(e=>({tag:e.tagName,class:e.className,text:e.innerText?.substring(0,4096)||''})))"
                            returnByValue = $true
                        }},
                        @{ id = 3; method = "Runtime.evaluate"; params = @{
                            expression = "JSON.stringify({localStorage:Object.fromEntries(Object.entries(localStorage).filter(([k])=>k.match(/chat|copilot|cursor|conversation|history|model|ai/i)).map(([k,v])=>[k,v.substring(0,8192)]))})"
                            returnByValue = $true
                        }},
                        @{ id = 4; method = "Runtime.evaluate"; params = @{
                            expression = "JSON.stringify(performance.getEntriesByType('resource').filter(e=>e.name.match(/api|chat|copilot|model|inference|completion/i)).map(e=>({name:e.name,duration:e.duration,size:e.transferSize})))"
                            returnByValue = $true
                        }}
                    )

                    foreach ($cmd in $cdpCommands) {
                        try {
                            $json = $cmd | ConvertTo-Json -Depth 10 -Compress
                            $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
                            $seg = New-Object System.ArraySegment[byte] $bytes, 0, $bytes.Length
                            $ws.SendAsync($seg, [System.Net.WebSockets.WebSocketMessageType]::Text, $true, $cts.Token).Wait()

                            # Read response
                            $buf = New-Object byte[] 262144
                            $result = $ws.ReceiveAsync((New-Object System.ArraySegment[byte] $buf, 0, $buf.Length), $cts.Token).Result
                            $response = [System.Text.Encoding]::UTF8.GetString($buf, 0, $result.Count)
                            $parsed = $response | ConvertFrom-Json -ErrorAction SilentlyContinue

                            [void]$sb.AppendLine("")
                            [void]$sb.AppendLine("  CDP Command $($cmd.id) response:")
                            if ($parsed.result.result.value) {
                                [void]$sb.AppendLine("  $($parsed.result.result.value)")
                            } else {
                                [void]$sb.AppendLine("  $response")
                            }
                        } catch {
                            [void]$sb.AppendLine("  CDP Command $($cmd.id) failed: $($_.Exception.Message)")
                        }
                    }

                    $ws.CloseAsync([System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure, "", $cts.Token).Wait()
                } catch {
                    [void]$sb.AppendLine("  WebSocket connection failed: $($_.Exception.Message)")
                } finally {
                    $ws.Dispose()
                    $cts.Dispose()
                }
            }
        } catch { }
    }

    # Write CDP-specific JSON extracts to separate files
    $cdpDir = Join-Path $OutputDir "cdp"
    New-Item -ItemType Directory -Path $cdpDir -Force | Out-Null
    [System.IO.File]::WriteAllText($outFile, $sb.ToString(), [System.Text.Encoding]::UTF8)

    # Also dump all targets as JSON
    foreach ($dp in $debugPorts) {
        try {
            $targets = Invoke-RestMethod -Uri "http://127.0.0.1:$($dp.Port)/json" -TimeoutSec 5 -ErrorAction Stop
            $targets | ConvertTo-Json -Depth 10 | Set-Content "$cdpDir\targets_port$($dp.Port).json" -Encoding UTF8
        } catch { }
    }

    Write-Dump "CDP extraction: $outFile"
}

if ($All -or $CDP) { Dump-CDP }

# ============================================================================
# 3. NAMED PIPE DISCOVERY — find Cursor/Code IPC channels
# ============================================================================
function Dump-Pipes {
    $outFile = Join-Path $OutputDir "named_pipes_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
    $sb = New-Object System.Text.StringBuilder

    [void]$sb.AppendLine("Named Pipe Discovery - $(Get-Date)")
    [void]$sb.AppendLine("=" * 80)

    # Enumerate all named pipes matching VS Code / Cursor patterns
    $pipePatterns = @("*vscode*","*cursor*","*electron*","*exthost*","*langserver*",
                      "*lsp*","*copilot*","*git*","*typescript*","*node*","*debug*",
                      "*pty*","*terminal*")

    $allPipes = Get-ChildItem "\\.\pipe\" -ErrorAction SilentlyContinue
    $matched = @()
    foreach ($pipe in $allPipes) {
        foreach ($pat in $pipePatterns) {
            if ($pipe.Name -like $pat) {
                $matched += $pipe
                break
            }
        }
    }

    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("Found $($matched.Count) relevant pipes:")
    [void]$sb.AppendLine("-" * 80)

    foreach ($pipe in ($matched | Sort-Object Name)) {
        [void]$sb.AppendLine("  $($pipe.Name)")

        # Try to identify the owning process
        try {
            $handle = $null
            $procs = Get-Process -ErrorAction SilentlyContinue | Where-Object {
                $_.Name -match "Cursor|Code|electron|node"
            }
            foreach ($p in $procs) {
                # Check if process has this pipe open via handle
                [void]$sb.AppendLine("    -> Likely owner: $($p.Name) (PID $($p.Id))")
                break
            }
        } catch { }
    }

    # Also enumerate all pipes for completeness (just Code/Cursor related)
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("Full pipe listing (Cursor/Code processes):")
    [void]$sb.AppendLine("-" * 80)
    $ideProcs = Get-Process -Name "Cursor","Code","electron" -ErrorAction SilentlyContinue
    foreach ($proc in $ideProcs) {
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("Process: $($proc.Name) PID=$($proc.Id)")
        [void]$sb.AppendLine("  MainWindow: $($proc.MainWindowTitle)")
        [void]$sb.AppendLine("  StartTime:  $($proc.StartTime)")
        [void]$sb.AppendLine("  Threads:    $($proc.Threads.Count)")
        [void]$sb.AppendLine("  Memory:     $([math]::Round($proc.WorkingSet64/1MB, 1)) MB")
    }

    # Enumerate VSCode socket files (used on some configs)
    $sockDirs = @(
        "$env:TEMP",
        "$env:LOCALAPPDATA\Temp",
        "$env:APPDATA\Cursor",
        "$env:APPDATA\Code"
    )
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("Socket/IPC files:")
    [void]$sb.AppendLine("-" * 80)
    foreach ($dir in $sockDirs) {
        if (-not (Test-Path $dir)) { continue }
        Get-ChildItem $dir -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match "\.sock$|\.ipc$|vscode-ipc|cursor-ipc" } |
            ForEach-Object { [void]$sb.AppendLine("  $($_.FullName) ($($_.Length) bytes)") }
    }

    [System.IO.File]::WriteAllText($outFile, $sb.ToString(), [System.Text.Encoding]::UTF8)
    Write-Dump "Named pipes: $outFile"
}

if ($All -or $Pipes) { Dump-Pipes }

# ============================================================================
# 4. NETWORK ENDPOINT CORRELATION — which AI backends are being contacted
# ============================================================================
function Dump-Network {
    $outFile = Join-Path $OutputDir "network_endpoints_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
    $sb = New-Object System.Text.StringBuilder

    [void]$sb.AppendLine("Network Endpoint Correlation - $(Get-Date)")
    [void]$sb.AppendLine("=" * 80)

    # Get PIDs for Cursor/Code processes
    $ideProcs = Get-Process -Name "Cursor","Code","electron","node" -ErrorAction SilentlyContinue
    $idePids = $ideProcs | ForEach-Object { $_.Id }

    if ($idePids.Count -eq 0) {
        [void]$sb.AppendLine("No Cursor/Code processes running.")
        [System.IO.File]::WriteAllText($outFile, $sb.ToString(), [System.Text.Encoding]::UTF8)
        Write-Dump "Network: No IDE processes found" Yellow
        return
    }

    [void]$sb.AppendLine("Monitoring PIDs: $($idePids -join ', ')")
    [void]$sb.AppendLine("")

    # Get TCP connections via netstat and correlate with PIDs
    $netstat = netstat -ano -p TCP 2>$null | Select-Object -Skip 4
    $connections = @()
    foreach ($line in $netstat) {
        $parts = $line.Trim() -split '\s+'
        if ($parts.Count -ge 5) {
            $pid = [int]$parts[4]
            if ($pid -in $idePids) {
                $connections += [PSCustomObject]@{
                    Protocol    = $parts[0]
                    LocalAddr   = $parts[1]
                    RemoteAddr  = $parts[2]
                    State       = $parts[3]
                    PID         = $pid
                    ProcessName = ($ideProcs | Where-Object { $_.Id -eq $pid } | Select-Object -First 1).Name
                }
            }
        }
    }

    # Group by remote address and resolve DNS
    $remoteEndpoints = $connections | Where-Object { $_.State -eq "ESTABLISHED" -and $_.RemoteAddr -notmatch "127\.0\.0\.1|0\.0\.0\.0|\[::\]" }
    $grouped = $remoteEndpoints | Group-Object RemoteAddr | Sort-Object Count -Descending

    [void]$sb.AppendLine("Active connections from Cursor/Code:")
    [void]$sb.AppendLine("-" * 80)
    foreach ($g in $grouped) {
        $addr = $g.Name
        $ip = ($addr -split ':')[0]
        $port = ($addr -split ':')[-1]
        $dns = ""
        try { $dns = [System.Net.Dns]::GetHostEntry($ip).HostName } catch { $dns = "(unresolved)" }

        $sample = $g.Group[0]
        [void]$sb.AppendLine("  $addr -> $dns :$port [$($sample.ProcessName) PID=$($sample.PID)] x$($g.Count)")
    }

    # Identify known AI endpoints
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("Known AI/Cloud endpoint matches:")
    [void]$sb.AppendLine("-" * 80)
    $aiPatterns = @(
        @{ Pattern = "openai"; Label = "OpenAI API" },
        @{ Pattern = "anthropic"; Label = "Anthropic API" },
        @{ Pattern = "cursor\.sh|cursor\.so|cursorapi"; Label = "Cursor Backend" },
        @{ Pattern = "copilot|githubcopilot"; Label = "GitHub Copilot" },
        @{ Pattern = "azure\.com|cognitiveservices"; Label = "Azure AI" },
        @{ Pattern = "googleapis|gemini"; Label = "Google AI" },
        @{ Pattern = "ollama|localhost:11434"; Label = "Ollama (local)" },
        @{ Pattern = "huggingface"; Label = "HuggingFace" },
        @{ Pattern = "vscode|visualstudio\.com|microsoft\.com"; Label = "VS Code Services" },
        @{ Pattern = "github\.com|api\.github"; Label = "GitHub" }
    )

    foreach ($g in $grouped) {
        $addr = $g.Name
        $ip = ($addr -split ':')[0]
        $dns = ""
        try { $dns = [System.Net.Dns]::GetHostEntry($ip).HostName } catch { }
        $combined = "$addr $dns"

        foreach ($ap in $aiPatterns) {
            if ($combined -match $ap.Pattern) {
                [void]$sb.AppendLine("  [$($ap.Label)] $addr ($dns)")
            }
        }
    }

    # Also dump all listening ports from these processes
    $listeners = $connections | Where-Object { $_.State -eq "LISTENING" }
    if ($listeners.Count -gt 0) {
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("Listening ports (local servers):")
        [void]$sb.AppendLine("-" * 80)
        foreach ($l in $listeners) {
            [void]$sb.AppendLine("  $($l.LocalAddr) [$($l.ProcessName) PID=$($l.PID)]")
        }
    }

    [System.IO.File]::WriteAllText($outFile, $sb.ToString(), [System.Text.Encoding]::UTF8)
    Write-Dump "Network endpoints: $outFile"
}

if ($All -or $Network) { Dump-Network }

# ============================================================================
# 5. PROCESS INTELLIGENCE — command lines, env vars, loaded modules
# ============================================================================
function Dump-ProcessIntel {
    $outFile = Join-Path $OutputDir "process_intel_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
    $sb = New-Object System.Text.StringBuilder

    [void]$sb.AppendLine("Process Intelligence Report - $(Get-Date)")
    [void]$sb.AppendLine("=" * 100)

    $ideProcs = Get-Process -Name "Cursor","Code","electron","node" -ErrorAction SilentlyContinue
    foreach ($proc in $ideProcs) {
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("=" * 100)
        [void]$sb.AppendLine("PROCESS: $($proc.Name)  PID=$($proc.Id)")
        [void]$sb.AppendLine("=" * 100)

        # Command line
        try {
            $wmi = Get-CimInstance Win32_Process -Filter "ProcessId=$($proc.Id)" -ErrorAction Stop
            [void]$sb.AppendLine("")
            [void]$sb.AppendLine("Command Line:")
            [void]$sb.AppendLine("  $($wmi.CommandLine)")

            # Extract interesting flags
            $cmdline = $wmi.CommandLine
            $flags = @()
            if ($cmdline -match "--remote-debugging-port=(\d+)") { $flags += "DebugPort=$($Matches[1])" }
            if ($cmdline -match "--inspect=?(\S*)") { $flags += "Inspect=$($Matches[1])" }
            if ($cmdline -match "--extensions-dir=([^\s""]+)") { $flags += "ExtDir=$($Matches[1])" }
            if ($cmdline -match "--user-data-dir=([^\s""]+)") { $flags += "UserData=$($Matches[1])" }
            if ($cmdline -match "--disable-extensions") { $flags += "ExtensionsDisabled" }
            if ($cmdline -match "--enable-proposed-api") { $flags += "ProposedAPIEnabled" }
            if ($cmdline -match "--log=(\w+)") { $flags += "LogLevel=$($Matches[1])" }
            if ($cmdline -match "--crash-reporter-directory=([^\s""]+)") { $flags += "CrashDir=$($Matches[1])" }
            if ($cmdline -match "--type=(\w+)") { $flags += "ProcessType=$($Matches[1])" }
            if ($flags.Count -gt 0) {
                [void]$sb.AppendLine("")
                [void]$sb.AppendLine("Extracted flags:")
                foreach ($f in $flags) { [void]$sb.AppendLine("  - $f") }
            }
        } catch { }

        # Process details
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("Details:")
        [void]$sb.AppendLine("  Path:           $($proc.Path)")
        [void]$sb.AppendLine("  StartTime:      $($proc.StartTime)")
        [void]$sb.AppendLine("  WorkingSet:     $([math]::Round($proc.WorkingSet64/1MB, 1)) MB")
        [void]$sb.AppendLine("  PrivateBytes:   $([math]::Round($proc.PrivateMemorySize64/1MB, 1)) MB")
        [void]$sb.AppendLine("  VirtualMem:     $([math]::Round($proc.VirtualMemorySize64/1MB, 1)) MB")
        [void]$sb.AppendLine("  Threads:        $($proc.Threads.Count)")
        [void]$sb.AppendLine("  Handles:        $($proc.HandleCount)")
        [void]$sb.AppendLine("  CPU(s):         $([math]::Round($proc.TotalProcessorTime.TotalSeconds, 2))")
        [void]$sb.AppendLine("  MainWindow:     $($proc.MainWindowTitle)")
        [void]$sb.AppendLine("  Priority:       $($proc.PriorityClass)")

        # Loaded modules (DLLs) — look for interesting ones
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("Loaded modules (filtered):")
        try {
            $modules = $proc.Modules | Sort-Object ModuleName
            $interesting = $modules | Where-Object {
                $_.ModuleName -match "electron|node|v8|chromium|icu|ssl|crypto|sqlite|napi|addon|native"
            }
            foreach ($mod in $interesting) {
                [void]$sb.AppendLine("  $($mod.ModuleName) | $($mod.FileName) | $([math]::Round($mod.ModuleMemorySize/1KB))KB")
            }
            [void]$sb.AppendLine("  (Total loaded: $($modules.Count) modules)")
        } catch {
            [void]$sb.AppendLine("  (Access denied — run elevated for full module list)")
        }

        # Environment variables (from the process, if accessible)
        try {
            $envBlock = (Get-CimInstance Win32_Process -Filter "ProcessId=$($proc.Id)").GetOwner()
            [void]$sb.AppendLine("")
            [void]$sb.AppendLine("Process owner: $($envBlock.Domain)\$($envBlock.User)")
        } catch { }
    }

    # Child process tree
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("Process hierarchy:")
    [void]$sb.AppendLine("-" * 80)
    $allProcs = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue
    foreach ($proc in $ideProcs) {
        $children = $allProcs | Where-Object { $_.ParentProcessId -eq $proc.Id }
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("$($proc.Name) (PID $($proc.Id))")
        foreach ($child in $children) {
            $childName = try { (Get-Process -Id $child.ProcessId -ErrorAction Stop).Name } catch { "?" }
            [void]$sb.AppendLine("  └─ $childName (PID $($child.ProcessId)) $($child.CommandLine | Select-Object -First 200)")
            # grandchildren
            $grandchildren = $allProcs | Where-Object { $_.ParentProcessId -eq $child.ProcessId }
            foreach ($gc in $grandchildren) {
                $gcName = try { (Get-Process -Id $gc.ProcessId -ErrorAction Stop).Name } catch { "?" }
                [void]$sb.AppendLine("      └─ $gcName (PID $($gc.ProcessId))")
            }
        }
    }

    [System.IO.File]::WriteAllText($outFile, $sb.ToString(), [System.Text.Encoding]::UTF8)
    Write-Dump "Process intel: $outFile"
}

if ($All -or $ProcessIntel) { Dump-ProcessIntel }

# ============================================================================
# 6. EXTENSION SIDELOAD DUMPER — generate a tiny VSIX that exports internal state
# ============================================================================
function Dump-ExtensionSideload {
    $extDir = Join-Path $OutputDir "rawrxd-state-dumper"
    $outFile = Join-Path $OutputDir "extension_sideload_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
    $sb = New-Object System.Text.StringBuilder

    [void]$sb.AppendLine("Extension Sideload Dumper - $(Get-Date)")
    [void]$sb.AppendLine("=" * 80)

    # Create the extension directory structure
    New-Item -ItemType Directory -Path $extDir -Force | Out-Null

    # package.json
    $packageJson = @{
        name = "rawrxd-state-dumper"
        displayName = "RawrXD State Dumper"
        version = "0.0.1"
        publisher = "rawrxd"
        engines = @{ vscode = "^1.70.0" }
        activationEvents = @("onCommand:rawrxd.dumpState")
        main = "./extension.js"
        contributes = @{
            commands = @(
                @{ command = "rawrxd.dumpState"; title = "RawrXD: Dump IDE State" }
            )
        }
    } | ConvertTo-Json -Depth 5
    Set-Content "$extDir\package.json" $packageJson -Encoding UTF8

    # extension.js — calls vscode.* APIs to extract all available state
    $extensionJs = @'
const vscode = require('vscode');
const fs = require('fs');
const path = require('path');

function activate(context) {
    let cmd = vscode.commands.registerCommand('rawrxd.dumpState', async () => {
        const outDir = 'D:\\rawrxd\\dumps\\chat_panes\\extension_state';
        if (!fs.existsSync(outDir)) fs.mkdirSync(outDir, { recursive: true });
        const ts = new Date().toISOString().replace(/[:.]/g, '-');
        const state = {};

        // 1. Open editors
        state.visibleEditors = vscode.window.visibleTextEditors.map(e => ({
            fileName: e.document?.fileName,
            languageId: e.document?.languageId,
            lineCount: e.document?.lineCount,
            isDirty: e.document?.isDirty,
            selection: { start: e.selection?.start, end: e.selection?.end },
            viewColumn: e.viewColumn
        }));

        // 2. All open tabs
        state.tabs = vscode.window.tabGroups?.all?.flatMap(g =>
            g.tabs.map(t => ({
                label: t.label,
                isActive: t.isActive,
                isPinned: t.isPinned,
                isDirty: t.isDirty,
                group: g.viewColumn,
                uri: t.input?.uri?.toString?.() || ''
            }))
        ) || [];

        // 3. Active terminal info
        state.terminals = vscode.window.terminals.map(t => ({
            name: t.name,
            processId: t.processId,
            creationOptions: t.creationOptions
        }));

        // 4. Workspace info
        state.workspace = {
            name: vscode.workspace.name,
            workspaceFolders: vscode.workspace.workspaceFolders?.map(f => f.uri.fsPath) || [],
            configuration: {}
        };

        // 5. Extensions
        state.extensions = vscode.extensions.all.map(e => ({
            id: e.id,
            isActive: e.isActive,
            extensionPath: e.extensionPath,
            packageJSON: { name: e.packageJSON?.name, version: e.packageJSON?.version, publisher: e.packageJSON?.publisher }
        }));

        // 6. Global/workspace state from extension context
        state.globalState = {};
        try {
            const keys = context.globalState.keys?.() || [];
            for (const key of keys) {
                state.globalState[key] = context.globalState.get(key);
            }
        } catch(e) { state.globalState._error = e.message; }

        // 7. Registered commands
        state.commands = await vscode.commands.getCommands(true);

        // 8. Recent files / workspace history (from settings)
        try {
            const config = vscode.workspace.getConfiguration();
            state.recentConfig = {
                editorFontSize: config.get('editor.fontSize'),
                editorTheme: config.get('workbench.colorTheme'),
                terminalShell: config.get('terminal.integrated.defaultProfile.windows'),
                chatModel: config.get('github.copilot.chat.localeOverride'),
            };
        } catch(e) {}

        // 9. Diagnostics (errors/warnings across all files)
        state.diagnostics = [];
        vscode.languages.getDiagnostics().forEach(([uri, diags]) => {
            diags.forEach(d => {
                state.diagnostics.push({
                    file: uri.fsPath,
                    severity: d.severity,
                    message: d.message,
                    range: d.range
                });
            });
        });

        // 10. Output channels - list what's available
        // (Can't read content of output channels via API, but we can log their existence)

        const outPath = path.join(outDir, `state_${ts}.json`);
        fs.writeFileSync(outPath, JSON.stringify(state, null, 2), 'utf8');

        // Also dump all visible editor content
        for (const editor of vscode.window.visibleTextEditors) {
            try {
                const name = path.basename(editor.document.fileName).replace(/[^a-zA-Z0-9._-]/g, '_');
                const content = editor.document.getText();
                fs.writeFileSync(path.join(outDir, `editor_${name}_${ts}.txt`), content, 'utf8');
            } catch(e) {}
        }

        vscode.window.showInformationMessage(`RawrXD: State dumped to ${outPath}`);
    });

    context.subscriptions.push(cmd);

    // Auto-dump on activation if env var set
    if (process.env.RAWRXD_AUTO_DUMP === '1') {
        vscode.commands.executeCommand('rawrxd.dumpState');
    }
}

function deactivate() {}
module.exports = { activate, deactivate };
'@
    Set-Content "$extDir\extension.js" $extensionJs -Encoding UTF8

    [void]$sb.AppendLine("Extension generated at: $extDir")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("To install in VS Code / Cursor:")
    [void]$sb.AppendLine('  code --install-extension "' + $extDir + '"    (will load as folder)')
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("Or copy to extensions dir:")

    # Find actual extensions directories
    $extDirs = @(
        "$env:USERPROFILE\.vscode\extensions",
        "$env:USERPROFILE\.cursor\extensions",
        "$env:APPDATA\Code\User\extensions",
        "$env:APPDATA\Cursor\User\extensions"
    )
    foreach ($ed in $extDirs) {
        if (Test-Path (Split-Path $ed -Parent)) {
            $dest = Join-Path $ed "rawrxd-state-dumper"
            [void]$sb.AppendLine("  Copy-Item -Recurse '$extDir' '$dest'")
        }
    }

    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("Then run command palette: 'RawrXD: Dump IDE State'")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("Or auto-dump on launch:")
    [void]$sb.AppendLine('  $env:RAWRXD_AUTO_DUMP = "1"; code .')

    # Auto-install if VS Code CLI is available
    $installed = $false
    foreach ($cli in @("code", "cursor")) {
        $cliPath = Get-Command $cli -ErrorAction SilentlyContinue
        if ($cliPath) {
            [void]$sb.AppendLine("")
            [void]$sb.AppendLine("Attempting auto-install via $cli CLI...")
            try {
                # For folder-based extensions, copy to extensions dir
                $targetExt = if ($cli -eq "cursor") { "$env:USERPROFILE\.cursor\extensions\rawrxd-state-dumper" }
                              else { "$env:USERPROFILE\.vscode\extensions\rawrxd-state-dumper" }
                if (-not (Test-Path $targetExt)) {
                    Copy-Item -Recurse $extDir $targetExt -Force
                    [void]$sb.AppendLine("Installed to: $targetExt")
                    [void]$sb.AppendLine("Restart $cli and run: RawrXD: Dump IDE State")
                    $installed = $true
                } else {
                    [void]$sb.AppendLine("Already installed at: $targetExt")
                    $installed = $true
                }
            } catch {
                [void]$sb.AppendLine("Install failed: $($_.Exception.Message)")
            }
        }
    }

    [System.IO.File]::WriteAllText($outFile, $sb.ToString(), [System.Text.Encoding]::UTF8)
    Write-Dump "Extension sideload: $outFile"
    if ($installed) { Write-Dump "Extension installed — restart IDE and run 'RawrXD: Dump IDE State'" Cyan }
}

if ($All -or $ExtensionDump) { Dump-ExtensionSideload }

# ============================================================================
# 7. REAL-TIME FILE WATCHER — stream state.vscdb / log changes as they happen
# ============================================================================
function Start-FileWatcher {
    $outFile = Join-Path $OutputDir "filewatcher_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"

    Write-Dump "Starting real-time file watcher (Ctrl+C to stop)..." Cyan
    Write-Dump "Logging to: $outFile" Cyan

    $watchPaths = @(
        "$env:APPDATA\Cursor",
        "$env:APPDATA\Code"
    ) | Where-Object { Test-Path $_ }

    if ($watchPaths.Count -eq 0) {
        Write-Dump "No Cursor/Code AppData directories found" Yellow
        return
    }

    $watchers = @()
    $interestingExtensions = @("*.vscdb","*.json","*.log","*.sqlite","*.db")

    foreach ($wp in $watchPaths) {
        foreach ($filter in $interestingExtensions) {
            $watcher = New-Object System.IO.FileSystemWatcher
            $watcher.Path = $wp
            $watcher.Filter = $filter
            $watcher.IncludeSubdirectories = $true
            $watcher.NotifyFilter = [System.IO.NotifyFilters]::LastWrite -bor
                                    [System.IO.NotifyFilters]::FileName -bor
                                    [System.IO.NotifyFilters]::Size

            $action = {
                $ts = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss.fff")
                $line = "$ts | $($Event.SourceEventArgs.ChangeType) | $($Event.SourceEventArgs.FullPath)"
                Add-Content $using:outFile $line
                Write-Host $line -ForegroundColor DarkGray

                # If it's a state.vscdb change, snapshot the ItemTable
                if ($Event.SourceEventArgs.FullPath -match "state\.vscdb$") {
                    try {
                        Start-Sleep -Milliseconds 500  # let the write complete
                        $snapFile = Join-Path $using:OutputDir "vscdb_snapshot_$(Get-Date -Format 'HHmmss_fff').txt"
                        # Quick read of ItemTable
                        $bytes = [System.IO.File]::ReadAllBytes($Event.SourceEventArgs.FullPath)
                        [System.IO.File]::WriteAllBytes($snapFile, $bytes[0..([Math]::Min(262144, $bytes.Length - 1))])
                        Add-Content $using:outFile "$ts | SNAPSHOT | $snapFile"
                    } catch { }
                }
            }

            Register-ObjectEvent $watcher Changed -Action $action | Out-Null
            Register-ObjectEvent $watcher Created -Action $action | Out-Null
            Register-ObjectEvent $watcher Renamed -Action $action | Out-Null
            $watcher.EnableRaisingEvents = $true
            $watchers += $watcher
        }
    }

    Write-Dump "Watching $($watchers.Count) file patterns across $($watchPaths.Count) directories"
    Write-Dump "Press Ctrl+C to stop..."

    try {
        while ($true) { Start-Sleep -Seconds 1 }
    } finally {
        foreach ($w in $watchers) {
            $w.EnableRaisingEvents = $false
            $w.Dispose()
        }
        Get-EventSubscriber | Where-Object { $_.SourceObject -is [System.IO.FileSystemWatcher] } |
            Unregister-Event -ErrorAction SilentlyContinue
        Write-Dump "File watchers stopped."
    }
}

if ($Watch) { Start-FileWatcher; return }  # -Watch is exclusive (blocks)

# ============================================================================
# 8. APPDATA FILE DUMPS — Cursor + VS Code paths
# ============================================================================

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
    "$env:USERPROFILE\.cursor",
    "$env:APPDATA\Cursor\logs",
    "$env:APPDATA\Cursor\Backups",
    "$env:APPDATA\Cursor\User\snippets",
    "$env:APPDATA\Cursor\User\keybindings.json",
    "$env:APPDATA\Cursor\User\settings.json",
    "$env:APPDATA\Cursor\User\profiles"
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
    "$env:USERPROFILE\.vscode",
    "$env:APPDATA\Code\logs",
    "$env:APPDATA\Code\Backups",
    "$env:APPDATA\Code\User\snippets",
    "$env:APPDATA\Code\User\profiles"
)

$results = @{ Cursor = @(); Copilot = @() }

# Helper: Read SQLite without System.Data.SQLite (uses Microsoft.Data.Sqlite or raw byte scan)
function Read-SQLiteManaged {
    param([string]$dbPath)
    $content = $null

    # Try 1: System.Data.SQLite (if available)
    try {
        $conn = New-Object System.Data.SQLite.SQLiteConnection("Data Source=$dbPath;Read Only=True;FailIfMissing=False")
        $conn.Open()
        $cmd = $conn.CreateCommand()
        $cmd.CommandText = "SELECT name FROM sqlite_master WHERE type='table'"
        $tables = @(); $r = $cmd.ExecuteReader(); while ($r.Read()) { $tables += $r["name"] }; $r.Close()
        $out = @{}
        foreach ($t in $tables) {
            $cmd.CommandText = "SELECT * FROM [$t] LIMIT 10000"
            $r = $cmd.ExecuteReader()
            $rows = @()
            while ($r.Read()) {
                $row = @{}
                for ($i = 0; $i -lt $r.FieldCount; $i++) {
                    $val = $r.GetValue($i)
                    if ($val -is [byte[]]) { $val = [System.Convert]::ToBase64String($val[0..([Math]::Min(4096, $val.Length - 1))]) }
                    $row[$r.GetName($i)] = $val
                }
                $rows += [PSCustomObject]$row
            }
            $r.Close()
            $out[$t] = $rows
        }
        $conn.Close()
        $content = $out | ConvertTo-Json -Depth 15 -Compress
        return $content
    } catch { }

    # Try 2: sqlite3 CLI (ships with many systems)
    try {
        $sqlite3 = Get-Command sqlite3 -ErrorAction SilentlyContinue
        if ($sqlite3) {
            $tables = & sqlite3 $dbPath ".tables" 2>$null
            $allOut = @{}
            foreach ($t in ($tables -split '\s+' | Where-Object { $_ })) {
                $rows = & sqlite3 $dbPath ".mode json" "SELECT * FROM [$t] LIMIT 5000;" 2>$null
                if ($rows) { $allOut[$t] = $rows -join "`n" }
            }
            $content = $allOut | ConvertTo-Json -Depth 10 -Compress
            return $content
        }
    } catch { }

    # Try 3: Raw binary extraction — scan for UTF-8 strings in the SQLite file
    try {
        $bytes = [System.IO.File]::ReadAllBytes($dbPath)
        $maxLen = [Math]::Min(262144, $bytes.Length)
        $content = [System.Text.Encoding]::UTF8.GetString($bytes[0..($maxLen - 1)])
        return $content
    } catch { }

    return $null
}

function Dump-PathSet {
    param([string[]]$paths, [string]$tag)
    $fileCount = 0
    foreach ($p in $paths) {
        if (-not (Test-Path $p)) { continue }
        # Handle if $p is a file (not directory)
        if (Test-Path $p -PathType Leaf) {
            $files = @(Get-Item $p)
        } else {
            $files = Get-ChildItem $p -Recurse -File -ErrorAction SilentlyContinue
        }
        foreach ($f in $files) {
            $ext = $f.Extension.ToLower()
            $rel = $f.FullName.Replace($p, "").TrimStart("\")
            $dump = $true
            if ($ext -notin ".json",".db",".sqlite",".sqlite3",".vscdb",".log",".txt",".bak","" -and $f.Name -notmatch "state\.vscdb|\.vscdb$") { $dump = $false }
            if ($f.Length -gt 52428800) { $dump = $false }  # skip files > 50MB
            if (-not $dump) { continue }
            try {
                $content = $null
                if ($ext -eq ".json") {
                    $content = Get-Content $f.FullName -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
                }
                elseif ($ext -in ".db",".sqlite",".sqlite3",".vscdb") {
                    $content = Read-SQLiteManaged -dbPath $f.FullName
                }
                else {
                    $maxRead = [Math]::Min(262144, $f.Length)
                    if ($maxRead -gt 0) {
                        $content = [System.Text.Encoding]::UTF8.GetString(
                            [System.IO.File]::ReadAllBytes($f.FullName)[0..($maxRead - 1)])
                    }
                }
                if ($content) {
                    $safe = $rel -replace '[\\/:*?"<>|]','_'
                    [System.IO.File]::WriteAllText("$OutputDir\${tag}_$safe.txt", $content, [System.Text.Encoding]::UTF8)
                    $results[$tag] += [PSCustomObject]@{ Path = $rel; Size = $f.Length; Extension = $ext }
                    $fileCount++
                }
            } catch { }
        }
    }
    Write-Dump "$tag file dump: $fileCount files extracted" Green
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
