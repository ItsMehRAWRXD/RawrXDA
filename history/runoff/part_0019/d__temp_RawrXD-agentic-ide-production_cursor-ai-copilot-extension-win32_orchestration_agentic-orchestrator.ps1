#!/usr/bin/env pwsh

<#
.SYNOPSIS
    RawrXD Agentic IDE - PowerShell Orchestration CLI
    Hybrid Node.js + PowerShell launcher for Win32 agentic workflows

.DESCRIPTION
    Command-line orchestrator for Cursor AI Copilot agentic workflows.
    Integrates with gpt-5.2-pro for reasoning, planning, and code generation.

.EXAMPLE
    .\agentic-orchestrator.ps1 -Objective "Build a REST API" -Language "typescript" -Mode "workflow"
#>

param(
    [Parameter(ValueFromPipeline, ValueFromPipelineByPropertyName)]
    [string]$Objective,

    [ValidateSet("typescript", "python", "cpp", "csharp", "javascript")]
    [string]$Language = "typescript",

    [ValidateSet("workflow", "plan", "code", "verify", "batch")]
    [string]$Mode = "workflow",

    [string]$ConfigPath = "$PSScriptRoot\..\config\settings.json",
    [string]$LogPath = "$env:APPDATA\RawrXD\Cursor-AI-Copilot\logs"
)

# Setup environment
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# Ensure log directory exists
if (-not (Test-Path $LogPath)) {
    New-Item -ItemType Directory -Path $LogPath -Force | Out-Null
}

# Load configuration
$config = @{}
if (Test-Path $ConfigPath) {
    $config = Get-Content $ConfigPath -Raw | ConvertFrom-Json
}

# Set environment variables from config
$env:OPENAI_API_KEY = $config."cursor-ai-copilot".apiKey
$env:OPENAI_API_BASE = $config."cursor-ai-copilot".apiEndpoint
$env:OPENAI_MODEL = $config."cursor-ai-copilot".model

Write-Host "🚀 RawrXD Agentic Orchestrator" -ForegroundColor Cyan
Write-Host "Mode: $Mode | Language: $Language | Model: $env:OPENAI_MODEL" -ForegroundColor Gray

# Load Node.js modules
$nodeModules = @{
    OpenAIClient = "$PSScriptRoot\..\modules\openai-client.js"
    AgentOrchestrator = "$PSScriptRoot\agent.js"
}

# Verify Node.js and modules
$nodeAvailable = $null -ne (Get-Command node -ErrorAction SilentlyContinue)
if (-not $nodeAvailable) {
    Write-Error "Node.js not found in PATH. Please install Node.js 16+"
}

# Create temporary Node.js script file
$tempScript = [System.IO.Path]::GetTempFileName() + ".js"
$nodeScript = @"
const OpenAIClient = require('$($nodeModules.OpenAIClient.Replace('\', '/'))');
const AgentOrchestrator = require('$($nodeModules.AgentOrchestrator.Replace('\', '/'))');

const config = {
  openai: {
    apiKey: process.env.OPENAI_API_KEY,
    apiBase: process.env.OPENAI_API_BASE,
    model: process.env.OPENAI_MODEL,
    maxRetries: 5,
    baseRetryDelay: 2000
  },
  maxIterations: 5,
  agentTimeout: 60000,
  enableFallback: true,
  logPath: '$($LogPath.Replace('\', '/'))'
};

const agent = new AgentOrchestrator(config);

// Forward events to console
agent.on('info', msg => console.log('[INFO] ' + msg));
agent.on('debug', msg => console.log('[DEBUG] ' + msg));
agent.on('warn', msg => console.warn('[WARN] ' + msg));
agent.on('error', msg => console.error('[ERROR] ' + msg));

async function main() {
  try {
    const context = {
      language: '$Language',
      contextBlocks: []
    };

    const result = await agent.executeAgenticWorkflow('$Objective', context);
    console.log(JSON.stringify(result, null, 2));
    process.exit(result.status === 'success' ? 0 : 1);
  } catch (error) {
    console.error('Workflow failed:', error.message);
    
    // If rate limit, suggest waiting
    if (error.message.includes('429')) {
      console.log('Rate limit hit. Wait 60 seconds and try again.');
      console.log('Or use local Ollama: .\\\\orchestration\\\\ollama-orchestrator.ps1');
    }
    
    process.exit(1);
  }
}

main();
"@

Set-Content -Path $tempScript -Value $nodeScript -Encoding UTF8

# Execute
node $tempScript
$exitCode = $LASTEXITCODE

# Cleanup
Remove-Item $tempScript -ErrorAction SilentlyContinue

if ($exitCode -ne 0) {
    Write-Error "Workflow execution failed"
    exit 1
}

Write-Host "✅ Workflow completed successfully" -ForegroundColor Green
