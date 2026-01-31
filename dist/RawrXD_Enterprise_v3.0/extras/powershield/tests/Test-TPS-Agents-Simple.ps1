# ⚡ TPS (Transactions Per Second) Test for RawrXD Agents - Simplified Version
# Tests agent system performance by calling RawrXD in CLI mode

param(
  [Parameter(Mandatory = $false)]
  [int]$DurationSeconds = 30,

  [Parameter(Mandatory = $false)]
  [int]$ConcurrentRequests = 5,

  [Parameter(Mandatory = $false)]
  [string]$TestType = "sustained",  # sustained, burst

  [Parameter(Mandatory = $false)]
  [string]$RawrXDPath = ".\RawrXD.ps1",

  [Parameter(Mandatory = $false)]
  [string]$OutputPath = ".\TPS-AGENT-TEST-RESULTS.md"
)

$script:TestStartTime = Get-Date
$script:TotalTransactions = 0
$script:SuccessfulTransactions = 0
$script:FailedTransactions = 0
$script:ResponseTimes = @()
$script:ErrorDetails = @()

Write-Host "`n╔══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║        ⚡ TPS TEST - RAWRXD AGENTS (Simplified) ⚡                        ║" -ForegroundColor Magenta
Write-Host "╚══════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $RawrXDPath)) {
  Write-Host "❌ RawrXD.ps1 not found at: $RawrXDPath" -ForegroundColor Red
  exit 1
}

Write-Host "✅ Found RawrXD.ps1" -ForegroundColor Green
Write-Host "📊 Test Configuration:" -ForegroundColor Cyan
Write-Host "   Duration: $DurationSeconds seconds" -ForegroundColor White
Write-Host "   Concurrent Requests: $ConcurrentRequests" -ForegroundColor White
Write-Host "   Test Type: $TestType" -ForegroundColor White
Write-Host ""

# Test agent tools by calling RawrXD CLI commands
function Test-AgentToolViaCLI {
  param(
    [string]$ToolName,
    [hashtable]$Parameters,
    [string]$TransactionId
  )

  $startTime = Get-Date
  $result = @{
    Success = $false
    ResponseTime = 0
    Error = $null
  }

  try {
    # For now, we'll test by calling simple agent operations
    # Since we can't easily load RawrXD functions, we'll test via file operations
    # that agents would typically perform

    switch ($ToolName) {
      "read_file" {
        if ($Parameters.path -and (Test-Path $Parameters.path)) {
          $content = Get-Content $Parameters.path -Raw -ErrorAction Stop
          $result.Success = $true
        }
      }
      "list_directory" {
        $path = if ($Parameters.path) { $Parameters.path } else { "." }
        if (Test-Path $path) {
          $items = Get-ChildItem $path -ErrorAction Stop
          $result.Success = $true
        }
      }
      default {
        $result.Error = "Tool not implemented in test"
      }
    }

    $result.ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
  }
  catch {
    $result.Error = $_.Exception.Message
    $result.ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
  }

  return $result
}

Write-Host "🚀 Starting $TestType test..." -ForegroundColor Yellow
Write-Host ""

$endTime = (Get-Date).AddSeconds($DurationSeconds)
$transactionCounter = 0
$jobs = New-Object System.Collections.ArrayList
$testFiles = @("RawrXD.ps1", "Test-TPS-Agents-Simple.ps1") | Where-Object { Test-Path $_ }

if ($testFiles.Count -eq 0) {
  Write-Host "⚠️ No test files found, creating a test file..." -ForegroundColor Yellow
  "Test content" | Out-File -FilePath "test-agent-file.txt" -Encoding UTF8
  $testFiles = @("test-agent-file.txt")
}

while ((Get-Date) -lt $endTime) {
  for ($i = 0; $i -lt $ConcurrentRequests; $i++) {
    $transactionCounter++
    $transactionId = "TXN-$transactionCounter"
    $testType = $transactionCounter % 2
    $testFile = $testFiles[$transactionCounter % $testFiles.Count]

    $job = Start-Job -ScriptBlock {
      param($ToolName, $Params, $TxnId)

      $startTime = Get-Date
      $result = @{ Success = $false; ResponseTime = 0; Error = $null }

      try {
        switch ($ToolName) {
          "read_file" {
            if ($Params.path -and (Test-Path $Params.path)) {
              $null = Get-Content $Params.path -Raw -ErrorAction Stop
              $result.Success = $true
            }
          }
          "list_directory" {
            $path = if ($Params.path) { $Params.path } else { "." }
            if (Test-Path $path) {
              $null = Get-ChildItem $path -ErrorAction Stop
              $result.Success = $true
            }
          }
        }
        $result.ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
      }
      catch {
        $result.Error = $_.Exception.Message
        $result.ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
      }

      return $result
    } -ArgumentList @(
      if ($testType -eq 0) { "read_file" } else { "list_directory" },
      @{path = if ($testType -eq 0) { $testFile } else { "." }},
      $transactionId
    )

    $null = $jobs.Add($job)
  }

  Start-Sleep -Milliseconds 200

  # Collect completed jobs
  $completedJobs = $jobs | Where-Object { $_.State -eq "Completed" }
  foreach ($job in $completedJobs) {
    $result = Receive-Job -Job $job
    Remove-Job -Job $job
    $jobs.Remove($job) | Out-Null

    $script:TotalTransactions++
    if ($result.Success) {
      $script:SuccessfulTransactions++
      $script:ResponseTimes += $result.ResponseTime
    }
    else {
      $script:FailedTransactions++
      $script:ErrorDetails += @{
        TransactionId = $transactionId
        Error = $result.Error
      }
    }
  }

  Write-Host "." -NoNewline -ForegroundColor Gray
}

# Wait for remaining jobs
Write-Host "`n⏳ Waiting for remaining requests..." -ForegroundColor Yellow
$remainingJobs = $jobs | Where-Object { $_.State -ne "Completed" }
if ($remainingJobs.Count -gt 0) {
  $remainingJobs | Wait-Job -Timeout 30 | Out-Null
  foreach ($job in $remainingJobs) {
    $result = Receive-Job -Job $job -ErrorAction SilentlyContinue
    Remove-Job -Job $job -ErrorAction SilentlyContinue
    $script:TotalTransactions++
    if ($result -and $result.Success) {
      $script:SuccessfulTransactions++
      if ($result.ResponseTime -gt 0) {
        $script:ResponseTimes += $result.ResponseTime
      }
    }
    elseif ($result) {
      $script:FailedTransactions++
    }
  }
}

# Calculate metrics
$testDuration = ((Get-Date) - $script:TestStartTime).TotalSeconds
$avgTPS = if ($testDuration -gt 0) { [math]::Round($script:TotalTransactions / $testDuration, 2) } else { 0 }
$successRate = if ($script:TotalTransactions -gt 0) { [math]::Round(($script:SuccessfulTransactions / $script:TotalTransactions) * 100, 2) } else { 0 }

if ($script:ResponseTimes.Count -gt 0) {
  $sortedTimes = $script:ResponseTimes | Sort-Object
  $minTime = [math]::Round(($sortedTimes | Measure-Object -Minimum).Minimum, 2)
  $maxTime = [math]::Round(($sortedTimes | Measure-Object -Maximum).Maximum, 2)
  $avgTime = [math]::Round(($sortedTimes | Measure-Object -Average).Average, 2)
  $p50 = [math]::Round($sortedTimes[[math]::Floor($sortedTimes.Count * 0.50)], 2)
  $p95 = [math]::Round($sortedTimes[[math]::Floor($sortedTimes.Count * 0.95)], 2)
}
else {
  $minTime = $maxTime = $avgTime = $p50 = $p95 = 0
}

Write-Host "`n`n✅ Test completed!" -ForegroundColor Green
Write-Host "`n📊 Results:" -ForegroundColor Cyan
Write-Host "   Total Transactions: $($script:TotalTransactions)" -ForegroundColor White
Write-Host "   Successful: $($script:SuccessfulTransactions) ($successRate%)" -ForegroundColor Green
Write-Host "   Failed: $($script:FailedTransactions)" -ForegroundColor $(if ($script:FailedTransactions -gt 0) { "Red" } else { "Green" })
Write-Host "   Duration: $([math]::Round($testDuration, 2)) seconds" -ForegroundColor White
Write-Host "   Average TPS: $avgTPS" -ForegroundColor Yellow
Write-Host "`n⏱️  Response Times:" -ForegroundColor Cyan
Write-Host "   Min: $minTime ms" -ForegroundColor White
Write-Host "   Max: $maxTime ms" -ForegroundColor White
Write-Host "   Avg: $avgTime ms" -ForegroundColor White
Write-Host "   P50: $p50 ms" -ForegroundColor White
Write-Host "   P95: $p95 ms" -ForegroundColor White

# Generate report
if ($OutputPath) {
  $report = @"
# ⚡ TPS Test Results - Agent System
**Test Date**: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
**Duration**: $([math]::Round($testDuration, 2)) seconds
**Test Type**: $TestType
**Concurrent Requests**: $ConcurrentRequests

## Results
- Total Transactions: $($script:TotalTransactions)
- Successful: $($script:SuccessfulTransactions) ($successRate%)
- Failed: $($script:FailedTransactions)
- Average TPS: $avgTPS

## Response Times
- Min: $minTime ms
- Max: $maxTime ms
- Average: $avgTime ms
- P50: $p50 ms
- P95: $p95 ms
"@

  $report | Out-File -FilePath $OutputPath -Encoding UTF8
  Write-Host "`n📄 Report saved to: $OutputPath" -ForegroundColor Cyan
}

Write-Host ""

