# Test script to capture raw agentic model outputs
param(
    [string]$Model = "bigdaddyg-personalized-agentic:latest",
    [int]$MaxIter = 10
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# Load the agentic framework
. .\Agentic.ps1 -Prompt "test" -Model $Model -MaxIter 1 -ErrorAction SilentlyContinue

# Override to capture raw outputs
$rawOutputs = @()
$Ollama = 'http://localhost:11434'

# Get the system prompt and tools from Agentic.ps1
$SYS = @"
You are Agent-1B, an agentic assistant.
Tools: shell, powershell, read_file, write_file, list_dir, web_search, download_file, unzip, hash_file, reg_get, env_get, env_set, clipboard, speak, read_json, read_yaml, csv_to_json
Call: TOOL:name:{"arg":"value"}
Reply: ANSWER: <final answer>
"@

$msgs = @(
    @{ role = 'system'; content = $SYS },
    @{ role = 'user';   content = "Count how many .ps1 files are in the current directory. Use the shell tool to run a command that counts them, then provide the final answer." }
)

Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RAW MODEL OUTPUT TEST - $Model" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

1..$MaxIter | ForEach-Object {
    Write-Host "[Iteration $_]" -ForegroundColor Cyan
    Write-Host "─────────────────────────────────────────────────────" -ForegroundColor DarkGray

    try {
        # Use /api/chat for standard Ollama models
        $body = @{
            model = $Model
            messages = $msgs
            stream = $false
        } | ConvertTo-Json -Depth 10

        $uri = "$($Ollama.ToString().TrimEnd('/'))/api/chat"
        $resp = Invoke-RestMethod -Uri $uri -Method Post -ContentType "application/json" -Body $body -TimeoutSec 60
        $txt = $resp.message.content

        # Capture raw output
        $rawOutputs += [PSCustomObject]@{
            Iteration = $_
            RawOutput = $txt
            HasToolFormat = $txt -match '^TOOL:'
            HasAnswerFormat = $txt -match '^ANSWER:'
            ToolCall = if ($txt -match 'TOOL:([^:]+):?(.*)') { $matches[0] } else { $null }
            IsValidJSON = $false
        }

        # Check if tool call has valid JSON
        if ($txt -match 'TOOL:([^:]+):(.+)') {
            try {
                $json = $matches[2] | ConvertFrom-Json
                $rawOutputs[-1].IsValidJSON = $true
            } catch {
                $rawOutputs[-1].IsValidJSON = $false
            }
        }

        Write-Host "RAW OUTPUT:" -ForegroundColor Yellow
        Write-Host $txt -ForegroundColor White
        Write-Host ""

        # Check for answer
        if ($txt -match '^ANSWER:\s*(.+)') {
            Write-Host "✅ FINAL ANSWER: $($Matches[1])" -ForegroundColor Green
            break
        }

        # Check for tool call
        if ($txt -match 'TOOL:') {
            Write-Host "🔧 Tool call detected" -ForegroundColor Magenta
            # Add to conversation
            $msgs += @{ role = 'assistant'; content = $txt }
            $msgs += @{ role = 'user'; content = 'Observation: Tool executed (simulated)' }
        }
        else {
            $msgs += @{ role = 'assistant'; content = $txt }
            $msgs += @{ role = 'user'; content = 'Use TOOL:name:json or ANSWER: text' }
        }
    }
    catch {
        Write-Host "Error: $_" -ForegroundColor Red
        $rawOutputs += [PSCustomObject]@{
            Iteration = $_
            RawOutput = "ERROR: $_"
            HasToolFormat = $false
            HasAnswerFormat = $false
            ToolCall = $null
            IsValidJSON = $false
        }
    }
}

Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$summary = @{
    TotalIterations = $rawOutputs.Count
    ToolFormatCount = ($rawOutputs | Where-Object { $_.HasToolFormat }).Count
    AnswerFormatCount = ($rawOutputs | Where-Object { $_.HasAnswerFormat }).Count
    ValidJSONCount = ($rawOutputs | Where-Object { $_.IsValidJSON }).Count
    FormatDrift = $false
}

Write-Host "Total Iterations: $($summary.TotalIterations)" -ForegroundColor White
Write-Host "Tool Format Used: $($summary.ToolFormatCount)" -ForegroundColor $(if($summary.ToolFormatCount -gt 0){'Green'}else{'Red'})
Write-Host "Answer Format Used: $($summary.AnswerFormatCount)" -ForegroundColor $(if($summary.AnswerFormatCount -gt 0){'Green'}else{'Yellow'})
Write-Host "Valid JSON in Tool Calls: $($summary.ValidJSONCount)" -ForegroundColor $(if($summary.ValidJSONCount -gt 0){'Green'}else{'Red'})

Write-Host "`nDetailed Tool Calls:" -ForegroundColor Cyan
$rawOutputs | Where-Object { $_.ToolCall } | ForEach-Object {
    Write-Host "  Iter $($_.Iteration): $($_.ToolCall)" -ForegroundColor Gray
    Write-Host "    Valid JSON: $($_.IsValidJSON)" -ForegroundColor $(if($_.IsValidJSON){'Green'}else{'Red'})
}

# Export raw outputs
$rawOutputs | Export-Csv -Path "agentic-test-raw-outputs.csv" -NoTypeInformation
Write-Host "`n✅ Raw outputs saved to: agentic-test-raw-outputs.csv" -ForegroundColor Green

