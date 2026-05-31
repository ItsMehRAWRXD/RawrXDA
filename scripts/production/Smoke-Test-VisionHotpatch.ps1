param(
    [string]$ImagePath = "C:\Users\HiH8e\OneDrive\Desktop\Screenshot 2026-04-12 142141.png",
    [string]$BaseUrl = "http://localhost:11434",
    [string[]]$VisionModels = @("qwen3-vl:4b", "gemma3:1b", "gemma3:latest"),
    [string[]]$NonVisionModels = @("phi:latest", "phi3:mini"),
    [string]$ReportPath = "D:\RawrXD\reports\14day\vision_hotpatch_smoke.json"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Net.Http

function Invoke-OllamaChatJson {
    param([string]$Url, [string]$Body)

    $handler = New-Object System.Net.Http.HttpClientHandler
    $client = New-Object System.Net.Http.HttpClient($handler)
    $client.Timeout = [TimeSpan]::FromSeconds(600)
    try {
        $content = New-Object System.Net.Http.StringContent($Body, [System.Text.Encoding]::UTF8, "application/json")
        $response = $client.PostAsync($Url, $content).GetAwaiter().GetResult()
        $respBody = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        return [ordered]@{
            statusCode = [int]$response.StatusCode
            isSuccess = $response.IsSuccessStatusCode
            body = $respBody
        }
    } finally {
        $client.Dispose()
        $handler.Dispose()
    }
}

function Test-ModelPresent {
    param([string]$ModelName)
    $output = & ollama list
    return ($output | Select-String -SimpleMatch $ModelName -Quiet)
}

function Get-AnchorScore {
    param([string]$Text)

    if ([string]::IsNullOrWhiteSpace($Text)) { return 0 }
    $score = 0
    $patterns = @(
        'spotify',
        'visual studio code|vs code',
        'red|maroon|dark red',
        'multiple windows|several windows|floating windows'
    )
    foreach ($p in $patterns) {
        if ($Text -match $p) { $score++ }
    }
    return $score
}

if (-not (Test-Path $ImagePath)) {
    throw "Image not found: $ImagePath"
}

$reportDir = Split-Path -Parent $ReportPath
if (-not (Test-Path $reportDir)) {
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
}

$imageBytes = [System.IO.File]::ReadAllBytes($ImagePath)
$imageBase64 = [Convert]::ToBase64String($imageBytes)

$prompt = "From the screenshot only, answer in one line: green_taskbar_app=<name>; center_editor=<name>; window_theme=<color>. If you cannot inspect the image, set each value to unknown."
$chatUrl = "$BaseUrl/api/chat"

$result = [ordered]@{
    ok = $true
    timestamp = (Get-Date).ToUniversalTime().ToString("o")
    imagePath = $ImagePath
    rawrxdClientSupportsImageUpload = $false
    rawrxdClientReason = "src/ollama_client.h and src/ollama_client.cpp define no image field on OllamaChatMessage and createChatRequestJson serializes only role/content/tool fields."
    hotpatchVisionForNonVisionModelsObserved = $false
    visionHotpatchReason = "No dedicated vision hotpatch bridge was found; current hotpatch code in src/hotpatch.cpp and src/hot_patcher.cpp is generic byte-patching only."
    tests = @()
}

$visionModel = $null
foreach ($candidate in $VisionModels) {
    if (Test-ModelPresent $candidate) {
        $visionModel = $candidate
        break
    }
}

if ($visionModel) {
    $visionBody = @{
        model = $visionModel
        stream = $false
        options = @{
            num_predict = 48
            temperature = 0
        }
        messages = @(
            @{
                role = "user"
                content = $prompt
                images = @($imageBase64)
            }
        )
    } | ConvertTo-Json -Depth 8 -Compress

    $visionResp = Invoke-OllamaChatJson -Url $chatUrl -Body $visionBody
    $visionParsed = $null
    try { $visionParsed = $visionResp.body | ConvertFrom-Json -Depth 20 } catch {}
    $visionContent = if ($visionParsed -and $visionParsed.message) { [string]$visionParsed.message.content } else { $visionResp.body.Substring(0, [Math]::Min(400, $visionResp.body.Length)) }

    $result.tests += [ordered]@{
        model = $visionModel
        category = "vision-control"
        httpStatus = $visionResp.statusCode
        success = $visionResp.isSuccess
        responsePreview = $visionContent
        acceptedImagePayload = ($visionResp.statusCode -eq 200)
        anchorScore = (Get-AnchorScore -Text $visionContent)
    }
} else {
    $result.tests += [ordered]@{
        model = ($VisionModels -join ', ')
        category = "vision-control"
        skipped = $true
        reason = "No candidate vision model installed"
    }
}

foreach ($model in $NonVisionModels) {
    if (-not (Test-ModelPresent $model)) {
        $result.tests += [ordered]@{
            model = $model
            category = "non-vision"
            skipped = $true
            reason = "Model not installed"
        }
        continue
    }

    $body = @{
        model = $model
        stream = $false
        options = @{
            num_predict = 48
            temperature = 0
        }
        messages = @(
            @{
                role = "user"
                content = $prompt
                images = @($imageBase64)
            }
        )
    } | ConvertTo-Json -Depth 8 -Compress

    $resp = Invoke-OllamaChatJson -Url $chatUrl -Body $body
    $parsed = $null
    try { $parsed = $resp.body | ConvertFrom-Json -Depth 20 } catch {}

    $content = if ($parsed -and $parsed.message) { [string]$parsed.message.content } else { $resp.body.Substring(0, [Math]::Min(400, $resp.body.Length)) }

    $controlBody = @{
        model = $model
        stream = $false
        options = @{
            num_predict = 48
            temperature = 0
        }
        messages = @(
            @{
                role = "user"
                content = $prompt
            }
        )
    } | ConvertTo-Json -Depth 8 -Compress

    $controlResp = Invoke-OllamaChatJson -Url $chatUrl -Body $controlBody
    $controlParsed = $null
    try { $controlParsed = $controlResp.body | ConvertFrom-Json -Depth 20 } catch {}
    $controlContent = if ($controlParsed -and $controlParsed.message) { [string]$controlParsed.message.content } else { $controlResp.body.Substring(0, [Math]::Min(400, $controlResp.body.Length)) }

    $imageScore = Get-AnchorScore -Text $content
    $controlScore = Get-AnchorScore -Text $controlContent
    $looksVisionCapable = (($imageScore -ge 2) -and ($imageScore -gt $controlScore))

    $result.tests += [ordered]@{
        model = $model
        category = "non-vision"
        httpStatus = $resp.statusCode
        success = $resp.isSuccess
        responsePreview = $content
        controlPreview = $controlContent
        acceptedImagePayload = ($resp.statusCode -eq 200)
        imageAnchorScore = $imageScore
        controlAnchorScore = $controlScore
        appearsToActuallyUseVision = [bool]$looksVisionCapable
    }
}

$nonVisionPositive = @($result.tests | Where-Object {
    $_.category -eq 'non-vision' -and $_.success -eq $true -and $_.appearsToActuallyUseVision -eq $true
}).Count

$result.hotpatchVisionForNonVisionModelsObserved = ($nonVisionPositive -gt 0)
if (-not $result.hotpatchVisionForNonVisionModelsObserved) {
    $result.ok = $true
}

$result | ConvertTo-Json -Depth 10 | Set-Content -Path $ReportPath -Encoding UTF8
Write-Host "Wrote report: $ReportPath"
Get-Content -Path $ReportPath
