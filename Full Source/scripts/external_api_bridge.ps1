#!/usr/bin/env pwsh
param([Parameter(Mandatory)]$Provider,[Parameter(Mandatory)]$Prompt,[switch]$Streaming,[string]$Model="",[string]$ApiKey=$env:OPENAI_API_KEY)
$env:OPENAI_API_KEY = if ($Provider -eq 'openai' -and $ApiKey) { $ApiKey } else { $env:OPENAI_API_KEY }
if ($Provider -match 'anthropic|claude') { $env:ANTHROPIC_API_KEY = if ($ApiKey) { $ApiKey } else { $env:ANTHROPIC_API_KEY } }
& "$PSScriptRoot\RawrXD_Drive.ps1" -Action api -Provider $Provider -Prompt $Prompt
