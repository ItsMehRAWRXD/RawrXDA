
# Cache for function results
$script:FunctionCache = @{}

function Get-FromCache {
    param([string]$Key)
    if ($script:FunctionCache.ContainsKey($Key)) {
        return $script:FunctionCache[$Key]
    }
    return $null
}

function Set-Cache {
    param([string]$Key, $Value)
    $script:FunctionCache[$Key] = $Value
}
# Import logging if available
if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error','Debug')][string]$Level = 'Info',
            [string]$Function = $null,
            [hashtable]$Data = $null
        )
        $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
        $caller = if ($Function) { $Function } else { (Get-PSCallStack)[1].FunctionName }
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$timestamp][$caller][$Level] $Message" -ForegroundColor $color
    }
}# RawrXD_UniversalIntegrator.psm1
# Integrates, connects, and configures all auto-generated, documented, and AI-enhanced modules

function Invoke-RawrXD_UniversalIntegration {
    [CmdletBinding()]
    param(
        [string]$AutoMethodsDir = "D:/lazy init ide/auto_generated_methods",
        [string]$AIInterventionModule = "D:/lazy init ide/auto_generated_methods/RawrXD_AIIntervention.psm1",
        [string]$ManifestDir = "D:/lazy init ide/orchestrator_smoke_output/manifest_tracer"
    )

    Write-Host "Starting RawrXD Universal Integration..."

    # Ensure core helpers (logging, config) are available to methods
    $loggingModule = 'D:/lazy init ide/RawrXD.Logging.psm1'
    $configModule = 'D:/lazy init ide/RawrXD.Config.psm1'
    if (Test-Path $loggingModule) {
        try { Import-Module $loggingModule -Force } catch { Write-Warning "Failed to import logging module: $_" }
    }
    if (Test-Path $configModule) {
        try { Import-Module $configModule -Force } catch { Write-Warning "Failed to import config module: $_" }
    }

    # Validate manifests and collect results
    $manifests = Get-ChildItem -Path $ManifestDir -Filter "*.json"
    $manifestResults = @()
    foreach ($manifest in $manifests) {
        Write-Host "Validating manifest: $($manifest.Name)"
        $mr = [PSCustomObject]@{ Name = $manifest.Name; Path = $manifest.FullName; Valid = $false; Error = $null }
        try {
            $content = Get-Content -Path $manifest.FullName -Raw | ConvertFrom-Json
            if ($content -and $content.Summary -and $content.Summary.TotalParsed -gt 0) {
                Write-Host "Manifest validated successfully: $($manifest.Name)" -ForegroundColor Green
                $mr.Valid = $true
            } else {
                Write-Warning "Manifest validation failed: $($manifest.Name)"
                $mr.Error = 'Parsing or TotalParsed==0'
            }
        } catch {
            Write-Error "Error validating manifest: $($manifest.Name). Error: $_"
            $mr.Error = $_.ToString()
        }
        $manifestResults += $mr
    }

    # Import all auto-generated method and feature files
    $methodFiles = Get-ChildItem -Path $AutoMethodsDir -Include "*_AutoMethod.ps1","*_AutoFeature.ps1" -File
    foreach ($file in $methodFiles) {
        try {
            . $file.FullName
            Write-Host "Imported: $($file.Name)"
        } catch {
            Write-Error "[UniversalIntegrator][ERROR] Failed to import $($file.Name): $_"
        }
    }

    # Import AI intervention module if present
    if (Test-Path $AIInterventionModule) {
        Import-Module $AIInterventionModule -Force
    }

    # Connect and configure methods (only attempt to execute *_AutoMethod stubs)
    $results = @()
    $autoMethodFiles = $methodFiles | Where-Object { $_.Name -like '*_AutoMethod.ps1' }
    foreach ($file in $autoMethodFiles) {
        $baseName = $file.BaseName -replace '_AutoMethod$',''
        $funcName = "Invoke-${baseName}Auto"
        $aiResult = $null
        $status = 'OK'
        $errorMsg = $null
        try {
            if (Get-Command Invoke-RawrXD_AIIntervention -ErrorAction SilentlyContinue) {
                $aiResult = Invoke-RawrXD_AIIntervention -SourceFile $baseName -MethodStubPath $file.FullName
            }
            if (Get-Command $funcName -ErrorAction SilentlyContinue) {
                $result = & $funcName
            } else {
                $result = 'skipped (no function)'
            }
        } catch {
            $status = 'ERROR'
            $errorMsg = $_
            $result = $null
        }
        $results += [PSCustomObject]@{
            Method = $funcName
            Result = $result
            AIIntervention = $aiResult
            Status = $status
            Error = $errorMsg
        }
    }

    # Health check: ensure all methods loaded and executed
    $healthy = ($results | Where-Object { $_.Status -eq 'OK' }).Count -eq $results.Count
    # Prepare serializable results (stringify Errors and non-serializable fields)
    $serialResults = @()
    foreach ($r in $results) {
        $err = $null
        if ($r.Error) {
            try { $err = $r.Error.ToString() } catch { $err = "<unserializable error>" }
        }
        $resValue = $null
        try {
            $resValue = $r.Result
        } catch { $resValue = "$($r.Result)" }
        $serialResults += [PSCustomObject]@{
            Method = $r.Method
            Result = $resValue
            AIIntervention = $r.AIIntervention
            Status = $r.Status
            Error = $err
        }
    }

    $report = [PSCustomObject]@{
        Timestamp = (Get-Date).ToString('o')
        AutoMethodsDir = $AutoMethodsDir
        TotalMethods = $results.Count
        Healthy = $healthy
        Results = $serialResults
        ManifestValidation = $manifestResults
        Analyzer = $null
    }

    # Write JSON integration report
    $reportPath = Join-Path $AutoMethodsDir 'integration_report.json'
    $report | ConvertTo-Json -Depth 6 | Set-Content $reportPath

    # Run analyzer to produce CI report (includes _AutoFeature files)
    $analyzer = Join-Path $AutoMethodsDir 'Analyze_AutoGeneratedMethods.ps1'
    if (Test-Path $analyzer) {
        try {
            . $analyzer
            $ciJson = Join-Path $AutoMethodsDir 'AutoGeneratedMethods_CIReport.json'
            if (Test-Path $ciJson) {
                $ciSummary = Get-Content $ciJson -Raw | ConvertFrom-Json
                $report.Analyzer = $ciSummary
                # update integration report with analyzer details
                $report | ConvertTo-Json -Depth 8 | Set-Content $reportPath
            }
        } catch {
            Write-Warning "Analyzer failed: $_"
        }
    }

    if ($healthy) {
        Write-Host "[UniversalIntegrator] All modules healthy and integrated. Report: $reportPath"
    } else {
        Write-Host "[UniversalIntegrator][WARNING] Some modules failed to load or execute. See $reportPath for details."
    }
    # Output integration summary
    $results | Format-Table -AutoSize

    Write-Host "RawrXD Universal Integration completed successfully." -ForegroundColor Cyan
}

Export-ModuleMember -Function Invoke-RawrXD_UniversalIntegration


