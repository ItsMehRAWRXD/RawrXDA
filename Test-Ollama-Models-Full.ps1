# ============================================================================
# Test-Ollama-Models-Full.ps1 — Full Ollama model list, smoke test & integration
# ============================================================================
# Requirements (run before full session audit):
#   - PowerShell 5.1+ (pwsh or Windows PowerShell)
#   - Ollama installed and running (default: http://localhost:11434)
#   - At least one model pulled (e.g. ollama pull llama3.2)
#
# Usage:
#   .\Test-Ollama-Models-Full.ps1                    # Full: list + smoke one model
#   .\Test-Ollama-Models-Full.ps1 -SmokeEachModel     # Smoke test every model
#   .\Test-Ollama-Models-Full.ps1 -OllamaHost "http://127.0.0.1:11434"
#   .\Test-Ollama-Models-Full.ps1 -SkipGenerate       # Only list models, no generate
# ============================================================================

param(
    [string]$OllamaHost = $env:OLLAMA_HOST,
    [string]$OutputDir = $PSScriptRoot,
    [switch]$SmokeEachModel,
    [switch]$SkipGenerate,
    [int]$GenerateTimeoutSec = 60
)

if (-not $OllamaHost) { $OllamaHost = "http://localhost:11434" }
$OllamaHost = $OllamaHost.TrimEnd('/')

$script:Failed = 0
$script:Passed = 0

function Write-Result { param([string]$Name, [bool]$Ok, [string]$Detail = "")
    $script:Passed += [int]$Ok
    if (-not $Ok) { $script:Failed += 1 }
    $tag = if ($Ok) { "PASS" } else { "FAIL" }
    $color = if ($Ok) { "Green" } else { "Red" }
    Write-Host "  [$tag] $Name" -ForegroundColor $color
    if ($Detail) { Write-Host "         $Detail" -ForegroundColor Gray }
}

function Get-OllamaListCli {
    try {
        $out = & ollama list 2>&1
        if ($LASTEXITCODE -eq 0 -and $out) { return $out }
    } catch {}
    return $null
}

function Get-OllamaTagsFromApi {
    try {
        $r = Invoke-RestMethod -Uri "$OllamaHost/api/tags" -Method GET -TimeoutSec 10 -ErrorAction Stop
        return $r
    } catch {
        Write-Host "  [ERROR] GET $OllamaHost/api/tags : $($_.Exception.Message)" -ForegroundColor Red
        return $null
    }
}

function Invoke-OllamaGenerate {
    param([string]$Model, [string]$Prompt = "Reply with exactly: OK", [int]$TimeoutSec = 60)
    try {
        $body = @{ model = $Model; prompt = $Prompt; stream = $false } | ConvertTo-Json
        $r = Invoke-RestMethod -Uri "$OllamaHost/api/generate" -Method POST -Body $body -ContentType "application/json" -TimeoutSec $TimeoutSec -ErrorAction Stop
        return @{ ok = $true; response = $r.response; error = $r.error }
    } catch {
        return @{ ok = $false; response = $null; error = $_.Exception.Message }
    }
}

# ----------------------------------------------------------------------------
# 1) Connectivity
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== 1) Ollama connectivity ===" -ForegroundColor Cyan
$tagsPayload = Get-OllamaTagsFromApi
if (-not $tagsPayload) {
    Write-Result "Ollama reachable at $OllamaHost" $false "GET /api/tags failed. Start Ollama (e.g. ollama serve)."
    Write-Host "`nCannot continue without Ollama. Exiting with code 1." -ForegroundColor Red
    exit 1
}
Write-Result "Ollama reachable at $OllamaHost" $true

# ----------------------------------------------------------------------------
# 2) Full model list from API (original names and details)
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== 2) Full available model list (original names) ===" -ForegroundColor Cyan
$models = @($tagsPayload.models)
if ($models.Count -eq 0) {
    Write-Result "At least one model available" $false "No models in /api/tags. Run: ollama pull llama3.2"
    exit 1
}
Write-Result "At least one model available" $true "($($models.Count) models)"

# Build list with original names and full API fields
$fullList = foreach ($m in $models) {
    [PSCustomObject]@{
        name        = $m.name
        digest      = $m.digest
        size        = $m.size
        modified_at = $m.modified_at
        details     = $m.details
    }
}

# Optional: capture ollama list CLI output for "original names as displayed"
$cliList = Get-OllamaListCli
if ($cliList) {
    Write-Result "ollama list CLI available" $true
} else {
    Write-Result "ollama list CLI available" $false "Optional: ensure 'ollama' is on PATH"
}

# ----------------------------------------------------------------------------
# 3) Write OllamaAvailableModels.json and .md
# ----------------------------------------------------------------------------
$jsonPath = Join-Path $OutputDir "OllamaAvailableModels.json"
$mdPath   = Join-Path $OutputDir "OllamaAvailableModels.md"

$export = @{
    generated = (Get-Date).ToString('o')
    ollama_host = $OllamaHost
    count = $fullList.Count
    models = $fullList
    ollama_list_cli = if ($cliList) { $cliList } else { $null }
}
$export | ConvertTo-Json -Depth 6 | Set-Content -Path $jsonPath -Encoding UTF8
Write-Result "Write OllamaAvailableModels.json" $true $jsonPath

$mdLines = @(
    "# Ollama available models",
    "",
    "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
    "Host: $OllamaHost",
    "Total: $($fullList.Count) models",
    "",
    "## Original names (from API)",
    "",
    "| Name | Size | Modified | Family / Details |",
    "|------|------|----------|------------------|"
)
foreach ($m in $fullList) {
    $fam = ""
    if ($m.details) {
        if ($m.details.family) { $fam = $m.details.family }
        if ($m.details.parameter_size) { $fam += " / $($m.details.parameter_size)" }
    }
    $mdLines += "| $($m.name) | $($m.size) | $($m.modified_at) | $fam |"
}
$mdLines += ""
$mdLines += "## Names only (for copy-paste)"
$mdLines += ""
foreach ($m in $fullList) { $mdLines += "- ``$($m.name)``" }
if ($cliList) {
    $mdLines += ""
    $mdLines += "## ollama list (CLI output)"
    $mdLines += ""
    $mdLines += "``````"
    $mdLines += $cliList
    $mdLines += "``````"
}
$mdLines | Set-Content -Path $mdPath -Encoding UTF8
Write-Result "Write OllamaAvailableModels.md" $true $mdPath

# ----------------------------------------------------------------------------
# 4) Smoke test — generate with first model
# ----------------------------------------------------------------------------
if (-not $SkipGenerate) {
    Write-Host ""
    Write-Host "=== 4) Smoke test — load and use model ===" -ForegroundColor Cyan
    $firstModel = $fullList[0].name
    Write-Host "  Using model: $firstModel" -ForegroundColor Gray
    $gen = Invoke-OllamaGenerate -Model $firstModel -Prompt "Reply with exactly: OLLAMA_SMOKE_OK" -TimeoutSec $GenerateTimeoutSec
    if ($gen.ok -and $gen.response -match "OLLAMA_SMOKE_OK") {
        Write-Result "Generate smoke test ($firstModel)" $true
    } elseif ($gen.ok) {
        Write-Result "Generate smoke test ($firstModel)" $true "Response received (content not exact match)"
    } else {
        Write-Result "Generate smoke test ($firstModel)" $false $gen.error
    }

    if ($SmokeEachModel -and $fullList.Count -gt 1) {
        Write-Host ""
        Write-Host "=== 5) Smoke each model ===" -ForegroundColor Cyan
        foreach ($m in $fullList) {
            $n = $m.name
            $g = Invoke-OllamaGenerate -Model $n -Prompt "Say OK" -TimeoutSec $GenerateTimeoutSec
            Write-Result "Generate with $n" $g.ok $(if (-not $g.ok) { $g.error })
        }
    }
}

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Cyan
Write-Host "  Passed: $script:Passed" -ForegroundColor Green
if ($script:Failed -gt 0) {
    Write-Host "  Failed: $script:Failed" -ForegroundColor Red
    exit 1
}
Write-Host "  Model list: $jsonPath" -ForegroundColor Gray
Write-Host "  Readable:   $mdPath" -ForegroundColor Gray
Write-Host ""
Write-Host "Ollama model list and smoke test complete. Integration ready." -ForegroundColor Green
exit 0
