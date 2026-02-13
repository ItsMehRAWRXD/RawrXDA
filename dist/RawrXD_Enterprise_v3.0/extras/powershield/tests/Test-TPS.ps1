# ⚡ TPS (Transactions Per Second) Performance Test
# Comprehensive throughput testing for Ollama API integration
# Tests concurrent request handling, response times, and system capacity

param(
  [Parameter(Mandatory = $false)]
  [string]$OllamaServer = "localhost:11434",

  [Parameter(Mandatory = $false)]
  [string]$Model = "bigdaddyg-fast:latest",

  [Parameter(Mandatory = $false)]
  [int]$DurationSeconds = 30,

  [Parameter(Mandatory = $false)]
  [int]$ConcurrentRequests = 5,

  [Parameter(Mandatory = $false)]
  [int]$MaxTokens = 50,

  [Parameter(Mandatory = $false)]
  [string]$TestType = "sustained",  # sustained, burst, ramp-up

  [Parameter(Mandatory = $false)]
  [switch]$GenerateReport = $true,

  [Parameter(Mandatory = $false)]
  [string]$OutputPath = ".\TPS-TEST-RESULTS.md",

  [Parameter(Mandatory = $false)]
  [int]$TimeoutSeconds = 60
)

# Test configuration and globals
$script:TestResults = @()
$script:TestStartTime = Get-Date
$script:TotalTransactions = 0
$script:SuccessfulTransactions = 0
$script:FailedTransactions = 0
$script:ResponseTimes = @()
$script:ErrorDetails = @()
$script:TPSResults = @{
  PeakTPS = 0
  AverageTPS = 0
  SustainedTPS = 0
  MinResponseTime = 0
  MaxResponseTime = 0
  AvgResponseTime = 0
  P50ResponseTime = 0
  P95ResponseTime = 0
  P99ResponseTime = 0
  SuccessRate = 0
  ErrorRate = 0
}

# Colors for output
$Red = "Red"
$Green = "Green"
$Yellow = "Yellow"
$Cyan = "Cyan"
$Magenta = "Magenta"

function Write-TestHeader {
  param([string]$Title)
  Write-Host "`n" -NoNewline
  Write-Host "=" * 80 -ForegroundColor Cyan
  Write-Host "⚡ $Title" -ForegroundColor Magenta
  Write-Host "=" * 80 -ForegroundColor Cyan
}

function Write-TestResult {
  param(
    [string]$TestName,
    [string]$Status,
    [string]$Details = "",
    [string]$Category = "GENERAL"
  )

  $colorMap = @{
    "PASS" = $Green
    "FAIL" = $Red
    "WARN" = $Yellow
    "INFO" = $Cyan
  }

  $icon = switch ($Status) {
    "PASS" { "✅" }
    "FAIL" { "❌" }
    "WARN" { "⚠️" }
    "INFO" { "ℹ️" }
  }

  Write-Host "$icon [$Category] $TestName" -ForegroundColor $colorMap[$Status]
  if ($Details) {
    Write-Host "   └─ $Details" -ForegroundColor Gray
  }

  $script:TestResults += @{
    TestName  = $TestName
    Category  = $Category
    Status    = $Status
    Details   = $Details
    Timestamp = Get-Date
  }
}

function Test-OllamaConnectivity {
  Write-TestHeader "OLLAMA SERVICE CONNECTIVITY"

  try {
    $serverHost = $OllamaServer.Split(':')[0]
    $serverPort = if ($OllamaServer.Contains(':')) { [int]$OllamaServer.Split(':')[1] } else { 11434 }

    $testConnection = Test-NetConnection -ComputerName $serverHost -Port $serverPort -InformationLevel Quiet -WarningAction SilentlyContinue

    if ($testConnection) {
      Write-TestResult "Ollama Service Reachable" "PASS" "Connected to $OllamaServer" "CONNECTIVITY"

      # Test API endpoint
      try {
        $response = Invoke-RestMethod -Uri "http://$OllamaServer/api/tags" -Method GET -TimeoutSec 10
        Write-TestResult "Ollama API Accessible" "PASS" "API responding correctly" "CONNECTIVITY"

        # Verify model exists
        $modelFound = $false
        if ($response.models) {
          foreach ($m in $response.models) {
            if ($m.name -eq $Model) {
              $modelFound = $true
              break
            }
          }
        }

        if ($modelFound) {
          Write-TestResult "Model Available" "PASS" "Model '$Model' found and ready" "CONNECTIVITY"
          return $true
        }
        else {
          Write-TestResult "Model Available" "WARN" "Model '$Model' not found, will attempt anyway" "CONNECTIVITY"
          return $true
        }

      }
      catch {
        Write-TestResult "Ollama API Accessible" "FAIL" "API error: $($_.Exception.Message)" "CONNECTIVITY"
        return $false
      }

    }
    else {
      Write-TestResult "Ollama Service Reachable" "FAIL" "Cannot connect to $OllamaServer" "CONNECTIVITY"
      return $false
    }

  }
  catch {
    Write-TestResult "Ollama Service Connectivity" "FAIL" "Connection test failed: $($_.Exception.Message)" "CONNECTIVITY"
    return $false
  }
}

function Invoke-SingleTransaction {
  param(
    [string]$TransactionId,
    [string]$TestPrompt
  )

  $result = @{
    Id = $TransactionId
    Success = $false
    ResponseTime = 0
    Error = $null
    Timestamp = Get-Date
  }

  try {
    $requestBody = @{
      model   = $Model
      prompt  = $TestPrompt
      stream  = $false
      options = @{
        max_tokens = $MaxTokens
      }
    } | ConvertTo-Json -Depth 3

    $startTime = Get-Date
    $response = Invoke-RestMethod -Uri "http://$OllamaServer/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec $TimeoutSeconds
    $endTime = Get-Date

    $responseTime = ($endTime - $startTime).TotalMilliseconds
    $result.Success = $true
    $result.ResponseTime = $responseTime

    # Thread-safe increment
    [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
    [System.Threading.Interlocked]::Increment([ref]$script:SuccessfulTransactions) | Out-Null

    lock ($script:ResponseTimes) {
      $script:ResponseTimes += $responseTime
    }

  }
  catch {
    $result.Success = $false
    $result.Error = $_.Exception.Message
    $result.ResponseTime = 0

    [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
    [System.Threading.Interlocked]::Increment([ref]$script:FailedTransactions) | Out-Null

    lock ($script:ErrorDetails) {
      $script:ErrorDetails += @{
        TransactionId = $TransactionId
        Error = $_.Exception.Message
        Timestamp = Get-Date
      }
    }
  }

  return $result
}

function Test-SustainedLoad {
  Write-TestHeader "SUSTAINED LOAD TEST"
  Write-Host "Duration: $DurationSeconds seconds | Concurrent: $ConcurrentRequests | Model: $Model" -ForegroundColor Cyan

  $endTime = (Get-Date).AddSeconds($DurationSeconds)
  $transactionCounter = 0
  $testPrompts = @(
    "Count to 5.",
    "Say hello.",
    "What is 2+2?",
    "Name a color.",
    "List one number."
  )

  $jobs = @()
  $tpsWindow = @()  # Track TPS over time windows
  $windowStart = Get-Date

  Write-Host "`n🚀 Starting sustained load test..." -ForegroundColor Yellow

  while ((Get-Date) -lt $endTime) {
    $currentTime = Get-Date
    $windowTPS = @()

    # Launch concurrent requests
    for ($i = 0; $i -lt $ConcurrentRequests; $i++) {
      $transactionCounter++
      $prompt = $testPrompts[$transactionCounter % $testPrompts.Count]
      $transactionId = "TXN-$transactionCounter"

      $job = Start-Job -ScriptBlock {
        param($Server, $Model, $Prompt, $MaxTokens, $Timeout, $TransactionId)

        try {
          $requestBody = @{
            model   = $Model
            prompt  = $Prompt
            stream  = $false
            options = @{
              max_tokens = $MaxTokens
            }
          } | ConvertTo-Json -Depth 3

          $startTime = Get-Date
          $response = Invoke-RestMethod -Uri "http://$Server/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec $Timeout
          $endTime = Get-Date

          return @{
            Success = $true
            ResponseTime = ($endTime - $startTime).TotalMilliseconds
            TransactionId = $TransactionId
            Timestamp = $startTime
          }
        }
        catch {
          return @{
            Success = $false
            ResponseTime = 0
            TransactionId = $TransactionId
            Error = $_.Exception.Message
            Timestamp = Get-Date
          }
        }
      } -ArgumentList $OllamaServer, $Model, $prompt, $MaxTokens, $TimeoutSeconds, $transactionId

      $jobs += $job
    }

    # Wait a bit before next batch (to control rate)
    Start-Sleep -Milliseconds 100

    # Collect completed jobs
    $completedJobs = $jobs | Where-Object { $_.State -eq "Completed" }
    foreach ($job in $completedJobs) {
      $result = Receive-Job -Job $job
      Remove-Job -Job $job
      $jobs = $jobs | Where-Object { $_.Id -ne $job.Id }

      if ($result.Success) {
        [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
        [System.Threading.Interlocked]::Increment([ref]$script:SuccessfulTransactions) | Out-Null
        $script:ResponseTimes += $result.ResponseTime
        $windowTPS += $result.Timestamp
      }
      else {
        [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
        [System.Threading.Interlocked]::Increment([ref]$script:FailedTransactions) | Out-Null
        $script:ErrorDetails += @{
          TransactionId = $result.TransactionId
          Error = $result.Error
          Timestamp = $result.Timestamp
        }
      }
    }

    # Calculate TPS for this window (1 second windows)
    $windowElapsed = ($currentTime - $windowStart).TotalSeconds
    if ($windowElapsed -ge 1.0) {
      $windowTPS = $windowTPS | Where-Object { ($currentTime - $_).TotalSeconds -le 1.0 }
      $currentTPS = $windowTPS.Count
      $tpsWindow += $currentTPS
      $windowStart = $currentTime

      if ($currentTPS -gt $script:TPSResults.PeakTPS) {
        $script:TPSResults.PeakTPS = $currentTPS
      }

      # Progress indicator
      Write-Host "." -NoNewline -ForegroundColor Gray
    }
  }

  # Wait for remaining jobs to complete
  Write-Host "`n⏳ Waiting for remaining requests to complete..." -ForegroundColor Yellow
  $remainingJobs = $jobs | Where-Object { $_.State -ne "Completed" }
  if ($remainingJobs.Count -gt 0) {
    $remainingJobs | Wait-Job -Timeout ($TimeoutSeconds + 10) | Out-Null
    foreach ($job in $remainingJobs) {
      $result = Receive-Job -Job $job -ErrorAction SilentlyContinue
      Remove-Job -Job $job -ErrorAction SilentlyContinue
      if ($result -and $result.Success) {
        [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
        [System.Threading.Interlocked]::Increment([ref]$script:SuccessfulTransactions) | Out-Null
        if ($result.ResponseTime -gt 0) {
          $script:ResponseTimes += $result.ResponseTime
        }
      }
      elseif ($result) {
        [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
        [System.Threading.Interlocked]::Increment([ref]$script:FailedTransactions) | Out-Null
      }
    }
  }

  Write-Host "`n✅ Sustained load test completed" -ForegroundColor Green
}

function Test-BurstLoad {
  Write-TestHeader "BURST LOAD TEST"
  Write-Host "Burst Size: $ConcurrentRequests requests | Model: $Model" -ForegroundColor Cyan

  $testPrompts = @(
    "Count to 5.",
    "Say hello.",
    "What is 2+2?",
    "Name a color.",
    "List one number."
  )

  Write-Host "`n🚀 Launching burst of $ConcurrentRequests concurrent requests..." -ForegroundColor Yellow

  $jobs = @()
  $transactionCounter = 0

  for ($i = 0; $i -lt $ConcurrentRequests; $i++) {
    $transactionCounter++
    $prompt = $testPrompts[$transactionCounter % $testPrompts.Count]
    $transactionId = "BURST-$transactionCounter"

    $job = Start-Job -ScriptBlock {
      param($Server, $Model, $Prompt, $MaxTokens, $Timeout, $TransactionId)

      try {
        $requestBody = @{
          model   = $Model
          prompt  = $Prompt
          stream  = $false
          options = @{
            max_tokens = $MaxTokens
          }
        } | ConvertTo-Json -Depth 3

        $startTime = Get-Date
        $response = Invoke-RestMethod -Uri "http://$Server/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec $Timeout
        $endTime = Get-Date

        return @{
          Success = $true
          ResponseTime = ($endTime - $startTime).TotalMilliseconds
          TransactionId = $TransactionId
          Timestamp = $startTime
        }
      }
      catch {
        return @{
          Success = $false
          ResponseTime = 0
          TransactionId = $TransactionId
          Error = $_.Exception.Message
          Timestamp = $startTime
        }
      }
    } -ArgumentList $OllamaServer, $Model, $prompt, $MaxTokens, $TimeoutSeconds, $transactionId

    $jobs += $job
  }

  Write-Host "⏳ Waiting for all requests to complete..." -ForegroundColor Yellow
  $jobs | Wait-Job | Out-Null

  $burstStartTime = ($jobs | Select-Object -First 1 | Receive-Job).Timestamp
  $burstEndTime = Get-Date
  $burstDuration = ($burstEndTime - $burstStartTime).TotalSeconds

  foreach ($job in $jobs) {
    $result = Receive-Job -Job $job
    Remove-Job -Job $job

    if ($result.Success) {
      [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
      [System.Threading.Interlocked]::Increment([ref]$script:SuccessfulTransactions) | Out-Null
      $script:ResponseTimes += $result.ResponseTime
    }
    else {
      [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
      [System.Threading.Interlocked]::Increment([ref]$script:FailedTransactions) | Out-Null
      $script:ErrorDetails += @{
        TransactionId = $result.TransactionId
        Error = $result.Error
        Timestamp = $result.Timestamp
      }
    }
  }

  if ($burstDuration -gt 0) {
    $burstTPS = $script:TotalTransactions / $burstDuration
    $script:TPSResults.PeakTPS = [math]::Round($burstTPS, 2)
  }

  Write-Host "✅ Burst load test completed" -ForegroundColor Green
}

function Test-RampUpLoad {
  Write-TestHeader "RAMP-UP LOAD TEST"
  Write-Host "Max Concurrent: $ConcurrentRequests | Duration: $DurationSeconds seconds | Model: $Model" -ForegroundColor Cyan

  $endTime = (Get-Date).AddSeconds($DurationSeconds)
  $transactionCounter = 0
  $currentConcurrency = 1
  $rampUpInterval = [math]::Max(1, $DurationSeconds / $ConcurrentRequests)
  $testPrompts = @(
    "Count to 5.",
    "Say hello.",
    "What is 2+2?",
    "Name a color.",
    "List one number."
  )

  Write-Host "`n🚀 Starting ramp-up load test..." -ForegroundColor Yellow

  $jobs = @()
  $lastRampUp = Get-Date

  while ((Get-Date) -lt $endTime -and $currentConcurrency -le $ConcurrentRequests) {
    # Ramp up concurrency
    $timeSinceRampUp = ((Get-Date) - $lastRampUp).TotalSeconds
    if ($timeSinceRampUp -ge $rampUpInterval -and $currentConcurrency -lt $ConcurrentRequests) {
      $currentConcurrency++
      $lastRampUp = Get-Date
      Write-Host "📈 Ramping up to $currentConcurrency concurrent requests..." -ForegroundColor Cyan
    }

    # Launch requests at current concurrency level
    $activeJobs = ($jobs | Where-Object { $_.State -eq "Running" }).Count
    $jobsToLaunch = $currentConcurrency - $activeJobs

    for ($i = 0; $i -lt $jobsToLaunch; $i++) {
      $transactionCounter++
      $prompt = $testPrompts[$transactionCounter % $testPrompts.Count]
      $transactionId = "RAMP-$transactionCounter"

      $job = Start-Job -ScriptBlock {
        param($Server, $Model, $Prompt, $MaxTokens, $Timeout, $TransactionId)

        try {
          $requestBody = @{
            model   = $Model
            prompt  = $Prompt
            stream  = $false
            options = @{
              max_tokens = $MaxTokens
            }
          } | ConvertTo-Json -Depth 3

          $startTime = Get-Date
          $response = Invoke-RestMethod -Uri "http://$Server/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec $Timeout
          $endTime = Get-Date

          return @{
            Success = $true
            ResponseTime = ($endTime - $startTime).TotalMilliseconds
            TransactionId = $TransactionId
            Timestamp = $startTime
          }
        }
        catch {
          return @{
            Success = $false
            ResponseTime = 0
            TransactionId = $TransactionId
            Error = $_.Exception.Message
            Timestamp = Get-Date
          }
        }
      } -ArgumentList $OllamaServer, $Model, $prompt, $MaxTokens, $TimeoutSeconds, $transactionId

      $jobs += $job
    }

    # Collect completed jobs
    $completedJobs = $jobs | Where-Object { $_.State -eq "Completed" }
    foreach ($job in $completedJobs) {
      $result = Receive-Job -Job $job
      Remove-Job -Job $job
      $jobs = $jobs | Where-Object { $_.Id -ne $job.Id }

      if ($result.Success) {
        [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
        [System.Threading.Interlocked]::Increment([ref]$script:SuccessfulTransactions) | Out-Null
        $script:ResponseTimes += $result.ResponseTime
      }
      else {
        [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
        [System.Threading.Interlocked]::Increment([ref]$script:FailedTransactions) | Out-Null
        $script:ErrorDetails += @{
          TransactionId = $result.TransactionId
          Error = $result.Error
          Timestamp = $result.Timestamp
        }
      }
    }

    Start-Sleep -Milliseconds 100
  }

  # Wait for remaining jobs
  Write-Host "`n⏳ Waiting for remaining requests to complete..." -ForegroundColor Yellow
  $remainingJobs = $jobs | Where-Object { $_.State -ne "Completed" }
  if ($remainingJobs.Count -gt 0) {
    $remainingJobs | Wait-Job -Timeout ($TimeoutSeconds + 10) | Out-Null
    foreach ($job in $remainingJobs) {
      $result = Receive-Job -Job $job -ErrorAction SilentlyContinue
      Remove-Job -Job $job -ErrorAction SilentlyContinue
      if ($result -and $result.Success) {
        [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
        [System.Threading.Interlocked]::Increment([ref]$script:SuccessfulTransactions) | Out-Null
        if ($result.ResponseTime -gt 0) {
          $script:ResponseTimes += $result.ResponseTime
        }
      }
      elseif ($result) {
        [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
        [System.Threading.Interlocked]::Increment([ref]$script:FailedTransactions) | Out-Null
      }
    }
  }

  Write-Host "✅ Ramp-up load test completed" -ForegroundColor Green
}

function Calculate-PerformanceMetrics {
  Write-TestHeader "PERFORMANCE METRICS CALCULATION"

  $testDuration = ((Get-Date) - $script:TestStartTime).TotalSeconds

  if ($script:TotalTransactions -eq 0) {
    Write-TestResult "Transaction Count" "FAIL" "No transactions completed" "METRICS"
    return
  }

  # Calculate TPS
  if ($testDuration -gt 0) {
    $script:TPSResults.AverageTPS = [math]::Round($script:TotalTransactions / $testDuration, 2)
    $script:TPSResults.SustainedTPS = [math]::Round($script:SuccessfulTransactions / $testDuration, 2)
  }

  # Calculate success/error rates
  $script:TPSResults.SuccessRate = [math]::Round(($script:SuccessfulTransactions / $script:TotalTransactions) * 100, 2)
  $script:TPSResults.ErrorRate = [math]::Round(($script:FailedTransactions / $script:TotalTransactions) * 100, 2)

  # Calculate response time statistics
  if ($script:ResponseTimes.Count -gt 0) {
    $sortedTimes = $script:ResponseTimes | Sort-Object
    $script:TPSResults.MinResponseTime = [math]::Round(($sortedTimes | Measure-Object -Minimum).Minimum, 2)
    $script:TPSResults.MaxResponseTime = [math]::Round(($sortedTimes | Measure-Object -Maximum).Maximum, 2)
    $script:TPSResults.AvgResponseTime = [math]::Round(($sortedTimes | Measure-Object -Average).Average, 2)

    # Percentiles
    $count = $sortedTimes.Count
    $script:TPSResults.P50ResponseTime = [math]::Round($sortedTimes[[math]::Floor($count * 0.50)], 2)
    $script:TPSResults.P95ResponseTime = [math]::Round($sortedTimes[[math]::Floor($count * 0.95)], 2)
    $script:TPSResults.P99ResponseTime = [math]::Round($sortedTimes[[math]::Floor($count * 0.99)], 2)
  }

  # Display results
  Write-Host "`n📊 Performance Summary:" -ForegroundColor Cyan
  Write-Host "   Total Transactions: $($script:TotalTransactions)" -ForegroundColor White
  Write-Host "   Successful: $($script:SuccessfulTransactions) ($($script:TPSResults.SuccessRate)%)" -ForegroundColor Green
  Write-Host "   Failed: $($script:FailedTransactions) ($($script:TPSResults.ErrorRate)%)" -ForegroundColor $(if ($script:FailedTransactions -gt 0) { $Red } else { $Green })
  Write-Host "   Test Duration: $([math]::Round($testDuration, 2)) seconds" -ForegroundColor White

  Write-Host "`n⚡ Throughput Metrics:" -ForegroundColor Cyan
  Write-Host "   Peak TPS: $($script:TPSResults.PeakTPS)" -ForegroundColor Yellow
  Write-Host "   Average TPS: $($script:TPSResults.AverageTPS)" -ForegroundColor White
  Write-Host "   Sustained TPS: $($script:TPSResults.SustainedTPS)" -ForegroundColor White

  Write-Host "`n⏱️  Response Time Metrics:" -ForegroundColor Cyan
  Write-Host "   Min: $($script:TPSResults.MinResponseTime) ms" -ForegroundColor Green
  Write-Host "   Max: $($script:TPSResults.MaxResponseTime) ms" -ForegroundColor $(if ($script:TPSResults.MaxResponseTime -gt 5000) { $Red } else { $Yellow })
  Write-Host "   Average: $($script:TPSResults.AvgResponseTime) ms" -ForegroundColor White
  Write-Host "   P50 (Median): $($script:TPSResults.P50ResponseTime) ms" -ForegroundColor White
  Write-Host "   P95: $($script:TPSResults.P95ResponseTime) ms" -ForegroundColor $(if ($script:TPSResults.P95ResponseTime -gt 3000) { $Yellow } else { $White })
  Write-Host "   P99: $($script:TPSResults.P99ResponseTime) ms" -ForegroundColor $(if ($script:TPSResults.P99ResponseTime -gt 5000) { $Red } else { $Yellow })

  # Evaluate performance
  if ($script:TPSResults.SuccessRate -ge 95) {
    Write-TestResult "Success Rate" "PASS" "$($script:TPSResults.SuccessRate)% (Excellent)" "METRICS"
  }
  elseif ($script:TPSResults.SuccessRate -ge 90) {
    Write-TestResult "Success Rate" "PASS" "$($script:TPSResults.SuccessRate)% (Good)" "METRICS"
  }
  else {
    Write-TestResult "Success Rate" "WARN" "$($script:TPSResults.SuccessRate)% (Needs Improvement)" "METRICS"
  }

  if ($script:TPSResults.AvgResponseTime -lt 1000) {
    Write-TestResult "Average Response Time" "PASS" "$($script:TPSResults.AvgResponseTime)ms (Fast)" "METRICS"
  }
  elseif ($script:TPSResults.AvgResponseTime -lt 3000) {
    Write-TestResult "Average Response Time" "PASS" "$($script:TPSResults.AvgResponseTime)ms (Acceptable)" "METRICS"
  }
  else {
    Write-TestResult "Average Response Time" "WARN" "$($script:TPSResults.AvgResponseTime)ms (Slow)" "METRICS"
  }

  if ($script:TPSResults.AverageTPS -gt 0) {
    Write-TestResult "Throughput" "INFO" "$($script:TPSResults.AverageTPS) TPS average" "METRICS"
  }
}

function Generate-TestReport {
  if (-not $GenerateReport) {
    return
  }

  Write-TestHeader "GENERATING TEST REPORT"

  $testDuration = ((Get-Date) - $script:TestStartTime).TotalSeconds
  $reportContent = @"
# ⚡ TPS (Transactions Per Second) Test Results
**Test Date**: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
**Test Duration**: $([math]::Round($testDuration, 2)) seconds
**Test Type**: $TestType
**Ollama Server**: $OllamaServer
**Model**: $Model
**Concurrent Requests**: $ConcurrentRequests
**Max Tokens**: $MaxTokens
**Timeout**: $TimeoutSeconds seconds

## 📊 Executive Summary

**Overall Results**:
- 🎯 **Total Transactions**: $($script:TotalTransactions)
- ✅ **Successful**: $($script:SuccessfulTransactions) ($($script:TPSResults.SuccessRate)%)
- ❌ **Failed**: $($script:FailedTransactions) ($($script:TPSResults.ErrorRate)%)

**Success Rate**: $($script:TPSResults.SuccessRate)%

## ⚡ Throughput Metrics

- **Peak TPS**: $($script:TPSResults.PeakTPS)
- **Average TPS**: $($script:TPSResults.AverageTPS)
- **Sustained TPS**: $($script:TPSResults.SustainedTPS)

## ⏱️ Response Time Statistics

- **Minimum**: $($script:TPSResults.MinResponseTime) ms
- **Maximum**: $($script:TPSResults.MaxResponseTime) ms
- **Average**: $($script:TPSResults.AvgResponseTime) ms
- **P50 (Median)**: $($script:TPSResults.P50ResponseTime) ms
- **P95**: $($script:TPSResults.P95ResponseTime) ms
- **P99**: $($script:TPSResults.P99ResponseTime) ms

## 📋 Detailed Test Results

### Test Configuration
- Test Type: $TestType
- Duration: $DurationSeconds seconds
- Concurrent Requests: $ConcurrentRequests
- Model: $Model
- Max Tokens: $MaxTokens

### Performance Evaluation

"@

  if ($script:TPSResults.SuccessRate -ge 95) {
    $reportContent += "✅ **Success Rate**: EXCELLENT ($($script:TPSResults.SuccessRate)%)`n"
  }
  elseif ($script:TPSResults.SuccessRate -ge 90) {
    $reportContent += "✅ **Success Rate**: GOOD ($($script:TPSResults.SuccessRate)%)`n"
  }
  else {
    $reportContent += "⚠️ **Success Rate**: NEEDS IMPROVEMENT ($($script:TPSResults.SuccessRate)%)`n"
  }

  if ($script:TPSResults.AvgResponseTime -lt 1000) {
    $reportContent += "✅ **Response Time**: FAST ($($script:TPSResults.AvgResponseTime)ms average)`n"
  }
  elseif ($script:TPSResults.AvgResponseTime -lt 3000) {
    $reportContent += "✅ **Response Time**: ACCEPTABLE ($($script:TPSResults.AvgResponseTime)ms average)`n"
  }
  else {
    $reportContent += "⚠️ **Response Time**: SLOW ($($script:TPSResults.AvgResponseTime)ms average)`n"
  }

  $reportContent += @"

### Error Details

"@

  if ($script:ErrorDetails.Count -gt 0) {
    $reportContent += "**Total Errors**: $($script:ErrorDetails.Count)`n`n"
    $uniqueErrors = $script:ErrorDetails | Group-Object -Property Error
    foreach ($errorGroup in $uniqueErrors) {
      $reportContent += "- **$($errorGroup.Name)**: $($errorGroup.Count) occurrences`n"
    }
  }
  else {
    $reportContent += "✅ No errors encountered`n"
  }

  $reportContent += @"

## 🎯 Recommendations

"@

  if ($script:TPSResults.SuccessRate -lt 95) {
    $reportContent += "- ⚠️ Consider reducing concurrent request load to improve success rate`n"
    $reportContent += "- ⚠️ Check Ollama server capacity and resource availability`n"
  }

  if ($script:TPSResults.AvgResponseTime -gt 3000) {
    $reportContent += "- ⚠️ Response times are high - consider optimizing model or reducing max_tokens`n"
    $reportContent += "- ⚠️ Check network latency and server performance`n"
  }

  if ($script:TPSResults.P95ResponseTime -gt 5000) {
    $reportContent += "- ⚠️ P95 response time indicates tail latency issues - investigate server bottlenecks`n"
  }

  if ($script:TPSResults.SuccessRate -ge 95 -and $script:TPSResults.AvgResponseTime -lt 2000) {
    $reportContent += "- ✅ System performance is excellent - current configuration is optimal`n"
  }

  $reportContent += @"

---
*Report generated by Test-TPS.ps1*
"@

  try {
    $reportContent | Out-File -FilePath $OutputPath -Encoding UTF8
    Write-TestResult "Report Generation" "PASS" "Report saved to $OutputPath" "REPORT"
  }
  catch {
    Write-TestResult "Report Generation" "FAIL" "Failed to save report: $($_.Exception.Message)" "REPORT"
  }
}

# ============================================
# MAIN EXECUTION
# ============================================

Write-Host "`n" -NoNewline
Write-Host "╔══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    ⚡ TPS PERFORMANCE TEST SUITE ⚡                          ║" -ForegroundColor Magenta
Write-Host "╚══════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Test connectivity first
if (-not (Test-OllamaConnectivity)) {
  Write-Host "`n❌ Cannot proceed with TPS testing - Ollama service is not accessible" -ForegroundColor Red
  exit 1
}

# Initialize response times array (thread-safe)
$script:ResponseTimes = New-Object System.Collections.ArrayList
$script:ErrorDetails = New-Object System.Collections.ArrayList

# Run the appropriate test based on type
switch ($TestType.ToLower()) {
  "sustained" {
    Test-SustainedLoad
  }
  "burst" {
    Test-BurstLoad
  }
  "ramp-up" {
    Test-RampUpLoad
  }
  default {
    Write-Host "⚠️ Unknown test type '$TestType', defaulting to 'sustained'" -ForegroundColor Yellow
    Test-SustainedLoad
  }
}

# Calculate and display metrics
Calculate-PerformanceMetrics

# Generate report
if ($GenerateReport) {
  Generate-TestReport
}

Write-Host "`n✅ TPS testing completed!" -ForegroundColor Green
Write-Host ""

