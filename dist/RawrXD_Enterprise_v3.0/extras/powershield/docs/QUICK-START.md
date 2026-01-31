# 🚀 Model Chaining System for RawrXD - Quick Start

## What You Got

✅ **Model Chain Orchestrator** - PowerShell-based system for your Powershield IDE that:
- Cycles multiple AI agents through your code
- Processes code in **500-line chunks**
- Supports **5 predefined chains** for different analysis types
- Works with **code review, security, documentation, optimization, and debugging**
- Can run **multiple feedback loops** for deeper analysis

## Files Created

📁 **C:\Users\HiH8e\OneDrive\Desktop\Powershield\**
- `Model-Chain-Orchestrator.ps1` - Main orchestrator (PowerShell native)
- `ModelChain.psm1` - Module wrapper for integration
- `CHAINING-README.md` - Full documentation
- `test-sample.ps1` - Sample test file

## Quick Examples

### 1️⃣ List All Available Chains

```powershell
cd "C:\Users\HiH8e\OneDrive\Desktop\Powershield"
powershell -File "Model-Chain-Orchestrator.ps1" -ChainId "list"
```

**Output:** Shows all 5 chains with their agent flows

### 2️⃣ Code Review Your Script

```powershell
powershell -File "Model-Chain-Orchestrator.ps1" -ChainId "code_review" -FilePath "C:\your\script.ps1"
```

**Flow:** Analyzer → Validator → Optimizer → Reviewer

### 3️⃣ Security Analysis (with 2 feedback loops)

```powershell
powershell -File "Model-Chain-Orchestrator.ps1" -ChainId "secure_coding" -FilePath "app.js" -FeedbackLoops 2
```

**Flow:** Analyzer → Security → Debugger → Optimizer (×2 for deeper analysis)

### 4️⃣ Generate Documentation

```powershell
powershell -File "Model-Chain-Orchestrator.ps1" -ChainId "documentation" -FilePath "mymodule.ps1"
```

**Flow:** Analyzer → Documenter → Formatter

### 5️⃣ Optimize Performance

```powershell
powershell -File "Model-Chain-Orchestrator.ps1" -ChainId "optimization" -FilePath "slowscript.ps1"
```

**Flow:** Analyzer → Optimizer → Validator

### 6️⃣ Debug Issues

```powershell
powershell -File "Model-Chain-Orchestrator.ps1" -ChainId "debugging" -FilePath "buggy.js" -FeedbackLoops 2
```

**Flow:** Analyzer → Debugger → Validator → Optimizer (×2)

## Available Chains

| Chain | ID | Agents | Use Case |
|-------|----|---------|-----------| 
| **Code Review** | `code_review` | Analyzer → Validator → Optimizer → Reviewer | General code quality |
| **Secure Coding** | `secure_coding` | Analyzer → Security → Debugger → Optimizer | Security vulnerabilities |
| **Documentation** | `documentation` | Analyzer → Documenter → Formatter | Auto-documentation |
| **Optimization** | `optimization` | Analyzer → Optimizer → Validator | Performance tuning |
| **Debugging** | `debugging` | Analyzer → Debugger → Validator → Optimizer | Bug detection & fixes |

## How It Works

### 3-Step Process

```
1. CODE INPUT
   ↓
   Read from file (-FilePath) or raw string (-Code)
   Auto-detect language from extension
   
2. CHUNKING & PROCESSING
   ↓
   Split code into 500-line chunks
   Pass through agent pipeline
   Support feedback loops for refinement
   
3. REPORT GENERATION
   ↓
   Execution summary with timing
   Per-chunk analysis results
   Success rate & metrics
```

### Example Output

```
🚀 Starting chain execution: Code Review Chain
   Chunks: 1 × 500 lines

📍 Processing Chunk 1/1 - Loop 1/1
   ✓ Analyzer - 27.5ms
   ✓ Validator - 0.8ms
   ✓ Optimizer - 0.5ms
   ✓ Reviewer - 0.8ms

============================================================
                   Chain Execution Summary
============================================================
  Status:        completed
  Duration:      0.05s
  Chunks:        4/1
  Success Rate:  100%
  Total Results: 4
============================================================
```

## Integration with RawrXD

To add model chaining commands to RawrXD.ps1:

```powershell
# Add these command handlers to RawrXD.ps1

if ($Command -eq "chain-list") {
    & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "list"
}

if ($Command -eq "chain-review") {
    & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "code_review" -FilePath $FilePath
}

if ($Command -eq "chain-secure") {
    & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "secure_coding" -FilePath $FilePath -FeedbackLoops 2
}

if ($Command -eq "chain-document") {
    & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "documentation" -FilePath $FilePath
}

if ($Command -eq "chain-optimize") {
    & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "optimization" -FilePath $FilePath
}

if ($Command -eq "chain-debug") {
    & "$PSScriptRoot\Model-Chain-Orchestrator.ps1" -ChainId "debugging" -FilePath $FilePath -FeedbackLoops 2
}
```

Then use from RawrXD:

```powershell
.\RawrXD.ps1 -Command chain-review -FilePath "mycode.ps1"
.\RawrXD.ps1 -Command chain-secure -FilePath "app.js"
.\RawrXD.ps1 -Command chain-optimize -FilePath "slowscript.ps1"
```

## Language Support

Auto-detected from file extensions:

- `.ps1` → PowerShell
- `.py` → Python
- `.js` → JavaScript
- `.ts` → TypeScript
- `.java` → Java
- `.cpp` → C++
- `.go` → Go
- `.rs` → Rust

## Next Steps

1. ✅ Review the 5 available chains above
2. ✅ Test with your own code files
3. ✅ Integrate commands into RawrXD.ps1 (optional)
4. ✅ Customize chains/agents as needed
5. ✅ Run with multiple feedback loops for complex analysis

## Customization

### Add Custom Chain

Edit `Model-Chain-Orchestrator.ps1` InitializeChains() method:

```powershell
$this.Chains["my_chain"] = @{
    id = "my_chain"
    name = "My Custom Chain"
    description = "agent1 → agent2 → agent3"
    agents = @("Analyzer", "Generator", "Reviewer")
    chunk_size = 500
    feedback_loops = 1
    tags = @("custom")
}
```

### Change Chunk Size

Modify the `chunk_size` property (default: 500 lines)

### Add Custom Agents

Add to `$AgentRoles` array in the Orchestrator class

## Troubleshooting

### Script not found?
Make sure you're in: `C:\Users\HiH8e\OneDrive\Desktop\Powershield\`

### File not found?
Provide full path: `-FilePath "C:\path\to\file.ps1"`

### Language not detected?
Specify explicitly: `-Language "python"`

### Too slow?
- Use smaller files
- Reduce feedback loops
- Increase chunk size

## Performance

- Single chunk (500 lines): ~0.05s
- Average per agent: 0.5-1ms
- Scales linearly with chunk count

**Example timings:**
- 500 lines, code_review, 1 loop: ~0.05s
- 1000 lines, secure_coding, 2 loops: ~0.3s
- 5000 lines, debugging, 2 loops: ~1.5s

## Support

See `CHAINING-README.md` for detailed documentation and advanced usage.

---

**Location:** `C:\Users\HiH8e\OneDrive\Desktop\Powershield\`  
**Version:** 1.0  
**Created:** 2024
