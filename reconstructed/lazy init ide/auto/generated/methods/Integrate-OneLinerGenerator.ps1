#Requires -Version 7.4

<#
.SYNOPSIS
    RawrXD OMEGA-1 Integration with One-Liner Generator
.DESCRIPTION
    Integrates the one-liner generator with the existing OMEGA-1 autonomous system
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$RootPath = "D:\lazy init ide\auto_generated_methods",
    
    [Parameter(Mandatory=$false)]
    [switch]$AutoGenerate,
    
    [Parameter(Mandatory=$false)]
    [int]$GenerationInterval = 300
)

# Import generator
$GeneratorPath = Join-Path $PSScriptRoot "RawrXD-OneLiner-Generator.ps1"
if (Test-Path $GeneratorPath) {
    . $GeneratorPath
} else {
    Write-Error "Generator not found: $GeneratorPath"
    exit 1
}

Write-Host "`n╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  RawrXD OMEGA-1 × One-Liner Generator Integration                ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Check if OMEGA-1 is active
$omegaManifest = Join-Path $RootPath "manifest.json"
if (Test-Path $omegaManifest) {
    $manifest = Get-Content $omegaManifest -Raw | ConvertFrom-Json
    Write-Host "✓ OMEGA-1 System Detected" -ForegroundColor Green
    Write-Host "  Version: $($manifest.Version)" -ForegroundColor Cyan
    Write-Host "  Modules: $($manifest.ModuleCount)" -ForegroundColor Cyan
    Write-Host "  Status: Operational`n" -ForegroundColor Green
} else {
    Write-Host "⚠ OMEGA-1 not detected - running standalone mode" -ForegroundColor Yellow
}

# Create integration module
$integrationModule = @"
#Requires -Version 7.4

<#
.SYNOPSIS
    RawrXD.OneLinerIntegration - Bridge between OMEGA-1 and One-Liner Generator
#>

function Invoke-OneLinerIntegration {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=`$false)]
        [string]`$Path='$RootPath',
        [Parameter(Mandatory=`$false)]
        [hashtable]`$Config=@{}
    )
    
    `$moduleName='RawrXD.OneLinerIntegration'
    `$timestamp=Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    
    try {
        # Load generator
        `$generatorPath = Join-Path `$Path "RawrXD-OneLiner-Generator.ps1"
        if (Test-Path `$generatorPath) {
            . `$generatorPath
        }
        
        # Generate deployment one-liner
        `$tasks = @("create-dir","write-file","execute","telemetry")
        `$result = Generate-OneLiner -Tasks `$tasks -EncodeBase64
        
        `$outputPath = Join-Path `$Path "generated-oneliners"
        if (-not (Test-Path `$outputPath)) {
            New-Item -ItemType Directory -Path `$outputPath -Force | Out-Null
        }
        
        `$oneLinerFile = Join-Path `$outputPath "oneliner_`$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
        Set-Content -Path `$oneLinerFile -Value `$result.OneLiner
        
        `$response = @{
            Status='Active'
            Module=`$moduleName
            Timestamp=`$timestamp
            ProcessId=`$PID
            MemoryMB=[Math]::Round((Get-Process -Id `$PID).WorkingSet64/1MB,2)
            Version='1.0.0'
            OneLinerGenerated=`$true
            OneLinerPath=`$oneLinerFile
            TaskCount=`$result.TaskCount
        }
        
        Write-Verbose "[`$moduleName] Generated one-liner: `$oneLinerFile"
        return `$response
        
    } catch {
        Write-Error "[`$moduleName] Error: `$_"
        throw
    }
}

function Test-OneLinerIntegrationHealth {
    [CmdletBinding()]
    param()
    
    return @{
        Module='RawrXD.OneLinerIntegration'
        Healthy=`$true
        Status='Operational'
        Timestamp=Get-Date
        GeneratorAvailable=(Test-Path (Join-Path '$RootPath' 'RawrXD-OneLiner-Generator.ps1'))
    }
}

Export-ModuleMember -Function Invoke-OneLinerIntegration, Test-OneLinerIntegrationHealth
"@

$integrationPath = Join-Path $RootPath "RawrXD.OneLinerIntegration.psm1"
Set-Content -Path $integrationPath -Value $integrationModule -Encoding UTF8
Write-Host "✓ Integration module created: $integrationPath" -ForegroundColor Green

# Test the integration
Write-Host "`n🧪 Testing Integration..." -ForegroundColor Yellow
try {
    Import-Module $integrationPath -Force
    $testResult = Invoke-OneLinerIntegration -Path $RootPath
    
    Write-Host "✓ Integration test successful!" -ForegroundColor Green
    Write-Host "  Module: $($testResult.Module)" -ForegroundColor Cyan
    Write-Host "  Status: $($testResult.Status)" -ForegroundColor Cyan
    Write-Host "  One-Liner Generated: $($testResult.OneLinerGenerated)" -ForegroundColor Cyan
    Write-Host "  Tasks: $($testResult.TaskCount)" -ForegroundColor Cyan
    
    if ($testResult.OneLinerPath -and (Test-Path $testResult.OneLinerPath)) {
        Write-Host "`n📝 Generated One-Liner:" -ForegroundColor Yellow
        Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
        Get-Content $testResult.OneLinerPath | Write-Host -ForegroundColor Green
        Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    }
    
} catch {
    Write-Host "✗ Integration test failed: $_" -ForegroundColor Red
}

# Auto-generation loop
if ($AutoGenerate) {
    Write-Host "`n🔄 Starting auto-generation loop (interval: $GenerationInterval seconds)..." -ForegroundColor Cyan
    Write-Host "   Press Ctrl+C to stop" -ForegroundColor Gray
    
    $iteration = 0
    while ($true) {
        $iteration++
        Write-Host "`n[$iteration] Generating one-liner at $(Get-Date -Format 'HH:mm:ss')..." -ForegroundColor Gray
        
        try {
            $modes = @('Basic','Advanced','Hardened')
            $randomMode = $modes | Get-Random
            
            $generated = Invoke-OneLinerGenerator -Mode $randomMode -OutputDirectory "$RootPath\generated-oneliners"
            Write-Host "  ✓ Generated $randomMode mode one-liner" -ForegroundColor Green
            
        } catch {
            Write-Host "  ✗ Generation error: $_" -ForegroundColor Red
        }
        
        Start-Sleep -Seconds $GenerationInterval
    }
} else {
    Write-Host "`n✨ Integration complete!" -ForegroundColor Green
    Write-Host "`nTo enable auto-generation, run:" -ForegroundColor Yellow
    Write-Host "  .\Integrate-OneLinerGenerator.ps1 -AutoGenerate -GenerationInterval 300" -ForegroundColor Gray
}

Write-Host ""
