#Requires -Version 7.4
<#
.SYNOPSIS
    RawrXD OMEGA-1 Agentic Job Dispatcher
.DESCRIPTION
    Accepts natural language jobs, parses intent, maps to module functions, and executes with error handling and reporting.
    Integrates with OmegaAgent for autonomous task execution and decision making.
#>

using namespace System.Collections.Generic
using namespace System.Management.Automation

class RawrXDJobIntent {
    [string]$OriginalText
    [string]$PrimaryIntent
    [hashtable]$Parameters
    [double]$Confidence
    [string[]]$RequiredModules
    [string]$EstimatedComplexity
    
    RawrXDJobIntent([string]$text) {
        $this.OriginalText = $text
        $this.Parameters = @{}
        $this.Confidence = 0.0
        $this.RequiredModules = @()
        $this.EstimatedComplexity = "Unknown"
    }
}

class RawrXDJobDispatcher {
    [Dictionary[string, scriptblock]]$IntentHandlers
    [Dictionary[string, string]]$ModuleMappings
    [object]$OmegaAgent
    [hashtable]$ExecutionHistory
    [string]$LastError
    
    RawrXDJobDispatcher() {
        $this.IntentHandlers = [Dictionary[string, scriptblock]]::new()
        $this.ModuleMappings = [Dictionary[string, string]]::new()
        $this.ExecutionHistory = @{}
        $this.LastError = ""
        $this.InitializeDefaultMappings()
    }
    
    [void]InitializeDefaultMappings() {
        # Map common intents to module functions
        $this.ModuleMappings["build"] = "RawrXD.Production"
        $this.ModuleMappings["test"] = "RawrXD.TestFramework"
        $this.ModuleMappings["deploy"] = "RawrXD.Production"
        $this.ModuleMappings["load model"] = "RawrXD.ModelLoader"
        $this.ModuleMappings["scan"] = "RawrXD.SecurityScanner"
        $this.ModuleMappings["patch"] = "RawrXD.Win32Deployment"
        $this.ModuleMappings["optimize"] = "RawrXD.AutonomousEnhancement"
        $this.ModuleMappings["swarm"] = "RawrXD.SwarmOrchestrator"
        $this.ModuleMappings["analyze"] = "RawrXD.AutonomousEnhancement"
        
        # Register default intent handlers
        $this.RegisterIntentHandler("build", {
            param($intent)
            Write-RawrXDLog "Executing build job: $($intent.OriginalText)" -Level "INFO" -Component "Dispatcher"
            try {
                $configPath = if ($intent.Parameters.ContainsKey("ConfigPath")) { 
                    $intent.Parameters["ConfigPath"] 
                } else { 
                    "$PSScriptRoot\config\production.config.json" 
                }
                
                Import-Module "$PSScriptRoot\RawrXD.Production.psm1" -Force
                Start-RawrXDProductionPipeline -ConfigPath $configPath
                
                return @{
                    Success = $true
                    Result = "Build pipeline completed successfully"
                    Duration = [datetime]::Now
                }
            }
            catch {
                return @{
                    Success = $false
                    Error = $_.Exception.Message
                    Duration = [datetime]::Now
                }
            }
        })
        
        $this.RegisterIntentHandler("test", {
            param($intent)
            Write-RawrXDLog "Executing test job: $($intent.OriginalText)" -Level "INFO" -Component "Dispatcher"
            try {
                $testPath = if ($intent.Parameters.ContainsKey("TestPath")) { 
                    $intent.Parameters["TestPath"] 
                } else { ".\tests" }
                
                $tags = if ($intent.Parameters.ContainsKey("Tags")) { 
                    $intent.Parameters["Tags"] 
                } else { @() }
                
                Import-Module "$PSScriptRoot\RawrXD.TestFramework.psm1" -Force
                $results = Invoke-RawrXDTestRunner -TestPath $testPath -Tags $tags
                
                return @{
                    Success = $results.SuccessRate -ge 80
                    Result = $results
                    Duration = [datetime]::Now
                }
            }
            catch {
                return @{
                    Success = $false
                    Error = $_.Exception.Message
                    Duration = [datetime]::Now
                }
            }
        })
        
        $this.RegisterIntentHandler("deploy", {
            param($intent)
            Write-RawrXDLog "Executing deploy job: $($intent.OriginalText)" -Level "INFO" -Component "Dispatcher"
            try {
                $artifacts = if ($intent.Parameters.ContainsKey("Artifacts")) { 
                    $intent.Parameters["Artifacts"] 
                } else { 
                    Get-Content "$PSScriptRoot\config\artifacts.json" | ConvertFrom-Json 
                }
                
                Import-Module "$PSScriptRoot\RawrXD.Production.psm1" -Force
                Deploy-RawrXDArtifacts -Artifacts $artifacts
                
                return @{
                    Success = $true
                    Result = "Deployment completed successfully"
                    Duration = [datetime]::Now
                }
            }
            catch {
                return @{
                    Success = $false
                    Error = $_.Exception.Message
                    Duration = [datetime]::Now
                }
            }
        })
        
        $this.RegisterIntentHandler("load model", {
            param($intent)
            Write-RawrXDLog "Executing model load job: $($intent.OriginalText)" -Level "INFO" -Component "Dispatcher"
            try {
                $backend = if ($intent.Parameters.ContainsKey("Backend")) { 
                    $intent.Parameters["Backend"] 
                } else { "GGUF" }
                
                $modelPath = if ($intent.Parameters.ContainsKey("ModelPath")) { 
                    $intent.Parameters["ModelPath"] 
                } else { 
                    throw "ModelPath parameter is required"
                }
                
                $device = if ($intent.Parameters.ContainsKey("Device")) { 
                    $intent.Parameters["Device"] 
                } else { "CPU" }
                
                Import-Module "$PSScriptRoot\RawrXD.ModelLoader.psm1" -Force
                $loader = Initialize-RawrXDModelLoader -Backend $backend -ModelPath $modelPath -Device $device
                
                return @{
                    Success = $true
                    Result = $loader
                    Duration = [datetime]::Now
                }
            }
            catch {
                return @{
                    Success = $false
                    Error = $_.Exception.Message
                    Duration = [datetime]::Now
                }
            }
        })
        
        $this.RegisterIntentHandler("scan", {
            param($intent)
            Write-RawrXDLog "Executing security scan job: $($intent.OriginalText)" -Level "INFO" -Component "Dispatcher"
            try {
                $scanPath = if ($intent.Parameters.ContainsKey("ScanPath")) { 
                    $intent.Parameters["ScanPath"] 
                } else { "." }
                
                $ruleSets = if ($intent.Parameters.ContainsKey("RuleSets")) { 
                    $intent.Parameters["RuleSets"] 
                } else { @("Common", "PowerShell") }
                
                Import-Module "$PSScriptRoot\RawrXD.SecurityScanner.psm1" -Force
                $results = Start-RawrXDSecurityScan -ScanPath $scanPath -RuleSets $ruleSets
                
                return @{
                    Success = $results.RiskScore -lt 50
                    Result = $results
                    Duration = [datetime]::Now
                }
            }
            catch {
                return @{
                    Success = $false
                    Error = $_.Exception.Message
                    Duration = [datetime]::Now
                }
            }
        })
        
        $this.RegisterIntentHandler("patch", {
            param($intent)
            Write-RawrXDLog "Executing patch job: $($intent.OriginalText)" -Level "INFO" -Component "Dispatcher"
            try {
                $targetAddress = if ($intent.Parameters.ContainsKey("TargetAddress")) { 
                    [IntPtr]$intent.Parameters["TargetAddress"] 
                } else { 
                    throw "TargetAddress parameter is required" 
                }
                
                $patchBytes = if ($intent.Parameters.ContainsKey("PatchBytes")) { 
                    $intent.Parameters["PatchBytes"] 
                } else { 
                    throw "PatchBytes parameter is required" 
                }
                
                $description = if ($intent.Parameters.ContainsKey("Description")) { 
                    $intent.Parameters["Description"] 
                } else { "Hotpatch" }
                
                Import-Module "$PSScriptRoot\RawrXD.Win32Deployment.psm1" -Force
                $result = New-RawrXDMemoryPatch -TargetAddress $targetAddress -PatchBytes $patchBytes -Description $description
                
                return @{
                    Success = $true
                    Result = $result
                    Duration = [datetime]::Now
                }
            }
            catch {
                return @{
                    Success = $false
                    Error = $_.Exception.Message
                    Duration = [datetime]::Now
                }
            }
        })
        
        $this.RegisterIntentHandler("optimize", {
            param($intent)
            Write-RawrXDLog "Executing optimization job: $($intent.OriginalText)" -Level "INFO" -Component "Dispatcher"
            try {
                $targetModule = if ($intent.Parameters.ContainsKey("TargetModule")) { 
                    $intent.Parameters["TargetModule"] 
                } else { 
                    throw "TargetModule parameter is required" 
                }
                
                Import-Module "$PSScriptRoot\RawrXD.AutonomousEnhancement.psm1" -Force
                $analysis = Start-RawrXDAutonomousAnalysis -TargetModule $targetModule
                
                if ($analysis.ImprovementCount -gt 0) {
                    $modulePath = "$PSScriptRoot\RawrXD.$targetModule.psm1"
                    $refactorResult = Invoke-RawrXDAutoRefactor -ModulePath $modulePath
                    
                    return @{
                        Success = $true
                        Result = $refactorResult
                        Analysis = $analysis
                        Duration = [datetime]::Now
                    }
                }
                else {
                    return @{
                        Success = $true
                        Result = "No improvements needed"
                        Analysis = $analysis
                        Duration = [datetime]::Now
                    }
                }
            }
            catch {
                return @{
                    Success = $false
                    Error = $_.Exception.Message
                    Duration = [datetime]::Now
                }
            }
        })
        
        $this.RegisterIntentHandler("swarm", {
            param($intent)
            Write-RawrXDLog "Executing swarm job: $($intent.OriginalText)" -Level "INFO" -Component "Dispatcher"
            try {
                $swarmId = if ($intent.Parameters.ContainsKey("SwarmId")) { 
                    $intent.Parameters["SwarmId"] 
                } else { "default" }
                
                $agentId = if ($intent.Parameters.ContainsKey("AgentId")) { 
                    $intent.Parameters["AgentId"] 
                } else { "agent_$(Get-Random)" }
                
                $role = if ($intent.Parameters.ContainsKey("Role")) { 
                    $intent.Parameters["Role"] 
                } else { "worker" }
                
                $capabilities = if ($intent.Parameters.ContainsKey("Capabilities")) { 
                    $intent.Parameters["Capabilities"] 
                } else { @{} }
                
                Import-Module "$PSScriptRoot\RawrXD.SwarmOrchestrator.psm1" -Force
                $swarm = New-RawrXDSwarm -SwarmId $swarmId
                $agent = Start-RawrXDSwarmAgent -AgentId $agentId -Role $role -Capabilities $capabilities
                $swarm.RegisterAgent($agent)
                
                return @{
                    Success = $true
                    Result = $swarm.GetSwarmStatus()
                    Duration = [datetime]::Now
                }
            }
            catch {
                return @{
                    Success = $false
                    Error = $_.Exception.Message
                    Duration = [datetime]::Now
                }
            }
        })
    }
    
    [void]RegisterIntentHandler([string]$intentName, [scriptblock]$handler) {
        $this.IntentHandlers[$intentName] = $handler
        Write-RawrXDLog "Registered intent handler for: $intentName" -Level "DEBUG" -Component "Dispatcher"
    }
    
    [RawrXDJobIntent]ParseIntent([string]$jobText) {
        $intent = [RawrXDJobIntent]::new($jobText)
        
        # Simple intent classification (expandable with ML)
        $lowerText = $jobText.ToLower()
        
        if ($lowerText -match "\bbuild\b") {
            $intent.PrimaryIntent = "build"
            $intent.Confidence = 0.9
            $intent.RequiredModules = @("RawrXD.Production")
            $intent.EstimatedComplexity = "Medium"
        }
        elseif ($lowerText -match "\btest\b") {
            $intent.PrimaryIntent = "test"
            $intent.Confidence = 0.9
            $intent.RequiredModules = @("RawrXD.TestFramework")
            $intent.EstimatedComplexity = "Low"
        }
        elseif ($lowerText -match "\bdeploy\b") {
            $intent.PrimaryIntent = "deploy"
            $intent.Confidence = 0.9
            $intent.RequiredModules = @("RawrXD.Production")
            $intent.EstimatedComplexity = "High"
        }
        elseif ($lowerText -match "\bload model\b|\bmodel\b") {
            $intent.PrimaryIntent = "load model"
            $intent.Confidence = 0.8
            $intent.RequiredModules = @("RawrXD.ModelLoader")
            $intent.EstimatedComplexity = "Medium"
            
            # Extract model path if present
            if ($jobText -match "['\"]([^'\"]+\.gguf)['\"]") {
                $intent.Parameters["ModelPath"] = $matches[1]
            }
        }
        elseif ($lowerText -match "\bscan\b|\bsecurity\b") {
            $intent.PrimaryIntent = "scan"
            $intent.Confidence = 0.85
            $intent.RequiredModules = @("RawrXD.SecurityScanner")
            $intent.EstimatedComplexity = "Medium"
        }
        elseif ($lowerText -match "\bpatch\b|\bhotpatch\b") {
            $intent.PrimaryIntent = "patch"
            $intent.Confidence = 0.8
            $intent.RequiredModules = @("RawrXD.Win32Deployment")
            $intent.EstimatedComplexity = "High"
        }
        elseif ($lowerText -match "\boptimize\b|\brefactor\b") {
            $intent.PrimaryIntent = "optimize"
            $intent.Confidence = 0.75
            $intent.RequiredModules = @("RawrXD.AutonomousEnhancement")
            $intent.EstimatedComplexity = "High"
        }
        elseif ($lowerText -match "\bswarm\b|\bagent\b") {
            $intent.PrimaryIntent = "swarm"
            $intent.Confidence = 0.8
            $intent.RequiredModules = @("RawrXD.SwarmOrchestrator")
            $intent.EstimatedComplexity = "Medium"
        }
        elseif ($lowerText -match "\banalyze\b|\breview\b") {
            $intent.PrimaryIntent = "analyze"
            $intent.Confidence = 0.7
            $intent.RequiredModules = @("RawrXD.AutonomousEnhancement")
            $intent.EstimatedComplexity = "Medium"
        }
        else {
            $intent.PrimaryIntent = "unknown"
            $intent.Confidence = 0.0
            $intent.EstimatedComplexity = "Unknown"
        }
        
        Write-RawrXDLog "Parsed intent: $($intent.PrimaryIntent) with confidence: $($intent.Confidence)" -Level "DEBUG" -Component "Dispatcher"
        return $intent
    }
    
    [hashtable]ExecuteJob([RawrXDJobIntent]$intent) {
        $jobId = [Guid]::NewGuid().ToString()
        $startTime = Get-Date
        
        Write-RawrXDLog "Starting job $jobId: $($intent.OriginalText)" -Level "INFO" -Component "Dispatcher"
        
        # Check if required modules are available
        foreach ($module in $intent.RequiredModules) {
            if (!(Get-Module -ListAvailable -Name $module)) {
                $errorMsg = "Required module not available: $module"
                Write-RawrXDLog $errorMsg -Level "ERROR" -Component "Dispatcher"
                
                $this.ExecutionHistory[$jobId] = @{
                    JobId = $jobId
                    Intent = $intent
                    Success = $false
                    Error = $errorMsg
                    StartTime = $startTime
                    EndTime = Get-Date
                    Duration = (Get-Date) - $startTime
                }
                
                return $this.ExecutionHistory[$jobId]
            }
        }
        
        # Execute the appropriate handler
        if ($this.IntentHandlers.ContainsKey($intent.PrimaryIntent)) {
            try {
                $handler = $this.IntentHandlers[$intent.PrimaryIntent]
                $result = & $handler $intent
                
                $this.ExecutionHistory[$jobId] = @{
                    JobId = $jobId
                    Intent = $intent
                    Success = $result.Success
                    Result = $result.Result
                    Error = if ($result.ContainsKey("Error")) { $result.Error } else { "" }
                    StartTime = $startTime
                    EndTime = Get-Date
                    Duration = (Get-Date) - $startTime
                }
                
                Write-RawrXDLog "Job $jobId completed: Success = $($result.Success)" -Level "INFO" -Component "Dispatcher"
            }
            catch {
                $this.ExecutionHistory[$jobId] = @{
                    JobId = $jobId
                    Intent = $intent
                    Success = $false
                    Error = $_.Exception.Message
                    StartTime = $startTime
                    EndTime = Get-Date
                    Duration = (Get-Date) - $startTime
                }
                
                Write-RawrXDLog "Job $jobId failed: $($_.Exception.Message)" -Level "ERROR" -Component "Dispatcher"
            }
        }
        else {
            $errorMsg = "No handler registered for intent: $($intent.PrimaryIntent)"
            Write-RawrXDLog $errorMsg -Level "ERROR" -Component "Dispatcher"
            
            $this.ExecutionHistory[$jobId] = @{
                JobId = $jobId
                Intent = $intent
                Success = $false
                Error = $errorMsg
                StartTime = $startTime
                EndTime = Get-Date
                Duration = (Get-Date) - $startTime
            }
        }
        
        return $this.ExecutionHistory[$jobId]
    }
    
    [hashtable[]]GetExecutionHistory([int]$limit = 10) {
        $history = $this.ExecutionHistory.Values | Sort-Object -Property StartTime -Descending
        
        if ($limit -gt 0) {
            return $history | Select-Object -First $limit
        }
        
        return $history
    }
    
    [void]ClearExecutionHistory() {
        $this.ExecutionHistory.Clear()
        Write-RawrXDLog "Execution history cleared" -Level "INFO" -Component "Dispatcher"
    }
}

function New-RawrXDJobDispatcher {
    [CmdletBinding()]
    param()
    
    return [RawrXDJobDispatcher]::new()
}

function Invoke-RawrXDJob {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$JobText,
        
        [Parameter()]
        [hashtable]$Parameters = @{},
        
        [Parameter()]
        [object]$Dispatcher
    )
    
    if ($null -eq $Dispatcher) {
        $Dispatcher = New-RawrXDJobDispatcher
    }
    
    Write-RawrXDLog "Processing job: $JobText" -Level "INFO" -Component "JobDispatcher"
    
    # Parse intent
    $intent = $Dispatcher.ParseIntent($JobText)
    
    # Add custom parameters
    foreach ($key in $Parameters.Keys) {
        $intent.Parameters[$key] = $Parameters[$key]
    }
    
    # Execute job
    $result = $Dispatcher.ExecuteJob($intent)
    
    return $result
}

function Get-RawrXDJobHistory {
    [CmdletBinding()]
    param(
        [Parameter()]
        [object]$Dispatcher,
        
        [Parameter()]
        [int]$Limit = 10
    )
    
    if ($null -eq $Dispatcher) {
        $Dispatcher = New-RawrXDJobDispatcher
    }
    
    return $Dispatcher.GetExecutionHistory($Limit)
}

function Clear-RawrXDJobHistory {
    [CmdletBinding()]
    param(
        [Parameter()]
        [object]$Dispatcher
    )
    
    if ($null -eq $Dispatcher) {
        $Dispatcher = New-RawrXDJobDispatcher
    }
    
    $Dispatcher.ClearExecutionHistory()
}

Export-ModuleMember -Function New-RawrXDJobDispatcher, Invoke-RawrXDJob, Get-RawrXDJobHistory, Clear-RawrXDJobHistory
