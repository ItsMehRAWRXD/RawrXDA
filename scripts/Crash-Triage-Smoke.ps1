param(
    [string]$ExePath = "D:\rawrxd\build_pipeline\bin\RawrXD-Win32IDE.exe",
    [string]$ModelPath = "D:\phi3mini.gguf",
    [string]$Prompt = "Say exactly: AGENTIC_TEST_OK",
    [int]$MaxTokens = 16,
    [int]$TimeoutMs = 120000
)

$ErrorActionPreference = 'Stop'

function Run-Variant([string]$Name, [string[]]$Flags) {
    Write-Host "`n=== $Name ==="
    $args = @(
        '--chat-ui-smoke-noninteractive',
        '--test-model', $ModelPath,
        '--test-prompt', $Prompt,
        '--test-max-tokens', "$MaxTokens",
        '--test-timeout-ms', "$TimeoutMs"
    ) + $Flags

    $outFile = "D:\rawrxd\tmp\crash_triage_${Name}.out.txt"
    $errFile = "D:\rawrxd\tmp\crash_triage_${Name}.err.txt"
    $p = Start-Process -FilePath $ExePath -ArgumentList $args -RedirectStandardOutput $outFile -RedirectStandardError $errFile -NoNewWindow -PassThru -Wait
    Write-Host "exit=$($p.ExitCode) out=$outFile err=$errFile"
}

$env:RAWRXD_SMOKE_CRASH_TRIAGE_LOG = '1'
$env:RAWRXD_SMOKE_CRASH_TRIAGE_BREAK = '0'

Run-Variant -Name 'baseline' -Flags @()
Run-Variant -Name 'no_stream' -Flags @('--no-stream')
Run-Variant -Name 'force_cpu' -Flags @('--force-cpu')
Run-Variant -Name 'no_ui_binding' -Flags @('--no-ui-binding')

Remove-Item Env:\RAWRXD_SMOKE_CRASH_TRIAGE_LOG -ErrorAction SilentlyContinue
Remove-Item Env:\RAWRXD_SMOKE_CRASH_TRIAGE_BREAK -ErrorAction SilentlyContinue

Write-Host "Done. Inspect D:\rawrxd\tmp\crash_triage_*.out.txt and *.err.txt"
