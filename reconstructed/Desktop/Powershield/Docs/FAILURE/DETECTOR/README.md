# Model Invocation Failure Detector

## Overview

The Model Invocation Failure Detector provides production-grade failure detection, circuit breaking, and automatic recovery for AI model invocations in RawrXD 3.0 PRO.

## Features

### Circuit Breaker Pattern
- **Closed State**: Normal operation - requests flow through
- **Open State**: Failing - requests are rejected immediately to prevent cascade failures
- **Half-Open State**: Testing - limited requests allowed to test recovery

### Failure Classification
The system automatically classifies failures into categories:
- `Timeout` - Request exceeded time limit
- `ConnectionError` - Network/connectivity issues
- `ModelNotFound` - Specified model doesn't exist
- `RateLimitExceeded` - API rate limiting (429)
- `InvalidResponse` - Malformed/unparseable response
- `OutOfMemory` - GPU/system memory exhaustion
- `GPUError` - CUDA/ROCm device errors
- `TokenLimitExceeded` - Context length exceeded
- `AuthenticationError` - Auth failures (401/403)
- `UnknownError` - Unclassified errors

### Automatic Recovery Strategies
Each failure type has a configured recovery action:
- **Timeout**: Increase timeout for next request
- **ConnectionError**: Retry with exponential backoff
- **RateLimitExceeded**: Wait and retry (60s default)
- **OutOfMemory**: Suggest using smaller model
- **GPUError**: Fall back to CPU execution
- **TokenLimitExceeded**: Truncate input

### Retry Executor
- Exponential backoff with configurable delays
- Respects circuit breaker state
- Maximum retry limit (default: 3)
- Backoff multiplier (default: 2.0x)

## Usage

### Basic Model Invocation
```powershell
$result = Invoke-ModelWithFailureDetection `
    -ModelName "llama-3.2" `
    -BackendType "LlamaCpp" `
    -Action { 
        # Your model invocation code here
        Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Body $payload
    }
```

### With Retry Logic
```powershell
$result = Invoke-ModelWithFailureDetection `
    -ModelName "llama-3.2" `
    -BackendType "Ollama" `
    -Action { Invoke-OllamaGenerate -Prompt "Hello" } `
    -EnableRetry `
    -MaxRetries 3
```

### Check Model Health
```powershell
# Overall health status
$health = Get-ModelHealth
$health.OverallHealth  # Healthy, Degraded, or Recovering

# Individual model status
$health.Models | Format-Table Name, State, SuccessRate

# Recent failures
Get-RecentModelFailures -Count 10
```

### Reset Circuit Breakers
```powershell
# Reset specific model
Reset-ModelCircuitBreaker -ModelName "llama-3.2" -BackendType "LlamaCpp"

# Reset all circuit breakers
Reset-AllModelCircuitBreakers
```

## Configuration

Default settings (can be modified in `FailureDetectorConfig`):
```
FailureThreshold:       5 failures before circuit opens
SuccessThreshold:       3 successes to close circuit
WindowDuration:         5 minutes sliding window
RecoveryTimeout:        2 minutes before retry allowed
HalfOpenRetryInterval:  30 seconds between test requests
FailureRateThreshold:   50% failure rate triggers circuit
MinimumRequests:        10 requests minimum before rate calculation
```

## Integration with RawrXD

The failure detector is automatically integrated into:
- `Invoke-OllamaInference` - All Ollama model calls
- `Invoke-AgenticFlow` - Agentic pipeline operations
- All AI-powered features in the IDE

## Exported Functions

| Function | Description |
|----------|-------------|
| `Get-FailureDetector` | Get global failure detector instance |
| `Invoke-ModelWithFailureDetection` | Execute action with failure handling |
| `Get-ModelHealth` | Get overall health status |
| `Get-AllModelHealth` | Get status of all tracked models |
| `Get-RecentModelFailures` | Get recent failure events |
| `Reset-ModelCircuitBreaker` | Reset specific circuit breaker |
| `Reset-AllModelCircuitBreakers` | Reset all circuit breakers |

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Model Invocation Request                  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Circuit Breaker Check                     │
│   ┌─────────┐    ┌─────────┐    ┌───────────┐               │
│   │ Closed  │───▶│  Open   │───▶│ Half-Open │               │
│   └─────────┘    └─────────┘    └───────────┘               │
└─────────────────────────────────────────────────────────────┘
                              │
                    (If allowed)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Execute Model Action                      │
└─────────────────────────────────────────────────────────────┘
                       │              │
                  (Success)       (Failure)
                       │              │
                       ▼              ▼
              ┌────────────┐   ┌──────────────┐
              │Record      │   │Classify      │
              │Success     │   │Failure       │
              └────────────┘   └──────────────┘
                                     │
                                     ▼
                              ┌──────────────┐
                              │Recovery      │
                              │Strategy      │
                              └──────────────┘
                                     │
                                     ▼
                              ┌──────────────┐
                              │Retry with    │
                              │Backoff?      │
                              └──────────────┘
```

## Metrics & Monitoring

The failure detector tracks:
- Total requests per model/backend
- Success/failure counts
- Average latency
- Circuit breaker trip count
- Consecutive failures
- Last success/failure timestamps

Access via `Get-ModelHealth` for real-time dashboard data.
