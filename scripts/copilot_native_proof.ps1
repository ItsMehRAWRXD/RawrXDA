param(
    [string]$ExePath = "",
    [string]$ModelPath = "F:\OllamaModels\Phi-3-mini-4k-instruct-q8_0.gguf",
    [string]$Prompt = "<|user|>`ncreate a react server<|end|>`n<|assistant|>`n",
    [int]$MaxTokens = 16,
    [int]$TimeoutSeconds = 360,
    [string]$OutFile = "",
    [string]$StatusFile = "",
    [string]$VerboseFile = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ExePath) {
    foreach ($c in @(
            (Join-Path $repoRoot "build-win32\bin\RawrEngine.exe"),
            (Join-Path $repoRoot "build-ninja\bin\RawrEngine.exe"),
            (Join-Path $repoRoot "build\bin\RawrEngine.exe"))) {
        if (Test-Path -LiteralPath $c) { $ExePath = $c; break }
    }
}
if (-not $ExePath) {
    $ExePath = Join-Path $repoRoot "build-win32\bin\RawrEngine.exe"
}
if (-not $OutFile) { $OutFile = Join-Path $repoRoot "rawrengine_cli_proof.txt" }
if (-not $StatusFile) { $StatusFile = Join-Path $repoRoot "_proof_status.txt" }
if (-not $VerboseFile) { $VerboseFile = Join-Path $repoRoot "_infer_verbose.txt" }

"EXE=$ExePath" | Set-Content -Path $StatusFile
"MODEL=$ModelPath" | Add-Content -Path $StatusFile
"PROMPT=$Prompt" | Add-Content -Path $StatusFile
"MAX_TOKENS=$MaxTokens" | Add-Content -Path $StatusFile

# Verbose OFF: keeps stdout clean (only generated text, no printf noise)
$env:RAWRXD_INFER_VERBOSE = "0"

if (-not (Test-Path -LiteralPath $ExePath)) {
    "ERROR=EXE_NOT_FOUND" | Add-Content -Path $StatusFile
    exit 2
}

if (-not (Test-Path -LiteralPath $ModelPath)) {
    "ERROR=MODEL_NOT_FOUND" | Add-Content -Path $StatusFile
    exit 3
}

$modelSize = (Get-Item -LiteralPath $ModelPath).Length
"MODEL_SIZE=$modelSize" | Add-Content -Path $StatusFile

$argList = @(
    "--infer", $ModelPath,
    "--prompt", $Prompt,
    "--max-tokens", "$MaxTokens"
)

$tmpOut = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), ("rawr_proof_out_{0}.txt" -f ([guid]::NewGuid().ToString("N"))))
$tmpErr = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), ("rawr_proof_err_{0}.txt" -f ([guid]::NewGuid().ToString("N"))))

$proc = Start-Process -FilePath $ExePath -ArgumentList $argList -PassThru -NoNewWindow -RedirectStandardOutput $tmpOut -RedirectStandardError $tmpErr
$timedOut = $false
if ($proc.WaitForExit($TimeoutSeconds * 1000)) {
    "EXITCODE=$($proc.ExitCode)" | Add-Content -Path $StatusFile
} else {
    Stop-Process -Id $proc.Id -Force
    $proc.WaitForExit(5000) | Out-Null  # brief drain so pipe buffers flush
    "TIMEOUT=1" | Add-Content -Path $StatusFile
    $timedOut = $true
}

# Always capture both stdout and stderr regardless of timeout
if (Test-Path -LiteralPath $tmpOut) {
    Get-Content -LiteralPath $tmpOut -Raw | Set-Content -Path $OutFile
}
# stderr = verbose stage diagnostics; write to separate verbose file for inspection
if (Test-Path -LiteralPath $tmpErr) {
    Get-Content -LiteralPath $tmpErr -Raw | Set-Content -Path $VerboseFile
}
Remove-Item -LiteralPath $tmpOut -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $tmpErr -Force -ErrorAction SilentlyContinue

if ($timedOut) { exit 124 }
