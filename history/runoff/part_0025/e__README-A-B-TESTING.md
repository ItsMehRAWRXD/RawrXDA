# A/B Testing Framework - File Index & Getting Started

## 🚀 Start Here (30 Seconds)

### Option 1: Quick Launch
```powershell
cd e:\
.\A-B-Testing-Launcher.ps1 -Mode quick
```

### Option 2: Manual Start
```powershell
cd e:\
.\A-B-Testing-RealTime-Models.ps1 -NumTests 3
```

### Option 3: View Documentation
```powershell
Get-Content A-B-TESTING-QUICK-REFERENCE.md | more
```

---

## 📁 Complete File Structure

### Executable Scripts

| File | Purpose | Runtime | Best For |
|------|---------|---------|----------|
| `A-B-Testing-Launcher.ps1` | Entry point menu | 30 sec | Starting out, new users |
| `A-B-Testing-RealTime-Models.ps1` | Comprehensive testing | 3-5 min | Production comparisons |
| `A-B-Testing-RealTime-Streaming.ps1` | Real-time streaming | 2-3 min | Quick checks, trends |
| `ab-testing-real-models.py` | Statistical analysis | 3-5 min | Research, t-tests |

### Documentation Files

| File | Content | Read Time | When to Use |
|------|---------|-----------|-------------|
| `A-B-TESTING-QUICK-REFERENCE.md` | One-liners, quick setup | 5 min | Quick start, troubleshooting |
| `A-B-TESTING-GUIDE.md` | Complete guide | 15 min | Learning the framework |
| `A-B-TESTING-CURL-COMMANDS.md` | Raw curl examples | 10 min | Manual testing, debugging |
| `A-B-Testing-Framework-Summary.md` | Architecture overview | 10 min | Understanding design |
| `README-A-B-TESTING.md` | This file | 5 min | Navigation & overview |

---

## 🎯 Quick Decision Tree

### I want to...

**...compare two models in 2 minutes**
```powershell
.\A-B-Testing-Launcher.ps1 -Mode quick
```
→ See `A-B-TESTING-QUICK-REFERENCE.md`

**...do a complete production comparison**
```powershell
.\A-B-Testing-RealTime-Models.ps1 -NumTests 10 -OutputFile "results.json"
```
→ See `A-B-TESTING-GUIDE.md`

**...monitor real-time performance**
```powershell
.\A-B-Testing-RealTime-Streaming.ps1 -NumIterations 30
```
→ See `A-B-TESTING-QUICK-REFERENCE.md`

**...do statistical analysis**
```bash
python ab-testing-real-models.py --tests 20 --output results.json
```
→ See `A-B-TESTING-GUIDE.md`

**...test manually with curl**
```bash
curl -X POST http://localhost:11434/api/generate ...
```
→ See `A-B-TESTING-CURL-COMMANDS.md`

**...understand how it works**
→ See `A-B-Testing-Framework-Summary.md`

---

## 📊 What You Get

Each test produces:

### Console Output
- Real-time progress display
- Color-coded results
- Summary statistics
- Winner determination

### JSON Output (Optional)
```json
{
  "MetricsA": { "AvgLatencyMs": 523.45, ... },
  "MetricsB": { "AvgLatencyMs": 487.12, ... },
  "ComparisonMetrics": { "Winner": "Model B", ... },
  "ResultsA": [ {...}, {...} ],
  "ResultsB": [ {...}, {...} ]
}
```

### CSV Output (Streaming)
- Time-series data
- Trend analysis ready
- Importable to Excel/R

---

## 🔧 Common Commands

### PowerShell

```powershell
# Quick test
.\A-B-Testing-Launcher.ps1 -Mode quick

# Compare specific models
.\A-B-Testing-RealTime-Models.ps1 `
    -ModelA "mistral:latest" `
    -ModelB "neural-chat:latest" `
    -NumTests 10

# Save results
.\A-B-Testing-RealTime-Models.ps1 `
    -OutputFile "test-$(Get-Date -Format 'yyyy-MM-dd').json"

# Stream mode
.\A-B-Testing-RealTime-Streaming.ps1 -NumIterations 20

# Verbose output
.\A-B-Testing-RealTime-Models.ps1 -Verbose
```

### Python

```bash
# Quick test
python ab-testing-real-models.py

# Custom models
python ab-testing-real-models.py \
    --model-a "mistral:latest" \
    --model-b "neural-chat:latest" \
    --tests 10

# Save results
python ab-testing-real-models.py \
    --output "results.json"

# Statistical analysis
python ab-testing-real-models.py \
    --tests 20 \
    --output "stats.json"
```

### Raw curl

```bash
# Single test
curl -s -X POST http://localhost:11434/api/generate \
  -H "Content-Type: application/json" \
  -d '{"model":"mistral:latest","prompt":"Test","stream":false}'

# With timing
curl -s -w "\nTime: %{time_total}s\n" \
  -X POST http://localhost:11434/api/generate \
  -H "Content-Type: application/json" \
  -d '{"model":"mistral:latest","prompt":"Test","stream":false}'
```

---

## 📈 Metrics Guide

| Metric | Measured | Unit | Lower Better? |
|--------|----------|------|---------------|
| Latency | Response time | ms | ✓ Yes |
| Throughput | Generation speed | tokens/sec | ✗ No (higher=better) |
| Success Rate | Requests succeeded | % | ✗ No (higher=better) |
| Std Dev | Consistency | ms | ✓ Yes (lower=more consistent) |
| T-Test | Statistical difference | score | - |

---

## 🚨 Troubleshooting

### "No response from API"
```powershell
# Make sure Ollama is running
ollama serve

# Check it's accessible
curl http://localhost:11434/api/tags
```

### "Model not found"
```powershell
# List available models
ollama list

# Pull a model
ollama pull mistral:latest
```

### "Permission denied" (PowerShell)
```powershell
# Run as Administrator, then:
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### "Timeout exceeded"
```powershell
# Use longer timeout
.\A-B-Testing-RealTime-Models.ps1 -Timeout 180
```

### Python not found
```bash
# Install from https://www.python.org/
# Or use WSL: wsl python ab-testing-real-models.py
```

---

## 📚 Documentation Guide

### For Quick Start
**Read**: `A-B-TESTING-QUICK-REFERENCE.md` (5 min)
- Common commands
- One-liners
- Quick troubleshooting
- File locations

### For Full Understanding
**Read**: `A-B-TESTING-GUIDE.md` (15 min)
- Complete usage guide
- All parameters explained
- Advanced scenarios
- Best practices
- CI/CD integration

### For Manual Testing
**Read**: `A-B-TESTING-CURL-COMMANDS.md` (10 min)
- Raw curl commands
- PowerShell examples
- Stress testing
- Batch processing
- Error handling

### For Architecture
**Read**: `A-B-Testing-Framework-Summary.md` (10 min)
- Design overview
- Data flow
- Metrics calculation
- Performance characteristics

---

## ⚡ Performance Tips

1. **First test slow?** - Model is loading, run again
2. **Inconsistent results?** - Run more tests (-NumTests 20)
3. **Want faster?** - Use quick mode (-NumTests 3)
4. **Want accurate?** - Use more tests (-NumTests 30)
5. **High variance?** - Close background apps, try again

---

## 🎓 Learning Path

### Beginner (15 minutes)
1. Read: `A-B-TESTING-QUICK-REFERENCE.md`
2. Run: `.\A-B-Testing-Launcher.ps1 -Mode quick`
3. Review console output
4. Done! You know how to run tests

### Intermediate (30 minutes)
1. Read: `A-B-TESTING-GUIDE.md` sections 1-3
2. Run: `.\A-B-Testing-RealTime-Models.ps1 -NumTests 10 -Verbose`
3. Save results: Add `-OutputFile "test.json"`
4. Explore results: `Get-Content "test.json" | ConvertFrom-Json | Format-List`

### Advanced (1 hour)
1. Read: Full `A-B-TESTING-GUIDE.md`
2. Run: `python ab-testing-real-models.py --tests 20 --output results.json`
3. Analyze statistical output
4. Read: `A-B-Testing-Framework-Summary.md`

---

## 📋 Use Case Examples

### Scenario 1: Quick Model Check
```powershell
# "Which is faster: model A or B?"
.\A-B-Testing-Launcher.ps1 -Mode quick
# Answer in 2 minutes
```

### Scenario 2: Production Deployment
```powershell
# "Is the new model good enough?"
.\A-B-Testing-RealTime-Models.ps1 `
    -ModelA "current:latest" `
    -ModelB "candidate:latest" `
    -NumTests 20 `
    -OutputFile "deployment-check.json"
# Answer with statistical confidence in 10 minutes
```

### Scenario 3: Performance Monitoring
```powershell
# "How has model performance changed?"
for ($i=0; $i -lt 5; $i++) {
    .\A-B-Testing-Launcher.ps1 -Mode streaming
    Get-Date
}
# Trend analysis over multiple runs
```

### Scenario 4: Research & Analysis
```bash
# "What's the statistical significance of the difference?"
python ab-testing-real-models.py --tests 50 --output research.json
# T-test results and confidence intervals
```

---

## 🔍 Key Concepts

### Latency
- How long it takes to get a response
- Lower is better
- Measured in milliseconds

### Throughput
- How fast the model generates tokens
- Higher is better
- Measured in tokens/second

### Consistency
- How stable performance is
- Lower standard deviation = more consistent
- Important for production systems

### Statistical Significance
- Whether observed difference is real (not random)
- T-test score > 2.0 = significant
- Important for deciding on deployments

---

## 🤔 FAQ

**Q: How many tests should I run?**
A: 
- Quick check: 3 tests
- Regular comparison: 10 tests
- Production decision: 20+ tests
- Research: 50+ tests

**Q: Why are results different each time?**
A: Models and systems have natural variance. Run multiple tests and look at averages and trends.

**Q: Can I test 3 models?**
A: Yes! Run A vs B, then B vs C, then A vs C. Or modify the scripts.

**Q: What models should I test?**
A: Any Ollama model. Check available: `ollama list`

**Q: Can I use custom prompts?**
A: Yes! Edit the `$testPrompts` array in the PowerShell script.

**Q: How do I schedule automatic testing?**
A: Use Windows Task Scheduler. See `A-B-TESTING-GUIDE.md` for examples.

**Q: Can I use this on Linux?**
A: Yes! Use the Python script or raw curl commands. PowerShell scripts also work with PowerShell Core.

---

## 📞 Support

### Getting Help
1. Check Quick Reference: `A-B-TESTING-QUICK-REFERENCE.md`
2. Check Full Guide: `A-B-TESTING-GUIDE.md`
3. Check Curl Reference: `A-B-TESTING-CURL-COMMANDS.md`

### Debugging
```powershell
# Enable verbose output
.\A-B-Testing-RealTime-Models.ps1 -Verbose

# Check API status
curl http://localhost:11434/api/tags
```

---

## ✅ Checklist: Getting Started

- [ ] Read this file (README)
- [ ] Ensure Ollama is running (`ollama serve`)
- [ ] Check models available (`ollama list`)
- [ ] Run quick test (`.\A-B-Testing-Launcher.ps1 -Mode quick`)
- [ ] Review results
- [ ] Read `A-B-TESTING-QUICK-REFERENCE.md`
- [ ] Try full test (`.\A-B-Testing-RealTime-Models.ps1`)
- [ ] Save results to JSON
- [ ] Explore documentation as needed

---

## 🎯 Next Steps

1. **Right Now**: Run `.\A-B-Testing-Launcher.ps1 -Mode quick`
2. **Next**: Review output, read `A-B-TESTING-QUICK-REFERENCE.md`
3. **Then**: Try full test with `-OutputFile "results.json"`
4. **Later**: Explore Python version for statistical analysis
5. **Finally**: Integrate into your CI/CD pipeline

---

**Location**: `e:\README-A-B-TESTING.md`
**Created**: December 5, 2025
**Framework Version**: 1.0

All files ready in `e:\`
