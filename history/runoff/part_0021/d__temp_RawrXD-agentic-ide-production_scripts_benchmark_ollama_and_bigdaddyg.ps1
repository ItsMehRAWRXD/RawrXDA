param(
    [string]$OutputDir = "benchmarks",
    [int]$Tokens = 128,
    [int]$WarmupTokens = 32,
    [string]$Prompt = "Benchmark throughput run.",
    [string]$LocalModelDir = "D:\\OllamaModels",
    [switch]$SkipCreateFromGGUF
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Ensure output directory exists
$fullOut = Resolve-Path -LiteralPath $OutputDir -ErrorAction SilentlyContinue
if (-not $fullOut) { $fullOut = New-Item -ItemType Directory -Force -Path $OutputDir }

function Get-OllamaModels {
    $lines = ollama list
    return $lines | Select-Object -Skip 1 | Where-Object { $_ -and (-not $_.StartsWith('ID ')) } | ForEach-Object {
        $parts = ($_ -split '\s{2,}') | Where-Object { $_ -ne '' }
        if ($parts.Count -lt 1) { return }
        $name = $parts[0]
        $sizeGb = $null
        if ($parts.Count -ge 3) {
            $sizeStr = $parts[2]
            if ($sizeStr -match '([0-9\.]+)\s*GB') { $sizeGb = [double]$matches[1] }
        }
        [pscustomobject]@{
            Name   = $name
            ID     = if ($parts.Count -ge 2) { $parts[1] } else { $null }
            SizeGB = $sizeGb
            Source = 'ollama-installed'
        }
    }
}

function Get-LocalGgufModels {
    param([string]$Dir)
    if (-not (Test-Path -LiteralPath $Dir)) { return @() }
    Get-ChildItem -LiteralPath $Dir -Recurse -Filter '*.gguf' -File -ErrorAction SilentlyContinue |
        ForEach-Object {
            [pscustomobject]@{
                Name   = $_.BaseName
                Path   = $_.FullName
                SizeGB = [math]::Round($_.Length / 1GB, 2)
                Source = 'gguf-file'
            }
        }
    }

function Ensure-OllamaModelFromGguf {
    param(
        [pscustomobject]$GgufModel
    )
    $existing = Get-OllamaModels | Where-Object { $_.Name -eq $GgufModel.Name }
    if ($existing) { return $GgufModel.Name }

    # If the GGUF file exists on disk but no Ollama entry is present, respect it and skip creation.
    # This lets already-curated GGUF blobs run without forcing ollama create.
    if (Test-Path -LiteralPath $GgufModel.Path) {
        Write-Host "[skip-create] Using existing GGUF file on disk for '$($GgufModel.Name)' (no ollama create)" -ForegroundColor Yellow
        return $GgufModel.Name
    }
        if ($SkipCreateFromGGUF) { return $GgufModel.Name }

    $path = ($GgufModel.Path -replace '^\\+','' -replace '\\','/')
    $modelfile = @(
        "FROM $path",
        'TEMPLATE "{{ prompt }}"'
    ) -join "`n"

    $tmp = New-TemporaryFile
    Set-Content -LiteralPath $tmp -Value $modelfile -Encoding ASCII
    try {
        Write-Host "[create] $($GgufModel.Name) from $($GgufModel.Path)"
        ollama create $GgufModel.Name -f $tmp | Out-Null
        return $GgufModel.Name
    }
    catch {
        Write-Warning "Failed to create model $($GgufModel.Name): $_"
        return $null
    }
    finally {
        Remove-Item -LiteralPath $tmp -ErrorAction SilentlyContinue
    }
}

function Invoke-Benchmark {
    param(
        [string]$ModelName,
        [string]$PromptText,
        [int]$PredictTokens,
        [int]$Warmup
    )
    $result = [pscustomobject]@{
        name          = $ModelName
        tokens        = $PredictTokens
        warmup_tokens = $Warmup
        duration_ms   = $null
        tps           = $null
        eval_duration_ms = $null
        eval_tps      = $null
        error         = $null
    }

    try {
        # Use HTTP API to control num_predict since this Ollama build lacks CLI token flags
        $uri = "http://127.0.0.1:11434/api/generate"

        if ($Warmup -gt 0) {
            $warmupBody = @{ model = $ModelName; prompt = $PromptText; num_predict = $Warmup; stream = $false }
            Invoke-RestMethod -Method Post -Uri $uri -Body ($warmupBody | ConvertTo-Json -Depth 5) -ContentType 'application/json' | Out-Null
        }

        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $body = @{ model = $ModelName; prompt = $PromptText; num_predict = $PredictTokens; stream = $false }
        $response = Invoke-RestMethod -Method Post -Uri $uri -Body ($body | ConvertTo-Json -Depth 5) -ContentType 'application/json'
        $sw.Stop()

        $result.duration_ms = [math]::Round($sw.Elapsed.TotalMilliseconds, 2)
        $result.tps = if ($result.duration_ms -gt 0) { [math]::Round($PredictTokens / ($result.duration_ms/1000), 2) } else { $null }

        if ($response -and $response.eval_count -and $response.eval_duration) {
            $result.eval_duration_ms = [double]$response.eval_duration
            $result.eval_tps = if ($result.eval_duration_ms -gt 0) { [math]::Round(($response.eval_count / ($result.eval_duration_ms/1000)), 2) } else { $null }
        }
    }
    catch {
        $result.error = $_.ToString()
    }
    return $result
}

$all = @()
$ollamaModels = Get-OllamaModels
$all += $ollamaModels

$ggufs = Get-LocalGgufModels -Dir $LocalModelDir
foreach ($g in $ggufs) {
    $name = Ensure-OllamaModelFromGguf -GgufModel $g
    if ($name) {
        $all += [pscustomobject]@{ Name=$name; SizeGB=$g.SizeGB; Source='gguf-file'; Path=$g.Path }
    }
}

$all = $all | Sort-Object -Property Name -Unique

$results = @()
foreach ($m in $all) {
    Write-Host "[bench] $($m.Name)" -ForegroundColor Cyan
    $r = Invoke-Benchmark -ModelName $m.Name -PromptText $Prompt -PredictTokens $Tokens -Warmup $WarmupTokens
    $r | Add-Member -NotePropertyName source -NotePropertyValue $m.Source -Force
    $r | Add-Member -NotePropertyName size_gb -NotePropertyValue $m.SizeGB -Force
    if ($m.PSObject.Properties['Path']) { $r | Add-Member -NotePropertyName gguf_path -NotePropertyValue $m.Path -Force }
    $results += $r
}

$csvPath = Join-Path $fullOut "ollama-benchmarks.csv"
$jsonPath = Join-Path $fullOut "ollama-benchmarks.json"
$results | Export-Csv -NoTypeInformation -Path $csvPath -Encoding UTF8
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

Write-Host "Results saved:" -ForegroundColor Green
Write-Host "  CSV:  $csvPath"
Write-Host "  JSON: $jsonPath"
