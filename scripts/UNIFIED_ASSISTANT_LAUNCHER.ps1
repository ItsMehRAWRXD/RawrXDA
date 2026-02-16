#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Unified Assistant Launcher - One command for full assistant stack

.DESCRIPTION
    Orchestrates: digest (if needed) -> enhanced chatbot or voice assistant.
    Fixes the "bad news" gap: single entry point for functional workflows.

.PARAMETER Mode
    enhanced = Enhanced chatbot (KB-powered)
    voice = Voice assistant
    digest-only = Just run source digester

.PARAMETER EnableVoice
    For voice mode: enable speech I/O

.PARAMETER EnableExternalAPI
    When set, chatbot can fallback to OpenAI/Anthropic (requires env keys)

.EXAMPLE
    .\UNIFIED_ASSISTANT_LAUNCHER.ps1 -Mode enhanced
.EXAMPLE
    .\UNIFIED_ASSISTANT_LAUNCHER.ps1 -Mode voice -EnableVoice
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('enhanced', 'voice', 'digest-only')]
    [string]$Mode = "enhanced",
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableVoice,
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableExternalAPI,
    
    [Parameter(Mandatory=$false)]
    [switch]$DigestFirst
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = if ($env:LAZY_INIT_IDE_ROOT) { $env:LAZY_INIT_IDE_ROOT } else { (Resolve-Path (Join-Path $PSScriptRoot "..") -ErrorAction SilentlyContinue).Path }
if (-not $projectRoot) { $projectRoot = (Get-Location).Path }

$kbPath = Join-Path $projectRoot "data" "knowledge_base.json"
$scriptsDir = $PSScriptRoot

Write-Host "`n╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║         RawrXD Unified Assistant Launcher                         ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Ensure KB exists or run digest
if (-not (Test-Path $kbPath) -or $DigestFirst) {
    Write-Host "  📚 Knowledge base missing or DigestFirst requested. Running source digester..." -ForegroundColor Yellow
    & "$scriptsDir\source_digester.ps1" -Operation digest
    if (-not (Test-Path $kbPath)) {
        Write-Host "  ⚠ Digestion may have failed. Proceeding in limited mode.`n" -ForegroundColor Yellow
    } else {
        Write-Host "  ✓ Knowledge base ready.`n" -ForegroundColor Green
    }
} else {
    Write-Host "  ✓ Knowledge base found: $kbPath`n" -ForegroundColor Green
}

switch ($Mode) {
    "digest-only" {
        if (-not (Test-Path $kbPath)) {
            & "$scriptsDir\source_digester.ps1" -Operation digest
        }
        Write-Host "Done. KB at $kbPath" -ForegroundColor Green
        exit 0
    }
    "enhanced" {
        if ($EnableExternalAPI) {
            Write-Host "  💡 External API fallback enabled (OPENAI_API_KEY / ANTHROPIC_API_KEY)`n" -ForegroundColor Cyan
        }
        & "$scriptsDir\ide_chatbot_enhanced.ps1" -Mode interactive @(if ($EnableExternalAPI) { "-UseExternalAPIFallback" })
    }
    "voice" {
        $voicePath = Join-Path $scriptsDir "voice_assistant.ps1"
        if ($EnableVoice) {
            & $voicePath -Mode voice -EnableVoice
        } else {
            & $voicePath -Mode interactive
        }
    }
}
