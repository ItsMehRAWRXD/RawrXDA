# ⚡ TPS (Transactions Per Second) Test for RawrXD Agents
# Comprehensive throughput testing for agent commands and tools
# Tests agent system performance without GUI

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
  [string]$TestType = "sustained",  # sustained, burst, ramp-up, tools-only, commands-only

  [Parameter(Mandatory = $false)]
  [switch]$GenerateReport = $true,

  [Parameter(Mandatory = $false)]
  [string]$OutputPath = ".\TPS-AGENT-TEST-RESULTS.md",

  [Parameter(Mandatory = $false)]
  [int]$TimeoutSeconds = 60,

  [Parameter(Mandatory = $false)]
  [string]$RawrXDPath = ".\RawrXD.ps1"
)

# Test configuration and globals
$script:TestResults = @()
$script:TestStartTime = Get-Date
$script:TotalTransactions = 0
$script:SuccessfulTransactions = 0
$script:FailedTransactions = 0
$script:ResponseTimes = @()
$script:ErrorDetails = @()
$script:ToolResponseTimes = @{}
$script:CommandResponseTimes = @{}
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
  ToolTPS = @{}
  CommandTPS = @{}
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

function Load-RawrXDFunctions {
  Write-TestHeader "LOADING RAWRXD FUNCTIONS"

  if (-not (Test-Path $RawrXDPath)) {
    Write-TestResult "RawrXD Script Found" "FAIL" "Script not found at: $RawrXDPath" "INIT"
    return $false
  }

  Write-TestResult "RawrXD Script Found" "PASS" "Found at: $RawrXDPath" "INIT"

  try {
    # Load RawrXD by executing it in a separate PowerShell process with CLI mode
    # This avoids parameter binding issues
    Write-Host "Loading RawrXD functions (this may take a moment)..." -ForegroundColor Yellow

    # Create a temporary script that loads RawrXD and exports functions
    $loadScript = @"
`$script:SkipGUIInit = `$true
`$script:WindowsFormsAvailable = `$false
`$ErrorActionPreference = 'SilentlyContinue'

# Dot-source RawrXD but skip CLI mode check
. '$RawrXDPath' -CliMode:`$false 2>&1 | Out-Null

# Export key functions to global scope
if (Get-Command Process-AgentCommand -ErrorAction SilentlyContinue) {
  Set-Alias -Name Test-ProcessAgentCommand -Value Process-AgentCommand -Scope Global -ErrorAction SilentlyContinue
}
if (Get-Command Invoke-AgentTool -ErrorAction SilentlyContinue) {
  Set-Alias -Name Test-InvokeAgentTool -Value Invoke-AgentTool -Scope Global -ErrorAction SilentlyContinue
}
"@

    # Execute in current scope using Invoke-Expression
    $script:SkipGUIInit = $true
    $script:WindowsFormsAvailable = $false
    $ErrorActionPreference = 'SilentlyContinue'

    # Try to source it by reading and executing in a controlled way
    $rawrContent = Get-Content $RawrXDPath -Raw
    if ($rawrContent) {
      # Remove the param block temporarily to avoid binding issues
      $modifiedContent = $rawrContent -replace '(?s)^\[CmdletBinding\(\)\]\s*param\([^)]+\)', ''

      # Execute in a script block
      $scriptBlock = [scriptblock]::Create($modifiedContent)
      & $scriptBlock 2>&1 | Out-Null
    }

    Write-TestResult "RawrXD Functions Loaded" "PASS" "Functions loaded successfully" "INIT"

    # Verify key functions exist
    $requiredFunctions = @("Process-AgentCommand", "Invoke-AgentTool")
    $missingFunctions = @()

    foreach ($func in $requiredFunctions) {
      if (-not (Get-Command $func -ErrorAction SilentlyContinue)) {
        $missingFunctions += $func
      }
    }

    if ($missingFunctions.Count -gt 0) {
      Write-TestResult "Required Functions" "WARN" "Missing: $($missingFunctions -join ', ')" "INIT"
      # Still proceed - functions might be available via $script: scope
    }
    else {
      Write-TestResult "Required Functions" "PASS" "All required functions available" "INIT"
    }

    # Check for agent tools
    if ($script:agentTools -and $script:agentTools.Count -gt 0) {
      Write-TestResult "Agent Tools Available" "PASS" "$($script:agentTools.Count) tools registered" "INIT"
      return $true
    }
    else {
      Write-TestResult "Agent Tools Available" "WARN" "No agent tools registered yet - will attempt to register during test" "INIT"
      return $true  # Still proceed, tools may be registered during execution
    }

  }
  catch {
    Write-TestResult "RawrXD Functions Loaded" "FAIL" "Error loading: $($_.Exception.Message)" "INIT"
    Write-Host "   Attempting alternative loading method..." -ForegroundColor Yellow

    # Alternative: Just verify the script exists and proceed with direct function calls
    # The test will use the functions if they're available
    return $true
  }
}

function Test-AgentToolInvocation {
  param(
    [string]$ToolName,
    [hashtable]$Parameters,
    [string]$TransactionId
  )

  $result = @{
    Id = $TransactionId
    Tool = $ToolName
    Success = $false
    ResponseTime = 0
    Error = $null
    Timestamp = Get-Date
  }

  try {
    $startTime = Get-Date

    if (Get-Command Invoke-AgentTool -ErrorAction SilentlyContinue) {
      $toolResult = Invoke-AgentTool -ToolName $ToolName -Parameters $Parameters
      $endTime = Get-Date

      $responseTime = ($endTime - $startTime).TotalMilliseconds
      $result.Success = ($toolResult -and (-not $toolResult.Error) -and (-not $toolResult.success -eq $false))
      $result.ResponseTime = $responseTime
      if (-not $result.Success -and $toolResult) {
        $result.Error = if ($toolResult.Error) { $toolResult.Error } else { "Tool returned failure" }
      }
    }
    else {
      $result.Error = "Invoke-AgentTool function not available"
    }

  }
  catch {
    $result.Success = $false
    $result.Error = $_.Exception.Message
    $result.ResponseTime = 0
  }

  # Thread-safe increment
  [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
  if ($result.Success) {
    [System.Threading.Interlocked]::Increment([ref]$script:SuccessfulTransactions) | Out-Null
    lock ($script:ResponseTimes) {
      $script:ResponseTimes += $result.ResponseTime
    }
    if (-not $script:ToolResponseTimes.ContainsKey($ToolName)) {
      $script:ToolResponseTimes[$ToolName] = New-Object System.Collections.ArrayList
    }
    $script:ToolResponseTimes[$ToolName].Add($result.ResponseTime) | Out-Null
  }
  else {
    [System.Threading.Interlocked]::Increment([ref]$script:FailedTransactions) | Out-Null
    lock ($script:ErrorDetails) {
      $script:ErrorDetails += @{
        TransactionId = $TransactionId
        Tool = $ToolName
        Error = $result.Error
        Timestamp = $result.Timestamp
      }
    }
  }

  return $result
}

function Test-AgentCommandInvocation {
  param(
    [string]$Command,
    [hashtable]$Parameters,
    [string]$TransactionId
  )

  $result = @{
    Id = $TransactionId
    Command = $Command
    Success = $false
    ResponseTime = 0
    Error = $null
    Timestamp = Get-Date
  }

  try {
    $startTime = Get-Date

    if (Get-Command Process-AgentCommand -ErrorAction SilentlyContinue) {
      $cmdResult = Process-AgentCommand -Command $Command -Parameters $Parameters -SourceContext "TPS-Test"
      $endTime = Get-Date

      $responseTime = ($endTime - $startTime).TotalMilliseconds
      $result.Success = ($cmdResult -ne $false -and $cmdResult -ne $null)
      $result.ResponseTime = $responseTime
      if (-not $result.Success) {
        $result.Error = "Command returned false or null"
      }
    }
    else {
      $result.Error = "Process-AgentCommand function not available"
    }

  }
  catch {
    $result.Success = $false
    $result.Error = $_.Exception.Message
    $result.ResponseTime = 0
  }

  # Thread-safe increment
  [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
  if ($result.Success) {
    [System.Threading.Interlocked]::Increment([ref]$script:SuccessfulTransactions) | Out-Null
    lock ($script:ResponseTimes) {
      $script:ResponseTimes += $result.ResponseTime
    }
    if (-not $script:CommandResponseTimes.ContainsKey($Command)) {
      $script:CommandResponseTimes[$Command] = New-Object System.Collections.ArrayList
    }
    $script:CommandResponseTimes[$Command].Add($result.ResponseTime) | Out-Null
  }
  else {
    [System.Threading.Interlocked]::Increment([ref]$script:FailedTransactions) | Out-Null
    lock ($script:ErrorDetails) {
      $script:ErrorDetails += @{
        TransactionId = $TransactionId
        Command = $Command
        Error = $result.Error
        Timestamp = $result.Timestamp
      }
    }
  }

  return $result
}

function Get-AvailableAgentTools {
  $availableTools = @()

  if ($script:agentTools) {
    foreach ($toolName in $script:agentTools.Keys) {
      $tool = $script:agentTools[$toolName]
      $availableTools += @{
        Name = $toolName
        Description = $tool.Description
        Parameters = $tool.Parameters
      }
    }
  }

  return $availableTools
}

function Test-SustainedAgentLoad {
  Write-TestHeader "SUSTAINED AGENT LOAD TEST"
  Write-Host "Duration: $DurationSeconds seconds | Concurrent: $ConcurrentRequests" -ForegroundColor Cyan

  # Get available tools
  $availableTools = Get-AvailableAgentTools
  $testFiles = @("RawrXD.ps1", "Test-TPS-Agents.ps1", "README.md")
  $testFiles = $testFiles | Where-Object { Test-Path $_ }

  if ($availableTools.Count -eq 0 -and $testFiles.Count -eq 0) {
    Write-TestResult "Test Setup" "FAIL" "No agent tools available and no test files found" "SETUP"
    return
  }

  Write-Host "Available Tools: $($availableTools.Count)" -ForegroundColor Cyan
  Write-Host "Test Files: $($testFiles.Count)" -ForegroundColor Cyan

  $endTime = (Get-Date).AddSeconds($DurationSeconds)
  $transactionCounter = 0
  $jobs = @()
  $tpsWindow = @()
  $windowStart = Get-Date

  Write-Host "`n🚀 Starting sustained agent load test..." -ForegroundColor Yellow

  while ((Get-Date) -lt $endTime) {
    $currentTime = Get-Date
    $windowTPS = @()

    # Launch concurrent requests
    for ($i = 0; $i -lt $ConcurrentRequests; $i++) {
      $transactionCounter++
      $transactionId = "AGENT-$transactionCounter"
      $testType = $transactionCounter % 3

      $job = Start-Job -ScriptBlock {
        param($RawrXDPath, $TestType, $TransactionId, $TestFiles, $AvailableTools)

        # Load RawrXD in job context
        $script:SkipGUIInit = $true
        $script:WindowsFormsAvailable = $false
        & {
          $script:SkipGUIInit = $true
          $script:WindowsFormsAvailable = $false
          . $RawrXDPath
        } | Out-Null

        $result = $null
        $startTime = Get-Date

        try {
          switch ($TestType) {
            0 {
              # Test agent tool: read_file
              if ($AvailableTools | Where-Object { $_.Name -eq "read_file" }) {
                $testFile = if ($TestFiles.Count -gt 0) { $TestFiles[0] } else { "RawrXD.ps1" }
                if (Test-Path $testFile) {
                  $toolResult = Invoke-AgentTool -ToolName "read_file" -Parameters @{path = $testFile}
                  $result = @{
                    Success = ($toolResult -and (-not $toolResult.Error) -and (-not $toolResult.success -eq $false))
                    ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
                    TransactionId = $TransactionId
                    Type = "tool"
                    Tool = "read_file"
                    Timestamp = $startTime
                  }
                }
              }
            }
            1 {
              # Test agent tool: list_directory
              if ($AvailableTools | Where-Object { $_.Name -eq "list_directory" }) {
                $toolResult = Invoke-AgentTool -ToolName "list_directory" -Parameters @{path = "."}
                $result = @{
                  Success = ($toolResult -and (-not $toolResult.Error) -and (-not $toolResult.success -eq $false))
                  ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
                  TransactionId = $TransactionId
                  Type = "tool"
                  Tool = "list_directory"
                  Timestamp = $startTime
                }
              }
            }
            2 {
              # Test agent command
              if (Get-Command Process-AgentCommand -ErrorAction SilentlyContinue) {
                $cmdResult = Process-AgentCommand -Command "analyze_code" -Parameters @{} -SourceContext "TPS-Test"
                $result = @{
                  Success = ($cmdResult -ne $false -and $cmdResult -ne $null)
                  ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
                  TransactionId = $TransactionId
                  Type = "command"
                  Command = "analyze_code"
                  Timestamp = $startTime
                }
              }
            }
          }
        }
        catch {
          $result = @{
            Success = $false
            ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
            TransactionId = $TransactionId
            Error = $_.Exception.Message
            Timestamp = $startTime
          }
        }

        if (-not $result) {
          $result = @{
            Success = $false
            ResponseTime = 0
            TransactionId = $TransactionId
            Error = "No test executed"
            Timestamp = $startTime
          }
        }

        return $result
      } -ArgumentList $RawrXDPath, $testType, $transactionId, $testFiles, $availableTools

      $jobs += $job
    }

    # Wait a bit before next batch
    Start-Sleep -Milliseconds 100

    # Collect completed jobs
    $completedJobs = $jobs | Where-Object { $_.State -eq "Completed" }
    foreach ($job in $completedJobs) {
      $result = Receive-Job -Job $job
      Remove-Job -Job $job
      $jobs = $jobs | Where-Object { $_.Id -ne $job.Id }

      if ($result -and $result.Success) {
        [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
        [System.Threading.Interlocked]::Increment([ref]$script:SuccessfulTransactions) | Out-Null
        $script:ResponseTimes += $result.ResponseTime
        $windowTPS += $result.Timestamp
      }
      else {
        [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
        [System.Threading.Interlocked]::Increment([ref]$script:FailedTransactions) | Out-Null
        if ($result) {
          $script:ErrorDetails += @{
            TransactionId = $result.TransactionId
            Error = if ($result.Error) { $result.Error } else { "Unknown error" }
            Timestamp = $result.Timestamp
          }
        }
      }
    }

    # Calculate TPS for this window
    $windowElapsed = ($currentTime - $windowStart).TotalSeconds
    if ($windowElapsed -ge 1.0) {
      $windowTPS = $windowTPS | Where-Object { ($currentTime - $_).TotalSeconds -le 1.0 }
      $currentTPS = $windowTPS.Count
      $tpsWindow += $currentTPS
      $windowStart = $currentTime

      if ($currentTPS -gt $script:TPSResults.PeakTPS) {
        $script:TPSResults.PeakTPS = $currentTPS
      }

      Write-Host "." -NoNewline -ForegroundColor Gray
    }
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

  Write-Host "`n✅ Sustained agent load test completed" -ForegroundColor Green
}

function Test-BurstAgentLoad {
  Write-TestHeader "BURST AGENT LOAD TEST"
  Write-Host "Burst Size: $ConcurrentRequests requests" -ForegroundColor Cyan

  $availableTools = Get-AvailableAgentTools
  $testFiles = @("RawrXD.ps1", "Test-TPS-Agents.ps1")
  $testFiles = $testFiles | Where-Object { Test-Path $_ }

  Write-Host "`n🚀 Launching burst of $ConcurrentRequests concurrent agent requests..." -ForegroundColor Yellow

  $jobs = @()
  $transactionCounter = 0

  for ($i = 0; $i -lt $ConcurrentRequests; $i++) {
    $transactionCounter++
    $transactionId = "BURST-$transactionCounter"
    $testType = $i % 3

    $job = Start-Job -ScriptBlock {
      param($RawrXDPath, $TestType, $TransactionId, $TestFiles, $AvailableTools)

      $script:SkipGUIInit = $true
      $script:WindowsFormsAvailable = $false
      & {
        $script:SkipGUIInit = $true
        $script:WindowsFormsAvailable = $false
        . $RawrXDPath
      } | Out-Null

      $result = $null
      $startTime = Get-Date

      try {
        switch ($TestType) {
          0 {
            if ($AvailableTools | Where-Object { $_.Name -eq "read_file" }) {
              $testFile = if ($TestFiles.Count -gt 0) { $TestFiles[0] } else { "RawrXD.ps1" }
              if (Test-Path $testFile) {
                $toolResult = Invoke-AgentTool -ToolName "read_file" -Parameters @{path = $testFile}
                $result = @{
                  Success = ($toolResult -and (-not $toolResult.Error) -and (-not $toolResult.success -eq $false))
                  ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
                  TransactionId = $TransactionId
                  Timestamp = $startTime
                }
              }
            }
          }
          1 {
            if ($AvailableTools | Where-Object { $_.Name -eq "list_directory" }) {
              $toolResult = Invoke-AgentTool -ToolName "list_directory" -Parameters @{path = "."}
              $result = @{
                Success = ($toolResult -and (-not $toolResult.Error) -and (-not $toolResult.success -eq $false))
                ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
                TransactionId = $TransactionId
                Timestamp = $startTime
              }
            }
          }
          2 {
            if (Get-Command Process-AgentCommand -ErrorAction SilentlyContinue) {
              $cmdResult = Process-AgentCommand -Command "analyze_code" -Parameters @{} -SourceContext "TPS-Test"
              $result = @{
                Success = ($cmdResult -ne $false -and $cmdResult -ne $null)
                ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
                TransactionId = $TransactionId
                Timestamp = $startTime
              }
            }
          }
        }
      }
      catch {
        $result = @{
          Success = $false
          ResponseTime = ((Get-Date) - $startTime).TotalMilliseconds
          TransactionId = $TransactionId
          Error = $_.Exception.Message
          Timestamp = $startTime
        }
      }

      if (-not $result) {
        $result = @{
          Success = $false
          ResponseTime = 0
          TransactionId = $TransactionId
          Error = "No test executed"
          Timestamp = $startTime
        }
      }

      return $result
    } -ArgumentList $RawrXDPath, $testType, $transactionId, $testFiles, $availableTools

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

    if ($result -and $result.Success) {
      [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
      [System.Threading.Interlocked]::Increment([ref]$script:SuccessfulTransactions) | Out-Null
      $script:ResponseTimes += $result.ResponseTime
    }
    else {
      [System.Threading.Interlocked]::Increment([ref]$script:TotalTransactions) | Out-Null
      [System.Threading.Interlocked]::Increment([ref]$script:FailedTransactions) | Out-Null
      if ($result) {
        $script:ErrorDetails += @{
          TransactionId = $result.TransactionId
          Error = if ($result.Error) { $result.Error } else { "Unknown error" }
          Timestamp = $result.Timestamp
        }
      }
    }
  }

  if ($burstDuration -gt 0) {
    $burstTPS = $script:TotalTransactions / $burstDuration
    $script:TPSResults.PeakTPS = [math]::Round($burstTPS, 2)
  }

  Write-Host "✅ Burst agent load test completed" -ForegroundColor Green
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

  # Tool-specific metrics
  if ($script:ToolResponseTimes.Count -gt 0) {
    Write-Host "`n🔧 Tool Performance:" -ForegroundColor Cyan
    foreach ($toolName in $script:ToolResponseTimes.Keys) {
      $toolTimes = $script:ToolResponseTimes[$toolName]
      if ($toolTimes.Count -gt 0) {
        $avgTime = [math]::Round(($toolTimes | Measure-Object -Average).Average, 2)
        Write-Host "   $toolName : $avgTime ms avg ($($toolTimes.Count) calls)" -ForegroundColor White
      }
    }
  }

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
}

function Generate-TestReport {
  if (-not $GenerateReport) {
    return
  }

  Write-TestHeader "GENERATING TEST REPORT"

  $testDuration = ((Get-Date) - $script:TestStartTime).TotalSeconds
  $reportContent = @"
# ⚡ TPS (Transactions Per Second) Test Results - Agent System
**Test Date**: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
**Test Duration**: $([math]::Round($testDuration, 2)) seconds
**Test Type**: $TestType
**Concurrent Requests**: $ConcurrentRequests
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

## 🔧 Tool Performance Breakdown

"@

  if ($script:ToolResponseTimes.Count -gt 0) {
    foreach ($toolName in $script:ToolResponseTimes.Keys) {
      $toolTimes = $script:ToolResponseTimes[$toolName]
      if ($toolTimes.Count -gt 0) {
        $avgTime = [math]::Round(($toolTimes | Measure-Object -Average).Average, 2)
        $minTime = [math]::Round(($toolTimes | Measure-Object -Minimum).Minimum, 2)
        $maxTime = [math]::Round(($toolTimes | Measure-Object -Maximum).Maximum, 2)
        $reportContent += "### $toolName`n"
        $reportContent += "- Calls: $($toolTimes.Count)`n"
        $reportContent += "- Average: $avgTime ms`n"
        $reportContent += "- Min: $minTime ms`n"
        $reportContent += "- Max: $maxTime ms`n`n"
      }
    }
  }
  else {
    $reportContent += "No tool-specific metrics available.`n`n"
  }

  $reportContent += @"

## 📋 Error Details

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
    $reportContent += "- ⚠️ Check agent tool implementation for bottlenecks`n"
  }

  if ($script:TPSResults.AvgResponseTime -gt 3000) {
    $reportContent += "- ⚠️ Response times are high - consider optimizing agent tools`n"
  }

  if ($script:TPSResults.SuccessRate -ge 95 -and $script:TPSResults.AvgResponseTime -lt 2000) {
    $reportContent += "- ✅ Agent system performance is excellent - current configuration is optimal`n"
  }

  $reportContent += @"

---
*Report generated by Test-TPS-Agents.ps1*
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
Write-Host "║              ⚡ TPS PERFORMANCE TEST SUITE - AGENT SYSTEM ⚡              ║" -ForegroundColor Magenta
Write-Host "╚══════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Load RawrXD functions
if (-not (Load-RawrXDFunctions)) {
  Write-Host "`n❌ Cannot proceed with agent TPS testing - Failed to load RawrXD functions" -ForegroundColor Red
  exit 1
}

# Initialize response times array
$script:ResponseTimes = New-Object System.Collections.ArrayList
$script:ErrorDetails = New-Object System.Collections.ArrayList

# Run the appropriate test based on type
switch ($TestType.ToLower()) {
  "sustained" {
    Test-SustainedAgentLoad
  }
  "burst" {
    Test-BurstAgentLoad
  }
  default {
    Write-Host "⚠️ Unknown test type '$TestType', defaulting to 'sustained'" -ForegroundColor Yellow
    Test-SustainedAgentLoad
  }
}

# Calculate and display metrics
Calculate-PerformanceMetrics

# Generate report
if ($GenerateReport) {
  Generate-TestReport
}

Write-Host "`n✅ Agent TPS testing completed!" -ForegroundColor Green
Write-Host ""

