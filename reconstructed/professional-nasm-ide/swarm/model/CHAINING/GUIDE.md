# 🔗 Model Chaining System - Complete Guide

## Overview

The **Model Chaining System** enables you to cycle multiple AI models/agents through code in **500-line chunks**, with each specialized agent performing its role before passing the code to the next agent in the chain.

This is perfect for agentic coding workflows where you want:
- **Code Review** → validation → optimization → quality assurance
- **Security Analysis** → vulnerability scanning → debugging → performance tuning  
- **Code Generation** → analysis → refinement → documentation
- **Complex Refactoring** → architecture → implementation → formatting → review

## 🎯 Key Features

### 1. **Automatic Chunking (500 Lines)**
Code is automatically split into 500-line chunks to ensure each model can process it effectively without token limits.

```
Code (10,000 lines)
    ↓
[Chunk 1: Lines 1-500]
[Chunk 2: Lines 501-1000]
[Chunk 3: Lines 1001-1500]
... (20 chunks total)
```

### 2. **Model Cycling**
Each chunk passes through the configured agent chain sequentially:

```
Chunk 1: Analyzer → Validator → Optimizer → Reviewer
Chunk 2: Analyzer → Validator → Optimizer → Reviewer
Chunk 3: Analyzer → Validator → Optimizer → Reviewer
```

### 3. **Specialized Agent Roles**

| Role | Specialization | Use Case |
|------|---|---|
| **Analyzer** | Code structure, patterns, complexity | Initial assessment |
| **Generator** | Code generation, completion | Creating missing functionality |
| **Validator** | Correctness, edge cases, logic | Quality assurance |
| **Optimizer** | Performance, algorithms, memory | Speed improvements |
| **Documenter** | Comments, docstrings, examples | Creating documentation |
| **Debugger** | Bug detection, fixes, error handling | Fixing issues |
| **Security** | Vulnerabilities, injection risks, auth | Security audits |
| **Reviewer** | Code quality, conventions, readability | Best practices |
| **Formatter** | Style, formatting, organization | Code consistency |
| **Architect** | Design patterns, modularity, scalability | Architecture review |

### 4. **Feedback Loops**
Chains support multiple passes through the code with feedback accumulation:

```
Loop 1: Initial analysis, validation, optimization
Loop 2: Refinement based on Loop 1 results
Loop 3: Final validation and optimization
```

## 📦 Predefined Chains

### 1. **Code Review Chain**
```
analyzer → validator → optimizer → reviewer
```
**Purpose:** Complete code review with quality assurance  
**Use:** Before merging PRs, code quality validation

### 2. **Secure Coding Chain**
```
analyzer → security → debugger → optimizer
```
**Feedback Loops:** 2  
**Purpose:** Security-first development  
**Use:** Production code, sensitive operations

### 3. **Documentation Chain**
```
analyzer → documenter → formatter
```
**Purpose:** Generate comprehensive documentation  
**Use:** API docs, code documentation

### 4. **Optimization Chain**
```
analyzer → optimizer → validator
```
**Purpose:** Performance optimization  
**Use:** Performance improvements, bottleneck fixes

### 5. **Complete Analysis Chain**
```
analyzer → security → optimizer → documenter → reviewer
```
**Purpose:** Full-spectrum analysis  
**Use:** Important modules, critical paths

### 6. **Debugging Chain**
```
analyzer → debugger → validator → optimizer
```
**Feedback Loops:** 2  
**Purpose:** Find and fix bugs  
**Use:** Bug fixes, issue resolution

### 7. **Refactoring Chain**
```
analyzer → architect → generator → formatter → reviewer
```
**Purpose:** Major code refactoring  
**Use:** Large refactors, architectural changes

## 🚀 Quick Start

### Via Command Line

```bash
# List available chains
python chain_cli.py list

# Execute a chain on a file
python chain_cli.py execute code_review_chain mycode.py

# Quick review
python chain_cli.py review mycode.py

# Quick security check
python chain_cli.py secure mycode.py

# Save report
python chain_cli.py execute code_review_chain mycode.py -o report.json

# Custom feedback loops
python chain_cli.py execute secure_coding_chain mycode.py --feedback-loops 3
```

### Via Python Code

```python
import asyncio
from swarm.chain_controller import ChainController, QuickChainExecutor

async def main():
    controller = ChainController()
    
    # Execute a chain
    execution = await controller.execute_chain_on_file(
        chain_id="code_review_chain",
        file_path="mycode.py",
        language="python"
    )
    
    # Get report
    report = controller.get_execution_report(execution.execution_id)
    print(f"Status: {report['status']}")
    print(f"Chunks processed: {report['processed_chunks']}/{report['total_chunks']}")
    
    # Quick shortcuts
    executor = QuickChainExecutor(controller)
    review_report = await executor.review_and_optimize(code_string)
    secure_report = await executor.secure_code(code_string)
    docs_report = await executor.document_code(code_string)

asyncio.run(main())
```

## 🔧 Creating Custom Chains

### Via CLI

```bash
# Create custom chain with 3 agents, 2 feedback loops
python chain_cli.py create-chain my_chain "My Custom Chain" \
    "analyzer,optimizer,validator" \
    --feedback-loops 2 \
    --tags custom,production
```

### Via Python

```python
from swarm.chain_controller import ChainController

controller = ChainController()

# Create custom chain
config = controller.create_custom_chain(
    chain_id="my_chain",
    name="My Custom Chain",
    agent_roles=["Analyzer", "Optimizer", "Validator"],
    chunk_size=500,
    feedback_loops=2,
    tags=["custom", "production"],
    save_to_config=True
)

# Use it immediately
execution = await controller.execute_chain_on_code(
    "my_chain",
    code_string,
    language="python"
)
```

## 📊 Understanding Reports

Each execution generates a detailed report:

```json
{
  "execution_id": "exec_code_review_chain_1700000000000",
  "chain_id": "code_review_chain",
  "status": "completed",
  "duration_seconds": 45.23,
  "total_chunks": 20,
  "processed_chunks": 20,
  "failed_chunks": 0,
  "success_rate": "100%",
  "results_count": 80,
  "results": [
    {
      "model_id": "analyzer",
      "agent_role": "analyzer",
      "phase": "analyze",
      "chunk_id": "chunk_0001_a1b2c3d4",
      "chunk_number": 1,
      "processed_content": "...",
      "analysis": {
        "findings": [...]
      },
      "metrics": {
        "chunk_size": 500,
        "processing_speed_lines_per_sec": 8.2
      },
      "execution_time_ms": 610.5,
      "success": true,
      "suggestions": [...]
    },
    ...
  ]
}
```

## 🔌 Integration with Swarm Controller

The model chaining system integrates seamlessly with the existing swarm controller:

```python
from swarm_controller import SwarmController
from swarm.chain_controller import ChainController

# Initialize swarm
swarm = SwarmController()

# Create chain controller with swarm
chain_controller = ChainController(swarm_controller=swarm)

# Chains now use the full swarm agent pool
execution = await chain_controller.execute_chain_on_code(
    "code_review_chain",
    code
)
```

## 💡 Use Cases & Examples

### Use Case 1: Pre-Commit Code Review
```bash
# Review code before commit
python chain_cli.py review src/main.py -o review_report.json

# Check output and review suggestions
cat review_report.json
```

### Use Case 2: Security Audit
```bash
# Multi-loop security analysis
python chain_cli.py execute secure_coding_chain src/api.py \
    --feedback-loops 3 \
    -o security_audit.json
```

### Use Case 3: Documentation Generation
```python
import asyncio
from swarm.chain_controller import QuickChainExecutor, get_chain_controller

async def generate_docs(source_file):
    executor = QuickChainExecutor(get_chain_controller())
    report = await executor.document_code(open(source_file).read())
    
    # Extract generated documentation from results
    docs = "\n".join([
        result['processed_content'] 
        for result in report['results'] 
        if result['agent_role'] == 'documenter'
    ])
    return docs
```

### Use Case 4: Performance Optimization
```bash
# Find and optimize performance bottlenecks
python chain_cli.py execute optimization_chain slow_code.py \
    --feedback-loops 1 \
    -o optimizations.json
```

### Use Case 5: Major Refactoring
```python
# Complete refactoring workflow
execution = await controller.execute_chain_on_file(
    chain_id="refactoring_chain",
    file_path="legacy_code.py",
    language="python"
)

# Review architect recommendations first
report = controller.get_execution_report(execution.execution_id)
architect_phase = [r for r in report['results'] 
                   if r['agent_role'] == 'architect']
print(architect_phase[0]['analysis'])
```

## ⚙️ Configuration

### Default Chunks
- **Chunk Size:** 500 lines
- **Timeout:** 300-900 seconds (depends on chain)
- **Async Processing:** Enabled by default

### Customizing Chunk Size

```python
execution = await controller.execute_chain_on_code(
    chain_id="my_chain",
    code=code,
    # Override chunk size in chain config
)

# Or modify chain before execution
config = controller.orchestrator.chains["my_chain"]
config.chunk_size = 1000  # Process 1000 lines per chunk
```

### Feedback Loops

More feedback loops = better refinement but longer execution:

```
1 loop:   Fast analysis, good results
2 loops:  Refined analysis, better accuracy
3+ loops: Comprehensive analysis, slower
```

## 🎨 Chain Architecture

```
ChainController
  ├── ChainableAgent (each model/role)
  │   ├── process_chunk()
  │   ├── _build_prompt()
  │   └── state tracking
  │
  ├── ModelChainOrchestrator
  │   ├── register_chain()
  │   ├── split_into_chunks()
  │   ├── execute_chain()
  │   └── execution tracking
  │
  └── QuickChainExecutor (shortcuts)
      ├── review_and_optimize()
      ├── secure_code()
      ├── document_code()
      └── optimize_performance()
```

## 📈 Performance Characteristics

| Metric | Value |
|--------|-------|
| **Chunk Processing Speed** | ~8-15 lines/sec per agent |
| **10K line file** | ~20 chunks × 4 agents ≈ 2-4 min |
| **Memory per Agent** | ~200-400MB (model-dependent) |
| **Parallelization** | Sequential (can be async) |
| **Cache Hit Rate** | 20-40% (if enabled) |

## 🔐 Error Handling

The system includes comprehensive error handling:

```python
execution = await controller.execute_chain_on_code(chain_id, code)

if execution.status == "failed":
    print(f"Error: {execution.error}")
    
    # Check which chunks failed
    for result in execution.results:
        if not result['success']:
            print(f"Failed: {result['model_id']} on {result['chunk_id']}")
            print(f"Error: {result['error']}")
```

## 📝 Logging

Enable detailed logging:

```python
import logging
logging.basicConfig(level=logging.DEBUG)

# Now all chain operations are logged
executor = await orchestrator.execute_chain(...)
```

## 🚀 Advanced Usage

### Custom Role Implementation

```python
from swarm.model_chain_orchestrator import ChainableAgent, AgentRole

class CustomAgent(ChainableAgent):
    async def process_chunk(self, chunk, phase, context=None):
        # Custom processing logic
        result = await super().process_chunk(chunk, phase, context)
        # Post-process results
        return result
```

### Chaining Chains

```python
# Execute multiple chains in sequence
async def full_workflow(code):
    # 1. Security analysis
    security_exec = await controller.execute_chain_on_code(
        "secure_coding_chain", code
    )
    
    # 2. Optimization based on security results
    opt_exec = await controller.execute_chain_on_code(
        "optimization_chain", code
    )
    
    # 3. Documentation
    doc_exec = await controller.execute_chain_on_code(
        "documentation_chain", code
    )
    
    return {
        "security": security_exec,
        "optimization": opt_exec,
        "documentation": doc_exec
    }
```

## 📚 References

- **Module:** `swarm/model_chain_orchestrator.py` - Core orchestrator
- **Controller:** `swarm/chain_controller.py` - High-level API
- **CLI:** `swarm/chain_cli.py` - Command-line interface
- **Config:** `swarm/chains_config.json` - Chain definitions

## 🆘 Troubleshooting

### Chains Running Too Slow
- Reduce `feedback_loops`
- Increase `chunk_size`
- Enable caching: `enable_caching: true`

### Memory Issues
- Reduce chunk size
- Process fewer chunks in parallel
- Monitor with system tools

### Agent Not Processing
- Check if agent_id exists in config
- Verify swarm controller connectivity
- Check logs for errors

## 🎓 Learning Resources

1. **Start Simple:** Use quick commands (`review`, `secure`, `document`)
2. **Try Presets:** Execute built-in chains
3. **Create Custom:** Build your own chain
4. **Integrate:** Connect to your IDE/workflow
5. **Monitor:** Track performance and results

---

**Created for Professional NASM IDE - Advanced Agentic Coding**
