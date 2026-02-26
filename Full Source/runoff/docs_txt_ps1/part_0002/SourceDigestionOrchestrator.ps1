<#
.SYNOPSIS
    Source Digestion Orchestrator - Unified Source Code Analysis and Reverse Engineering System
.DESCRIPTION
    Comprehensive orchestration system that combines:
    - Source Digestion Engine
    - Reverse Engineering Engine
    - Deployment Audit Engine
    
    Provides automated analysis, reverse engineering, and deployment auditing
    for multi-language projects with integrated reporting and visualization.
#>

using namespace System.Collections.Generic
using namespace System.IO

param(
    [Parameter(Mandatory=$false)]
    [string]$SourcePath,
    
    [Parameter(Mandatory=$false)]
    [string]$OutputPath = ".\source_digestion_output",
    
    [Parameter(Mandatory=$false)]
    [string]$ConfigPath = ".\orchestrator_config.json",
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableAllEngines,
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableSourceDigestion,
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableReverseEngineering,
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableDeploymentAudit,

    [Parameter(Mandatory=$false)]
    [switch]$EnableManifestTracer,

    [Parameter(Mandatory=$false)]
    [switch]$EnableArchitectureEnhancement,
    
    [Parameter(Mandatory=$false)]
    [switch]$GenerateComprehensiveReport,
    
    [Parameter(Mandatory=$false)]
    [string]$ReportFormat = "HTML",
    
    [Parameter(Mandatory=$false)]
    [switch]$Interactive,
    
    [Parameter(Mandatory=$false)]
    [switch]$VerboseLogging
)

# Global configuration
$global:OrchestratorConfig = @{
    Version = "1.0.0"
    EngineName = "SourceDigestionOrchestrator"
    OrchestrationMode = "Sequential"  # Sequential, Parallel, Hybrid
    EnableAllEngines = $true
    EngineExecutionOrder = @(
        "SourceDigestion",
        "ReverseEngineering", 
        "DeploymentAudit",
        "ManifestTracer",
        "ArchitectureEnhancement"
    )
    ResultAggregation = @{
        Enabled = $true
        MergeResults = $true
        GenerateUnifiedReport = $true
        CreateSummaryDashboard = $true
    }
    ParallelProcessing = @{
        Enabled = $true
        MaxConcurrentEngines = 3
        ThreadPoolSize = 16
    }
    ErrorHandling = @{
        ContinueOnError = $true
        RetryFailedEngines = $true
        MaxRetryAttempts = 3
        LogErrors = $true
    }
    Integration = @{
        ShareDataBetweenEngines = $true
        PassResultsToNextEngine = $true
        MaintainState = $true
    }
    ManifestTracer = @{
        Enabled = $true
        AutoDiscover = $true
        SearchPath = $null
        IncludePatterns = @('*manifest*', '*config*', '*settings*', '*orchestrator*')
        ExcludePatterns = @('node_modules', '.git', 'bin', 'obj', 'packages')
        BuildDependencyGraph = $true
        ValidateManifests = $true
        GenerateReport = $true
        ReportFormat = "HTML"
    }
    ArchitectureEnhancement = @{
        Enabled = $true
        AutoLocate = $true
        SelfHeal = $true
        FullAutomation = $true
        OutputSubdir = "architecture_enhancements"
        Configuration = @{}
    }
}

# Unified results storage
$global:UnifiedResults = @{
    SourceDigestion = $null
    ReverseEngineering = $null
    DeploymentAudit = $null
    ManifestTracer = $null
    ArchitectureEnhancement = $null
    AggregatedResults = @{
        TotalFiles = 0
        TotalIssues = 0
        TotalVulnerabilities = 0
        TotalBottlenecks = 0
        TotalDependencies = 0
        TotalManifests = 0
        TotalManifestIssues = 0
        TotalEnhancements = 0
        TotalArtifacts = 0
        RiskScore = 0
        ReadinessScore = 0
    }
    Timeline = [List[hashtable]]::new()
    PerformanceMetrics = @{
        TotalDuration = 0
        EngineDurations = @{}
        MemoryUsage = 0
        CpuUsage = 0
    }
    Recommendations = [List[string]]::new()
    ActionItems = [List[hashtable]]::new()
}

# Initialize logging
function Initialize-Logging {
    param($LogPath)
    
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $global:LogFile = Join-Path $LogPath "orchestrator_$timestamp.log"
    $global:EmergencyLog = Join-Path $LogPath "emergency_orchestrator.log"
    
    # Create log directory
    if (!(Test-Path $LogPath)) {
        New-Item -ItemType Directory -Path $LogPath -Force | Out-Null
    }
    
    Write-Log "INFO" "Source Digestion Orchestrator v$($global:OrchestratorConfig.Version) initialized"
    Write-Log "INFO" "Log file: $global:LogFile"
}

# Enhanced logging function
function Write-Log {
    param(
        [string]$Level = "INFO",
        [string]$Message,
        [string]$Component = "Main",
        [switch]$ConsoleOnly
    )
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    $logEntry = "[$timestamp] [$Level] [$Component] $Message"
    
    # Console output with colors
    $color = switch ($Level) {
        "ERROR" { "Red" }
        "WARNING" { "Yellow" }
        "SUCCESS" { "Green" }
        "INFO" { "Cyan" }
        "DEBUG" { "Gray" }
        default { "White" }
    }
    
    Write-Host $logEntry -ForegroundColor $color
    
    # File logging
    if ($global:LogFile -and !$ConsoleOnly) {
        Add-Content -Path $global:LogFile -Value $logEntry -Encoding UTF8
    }
}

# Load configuration
function Load-Configuration {
    param($ConfigPath)
    
    if (Test-Path $ConfigPath) {
        try {
            $configContent = Get-Content $ConfigPath -Raw | ConvertFrom-Json
            
            # Merge configurations
            foreach ($key in $configContent.PSObject.Properties.Name) {
                if ($global:OrchestratorConfig.ContainsKey($key)) {
                    $value = $configContent.$key
                    if ($value -is [System.Management.Automation.PSCustomObject]) {
                        foreach ($subKey in $value.PSObject.Properties.Name) {
                            $global:OrchestratorConfig[$key][$subKey] = $value.$subKey
                        }
                    }
                    else {
                        $global:OrchestratorConfig[$key] = $value
                    }
                }
            }
            
            Write-Log "INFO" "Configuration loaded from $ConfigPath"
        }
        catch {
            Write-Log "WARNING" "Failed to load configuration: $_"
        }
    }
}

# Save configuration
function Save-Configuration {
    param($ConfigPath)
    
    try {
        $global:OrchestratorConfig | ConvertTo-Json -Depth 10 | Out-File $ConfigPath -Encoding UTF8
        Write-Log "INFO" "Configuration saved to $ConfigPath"
    }
    catch {
        Write-Log "ERROR" "Failed to save configuration: $_"
    }
}

# Main orchestration function
function Start-Orchestration {
    param(
        [string]$SourcePath,
        [string]$OutputPath
    )
    
    Write-Log "INFO" "Starting source digestion orchestration"
    Write-Log "INFO" "Source: $SourcePath"
    Write-Log "INFO" "Output: $OutputPath"
    
    # Validate source path
    if (!(Test-Path $SourcePath)) {
        Write-Log "ERROR" "Source path does not exist: $SourcePath"
        return $false
    }
    
    # Create output directory
    if (!(Test-Path $OutputPath)) {
        New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
        Write-Log "INFO" "Created output directory: $OutputPath"
    }
    
    # Initialize logging
    Initialize-Logging -LogPath (Join-Path $OutputPath "logs")
    
    $startTime = Get-Date
    
    # Record start event
    $global:UnifiedResults.Timeline.Add(@{
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
        Event = "OrchestrationStarted"
        SourcePath = $SourcePath
        OutputPath = $OutputPath
    })
    
    # Determine which engines to run
    $enginesToRun = @()
    
    if ($EnableAllEngines -or $global:OrchestratorConfig.EnableAllEngines) {
        $enginesToRun = @($global:OrchestratorConfig.EngineExecutionOrder)
    }
    else {
        if ($EnableSourceDigestion) { $enginesToRun += "SourceDigestion" }
        if ($EnableReverseEngineering) { $enginesToRun += "ReverseEngineering" }
        if ($EnableDeploymentAudit) { $enginesToRun += "DeploymentAudit" }
        if ($EnableManifestTracer -or $global:OrchestratorConfig.ManifestTracer.Enabled) { $enginesToRun += "ManifestTracer" }
        if ($EnableArchitectureEnhancement -or $global:OrchestratorConfig.ArchitectureEnhancement.Enabled) { $enginesToRun += "ArchitectureEnhancement" }
    }

    if ($global:OrchestratorConfig.EngineExecutionOrder) {
        $enginesToRun = $global:OrchestratorConfig.EngineExecutionOrder | Where-Object { $_ -in $enginesToRun }
    }

    $enginesToRun = $enginesToRun | Select-Object -Unique
    
    if ($enginesToRun.Count -eq 0) {
        Write-Log "ERROR" "No engines selected to run"
        return $false
    }
    
    Write-Log "INFO" "Engines to run: $($enginesToRun -join ', ')"
    
    # Execute engines based on orchestration mode
    switch ($global:OrchestratorConfig.OrchestrationMode) {
        "Sequential" {
            $success = Start-SequentialExecution -Engines $enginesToRun -SourcePath $SourcePath -OutputPath $OutputPath
        }
        "Parallel" {
            $success = Start-ParallelExecution -Engines $enginesToRun -SourcePath $SourcePath -OutputPath $OutputPath
        }
        "Hybrid" {
            $success = Start-HybridExecution -Engines $enginesToRun -SourcePath $SourcePath -OutputPath $OutputPath
        }
        default {
            $success = Start-SequentialExecution -Engines $enginesToRun -SourcePath $SourcePath -OutputPath $OutputPath
        }
    }
    
    if (!$success) {
        Write-Log "ERROR" "Orchestration failed"
        return $false
    }
    
    # Aggregate results
    if ($global:OrchestratorConfig.ResultAggregation.Enabled) {
        Write-Log "INFO" "Aggregating results from all engines"
        Aggregate-Results
    }
    
    # Generate comprehensive report
    if ($GenerateComprehensiveReport -or $global:OrchestratorConfig.ResultAggregation.GenerateUnifiedReport) {
        Write-Log "INFO" "Generating comprehensive report"
        $reportPath = Generate-ComprehensiveReport -OutputPath $OutputPath -Format $ReportFormat
        Write-Log "SUCCESS" "Comprehensive report generated: $reportPath"
    }
    
    # Generate summary dashboard
    if ($global:OrchestratorConfig.ResultAggregation.CreateSummaryDashboard) {
        Write-Log "INFO" "Creating summary dashboard"
        $dashboardPath = Generate-SummaryDashboard -OutputPath $OutputPath
        Write-Log "SUCCESS" "Summary dashboard created: $dashboardPath"
    }
    
    # Record completion event
    $endTime = Get-Date
    $global:UnifiedResults.Timeline.Add(@{
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
        Event = "OrchestrationCompleted"
        Duration = ($endTime - $startTime).TotalSeconds
        Success = $true
    })
    
    # Update performance metrics
    $global:UnifiedResults.PerformanceMetrics.TotalDuration = ($endTime - $startTime).TotalSeconds
    
    Write-Log "SUCCESS" "Source digestion orchestration completed successfully"
    Write-Log "INFO" "Total duration: $($global:UnifiedResults.PerformanceMetrics.TotalDuration) seconds"
    Write-Log "INFO" "Files processed: $($global:UnifiedResults.AggregatedResults.TotalFiles)"
    Write-Log "INFO" "Total issues found: $($global:UnifiedResults.AggregatedResults.TotalIssues)"
    Write-Log "INFO" "Overall risk score: $($global:UnifiedResults.AggregatedResults.RiskScore)"
    
    return $true
}

# Sequential execution
function Start-SequentialExecution {
    param(
        [string[]]$Engines,
        [string]$SourcePath,
        [string]$OutputPath
    )
    
    Write-Log "INFO" "Starting sequential execution of engines"
    
    foreach ($engine in $Engines) {
        $engineStartTime = Get-Date
        
        Write-Log "INFO" "Starting $engine engine"
        
        # Record engine start event
        $global:UnifiedResults.Timeline.Add(@{
            Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
            Event = "EngineStarted"
            Engine = $engine
        })
        
        try {
            switch ($engine) {
                "SourceDigestion" {
                    $enginePath = Join-Path $PSScriptRoot "SourceDigestionEngine.ps1"
                    $engineOutput = Join-Path $OutputPath "source_digestion"
                    
                    $params = @{
                        SourcePath = $SourcePath
                        OutputPath = $engineOutput
                        ConfigPath = Join-Path $PSScriptRoot "digestion_config.json"
                        EnableReverseEngineering = $false
                        EnableDeploymentAudit = $false
                        GenerateReport = $true
                        ReportFormat = "JSON"
                    }
                    
                    & $enginePath @params
                    
                    # Load results
                    $resultsFile = Join-Path $engineOutput "analysis_results.json"
                    if (Test-Path $resultsFile) {
                        $global:UnifiedResults.SourceDigestion = Get-Content $resultsFile -Raw | ConvertFrom-Json
                    }
                }
                
                "ReverseEngineering" {
                    $enginePath = Join-Path $PSScriptRoot "ReverseEngineeringEngine.ps1"
                    $engineOutput = Join-Path $OutputPath "reverse_engineering"
                    
                    $params = @{
                        TargetPath = $SourcePath
                        OutputPath = $engineOutput
                        ConfigPath = Join-Path $PSScriptRoot "reverse_engineering_config.json"
                        AnalyzeBinaries = $true
                        ExtractAPIs = $true
                        MapDependencies = $true
                        DetectVulnerabilities = $true
                        IdentifyBottlenecks = $true
                        ReconstructStructure = $true
                    }
                    
                    & $enginePath @params
                    
                    # Load results
                    $resultsFile = Join-Path $engineOutput "reverse_engineering_results.json"
                    if (Test-Path $resultsFile) {
                        $global:UnifiedResults.ReverseEngineering = Get-Content $resultsFile -Raw | ConvertFrom-Json
                    }
                }
                
                "DeploymentAudit" {
                    $enginePath = Join-Path $PSScriptRoot "DeploymentAuditEngine.ps1"
                    $engineOutput = Join-Path $OutputPath "deployment_audit"
                    
                    $params = @{
                        ProjectPath = $SourcePath
                        OutputPath = $engineOutput
                        ConfigPath = Join-Path $PSScriptRoot "deployment_audit_config.json"
                        ValidateConfiguration = $true
                        CheckSecurity = $true
                        VerifyDependencies = $true
                        CheckCompliance = $true
                        GenerateReadinessReport = $true
                    }
                    
                    & $enginePath @params
                    
                    # Load results
                    $resultsFile = Join-Path $engineOutput "deployment_audit_results.json"
                    if (Test-Path $resultsFile) {
                        $global:UnifiedResults.DeploymentAudit = Get-Content $resultsFile -Raw | ConvertFrom-Json
                    }
                }

                "ManifestTracer" {
                    $enginePath = Join-Path $PSScriptRoot "ManifestTracer.psm1"
                    $engineOutput = Join-Path $OutputPath "manifest_tracer"

                    if (!(Test-Path $enginePath)) {
                        throw "ManifestTracer module not found: $enginePath"
                    }

                    $manifestConfig = $global:OrchestratorConfig.ManifestTracer
                    $searchPath = if ($manifestConfig.SearchPath) { $manifestConfig.SearchPath } else { $SourcePath }

                    Import-Module $enginePath -Force

                    $pipelineResult = Start-ManifestTracerPipeline -SearchPath $searchPath -OutputPath $engineOutput `
                        -BuildDependencyGraph:$manifestConfig.BuildDependencyGraph `
                        -ValidateManifests:$manifestConfig.ValidateManifests `
                        -GenerateReport:$manifestConfig.GenerateReport `
                        -ReportFormat $manifestConfig.ReportFormat

                    if ($pipelineResult) {
                        $manifestModule = Get-Module -Name "ManifestTracer"
                        if ($manifestModule) {
                            $registry = $manifestModule.SessionState.PSVariable.GetValue("ManifestRegistry")
                            $global:UnifiedResults.ManifestTracer = $registry
                        }
                    }
                }

                "ArchitectureEnhancement" {
                    $enginePath = Join-Path $PSScriptRoot "ArchitectureEnhancementEngine.psm1"
                    $engineOutputRoot = Join-Path $OutputPath "architecture_enhancement"

                    if (!(Test-Path $enginePath)) {
                        throw "ArchitectureEnhancementEngine module not found: $enginePath"
                    }

                    $archConfig = $global:OrchestratorConfig.ArchitectureEnhancement
                    $engineOutput = Join-Path $engineOutputRoot $archConfig.OutputSubdir

                    Import-Module $enginePath -Force

                    $pipelineResult = Start-ArchitectureEnhancementPipeline -SourcePath $SourcePath -OutputPath $engineOutput `
                        -Configuration $archConfig.Configuration `
                        -AutoLocate:$archConfig.AutoLocate `
                        -SelfHeal:$archConfig.SelfHeal `
                        -FullAutomation:$archConfig.FullAutomation

                    if ($pipelineResult) {
                        $archModule = Get-Module -Name "ArchitectureEnhancementEngine"
                        if ($archModule) {
                            $registry = $archModule.SessionState.PSVariable.GetValue("EnhancementRegistry")
                            $global:UnifiedResults.ArchitectureEnhancement = $registry
                        }
                    }
                }
            }
            
            $engineEndTime = Get-Date
            $engineDuration = ($engineEndTime - $engineStartTime).TotalSeconds
            
            # Record engine completion event
            $global:UnifiedResults.Timeline.Add(@{
                Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
                Event = "EngineCompleted"
                Engine = $engine
                Duration = $engineDuration
                Success = $true
            })
            
            $global:UnifiedResults.PerformanceMetrics.EngineDurations[$engine] = $engineDuration
            
            Write-Log "SUCCESS" "$engine engine completed in $engineDuration seconds"
        }
        catch {
            $engineEndTime = Get-Date
            $engineDuration = ($engineEndTime - $engineStartTime).TotalSeconds
            
            # Record engine failure event
            $global:UnifiedResults.Timeline.Add(@{
                Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
                Event = "EngineFailed"
                Engine = $engine
                Duration = $engineDuration
                Error = $_.Exception.Message
            })
            
            Write-Log "ERROR" "$engine engine failed: $_"
            
            if (!$global:OrchestratorConfig.ErrorHandling.ContinueOnError) {
                return $false
            }
        }
    }
    
    return $true
}

# Parallel execution
function Start-ParallelExecution {
    param(
        [string[]]$Engines,
        [string]$SourcePath,
        [string]$OutputPath
    )
    
    Write-Log "INFO" "Starting parallel execution of engines"
    
    $jobs = @()
    
    foreach ($engine in $Engines) {
        Write-Log "INFO" "Starting $engine engine in parallel"
        
        # Record engine start event
        $global:UnifiedResults.Timeline.Add(@{
            Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
            Event = "EngineStarted"
            Engine = $engine
            Mode = "Parallel"
        })
        
        $job = Start-ThreadJob -ScriptBlock {
            param($engine, $sourcePath, $outputPath, $scriptRoot, $orchestratorConfig)
            
            $engineStartTime = Get-Date
            $result = @{
                Engine = $engine
                Success = $false
                Duration = 0
                Error = $null
                Results = $null
            }
            
            try {
                switch ($engine) {
                    "SourceDigestion" {
                        $enginePath = Join-Path $scriptRoot "SourceDigestionEngine.ps1"
                        $engineOutput = Join-Path $outputPath "source_digestion"
                        
                        $params = @{
                            SourcePath = $sourcePath
                            OutputPath = $engineOutput
                            ConfigPath = Join-Path $scriptRoot "digestion_config.json"
                            EnableReverseEngineering = $false
                            EnableDeploymentAudit = $false
                            GenerateReport = $true
                            ReportFormat = "JSON"
                        }
                        
                        & $enginePath @params
                        
                        # Load results
                        $resultsFile = Join-Path $engineOutput "analysis_results.json"
                        if (Test-Path $resultsFile) {
                            $result.Results = Get-Content $resultsFile -Raw | ConvertFrom-Json
                        }
                    }
                    
                    "ReverseEngineering" {
                        $enginePath = Join-Path $scriptRoot "ReverseEngineeringEngine.ps1"
                        $engineOutput = Join-Path $outputPath "reverse_engineering"
                        
                        $params = @{
                            TargetPath = $sourcePath
                            OutputPath = $engineOutput
                            ConfigPath = Join-Path $scriptRoot "reverse_engineering_config.json"
                            AnalyzeBinaries = $true
                            ExtractAPIs = $true
                            MapDependencies = $true
                            DetectVulnerabilities = $true
                            IdentifyBottlenecks = $true
                            ReconstructStructure = $true
                        }
                        
                        & $enginePath @params
                        
                        # Load results
                        $resultsFile = Join-Path $engineOutput "reverse_engineering_results.json"
                        if (Test-Path $resultsFile) {
                            $result.Results = Get-Content $resultsFile -Raw | ConvertFrom-Json
                        }
                    }
                    
                    "DeploymentAudit" {
                        $enginePath = Join-Path $scriptRoot "DeploymentAuditEngine.ps1"
                        $engineOutput = Join-Path $outputPath "deployment_audit"
                        
                        $params = @{
                            ProjectPath = $sourcePath
                            OutputPath = $engineOutput
                            ConfigPath = Join-Path $scriptRoot "deployment_audit_config.json"
                            ValidateConfiguration = $true
                            CheckSecurity = $true
                            VerifyDependencies = $true
                            CheckCompliance = $true
                            GenerateReadinessReport = $true
                        }
                        
                        & $enginePath @params
                        
                        # Load results
                        $resultsFile = Join-Path $engineOutput "deployment_audit_results.json"
                        if (Test-Path $resultsFile) {
                            $result.Results = Get-Content $resultsFile -Raw | ConvertFrom-Json
                        }
                    }

                    "ManifestTracer" {
                        $enginePath = Join-Path $scriptRoot "ManifestTracer.psm1"
                        $engineOutput = Join-Path $outputPath "manifest_tracer"

                        if (!(Test-Path $enginePath)) {
                            throw "ManifestTracer module not found: $enginePath"
                        }

                        $manifestConfig = $orchestratorConfig.ManifestTracer
                        $searchPath = if ($manifestConfig.SearchPath) { $manifestConfig.SearchPath } else { $sourcePath }

                        Import-Module $enginePath -Force

                        $pipelineResult = Start-ManifestTracerPipeline -SearchPath $searchPath -OutputPath $engineOutput `
                            -BuildDependencyGraph:$manifestConfig.BuildDependencyGraph `
                            -ValidateManifests:$manifestConfig.ValidateManifests `
                            -GenerateReport:$manifestConfig.GenerateReport `
                            -ReportFormat $manifestConfig.ReportFormat

                        if ($pipelineResult) {
                            $manifestModule = Get-Module -Name "ManifestTracer"
                            if ($manifestModule) {
                                $result.Results = $manifestModule.SessionState.PSVariable.GetValue("ManifestRegistry")
                            }
                        }
                    }

                    "ArchitectureEnhancement" {
                        $enginePath = Join-Path $scriptRoot "ArchitectureEnhancementEngine.psm1"
                        $engineOutputRoot = Join-Path $outputPath "architecture_enhancement"

                        if (!(Test-Path $enginePath)) {
                            throw "ArchitectureEnhancementEngine module not found: $enginePath"
                        }

                        $archConfig = $orchestratorConfig.ArchitectureEnhancement
                        $engineOutput = Join-Path $engineOutputRoot $archConfig.OutputSubdir

                        Import-Module $enginePath -Force

                        $pipelineResult = Start-ArchitectureEnhancementPipeline -SourcePath $sourcePath -OutputPath $engineOutput `
                            -Configuration $archConfig.Configuration `
                            -AutoLocate:$archConfig.AutoLocate `
                            -SelfHeal:$archConfig.SelfHeal `
                            -FullAutomation:$archConfig.FullAutomation

                        if ($pipelineResult) {
                            $archModule = Get-Module -Name "ArchitectureEnhancementEngine"
                            if ($archModule) {
                                $result.Results = $archModule.SessionState.PSVariable.GetValue("EnhancementRegistry")
                            }
                        }
                    }
                }
                
                $result.Success = $true
            }
            catch {
                $result.Error = $_.Exception.Message
                $result.Success = $false
            }
            
            $engineEndTime = Get-Date
            $result.Duration = ($engineEndTime - $engineStartTime).TotalSeconds
            
            return $result
        } -ArgumentList $engine, $SourcePath, $OutputPath, $PSScriptRoot, $global:OrchestratorConfig
        
        $jobs += $job
    }
    
    # Wait for all jobs to complete
    $results = Wait-Job $jobs | Receive-Job
    
    # Process results
    foreach ($result in $results) {
        # Record engine completion event
        $global:UnifiedResults.Timeline.Add(@{
            Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
            Event = if ($result.Success) { "EngineCompleted" } else { "EngineFailed" }
            Engine = $result.Engine
            Duration = $result.Duration
            Mode = "Parallel"
            Error = $result.Error
        })
        
        $global:UnifiedResults.PerformanceMetrics.EngineDurations[$result.Engine] = $result.Duration
        
        if ($result.Success) {
            Write-Log "SUCCESS" "$($result.Engine) engine completed in $($result.Duration) seconds"
            
            # Store results
            switch ($result.Engine) {
                "SourceDigestion" { $global:UnifiedResults.SourceDigestion = $result.Results }
                "ReverseEngineering" { $global:UnifiedResults.ReverseEngineering = $result.Results }
                "DeploymentAudit" { $global:UnifiedResults.DeploymentAudit = $result.Results }
                "ManifestTracer" { $global:UnifiedResults.ManifestTracer = $result.Results }
                "ArchitectureEnhancement" { $global:UnifiedResults.ArchitectureEnhancement = $result.Results }
            }
        }
        else {
            Write-Log "ERROR" "$($result.Engine) engine failed: $($result.Error)"
            
            if (!$global:OrchestratorConfig.ErrorHandling.ContinueOnError) {
                Remove-Job $jobs -Force
                return $false
            }
        }
    }
    
    # Clean up jobs
    Remove-Job $jobs -Force
    
    return $true
}

# Hybrid execution (parallel for independent engines, sequential for dependent ones)
function Start-HybridExecution {
    param(
        [string[]]$Engines,
        [string]$SourcePath,
        [string]$OutputPath
    )
    
    Write-Log "INFO" "Starting hybrid execution of engines"
    
    # Phase 1: Run independent engines in parallel
    $independentEngines = @("SourceDigestion", "ReverseEngineering", "ManifestTracer", "ArchitectureEnhancement")
    $enginesToRunParallel = $Engines | Where-Object { $_ -in $independentEngines }
    
    if ($enginesToRunParallel.Count -gt 0) {
        Write-Log "INFO" "Phase 1: Running independent engines in parallel"
        $parallelSuccess = Start-ParallelExecution -Engines $enginesToRunParallel -SourcePath $SourcePath -OutputPath $OutputPath
        
        if (!$parallelSuccess -and !$global:OrchestratorConfig.ErrorHandling.ContinueOnError) {
            return $false
        }
    }
    
    # Phase 2: Run dependent engines sequentially
    $dependentEngines = @("DeploymentAudit")
    $enginesToRunSequential = $Engines | Where-Object { $_ -in $dependentEngines }
    
    if ($enginesToRunSequential.Count -gt 0) {
        Write-Log "INFO" "Phase 2: Running dependent engines sequentially"
        $sequentialSuccess = Start-SequentialExecution -Engines $enginesToRunSequential -SourcePath $SourcePath -OutputPath $OutputPath
        
        if (!$sequentialSuccess) {
            return $false
        }
    }
    
    return $true
}

# Results aggregation
function Aggregate-Results {
    Write-Log "INFO" "Aggregating results from all engines"
    
    $aggregated = $global:UnifiedResults.AggregatedResults
    
    # Aggregate Source Digestion results
    if ($global:UnifiedResults.SourceDigestion) {
        $sd = $global:UnifiedResults.SourceDigestion
        
        if ($sd.SourceFiles) {
            $aggregated.TotalFiles += $sd.SourceFiles.Count
        }
        
        if ($sd.SecurityIssues) {
            $aggregated.TotalVulnerabilities += $sd.SecurityIssues.Count
        }
        
        if ($sd.Dependencies) {
            $aggregated.TotalDependencies += $sd.Dependencies.Count
        }
        
        # Calculate risk score from source digestion
        if ($sd.AnalysisResults) {
            foreach ($result in $sd.AnalysisResults) {
                if ($result.SecurityIssues) {
                    foreach ($issue in $result.SecurityIssues) {
                        $aggregated.RiskScore += Get-RiskScore -Severity $issue.Severity
                    }
                }
            }
        }
    }
    
    # Aggregate Reverse Engineering results
    if ($global:UnifiedResults.ReverseEngineering) {
        $re = $global:UnifiedResults.ReverseEngineering
        
        if ($re.Statistics) {
            $aggregated.TotalFiles += $re.Statistics.TotalFiles
            $aggregated.TotalVulnerabilities += $re.Statistics.TotalVulnerabilities
            $aggregated.TotalBottlenecks += $re.Statistics.TotalBottlenecks
            $aggregated.TotalDependencies += $re.Statistics.TotalFunctions
        }
        
        # Add risk from vulnerabilities
        if ($re.Vulnerabilities) {
            foreach ($vuln in $re.Vulnerabilities) {
                if ($vuln.Issues) {
                    foreach ($issue in $vuln.Issues) {
                        $aggregated.RiskScore += Get-RiskScore -Severity $issue.Severity
                    }
                }
            }
        }
    }
    
    # Aggregate Deployment Audit results
    if ($global:UnifiedResults.DeploymentAudit) {
        $da = $global:UnifiedResults.DeploymentAudit
        
        if ($da.RiskAssessment) {
            $aggregated.RiskScore += $da.RiskAssessment.RiskScore
        }
        
        if ($da.ConfigurationIssues) {
            $aggregated.TotalIssues += $da.ConfigurationIssues.Count
        }
        
        if ($da.SecurityGaps) {
            $aggregated.TotalIssues += $da.SecurityGaps.Count
        }
        
        if ($da.MissingDependencies) {
            $aggregated.TotalIssues += $da.MissingDependencies.Count
        }
    }

    # Aggregate Manifest Tracer results
    if ($global:UnifiedResults.ManifestTracer) {
        $mt = $global:UnifiedResults.ManifestTracer

        if ($mt.DiscoveredManifests) {
            $aggregated.TotalManifests += $mt.DiscoveredManifests.Count
        }

        if ($mt.ValidationResults) {
            foreach ($validation in $mt.ValidationResults) {
                $aggregated.TotalManifestIssues += $validation.Errors.Count
                $aggregated.TotalIssues += $validation.Errors.Count
            }
        }
    }

    # Aggregate Architecture Enhancement results
    if ($global:UnifiedResults.ArchitectureEnhancement) {
        $ae = $global:UnifiedResults.ArchitectureEnhancement

        if ($ae.EnhancementPlan -and $ae.EnhancementPlan.Enhancements) {
            $aggregated.TotalEnhancements += $ae.EnhancementPlan.Enhancements.Count
        }

        if ($ae.GeneratedArtifacts) {
            $aggregated.TotalArtifacts += $ae.GeneratedArtifacts.Count
        }
    }
    
    # Generate recommendations based on aggregated results
    Generate-Recommendations
    
    Write-Log "SUCCESS" "Results aggregation completed"
    Write-Log "INFO" "Total files: $($aggregated.TotalFiles)"
    Write-Log "INFO" "Total issues: $($aggregated.TotalIssues)"
    Write-Log "INFO" "Total vulnerabilities: $($aggregated.TotalVulnerabilities)"
    Write-Log "INFO" "Total bottlenecks: $($aggregated.TotalBottlenecks)"
    Write-Log "INFO" "Overall risk score: $($aggregated.RiskScore)"
}

# Generate recommendations
function Generate-Recommendations {
    $recommendations = $global:UnifiedResults.Recommendations
    $actionItems = $global:UnifiedResults.ActionItems
    
    $aggregated = $global:UnifiedResults.AggregatedResults
    
    # High-priority recommendations
    if ($aggregated.TotalVulnerabilities -gt 0) {
        $recommendations.Add("Address $aggregated.TotalVulnerabilities security vulnerabilities immediately")
        $actionItems.Add(@{
            Priority = "Critical"
            Category = "Security"
            Description = "Fix security vulnerabilities"
            EstimatedEffort = "High"
            Dependencies = @()
        })
    }
    
    if ($aggregated.RiskScore -gt 50) {
        $recommendations.Add("Overall risk score is critical. Immediate action required.")
        $actionItems.Add(@{
            Priority = "Critical"
            Category = "RiskManagement"
            Description = "Reduce overall risk score below 50"
            EstimatedEffort = "High"
            Dependencies = @()
        })
    }
    
    # Medium-priority recommendations
    if ($aggregated.TotalBottlenecks -gt 0) {
        $recommendations.Add("Optimize $aggregated.TotalBottlenecks performance bottlenecks")
        $actionItems.Add(@{
            Priority = "High"
            Category = "Performance"
            Description = "Address performance bottlenecks"
            EstimatedEffort = "Medium"
            Dependencies = @()
        })
    }
    
    if ($aggregated.TotalDependencies -gt 100) {
        $recommendations.Add("Review and optimize dependency structure (currently $aggregated.TotalDependencies dependencies)")
        $actionItems.Add(@{
            Priority = "Medium"
            Category = "Architecture"
            Description = "Optimize dependency structure"
            EstimatedEffort = "Medium"
            Dependencies = @()
        })
    }
    
    # Low-priority recommendations
    if ($aggregated.TotalFiles -gt 1000) {
        $recommendations.Add("Consider modularizing large codebase ($aggregated.TotalFiles files)")
        $actionItems.Add(@{
            Priority = "Low"
            Category = "Architecture"
            Description = "Modularize large codebase"
            EstimatedEffort = "High"
            Dependencies = @()
        })
    }
    
    Write-Log "INFO" "Generated $($recommendations.Count) recommendations and $($actionItems.Count) action items"
}

# Risk score calculation
function Get-RiskScore {
    param($Severity)
    
    switch ($Severity) {
        "Critical" { return 10 }
        "High" { return 7 }
        "Medium" { return 4 }
        "Low" { return 1 }
        default { return 0 }
    }
}

# Comprehensive report generation
function Generate-ComprehensiveReport {
    param(
        [string]$OutputPath,
        [string]$Format = "HTML"
    )
    
    $reportPath = Join-Path $OutputPath "comprehensive_report.$($Format.ToLower())"
    
    Write-Log "INFO" "Generating comprehensive report in $Format format"
    
    switch ($Format.ToUpper()) {
        "HTML" {
            $report = Generate-HTMLReport
            $report | Out-File $reportPath -Encoding UTF8
        }
        "JSON" {
            # Trim oversized fields to prevent freeze
            $trimmedResults = @{
                AggregatedResults = $global:UnifiedResults.AggregatedResults
                Timeline = $global:UnifiedResults.Timeline | Select-Object -First 100
                PerformanceMetrics = $global:UnifiedResults.PerformanceMetrics
                Recommendations = $global:UnifiedResults.Recommendations | Select-Object -First 50
                ActionItems = $global:UnifiedResults.ActionItems | Select-Object -First 50
            }
            
            # Write per-engine artifacts separately to avoid huge nested serialization
            if ($global:UnifiedResults.SourceDigestion) {
                try {
                    $enginePath = Join-Path $OutputPath "source_digestion_artifact.json"
                    $global:UnifiedResults.SourceDigestion | ConvertTo-Json -Depth 5 -Compress | Out-File $enginePath -Encoding UTF8
                    $trimmedResults.SourceDigestion = "[See source_digestion_artifact.json]"
                } catch {
                    $trimmedResults.SourceDigestion = "[Serialization error: $($_.Exception.Message)]"
                }
            }
            
            if ($global:UnifiedResults.ReverseEngineering) {
                try {
                    $enginePath = Join-Path $OutputPath "reverse_engineering_artifact.json"
                    $global:UnifiedResults.ReverseEngineering | ConvertTo-Json -Depth 5 -Compress | Out-File $enginePath -Encoding UTF8
                    $trimmedResults.ReverseEngineering = "[See reverse_engineering_artifact.json]"
                } catch {
                    $trimmedResults.ReverseEngineering = "[Serialization error: $($_.Exception.Message)]"
                }
            }
            
            if ($global:UnifiedResults.DeploymentAudit) {
                try {
                    $enginePath = Join-Path $OutputPath "deployment_audit_artifact.json"
                    $global:UnifiedResults.DeploymentAudit | ConvertTo-Json -Depth 5 -Compress | Out-File $enginePath -Encoding UTF8
                    $trimmedResults.DeploymentAudit = "[See deployment_audit_artifact.json]"
                } catch {
                    $trimmedResults.DeploymentAudit = "[Serialization error: $($_.Exception.Message)]"
                }
            }
            
            if ($global:UnifiedResults.ManifestTracer) {
                try {
                    $enginePath = Join-Path $OutputPath "manifest_tracer_artifact.json"
                    $manifestSummary = @{
                        TotalManifests = if ($global:UnifiedResults.ManifestTracer.TotalParsed) { $global:UnifiedResults.ManifestTracer.TotalParsed } else { 0 }
                        ValidationErrors = if ($global:UnifiedResults.ManifestTracer.ValidationErrors) { $global:UnifiedResults.ManifestTracer.ValidationErrors.Count } else { 0 }
                    }
                    $manifestSummary | ConvertTo-Json -Compress | Out-File $enginePath -Encoding UTF8
                    $trimmedResults.ManifestTracer = "[See manifest_tracer_artifact.json]"
                } catch {
                    $trimmedResults.ManifestTracer = "[Serialization error: $($_.Exception.Message)]"
                }
            }
            
            if ($global:UnifiedResults.ArchitectureEnhancement) {
                try {
                    $enginePath = Join-Path $OutputPath "architecture_enhancement_artifact.json"
                    $archSummary = @{
                        TotalEnhancements = if ($global:UnifiedResults.ArchitectureEnhancement.TotalEnhancements) { $global:UnifiedResults.ArchitectureEnhancement.TotalEnhancements } else { 0 }
                    }
                    $archSummary | ConvertTo-Json -Compress | Out-File $enginePath -Encoding UTF8
                    $trimmedResults.ArchitectureEnhancement = "[See architecture_enhancement_artifact.json]"
                } catch {
                    $trimmedResults.ArchitectureEnhancement = "[Serialization error: $($_.Exception.Message)]"
                }
            }
            
            # Write the main trimmed report
            try {
                $report = $trimmedResults | ConvertTo-Json -Depth 10 -Compress
                $report | Out-File $reportPath -Encoding UTF8
            } catch {
                Write-Log "ERROR" "JSON serialization failed: $($_.Exception.Message)"
                # Fallback: write minimal summary
                @{
                    Error = "Report generation failed: $($_.Exception.Message)"
                    AggregatedResults = $global:UnifiedResults.AggregatedResults
                } | ConvertTo-Json -Compress | Out-File $reportPath -Encoding UTF8
            }
        }
        "MARKDOWN" {
            $report = Generate-MarkdownReport
            $report | Out-File $reportPath -Encoding UTF8
        }
        default {
            Write-Log "WARNING" "Unsupported report format: $Format"
            return $null
        }
    }
    
    Write-Log "SUCCESS" "Comprehensive report generated: $reportPath"
    
    return $reportPath
}

# HTML report generation
function Generate-HTMLReport {
    $html = @"
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Source Digestion Analysis Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }
        .header { background-color: #2c3e50; color: white; padding: 20px; border-radius: 5px; }
        .summary { background-color: white; padding: 20px; margin: 20px 0; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .metrics { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 20px 0; }
        .metric { background-color: white; padding: 15px; border-radius: 5px; text-align: center; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .metric-value { font-size: 2em; font-weight: bold; color: #2c3e50; }
        .metric-label { color: #7f8c8d; margin-top: 5px; }
        .section { background-color: white; padding: 20px; margin: 20px 0; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .section-title { color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 10px; margin-bottom: 15px; }
        .recommendation { background-color: #e8f4f8; padding: 10px; margin: 10px 0; border-left: 4px solid #3498db; border-radius: 3px; }
        .action-item { background-color: #fff3cd; padding: 10px; margin: 10px 0; border-left: 4px solid #ffc107; border-radius: 3px; }
        .timeline { background-color: white; padding: 20px; margin: 20px 0; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .timeline-item { padding: 10px; border-left: 3px solid #3498db; margin: 10px 0; }
        .timeline-time { color: #7f8c8d; font-size: 0.9em; }
        .timeline-event { font-weight: bold; color: #2c3e50; }
        .footer { text-align: center; color: #7f8c8d; margin-top: 40px; padding: 20px; }
        .risk-critical { color: #e74c3c; }
        .risk-high { color: #e67e22; }
        .risk-medium { color: #f39c12; }
        .risk-low { color: #27ae60; }
    </style>
</head>
<body>
    <div class="header">
        <h1>Source Digestion Analysis Report</h1>
        <p>Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")</p>
        <p>Version: $($global:OrchestratorConfig.Version)</p>
    </div>

    <div class="summary">
        <h2 class="section-title">Executive Summary</h2>
        <p>This comprehensive analysis report provides insights into the source code structure, security posture, and deployment readiness of the analyzed project.</p>
        
        <div class="metrics">
            <div class="metric">
                <div class="metric-value">$($global:UnifiedResults.AggregatedResults.TotalFiles)</div>
                <div class="metric-label">Files Analyzed</div>
            </div>
            <div class="metric">
                <div class="metric-value">$($global:UnifiedResults.AggregatedResults.TotalIssues)</div>
                <div class="metric-label">Total Issues</div>
            </div>
            <div class="metric">
                <div class="metric-value">$($global:UnifiedResults.AggregatedResults.TotalVulnerabilities)</div>
                <div class="metric-label">Vulnerabilities</div>
            </div>
            <div class="metric">
                <div class="metric-value">$($global:UnifiedResults.AggregatedResults.TotalBottlenecks)</div>
                <div class="metric-label">Performance Bottlenecks</div>
            </div>
            <div class="metric">
                <div class="metric-value">$($global:UnifiedResults.AggregatedResults.TotalDependencies)</div>
                <div class="metric-label">Dependencies</div>
            </div>
            <div class="metric">
                <div class="metric-value class="risk-$(Get-RiskLevel -Score $global:UnifiedResults.AggregatedResults.RiskScore).ToLower()">$($global:UnifiedResults.AggregatedResults.RiskScore)</div>
                <div class="metric-label">Overall Risk Score</div>
            </div>
        </div>
    </div>

    <div class="section">
        <h2 class="section-title">Recommendations</h2>
"@
    
    foreach ($recommendation in $global:UnifiedResults.Recommendations) {
        $html += @"
        <div class="recommendation">$recommendation</div>
"@
    }
    
    $html += @"
    </div>

    <div class="section">
        <h2 class="section-title">Action Items</h2>
"@
    
    foreach ($item in $global:UnifiedResults.ActionItems) {
        $html += @"
        <div class="action-item">
            <strong>[$($item.Priority)] $($item.Category)</strong>: $($item.Description)
            <br><em>Estimated Effort: $($item.EstimatedEffort)</em>
        </div>
"@
    }
    
    $html += @"
    </div>

    <div class="timeline">
        <h2 class="section-title">Analysis Timeline</h2>
"@
    
    foreach ($event in $global:UnifiedResults.Timeline) {
        $html += @"
        <div class="timeline-item">
            <div class="timeline-time">$($event.Timestamp)</div>
            <div class="timeline-event">$($event.Event)</div>
"@
        if ($event.Engine) {
            $html += @"
            <div>Engine: $($event.Engine)</div>
"@
        }
        if ($event.Duration) {
            $html += @"
            <div>Duration: $($event.Duration) seconds</div>
"@
        }
        $html += @"
        </div>
"@
    }
    
    $html += @"
    </div>

    <div class="footer">
        <p>Generated by Source Digestion Orchestrator v$($global:OrchestratorConfig.Version)</p>
        <p>Total Analysis Time: $($global:UnifiedResults.PerformanceMetrics.TotalDuration) seconds</p>
    </div>
</body>
</html>
"@
    
    return $html
}

# Summary dashboard generation
function Generate-SummaryDashboard {
    param($OutputPath)
    
    $dashboardPath = Join-Path $OutputPath "summary_dashboard.html"
    
    $html = @"
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Analysis Summary Dashboard</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }
        .dashboard { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .card { background-color: white; padding: 20px; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .card-title { font-size: 1.2em; font-weight: bold; color: #2c3e50; margin-bottom: 15px; }
        .metric { display: flex; justify-content: space-between; padding: 5px 0; border-bottom: 1px solid #ecf0f1; }
        .metric-label { color: #7f8c8d; }
        .metric-value { font-weight: bold; color: #2c3e50; }
        .status-good { color: #27ae60; }
        .status-warning { color: #f39c12; }
        .status-critical { color: #e74c3c; }
    </style>
</head>
<body>
    <h1>Analysis Summary Dashboard</h1>
    <div class="dashboard">
        <div class="card">
            <div class="card-title">Project Overview</div>
            <div class="metric">
                <span class="metric-label">Files Analyzed:</span>
                <span class="metric-value">$($global:UnifiedResults.AggregatedResults.TotalFiles)</span>
            </div>
            <div class="metric">
                <span class="metric-label">Total Issues:</span>
                <span class="metric-value">$($global:UnifiedResults.AggregatedResults.TotalIssues)</span>
            </div>
            <div class="metric">
                <span class="metric-label">Analysis Duration:</span>
                <span class="metric-value">$([Math]::Round($global:UnifiedResults.PerformanceMetrics.TotalDuration, 2)) seconds</span>
            </div>
        </div>

        <div class="card">
            <div class="card-title">Security Status</div>
            <div class="metric">
                <span class="metric-label">Vulnerabilities:</span>
                <span class="metric-value status-$(if($global:UnifiedResults.AggregatedResults.TotalVulnerabilities -gt 0){'critical'}else{'good'})">$($global:UnifiedResults.AggregatedResults.TotalVulnerabilities)</span>
            </div>
            <div class="metric">
                <span class="metric-label">Risk Score:</span>
                <span class="metric-value status-$(Get-RiskLevel -Score $global:UnifiedResults.AggregatedResults.RiskScore).ToLower()">$($global:UnifiedResults.AggregatedResults.RiskScore)</span>
            </div>
        </div>

        <div class="card">
            <div class="card-title">Performance</div>
            <div class="metric">
                <span class="metric-label">Bottlenecks:</span>
                <span class="metric-value">$($global:UnifiedResults.AggregatedResults.TotalBottlenecks)</span>
            </div>
            <div class="metric">
                <span class="metric-label">Dependencies:</span>
                <span class="metric-value">$($global:UnifiedResults.AggregatedResults.TotalDependencies)</span>
            </div>
        </div>
    </div>
</body>
</html>
"@
    
    $html | Out-File $dashboardPath -Encoding UTF8
    
    Write-Log "SUCCESS" "Summary dashboard created: $dashboardPath"
    
    return $dashboardPath
}

# Interactive mode
function Start-InteractiveMode {
    Write-Host "`n=== Source Digestion Orchestrator - Interactive Mode ===" -ForegroundColor Cyan
    Write-Host "Version: $($global:OrchestratorConfig.Version)" -ForegroundColor Gray
    
    while ($true) {
        Write-Host "`nOptions:" -ForegroundColor Yellow
        Write-Host "1. Start new analysis"
        Write-Host "2. Load existing results"
        Write-Host "3. Configure orchestrator"
        Write-Host "4. View analysis statistics"
        Write-Host "5. Export reports"
        Write-Host "6. Exit"
        
        $choice = Read-Host "`nSelect option (1-6)"
        
        switch ($choice) {
            "1" {
                $source = Read-Host "Enter source path"
                $output = Read-Host "Enter output path (press Enter for default)"
                if ([string]::IsNullOrEmpty($output)) { $output = ".\source_digestion_output" }
                
                Write-Host "`nSelect engines to run:" -ForegroundColor Yellow
                Write-Host "A. All engines (recommended)"
                Write-Host "S. Source Digestion only"
                Write-Host "R. Reverse Engineering only"
                Write-Host "D. Deployment Audit only"
                Write-Host "M. Manifest Tracer only"
                Write-Host "E. Architecture Enhancement only"
                Write-Host "C. Custom selection"
                
                $engineChoice = Read-Host "Select engines (A/S/R/D/M/E/C)"
                
                $params = @{
                    SourcePath = $source
                    OutputPath = $output
                }
                
                switch ($engineChoice.ToUpper()) {
                    "A" { $params.EnableAllEngines = $true }
                    "S" { $params.EnableSourceDigestion = $true }
                    "R" { $params.EnableReverseEngineering = $true }
                    "D" { $params.EnableDeploymentAudit = $true }
                    "M" { $params.EnableManifestTracer = $true }
                    "E" { $params.EnableArchitectureEnhancement = $true }
                    "C" {
                        $params.EnableSourceDigestion = (Read-Host "Enable Source Digestion? (y/n)").ToLower() -eq 'y'
                        $params.EnableReverseEngineering = (Read-Host "Enable Reverse Engineering? (y/n)").ToLower() -eq 'y'
                        $params.EnableDeploymentAudit = (Read-Host "Enable Deployment Audit? (y/n)").ToLower() -eq 'y'
                        $params.EnableManifestTracer = (Read-Host "Enable Manifest Tracer? (y/n)").ToLower() -eq 'y'
                        $params.EnableArchitectureEnhancement = (Read-Host "Enable Architecture Enhancement? (y/n)").ToLower() -eq 'y'
                    }
                }
                
                Start-Orchestration @params
            }
            "2" {
                $resultsPath = Read-Host "Enter results file path"
                Load-AnalysisResults -Path $resultsPath
            }
            "3" {
                Show-ConfigurationMenu
            }
            "4" {
                Show-Statistics
            }
            "5" {
                $format = Read-Host "Report format (HTML/JSON/Markdown)"
                $path = Read-Host "Output path"
                Generate-ComprehensiveReport -OutputPath $path -Format $format
            }
            "6" {
                Write-Host "Exiting..." -ForegroundColor Yellow
                return
            }
            default {
                Write-Host "Invalid option. Please try again." -ForegroundColor Red
            }
        }
    }
}

# Risk level determination
function Get-RiskLevel {
    param($Score)
    
    if ($score -ge 50) { return "Critical" }
    elseif ($score -ge 30) { return "High" }
    elseif ($score -ge 15) { return "Medium" }
    elseif ($score -gt 0) { return "Low" }
    else { return "Minimal" }
}

# Main execution
if ($Interactive) {
    Start-InteractiveMode
}
elseif ($SourcePath) {
    Start-Orchestration -SourcePath $SourcePath -OutputPath $OutputPath
}
else {
    Write-Host "Source Digestion Orchestrator v$($global:OrchestratorConfig.Version)" -ForegroundColor Cyan
    Write-Host "Use -Interactive for interactive mode or specify -SourcePath" -ForegroundColor Yellow
    Write-Host "Example: .\SourceDigestionOrchestrator.ps1 -SourcePath 'C:\project' -EnableAllEngines -GenerateComprehensiveReport" -ForegroundColor Gray
}