#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Gap Remediation Workflow - Master orchestration for bad-news gaps

.DESCRIPTION
    Fully reverse-engineers the identified gaps into functional workflows:
    - Inline Edit: C++ exists; backend via Ollama/External API
    - Streaming: C++ + PowerShell bridge
    - External APIs: external_api_bridge.ps1
    - LSP: !lsp commands in CLI
    - Multi-Agent: multi_agent_parallel.ps1
    - Paths: fixed in validation/RE scripts

.PARAMETER Action
    validate, launch-assistant, external-api, multi-agent, fix-paths, docs

.EXAMPLE
    .\GAP_REMEDIATION_WORKFLOW.ps1 -Action validate
.EXAMPLE
    .\GAP_REMEDIATION_WORKFLOW.ps1 -Action launch-assistant -Mode enhanced
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('validate', 'launch-assistant', 'external-api', 'multi-agent', 'fix-paths', 'docs', 'all')]
    [string]$Action = "docs",
    
    [Parameter(Mandatory=$false)]
    [string]$Mode = "enhanced",
    
    [Parameter(Mandatory=$false)]
    [string]$Prompt = "",
    
    [Parameter(Mandatory=$false)]
    [string[]]$Tasks = @("Analyze src/", "Analyze scripts/")
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = if ($env:LAZY_INIT_IDE_ROOT) { $env:LAZY_INIT_IDE_ROOT } else { (Resolve-Path (Join-Path $PSScriptRoot "..") -ErrorAction SilentlyContinue).Path }
if (-not $projectRoot) { $projectRoot = (Get-Location).Path }

function Show-Docs {
    Write-Host "`n╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║         Gap Remediation - Functional Workflows                  ║" -ForegroundColor Magenta
    Write-Host "╚═══════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta
    Write-Host "QUICK REFERENCE:" -ForegroundColor Cyan
    Write-Host "  Launch assistant (KB + chatbot):  .\UNIFIED_ASSISTANT_LAUNCHER.ps1 -Mode enhanced" -ForegroundColor Gray
    Write-Host "  Voice assistant:                  .\UNIFIED_ASSISTANT_LAUNCHER.ps1 -Mode voice -EnableVoice" -ForegroundColor Gray
    Write-Host "  External API (OpenAI/Claude):     .\external_api_bridge.ps1 -Provider openai -Prompt '...'" -ForegroundColor Gray
    Write-Host "  Multi-agent parallel:             .\multi_agent_parallel.ps1 -Tasks @('task1','task2')" -ForegroundColor Gray
    Write-Host "  Validate system:                  .\..\VALIDATE_REVERSE_ENGINEERING.ps1" -ForegroundColor Gray
    Write-Host "  Full manifest:                    docs\GAP_REMEDIATION_MANIFEST.md`n" -ForegroundColor Gray
}

switch ($Action) {
    "docs"   { Show-Docs }
    "validate" {
        $validatePath = Join-Path $projectRoot "VALIDATE_REVERSE_ENGINEERING.ps1"
        if (Test-Path $validatePath) {
            & $validatePath
        } else {
            Write-Host "VALIDATE_REVERSE_ENGINEERING.ps1 not found at $validatePath" -ForegroundColor Yellow
        }
    }
    "launch-assistant" {
        & "$PSScriptRoot\UNIFIED_ASSISTANT_LAUNCHER.ps1" -Mode $Mode
    }
    "external-api" {
        if (-not $Prompt) { $Prompt = "Explain std::expected in C++20" }
        & "$PSScriptRoot\external_api_bridge.ps1" -Provider openai -Prompt $Prompt
    }
    "multi-agent" {
        & "$PSScriptRoot\multi_agent_parallel.ps1" -Tasks $Tasks
    }
    "all" {
        Show-Docs
        Write-Host "`nRunning validation..." -ForegroundColor Yellow
        $validatePath = Join-Path $projectRoot "VALIDATE_REVERSE_ENGINEERING.ps1"
        if (Test-Path $validatePath) { & $validatePath }
    }
}
