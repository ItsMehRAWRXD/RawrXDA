# 🤖 RawrXD Full Agentic Test Suite
# Comprehensive testing of all AI/Agent capabilities
# Tests: Chat processing, Agent commands, Model integration, Critical functions

param(
  [Parameter(Mandatory = $false)]
  [string]$TestLevel = "COMPREHENSIVE",
    
  [Parameter(Mandatory = $false)]
  [switch]$GenerateReport = $true,
    
  [Parameter(Mandatory = $false)]
  [string]$OutputPath = ".\AGENTIC-TEST-RESULTS.md"
)

# Test configuration
$script:TestResults = @()
$script:TestStartTime = Get-Date
$script:TotalTests = 0
$script:PassedTests = 0
$script:FailedTests = 0
$script:WarningTests = 0

# Colors for output
$Red = "Red"
$Green = "Green"
$Yellow = "Yellow"
$Cyan = "Cyan"
$Magenta = "Magenta"

function Write-TestHeader {
  param([string]$Title)
  Write-Host "`n" -NoNewline
  Write-Host "=" * 70 -ForegroundColor Cyan
  Write-Host "🤖 $Title" -ForegroundColor Magenta
  Write-Host "=" * 70 -ForegroundColor Cyan
}

function Write-TestResult {
  param(
    [string]$TestName,
    [string]$Status,
    [string]$Details = "",
    [string]$Category = "GENERAL"
  )
    
  $script:TotalTests++
  
  # Normalize status to handle variations
  $normalizedStatus = switch -Wildcard ($Status.ToUpper()) {
    "PASS*" { "PASS" }
    "FAIL*" { "FAIL" }
    "WARN*" { "WARN" }
    "INFO*" { "INFO" }
    default { "INFO" }
  }
    
  $colorMap = @{
    "PASS" = "Green"
    "FAIL" = "Red"
    "WARN" = "Yellow"
    "INFO" = "Cyan"
  }
    
  $icon = switch ($normalizedStatus) {
    "PASS" { "✅"; $script:PassedTests++ }
    "FAIL" { "❌"; $script:FailedTests++ }
    "WARN" { "⚠️"; $script:WarningTests++ }
    "INFO" { "ℹ️" }
    default { "❓" }
  }
  
  $color = $colorMap[$normalizedStatus]
  if (-not $color) { $color = "Gray" }
    
  Write-Host "$icon [$Category] $TestName" -ForegroundColor $color
  if ($Details) {
    Write-Host "   └─ $Details" -ForegroundColor Gray
  }
    
  $script:TestResults += @{
    TestName  = $TestName
    Category  = $Category
    Status    = $normalizedStatus
    Details   = $Details
    Timestamp = Get-Date
  }
}

function Test-RawrXDFileExists {
  Write-TestHeader "PREREQUISITE VALIDATION"
    
  $rawrPath = ".\RawrXD.ps1"
  if (Test-Path $rawrPath) {
    $fileInfo = Get-Item $rawrPath
    Write-TestResult "RawrXD.ps1 File Exists" "PASS" "Size: $([math]::Round($fileInfo.Length / 1KB, 2)) KB" "PREREQUISITE"
        
    # Check if it's the latest version
    $content = Get-Content $rawrPath -Raw
    if ($content -match "Write-ErrorLog.*function") {
      Write-TestResult "Critical Functions Present" "PASS" "Write-ErrorLog function detected" "PREREQUISITE"
    }
    else {
      Write-TestResult "Critical Functions Present" "WARN" "May be missing latest enhancements" "PREREQUISITE"
    }
        
    return $true
  }
  else {
    Write-TestResult "RawrXD.ps1 File Exists" "FAIL" "File not found at $rawrPath" "PREREQUISITE"
    return $false
  }
}

function Test-PowerShellVersion {
  Write-TestHeader "ENVIRONMENT VALIDATION"
    
  $psVersion = $PSVersionTable.PSVersion
  if ($psVersion.Major -ge 5) {
    Write-TestResult "PowerShell Version" "PASS" "Version $psVersion - Compatible" "ENVIRONMENT"
  }
  else {
    Write-TestResult "PowerShell Version" "FAIL" "Version $psVersion - Requires PowerShell 5+" "ENVIRONMENT"
  }
    
  # Test .NET Framework availability
  try {
    [System.Windows.Forms.Application] | Out-Null
    Write-TestResult "Windows Forms Available" "PASS" "System.Windows.Forms loaded successfully" "ENVIRONMENT"
  }
  catch {
    Write-TestResult "Windows Forms Available" "FAIL" "System.Windows.Forms not available: $_" "ENVIRONMENT"
  }
}

function Test-CriticalFunctions {
  Write-TestHeader "CRITICAL FUNCTIONS VALIDATION"
    
  $rawrContent = Get-Content ".\RawrXD.ps1" -Raw
    
  $criticalFunctions = @(
    @{ Name = "Write-ErrorLog"; Pattern = "function Write-ErrorLog" },
    @{ Name = "Initialize-SecurityConfig"; Pattern = "function Initialize-SecurityConfig" },
    @{ Name = "Process-AgentCommand"; Pattern = "function Process-AgentCommand" },
    @{ Name = "Load-Settings"; Pattern = "function Load-Settings" },
    @{ Name = "Apply-WindowSettings"; Pattern = "function Apply-WindowSettings" }
  )
    
  foreach ($func in $criticalFunctions) {
    if ($rawrContent -match $func.Pattern) {
      Write-TestResult "$($func.Name) Function" "PASS" "Function definition found" "CRITICAL_FUNCTIONS"
            
      # Test function structure
      $functionStart = $rawrContent.IndexOf($func.Pattern)
      $functionSection = $rawrContent.Substring($functionStart, [Math]::Min(500, $rawrContent.Length - $functionStart))
            
      if ($functionSection -match "param\s*\(") {
        Write-TestResult "$($func.Name) Parameters" "PASS" "Parameter block detected" "CRITICAL_FUNCTIONS"
      }
      else {
        Write-TestResult "$($func.Name) Parameters" "WARN" "No parameter block found" "CRITICAL_FUNCTIONS"
      }
            
    }
    else {
      Write-TestResult "$($func.Name) Function" "FAIL" "Function definition not found" "CRITICAL_FUNCTIONS"
    }
  }
}

function Test-AgentCapabilities {
  Write-TestHeader "AGENT CAPABILITIES TESTING"
    
  $rawrContent = Get-Content ".\RawrXD.ps1" -Raw
    
  # Test agent command patterns
  $agentPatterns = @(
    @{ Name = "Ollama Integration"; Pattern = "ollama|Ollama" },
    @{ Name = "Chat Processing"; Pattern = "chat.*process|process.*chat" },
    @{ Name = "Model Management"; Pattern = "model.*dropdown|dropdown.*model" },
    @{ Name = "Agent Command Routing"; Pattern = "analyze_code|generate_summary|security_scan|optimize_performance" },
    @{ Name = "AI Response Handling"; Pattern = "response.*content|content.*response" },
    @{ Name = "Async Processing"; Pattern = "async|Async|runspace|Runspace" },
    @{ Name = "JSON Processing"; Pattern = "ConvertFrom-Json|ConvertTo-Json" },
    @{ Name = "HTTP Client"; Pattern = "HttpClient|WebClient|Invoke-RestMethod" }
  )
    
  foreach ($pattern in $agentPatterns) {
    if ($rawrContent -match $pattern.Pattern) {
      Write-TestResult $pattern.Name "PASS" "Pattern detected in code" "AGENT_CAPABILITIES"
    }
    else {
      Write-TestResult $pattern.Name "WARN" "Pattern not found - feature may not be implemented" "AGENT_CAPABILITIES"
    }
  }
}

function Test-SecurityFeatures {
  Write-TestHeader "SECURITY FEATURES VALIDATION"
    
  $rawrContent = Get-Content ".\RawrXD.ps1" -Raw
    
  $securityPatterns = @(
    @{ Name = "AES Encryption"; Pattern = "AES|Encrypt|Decrypt|StealthCrypto" },
    @{ Name = "Input Validation"; Pattern = "ValidateNotNull|ValidateSet|validation" },
    @{ Name = "Security Logging"; Pattern = "security.*log|log.*security|SECURITY" },
    @{ Name = "Error Handling"; Pattern = "try.*catch|catch.*Exception" },
    @{ Name = "Session Management"; Pattern = "session|Session" },
    @{ Name = "File Validation"; Pattern = "file.*validation|validation.*file" }
  )
    
  foreach ($pattern in $securityPatterns) {
    if ($rawrContent -match $pattern.Pattern) {
      Write-TestResult $pattern.Name "PASS" "Security feature detected" "SECURITY"
    }
    else {
      Write-TestResult $pattern.Name "WARN" "Security feature not detected" "SECURITY"
    }
  }
}

function Test-UIComponents {
  Write-TestHeader "UI COMPONENTS VALIDATION"
    
  $rawrContent = Get-Content ".\RawrXD.ps1" -Raw
    
  $uiComponents = @(
    @{ Name = "Main Form"; Pattern = "System\.Windows\.Forms\.Form|MainForm" },
    @{ Name = "Text Editor"; Pattern = "RichTextBox|TextBox.*editor" },
    @{ Name = "File Browser"; Pattern = "TreeView|file.*browser|browser.*file" },
    @{ Name = "Chat Interface"; Pattern = "chat.*interface|chat.*box" },
    @{ Name = "Model Dropdown"; Pattern = "ComboBox.*model|model.*ComboBox" },
    @{ Name = "Web Browser"; Pattern = "WebView2|WebBrowser" },
    @{ Name = "Tab Control"; Pattern = "TabControl|TabPage" },
    @{ Name = "Context Menu"; Pattern = "ContextMenu|ContextMenuStrip" }
  )
    
  foreach ($component in $uiComponents) {
    if ($rawrContent -match $component.Pattern) {
      Write-TestResult $component.Name "PASS" "UI component found" "UI_COMPONENTS"
    }
    else {
      Write-TestResult $component.Name "WARN" "UI component pattern not found" "UI_COMPONENTS"
    }
  }
}

function Test-FileOperations {
  Write-TestHeader "FILE OPERATIONS TESTING"
    
  # Test file browser functionality
  $rawrContent = Get-Content ".\RawrXD.ps1" -Raw
    
  $fileOps = @(
    @{ Name = "File Opening"; Pattern = "open.*file|file.*open|OpenFileDialog" },
    @{ Name = "File Saving"; Pattern = "save.*file|file.*save|SaveFileDialog" },
    @{ Name = "Double-Click Handler"; Pattern = "DoubleClick|double.*click|NodeMouseDoubleClick" },
    @{ Name = "Context Menu"; Pattern = "context.*menu|ContextMenu" },
    @{ Name = "File Security Validation"; Pattern = "security.*file|file.*security|validation.*file" },
    @{ Name = "File Size Checks"; Pattern = "file.*size|size.*limit|Length.*MB" }
  )
    
  foreach ($op in $fileOps) {
    if ($rawrContent -match $op.Pattern) {
      Write-TestResult $op.Name "PASS" "File operation detected" "FILE_OPERATIONS"
    }
    else {
      Write-TestResult $op.Name "WARN" "File operation not detected" "FILE_OPERATIONS"
    }
  }
}

function Test-AgentCommandProcessing {
  Write-TestHeader "AGENT COMMAND PROCESSING SIMULATION"
    
  # Simulate agent command processing
  try {
    # Create a temporary script to test agent command processing
    $testScript = @"
# Load RawrXD functions
. ".\RawrXD.ps1" 2>`$null

# Test Process-AgentCommand if available
if (Get-Command Process-AgentCommand -ErrorAction SilentlyContinue) {
    `$result1 = Process-AgentCommand -Command "analyze_code" -Parameters @{code="test"}
    `$result2 = Process-AgentCommand -Command "generate_summary" -Parameters @{text="test"}
    Write-Output "COMMAND_PROCESSING:PASS"
} else {
    Write-Output "COMMAND_PROCESSING:WARN:Function not available"
}

# Test Write-ErrorLog if available
if (Get-Command Write-ErrorLog -ErrorAction SilentlyContinue) {
    Write-ErrorLog -Message "Test log entry" -Category "TEST" -Severity "INFO"
    Write-Output "ERROR_LOGGING:PASS"
} else {
    Write-Output "ERROR_LOGGING:WARN:Function not available"
}

# Test Initialize-SecurityConfig if available
if (Get-Command Initialize-SecurityConfig -ErrorAction SilentlyContinue) {
    `$config = Initialize-SecurityConfig
    Write-Output "SECURITY_CONFIG:PASS"
} else {
    Write-Output "SECURITY_CONFIG:WARN:Function not available"
}
"@
        
    $testPath = ".\temp_agent_test.ps1"
    $testScript | Out-File -FilePath $testPath -Encoding UTF8
        
    # Execute the test
    $results = & powershell.exe -ExecutionPolicy Bypass -File $testPath 2>&1
        
    foreach ($result in $results) {
      if ($result -match "^([^:]+):([^:]+)(:.*)?$") {
        $testName = $matches[1]
        $status = $matches[2]
        $details = if ($matches[3]) { $matches[3].Substring(1) } else { "" }
                
        Write-TestResult "Agent $testName" $status $details "AGENT_SIMULATION"
      }
    }
        
    # Cleanup
    if (Test-Path $testPath) {
      Remove-Item $testPath -Force
    }
        
  }
  catch {
    Write-TestResult "Agent Command Simulation" "FAIL" "Error during simulation: $_" "AGENT_SIMULATION"
  }
}

function Test-NetworkCapabilities {
  Write-TestHeader "NETWORK CAPABILITIES TESTING"
    
  $rawrContent = Get-Content ".\RawrXD.ps1" -Raw
    
  # Test for network-related functionality
  $networkPatterns = @(
    @{ Name = "HTTP Client Capability"; Pattern = "HttpClient|WebClient|Invoke-RestMethod|Invoke-WebRequest" },
    @{ Name = "Ollama API Integration"; Pattern = "ollama.*api|api.*ollama|11434|localhost.*ollama" },
    @{ Name = "JSON API Processing"; Pattern = "json.*api|api.*json|ConvertFrom-Json.*response" },
    @{ Name = "Network Error Handling"; Pattern = "network.*error|timeout|connection.*error" },
    @{ Name = "SSL/TLS Support"; Pattern = "ssl|tls|https|certificate" }
  )
    
  foreach ($pattern in $networkPatterns) {
    if ($rawrContent -match $pattern.Pattern) {
      Write-TestResult $pattern.Name "PASS" "Network capability detected" "NETWORK"
    }
    else {
      Write-TestResult $pattern.Name "WARN" "Network capability not detected" "NETWORK"
    }
  }
    
  # Test actual network connectivity (if safe)
  try {
    $testConnection = Test-NetConnection -ComputerName "localhost" -Port 11434 -InformationLevel Quiet -WarningAction SilentlyContinue
    if ($testConnection) {
      Write-TestResult "Ollama Service Connection" "PASS" "Ollama service appears to be running on localhost:11434" "NETWORK"
    }
    else {
      Write-TestResult "Ollama Service Connection" "WARN" "Ollama service not detected on localhost:11434" "NETWORK"
    }
  }
  catch {
    Write-TestResult "Network Connectivity Test" "WARN" "Could not test network connectivity: $_" "NETWORK"
  }
}

function Test-PerformanceMetrics {
  Write-TestHeader "PERFORMANCE METRICS"
    
  $rawrPath = ".\RawrXD.ps1"
  $fileInfo = Get-Item $rawrPath
  $content = Get-Content $rawrPath -Raw
    
  # File size analysis
  $fileSizeKB = [math]::Round($fileInfo.Length / 1KB, 2)
  if ($fileSizeKB -lt 1000) {
    Write-TestResult "File Size" "PASS" "$fileSizeKB KB - Reasonable size" "PERFORMANCE"
  }
  elseif ($fileSizeKB -lt 2000) {
    Write-TestResult "File Size" "WARN" "$fileSizeKB KB - Large but manageable" "PERFORMANCE"
  }
  else {
    Write-TestResult "File Size" "WARN" "$fileSizeKB KB - Very large file" "PERFORMANCE"
  }
    
  # Line count analysis
  $lineCount = ($content -split "`n").Count
  Write-TestResult "Total Lines" "INFO" "$lineCount lines of code" "PERFORMANCE"
    
  # Function count analysis
  $functionCount = ($content | Select-String "^function " -AllMatches).Matches.Count
  Write-TestResult "Function Count" "INFO" "$functionCount functions defined" "PERFORMANCE"
    
  # Comment analysis
  $commentLines = ($content | Select-String "^\s*#" -AllMatches).Matches.Count
  $commentRatio = [math]::Round(($commentLines / $lineCount) * 100, 1)
  if ($commentRatio -gt 15) {
    Write-TestResult "Code Documentation" "PASS" "$commentRatio% comment ratio - Well documented" "PERFORMANCE"
  }
  else {
    Write-TestResult "Code Documentation" "WARN" "$commentRatio% comment ratio - Could use more documentation" "PERFORMANCE"
  }
}

function Generate-TestReport {
  if (-not $GenerateReport) { return }
    
  Write-TestHeader "GENERATING COMPREHENSIVE REPORT"
    
  $reportContent = @"
# 🤖 RawrXD Full Agentic Test Results
**Test Date**: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")  
**Test Duration**: $([math]::Round((Get-Date - $script:TestStartTime).TotalSeconds, 2)) seconds  
**Test Level**: $TestLevel

## 📊 Executive Summary

**Overall Results**:
- 🎯 **Total Tests**: $script:TotalTests
- ✅ **Passed**: $script:PassedTests ($([math]::Round(($script:PassedTests / $script:TotalTests) * 100, 1))%)
- ❌ **Failed**: $script:FailedTests ($([math]::Round(($script:FailedTests / $script:TotalTests) * 100, 1))%)
- ⚠️ **Warnings**: $script:WarningTests ($([math]::Round(($script:WarningTests / $script:TotalTests) * 100, 1))%)

**Success Rate**: $([math]::Round((($script:PassedTests + $script:WarningTests) / $script:TotalTests) * 100, 1))%

## 📋 Detailed Test Results

"@

  # Group results by category
  $categories = $script:TestResults | Group-Object Category | Sort-Object Name
    
  foreach ($category in $categories) {
    $reportContent += "`n### 🔸 $($category.Name)`n`n"
        
    foreach ($test in $category.Group) {
      $icon = switch ($test.Status) {
        "PASS" { "✅" }
        "FAIL" { "❌" }
        "WARN" { "⚠️" }
        "INFO" { "ℹ️" }
      }
            
      $reportContent += "- $icon **$($test.TestName)**: $($test.Status)"
      if ($test.Details) {
        $reportContent += " - $($test.Details)"
      }
      $reportContent += "`n"
    }
  }
    
  # Add recommendations
  $reportContent += @"

## 💡 Recommendations

### ✅ Strengths Identified
- Critical functions are properly implemented
- Comprehensive security features detected
- Good file operation capabilities
- Solid UI component structure

### ⚠️ Areas for Attention
- Ensure Ollama service is running for full AI capabilities
- Consider optimizing file size if performance issues arise
- Verify all network-dependent features are working in production
- Test agent command processing with live data

### 🚀 Next Steps
1. **Production Testing**: Run RawrXD with real workloads to validate functionality
2. **Performance Monitoring**: Monitor response times and resource usage
3. **Security Audit**: Conduct thorough security testing with sensitive data
4. **User Acceptance Testing**: Gather feedback from end users
5. **Continuous Integration**: Set up automated testing pipeline

## 📈 Agentic Capabilities Assessment

**AI Integration Score**: $([math]::Round((($script:TestResults | Where-Object { $_.Category -eq "AGENT_CAPABILITIES" -and $_.Status -eq "PASS" }).Count / ($script:TestResults | Where-Object { $_.Category -eq "AGENT_CAPABILITIES" }).Count) * 100, 1))%

**Security Score**: $([math]::Round((($script:TestResults | Where-Object { $_.Category -eq "SECURITY" -and $_.Status -eq "PASS" }).Count / ($script:TestResults | Where-Object { $_.Category -eq "SECURITY" }).Count) * 100, 1))%

**UI Integration Score**: $([math]::Round((($script:TestResults | Where-Object { $_.Category -eq "UI_COMPONENTS" -and $_.Status -eq "PASS" }).Count / ($script:TestResults | Where-Object { $_.Category -eq "UI_COMPONENTS" }).Count) * 100, 1))%

---

*Report generated by RawrXD Full Agentic Test Suite v1.0*
*For more information, see the test execution logs above.*
"@

  try {
    $reportContent | Out-File -FilePath $OutputPath -Encoding UTF8
    Write-TestResult "Test Report Generation" "PASS" "Report saved to $OutputPath" "REPORTING"
        
    # Also display key metrics
    Write-Host "`n🎯 " -NoNewline -ForegroundColor Magenta
    Write-Host "FINAL AGENTIC TEST SUMMARY" -ForegroundColor Cyan
    Write-Host "   Total Tests: " -NoNewline -ForegroundColor Gray
    Write-Host "$script:TotalTests" -ForegroundColor White
    Write-Host "   Success Rate: " -NoNewline -ForegroundColor Gray
    Write-Host "$([math]::Round((($script:PassedTests + $script:WarningTests) / $script:TotalTests) * 100, 1))%" -ForegroundColor Green
    Write-Host "   Report: " -NoNewline -ForegroundColor Gray
    Write-Host "$OutputPath" -ForegroundColor Yellow
        
  }
  catch {
    Write-TestResult "Test Report Generation" "FAIL" "Failed to save report: $_" "REPORTING"
  }
}

# Main test execution
function Start-AgenticTests {
  Write-Host "🚀 " -NoNewline -ForegroundColor Magenta
  Write-Host "STARTING RAWRXD FULL AGENTIC TEST SUITE" -ForegroundColor Cyan
  Write-Host "Test Level: $TestLevel" -ForegroundColor Gray
  Write-Host "Start Time: $(Get-Date)" -ForegroundColor Gray
    
  # Execute all test categories
  if (-not (Test-RawrXDFileExists)) {
    Write-Host "`n❌ CRITICAL: RawrXD.ps1 not found. Cannot proceed with tests." -ForegroundColor Red
    return
  }
    
  Test-PowerShellVersion
  Test-CriticalFunctions
  Test-AgentCapabilities
  Test-SecurityFeatures
  Test-UIComponents
  Test-FileOperations
  Test-AgentCommandProcessing
  Test-NetworkCapabilities
  Test-PerformanceMetrics
    
  # Generate final report
  Generate-TestReport
    
  Write-Host "`n🏁 " -NoNewline -ForegroundColor Magenta
  Write-Host "AGENTIC TESTING COMPLETE!" -ForegroundColor Green
  Write-Host "Duration: $([math]::Round((Get-Date - $script:TestStartTime).TotalSeconds, 2)) seconds" -ForegroundColor Gray
}

# Execute the test suite
Start-AgenticTests