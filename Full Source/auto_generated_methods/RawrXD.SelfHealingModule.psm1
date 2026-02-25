
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
}<#
.SYNOPSIS
    RawrXD Self-Healing Module
    
.DESCRIPTION
    Production-ready module for automated system health monitoring and recovery.
    Provides comprehensive component health checking, automatic recovery, and system optimization.
    
.AUTHOR
    RawrXD Auto-Generation System
    
.VERSION
    1.0.0
#>

# Import required helpers
$scriptRoot = Split-Path -Parent $PSScriptRoot
$loggingModule = Join-Path $scriptRoot 'RawrXD.Logging.psm1'
$configModule = Join-Path $scriptRoot 'RawrXD.Config.psm1'

if (Test-Path $loggingModule) { 
    try { Import-Module $loggingModule -Force -ErrorAction SilentlyContinue } catch { } 
}
if (Test-Path $configModule) { 
    try { Import-Module $configModule -Force -ErrorAction SilentlyContinue } catch { } 
}

# Fallback logging if not available
if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param([string]$Message, [string]$Level = 'Info', [hashtable]$Context = $null)
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$Level] $Message" -ForegroundColor $color
    }
}

function Invoke-SelfHealingModule {
    <#
    .SYNOPSIS
        Performs comprehensive health monitoring and automatic recovery for PowerShell modules
        
    .DESCRIPTION
        Scans, loads, and tests PowerShell auto-feature modules with automatic recovery capabilities.
        Provides detailed health reporting, retry logic, and component validation.
        
    .PARAMETER ModuleDir
        Directory containing modules to monitor and heal (defaults to auto_generated_methods)
        
    .PARAMETER Recurse
        Recursively search subdirectories for modules
        
    .PARAMETER NoInvoke
        Load modules without invoking their main functions (dry run)
        
    .PARAMETER MaxRetries
        Maximum number of retry attempts for failed components
        
    .PARAMETER EnableAutoRecovery
        Automatically attempt to recover failed components
        
    .PARAMETER GenerateReport
        Generate comprehensive health report
        
    .PARAMETER ReportPath
        Path for health report output
        
    .EXAMPLE
        Invoke-SelfHealingModule -EnableAutoRecovery -GenerateReport
        
    .EXAMPLE
        Invoke-SelfHealingModule -ModuleDir "C:\Modules" -NoInvoke -MaxRetries 3
        
    .OUTPUTS
        System.Object[]
        Returns array of component health results
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string]$ModuleDir = $null,
        
        [Parameter(Mandatory=$false)]
        [switch]$Recurse,
        
        [Parameter(Mandatory=$false)]
        [switch]$NoInvoke,
        
        [Parameter(Mandatory=$false)]
        [ValidateRange(0, 10)]
        [int]$MaxRetries = 2,
        
        [Parameter(Mandatory=$false)]
        [switch]$EnableAutoRecovery,
        
        [Parameter(Mandatory=$false)]
        [switch]$GenerateReport,
        
        [Parameter(Mandatory=$false)]
        [string]$ReportPath = $null
    )
    
    # Setup and validation
    if (-not $ModuleDir) { 
        $ModuleDir = Join-Path (Split-Path -Parent $PSScriptRoot) 'auto_generated_methods' 
    }
    
    if (-not (Test-Path -Path $ModuleDir -PathType Container)) {
        $msg = "ModuleDir '$ModuleDir' not found."
        Write-StructuredLog -Message $msg -Level Error
        throw $msg
    }
    
    Write-StructuredLog -Message "Starting Self-Healing Module scan for: $ModuleDir" -Level Info
    
    # Get target files
    $gciParams = @{ 
        Path = $ModuleDir
        Filter = '*_AutoFeature.ps1'
        ErrorAction = 'SilentlyContinue' 
    }
    if ($Recurse) { $gciParams['Recurse'] = $true }
    $moduleFiles = Get-ChildItem @gciParams
    
    Write-StructuredLog -Message "Found $($moduleFiles.Count) auto-feature files to process" -Level Info
    
    $results = @()
    $successCount = 0
    $failureCount = 0
    
    # Process each module file
    foreach ($file in $moduleFiles) {
        $startTime = Get-Date
        
        $entry = @{
            File = $file.FullName
            FileName = $file.Name
            Function = 'Invoke-' + ($file.BaseName -replace '_AutoFeature$','')
            LoadTime = $null
            InvokeTime = $null
            TotalTime = $null
            Loaded = $false
            Invoked = $false
            Reloaded = $false
            Error = $null
            ErrorCount = 0
            HealthScore = 0
            Status = 'Unknown'
        }
        
        try {
            # Phase 1: Load (dot-source)
            $loadStart = Get-Date
            . $file.FullName
            $loadEnd = Get-Date
            $entry.LoadTime = ($loadEnd - $loadStart).TotalMilliseconds
            $entry.Loaded = $true
            
            Write-StructuredLog -Message "Loaded: $($file.Name) in $([math]::Round($entry.LoadTime,2))ms" -Level Debug
            
            # Phase 2: Validate function exists
            $cmd = Get-Command -Name $entry.Function -ErrorAction SilentlyContinue
            if (-not $cmd) {
                $entry.Error = "Function $($entry.Function) not found after loading"
                $entry.ErrorCount++
                Write-StructuredLog -Message $entry.Error -Level Warning
            }
            
            # Phase 3: Invoke function (if requested and function exists)
            if (-not $NoInvoke -and $cmd) {
                try {
                    $invokeStart = Get-Date
                    & $entry.Function -ErrorAction Stop | Out-Null
                    $invokeEnd = Get-Date
                    $entry.InvokeTime = ($invokeEnd - $invokeStart).TotalMilliseconds
                    $entry.Invoked = $true
                    
                    Write-StructuredLog -Message "Invoked: $($entry.Function) in $([math]::Round($entry.InvokeTime,2))ms" -Level Info
                    
                } catch {
                    $entry.Error = "Invocation failed: $($_.Exception.Message)"
                    $entry.ErrorCount++
                    Write-StructuredLog -Message $entry.Error -Level Error
                    
                    # Auto-recovery if enabled
                    if ($EnableAutoRecovery -and $MaxRetries -gt 0) {
                        Write-StructuredLog -Message "Attempting auto-recovery for $($entry.Function)" -Level Warning
                        
                        for ($retry = 1; $retry -le $MaxRetries; $retry++) {
                            try {
                                Start-Sleep -Milliseconds (250 * $retry)  # Progressive backoff
                                
                                # Reload and retry
                                . $file.FullName
                                & $entry.Function -ErrorAction Stop | Out-Null
                                
                                # Success
                                $entry.Reloaded = $true
                                $entry.Invoked = $true
                                $entry.Error = $null
                                $entry.ErrorCount = 0
                                Write-StructuredLog -Message "Auto-recovery successful for $($entry.Function) on attempt $retry" -Level Info
                                break
                                
                            } catch {
                                Write-StructuredLog -Message "Recovery attempt $retry failed: $($_.Exception.Message)" -Level Warning
                                if ($retry -eq $MaxRetries) {
                                    $entry.Error = "Recovery failed after $MaxRetries attempts: $($_.Exception.Message)"
                                }
                            }
                        }
                    }
                }
            }
            
        } catch {
            $entry.Error = "Loading failed: $($_.Exception.Message)"
            $entry.ErrorCount++
            Write-StructuredLog -Message $entry.Error -Level Error
        }
        
        # Calculate metrics
        $endTime = Get-Date
        $entry.TotalTime = ($endTime - $startTime).TotalMilliseconds
        
        # Health scoring (0-100)
        $entry.HealthScore = 100
        if (-not $entry.Loaded) { $entry.HealthScore -= 50 }
        if (-not $entry.Invoked -and -not $NoInvoke) { $entry.HealthScore -= 30 }
        if ($entry.ErrorCount -gt 0) { $entry.HealthScore -= ($entry.ErrorCount * 10) }
        if ($entry.Reloaded) { $entry.HealthScore -= 5 }  # Small penalty for needing recovery
        $entry.HealthScore = [Math]::Max(0, $entry.HealthScore)
        
        # Status determination
        if ($entry.HealthScore -ge 90) { $entry.Status = 'Excellent' }
        elseif ($entry.HealthScore -ge 70) { $entry.Status = 'Good' }
        elseif ($entry.HealthScore -ge 50) { $entry.Status = 'Fair' }
        elseif ($entry.HealthScore -ge 30) { $entry.Status = 'Poor' }
        else { $entry.Status = 'Critical' }
        
        if ($entry.Error) { $failureCount++ } else { $successCount++ }
        
        $results += $entry
    }
    
    # Summary
    Write-StructuredLog -Message "Self-Healing Module completed. Success: $successCount, Failed: $failureCount" -Level Info
    
    # Generate comprehensive report if requested
    if ($GenerateReport) {
        $healthReport = @{
            Timestamp = (Get-Date).ToString('o')
            ModuleDir = $ModuleDir
            Configuration = @{
                Recurse = $Recurse.IsPresent
                NoInvoke = $NoInvoke.IsPresent
                MaxRetries = $MaxRetries
                EnableAutoRecovery = $EnableAutoRecovery.IsPresent
            }
            Summary = @{
                TotalModules = $results.Count
                SuccessCount = $successCount
                FailureCount = $failureCount
                SuccessRate = if ($results.Count -gt 0) { [math]::Round(($successCount / $results.Count) * 100, 2) } else { 0 }
                AverageHealthScore = if ($results.Count -gt 0) { [math]::Round(($results | Measure-Object -Property HealthScore -Average).Average, 2) } else { 0 }
                TotalLoadTime = [math]::Round(($results | Measure-Object -Property LoadTime -Sum).Sum, 2)
                TotalInvokeTime = [math]::Round(($results | Measure-Object -Property InvokeTime -Sum).Sum, 2)
            }
            Results = $results
        }
        
        if (-not $ReportPath) {
            $ReportPath = Join-Path $ModuleDir "SelfHealing_Report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
        }
        
        # Ensure report directory exists
        $reportDir = Split-Path $ReportPath -Parent
        if (-not (Test-Path $reportDir)) {
            New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
        }
        
        $healthReport | ConvertTo-Json -Depth 10 | Set-Content $ReportPath -Encoding UTF8
        Write-StructuredLog -Message "Health report generated: $ReportPath" -Level Info
    }
    
    return $results
}

function Test-ComponentHealth {
    <#
    .SYNOPSIS
        Performs detailed health check on a specific PowerShell component
        
    .DESCRIPTION
        Validates file existence, parseability, function definitions, and dependencies
        for a PowerShell script or module file.
        
    .PARAMETER FilePath
        Path to the PowerShell file to test
        
    .PARAMETER FunctionName
        Expected function name to validate
        
    .EXAMPLE
        Test-ComponentHealth -FilePath "C:\Scripts\MyScript.ps1" -FunctionName "Invoke-MyFunction"
        
    .OUTPUTS
        System.Collections.Hashtable
        Returns detailed health information
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [ValidateScript({Test-Path $_})]
        [string]$FilePath,
        
        [Parameter(Mandatory=$false)]
        [string]$FunctionName
    )
    
    $health = @{
        FilePath = $FilePath
        FunctionName = $FunctionName
        FileExists = $false
        Parseable = $false
        FunctionDefined = $false
        DependenciesMet = $true
        FileSize = 0
        LastModified = $null
        Issues = @()
        Recommendations = @()
        HealthScore = 0
    }
    
    try {
        # File existence and basic info
        $fileInfo = Get-Item $FilePath -ErrorAction Stop
        $health.FileExists = $true
        $health.FileSize = $fileInfo.Length
        $health.LastModified = $fileInfo.LastWriteTime.ToString('o')
        
        # Parseability test
        $content = Get-Content $FilePath -Raw -ErrorAction Stop
        if ([string]::IsNullOrWhiteSpace($content)) {
            $health.Issues += "File is empty or contains only whitespace"
        } else {
            try {
                $tokens = $null
                $errors = $null
                $ast = [System.Management.Automation.Language.Parser]::ParseInput($content, [ref]$tokens, [ref]$errors)
                
                if ($errors.Count -eq 0) {
                    $health.Parseable = $true
                } else {
                    $health.Issues += "Parse errors found: $($errors.Count)"
                    foreach ($error in $errors | Select-Object -First 5) {
                        $health.Issues += "  Line $($error.Extent.StartLineNumber): $($error.Message)"
                    }
                }
            } catch {
                $health.Issues += "AST parsing failed: $($_.Exception.Message)"
            }
        }
        
        # Function validation (if specified)
        if ($FunctionName) {
            try {
                . $FilePath
                $cmd = Get-Command -Name $FunctionName -ErrorAction SilentlyContinue
                if ($cmd) {
                    $health.FunctionDefined = $true
                } else {
                    $health.Issues += "Function '$FunctionName' not found after loading"
                }
            } catch {
                $health.Issues += "Function validation failed: $($_.Exception.Message)"
            }
        }
        
        # Dependency analysis
        $missingDeps = @()
        $importMatches = [regex]::Matches($content, 'Import-Module\s+[''"]?([^''";\s]+)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        foreach ($match in $importMatches) {
            $depModule = $match.Groups[1].Value
            if (-not (Get-Module -ListAvailable -Name $depModule -ErrorAction SilentlyContinue)) {
                $missingDeps += $depModule
            }
        }
        
        if ($missingDeps.Count -gt 0) {
            $health.DependenciesMet = $false
            $health.Issues += "Missing dependencies: $($missingDeps -join ', ')"
        }
        
        # Health scoring
        $health.HealthScore = 100
        if (-not $health.FileExists) { $health.HealthScore = 0 }
        elseif (-not $health.Parseable) { $health.HealthScore -= 40 }
        elseif (-not $health.FunctionDefined -and $FunctionName) { $health.HealthScore -= 30 }
        elseif (-not $health.DependenciesMet) { $health.HealthScore -= 20 }
        
        # Recommendations
        if ($health.FileSize -gt 1MB) {
            $health.Recommendations += "Large file size ($([math]::Round($health.FileSize/1MB,2))MB) - consider splitting into smaller modules"
        }
        if ($missingDeps.Count -gt 0) {
            $health.Recommendations += "Install missing dependencies or add error handling for optional modules"
        }
        if ($health.Issues.Count -gt 3) {
            $health.Recommendations += "High number of issues detected - manual review recommended"
        }
        
    } catch {
        $health.Issues += "Health check error: $($_.Exception.Message)"
        $health.HealthScore = 0
    }
    
    return $health
}

# Export public functions
Export-ModuleMember -Function @(
    'Invoke-SelfHealingModule',
    'Test-ComponentHealth'
)

