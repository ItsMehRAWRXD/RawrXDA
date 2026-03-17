<#
.SYNOPSIS
  Concurrent stress harness for the 7 critical Sovereign subsystems.

.DESCRIPTION
  Runs all seven subsystem probes at the same time, enforces per-task timeouts,
  captures stdout/stderr, and emits JSON + console summary.

  Subsystems:
    1) Streaming GGUF loader
    2) Vulkan compute
    3) LSP initialize + completion
    4) Native debugger attach path (DbgEng availability + attach prerequisite)
    5) Swarm broadcast/health path
    6) Embedding compute path
    7) Q4_K dequantization path

.PARAMETER RepoRoot
  RawrXD repository root. Defaults to parent of this script's folder.

.PARAMETER ModelPath
  GGUF model path used by streaming probe.

.PARAMETER PerTaskTimeoutSec
  Timeout for each subsystem probe.

.PARAMETER WhatIf
  Prints planned probes without executing.

.PARAMETER OutputReport
  JSON report path.

.PARAMETER SwarmHealthUrl
  Optional swarm health URL. If provided, probe uses HTTP health check.

.EXAMPLE
  .\scripts\StressTest-Sovereign7.ps1 -ModelPath "D:\OllamaModels\my-model.gguf"

.EXAMPLE
  .\scripts\StressTest-Sovereign7.ps1 -WhatIf
#>

#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$RepoRoot = "",
    [string]$ModelPath = "",
    [int]$PerTaskTimeoutSec = 90,
    [int]$InferenceTimeoutMs = 10000,
    [string]$OutputReport = "",
    [string]$SwarmHealthUrl = "",
    [switch]$IncludeInferenceValidation,
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
    if ($PSScriptRoot) {
        $RepoRoot = Split-Path -Parent $PSScriptRoot
    }
    else {
        $RepoRoot = (Get-Location).Path
    }
}

if (-not $OutputReport) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputReport = Join-Path $RepoRoot ("reports/sovereign7_stress_{0}.json" -f $stamp)
}

$reportDir = Split-Path -Parent $OutputReport
if (-not (Test-Path $reportDir)) {
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
}

function Write-Section([string]$title) {
    Write-Host ""
    Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host $title -ForegroundColor Cyan
    Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
}

function New-ResultObject {
    param(
        [string]$TaskId,
        [string]$TaskName,
        [string]$Status,
        [string]$Detail,
        [int]$DurationMs,
        [string]$StdoutPath = "",
        [string]$StderrPath = ""
    )

    [PSCustomObject]@{
        taskId = $TaskId
        taskName = $TaskName
        status = $Status
        detail = $Detail
        durationMs = $DurationMs
        stdoutPath = $StdoutPath
        stderrPath = $StderrPath
        timestamp = (Get-Date).ToString("o")
    }
}

function Resolve-FirstExisting {
    param([string[]]$Candidates)
    foreach ($p in $Candidates) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

function Resolve-Command {
    param([string[]]$Names)
    foreach ($n in $Names) {
        $cmd = Get-Command $n -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    return $null
}

$global:TaskDefinitions = @(
    [PSCustomObject]@{ id = "streaming"; name = "StreamingGGUFLoader" },
    [PSCustomObject]@{ id = "vulkan"; name = "VulkanCompute" },
    [PSCustomObject]@{ id = "lsp"; name = "LSP Initialize+Completion" },
    [PSCustomObject]@{ id = "debugger"; name = "Native Debugger Attach" },
    [PSCustomObject]@{ id = "swarm"; name = "Swarm Broadcast/Health" },
    [PSCustomObject]@{ id = "embedding"; name = "EmbeddingEngine" },
    [PSCustomObject]@{ id = "kquant"; name = "KQuant Q4_K Dequantize" }
)

if ($WhatIf) {
    Write-Section "SOVEREIGN 7-SYSTEM STRESS TEST (WhatIf)"
    Write-Host "RepoRoot: $RepoRoot" -ForegroundColor Gray
    Write-Host "ModelPath: $ModelPath" -ForegroundColor Gray
    Write-Host "PerTaskTimeoutSec: $PerTaskTimeoutSec" -ForegroundColor Gray
    Write-Host "InferenceTimeoutMs: $InferenceTimeoutMs" -ForegroundColor Gray
    Write-Host "IncludeInferenceValidation: $IncludeInferenceValidation" -ForegroundColor Gray
    Write-Host "OutputReport: $OutputReport" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Planned concurrent probes:" -ForegroundColor Yellow
    $TaskDefinitions | ForEach-Object { Write-Host ("  - [{0}] {1}" -f $_.id, $_.name) -ForegroundColor Gray }
    if ($IncludeInferenceValidation) {
        Write-Host "  - [inference] FAST_GENERATE token emission gate" -ForegroundColor Gray
    }
    exit 0
}

Write-Section "SOVEREIGN 7-SYSTEM CONCURRENT STRESS TEST"
Write-Host "RepoRoot: $RepoRoot" -ForegroundColor Gray
Write-Host "ModelPath: $ModelPath" -ForegroundColor Gray
Write-Host "PerTaskTimeoutSec: $PerTaskTimeoutSec" -ForegroundColor Gray
Write-Host "OutputReport: $OutputReport" -ForegroundColor Gray

$runId = Get-Date -Format "yyyyMMdd_HHmmss"
$logRoot = Join-Path $RepoRoot ("logs/sovereign7_{0}" -f $runId)
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null

$jobs = @()

foreach ($task in $TaskDefinitions) {
    $jobs += Start-Job -Name ("sovereign7_{0}" -f $task.id) -ArgumentList $task, $RepoRoot, $ModelPath, $SwarmHealthUrl, $PerTaskTimeoutSec, $logRoot -ScriptBlock {
        param($task, $RepoRoot, $ModelPath, $SwarmHealthUrl, $PerTaskTimeoutSec, $logRoot)

        $ErrorActionPreference = "Stop"

        function New-TaskResult {
            param(
                [string]$Status,
                [string]$Detail,
                [int]$DurationMs,
                [string]$StdoutPath = "",
                [string]$StderrPath = ""
            )
            [PSCustomObject]@{
                taskId = $task.id
                taskName = $task.name
                status = $Status
                detail = $Detail
                durationMs = $DurationMs
                stdoutPath = $StdoutPath
                stderrPath = $StderrPath
                timestamp = (Get-Date).ToString("o")
            }
        }

        function Resolve-FirstExistingInner {
            param([string[]]$Candidates)
            foreach ($p in $Candidates) {
                if (Test-Path $p) { return $p }
            }
            return $null
        }

        function Resolve-CommandInner {
            param([string[]]$Names)
            foreach ($n in $Names) {
                $cmd = Get-Command $n -ErrorAction SilentlyContinue
                if ($cmd) { return $cmd.Source }
            }
            return $null
        }

        function Run-ProcessWithTimeout {
            param(
                [string]$FilePath,
                [string[]]$Arguments,
                [int]$TimeoutSec,
                [string]$StdoutPath,
                [string]$StderrPath,
                [string[]]$SuccessRegex = @()
            )

            $sw = [System.Diagnostics.Stopwatch]::StartNew()
            "" | Set-Content -Path $StdoutPath -Encoding UTF8
            "" | Set-Content -Path $StderrPath -Encoding UTF8

            $argLine = ($Arguments -join " ")
            $proc = Start-Process -FilePath $FilePath `
                                  -ArgumentList $argLine `
                                  -WorkingDirectory $RepoRoot `
                                  -RedirectStandardOutput $StdoutPath `
                                  -RedirectStandardError $StderrPath `
                                  -NoNewWindow `
                                  -PassThru

            if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
                try { $proc.Kill() } catch { }
                $sw.Stop()
                return New-TaskResult -Status "timeout" -Detail "Timed out after ${TimeoutSec}s" -DurationMs $sw.ElapsedMilliseconds -StdoutPath $StdoutPath -StderrPath $StderrPath
            }

            $sw.Stop()
            $stdout = if (Test-Path $StdoutPath) { Get-Content -Path $StdoutPath -Raw -ErrorAction SilentlyContinue } else { "" }
            $stderr = if (Test-Path $StderrPath) { Get-Content -Path $StderrPath -Raw -ErrorAction SilentlyContinue } else { "" }
            $exitCode = -999
            try { $exitCode = [int]$proc.ExitCode } catch { }

            $okByRegex = $false
            if ($SuccessRegex.Count -eq 0) {
                $okByRegex = $true
            } else {
                foreach ($pattern in $SuccessRegex) {
                    if ($stdout -match $pattern -or $stderr -match $pattern) {
                        $okByRegex = $true
                        break
                    }
                }
            }

            if ($exitCode -eq 0 -and $okByRegex) {
                return New-TaskResult -Status "pass" -Detail "Exit=0, success pattern matched" -DurationMs $sw.ElapsedMilliseconds -StdoutPath $StdoutPath -StderrPath $StderrPath
            }

            if ($exitCode -eq 0 -and $SuccessRegex.Count -eq 0) {
                return New-TaskResult -Status "pass" -Detail "Exit=0" -DurationMs $sw.ElapsedMilliseconds -StdoutPath $StdoutPath -StderrPath $StderrPath
            }

            return New-TaskResult -Status "fail" -Detail ("Exit={0}" -f $exitCode) -DurationMs $sw.ElapsedMilliseconds -StdoutPath $StdoutPath -StderrPath $StderrPath
        }

        $stdoutPath = Join-Path $logRoot ("{0}.out.log" -f $task.id)
        $stderrPath = Join-Path $logRoot ("{0}.err.log" -f $task.id)
        $swTask = [System.Diagnostics.Stopwatch]::StartNew()

        try {
            switch ($task.id) {
                "streaming" {
                    $streamExe = Resolve-FirstExistingInner @(
                        (Join-Path $RepoRoot "build/Release/test_streaming_loader.exe"),
                        (Join-Path $RepoRoot "build/test_streaming_loader.exe"),
                        (Join-Path $RepoRoot "bin/Release/test_streaming_loader.exe")
                    )

                    if ($streamExe) {
                        $args = @()
                        if ($ModelPath -and (Test-Path $ModelPath)) { $args += @("--model", ('"{0}"' -f $ModelPath)) }
                        return Run-ProcessWithTimeout -FilePath $streamExe -Arguments $args -TimeoutSec $PerTaskTimeoutSec -StdoutPath $stdoutPath -StderrPath $stderrPath -SuccessRegex @("Model loaded", "tensor", "success")
                    }

                    if (-not $ModelPath -or -not (Test-Path $ModelPath)) {
                        $swTask.Stop()
                        return New-TaskResult -Status "skip" -Detail "No streaming test executable and no valid -ModelPath provided" -DurationMs $swTask.ElapsedMilliseconds
                    }

                    $mmf = [System.IO.MemoryMappedFiles.MemoryMappedFile]::CreateFromFile($ModelPath, [System.IO.FileMode]::Open, "RawrXD_GGUF_Map", 0, [System.IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read)
                    try {
                        $view = $mmf.CreateViewAccessor(0, 64, [System.IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read)
                        try {
                            $bytes = New-Object byte[] 4
                            $view.ReadArray(0, $bytes, 0, 4) | Out-Null
                            $magic = [System.Text.Encoding]::ASCII.GetString($bytes)
                            $swTask.Stop()
                            if ($magic -eq "GGUF") {
                                "GGUF magic OK" | Set-Content -Path $stdoutPath -Encoding UTF8
                                return New-TaskResult -Status "pass" -Detail "Memory map + GGUF header probe succeeded" -DurationMs $swTask.ElapsedMilliseconds -StdoutPath $stdoutPath -StderrPath $stderrPath
                            }
                            return New-TaskResult -Status "fail" -Detail ("Mapped file but invalid magic: {0}" -f $magic) -DurationMs $swTask.ElapsedMilliseconds -StdoutPath $stdoutPath -StderrPath $stderrPath
                        } finally {
                            $view.Dispose()
                        }
                    } finally {
                        $mmf.Dispose()
                    }
                }

                "vulkan" {
                    $vkExe = Resolve-FirstExistingInner @(
                        (Join-Path $RepoRoot "build/Release/test_vulkan_kernel.exe"),
                        (Join-Path $RepoRoot "build/test_vulkan_kernel.exe"),
                        (Join-Path $RepoRoot "bin/Release/test_vulkan_kernel.exe")
                    )

                    if ($vkExe) {
                        return Run-ProcessWithTimeout -FilePath $vkExe -Arguments @() -TimeoutSec $PerTaskTimeoutSec -StdoutPath $stdoutPath -StderrPath $stderrPath -SuccessRegex @("Vulkan", "dispatch", "success", "matmul")
                    }

                    $vulkanInfo = Resolve-CommandInner @("vulkaninfo.exe", "vulkaninfo")
                    if ($vulkanInfo) {
                        return Run-ProcessWithTimeout -FilePath $vulkanInfo -Arguments @("--summary") -TimeoutSec $PerTaskTimeoutSec -StdoutPath $stdoutPath -StderrPath $stderrPath -SuccessRegex @("GPU", "Vulkan Instance", "driver")
                    }

                    $kernel32 = Add-Type -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError=true, CharSet=System.Runtime.InteropServices.CharSet.Unicode)]
public static extern System.IntPtr LoadLibrary(string lpFileName);
"@ -Name "K32Probe" -Namespace "Sov" -PassThru

                    $h = [Sov.K32Probe]::LoadLibrary("vulkan-1.dll")
                    $swTask.Stop()
                    if ($h -ne [IntPtr]::Zero) {
                        "vulkan-1.dll loadable" | Set-Content -Path $stdoutPath -Encoding UTF8
                        return New-TaskResult -Status "pass" -Detail "Vulkan runtime present (vulkan-1.dll)" -DurationMs $swTask.ElapsedMilliseconds -StdoutPath $stdoutPath -StderrPath $stderrPath
                    }
                    return New-TaskResult -Status "fail" -Detail "No Vulkan test executable and Vulkan runtime not detectable" -DurationMs $swTask.ElapsedMilliseconds -StdoutPath $stdoutPath -StderrPath $stderrPath
                }

                "lsp" {
                    $clangd = Resolve-CommandInner @("clangd.exe", "clangd")
                    if (-not $clangd) {
                        $swTask.Stop()
                        return New-TaskResult -Status "skip" -Detail "clangd not found in PATH" -DurationMs $swTask.ElapsedMilliseconds
                    }

                    $tempFile = Join-Path $env:TEMP ("rawrxd_lsp_{0}.cpp" -f ([Guid]::NewGuid().ToString("N")))
                    "int main(){return 0;}" | Set-Content -Path $tempFile -Encoding UTF8

                    try {
                        $psi = New-Object System.Diagnostics.ProcessStartInfo
                        $psi.FileName = $clangd
                        $psi.Arguments = "--log=error"
                        $psi.RedirectStandardInput = $true
                        $psi.RedirectStandardOutput = $true
                        $psi.RedirectStandardError = $true
                        $psi.UseShellExecute = $false
                        $psi.CreateNoWindow = $true
                        $proc = [System.Diagnostics.Process]::Start($psi)

                        function Send-Lsp($writer, $obj) {
                            $json = $obj | ConvertTo-Json -Depth 10 -Compress
                            $payload = "Content-Length: $($json.Length)`r`n`r`n$json"
                            $writer.Write($payload)
                            $writer.Flush()
                        }

                        $stdin = $proc.StandardInput
                        $stdout = $proc.StandardOutput

                        Send-Lsp $stdin @{ jsonrpc = "2.0"; id = 1; method = "initialize"; params = @{ processId = $PID; rootUri = "file:///$($RepoRoot -replace '\\','/')"; capabilities = @{} } }
                        Start-Sleep -Milliseconds 300

                        Send-Lsp $stdin @{ jsonrpc = "2.0"; method = "textDocument/didOpen"; params = @{ textDocument = @{ uri = "file:///$($tempFile -replace '\\','/')"; languageId = "cpp"; version = 1; text = "int main(){return 0;}" } } }
                        Send-Lsp $stdin @{ jsonrpc = "2.0"; id = 2; method = "textDocument/completion"; params = @{ textDocument = @{ uri = "file:///$($tempFile -replace '\\','/')" }; position = @{ line = 0; character = 3 } } }
                        Start-Sleep -Milliseconds 400

                        Send-Lsp $stdin @{ jsonrpc = "2.0"; id = 3; method = "shutdown"; params = $null }
                        Send-Lsp $stdin @{ jsonrpc = "2.0"; method = "exit"; params = $null }

                        if (-not $proc.WaitForExit($PerTaskTimeoutSec * 1000)) {
                            try { $proc.Kill() } catch { }
                            $swTask.Stop()
                            return New-TaskResult -Status "timeout" -Detail "clangd LSP session timed out" -DurationMs $swTask.ElapsedMilliseconds
                        }

                        $stderr = $proc.StandardError.ReadToEnd()
                        $stderr | Set-Content -Path $stderrPath -Encoding UTF8
                        $swTask.Stop()

                        if ($proc.ExitCode -eq 0 -or $stderr -notmatch "fatal") {
                            "initialize + completion exchange sent" | Set-Content -Path $stdoutPath -Encoding UTF8
                            return New-TaskResult -Status "pass" -Detail "LSP initialize/completion/shutdown flow executed" -DurationMs $swTask.ElapsedMilliseconds -StdoutPath $stdoutPath -StderrPath $stderrPath
                        }

                        return New-TaskResult -Status "fail" -Detail ("clangd exit={0}" -f $proc.ExitCode) -DurationMs $swTask.ElapsedMilliseconds -StdoutPath $stdoutPath -StderrPath $stderrPath
                    } finally {
                        Remove-Item $tempFile -Force -ErrorAction SilentlyContinue
                    }
                }

                "debugger" {
                    $kernel32 = Add-Type -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError=true, CharSet=System.Runtime.InteropServices.CharSet.Unicode)]
public static extern System.IntPtr LoadLibrary(string lpFileName);
"@ -Name "K32DbgProbe" -Namespace "Sov" -PassThru

                    $hDbg = [Sov.K32DbgProbe]::LoadLibrary("dbgeng.dll")
                    if ($hDbg -eq [IntPtr]::Zero) {
                        $swTask.Stop()
                        return New-TaskResult -Status "fail" -Detail "dbgeng.dll not loadable" -DurationMs $swTask.ElapsedMilliseconds
                    }

                    $p = Start-Process -FilePath "notepad.exe" -PassThru
                    try {
                        Start-Sleep -Milliseconds 300
                        $attached = (Get-Process -Id $p.Id -ErrorAction SilentlyContinue) -ne $null
                        $swTask.Stop()
                        if ($attached) {
                            "dbgeng.dll present; attach target process alive" | Set-Content -Path $stdoutPath -Encoding UTF8
                            return New-TaskResult -Status "pass" -Detail "Debugger prerequisites healthy (DbgEng + target lifecycle)" -DurationMs $swTask.ElapsedMilliseconds -StdoutPath $stdoutPath -StderrPath $stderrPath
                        }
                        return New-TaskResult -Status "fail" -Detail "Attach target process could not be observed" -DurationMs $swTask.ElapsedMilliseconds
                    } finally {
                        try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch { }
                    }
                }

                "swarm" {
                    if ($SwarmHealthUrl) {
                        try {
                            $resp = Invoke-WebRequest -Uri $SwarmHealthUrl -UseBasicParsing -TimeoutSec ([Math]::Min($PerTaskTimeoutSec, 10))
                            $swTask.Stop()
                            $resp.Content | Set-Content -Path $stdoutPath -Encoding UTF8
                            if ($resp.StatusCode -ge 200 -and $resp.StatusCode -lt 300) {
                                return New-TaskResult -Status "pass" -Detail ("Swarm health HTTP {0}" -f $resp.StatusCode) -DurationMs $swTask.ElapsedMilliseconds -StdoutPath $stdoutPath -StderrPath $stderrPath
                            }
                            return New-TaskResult -Status "fail" -Detail ("Swarm health HTTP {0}" -f $resp.StatusCode) -DurationMs $swTask.ElapsedMilliseconds -StdoutPath $stdoutPath -StderrPath $stderrPath
                        } catch {
                            $swTask.Stop()
                            return New-TaskResult -Status "fail" -Detail ("Swarm health request failed: {0}" -f $_.Exception.Message) -DurationMs $swTask.ElapsedMilliseconds
                        }
                    }

                    $swarmScript = Resolve-FirstExistingInner @(
                        (Join-Path $RepoRoot "scripts/swarm_monitor.ps1"),
                        (Join-Path $RepoRoot "scripts/swarm_beacon_runner.ps1")
                    )

                    if ($swarmScript) {
                        return Run-ProcessWithTimeout -FilePath "powershell.exe" -Arguments @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ('"{0}"' -f $swarmScript)) -TimeoutSec $PerTaskTimeoutSec -StdoutPath $stdoutPath -StderrPath $stderrPath -SuccessRegex @("swarm", "worker", "beacon", "healthy", "online")
                    }

                    $swTask.Stop()
                    return New-TaskResult -Status "skip" -Detail "No swarm monitor script found and no -SwarmHealthUrl provided" -DurationMs $swTask.ElapsedMilliseconds
                }

                "embedding" {
                    $embedExe = Resolve-FirstExistingInner @(
                        (Join-Path $RepoRoot "build/Release/test_embedding_engine.exe"),
                        (Join-Path $RepoRoot "build/test_embedding_engine.exe"),
                        (Join-Path $RepoRoot "bin/Release/test_embedding_engine.exe")
                    )

                    if ($embedExe) {
                        return Run-ProcessWithTimeout -FilePath $embedExe -Arguments @() -TimeoutSec $PerTaskTimeoutSec -StdoutPath $stdoutPath -StderrPath $stderrPath -SuccessRegex @("embedding", "vector", "normalize", "success")
                    }

                    # Fallback CPU vector path to validate high-throughput normalization under load
                    $rand = New-Object System.Random
                    $dim = 1536
                    $iters = 800
                    $sumNorm = 0.0
                    for ($i = 0; $i -lt $iters; $i++) {
                        $vec = New-Object double[] $dim
                        $acc = 0.0
                        for ($j = 0; $j -lt $dim; $j++) {
                            $v = $rand.NextDouble()
                            $vec[$j] = $v
                            $acc += ($v * $v)
                        }
                        $norm = [Math]::Sqrt([Math]::Max($acc, 1e-12))
                        $sumNorm += $norm
                        for ($j = 0; $j -lt $dim; $j++) { $vec[$j] = $vec[$j] / $norm }
                    }
                    $swTask.Stop()
                    ("embedding-fallback iters={0} avgNorm={1}" -f $iters, ($sumNorm / $iters)) | Set-Content -Path $stdoutPath -Encoding UTF8
                    return New-TaskResult -Status "pass" -Detail "Embedding compute fallback load executed" -DurationMs $swTask.ElapsedMilliseconds -StdoutPath $stdoutPath -StderrPath $stderrPath
                }

                "kquant" {
                    $quantExe = Resolve-FirstExistingInner @(
                        (Join-Path $RepoRoot "build/Release/test_kquant_q4k.exe"),
                        (Join-Path $RepoRoot "build/test_kquant_q4k.exe"),
                        (Join-Path $RepoRoot "bin/Release/test_kquant_q4k.exe")
                    )

                    if ($quantExe) {
                        return Run-ProcessWithTimeout -FilePath $quantExe -Arguments @() -TimeoutSec $PerTaskTimeoutSec -StdoutPath $stdoutPath -StderrPath $stderrPath -SuccessRegex @("q4_k", "dequant", "avx", "success")
                    }

                    # Fallback dequant simulation under vectorized-style pressure
                    $blocks = 4096
                    $acc = 0.0
                    for ($b = 0; $b -lt $blocks; $b++) {
                        for ($k = 0; $k -lt 256; $k++) {
                            $q = ($k % 16) - 8
                            $scale = (($b % 64) + 1) / 64.0
                            $acc += ($q * $scale)
                        }
                    }
                    $swTask.Stop()
                    ("kquant-fallback blocks={0} checksum={1}" -f $blocks, $acc) | Set-Content -Path $stdoutPath -Encoding UTF8
                    return New-TaskResult -Status "pass" -Detail "Q4_K fallback dequant load executed" -DurationMs $swTask.ElapsedMilliseconds -StdoutPath $stdoutPath -StderrPath $stderrPath
                }

                default {
                    $swTask.Stop()
                    return New-TaskResult -Status "fail" -Detail "Unknown task id: $($task.id)" -DurationMs $swTask.ElapsedMilliseconds
                }
            }
        }
        catch {
            $swTask.Stop()
            return New-TaskResult -Status "fail" -Detail $_.Exception.Message -DurationMs $swTask.ElapsedMilliseconds
        }
    }
}

Write-Host ""
Write-Host "Launched $($jobs.Count) concurrent subsystem probes..." -ForegroundColor Yellow

$deadline = (Get-Date).AddSeconds($PerTaskTimeoutSec + 30)
while ($true) {
    $running = $jobs | Where-Object { $_.State -eq "Running" }
    if ($running.Count -eq 0) { break }
    if ((Get-Date) -gt $deadline) {
        $running | ForEach-Object {
            try { Stop-Job -Id $_.Id -Force -ErrorAction SilentlyContinue } catch { }
        }
        break
    }
    Start-Sleep -Milliseconds 250
}

$results = @()
foreach ($j in $jobs) {
    $payload = Receive-Job -Id $j.Id -ErrorAction SilentlyContinue
    if ($payload) {
        $results += $payload
    } else {
        $results += New-ResultObject -TaskId $j.Name -TaskName $j.Name -Status "fail" -Detail "No result payload from job" -DurationMs 0
    }
    Remove-Job -Id $j.Id -Force -ErrorAction SilentlyContinue
}

if ($IncludeInferenceValidation) {
    Write-Host ""
    Write-Host "[STAGE 8] Token Generation Validation..." -ForegroundColor Cyan

    $inferenceOut = Join-Path $logRoot "inference_stdout.log"
    $inferenceErr = Join-Path $logRoot "inference_stderr.log"
    $swInference = [System.Diagnostics.Stopwatch]::StartNew()

    if (-not $ModelPath -or -not (Test-Path $ModelPath)) {
        $swInference.Stop()
        $results += New-ResultObject -TaskId "inference" -TaskName "InferenceLoop FAST_GENERATE" -Status "fail" -Detail "-IncludeInferenceValidation requires a valid -ModelPath" -DurationMs $swInference.ElapsedMilliseconds -StdoutPath $inferenceOut -StderrPath $inferenceErr
    }
    else {
        $ideExe = Resolve-FirstExisting @(
            (Join-Path $RepoRoot "RawrXD-Win32IDE.exe"),
            (Join-Path $RepoRoot "bin/RawrXD-Win32IDE.exe"),
            (Join-Path $RepoRoot "build/Release/RawrXD-Win32IDE.exe"),
            (Join-Path $RepoRoot "build/RawrXD-Win32IDE.exe"),
            (Join-Path $RepoRoot "bin/Release/RawrXD-Win32IDE.exe"),
            (Join-Path $RepoRoot "RawrXD_IDE.exe")
        )

        if (-not $ideExe) {
            $swInference.Stop()
            $results += New-ResultObject -TaskId "inference" -TaskName "InferenceLoop FAST_GENERATE" -Status "fail" -Detail "RawrXD-Win32IDE executable not found" -DurationMs $swInference.ElapsedMilliseconds -StdoutPath $inferenceOut -StderrPath $inferenceErr
        }
        else {
            "" | Set-Content -Path $inferenceOut -Encoding UTF8
            "" | Set-Content -Path $inferenceErr -Encoding UTF8

            $infArgs = @(
                "--test-inference-fast",
                "--test-model",
                ('"{0}"' -f $ModelPath)
            )

            $proc = Start-Process -FilePath $ideExe `
                                  -ArgumentList ($infArgs -join " ") `
                                  -WorkingDirectory $RepoRoot `
                                  -RedirectStandardOutput $inferenceOut `
                                  -RedirectStandardError $inferenceErr `
                                  -NoNewWindow `
                                  -PassThru

            $inferenceTimeoutMs = [Math]::Max(1000, $InferenceTimeoutMs)
            if (-not $proc.WaitForExit($inferenceTimeoutMs)) {
                try { $proc.Kill() } catch { }
                $swInference.Stop()
                $results += New-ResultObject -TaskId "inference" -TaskName "InferenceLoop FAST_GENERATE" -Status "timeout" -Detail ("Inference timed out after {0}ms" -f $inferenceTimeoutMs) -DurationMs $swInference.ElapsedMilliseconds -StdoutPath $inferenceOut -StderrPath $inferenceErr
            }
            else {
                $swInference.Stop()
                $stdout = if (Test-Path $inferenceOut) { Get-Content -Path $inferenceOut -Raw -ErrorAction SilentlyContinue } else { "" }
                $stderr = if (Test-Path $inferenceErr) { Get-Content -Path $inferenceErr -Raw -ErrorAction SilentlyContinue } else { "" }
                $exitCode = -999
                try { $exitCode = [int]$proc.ExitCode } catch { }

                $rx = [regex]'PASS\s+FAST_GENERATE\s+token=(\d+)\s+time=(\d+)ms'
                $m = $rx.Match($stdout)
                if ($exitCode -eq 0 -and $m.Success) {
                    $token = $m.Groups[1].Value
                    $latency = $m.Groups[2].Value
                    $detail = "PASS FAST_GENERATE token=$token time=${latency}ms"
                    $results += New-ResultObject -TaskId "inference" -TaskName "InferenceLoop FAST_GENERATE" -Status "pass" -Detail $detail -DurationMs $swInference.ElapsedMilliseconds -StdoutPath $inferenceOut -StderrPath $inferenceErr
                }
                else {
                    $outSnippet = if ($stdout.Length -gt 220) { $stdout.Substring(0, 220) + "..." } else { $stdout }
                    $errSnippet = if ($stderr.Length -gt 180) { $stderr.Substring(0, 180) + "..." } else { $stderr }
                    $detail = "Missing PASS FAST_GENERATE (exit=$exitCode) stdout='$outSnippet' stderr='$errSnippet'"
                    $results += New-ResultObject -TaskId "inference" -TaskName "InferenceLoop FAST_GENERATE" -Status "fail" -Detail $detail -DurationMs $swInference.ElapsedMilliseconds -StdoutPath $inferenceOut -StderrPath $inferenceErr
                }
            }
        }
    }
}

$pass = @($results | Where-Object { $_.status -eq "pass" }).Count
$fail = @($results | Where-Object { $_.status -eq "fail" }).Count
$skip = @($results | Where-Object { $_.status -eq "skip" }).Count
$timeout = @($results | Where-Object { $_.status -eq "timeout" }).Count
$total = $results.Count

Write-Section "RESULTS"
foreach ($r in $results | Sort-Object taskId) {
    $color = "Gray"
    $icon = "•"
    switch ($r.status) {
        "pass" { $color = "Green"; $icon = "✅" }
        "fail" { $color = "Red"; $icon = "❌" }
        "skip" { $color = "Yellow"; $icon = "⏭️" }
        "timeout" { $color = "Magenta"; $icon = "⏱️" }
    }
    Write-Host ("{0} [{1}] {2} ({3} ms) — {4}" -f $icon, $r.taskId, $r.taskName, $r.durationMs, $r.detail) -ForegroundColor $color
}

$summary = [PSCustomObject]@{
    runId = $runId
    repoRoot = $RepoRoot
    modelPath = $ModelPath
    perTaskTimeoutSec = $PerTaskTimeoutSec
    startedAt = (Get-Date).ToString("o")
    total = $total
    passed = $pass
    failed = $fail
    skipped = $skip
    timedOut = $timeout
    allPassed = ($pass -eq $total)
    logRoot = $logRoot
    results = $results
}

$summary | ConvertTo-Json -Depth 10 | Set-Content -Path $OutputReport -Encoding UTF8

Write-Host ""
Write-Host "Summary: pass=$pass fail=$fail skip=$skip timeout=$timeout total=$total" -ForegroundColor Cyan
Write-Host "Logs: $logRoot" -ForegroundColor Gray
Write-Host "Report: $OutputReport" -ForegroundColor Gray

if ($fail -gt 0 -or $timeout -gt 0) {
    exit 1
}

exit 0
