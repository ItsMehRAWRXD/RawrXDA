# Production-ready feature: SelfHealingModule (Simplified)
# Import shared helpers if present
$scriptRoot = Split-Path -Parent $PSScriptRoot
$loggingModule = Join-Path $scriptRoot 'RawrXD.Logging.psm1'
$configModule = Join-Path $scriptRoot 'RawrXD.Config.psm1'
if (Test-Path $loggingModule) { try { Import-Module $loggingModule -Force -ErrorAction SilentlyContinue } catch { } }
if (Test-Path $configModule) { try { Import-Module $configModule -Force -ErrorAction SilentlyContinue } catch { } }

# Fallback logging function
if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param([string]$Message, [string]$Level = 'Info', [hashtable]$Context = $null)
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$Level] $Message" -ForegroundColor $color
    }
}

function Invoke-SelfHealingModule {
    [CmdletBinding()]
    param(
        [string]$ModuleDir = $null,
        [switch]$Recurse,
        [switch]$NoInvoke,
        [int]$MaxRetries = 2
    )
    
    # Setup
    if (-not $ModuleDir) { 
        $ModuleDir = Join-Path (Split-Path -Parent $PSScriptRoot) 'auto_generated_methods' 
    }
    
    if (-not (Test-Path -Path $ModuleDir -PathType Container)) {
        $msg = "ModuleDir '$ModuleDir' not found."
        Write-StructuredLog -Message $msg -Level Error
        throw $msg
    }
    
    $results = @()
    $gciParams = @{ Path = $ModuleDir; Filter = '*_AutoFeature.ps1'; ErrorAction = 'SilentlyContinue' }
    if ($Recurse) { $gciParams['Recurse'] = $true }
    $moduleFiles = Get-ChildItem @gciParams
    
    Write-StructuredLog -Message "Processing $($moduleFiles.Count) auto-feature files" -Level Info
    
    # Process each file
    foreach ($file in $moduleFiles) {
        $entry = @{
            File       = $file.FullName
            Reloaded   = $false
            Invoked    = $false
            Error      = $null
            DurationMs = 0
        }
        
        # Dot-source the script (safe, measured)
        try {
            $dotDuration = Measure-Command { . $file.FullName }
            $entry.DurationMs += $dotDuration.TotalMilliseconds
            $msg = "Dot-sourced $($file.Name) in $([math]::Round($dotDuration.TotalMilliseconds,2))ms"
            Write-StructuredLog -Message $msg -Level Debug
        } catch {
            $entry.Error = $_.Exception.Message
            $msg = "Failed to dot-source $($file.Name): $($entry.Error)"
            Write-StructuredLog -Message $msg -Level Error
            
            # Attempt reload
            for ($i = 0; $i -lt $MaxRetries; $i++) {
                try {
                    $reloadDuration = Measure-Command { . $file.FullName }
                    $entry.DurationMs += $reloadDuration.TotalMilliseconds
                    $entry.Reloaded = $true
                    $entry.Error = $null
                    $msg = "Reloaded $($file.Name) attempt $($i+1) in $([math]::Round($reloadDuration.TotalMilliseconds,2))ms"
                    Write-StructuredLog -Message $msg -Level Info
                    break
                } catch {
                    $entry.Error = $_.Exception.Message
                }
            }
            
            $results += $entry
            continue
        }
        
        # Resolve the expected function name and optionally invoke it
        $funcName = 'Invoke-' + ($file.BaseName -replace '_AutoFeature$','')
        $cmd = Get-Command -Name $funcName -ErrorAction SilentlyContinue
        if (-not $cmd) {
            $msg = "Function $funcName not found after dot-source."
            Write-StructuredLog -Message $msg -Level Warning
            $entry.Error = 'FunctionNotFound'
            $results += $entry
            continue
        }
        
        if ($NoInvoke) {
            $results += $entry
            continue
        }
        
        # Invoke function
        try {
            $invokeDuration = Measure-Command { & $funcName -ErrorAction Stop | Out-Null }
            $entry.DurationMs += $invokeDuration.TotalMilliseconds
            $entry.Invoked = $true
            $msg = "Invoked $funcName in $([math]::Round($invokeDuration.TotalMilliseconds,2))ms"
            Write-StructuredLog -Message $msg -Level Info
        } catch {
            $entry.Error = $_.Exception.Message
            $msg = "Invocation failed for $funcName : $($entry.Error). Attempting reload..."
            Write-StructuredLog -Message $msg -Level Error
            
            # Attempt reload and retry invoke
            for ($i = 0; $i -lt $MaxRetries; $i++) {
                try {
                    . $file.FullName
                    $entry.Reloaded = $true
                    $msg = "Reloaded $($file.Name) attempt $($i+1)"
                    Write-StructuredLog -Message $msg -Level Info
                    try {
                        $retryDuration = Measure-Command { & $funcName -ErrorAction Stop | Out-Null }
                        $entry.DurationMs += $retryDuration.TotalMilliseconds
                        $entry.Invoked = $true
                        $entry.Error = $null
                        break
                    } catch {
                        $entry.Error = $_.Exception.Message
                    }
                } catch {
                    $entry.Error = $_.Exception.Message
                }
            }
        }
        
        $results += $entry
    }
    
    # Summary
    $successful = ($results | Where-Object { $_.Invoked -eq $true -or ($_.Error -eq $null) }).Count
    $failed = ($results | Where-Object { $_.Error -ne $null }).Count
    
    Write-StructuredLog -Message "SelfHealingModule complete. Success: $successful, Failed: $failed" -Level Info
    
    return $results
}