# ==============================================================================
# UnifiedAgentProcessor.ps1
# Minimal AI Agent with unified processing capabilities
# ==============================================================================

using namespace System
using namespace System.Collections.Generic

# Enums
enum RequestType {
    TextCompletion
    CodeGeneration
    CloudResource
    AgenticTask
}

# Base Classes
class BaseRequest {
    [string] $Content
    [RequestType] $Type
    
    BaseRequest() {
        $this.Type = [RequestType]::TextCompletion
    }
}

class BaseResponse {
    [string] $Content
    [bool] $Success = $true
    [string] $Error
    [hashtable] $AgenticActions
    [bool] $IsOfflineMode = $false
    
    BaseResponse() {
        $this.AgenticActions = @{}
    }
}

class ModelConfig {
    [string] $Name
    [string] $Endpoint
    [string] $ApiKey
    [int] $Priority
    [int] $AvailabilityWeight = 100
    [bool] $IsAvailable = $true
    [int] $FailureCount = 0
    [int] $SuccessCount = 0
    
    ModelConfig([string]$name, [string]$endpoint, [string]$apiKey, [int]$priority) {
        if ([string]::IsNullOrEmpty($name)) { throw "Name cannot be null or empty" }
        $this.Name = $name
        $this.Endpoint = $endpoint
        $this.ApiKey = $apiKey
        $this.Priority = $priority
    }
}

class SpecializedProcessor {
    [string] $Name
    [scriptblock] $Handler
    [string[]] $Keywords
    
    SpecializedProcessor([string]$name, [scriptblock]$handler, [string[]]$keywords) {
        if ([string]::IsNullOrEmpty($name)) { throw "Name cannot be null or empty" }
        if ($null -eq $handler) { throw "Handler cannot be null" }
        $this.Name = $name
        $this.Handler = $handler
        $this.Keywords = $keywords
    }
    
    [bool] CanProcess([string]$request) {
        foreach ($keyword in $this.Keywords) {
            if ($request -match $keyword) { 
                return $true 
            }
        }
        return $false
    }
}

# Main Processor Class
class UnifiedAgentProcessor {
    [ModelConfig[]] $Models
    [SpecializedProcessor[]] $Processors
    [bool] $EnableAgenticMode = $true
    [object] $HttpClient
    [Dictionary[string,object]] $Context
    
    UnifiedAgentProcessor() {
        $this.Models = @()
        $this.Processors = @()
        $this.HttpClient = $null
        $this.Context = [Dictionary[string,object]]::new()
        $this.InitializeModels()
        $this.InitializeProcessors()
    }
    
    [void] InitializeModels() {
        $this.Models = @(
            [ModelConfig]::new("GitHub-Copilot", "https://api.github.com/copilot", "", 1),
            [ModelConfig]::new("Amazon-Q", "https://api.aws.amazon.com/q", "", 2),
            [ModelConfig]::new("Local-Model", "http://localhost:8080", "", 3)
        )
    }
    
    [void] InitializeProcessors() {
        $codeHandler = {
            param($request, $context)
            $response = [BaseResponse]::new()
            $response.Content = "Generated code for: $($request.Content)"
            return $response
        }
        $cloudHandler = {
            param($request, $context)
            $response = [BaseResponse]::new()
            $response.Content = "Cloud resource processed: $($request.Content)"
            return $response
        }
        $agentHandler = {
            param($request, $context)
            $response = [BaseResponse]::new()
            $response.Content = "Agentic task executed: $($request.Content)"
            $response.AgenticActions = @{ "action" = "completed" }
            return $response
        }
        $this.Processors = @(
            [SpecializedProcessor]::new("CodeGenerator", $codeHandler, @("code", "function", "class")),
            [SpecializedProcessor]::new("CloudManager", $cloudHandler, @("aws", "cloud", "deploy")),
            [SpecializedProcessor]::new("AgentProcessor", $agentHandler, @("agent", "automate", "task"))
        )
    }

    [ModelConfig] SelectBestModel() {
        $available = $this.Models | Where-Object { $_.IsAvailable }
        return ($available | Sort-Object Priority | Select-Object -First 1)
    }

    [void] SetContext([string]$key, [object]$value) {
        $this.Context[$key] = $value
    }

    [object] GetContext([string]$key) {
        if ($this.Context.ContainsKey($key)) {
            return $this.Context[$key]
        }
        return $null
    }
    
    [BaseResponse] ProcessRequest([object] $request) {
        $typedRequest = $null
        
        if ($request -is [BaseRequest]) {
            $typedRequest = $request
        } elseif ($request -is [string]) {
            $typedRequest = [BaseRequest]::new()
            $typedRequest.Content = $request
        } else {
            $response = [BaseResponse]::new()
            $response.Success = $false
            $response.Error = "Invalid request type"
            return $response
        }
        
        try {
            $processor = $this.SelectProcessor($typedRequest.Content)
            return $processor.Handler.Invoke($typedRequest, $this.Context)
        }
        catch {
            $response = [BaseResponse]::new()
            $response.Success = $false
            $response.Error = $_.Exception.Message
            return $response
        }
    }
    
    [SpecializedProcessor] SelectProcessor([string] $content) {
        foreach ($processor in $this.Processors) {
            if ($processor.CanProcess($content)) {
                return $processor
            }
        }
        return $this.Processors[0]
    }
    
    [bool] CanHandle([string] $requestType) {
        return $true
    }
    
    [string] GetProcessorName() {
        return "UnifiedAgentProcessor"
    }
    
    [ModelConfig] SelectBestModel() {
        $available = $this.Models | Where-Object { $_.IsAvailable }
        return ($available | Sort-Object Priority | Select-Object -First 1)
    }
    
    [void] SetContext([string]$key, [object]$value) {
        $this.Context[$key] = $value
    }
    
    [object] GetContext([string]$key) {
        return $this.Context[$key]
    }
    
    [hashtable] GetCapabilities() {
        return @{
            "SupportsCodeGeneration" = $true
            "SupportsCloudResources" = $true
            "SupportsAgenticMode" = $this.EnableAgenticMode
            "ModelCount" = $this.Models.Count
            "ProcessorCount" = $this.Processors.Count
        }
    }
}

# Export the main class
Export-ModuleMember -Variable UnifiedAgentProcessor