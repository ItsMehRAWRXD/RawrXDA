#!/usr/bin/env pwsh

<#
.SYNOPSIS
    Ollama-based Agentic Orchestrator - NO RATE LIMITS
    Uses local cheetah-stealth-agentic and bigdaddyg-fast models

.EXAMPLE
    .\ollama-orchestrator.ps1 -Objective "Build a REST API" -Language "typescript"
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$Objective,

    [ValidateSet("typescript", "python", "cpp", "csharp", "javascript")]
    [string]$Language = "typescript",

    [string]$OllamaEndpoint = "http://localhost:11434",
    [string]$AgenticModel = "cheetah-stealth-agentic:latest",
    [string]$StandardModel = "bigdaddyg-fast:latest"
)

$ErrorActionPreference = "Stop"

Write-Host "🚀 RawrXD Agentic Orchestrator (Ollama - No Rate Limits)" -ForegroundColor Cyan
Write-Host "Model: $AgenticModel | Endpoint: $OllamaEndpoint" -ForegroundColor Gray

# Set environment variables
$env:OLLAMA_ENDPOINT = $OllamaEndpoint
$env:OLLAMA_AGENTIC_MODEL = $AgenticModel
$env:OLLAMA_STANDARD_MODEL = $StandardModel

# Create temporary script
$tempScript = [System.IO.Path]::GetTempFileName() + ".js"
$nodeScript = @"
const OllamaClient = require('$($PSScriptRoot.Replace('\', '/'))/../modules/ollama-client.js');
const AgentOrchestrator = require('$($PSScriptRoot.Replace('\', '/'))/agent.js');

// Create Ollama client
const ollamaClient = new OllamaClient({
  endpoint: process.env.OLLAMA_ENDPOINT,
  agenticModel: process.env.OLLAMA_AGENTIC_MODEL,
  standardModel: process.env.OLLAMA_STANDARD_MODEL
});

// Create config that bypasses OpenAI constructor
const config = {
  openai: ollamaClient,
  maxIterations: 10,
  agentTimeout: 120000,
  enableFallback: true
};

// Create agent with Ollama client (skip OpenAI init)
const agent = new AgentOrchestrator.__proto__.constructor.call({}, config);
agent.openai = ollamaClient;
agent.maxIterations = config.maxIterations;
agent.agentTimeout = config.agentTimeout;
agent.enableFallback = config.enableFallback;
agent.logPath = process.env.APPDATA + '/RawrXD/logs';
Object.setPrototypeOf(agent, AgentOrchestrator.prototype);

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

Write-Host "✅ Workflow completed successfully (Local Ollama)" -ForegroundColor Green
