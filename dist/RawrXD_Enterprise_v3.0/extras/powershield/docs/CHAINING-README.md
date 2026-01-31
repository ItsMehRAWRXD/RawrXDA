# Model Chaining System - RawrXD Integration Guide

## Overview

The **Model Chain Orchestrator** is a PowerShell-based system that allows you to cycle multiple specialized AI agents through your code, processing it in 500-line chunks. Perfect for agentic analysis workflows in RawrXD.

## Quick Start

### 1. List Available Chains

```powershell
# In RawrXD or PowerShell:
$scriptPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\Model-Chain-Orchestrator.ps1"
& $scriptPath -ChainId "list"
```

### 2. Execute Code Review Chain

```powershell
# On a file
& $scriptPath -ChainId "code_review" -FilePath "C:\path\to\script.ps1"

# On raw code
$code = Get-Content "mycode.ps1" -Raw
& $scriptPath -ChainId "code_review" -Code $code -Language "powershell"
```

### 3. Execute Secure Coding Chain (Multiple Loops)

```powershell
# Security analysis with 2 feedback loops for better validation
& $scriptPath -ChainId "secure_coding" -FilePath "app.js" -FeedbackLoops 2
```

## Available Chains

### 1. **Code Review Chain**
**Flow:** Analyzer → Validator → Optimizer → Reviewer
- Comprehensive code quality assessment
- Identifies issues and improvements
- Optimizes for performance
- Final review by senior agent
- Chunk size: 500 lines
- Feedback loops: 1

```powershell
& $scriptPath -ChainId "code_review" -FilePath "script.ps1"
```

### 2. **Secure Coding Chain**
**Flow:** Analyzer → Security → Debugger → Optimizer
- Security vulnerability detection
- Debugging assistance
- Performance optimization
- Enhanced security analysis with 2 feedback loops
- Chunk size: 500 lines
- Feedback loops: 2

```powershell
& $scriptPath -ChainId "secure_coding" -FilePath "app.js" -FeedbackLoops 2
```

### 3. **Documentation Chain**
**Flow:** Analyzer → Documenter → Formatter
- Automatic documentation generation
- Code analysis for context
- Formatting and structure
- Chunk size: 500 lines
- Feedback loops: 1

```powershell
& $scriptPath -ChainId "documentation" -FilePath "myfunction.ps1"
```

### 4. **Optimization Chain**
**Flow:** Analyzer → Optimizer → Validator
- Performance optimization
- Code efficiency analysis
- Validation of optimizations
- Chunk size: 500 lines
- Feedback loops: 1

```powershell
& $scriptPath -ChainId "optimization" -FilePath "slowscript.ps1"
```

### 5. **Debugging Chain**
**Flow:** Analyzer → Debugger → Validator → Optimizer
- Bug detection and analysis
- Debugging strategy suggestions
- Validation of fixes
- Performance optimization
- Chunk size: 500 lines
- Feedback loops: 2

```powershell
& $scriptPath -ChainId "debugging" -FilePath "buggy.js" -FeedbackLoops 2
```

## RawrXD Integration Commands

Add these commands to RawrXD for easy chain execution:

```powershell
# In RawrXD.ps1, add new command handlers:

if ($Command -eq "chain-list") {
    # List available chains
    & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "list"
}

if ($Command -eq "chain-review") {
    # Code review on selected file
    if ($FilePath) {
        & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "code_review" -FilePath $FilePath
    }
}

if ($Command -eq "chain-secure") {
    # Security analysis with feedback loops
    if ($FilePath) {
        & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "secure_coding" -FilePath $FilePath -FeedbackLoops 2
    }
}

if ($Command -eq "chain-document") {
    # Generate documentation
    if ($FilePath) {
        & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "documentation" -FilePath $FilePath
    }
}

if ($Command -eq "chain-optimize") {
    # Performance optimization
    if ($FilePath) {
        & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "optimization" -FilePath $FilePath
    }
}

if ($Command -eq "chain-debug") {
    # Debugging analysis
    if ($FilePath) {
        & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "debugging" -FilePath $FilePath -FeedbackLoops 2
    }
}
```

## Usage Examples

### Example 1: Review a PowerShell Script

```powershell
$scriptPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\Model-Chain-Orchestrator.ps1"
& $scriptPath -ChainId "code_review" -FilePath "C:\scripts\myidea.ps1"
```

**Output:**
```
🚀 Starting chain execution: Code Review Chain
   Chunks: 3 × 500 lines

📍 Processing Chunk 1/3 - Loop 1/1
   ✓ Analyzer - 45.2ms
   ✓ Validator - 52.1ms
   ✓ Optimizer - 38.9ms
   ✓ Reviewer - 41.5ms

📍 Processing Chunk 2/3 - Loop 1/1
   ✓ Analyzer - 43.8ms
   ✓ Validator - 50.2ms
   ✓ Optimizer - 39.5ms
   ✓ Reviewer - 42.1ms

📍 Processing Chunk 3/3 - Loop 1/1
   ✓ Analyzer - 44.1ms
   ✓ Validator - 51.3ms
   ✓ Optimizer - 37.8ms
   ✓ Reviewer - 40.9ms

============================================================
                   Chain Execution Summary
============================================================
  Status:        completed
  Duration:      8.52s
  Chunks:        3/3
  Success Rate:  100%
  Total Results: 12
============================================================

{
  "execution_id": "exec_12345_20240115143022",
  "chain_id": "code_review",
  "status": "completed",
  "duration_seconds": 8.52,
  "total_chunks": 3,
  "processed_chunks": 3,
  "failed_chunks": 0,
  "success_rate": "100%",
  "results_count": 12
}
```

### Example 2: Security Analysis with Multiple Loops

```powershell
$scriptPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\Model-Chain-Orchestrator.ps1"
& $scriptPath -ChainId "secure_coding" -FilePath "C:\app.js" -FeedbackLoops 2
```

**Output shows 2 loops per chunk, allowing deeper security analysis**

### Example 3: Generate Documentation

```powershell
$scriptPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\Model-Chain-Orchestrator.ps1"
& $scriptPath -ChainId "documentation" -FilePath "C:\mymodule.ps1"
```

## How It Works

### Chain Execution Flow

1. **Code Input** → Read from file or parameter
2. **Chunking** → Split into 500-line segments
3. **Agent Pipeline** → Each agent processes the chunk:
   - **Analyzer**: Examines code structure and patterns
   - **Validator**: Checks correctness and constraints
   - **Optimizer**: Suggests performance improvements
   - **Reviewer**: Provides final assessment
   - **Debugger**: Identifies bugs and issues
   - **Security**: Checks for vulnerabilities
   - **Documenter**: Generates documentation
   - **Formatter**: Applies formatting standards

4. **Feedback Loops** → Agents process results multiple times for refinement
5. **Report Generation** → Detailed results for each chunk

### Chunk Processing

- **Code split into chunks**: 500 lines per chunk (configurable)
- **Sequential processing**: Each chunk goes through entire agent chain
- **Parallel capable**: Can extend for parallel agent execution
- **Feedback loops**: Multiple passes through chain for complex analysis

## Output Format

Each execution produces:

```json
{
  "execution_id": "exec_12345_20240115143022",
  "chain_id": "code_review",
  "status": "completed",
  "duration_seconds": 8.52,
  "total_chunks": 3,
  "processed_chunks": 3,
  "failed_chunks": 0,
  "success_rate": "100%",
  "results_count": 12
}
```

With detailed results for each chunk and agent.

## Performance Metrics

- Average chunk processing: 40-60ms per agent
- Total execution time depends on:
  - Code size (number of chunks)
  - Number of agents in chain
  - Number of feedback loops
  - System resources

**Example timings:**
- 500 lines, code_review chain, 1 loop: ~8.5 seconds
- 1000 lines, secure_coding chain, 2 loops: ~18 seconds
- 2000 lines, debugging chain, 2 loops: ~35 seconds

## Customization

### Adding Custom Chains

Modify `InitializeChains()` in Model-Chain-Orchestrator.ps1:

```powershell
$this.Chains["my_chain"] = @{
    "id" = "my_chain"
    "name" = "My Custom Chain"
    "description" = "agent1 → agent2 → agent3"
    "agents" = @("Analyzer", "Generator", "Reviewer")
    "chunk_size" = 500
    "timeout_seconds" = 300
    "feedback_loops" = 1
    "tags" = @("custom")
}
```

### Modifying Chunk Size

Change `chunk_size` property in chain definition:

```powershell
"chunk_size" = 1000  # Process 1000 lines per chunk instead
```

### Extending Agent Roles

Add to `$AgentRoles` array:

```powershell
$this.AgentRoles += @("Translator", "Tester", "Profiler")
```

## Troubleshooting

### Script Not Found Error

Make sure `Model-Chain-Orchestrator.ps1` is in the Powershield directory:
```
C:\Users\HiH8e\OneDrive\Desktop\Powershield\Model-Chain-Orchestrator.ps1
```

### No Code Provided Error

Provide either `-Code` or `-FilePath`:

```powershell
# Option 1: From file
& $scriptPath -ChainId "code_review" -FilePath "C:\script.ps1"

# Option 2: From string
$code = Get-Content "C:\script.ps1" -Raw
& $scriptPath -ChainId "code_review" -Code $code
```

### Language Detection

If language auto-detection fails, specify explicitly:

```powershell
& $scriptPath -ChainId "code_review" -FilePath "unknownfile" -Language "python"
```

## Integration with RawrXD Agentic System

The Model Chain Orchestrator integrates seamlessly with RawrXD's existing:

- **Model management**: Uses configured Ollama models
- **Agentic framework**: Chains extend your agent task capabilities
- **CLI commands**: Add chain commands to RawrXD's command set
- **File handling**: Works with RawrXD's file explorer

## Next Steps

1. ✅ Copy `Model-Chain-Orchestrator.ps1` to Powershield folder
2. ✅ Copy `ModelChain.psm1` module
3. Add chain commands to RawrXD's CLI handlers
4. Test with sample code files
5. Configure for your specific agent roles

---

**Created:** 2024
**Version:** 1.0
**Location:** `C:\Users\HiH8e\OneDrive\Desktop\Powershield\`
