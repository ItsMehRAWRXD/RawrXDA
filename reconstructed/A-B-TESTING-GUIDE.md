# A/B Testing Framework - Real-Time Model Comparison Using curl

## Overview

This comprehensive A/B testing framework allows you to compare the performance and behavior of two AI models in real-time using curl. The framework provides:

- **Real-time latency measurement** - microsecond precision timing using curl
- **Throughput analysis** - tokens per second calculation
- **Statistical comparison** - t-tests and confidence metrics
- **Comprehensive reporting** - detailed side-by-side analysis
- **Multiple implementations** - PowerShell, Python, and raw curl examples

## Framework Components

### 1. PowerShell: Comprehensive A/B Testing (`A-B-Testing-RealTime-Models.ps1`)

**Purpose**: Full-featured A/B testing with 10 different test categories

**Features**:
- 10 test prompts across different domains (factual, code, creative, reasoning, etc.)
- Detailed metrics collection (latency, throughput, success rate)
- Statistical aggregation (mean, median, std dev, min/max)
- JSON output for further analysis
- Color-coded console reporting

**Usage**:
```powershell
# Basic usage (default: mistral vs neural-chat)
.\A-B-Testing-RealTime-Models.ps1

# Compare specific models with custom parameters
.\A-B-Testing-RealTime-Models.ps1 `
    -ModelA "bigdaddyg:latest" `
    -ModelB "mistral:latest" `
    -NumTests 5 `
    -Timeout 60 `
    -Verbose `
    -OutputFile "e:\my-ab-test-results.json"

# Quick 3-test comparison
.\A-B-Testing-RealTime-Models.ps1 -NumTests 3
```

**Parameters**:
- `-ModelA` (string): Model identifier for test group A (default: "mistral:latest")
- `-ModelB` (string): Model identifier for test group B (default: "neural-chat:latest")
- `-NumTests` (int): Number of tests to run per model (default: 10)
- `-Timeout` (int): Timeout per request in seconds (default: 120)
- `-Verbose` (switch): Enable verbose output
- `-IncludeQuality` (switch): Include quality metrics (WIP)
- `-OutputFile` (string): Path to save JSON results

**Output Example**:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TESTING MODEL A : mistral:latest
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  ✓ Test #1 (Factual-1): 245.32ms | 15.42 tokens/sec
  ✓ Test #2 (Code-1): 1523.44ms | 12.18 tokens/sec
  ...

📊 SUCCESS RATE
  Model A: 100.0% (10/10)
  Model B: 100.0% (10/10)

⏱️  LATENCY METRICS (milliseconds)
  ┌─ Average:
  │  Model A: 523.45ms
  │  Model B: 487.12ms
  │  Difference: 36.33ms (Model B is FASTER)
```

### 2. PowerShell: Real-Time Streaming (`A-B-Testing-RealTime-Streaming.ps1`)

**Purpose**: Quick real-time comparison of two models on a single prompt

**Features**:
- Parallel execution simulation (sequential for clarity)
- Live dashboard output
- Real-time latency comparison
- CSV export for trend analysis
- Minimal overhead

**Usage**:
```powershell
# Test with default prompt
.\A-B-Testing-RealTime-Streaming.ps1 -NumIterations 5

# Test with custom prompt and models
.\A-B-Testing-RealTime-Streaming.ps1 `
    -ModelA "mistral:latest" `
    -ModelB "bigdaddyg:latest" `
    -TestPrompt "Explain quantum computing in 2 sentences" `
    -NumIterations 10 `
    -OutputFile "e:\streaming-results.csv"
```

**Parameters**:
- `-ModelA` (string): Model A identifier
- `-ModelB` (string): Model B identifier
- `-TestPrompt` (string): Single prompt to repeat across iterations
- `-NumIterations` (int): Number of iterations (default: 5)
- `-Timeout` (int): Request timeout in seconds (default: 120)
- `-OutputFile` (string): CSV output path

### 3. Python: Advanced Statistical Analysis (`ab-testing-real-models.py`)

**Purpose**: Statistical analysis with t-tests and confidence intervals

**Features**:
- Subprocess-based curl execution (no dependencies)
- Welch's t-test for significance testing
- Comprehensive statistical output
- JSON output with full result history
- Cross-platform compatibility

**Usage**:
```bash
# Basic comparison
python ab-testing-real-models.py

# Compare specific models with statistical analysis
python ab-testing-real-models.py \
    --model-a "mistral:latest" \
    --model-b "neural-chat:latest" \
    --tests 10 \
    --timeout 120 \
    --output "results.json"

# Quick 5-test run
python ab-testing-real-models.py --tests 5
```

**Command-line Options**:
- `--model-a` (string): Model A identifier (default: "mistral:latest")
- `--model-b` (string): Model B identifier (default: "neural-chat:latest")
- `--tests` (int): Number of tests per model (default: 10)
- `--timeout` (int): Timeout per request (default: 120)
- `--api` (string): API base URL (default: "http://localhost:11434")
- `--output` (string): JSON output file path

## Test Categories

The framework includes 10 test prompts covering diverse domains:

1. **Factual** - Knowledge retrieval (e.g., "What is the capital of France?")
2. **Code** - Programming tasks (e.g., "Write a Python factorial function")
3. **Creative** - Creative writing (e.g., "Write a haiku about AI")
4. **Reasoning** - Logical thinking (e.g., Machine problems)
5. **Summarization** - Text condensing
6. **Instructions** - Task sequencing
7. **Comparative** - Difference analysis
8. **Math** - Numerical computation

## Metrics Explained

### Latency Metrics
- **Mean**: Average response time across all tests
- **Median**: Middle value (50th percentile)
- **Min/Max**: Fastest and slowest responses
- **Std Dev**: Variability measure (lower = more consistent)
- **Range**: Difference between max and min

### Throughput Metrics
- **Tokens/sec**: Generation speed indicator
- **Average**: Mean throughput across tests
- **Peak**: Maximum throughput achieved

### Success Metrics
- **Success Rate**: Percentage of successful requests
- **Failed Tests**: Number of timeouts or errors

### Statistical Metrics
- **T-Statistic**: Measures difference between models (|t| > 2.0 = significant)
- **Standard Error**: Confidence in the measurement

## Real-Time Measurement Details

### How curl is Used

The framework uses curl with timing hooks to measure real API latency:

```bash
curl -s \
    -w "\n%{time_total}" \
    -X POST "http://localhost:11434/api/generate" \
    -H "Content-Type: application/json" \
    -d '{"model":"mistral:latest","prompt":"...","stream":false}' \
    --max-time 120
```

**Key Parameters**:
- `-s`: Silent mode (no progress bar)
- `-w "\n%{time_total}"`: Append timing in seconds
- `-X POST`: Use POST method
- `-H`: Set content-type header
- `-d`: JSON payload
- `--max-time`: Request timeout

### Timing Breakdown

The response includes:
1. **Full response JSON** - Model output data
2. **Total time** - End of output (curl timing)

Latency is calculated as: `response_time - request_start_time`

## Output Files

### JSON Output Format

```json
{
  "TestMetadata": {
    "StartTime": "2025-12-05T14:32:10.1234567",
    "EndTime": "2025-12-05T14:35:45.5678901",
    "DurationSeconds": 195.43,
    "ModelA": "mistral:latest",
    "ModelB": "neural-chat:latest",
    "TestsPerModel": 10
  },
  "MetricsA": {
    "SuccessRate": 100.0,
    "AvgLatencyMs": 523.45,
    "AvgTokensPerSec": 14.2,
    ...
  },
  "MetricsB": {...},
  "ResultsA": [
    {
      "Model": "mistral:latest",
      "TestNum": 1,
      "Success": true,
      "LatencyMs": 245.32,
      "TokensPerSecond": 15.42,
      ...
    }
  ],
  "ResultsB": [...]
}
```

### CSV Output Format (Streaming)

```csv
Model,Iteration,Success,LatencyMs,TokensPerSec,Timestamp
mistral:latest,1,True,245.32,15.42,2025-12-05T14:32:10
neural-chat:latest,1,True,312.14,12.18,2025-12-05T14:32:15
...
```

## Usage Scenarios

### Scenario 1: Quick Model Comparison

Test two models 5 times each to see which is faster:

```powershell
.\A-B-Testing-RealTime-Models.ps1 -ModelA "model1:latest" -ModelB "model2:latest" -NumTests 5
```

**Time**: ~2-3 minutes
**Output**: Side-by-side latency and throughput comparison

### Scenario 2: Statistical Significance

Run 20 tests to determine if differences are statistically significant:

```bash
python ab-testing-real-models.py --model-a "model1" --model-b "model2" --tests 20 --output "results.json"
```

**Time**: ~10-15 minutes
**Output**: T-test results showing statistical significance

### Scenario 3: Real-Time Dashboard

Monitor a single prompt across 30 iterations to see consistency:

```powershell
.\A-B-Testing-RealTime-Streaming.ps1 `
    -TestPrompt "Your test prompt" `
    -NumIterations 30 `
    -OutputFile "dashboard.csv"
```

**Time**: ~5-10 minutes
**Output**: CSV file for plotting latency trends

### Scenario 4: Production Comparison

Full test suite before production deployment:

```powershell
.\A-B-Testing-RealTime-Models.ps1 `
    -ModelA "current-production:latest" `
    -ModelB "candidate:latest" `
    -NumTests 10 `
    -Timeout 120 `
    -OutputFile "production-ab-test-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
```

## Interpreting Results

### Example Results

**Model A (mistral:latest)**
```
Success Rate: 100.0% (10/10)
Avg Latency: 523.45ms
Min/Max: 245.12ms / 1203.45ms
Avg Throughput: 14.2 tokens/sec
Std Dev: 238.34ms
```

**Model B (neural-chat:latest)**
```
Success Rate: 100.0% (10/10)
Avg Latency: 487.12ms
Min/Max: 312.10ms / 892.34ms
Avg Throughput: 15.8 tokens/sec
Std Dev: 156.23ms
```

**Analysis**:
1. Both models have 100% success rate ✓
2. Model B is 36.33ms faster on average (6.9% improvement)
3. Model B has more consistent performance (lower std dev)
4. Model B generates 1.6 more tokens/sec on average
5. **Winner**: Model B for this workload

## Best Practices

1. **Run enough iterations**: At least 10 tests per model for statistical validity
2. **Use consistent prompts**: Same prompts for fair comparison
3. **Set appropriate timeout**: Allow enough time for responses
4. **Multiple runs**: Run A/B tests multiple times for variance
5. **Log results**: Save all outputs for historical comparison
6. **Consider categories**: Test across different prompt types
7. **Check API availability**: Ensure Ollama is running before tests
8. **Monitor system load**: Run tests when system is not heavily loaded

## Troubleshooting

### No Response from API
```
✗ Test #1: ERROR - No response from API
```
**Solution**: Verify Ollama is running: `ollama serve` or check `http://localhost:11434/api/tags`

### Timeout Errors
```
✗ Test #2: ERROR - Timeout exceeded
```
**Solution**: Increase timeout with `-Timeout 180` or check if model is available: `ollama list`

### JSON Parse Errors
```
✗ Test #3: ERROR - JSON parse failed
```
**Solution**: Verify model exists and can generate responses with a simple curl test

### Inconsistent Results
- Run more iterations (increase `-NumTests`)
- Check system load and close other applications
- Ensure model is fully loaded: run a test generation before A/B test

## Advanced Usage

### Custom Test Prompts

Edit the `$testPrompts` array in `A-B-Testing-RealTime-Models.ps1`:

```powershell
$testPrompts = @(
    @{
        Name = "CustomTest-1"
        Prompt = "Your custom prompt here"
        Category = "Custom"
    },
    # ... more tests
)
```

### Parallel Model Testing

Test 3+ models by running multiple scripts simultaneously in different terminal windows:

```powershell
# Terminal 1: Model A vs B
.\A-B-Testing-RealTime-Models.ps1 -ModelA "model1" -ModelB "model2" -OutputFile "ab-results.json"

# Terminal 2: Model B vs C
.\A-B-Testing-RealTime-Models.ps1 -ModelA "model2" -ModelB "model3" -OutputFile "bc-results.json"

# Terminal 3: Model A vs C
.\A-B-Testing-RealTime-Models.ps1 -ModelA "model1" -ModelB "model3" -OutputFile "ac-results.json"
```

### Continuous Monitoring

Create a scheduled task to run A/B tests hourly:

```powershell
# PowerShell
$trigger = New-ScheduledTaskTrigger -Hourly
$action = New-ScheduledTaskAction -ScriptBlock {
    & 'e:\A-B-Testing-RealTime-Models.ps1' -ModelA "model1" -ModelB "model2" `
        -OutputFile "e:\ab-tests\$(Get-Date -Format 'yyyyMMdd-HHmm').json"
}
Register-ScheduledTask -TaskName "HourlyABTest" -Trigger $trigger -Action $action
```

## Performance Tips

1. **Reduce number of tests** for quick comparisons (use `-NumTests 3`)
2. **Increase delays** between requests if seeing errors (modify `Start-Sleep`)
3. **Close other applications** to reduce system load
4. **Run during off-peak hours** for more consistent results
5. **Use consistent hardware state** (no updates, background tasks)

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Model Performance Check

on: [push]

jobs:
  ab-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Start Ollama
        run: ollama serve &
      - name: Run A/B Test
        run: |
          python ab-testing-real-models.py \
            --model-a "model1:latest" \
            --model-b "model2:latest" \
            --output "results.json"
      - name: Archive results
        uses: actions/upload-artifact@v2
        with:
          name: ab-test-results
          path: results.json
```

## Related Commands

```bash
# Check available models
curl http://localhost:11434/api/tags

# Single model test
curl -X POST http://localhost:11434/api/generate \
  -H "Content-Type: application/json" \
  -d '{"model":"mistral:latest","prompt":"Hello","stream":false}'

# Check API status
curl http://localhost:11434/api/status
```

## Support and Debugging

For detailed diagnostic information, enable verbose output:

```powershell
.\A-B-Testing-RealTime-Models.ps1 -Verbose
```

This will show:
- Full curl commands being executed
- Detailed timing information
- Response sizes and token estimates
- Full error messages

Created: December 5, 2025
