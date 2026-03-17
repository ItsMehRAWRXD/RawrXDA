# A/B Testing - Quick Start Reference

## Instant A/B Test (30 seconds to results)

```powershell
# Quick test with default models (mistral vs neural-chat)
cd e:\
.\A-B-Testing-RealTime-Models.ps1 -NumTests 3
```

## Common Commands

### PowerShell - Comprehensive Testing
```powershell
# Test 2 models with 5 tests each
.\A-B-Testing-RealTime-Models.ps1 `
    -ModelA "mistral:latest" `
    -ModelB "neural-chat:latest" `
    -NumTests 5

# Test with longer timeout
.\A-B-Testing-RealTime-Models.ps1 `
    -ModelA "bigdaddyg:latest" `
    -ModelB "mistral:latest" `
    -NumTests 10 `
    -Timeout 180

# Save results to specific file
.\A-B-Testing-RealTime-Models.ps1 `
    -ModelA "model1:latest" `
    -ModelB "model2:latest" `
    -OutputFile "my-results.json"
```

### PowerShell - Real-Time Streaming
```powershell
# Stream 10 iterations of single prompt
.\A-B-Testing-RealTime-Streaming.ps1 `
    -ModelA "mistral:latest" `
    -ModelB "neural-chat:latest" `
    -TestPrompt "What is machine learning?" `
    -NumIterations 10

# Export to CSV for analysis
.\A-B-Testing-RealTime-Streaming.ps1 `
    -NumIterations 20 `
    -OutputFile "results.csv"
```

### Python - Statistical Analysis
```bash
# Full statistical comparison (includes t-test)
python ab-testing-real-models.py

# Compare specific models
python ab-testing-real-models.py \
    --model-a "mistral:latest" \
    --model-b "neural-chat:latest" \
    --tests 10

# Save to JSON with full results
python ab-testing-real-models.py \
    --model-a "model1" \
    --model-b "model2" \
    --tests 15 \
    --output "results.json"
```

## What Gets Measured

| Metric | What it measures | Good = |
|--------|------------------|--------|
| **Latency (ms)** | Time to get response | Lower is better |
| **Throughput (tokens/sec)** | Generation speed | Higher is better |
| **Success Rate (%)** | Requests that worked | 100% is best |
| **Consistency (Std Dev)** | How stable performance is | Lower variation is better |

## Reading the Output

### Example Output
```
✓ Test #1 (Factual-1): 245.32ms | 15.42 tokens/sec
✓ Test #2 (Code-1): 1523.44ms | 12.18 tokens/sec
```

- `✓` = Test succeeded
- `245.32ms` = Response latency (how fast)
- `15.42 tokens/sec` = Generation speed

### Summary Comparison
```
Model A: 523.45ms avg latency | 14.2 tokens/sec
Model B: 487.12ms avg latency | 15.8 tokens/sec

Winner: Model B (36ms faster, 1.6 tokens/sec faster)
```

## File Locations

```
e:\
├── A-B-Testing-RealTime-Models.ps1       ← Comprehensive testing
├── A-B-Testing-RealTime-Streaming.ps1    ← Real-time streaming
├── ab-testing-real-models.py              ← Statistical analysis
├── A-B-TESTING-GUIDE.md                   ← Full documentation
├── A-B-TESTING-QUICK-REFERENCE.md         ← This file
│
├── A-B-test-results-*.json                ← Results (created by scripts)
└── A-B-streaming-results-*.csv            ← CSV results (created by scripts)
```

## Model Names to Try

Common Ollama models:
- `mistral:latest` - Fast, good reasoning
- `neural-chat:latest` - Conversational
- `bigdaddyg:latest` - Large model
- `dolphin-mixtral:latest` - Expert model
- `orca-mini:latest` - Small model

Check available: `ollama list`

## Troubleshooting in 30 Seconds

| Problem | Solution |
|---------|----------|
| "No response from API" | Start Ollama: `ollama serve` |
| "ERROR: Timeout exceeded" | Increase timeout: `-Timeout 180` |
| "Model not found" | List models: `ollama list` |
| "Connection refused" | Check Ollama running on localhost:11434 |
| "Permission denied" | Run PowerShell as Administrator |

## Performance Tips

1. **First test slower?** - Model might be loading. Run again.
2. **Inconsistent results?** - Close other apps, run more tests
3. **Want faster results?** - Use `-NumTests 3` for quick check
4. **Want accurate results?** - Use `-NumTests 20` for statistical significance

## One-Liner Examples

```powershell
# Quick 3-test comparison
.\A-B-Testing-RealTime-Models.ps1 -NumTests 3

# Compare different models
.\A-B-Testing-RealTime-Models.ps1 -ModelA "mistral:latest" -ModelB "dolphin-mixtral:latest"

# Real-time dashboard for 15 iterations
.\A-B-Testing-RealTime-Streaming.ps1 -NumIterations 15

# Get full statistical analysis
python ab-testing-real-models.py --tests 20 --output results.json
```

## Next Steps

1. **Verify Ollama is running** - `ollama serve` in another terminal
2. **List available models** - `ollama list`
3. **Pick two models to compare**
4. **Run a quick 3-test trial** - `.\A-B-Testing-RealTime-Models.ps1 -NumTests 3`
5. **Review results** - Check console output
6. **Save detailed results** - Add `-OutputFile "myresults.json"`
7. **Analyze with Python** - Use `ab-testing-real-models.py` for statistics

## Understanding Statistics

### T-Test (Python output)
- **t > 2.0** = Difference is statistically significant
- **t < 2.0** = Difference could be random variance
- Higher |t| = More confident the models are different

### Confidence
- **Std Dev**: Lower = more consistent model
- **Min/Max Range**: Smaller = more predictable performance

## What to Compare

### For Production Decisions
- Use 20+ tests for each model
- Check both latency AND success rate
- Look for statistical significance
- Consider std dev (consistency matters!)

### For Quick Checks
- Use 5 tests per model
- Focus on average latency
- Good for spotting major differences

### For Detailed Analysis
- Use 30+ tests per model
- Run multiple times over days
- Compare statistical distributions
- Track trends over time

## Export and Share Results

```powershell
# Save results as JSON
.\A-B-Testing-RealTime-Models.ps1 -OutputFile "results-$(Get-Date -Format 'yyyy-MM-dd').json"

# Convert JSON to readable format
(Get-Content "results.json" | ConvertFrom-Json) | Format-List

# Export metrics only
$results = Get-Content "results.json" | ConvertFrom-Json
$results.MetricsA | Format-Table

# Compare metrics side-by-side
Write-Host "Model A: $($results.MetricsA.AvgLatencyMs)ms"
Write-Host "Model B: $($results.MetricsB.AvgLatencyMs)ms"
```

## Monitoring Trends

Collect results over time:
```powershell
# Run tests daily at 9 AM
$time = (Get-Date).AddDays(1).Date.AddHours(9)
$trigger = New-ScheduledTaskTrigger -At $time -RepeatInterval (New-TimeSpan -Days 1) -RepetitionDuration (New-TimeSpan -Days 30)
$action = New-ScheduledTaskAction -ScriptBlock { 
    cd e:\
    .\A-B-Testing-RealTime-Models.ps1 -OutputFile "daily\$(Get-Date -Format 'yyyy-MM-dd').json"
}
```

## Help and Docs

```powershell
# Get full help for PowerShell script
Get-Help .\A-B-Testing-RealTime-Models.ps1 -Full

# Get Python help
python ab-testing-real-models.py --help

# View full guide
Get-Content .\A-B-TESTING-GUIDE.md | more
```

---

**Location**: `e:\A-B-TESTING-QUICK-REFERENCE.md`
**Created**: December 5, 2025
**Updated**: December 5, 2025
