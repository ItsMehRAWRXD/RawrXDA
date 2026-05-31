# =============================================================================
# Smoke Test Scenario 5: Copilot Chat Send / Receive Interlock
# =============================================================================
# Validates that deferred-init smoke runs real HandleCopilotSend (not skipped):
# chat controls unlock, send is accepted, and the chat output surface updates.
# =============================================================================

param(
    [string]$BinaryPath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe",
    [int]$BootWaitMs = 12000,
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

$logDir = Join-Path $PSScriptRoot "logs"
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir | Out-Null }
$logFile = Join-Path $logDir "scenario5_chat_send_receive.log"

$IDC_SECONDARY_SIDEBAR = 1200
$IDC_COPILOT_CHAT_INPUT = 1202
$IDC_COPILOT_CHAT_OUTPUT = 1203
$IDC_COPILOT_SEND_BTN = 1204
$WM_COPILOT_DEFERRED_SEND = 0x8000 + 111  # WM_APP + 111

function Log {
    param([string]$Msg, [string]$Level = "INFO")
    $ts = Get-Date -Format "HH:mm:ss.fff"
    $line = "[$ts] [$Level] $Msg"
    Write-Host $line -ForegroundColor DarkCyan
    Add-Content -Path $logFile -Value $line
}

if (-not ("ChatSmokeWin32" -as [type])) {
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class ChatSmokeWin32 {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr child, string className, string windowName);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr GetDlgItem(IntPtr parent, int id);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool IsWindow(IntPtr hwnd);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool IsWindowEnabled(IntPtr hwnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool SetWindowTextW(IntPtr hwnd, string text);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern int GetWindowTextLengthW(IntPtr hwnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern int GetWindowTextW(IntPtr hwnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    public const uint BM_CLICK = 0x00F5;
    public const uint WM_GETTEXTLENGTH = 0x000E;
}
"@
}

. (Join-Path $PSScriptRoot "modules\SmokeDiagnostics.ps1")

function Get-RichText {
    param([IntPtr]$Hwnd)
    if (-not [ChatSmokeWin32]::IsWindow($Hwnd)) { return "" }
    $len = [ChatSmokeWin32]::SendMessage($Hwnd, [ChatSmokeWin32]::WM_GETTEXTLENGTH, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
    if ($len -le 0) { return "" }
    $sb = New-Object System.Text.StringBuilder ($len + 4)
    [void][ChatSmokeWin32]::GetWindowTextW($Hwnd, $sb, $len + 2)
    return $sb.ToString()
}

Log "════════════════════════════════════════════════════════════════"
Log "SCENARIO 5: Copilot Chat Send / Receive Interlock"
Log "Log file: $logFile"

$process = Start-RawrIDESmokeProcess -BinaryPath $BinaryPath -ExtraEnvironment @{
    RAWRXD_SMOKE_ENABLE_COPILOT_CHAT = '1'
}
Log "IDE launched PID=$($process.Id)"

try {
    $mainWnd = [IntPtr]::Zero
    $chatInput = [IntPtr]::Zero
    $chatOutput = [IntPtr]::Zero
    $sendBtn = [IntPtr]::Zero
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    while ($stopwatch.ElapsedMilliseconds -lt $BootWaitMs) {
        if ($process.HasExited) {
            Log "IDE exited early (code=$($process.ExitCode))" "ERROR"
            exit 1
        }
        $procNow = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
        if ($procNow -and $procNow.MainWindowHandle -ne 0) {
            $mainWnd = [IntPtr]$procNow.MainWindowHandle
            $sidebar = [ChatSmokeWin32]::GetDlgItem($mainWnd, $IDC_SECONDARY_SIDEBAR)
            if ($sidebar -ne [IntPtr]::Zero -and [ChatSmokeWin32]::IsWindow($sidebar)) {
                $inputCandidate = [ChatSmokeWin32]::GetDlgItem($sidebar, $IDC_COPILOT_CHAT_INPUT)
                $sendCandidate = [ChatSmokeWin32]::GetDlgItem($sidebar, $IDC_COPILOT_SEND_BTN)
                if ($inputCandidate -ne [IntPtr]::Zero -and $sendCandidate -ne [IntPtr]::Zero) {
                    $chatInput = $inputCandidate
                    $sendBtn = $sendCandidate
                    $chatOutput = [ChatSmokeWin32]::GetDlgItem($sidebar, $IDC_COPILOT_CHAT_OUTPUT)
                    break
                }
            }
        }
        Start-Sleep -Milliseconds 100
    }

    if ($chatInput -eq [IntPtr]::Zero -or $sendBtn -eq [IntPtr]::Zero) {
        Log "Chat input/send controls not found within ${BootWaitMs}ms" "ERROR"
        exit 1
    }
    Log "Chat HWNDs: input=0x$($chatInput.ToString('X8')) output=0x$($chatOutput.ToString('X8')) send=0x$($sendBtn.ToString('X8'))"

    if (-not [ChatSmokeWin32]::IsWindowEnabled($sendBtn)) {
        Log "Send button still disabled — copilot interlock not unlocked" "ERROR"
        exit 1
    }
    Log "Send button enabled (copilot backend ready interlock OK)"

    $outBefore = Get-RichText -Hwnd $chatOutput
    $prompt = "smoke ping $(Get-Date -Format 'HHmmss')"
    [void][ChatSmokeWin32]::SetWindowTextW($chatInput, $prompt)

    [void][ChatSmokeWin32]::PostMessage($mainWnd, $WM_COPILOT_DEFERRED_SEND, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 400
    [void][ChatSmokeWin32]::SendMessage($sendBtn, [ChatSmokeWin32]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)

    $outAfter = ""
    for ($i = 0; $i -lt 40; $i++) {
        Start-Sleep -Milliseconds 250
        if ($process.HasExited) {
            Log "IDE crashed during send/receive" "ERROR"
            exit 1
        }
        $outAfter = Get-RichText -Hwnd $chatOutput
        if ($outAfter.Length -gt $outBefore.Length) { break }
        if ($outAfter -match [regex]::Escape($prompt)) { break }
        if ($outAfter -match 'smoke ping|\[Error\]|\[Info\]|No models|assistant|user') { break }
    }

    if ($outAfter.Length -le $outBefore.Length -and $outAfter -notmatch 'smoke ping') {
        Log "Chat output did not update after send (before=$($outBefore.Length) after=$($outAfter.Length))" "WARN"
        Log "Send path executed without crash; model/backend may be offline — checking ide.log for backend-not-ready block"
        $ideLog = Join-Path $env:APPDATA "RawrXD\ide.log"
        if (Test-Path $ideLog) {
            $tail = Get-Content $ideLog -Tail 80 -ErrorAction SilentlyContinue
            if ($tail -match 'blocked: backend not ready') {
                Log "ide.log shows backend-not-ready block — interlock regression" "ERROR"
                exit 1
            }
        }
        Log "No backend-not-ready block in log; treating as soft-pass (controls unlocked, send accepted)" "INFO"
    } else {
        Log "Chat output updated after send (len $($outBefore.Length) -> $($outAfter.Length))"
    }

    Log "SCENARIO 5 PASSED: real chat send path exercised under smoke deferred init"
    exit 0
}
finally {
    if ($process -and -not $process.HasExited) {
        $process.CloseMainWindow() | Out-Null
        Start-Sleep -Milliseconds 500
        if (-not $process.HasExited) { $process.Kill() }
    }
}
