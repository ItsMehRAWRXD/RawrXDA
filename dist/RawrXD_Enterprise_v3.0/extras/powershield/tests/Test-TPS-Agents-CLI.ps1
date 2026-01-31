# ⚡ TPS (Transactions Per Second) Test for RawrXD Agents - Via CLI
# Tests actual agent system by calling RawrXD CLI commands
# This uses the REAL agent infrastructure through RawrXD's CLI interface

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
  [string]$OutputPath = ".\TPS-AGENT-CLI-TEST-RESULTS.md"
)

$script:TestStartTime = Get-Date
$script:TotalTransactions = 0
$script:SuccessfulTransactions = 0
$script:FailedTransactions = 0
$script:ResponseTimes = New-Object System.Collections.ArrayList
$script:ErrorDetails = New-Object System.Collections.ArrayList
$script:CommandResponseTimes = @{}

Write-Host "`n╔══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║     ⚡ TPS TEST - RAWRXD AGENTS VIA CLI (REAL SYSTEM) ⚡                   ║" -ForegroundColor Magenta
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

# Test commands that use agent system
$testCommands = @(
  @{Command = "list-models"; Description = "List Ollama models (uses agent infrastructure)"}
  @{Command = "test-ollama"; Description = "Test Ollama connection (uses agent system)"}
  @{Command = "list-agents"; Description = "List agent tasks (direct agent command)"}
)

Write-Host "🔄 Testing agent system via CLI commands..." -ForegroundColor Yellow
Write-Host "   Commands: $($testCommands.Command -join ', ')" -ForegroundColor Gray
Write-Host ""

Write-Host "🚀 Starting $TestType test with REAL agent CLI commands..." -ForegroundColor Yellow
Write-Host ""

$endTime = (Get-Date).AddSeconds($DurationSeconds)
$transactionCounter = 0
$jobs = New-Object System.Collections.ArrayList

while ((Get-Date) -lt $endTime) {
  for ($i = 0; $i -lt $ConcurrentRequests; $i++) {
    $transactionCounter++
    $transactionId = "TXN-$transactionCounter"

    # Rotate through test commands
    $cmdIndex = $transactionCounter % $testCommands.Count
    $testCmd = $testCommands[$cmdIndex]

    $job = Start-Job -ScriptBlock {
      param($Command, $RawrXDPath, $TxnId)

      $startTime = Get-Date
      $result = @{
        Success = $false
        ResponseTime = 0
        Error = $null
        TransactionId = $TxnId
        Command = $Command
      }

      try {
        # Call RawrXD CLI command
        $process = Start-Process -FilePath "powershell.exe" `
          -ArgumentList @("-ExecutionPolicy", "Bypass", "-File", $RawrXDPath, "-CliMode", "-Command", $Command) `
          -NoNewWindow -PassThru -Wait -RedirectStandardOutput "temp_output_$TxnId.txt" -RedirectStandardError "temp_error_$TxnId.txt"

        $output = Get-Content "temp_output_$TxnId.txt" -ErrorAction SilentlyContinue
        $errors = Get-Content "temp_error_$TxnId.txt" -ErrorAction SilentlyContinue

        # Clean up temp files
        Remove-Item "temp_output_$TxnId.txt" -ErrorAction SilentlyContinue
        Remove-Item "temp_error_$TxnId.txt" -ErrorAction SilentlyContinue

        # Check if command succeeded
        $hasErrors = $errors | Where-Object {
          $_ -match "Error|Failed|✗|❌" -and
          $_ -notmatch "Warning|⚠"
        }

        # Also check output for errors
        $outputErrors = $output | Where-Object {
          $_ -match "✗|❌|Error:" -and
          $_ -notmatch "Warning|⚠"
        }

        if ($process.ExitCode -eq 0 -and $hasErrors.Count -eq 0 -and $outputErrors.Count -eq 0) {
          $result.Success = $true
        } else {
          $result.Error = if ($hasErrors.Count -gt 0) {
            ($hasErrors | Select-Object -First 1).ToString()
          } elseif ($outputErrors.Count -gt 0) {
            ($outputErrors | Select-Object -First 1).ToString()
          } else {
            "Command returned exit code: $($process.ExitCode)"
          }
        }

        $result.ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
      }
      catch {
        $result.Error = $_.Exception.Message
        $result.ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
      }

      return $result
    } -ArgumentList $testCmd.Command, $RawrXDPath, $transactionId

    $null = $jobs.Add($job)
  }

  Start-Sleep -Milliseconds 300

  # Collect completed jobs
  $completedJobs = $jobs | Where-Object { $_.State -eq "Completed" }
  foreach ($job in $completedJobs) {
    $result = Receive-Job -Job $job
    Remove-Job -Job $job
    $jobs.Remove($job) | Out-Null

    $script:TotalTransactions++
    if ($result.Success) {
      $script:SuccessfulTransactions++
      $null = $script:ResponseTimes.Add($result.ResponseTime)

      # Track per-command metrics
      if (-not $script:CommandResponseTimes.ContainsKey($result.Command)) {
        $script:CommandResponseTimes[$result.Command] = New-Object System.Collections.ArrayList
      }
      $null = $script:CommandResponseTimes[$result.Command].Add($result.ResponseTime)
    }
    else {
      $script:FailedTransactions++
      $null = $script:ErrorDetails.Add(@{
        TransactionId = $result.TransactionId
        Error = $result.Error
        Command = $result.Command
      })
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
        $null = $script:ResponseTimes.Add($result.ResponseTime)
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
  $p99 = if ($sortedTimes.Count -gt 1) { [math]::Round($sortedTimes[[math]::Floor($sortedTimes.Count * 0.99)], 2) } else { $avgTime }
} else {
  $minTime = $maxTime = $avgTime = $p50 = $p95 = $p99 = 0
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
Write-Host "   P99: $p99 ms" -ForegroundColor White

# Command-specific metrics
if ($script:CommandResponseTimes.Count -gt 0) {
  Write-Host "`n🔧 Command Performance:" -ForegroundColor Cyan
  foreach ($cmdName in $script:CommandResponseTimes.Keys) {
    $cmdTimes = $script:CommandResponseTimes[$cmdName]
    if ($cmdTimes.Count -gt 0) {
      $cmdAvg = [math]::Round(($cmdTimes | Measure-Object -Average).Average, 2)
      Write-Host "   $cmdName : $cmdAvg ms avg ($($cmdTimes.Count) calls)" -ForegroundColor White
    }
  }
}

# Generate report
if ($OutputPath) {
  $report = @"
# ⚡ TPS Test Results - Agent System via CLI
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
- P99: $p99 ms

## Command Performance
"@

  if ($script:CommandResponseTimes.Count -gt 0) {
    foreach ($cmdName in $script:CommandResponseTimes.Keys) {
      $cmdTimes = $script:CommandResponseTimes[$cmdName]
      if ($cmdTimes.Count -gt 0) {
        $cmdAvg = [math]::Round(($cmdTimes | Measure-Object -Average).Average, 2)
        $cmdMin = [math]::Round(($cmdTimes | Measure-Object -Minimum).Minimum, 2)
        $cmdMax = [math]::Round(($cmdTimes | Measure-Object -Maximum).Maximum, 2)
        $report += "### $cmdName`n"
        $report += "- Calls: $($cmdTimes.Count)`n"
        $report += "- Average: $cmdAvg ms`n"
        $report += "- Min: $cmdMin ms`n"
        $report += "- Max: $cmdMax ms`n`n"
      }
    }
  }

  if ($script:ErrorDetails.Count -gt 0) {
    $report += "## Errors`n`n"
    $uniqueErrors = $script:ErrorDetails | Group-Object -Property Error
    foreach ($errorGroup in $uniqueErrors) {
      $report += "- **$($errorGroup.Name)**: $($errorGroup.Count) occurrences`n"
    }
  }

  $report | Out-File -FilePath $OutputPath -Encoding UTF8
  Write-Host "`n📄 Report saved to: $OutputPath" -ForegroundColor Cyan
}

Write-Host ""

