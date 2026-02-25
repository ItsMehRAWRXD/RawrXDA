# ============================================================================
# File: D:\lazy init ide\scripts\Run-TODOAutoResolver_v2.ps1
# Purpose: Minimal runner for TODOAutoResolver_v2
# ============================================================================

#Requires -Version 7.0

$modulePath = Join-Path $PSScriptRoot 'TODOAutoResolver_v2.psm1'
Import-Module $modulePath -Force

# Run a safe analysis-only pass (no modifications by default)
Invoke-IntelligentTODOResolution -Mode Hybrid -ConfidenceThreshold 0.80 -GenerateReport -WhatIf
