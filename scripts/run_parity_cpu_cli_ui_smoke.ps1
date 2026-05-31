# run_parity_cpu_cli_ui_smoke.ps1 — hardware-free end-to-end CLI/UI parity smoke
#
# Exercises both frontends against the same deterministic oracle backend via:
#   - rawrxd.exe run ... --emit-json-trace cli.json
#   - rawrxd-parity-ui-driver ... with RAWRXD_PIPELINE_TRACE=ui.json
#
# This is the strongest local proof available on non-GPU machines because it
# executes both the CLI path and the UI pipeline path, then diffs them.

[CmdletBinding()]
param(
    [string]$BinDir = "d:\rawrxd\build_pipeline\bin",
    [string]$OutDir = "d:\rawrxd\build_pipeline\parity_cpu_cli_ui",
    [string]$Model  = "parity-test-model",
    [string]$Prompt = "validate cli ui parity end to end",
    [int]$NumPredict = 32
)

$ErrorActionPreference = 'Stop'
$cli = Join-Path $BinDir 'rawrxd.exe'
$ui  = Join-Path $BinDir 'rawrxd-parity-ui-driver.exe'
$cmp = Join-Path $PSScriptRoot 'compare_parity_trace.ps1'

if (!(Test-Path $cli)) { throw "Missing CLI binary: $cli" }
if (!(Test-Path $ui))  { throw "Missing UI driver binary: $ui" }
if (!(Test-Path $cmp)) { throw "Missing comparator: $cmp" }

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$cliTrace = Join-Path $OutDir 'cli.json'
$uiTrace  = Join-Path $OutDir 'ui.json'
Remove-Item -Force $cliTrace, $uiTrace -ErrorAction SilentlyContinue

$env:RAWRXD_PARITY_CPU = '1'

function Invoke-NativeQuiet {
    param(
        [string]$Exe,
        [string[]]$InvocationArgs,
        [string]$Stem
    )
    $stdout = Join-Path $OutDir ($Stem + '.stdout.txt')
    $stderr = Join-Path $OutDir ($Stem + '.stderr.txt')
    Remove-Item -Force $stdout, $stderr -ErrorAction SilentlyContinue
    $argLine = ($InvocationArgs | ForEach-Object {
        '"' + (($_ -replace '"', '\"')) + '"'
    }) -join ' '
    $proc = Start-Process -FilePath $Exe `
        -ArgumentList $argLine `
        -RedirectStandardOutput $stdout `
        -RedirectStandardError $stderr `
        -NoNewWindow -PassThru -Wait
    return $proc.ExitCode
}

$cliArgs = @('run', [string]$Model, '--prompt', [string]$Prompt, '--max-tokens', [string]$NumPredict, '--emit-json-trace', [string]$cliTrace)
$code = Invoke-NativeQuiet -Exe $cli -InvocationArgs $cliArgs -Stem 'cli'
if ($code -ne 0) { throw "CLI run failed with exit $code" }
if (!(Test-Path $cliTrace)) { throw "CLI trace missing: $cliTrace" }

$env:RAWRXD_PIPELINE_TRACE = $uiTrace
$uiArgs = @('--model', [string]$Model, '--prompt', [string]$Prompt, '--num-predict', [string]$NumPredict)
$code = Invoke-NativeQuiet -Exe $ui -InvocationArgs $uiArgs -Stem 'ui'
if ($code -ne 0) { throw "UI driver failed with exit $code" }
if (!(Test-Path $uiTrace)) { throw "UI trace missing: $uiTrace" }

& $cmp -Cli $cliTrace -Ui $uiTrace
if ($LASTEXITCODE -ne 0) {
    throw "CLI/UI parity diff failed (exit $LASTEXITCODE)"
}

$a = Get-Content $cliTrace -Raw | ConvertFrom-Json
$b = Get-Content $uiTrace  -Raw | ConvertFrom-Json
Write-Host "[OK] CLI/UI CPU smoke passed: backend=$($a.backend)/$($b.backend), tokens=$($a.token_count)/$($b.token_count)"