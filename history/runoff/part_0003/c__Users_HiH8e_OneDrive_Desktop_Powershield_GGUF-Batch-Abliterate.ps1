#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Batch GGUF Model Abliteration - Process multiple models
.DESCRIPTION
    Processes all GGUF models in a directory and creates no-refusal versions
.PARAMETER ModelsPath
    Directory containing GGUF models
.PARAMETER OutputPath
    Directory for abliterated models (optional)
.PARAMETER Method
    Modification method: 'Metadata' or 'Weights'
.PARAMETER AggressionLevel
    How aggressive: 'Low', 'Medium', 'High', 'Extreme'
.PARAMETER Filter
    File filter pattern (default: *.gguf)
.EXAMPLE
    .\GGUF-Batch-Abliterate.ps1 -ModelsPath "D:\OllamaModels" -Method Metadata
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory=$false)]
    [string]$ModelsPath = "D:\OllamaModels",
    
    [Parameter(Mandatory=$false)]
    [string]$OutputPath = "",
    
    [Parameter(Mandatory=$false)]
    [ValidateSet('Metadata', 'Weights')]
    [string]$Method = 'Metadata',
    
    [Parameter(Mandatory=$false)]
    [ValidateSet('Low', 'Medium', 'High', 'Extreme')]
    [string]$AggressionLevel = 'Medium',
    
    [Parameter(Mandatory=$false)]
    [string]$Filter = "*.gguf"
)

function Write-Header { param([string]$Message) Write-Host "`n$('='*70)`n$Message`n$('='*70)" -ForegroundColor Magenta }
function Write-Info { param([string]$Message) Write-Host "[*] $Message" -ForegroundColor Cyan }
function Write-Success { param([string]$Message) Write-Host "[✓] $Message" -ForegroundColor Green }
function Write-Warning { param([string]$Message) Write-Host "[!] $Message" -ForegroundColor Yellow }

# Validate paths
if (-not (Test-Path $ModelsPath)) {
    Write-Error "Models path not found: $ModelsPath"
    exit 1
}

# Set output path if not specified
if ([string]::IsNullOrEmpty($OutputPath)) {
    $OutputPath = Join-Path $ModelsPath "NoRefuse"
}

# Create output directory
if (-not (Test-Path $OutputPath)) {
    New-Item -Path $OutputPath -ItemType Directory -Force | Out-Null
    Write-Success "Created output directory: $OutputPath"
}

Write-Header "Batch GGUF Model Abliteration"
Write-Info "Input: $ModelsPath"
Write-Info "Output: $OutputPath"
Write-Info "Method: $Method"
Write-Info "Aggression: $AggressionLevel"
Write-Info "Filter: $Filter"

# Find all GGUF models
$models = Get-ChildItem -Path $ModelsPath -Filter $Filter -File | 
    Where-Object { $_.Name -notmatch 'NOREFUSE|NO-REFUSE' }

if ($models.Count -eq 0) {
    Write-Warning "No GGUF models found matching filter: $Filter"
    exit 0
}

Write-Info "Found $($models.Count) models to process"
Write-Host ""

# Process each model
$processed = 0
$failed = 0
$startTime = Get-Date

foreach ($model in $models) {
    $modelNum = $processed + $failed + 1
    Write-Host "[$modelNum/$($models.Count)] Processing: $($model.Name)" -ForegroundColor Yellow
    
    $outputFile = Join-Path $OutputPath "$($model.BaseName)-NOREFUSE$($model.Extension)"
    
    try {
        # Call the main abliteration script
        $scriptPath = Join-Path $PSScriptRoot "GGUF-Abliterate.ps1"
        
        if (-not (Test-Path $scriptPath)) {
            Write-Warning "GGUF-Abliterate.ps1 not found in same directory"
            Write-Info "Creating it now..."
            # Script will be created by the other file
            Write-Error "Please ensure GGUF-Abliterate.ps1 exists"
            $failed++
            continue
        }
        
        & $scriptPath -ModelPath $model.FullName -OutputPath $outputFile -Method $Method -AggressionLevel $AggressionLevel
        
        if ($LASTEXITCODE -eq 0) {
            $processed++
            Write-Success "Completed: $($model.Name)"
        } else {
            $failed++
            Write-Warning "Failed: $($model.Name)"
        }
        
    } catch {
        $failed++
        Write-Warning "Error processing $($model.Name): $_"
    }
    
    Write-Host ""
}

# Summary
$elapsed = (Get-Date) - $startTime
Write-Header "Batch Processing Complete"
Write-Info "Processed: $processed models"
if ($failed -gt 0) {
    Write-Warning "Failed: $failed models"
}
Write-Info "Time elapsed: $($elapsed.ToString('hh\:mm\:ss'))"
Write-Info "Output directory: $OutputPath"

Write-Host "`n" -NoNewline
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Review the Modelfiles in: $OutputPath" -ForegroundColor Gray
Write-Host "2. Import models to Ollama using the generated Modelfiles" -ForegroundColor Gray
Write-Host "3. Test with sensitive prompts to verify abliteration" -ForegroundColor Gray
