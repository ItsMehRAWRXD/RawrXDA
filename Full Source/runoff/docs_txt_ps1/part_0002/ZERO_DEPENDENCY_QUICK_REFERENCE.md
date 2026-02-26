# Zero-Dependency Model Maker - Quick Reference

## 🚀 One-Liner Commands

### Build Models

```powershell
# 1.98GB Kernel Reverse Engineer
.\scripts\model_maker_zero_dep.ps1 -Operation build-from-scratch -ModelSize 7B -TargetSize 1.98 -SystemPrompt "you are a kernel reverse engineer specialist" -QuantizationType Q4_K

# 2.5GB Malware Analyst
.\scripts\model_maker_zero_dep.ps1 -Operation build-from-scratch -ModelSize 7B -TargetSize 2.5 -SystemPrompt "you are a malware analyst expert" -QuantizationType Q4_K -ContextLength 8192

# All Swarm Models
.\scripts\swarm_model_integration.ps1 -BuildModelsForSwarm
```

### Generate Prompts

```powershell
# Kernel RE
.\scripts\system_prompt_engine.ps1 -Role kernel-reverse-engineer -OutputFile prompt.txt

# Random variant
.\scripts\system_prompt_engine.ps1 -Role assembly-expert -RandomMode -OutputFile prompt.txt
```

### Reverse Engineering

```powershell
# Analyze
.\scripts\model_self_digest.ps1 -Operation analyze -SourceModel "model.gguf"

# Reconstruct
.\scripts\model_maker_zero_dep.ps1 -Operation reverse-engineer -SourceModel "model.gguf" -SystemPrompt "new role"

# Evolve
.\scripts\model_self_digest.ps1 -Operation evolve -SourceModel "model.gguf" -Generations 5
```

## 📋 Parameter Quick Reference

### Model Sizes
```
7B  → 1.98GB (Q4_K) | 4.5GB (Q8_0) | 14GB (FP16)
13B → 3.5GB (Q4_K)  | 8GB (Q8_0)   | 26GB (FP16)
70B → 20GB (Q4_K)   | 45GB (Q8_0)  | 140GB (FP16)
```

### Quantization Types
```
Q4_K  → 4-bit, smallest, good quality
Q8_0  → 8-bit, larger, high quality
FP16  → 16-bit, very large, best quality
FP32  → 32-bit, huge, perfect quality
```

### Context Lengths
```
2048   → Fast, minimal memory
4096   → Standard, balanced
8192   → Large, better long-range
16384  → Very large, memory intensive
32768  → Extreme, high memory
```

### Available Roles
```
kernel-reverse-engineer
assembly-expert
security-researcher
malware-analyst
binary-exploitation
crypto-specialist
firmware-analyst
driver-developer
```

## 🎯 Common Workflows

### Workflow 1: Custom Specialist Model
```powershell
# 1. Generate prompt
.\scripts\system_prompt_engine.ps1 -Role kernel-reverse-engineer -OutputFile temp.txt
$prompt = Get-Content temp.txt -Raw

# 2. Build model
.\scripts\model_maker_zero_dep.ps1 -Operation build-from-scratch -ModelSize 7B -TargetSize 1.98 -SystemPrompt $prompt -QuantizationType Q4_K
```

### Workflow 2: Swarm Deployment
```powershell
# Build all models
.\scripts\swarm_model_integration.ps1 -BuildModelsForSwarm

# Check status
.\scripts\swarm_model_integration.ps1 -ListModels

# Enable evolution
.\scripts\swarm_model_integration.ps1 -AutoEvolution
```

### Workflow 3: Model Evolution
```powershell
# Build base
.\scripts\model_maker_zero_dep.ps1 -Operation build-from-scratch -ModelSize 7B -TargetSize 2.0 -SystemPrompt "expert"

# Evolve
.\scripts\model_self_digest.ps1 -Operation evolve -SourceModel "base.gguf" -Generations 10 -MutationRate 0.05

# Analyze result
.\scripts\model_self_digest.ps1 -Operation analyze -SourceModel "evolved.gguf"
```

### Workflow 4: Self-Digesting Model
```powershell
# Create self-digest model
.\scripts\model_maker_zero_dep.ps1 -Operation self-digest -ModelSize 7B -ContextLength 8192

# Digest external data
.\scripts\model_self_digest.ps1 -Operation digest -SourceModel "self_digest.gguf" -DigestData "corpus.txt"
```

## 🔧 Troubleshooting

### Model Too Large
```powershell
# Reduce quantization
-QuantizationType Q4_K  # Use instead of Q8_0 or FP16

# Reduce context
-ContextLength 2048     # Use instead of 4096+
```

### Model Too Small
```powershell
# Increase quantization
-QuantizationType Q8_0  # Use instead of Q4_K

# Increase context
-ContextLength 8192     # Use instead of 2048
```

### Build Failed
```powershell
# Check paths
Test-Path "D:\OllamaModels"

# Create directories
New-Item -ItemType Directory "D:\OllamaModels\custom_built" -Force

# Verify scripts
Get-ChildItem "D:\lazy init ide\scripts\model_*.ps1"
```

## 📊 Size Calculator

```
Formula: Size (GB) ≈ Parameters × Bytes_per_param

Q4_K:  7B × 0.5 bytes  = 3.5GB → ~1.98GB (compressed)
Q8_0:  7B × 1 byte     = 7GB   → ~4.5GB (compressed)
FP16:  7B × 2 bytes    = 14GB  (no compression)
FP32:  7B × 4 bytes    = 28GB  (no compression)
```

## 🎓 Pro Tips

1. **Use Q4_K for swarm agents** - Best balance of size/quality
2. **Enable RandomMode for diversity** - Get varied model behaviors
3. **Evolve successful models** - Improve performance over time
4. **Digest domain-specific data** - Specialize without retraining
5. **Monitor file sizes** - Ensure quantization worked
6. **Save prompts** - Reuse successful configurations

## 🔗 Integration Points

### With Swarm Control Center
```powershell
# Auto-integrate
.\scripts\swarm_model_integration.ps1 -BuildModelsForSwarm

# Manual assignment
.\scripts\swarm_model_integration.ps1 -AssignRole "Scout" -ModelRole "security-researcher"
```

### With Autonomous Agents
```powershell
# In your agent script:
$modelPath = "D:\OllamaModels\swarm_models\Scout_model.gguf"
# Use with your inference engine
```

## 📞 Quick Support

**Issue:** Model won't build
**Fix:** Check disk space, permissions, PowerShell version

**Issue:** Wrong size generated
**Fix:** Adjust -TargetSize parameter, verify quantization

**Issue:** Can't find scripts
**Fix:** Verify path: `D:\lazy init ide\scripts\`

**Issue:** Out of memory
**Fix:** Reduce model size or context length

## 🎯 Performance Expectations

### Build Times (Approximate)
```
7B  Q4_K:   5-10 minutes
7B  Q8_0:   10-15 minutes
13B Q4_K:   15-25 minutes
70B Q4_K:   60-90 minutes
```

### Memory Usage (Build Process)
```
7B:   4-8GB RAM
13B:  8-16GB RAM
70B:  32-64GB RAM
```

### Disk Space (Per Model)
```
7B  Q4_K:   ~2GB
13B Q4_K:   ~3.5GB
70B Q4_K:   ~20GB
```

## ✅ Validation Checklist

After building a model:

- [ ] File size matches target (±10%)
- [ ] .gguf extension present
- [ ] Metadata file (.json) created
- [ ] System prompt embedded
- [ ] Model opens in inference engine
- [ ] Responds to prompts
- [ ] Role behavior matches intent

## 🚀 Advanced Features

### Batch Generation
```powershell
@("kernel-reverse-engineer", "malware-analyst", "binary-exploitation") | ForEach-Object {
    .\scripts\model_maker_zero_dep.ps1 -Operation build-from-scratch -ModelSize 7B -TargetSize 1.98 -SystemPrompt "expert in $_" -QuantizationType Q4_K
}
```

### Parallel Evolution
```powershell
1..5 | ForEach-Object -Parallel {
    pwsh -File ".\scripts\model_self_digest.ps1" -Operation evolve -SourceModel "base.gguf" -Generations 5
} -ThrottleLimit 3
```

### Conditional Building
```powershell
if (-not (Test-Path "model.gguf")) {
    .\scripts\model_maker_zero_dep.ps1 -Operation build-from-scratch -ModelSize 7B -TargetSize 2.0
}
```

---

**Remember:** No external dependencies required. Pure PowerShell implementation.

**Everything runs locally. Everything is self-contained. Everything is independent.**
