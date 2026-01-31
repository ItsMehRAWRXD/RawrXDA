# ⚡ TPS (Transactions Per Second) Test for RawrXD Agents - REAL AGENT SYSTEM
# Tests actual agent tools and commands via RawrXD's agent infrastructure
# Uses real agent functions, not simulations

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
  [string]$OutputPath = ".\TPS-AGENT-REAL-TEST-RESULTS.md"
)

$script:TestStartTime = Get-Date
$script:TotalTransactions = 0
$script:SuccessfulTransactions = 0
$script:FailedTransactions = 0
$script:ResponseTimes = New-Object System.Collections.ArrayList
$script:ErrorDetails = New-Object System.Collections.ArrayList
$script:ToolResponseTimes = @{}

Write-Host "`n╔══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║        ⚡ TPS TEST - RAWRXD REAL AGENT SYSTEM ⚡                          ║" -ForegroundColor Magenta
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

# Load RawrXD functions properly
Write-Host "🔄 Loading RawrXD agent system..." -ForegroundColor Yellow

# Set flags before loading
$script:SkipGUIInit = $true
$script:WindowsFormsAvailable = $false
$ErrorActionPreference = 'SilentlyContinue'

try {
  # Load RawrXD by reading and executing content (avoiding parameter binding issues)
  $rawrContent = Get-Content $RawrXDPath -Raw -ErrorAction Stop

  # Create a script block and execute it
  # This will load all functions including agent tools
  $scriptBlock = [scriptblock]::Create($rawrContent)

  # Execute in a sub-scope to avoid parameter issues
  & {
    $script:SkipGUIInit = $true
    $script:WindowsFormsAvailable = $false
    $ErrorActionPreference = 'SilentlyContinue'
    & $scriptBlock
  } 2>&1 | Out-Null

  # Check if agent tools are available
  $availableTools = @()
  if ($script:agentTools -and $script:agentTools.Count -gt 0) {
    $availableTools = $script:agentTools.Keys
    Write-Host "✅ Found $($availableTools.Count) registered agent tools" -ForegroundColor Green
    Write-Host "   Tools: $($availableTools -join ', ')" -ForegroundColor Gray
  } else {
    Write-Host "⚠️ Agent tools not registered yet. Checking common tools..." -ForegroundColor Yellow
    # Try to call Get-AgentToolsList if available
    if (Get-Command Get-AgentToolsList -ErrorAction SilentlyContinue) {
      $toolsList = Get-AgentToolsList
      if ($toolsList) {
        $availableTools = $toolsList.Values | ForEach-Object { $_.name }
        Write-Host "✅ Found $($availableTools.Count) tools via Get-AgentToolsList" -ForegroundColor Green
      }
    }

    if ($availableTools.Count -eq 0) {
      Write-Host "⚠️ Using fallback tool list" -ForegroundColor Yellow
      $availableTools = @("read_file", "list_directory", "write_file")
    }
  }

} catch {
  Write-Host "⚠️ Error loading RawrXD: $_" -ForegroundColor Yellow
  Write-Host "   Will use fallback tool list..." -ForegroundColor Gray
  $availableTools = @("read_file", "list_directory", "write_file")
}

# Test files
$testFiles = @("RawrXD.ps1", "Test-TPS-Agents-Real.ps1") | Where-Object { Test-Path $_ }
if ($testFiles.Count -eq 0) {
  "Test content" | Out-File -FilePath "test-agent-tps-file.txt" -Encoding UTF8 -ErrorAction SilentlyContinue
  $testFiles = @("test-agent-tps-file.txt")
}

Write-Host "`n🚀 Starting $TestType test with REAL agent tools..." -ForegroundColor Yellow
Write-Host ""

$endTime = (Get-Date).AddSeconds($DurationSeconds)
$transactionCounter = 0
$jobs = New-Object System.Collections.ArrayList

function Invoke-RealAgentTool {
  param(
    [string]$ToolName,
    [hashtable]$Parameters,
    [string]$RawrXDPath
  )

  $startTime = Get-Date
  $result = @{ Success = $false; ResponseTime = 0; Error = $null }

  try {
    # Load RawrXD in job context and call real agent tool
    $script:SkipGUIInit = $true
    $script:WindowsFormsAvailable = $false
    $ErrorActionPreference = 'SilentlyContinue'

    # Source RawrXD
    . $RawrXDPath -CliMode 2>&1 | Out-Null

    # Call the real Invoke-AgentTool function
    if (Get-Command Invoke-AgentTool -ErrorAction SilentlyContinue) {
      $toolResult = Invoke-AgentTool -ToolName $ToolName -Parameters $Parameters

      if ($toolResult -and (-not $toolResult.Error) -and (-not ($toolResult.success -eq $false))) {
        $result.Success = $true
      } else {
        $result.Error = if ($toolResult.Error) { $toolResult.Error } else { "Tool returned failure" }
      }
    } else {
      $result.Error = "Invoke-AgentTool function not available"
    }

    $result.ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
  }
  catch {
    $result.Error = $_.Exception.Message
    $result.ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
  }

  return $result
}

while ((Get-Date) -lt $endTime) {
  for ($i = 0; $i -lt $ConcurrentRequests; $i++) {
    $transactionCounter++
    $transactionId = "TXN-$transactionCounter"

    # Rotate through available tools
    $toolIndex = $transactionCounter % $availableTools.Count
    $toolName = $availableTools[$toolIndex]

    # Prepare parameters based on tool
    $params = @{}
    switch ($toolName) {
      "read_file" {
        $testFile = $testFiles[$transactionCounter % $testFiles.Count]
        if (Test-Path $testFile) {
          $params = @{path = $testFile}
        } else {
          continue
        }
      }
      "list_directory" {
        $params = @{path = "."}
      }
      "write_file" {
        $testPath = "test-agent-tps-$transactionId.txt"
        $params = @{path = $testPath; content = "Test content for TPS test at $(Get-Date)"}
      }
      default {
        # Try with empty params for other tools
        $params = @{}
      }
    }

    $job = Start-Job -ScriptBlock {
      param($ToolName, $Params, $TxnId, $RawrXDPath)

      $startTime = Get-Date
      $result = @{ Success = $false; ResponseTime = 0; Error = $null; TransactionId = $TxnId; ToolName = $ToolName }

      try {
        # Load RawrXD in job
        $script:SkipGUIInit = $true
        $script:WindowsFormsAvailable = $false
        $ErrorActionPreference = 'SilentlyContinue'

        # Load RawrXD by reading and executing
        $rawrContent = Get-Content $RawrXDPath -Raw -ErrorAction Stop
        $scriptBlock = [scriptblock]::Create($rawrContent)

        # Execute in sub-scope
        & {
          $script:SkipGUIInit = $true
          $script:WindowsFormsAvailable = $false
          $ErrorActionPreference = 'SilentlyContinue'
          & $scriptBlock
        } 2>&1 | Out-Null

        # Call real agent tool
        if (Get-Command Invoke-AgentTool -ErrorAction SilentlyContinue) {
          $toolResult = Invoke-AgentTool -ToolName $ToolName -Parameters $Params

          if ($toolResult -and (-not $toolResult.Error) -and (-not ($toolResult.success -eq $false))) {
            $result.Success = $true
          } else {
            $result.Error = if ($toolResult.Error) { $toolResult.Error } else { "Tool returned failure" }
          }
        } else {
          $result.Error = "Invoke-AgentTool not available"
        }

        $result.ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
      }
      catch {
        $result.Error = $_.Exception.Message
        $result.ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
      }

      return $result
    } -ArgumentList $toolName, $params, $transactionId, $RawrXDPath

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
      $null = $script:ResponseTimes.Add($result.ResponseTime)

      # Track per-tool metrics
      if (-not $script:ToolResponseTimes.ContainsKey($result.ToolName)) {
        $script:ToolResponseTimes[$result.ToolName] = New-Object System.Collections.ArrayList
      }
      $null = $script:ToolResponseTimes[$result.ToolName].Add($result.ResponseTime)
    }
    else {
      $script:FailedTransactions++
      $null = $script:ErrorDetails.Add(@{
        TransactionId = $result.TransactionId
        Error = $result.Error
        Tool = $toolName
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

# Tool-specific metrics
if ($script:ToolResponseTimes.Count -gt 0) {
  Write-Host "`n🔧 Tool Performance:" -ForegroundColor Cyan
  foreach ($toolName in $script:ToolResponseTimes.Keys) {
    $toolTimes = $script:ToolResponseTimes[$toolName]
    if ($toolTimes.Count -gt 0) {
      $toolAvg = [math]::Round(($toolTimes | Measure-Object -Average).Average, 2)
      Write-Host "   $toolName : $toolAvg ms avg ($($toolTimes.Count) calls)" -ForegroundColor White
    }
  }
}

# Generate report
if ($OutputPath) {
  $report = @"
# ⚡ TPS Test Results - Real Agent System
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

## Tool Performance
"@

  if ($script:ToolResponseTimes.Count -gt 0) {
    foreach ($toolName in $script:ToolResponseTimes.Keys) {
      $toolTimes = $script:ToolResponseTimes[$toolName]
      if ($toolTimes.Count -gt 0) {
        $toolAvg = [math]::Round(($toolTimes | Measure-Object -Average).Average, 2)
        $toolMin = [math]::Round(($toolTimes | Measure-Object -Minimum).Minimum, 2)
        $toolMax = [math]::Round(($toolTimes | Measure-Object -Maximum).Maximum, 2)
        $report += "### $toolName`n"
        $report += "- Calls: $($toolTimes.Count)`n"
        $report += "- Average: $toolAvg ms`n"
        $report += "- Min: $toolMin ms`n"
        $report += "- Max: $toolMax ms`n`n"
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

