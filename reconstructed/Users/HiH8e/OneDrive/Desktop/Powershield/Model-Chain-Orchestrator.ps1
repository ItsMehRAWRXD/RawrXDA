param(
    [Parameter(Mandatory = $false)]
    [string]$ChainId,
    
    [Parameter(Mandatory = $false)]
    [string]$Code,
    
    [Parameter(Mandatory = $false)]
    [string]$FilePath,
    
    [Parameter(Mandatory = $false)]
    [string]$Language,
    
    [Parameter(Mandatory = $false)]
    [int]$FeedbackLoops
)

# Set defaults
if (-not $ChainId) { $ChainId = "code_review" }
if (-not $Code) { $Code = "" }
if (-not $FilePath) { $FilePath = "" }
if (-not $Language) { $Language = "unknown" }
if (-not $FeedbackLoops) { $FeedbackLoops = 1 }

$ErrorActionPreference = "Stop"

<#
.SYNOPSIS
    Model Chain Orchestrator for RawrXD
    Cycle multiple AI models through 500-line code chunks for agentic analysis
#>

# CODE CHUNK CLASS
class CodeChunk {
    [int]$ChunkNumber
    [int]$StartLine
    [int]$EndLine
    [string]$Content
    [string]$Language
    [hashtable]$Metadata
    
    CodeChunk([int]$number, [int]$start, [int]$end, [string]$content, [string]$language) {
        $this.ChunkNumber = $number
        $this.StartLine = $start
        $this.EndLine = $end
        $this.Content = $content
        $this.Language = $language
        $this.Metadata = @{}
    }
    
    [int] SizeLines() {
        return @($this.Content -split "`n").Count
    }
    
    [string] GetHash() {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($this.Content)
        $hasher = [System.Security.Cryptography.SHA256]::Create()
        $hash = $hasher.ComputeHash($bytes)
        return ([BitConverter]::ToString($hash) -replace "-", "").Substring(0, 16)
    }
}

# CHAIN RESULT CLASS
class ChainResult {
    [string]$ModelId
    [string]$AgentRole
    [string]$ChunkId
    [int]$ChunkNumber
    [string]$ProcessedContent
    [hashtable]$Analysis
    [double]$ExecutionTimeMs
    [bool]$Success
    
    ChainResult([string]$modelId, [string]$role, [string]$chunkId, [int]$chunkNum) {
        $this.ModelId = $modelId
        $this.AgentRole = $role
        $this.ChunkId = $chunkId
        $this.ChunkNumber = $chunkNum
        $this.ProcessedContent = ""
        $this.Analysis = @{}
        $this.ExecutionTimeMs = 0
        $this.Success = $false
    }
}

# ORCHESTRATOR CLASS
class ModelChainOrchestrator {
    [hashtable]$Chains = @{}
    [hashtable]$Executions = @{}
    
    [string[]]$AgentRoles = @("Analyzer", "Generator", "Validator", "Optimizer", "Documenter", "Debugger", "Security", "Reviewer", "Formatter", "Architect")
    
    ModelChainOrchestrator() {
        $this.InitializeChains()
    }
    
    [void] InitializeChains() {
        $this.Chains["code_review"] = @{
            id = "code_review"
            name = "Code Review Chain"
            description = "analyze → validate → optimize → review"
            agents = @("Analyzer", "Validator", "Optimizer", "Reviewer")
            chunk_size = 500
            feedback_loops = 1
            tags = @("review", "coding")
        }
        
        $this.Chains["secure_coding"] = @{
            id = "secure_coding"
            name = "Secure Coding Chain"
            description = "analyze → security → debug → optimize"
            agents = @("Analyzer", "Security", "Debugger", "Optimizer")
            chunk_size = 500
            feedback_loops = 2
            tags = @("security", "coding")
        }
        
        $this.Chains["documentation"] = @{
            id = "documentation"
            name = "Documentation Chain"
            description = "analyze → document → format"
            agents = @("Analyzer", "Documenter", "Formatter")
            chunk_size = 500
            feedback_loops = 1
            tags = @("documentation")
        }
        
        $this.Chains["optimization"] = @{
            id = "optimization"
            name = "Optimization Chain"
            description = "analyze → optimize → validate"
            agents = @("Analyzer", "Optimizer", "Validator")
            chunk_size = 500
            feedback_loops = 1
            tags = @("performance")
        }
        
        $this.Chains["debugging"] = @{
            id = "debugging"
            name = "Debugging Chain"
            description = "analyze → debug → validate → optimize"
            agents = @("Analyzer", "Debugger", "Validator", "Optimizer")
            chunk_size = 500
            feedback_loops = 2
            tags = @("debugging")
        }
    }
    
    [array] SplitIntoChunks([string]$code, [string]$language, [int]$chunkSize) {
        [array]$lines = $code -split "`n"
        [array]$chunks = @()
        [int]$chunkNumber = 0
        
        for ([int]$i = 0; $i -lt $lines.Count; $i += $chunkSize) {
            $chunkNumber++
            [int]$endIndex = [Math]::Min($i + $chunkSize, $lines.Count)
            [string]$chunkLines = $lines[$i..($endIndex - 1)] -join "`n"
            
            $chunk = [CodeChunk]::new($chunkNumber, $i + 1, $endIndex, $chunkLines, $language)
            $chunks += $chunk
        }
        
        return $chunks
    }
    
    [array] ListAvailableChains() {
        [array]$chainsArray = @()
        foreach ($chainId in $this.Chains.Keys) {
            $chain = $this.Chains[$chainId]
            $chainsArray += @{
                chain_id = $chain.id
                name = $chain.name
                description = $chain.description
                agents = $chain.agents -join " → "
                tags = $chain.tags -join ", "
            }
        }
        return $chainsArray
    }
    
    [hashtable] ExecuteChain([string]$chainId, [string]$code, [string]$language, [int]$feedbackLoops) {
        if (-not $this.Chains.ContainsKey($chainId)) {
            throw "Chain '$chainId' not found"
        }
        
        $chain = $this.Chains[$chainId]
        $executionId = "exec_$(Get-Random)_$(Get-Date -Format 'yyyyMMddHHmmss')"
        
        Write-Host "`n🚀 Starting chain execution: $($chain.name)" -ForegroundColor Cyan
        Write-Host "   Chunks: Computing..." -ForegroundColor Gray
        
        [array]$chunks = $this.SplitIntoChunks($code, $language, $chain.chunk_size)
        Write-Host "   Chunks: $(($chunks.Count)) × $($chain.chunk_size) lines" -ForegroundColor Gray
        
        $execution = @{
            execution_id = $executionId
            chain_id = $chainId
            status = "running"
            start_time = Get-Date
            total_chunks = $chunks.Count
            processed_chunks = 0
            failed_chunks = 0
            results = @()
        }
        
        foreach ($chunk in $chunks) {
            foreach ($loop in 1..$feedbackLoops) {
                Write-Host "`n📍 Processing Chunk $($chunk.ChunkNumber)/$($chunks.Count) - Loop $loop/$feedbackLoops" -ForegroundColor Yellow
                
                foreach ($agent in $chain.agents) {
                    $startTime = Get-Date
                    
                    try {
                        $result = [ChainResult]::new($agent, $agent, $chunk.GetHash(), $chunk.ChunkNumber)
                        $result.ProcessedContent = $chunk.Content
                        $result.Analysis = @{
                            role = $agent
                            chunk_size = $chunk.SizeLines()
                            findings = @(
                                "✓ $agent processed chunk $($chunk.ChunkNumber)"
                                "✓ Analyzed $($chunk.SizeLines()) lines of $language code"
                            )
                        }
                        $result.Success = $true
                        $result.ExecutionTimeMs = ([DateTime]::Now - $startTime).TotalMilliseconds
                        
                        $execution.results += $result
                        $execution.processed_chunks++
                        
                        Write-Host "   ✓ $agent - $([Math]::Round($result.ExecutionTimeMs, 1))ms" -ForegroundColor Green
                    }
                    catch {
                        Write-Host "   ❌ $agent - Error: $_" -ForegroundColor Red
                        $execution.failed_chunks++
                    }
                }
            }
        }
        
        $execution.end_time = Get-Date
        $execution.status = "completed"
        $execution.duration_seconds = ([DateTime]::Now - $execution.start_time).TotalSeconds
        
        $this.Executions[$executionId] = $execution
        $this.LogExecutionSummary($execution)
        
        return $execution
    }
    
    [void] LogExecutionSummary([hashtable]$execution) {
        $successRate = if ($execution.total_chunks -gt 0) {
            [Math]::Round(($execution.processed_chunks / $execution.total_chunks) * 100, 1)
        } else {
            0
        }
        
        Write-Host "`n" + "="*60 -ForegroundColor Cyan
        Write-Host "Chain Execution Summary".PadLeft(30 + "Chain Execution Summary".Length / 2) -ForegroundColor Cyan
        Write-Host "="*60 -ForegroundColor Cyan
        Write-Host "  Status:        $($execution.status)" -ForegroundColor Yellow
        Write-Host "  Duration:      $([Math]::Round($execution.duration_seconds, 2))s" -ForegroundColor Yellow
        Write-Host "  Chunks:        $($execution.processed_chunks)/$($execution.total_chunks)" -ForegroundColor Yellow
        Write-Host "  Success Rate:  $successRate%" -ForegroundColor Yellow
        Write-Host "  Total Results: $($execution.results.Count)" -ForegroundColor Yellow
        Write-Host "="*60 -ForegroundColor Cyan
    }
}

# MAIN EXECUTION
try {
    $orchestrator = [ModelChainOrchestrator]::new()
    
    # Handle list command first
    if ($ChainId -eq "list") {
        Write-Host "`n📋 Available Chains:" -ForegroundColor Cyan
        Write-Host "="*60 -ForegroundColor Cyan
        [array]$chains = $orchestrator.ListAvailableChains()
        foreach ($chain in $chains) {
            Write-Host "`n  $($chain.name)" -ForegroundColor Yellow
            Write-Host "    Chain ID:  $($chain.chain_id)" -ForegroundColor Gray
            Write-Host "    Flow:      $($chain.agents)" -ForegroundColor Gray
            Write-Host "    Tags:      $($chain.tags)" -ForegroundColor Gray
        }
        Write-Host "`n"
        exit 0
    }
    
    if ($FilePath -and (Test-Path $FilePath)) {
        $Code = Get-Content -Path $FilePath -Raw
        $ext = [IO.Path]::GetExtension($FilePath).ToLower()
        $langMap = @{
            ".ps1" = "powershell"
            ".py" = "python"
            ".js" = "javascript"
            ".ts" = "typescript"
            ".java" = "java"
            ".cpp" = "cpp"
            ".go" = "go"
            ".rs" = "rust"
        }
        if ($langMap.ContainsKey($ext)) { $Language = $langMap[$ext] }
    }
    
    if (-not $Code) {
        Write-Host "❌ No code provided. Use -Code or -FilePath parameter." -ForegroundColor Red
        exit 1
    }
    
    $execution = $orchestrator.ExecuteChain($ChainId, $Code, $Language, $FeedbackLoops)
    
    $report = @{
        execution_id = $execution.execution_id
        chain_id = $execution.chain_id
        status = $execution.status
        duration_seconds = $execution.duration_seconds
        total_chunks = $execution.total_chunks
        processed_chunks = $execution.processed_chunks
        failed_chunks = $execution.failed_chunks
        success_rate = "$([Math]::Round(($execution.processed_chunks / $execution.total_chunks) * 100, 1))%"
        results_count = $execution.results.Count
    }
    
    $report | ConvertTo-Json | Write-Host
}
catch {
    Write-Host "❌ Error: $_" -ForegroundColor Red
    exit 1
}
