# Capture raw outputs from agentic model
param(
    [string]$Model = "bigdaddyg-personalized-agentic:latest",
    [int]$MaxIter = 10
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$Ollama = 'http://localhost:11434'
$SYS = "You are Agent-1B, an agentic assistant.`nTools: shell, powershell, read_file, write_file, list_dir, web_search`nCall: TOOL:name:{`"arg`":`"value`"}`nReply: ANSWER: <final answer>"

$msgs = @(
    @{ role = 'system'; content = $SYS },
    @{ role = 'user';   content = "Count how many .ps1 files are in the current directory. Use the shell tool to run a command that counts them, then provide the final answer." }
)

$rawOutputs = @()

Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RAW OUTPUT TEST - $Model" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

1..$MaxIter | ForEach-Object {
    $iter = $_
    Write-Host "[Iteration $iter]" -ForegroundColor Cyan
    Write-Host "─────────────────────────────────────────────────────" -ForegroundColor DarkGray

    try {
        $body = @{
            model = $Model
            messages = $msgs
            stream = $false
        } | ConvertTo-Json -Depth 10

        $uri = "$($Ollama.ToString().TrimEnd('/'))/api/chat"
        $resp = Invoke-RestMethod -Uri $uri -Method Post -ContentType "application/json" -Body $body -TimeoutSec 60
        $txt = $resp.message.content

        $rawOutputs += $txt

        Write-Host "RAW OUTPUT:" -ForegroundColor Yellow
        Write-Host $txt -ForegroundColor White
        Write-Host ""

        # Analyze format
        $hasTool = $txt -match 'TOOL:'
        $hasAnswer = $txt -match '^ANSWER:'
        $hasValidFormat = $txt -match '^TOOL:([^:]+):(.+)'
        $hasJSON = $false

        if ($hasValidFormat) {
            try {
                $jsonPart = $matches[2]
                $null = $jsonPart | ConvertFrom-Json
                $hasJSON = $true
            } catch { }
        }

        Write-Host "Analysis:" -ForegroundColor Cyan
        Write-Host "  Has TOOL: $hasTool" -ForegroundColor $(if($hasTool){'Green'}else{'Red'})
        Write-Host "  Has ANSWER: $hasAnswer" -ForegroundColor $(if($hasAnswer){'Green'}else{'Yellow'})
        Write-Host "  Valid format (TOOL:name:json): $hasValidFormat" -ForegroundColor $(if($hasValidFormat){'Green'}else{'Red'})
        Write-Host "  Valid JSON: $hasJSON" -ForegroundColor $(if($hasJSON){'Green'}else{'Red'})
        Write-Host ""

        if ($hasAnswer) {
            Write-Host "✅ FINAL ANSWER REACHED" -ForegroundColor Green
            break
        }

        # Continue conversation
        $msgs += @{ role = 'assistant'; content = $txt }
        if ($hasTool) {
            $msgs += @{ role = 'user'; content = 'Observation: Tool executed (simulated). Continue or provide ANSWER:' }
        } else {
            $msgs += @{ role = 'user'; content = 'Use TOOL:name:{"arg":"value"} or ANSWER: text' }
        }
    }
    catch {
        Write-Host "Error: $_" -ForegroundColor Red
        $rawOutputs += "ERROR: $_"
    }
}

Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  ALL RAW OUTPUTS (for analysis)" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$rawOutputs | ForEach-Object {
    Write-Host "─────────────────────────────────────────────────────" -ForegroundColor DarkGray
    Write-Host $_ -ForegroundColor White
    Write-Host ""
}

# Save to file
$rawOutputs | Out-File -FilePath "agentic-raw-outputs.txt" -Encoding UTF8
Write-Host "✅ Raw outputs saved to: agentic-raw-outputs.txt" -ForegroundColor Green

