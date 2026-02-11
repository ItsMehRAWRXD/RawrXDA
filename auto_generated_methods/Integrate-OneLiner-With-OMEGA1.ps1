<#
.SYNOPSIS
    Integration script connecting RawrXD OMEGA-X One-Liner Generator with OMEGA-1 System
    
.DESCRIPTION
    Creates a bridge between the one-liner generator and the autonomous deployment system,
    enabling dynamic one-liner generation based on system state and requirements.
#>

# Import required modules
Import-Module "$PSScriptRoot\RawrXD-OneLiner-Generator.ps1" -Force

function Invoke-OMEGA1OneLinerIntegration {
    [CmdletBinding()]
    param (
        [string]$SystemState = "Normal",
        [switch]$AutoGenerate,
        [int]$MutationLevel = 1
    )
    
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║          OMEGA-1 + ONE-LINER GENERATOR INTEGRATION         ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    # Analyze current system state
    $systemAnalysis = Analyze-SystemState
    Write-Host "🔍 System Analysis:" -ForegroundColor Yellow
    $systemAnalysis.GetEnumerator() | ForEach-Object {
        Write-Host "  $($_.Key): $($_.Value)" -ForegroundColor White
    }
    Write-Host ""
    
    # Determine appropriate one-liner configuration based on system state
    $recommendedConfig = Get-RecommendedOneLinerConfig -SystemState $SystemState -Analysis $systemAnalysis
    Write-Host "🎯 Recommended Configuration:" -ForegroundColor Yellow
    $recommendedConfig.GetEnumerator() | ForEach-Object {
        Write-Host "  $($_.Key): $($_.Value)" -ForegroundColor White
    }
    Write-Host ""
    
    # Generate one-liner
    if ($AutoGenerate) {
        Write-Host "🚀 Auto-Generating One-Liner..." -ForegroundColor Green
        $oneLinerResult = Generate-OneLiner @recommendedConfig
        
        Write-Host "✅ Generated One-Liner:" -ForegroundColor Green
        Write-Host $oneLinerResult.OneLiner.Substring(0, 150) + "..." -ForegroundColor Gray
        Write-Host ""
        
        # Save integration results
        Save-IntegrationResults -OneLinerResult $oneLinerResult -SystemAnalysis $systemAnalysis
        
        return $oneLinerResult
    }
    
    return $recommendedConfig
}

function Analyze-SystemState {
    $analysis = @{}
    
    # Check module availability
    $modules = Get-ChildItem "$PSScriptRoot" -Filter "RawrXD.*.psm1" -ErrorAction SilentlyContinue
    $analysis["ModuleCount"] = $modules.Count
    $analysis["ModulesHealthy"] = ($modules.Count -ge 20)  # At least 20 modules expected
    
    # Check manifest integrity
    $manifestPath = "$PSScriptRoot\manifest.json"
    $analysis["ManifestExists"] = Test-Path $manifestPath
    if ($analysis["ManifestExists"]) {
        $manifest = Get-Content $manifestPath | ConvertFrom-Json
        $analysis["ManifestVersion"] = $manifest.Version
        $analysis["MutationCount"] = $manifest.MutationCount
    }
    
    # Check system resources
    $analysis["MemoryUsageMB"] = [math]::Round((Get-Process -Id $PID).WorkingSet64 / 1MB, 2)
    $analysis["CPUAvailable"] = (Get-CimInstance Win32_Processor).NumberOfCores
    
    # Check network connectivity (optional)
    try {
        $analysis["NetworkAvailable"] = Test-NetConnection -ComputerName "8.8.8.8" -Port 53 -InformationLevel Quiet
    } catch {
        $analysis["NetworkAvailable"] = $false
    }
    
    return $analysis
}

function Get-RecommendedOneLinerConfig {
    param (
        [string]$SystemState,
        [hashtable]$Analysis
    )
    
    $config = @{
        Tasks = @()
        MutationLevel = 1
        Hardened = $false
        EmitShellcode = $false
    }
    
    switch ($SystemState) {
        "Normal" {
            $config["Tasks"] = @("create-dir", "write-file", "execute", "telemetry")
            $config["MutationLevel"] = 1
        }
        "Enhanced" {
            $config["Tasks"] = @("create-dir", "write-file", "execute", "loop", "self-mutate", "telemetry")
            $config["MutationLevel"] = 2
        }
        "Hardened" {
            $config["Tasks"] = @("create-dir", "hardware-key", "sandbox-check", "memory-protect", "execute", "integrity-check")
            $config["MutationLevel"] = 3
            $config["Hardened"] = $true
        }
        "Agentic" {
            $config["Tasks"] = @("create-dir", "write-file", "execute", "loop", "self-mutate", "beacon", "telemetry")
            $config["MutationLevel"] = 2
            $config["EmitShellcode"] = $true
        }
        default {
            $config["Tasks"] = @("create-dir", "write-file", "execute", "telemetry")
        }
    }
    
    # Adjust based on system analysis
    if ($Analysis["ModulesHealthy"] -eq $false) {
        $config["Tasks"] += "integrity-check"
    }
    
    if ($Analysis["NetworkAvailable"] -eq $true) {
        $config["Tasks"] += "beacon"
    }
    
    return $config
}

function Save-IntegrationResults {
    param (
        [hashtable]$OneLinerResult,
        [hashtable]$SystemAnalysis
    )
    
    $integrationData = @{
        Timestamp = Get-Date
        OneLinerResult = $OneLinerResult
        SystemAnalysis = $SystemAnalysis
        IntegrationVersion = "1.0"
    }
    
    $integrationPath = "$PSScriptRoot\one-liner-integration-results.json"
    $integrationData | ConvertTo-Json -Depth 5 | Set-Content $integrationPath
    
    Write-Host "📁 Integration results saved to: $integrationPath" -ForegroundColor Cyan
}

# Export functions
Export-ModuleMember -Function Invoke-OMEGA1OneLinerIntegration, Analyze-SystemState

# --- DEMONSTRATION ---
if ($MyInvocation.MyCommand.Name -eq $PSCommandPath) {
    Write-Host "🧪 Demonstrating OMEGA-1 + One-Liner Integration" -ForegroundColor Yellow
    Write-Host ""
    
    # Test different system states
    $states = @("Normal", "Enhanced", "Hardened", "Agentic")
    
    foreach ($state in $states) {
        Write-Host "Testing System State: $state" -ForegroundColor Cyan
        $result = Invoke-OMEGA1OneLinerIntegration -SystemState $state -AutoGenerate
        Write-Host ""
    }
    
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║                INTEGRATION DEMO COMPLETE                    ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Green
}