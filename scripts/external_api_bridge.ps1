#!/usr/bin/env pwsh
<#
.SYNOPSIS
    External API Bridge - OpenAI, Anthropic, Claude REST integration

.DESCRIPTION
    Enables frontier model access (OpenAI, Anthropic, Claude) via REST.
    Use when local GGUF is insufficient or for quick API calls from scripts/chatbot.
    Supports streaming for token-by-token output.

.PARAMETER Provider
    openai, anthropic, claude (anthropic + claude use Anthropic API)

.PARAMETER Prompt
    The prompt to send

.PARAMETER Streaming
    Enable streaming (SSE) response

.PARAMETER Model
    Model name (defaults per provider)

.EXAMPLE
    .\external_api_bridge.ps1 -Provider openai -Prompt "Explain std::expected in C++"
.EXAMPLE
    .\external_api_bridge.ps1 -Provider anthropic -Streaming -Prompt "Refactor this function"
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('openai', 'anthropic', 'claude')]
    [string]$Provider,
    
    [Parameter(Mandatory=$true)]
    [string]$Prompt,
    
    [Parameter(Mandatory=$false)]
    [switch]$Streaming,
    
    [Parameter(Mandatory=$false)]
    [string]$Model = "",
    
    [Parameter(Mandatory=$false)]
    [string]$ApiKey = $env:OPENAI_API_KEY
)

if ($Provider -eq 'anthropic' -or $Provider -eq 'claude') {
    if (-not $ApiKey -and $env:ANTHROPIC_API_KEY) { $ApiKey = $env:ANTHROPIC_API_KEY }
}

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-OpenAI {
    param([string]$prompt, [bool]$stream)
    $key = if ($ApiKey) { $ApiKey } else { $env:OPENAI_API_KEY }
    if (-not $key) { throw "OPENAI_API_KEY or -ApiKey required for OpenAI" }
    $model = if ($Model) { $Model } else { "gpt-4o-mini" }
    
    $body = @{
        model = $model
        messages = @(@{ role = "user"; content = $prompt })
        stream = $stream
    } | ConvertTo-Json -Depth 5
    
    $headers = @{
        "Authorization" = "Bearer $key"
        "Content-Type" = "application/json"
    }
    
    $url = "https://api.openai.com/v1/chat/completions"
    try {
        if ($stream) {
            $response = Invoke-WebRequest -Uri $url -Method Post -Body $body -Headers $headers -UseBasicParsing -TimeoutSec 120
            $lines = $response.Content -split "`n"
            foreach ($line in $lines) {
                if ($line -match '^data: \{"id".*"choices":\[.*"delta":\{"content":"([^"]*)"') {
                    Write-Host -NoNewline $Matches[1].Replace('\n', "`n").Replace('\r', '')
                }
            }
            Write-Host ""
        } else {
            $r = Invoke-RestMethod -Uri $url -Method Post -Body $body -Headers $headers
            $r.choices[0].message.content
        }
    } catch {
        Write-Error "OpenAI API error: $_"
    }
}

function Invoke-Anthropic {
    param([string]$prompt, [bool]$stream)
    $key = if ($ApiKey) { $ApiKey } else { $env:ANTHROPIC_API_KEY }
    if (-not $key) { throw "ANTHROPIC_API_KEY or -ApiKey required for Anthropic" }
    $model = if ($Model) { $Model } else { "claude-3-5-sonnet-20241022" }
    
    $body = @{
        model = $model
        max_tokens = 4096
        messages = @(@{ role = "user"; content = $prompt })
        stream = $stream
    } | ConvertTo-Json -Depth 5
    
    $headers = @{
        "x-api-key" = $key
        "anthropic-version" = "2023-06-01"
        "Content-Type" = "application/json"
    }
    
    $url = "https://api.anthropic.com/v1/messages"
    try {
        if ($stream) { $body = $body -replace '"stream":\s*true', '"stream":false' }
        $r = Invoke-RestMethod -Uri $url -Method Post -Body $body -Headers $headers
        $text = ($r.content | Where-Object { $_.type -eq "text" } | ForEach-Object { $_.text }) -join ""
        Write-Output $text
    } catch {
        Write-Error "Anthropic API error: $_"
    }
}

switch ($Provider) {
    'openai'  { Invoke-OpenAI -prompt $Prompt -stream $Streaming.IsPresent }
    'anthropic','claude' { Invoke-Anthropic -prompt $Prompt -stream $Streaming.IsPresent }
}
