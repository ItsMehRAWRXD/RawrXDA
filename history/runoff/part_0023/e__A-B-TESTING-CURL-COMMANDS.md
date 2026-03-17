# A/B Testing - Raw curl Commands Reference

## Single Model Test (Manual)

```bash
# Basic test - model response only
curl -X POST http://localhost:11434/api/generate \
  -H "Content-Type: application/json" \
  -d '{"model":"mistral:latest","prompt":"What is AI?","stream":false}'

# Test with timing information
curl -s -w "\nTime: %{time_total}s\n" \
  -X POST http://localhost:11434/api/generate \
  -H "Content-Type: application/json" \
  -d '{"model":"mistral:latest","prompt":"What is AI?","stream":false}'
```

## Timing Breakdown curl

```bash
# Get detailed timing breakdown
curl -w "\nDNS:      %{time_namelookup}s
Connect:  %{time_connect}s
AppConn:  %{time_appconnect}s
Start:    %{time_starttransfer}s
Total:    %{time_total}s\n" \
  -X POST http://localhost:11434/api/generate \
  -H "Content-Type: application/json" \
  -d '{"model":"mistral:latest","prompt":"Test","stream":false}'
```

## Model A Test (Complete)

```bash
#!/bin/bash
# Full A/B test - Model A (Mistral)

MODEL_A="mistral:latest"
PROMPT="Explain machine learning in 2 sentences."

echo "Testing Model A: $MODEL_A"
echo "Prompt: $PROMPT"
echo ""

curl -s -w "\nResponse Time: %{time_total}s\nHTTP Code: %{http_code}\n" \
  -X POST http://localhost:11434/api/generate \
  -H "Content-Type: application/json" \
  -d "{\"model\":\"$MODEL_A\",\"prompt\":\"$PROMPT\",\"stream\":false}"
```

## Model B Test (Complete)

```bash
#!/bin/bash
# Full A/B test - Model B (Neural-Chat)

MODEL_B="neural-chat:latest"
PROMPT="Explain machine learning in 2 sentences."

echo "Testing Model B: $MODEL_B"
echo "Prompt: $PROMPT"
echo ""

curl -s -w "\nResponse Time: %{time_total}s\nHTTP Code: %{http_code}\n" \
  -X POST http://localhost:11434/api/generate \
  -H "Content-Type: application/json" \
  -d "{\"model\":\"$MODEL_B\",\"prompt\":\"$PROMPT\",\"stream\":false}"
```

## PowerShell curl Commands

### Single Model Test

```powershell
# Test Model A
$payload = @{
    model = "mistral:latest"
    prompt = "What is AI?"
    stream = $false
} | ConvertTo-Json

$result = curl.exe -s -w "`nTime: %{time_total}s" `
    -X POST "http://localhost:11434/api/generate" `
    -H "Content-Type: application/json" `
    -d $payload

$result
```

### Complete A/B Comparison (PowerShell Manual)

```powershell
# A/B Test - Manual curl method

$ModelA = "mistral:latest"
$ModelB = "neural-chat:latest"
$Prompt = "Explain quantum computing in one sentence."

# Test Model A
Write-Host "Testing Model A: $ModelA" -ForegroundColor Cyan
$payloadA = @{model = $ModelA; prompt = $Prompt; stream = $false} | ConvertTo-Json
$startA = Get-Date
$resultA = & curl.exe -s -w "`n%{time_total}" -X POST "http://localhost:11434/api/generate" `
    -H "Content-Type: application/json" -d $payloadA
$timeA = (Get-Date) - $startA
Write-Host "Result A: $($resultA | Select-Object -First 1)" -ForegroundColor Green
Write-Host "Time: $($timeA.TotalMilliseconds)ms" -ForegroundColor Green

# Small delay
Start-Sleep -Seconds 1

# Test Model B
Write-Host "`nTesting Model B: $ModelB" -ForegroundColor Magenta
$payloadB = @{model = $ModelB; prompt = $Prompt; stream = $false} | ConvertTo-Json
$startB = Get-Date
$resultB = & curl.exe -s -w "`n%{time_total}" -X POST "http://localhost:11434/api/generate" `
    -H "Content-Type: application/json" -d $payloadB
$timeB = (Get-Date) - $startB
Write-Host "Result B: $($resultB | Select-Object -First 1)" -ForegroundColor Green
Write-Host "Time: $($timeB.TotalMilliseconds)ms" -ForegroundColor Green

# Compare
Write-Host "`nComparison:" -ForegroundColor Yellow
Write-Host "Model A: $($timeA.TotalMilliseconds)ms" -ForegroundColor Cyan
Write-Host "Model B: $($timeB.TotalMilliseconds)ms" -ForegroundColor Magenta
$diff = $timeA.TotalMilliseconds - $timeB.TotalMilliseconds
Write-Host "Difference: $([Math]::Abs($diff))ms ($( if ($diff -gt 0) { 'B is faster' } else { 'A is faster' }))" -ForegroundColor Green
```

## Stress Testing - Multiple Rapid Tests

```powershell
# Run same prompt 10 times rapidly with curl
$Model = "mistral:latest"
$Prompt = "Say hello"
$times = @()

for ($i = 1; $i -le 10; $i++) {
    $payload = @{model = $Model; prompt = $Prompt; stream = $false} | ConvertTo-Json
    $start = Get-Date
    $result = & curl.exe -s -X POST "http://localhost:11434/api/generate" `
        -H "Content-Type: application/json" -d $payload -m 60
    $elapsed = (Get-Date - $start).TotalMilliseconds
    $times += $elapsed
    Write-Host "Request $i : ${elapsed}ms"
    Start-Sleep -Milliseconds 500
}

$avg = ($times | Measure-Object -Average).Average
Write-Host "`nAverage: $($avg)ms"
```

## Batch Testing - CSV Output

```powershell
# Generate CSV of multiple model tests
$models = @("mistral:latest", "neural-chat:latest", "dolphin-mixtral:latest")
$prompt = "Test prompt"
$results = @()

foreach ($model in $models) {
    for ($i = 1; $i -le 5; $i++) {
        $payload = @{model = $model; prompt = $prompt; stream = $false} | ConvertTo-Json
        $start = Get-Date
        $result = & curl.exe -s -X POST "http://localhost:11434/api/generate" `
            -H "Content-Type: application/json" -d $payload
        $elapsed = (Get-Date - $start).TotalMilliseconds
        
        $results += [PSCustomObject]@{
            Model = $model
            Test = $i
            LatencyMs = [Math]::Round($elapsed, 2)
            Timestamp = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
        }
        
        Start-Sleep -Milliseconds 300
    }
}

$results | Export-Csv "batch_results.csv" -NoTypeInformation
$results | Format-Table
```

## Advanced - Concurrent Testing (Parallel)

```powershell
# Run two models in parallel using background jobs
$ModelA = "mistral:latest"
$ModelB = "neural-chat:latest"
$Prompt = "What is machine learning?"

$JobA = Start-Job -ScriptBlock {
    param($Model, $Prompt)
    $payload = @{model = $Model; prompt = $Prompt; stream = $false} | ConvertTo-Json
    $start = Get-Date
    $result = & curl.exe -s -X POST "http://localhost:11434/api/generate" `
        -H "Content-Type: application/json" -d $payload
    @{
        Model = $Model
        Time = ((Get-Date) - $start).TotalMilliseconds
        Success = $?
    }
} -ArgumentList $ModelA, $Prompt

$JobB = Start-Job -ScriptBlock {
    param($Model, $Prompt)
    $payload = @{model = $Model; prompt = $Prompt; stream = $false} | ConvertTo-Json
    $start = Get-Date
    $result = & curl.exe -s -X POST "http://localhost:11434/api/generate" `
        -H "Content-Type: application/json" -d $payload
    @{
        Model = $Model
        Time = ((Get-Date) - $start).TotalMilliseconds
        Success = $?
    }
} -ArgumentList $ModelB, $Prompt

# Wait for both to complete
$ResultA = Receive-Job -Job $JobA -Wait
$ResultB = Receive-Job -Job $JobB -Wait

Write-Host "Model A: $($ResultA.Time)ms" -ForegroundColor Cyan
Write-Host "Model B: $($ResultB.Time)ms" -ForegroundColor Magenta
```

## Error Handling - curl with Retries

```powershell
# Test with automatic retry on failure
function Test-ModelWithRetry {
    param(
        [string]$Model,
        [string]$Prompt,
        [int]$MaxRetries = 3
    )
    
    $attempt = 0
    while ($attempt -lt $MaxRetries) {
        try {
            $payload = @{model = $Model; prompt = $Prompt; stream = $false} | ConvertTo-Json
            $start = Get-Date
            
            $result = & curl.exe -s -f -X POST "http://localhost:11434/api/generate" `
                -H "Content-Type: application/json" `
                -d $payload `
                --max-time 120 `
                2>&1
            
            $elapsed = (Get-Date - $start).TotalMilliseconds
            
            if ($LASTEXITCODE -eq 0) {
                return @{
                    Success = $true
                    Time = $elapsed
                    Data = $result
                }
            }
        } catch {
            Write-Host "Attempt $($attempt + 1) failed, retrying..." -ForegroundColor Yellow
            Start-Sleep -Seconds 2
        }
        
        $attempt++
    }
    
    return @{
        Success = $false
        Error = "Max retries exceeded"
    }
}

$result = Test-ModelWithRetry -Model "mistral:latest" -Prompt "Test"
Write-Host "Result: $result"
```

## Performance - Throughput Test

```powershell
# Measure tokens generated and throughput
function Measure-Throughput {
    param(
        [string]$Model,
        [string]$Prompt,
        [int]$Iterations = 5
    )
    
    $times = @()
    $tokenCounts = @()
    
    for ($i = 0; $i -lt $Iterations; $i++) {
        $payload = @{model = $Model; prompt = $Prompt; stream = $false} | ConvertTo-Json
        $start = Get-Date
        
        $result = & curl.exe -s -X POST "http://localhost:11434/api/generate" `
            -H "Content-Type: application/json" -d $payload
        
        $elapsed = (Get-Date - $start).TotalMilliseconds
        
        try {
            $json = $result | ConvertFrom-Json
            $responseLength = $json.response.Length
            $tokenCount = [Math]::Ceiling($responseLength / 4)
            
            $times += $elapsed
            $tokenCounts += $tokenCount
            
            $tps = $tokenCount / ($elapsed / 1000)
            Write-Host "Iteration $($i+1): $elapsed ms | $tokenCount tokens | $($tps) tokens/sec"
        } catch {
            Write-Host "Iteration $($i+1): Error parsing response"
        }
        
        Start-Sleep -Milliseconds 500
    }
    
    $avgTime = ($times | Measure-Object -Average).Average
    $avgTokens = ($tokenCounts | Measure-Object -Average).Average
    $avgTps = $avgTokens / ($avgTime / 1000)
    
    Write-Host "`nAverage: $avgTime ms per request"
    Write-Host "Average: $avgTokens tokens per response"
    Write-Host "Throughput: $avgTps tokens/sec"
}

Measure-Throughput -Model "mistral:latest" -Prompt "Tell me a story" -Iterations 5
```

## Streaming Response Test

```powershell
# Test streaming mode (for real-time latency to first token)
function Test-Streaming {
    param(
        [string]$Model,
        [string]$Prompt
    )
    
    $payload = @{model = $Model; prompt = $Prompt; stream = $true} | ConvertTo-Json
    
    Write-Host "Streaming test for: $Model" -ForegroundColor Cyan
    $start = Get-Date
    
    $result = & curl.exe -s -N -X POST "http://localhost:11434/api/generate" `
        -H "Content-Type: application/json" `
        -d $payload
    
    $firstToken = Get-Date
    
    Write-Host "Time to first token: $(($firstToken - $start).TotalMilliseconds)ms" -ForegroundColor Green
    Write-Host "Response preview: $($result | Select-Object -First 200)"
}

Test-Streaming -Model "mistral:latest" -Prompt "Write a poem"
```

## Batch JSON Request

```powershell
# Create batch file of test requests
$batch = @(
    @{model = "mistral:latest"; prompt = "Test 1"},
    @{model = "neural-chat:latest"; prompt = "Test 2"},
    @{model = "mistral:latest"; prompt = "Test 3"}
) | ConvertTo-Json -AsArray

$batch | Set-Content "test_batch.json"

# Process batch
$requests = Get-Content "test_batch.json" | ConvertFrom-Json

foreach ($req in $requests) {
    $payload = @{model = $req.model; prompt = $req.prompt; stream = $false} | ConvertTo-Json
    Write-Host "Testing: $($req.model) with '$($req.prompt)'" -ForegroundColor Yellow
    
    $start = Get-Date
    $result = & curl.exe -s -X POST "http://localhost:11434/api/generate" `
        -H "Content-Type: application/json" -d $payload
    $elapsed = (Get-Date - $start).TotalMilliseconds
    
    Write-Host "Result: ${elapsed}ms`n" -ForegroundColor Green
}
```

## Check API Health Before Testing

```powershell
# Verify API is available and models are loaded
Write-Host "Checking API health..." -ForegroundColor Yellow

# Check connectivity
$healthCheck = curl.exe -s -w "%{http_code}" -X GET http://localhost:11434/api/tags

if ($healthCheck -match "200") {
    Write-Host "✓ API is responding" -ForegroundColor Green
} else {
    Write-Host "✗ API is not responding (code: $healthCheck)" -ForegroundColor Red
    exit 1
}

# Get available models
$models = curl.exe -s -X GET http://localhost:11434/api/tags | ConvertFrom-Json
Write-Host "Available models: $($models.models.Count)" -ForegroundColor Green
$models.models | ForEach-Object { Write-Host "  - $($_.name)" -ForegroundColor Cyan }
```

---

**Tips**:
- Use `-s` for silent mode (no progress bar)
- Use `-w "\n%{time_total}"` to append timing
- Use `-X POST` for POST requests
- Use `-H "Content-Type: application/json"` for JSON
- Use `-d` to specify JSON data
- Use `--max-time` for request timeout
- Use `-N` for streaming (no buffering)
- Use `-m` as alternative to `--max-time`

**For PowerShell**: Always use `& curl.exe` to explicitly call the external curl, not any PowerShell equivalent

Created: December 5, 2025
