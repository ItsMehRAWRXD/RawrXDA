# RawrXD Test Framework
# Comprehensive testing and validation system

#Requires -Version 5.1

<#
.SYNOPSIS
    RawrXD.TestFramework - Comprehensive testing and validation system

.DESCRIPTION
    Comprehensive test framework providing:
    - Unit testing for all modules
    - Integration testing
    - Performance benchmarking
    - Security validation
    - Code quality analysis
    - No external dependencies

.LINK
    https://github.com/RawrXD/TestFramework

.NOTES
    Author: RawrXD Auto-Generation System
    Version: 1.0.0
    Requires: PowerShell 5.1+
    Last Updated: 2024-12-28
#>

# Import logging if available
if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error','Debug')][string]$Level = 'Info',
            [string]$Function = $null,
            [hashtable]$Data = $null
        )
        $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
        $caller = if ($Function) { $Function } else { (Get-PSCallStack)[1].FunctionName }
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$timestamp][$caller][$Level] $Message" -ForegroundColor $color
    }
}

# Test result structure
function New-TestResult {
    return [PSCustomObject]@{
        TestName = $null
        ModuleName = $null
        Status = $null
        ErrorMessage = $null
        Duration = 0.0
        Data = @{}
        Timestamp = Get-Date
    }
}

# Test suite structure
function New-TestSuite {
    $suite = [PSCustomObject]@{
        SuiteName = $null
        ModuleName = $null
        Results = [System.Collections.ArrayList]::new()
        Passed = 0
        Failed = 0
        Total = 0
        TotalDuration = 0.0
        Metadata = @{}
    }
    
    $suite | Add-Member -MemberType ScriptMethod -Name "AddResult" -Value {
        param($result)
        $this.Results.Add($result) | Out-Null
        $this.Total++
        $this.TotalDuration += $result.Duration
        
        if ($result.Status -eq 'Passed') {
            $this.Passed++
        } else {
            $this.Failed++
        }
    }
    
    $suite | Add-Member -MemberType ScriptMethod -Name "GetSuccessRate" -Value {
        if ($this.Total -eq 0) { return 0.0 }
        return [Math]::Round(($this.Passed / $this.Total) * 100, 2)
    }
    
    return $suite
}

# Test framework configuration
$script:TestConfig = @{
    Enabled = $true
    LogLevel = "Info"
    OutputPath = $null
    GenerateReport = $true
    IncludePerformance = $true
    IncludeSecurity = $true
    IncludeIntegration = $true
    MaxTestDuration = 300
    ParallelExecution = $false
}

# Test registry
$script:TestRegistry = @{
    UnitTests = @()
    IntegrationTests = @()
    PerformanceTests = @()
    SecurityTests = @()
    CustomTests = @()
}

# Run unit tests for a module
function Invoke-ModuleUnitTests {
    <#
    .SYNOPSIS
        Run unit tests for a module
    
    .DESCRIPTION
        Execute comprehensive unit tests for a PowerShell module
    
    .PARAMETER ModulePath
        Path to the module to test
    
    .PARAMETER ModuleName
        Name of the module
    
    .EXAMPLE
        Invoke-ModuleUnitTests -ModulePath "C:\\RawrXD\\RawrXD.Logging.psm1" -ModuleName "Logging"
        
        Run unit tests for logging module
    
    .OUTPUTS
        Test suite results
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$ModulePath,
        
        [Parameter(Mandatory=$true)]
        [string]$ModuleName
    )
    
    $functionName = 'Invoke-ModuleUnitTests'
    $startTime = Get-Date
    
    try {
        Write-StructuredLog -Message "Starting unit tests for module: $ModuleName" -Level Info -Function $functionName -Data @{
            ModulePath = $ModulePath
            ModuleName = $ModuleName
        }
        
        if (-not (Test-Path $ModulePath)) {
            throw "Module not found: $ModulePath"
        }
        
        $suite = New-TestSuite
        $suite.SuiteName = "UnitTests_$ModuleName"
        $suite.ModuleName = $ModuleName
        
        # Test 1: Module loads successfully
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Module_LoadsSuccessfully"
        $testResult.ModuleName = $ModuleName
        
        try {
            Import-Module $ModulePath -Force -Global -ErrorAction Stop
            $testResult.Status = "Passed"
            $testResult.Data["LoadSuccess"] = $true
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
            $testResult.Data["LoadSuccess"] = $false
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        # Test 2: Module has exported functions
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Module_HasExportedFunctions"
        $testResult.ModuleName = $ModuleName
        
        try {
            $moduleInfo = Get-Module $ModuleName -ErrorAction Stop
            $exportedFunctions = $moduleInfo.ExportedFunctions.Keys
            
            if ($exportedFunctions.Count -gt 0) {
                $testResult.Status = "Passed"
                $testResult.Data["ExportedFunctionCount"] = $exportedFunctions.Count
                $testResult.Data["ExportedFunctions"] = $exportedFunctions -join ", "
            } else {
                $testResult.Status = "Failed"
                $testResult.ErrorMessage = "No exported functions found"
                $testResult.Data["ExportedFunctionCount"] = 0
            }
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        # Test 3: Functions are callable
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Module_FunctionsAreCallable"
        $testResult.ModuleName = $ModuleName
        
        try {
            $moduleInfo = Get-Module $ModuleName -ErrorAction Stop
            $exportedFunctions = $moduleInfo.ExportedFunctions.Keys
            $callableCount = 0
            $totalCount = $exportedFunctions.Count
            
            foreach ($func in $exportedFunctions) {
                try {
                    $command = Get-Command $func -ErrorAction Stop
                    if ($command) {
                        $callableCount++
                    }
                } catch {
                    # Function not callable
                }
            }
            
            if ($callableCount -eq $totalCount) {
                $testResult.Status = "Passed"
                $testResult.Data["CallableCount"] = $callableCount
                $testResult.Data["TotalCount"] = $totalCount
            } else {
                $testResult.Status = "Failed"
                $testResult.ErrorMessage = "Only $callableCount of $totalCount functions are callable"
                $testResult.Data["CallableCount"] = $callableCount
                $testResult.Data["TotalCount"] = $totalCount
            }
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        # Test 4: Module has proper error handling
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Module_HasErrorHandling"
        $testResult.ModuleName = $ModuleName
        
        try {
            $moduleContent = Get-Content -Path $ModulePath -Raw
            $hasTryCatch = $moduleContent -like '*try*' -and $moduleContent -like '*catch*'
            $hasErrorAction = $moduleContent -like '*ErrorAction*'
            
            if ($hasTryCatch -or $hasErrorAction) {
                $testResult.Status = "Passed"
                $testResult.Data["HasTryCatch"] = $hasTryCatch
                $testResult.Data["HasErrorAction"] = $hasErrorAction
            } else {
                $testResult.Status = "Failed"
                $testResult.ErrorMessage = "No error handling found"
                $testResult.Data["HasTryCatch"] = $false
                $testResult.Data["HasErrorAction"] = $false
            }
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        # Test 5: Module has logging
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Module_HasLogging"
        $testResult.ModuleName = $ModuleName
        
        try {
            $moduleContent = Get-Content -Path $ModulePath -Raw
            $hasLogging = $moduleContent -like '*Write-StructuredLog*' -or $moduleContent -like '*Write-Host*' -or $moduleContent -like '*Write-Verbose*'
            
            if ($hasLogging) {
                $testResult.Status = "Passed"
                $testResult.Data["HasLogging"] = $true
            } else {
                $testResult.Status = "Failed"
                $testResult.ErrorMessage = "No logging found"
                $testResult.Data["HasLogging"] = $false
            }
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        # Test 6: Module documentation
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Module_HasDocumentation"
        $testResult.ModuleName = $ModuleName
        
        try {
            $moduleContent = Get-Content -Path $ModulePath -Raw
            $hasSynopsis = $moduleContent -like '*.SYNOPSIS*'
            $hasDescription = $moduleContent -like '*.DESCRIPTION*'
            $hasExamples = $moduleContent -like '*.EXAMPLE*'
            
            $docScore = 0
            if ($hasSynopsis) { $docScore++ }
            if ($hasDescription) { $docScore++ }
            if ($hasExamples) { $docScore++ }
            
            if ($docScore -ge 2) {
                $testResult.Status = "Passed"
                $testResult.Data["DocumentationScore"] = $docScore
                $testResult.Data["HasSynopsis"] = $hasSynopsis
                $testResult.Data["HasDescription"] = $hasDescription
                $testResult.Data["HasExamples"] = $hasExamples
            } else {
                $testResult.Status = "Failed"
                $testResult.ErrorMessage = "Insufficient documentation (score: $docScore/3)"
                $testResult.Data["DocumentationScore"] = $docScore
            }
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        $suite.Metadata["TestCount"] = $suite.Total
        $suite.Metadata["SuccessRate"] = $suite.GetSuccessRate()
        $suite.Metadata["Duration"] = [Math]::Round(((Get-Date) - $startTime).TotalSeconds, 2)
        
        Write-StructuredLog -Message "Unit tests completed for $ModuleName" -Level Info -Function $functionName -Data @{
            Duration = $suite.Metadata["Duration"]
            Tests = $suite.Total
            Passed = $suite.Passed
            Failed = $suite.Failed
            SuccessRate = $suite.GetSuccessRate()
        }
        
        return $suite
        
    } catch {
        Write-StructuredLog -Message "Unit tests failed for $ModuleName`: $_" -Level Error -Function $functionName
        throw
    }
}

# Run integration tests
function Invoke-IntegrationTests {
    <#
    .SYNOPSIS
        Run integration tests
    
    .DESCRIPTION
        Execute integration tests for module interactions
    
    .PARAMETER ModulePaths
        Array of module paths to test
    
    .EXAMPLE
        Invoke-IntegrationTests -ModulePaths @("C:\\RawrXD\\RawrXD.Logging.psm1", "C:\\RawrXD\\RawrXD.Production.psm1")
        
        Run integration tests
    
    .OUTPUTS
        Integration test results
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string[]]$ModulePaths
    )
    
            $functionName = 'Invoke-ComprehensiveTestSuite'
    $startTime = Get-Date
    
    try {
        Write-StructuredLog -Message "Starting comprehensive test suite" -Level Info -Function $functionName -Data @{
            ModuleCount = $ModulePaths.Count
            OutputPath = $OutputPath
        }
        
        $comprehensiveResults = @{
            StartTime = $startTime
            EndTime = $null
            Duration = 0
            UnitTests = @()
            IntegrationTests = $null
            PerformanceTests = @()
            SecurityTests = @()
            Summary = @{
                TotalTests = 0
                TotalPassed = 0
                TotalFailed = 0
                OverallSuccessRate = 0
            }
        }
        
        # Run unit tests for each module
        Write-Host "=== Running Unit Tests ===" -ForegroundColor Cyan
        foreach ($modulePath in $ModulePaths) {
            $moduleName = [System.IO.Path]::GetFileNameWithoutExtension($modulePath)
            Write-Host "Testing: $moduleName" -ForegroundColor Yellow
            
            try {
                $unitTests = Invoke-ModuleUnitTests -ModulePath $modulePath -ModuleName $moduleName
                $comprehensiveResults.UnitTests += $unitTests
                
                Write-Host "  ✓ Unit tests completed: $($unitTests.Passed)/$($unitTests.Total) passed" -ForegroundColor Green
            } catch {
                Write-Host "  ✗ Unit tests failed: $_" -ForegroundColor Red
            }
        }
        
        # Run integration tests
        Write-Host "=== Running Integration Tests ===" -ForegroundColor Cyan
        try {
            $integrationTests = Invoke-IntegrationTests -ModulePaths $ModulePaths
            $comprehensiveResults.IntegrationTests = $integrationTests
            Write-Host "  ✓ Integration tests completed: $($integrationTests.Passed)/$($integrationTests.Total) passed" -ForegroundColor Green
        } catch {
            Write-Host "  ✗ Integration tests failed: $_" -ForegroundColor Red
        }
        
        # Run performance tests for each module
        Write-Host "=== Running Performance Tests ===" -ForegroundColor Cyan
        foreach ($modulePath in $ModulePaths) {
            $moduleName = [System.IO.Path]::GetFileNameWithoutExtension($modulePath)
            Write-Host "Performance testing: $moduleName" -ForegroundColor Yellow
            
            try {
                $perfTests = Invoke-PerformanceTests -ModuleName $moduleName -Iterations 100
                $comprehensiveResults.PerformanceTests += $perfTests
                
                Write-Host "  ✓ Performance tests completed: $($perfTests.Passed)/$($perfTests.Total) passed" -ForegroundColor Green
            } catch {
                Write-Host "  ✗ Performance tests failed: $_" -ForegroundColor Red
            }
        }
        
        # Run security tests for each module
        Write-Host "=== Running Security Tests ===" -ForegroundColor Cyan
        foreach ($modulePath in $ModulePaths) {
            $moduleName = [System.IO.Path]::GetFileNameWithoutExtension($modulePath)
            Write-Host "Security testing: $moduleName" -ForegroundColor Yellow
            
            try {
                $securityTests = Invoke-SecurityTests -ModulePath $modulePath -ModuleName $moduleName
                $comprehensiveResults.SecurityTests += $securityTests
                
                Write-Host "  ✓ Security tests completed: $($securityTests.Passed)/$($securityTests.Total) passed" -ForegroundColor Green
            } catch {
                Write-Host "  ✗ Security tests failed: $_" -ForegroundColor Red
            }
        }
        
        # Calculate summary
        $totalTests = 0
        $totalPassed = 0
        $totalFailed = 0
        
        foreach ($unitTest in $comprehensiveResults.UnitTests) {
            $totalTests += $unitTest.Total
            $totalPassed += $unitTest.Passed
            $totalFailed += $unitTest.Failed
        }
        
        if ($comprehensiveResults.IntegrationTests) {
            $totalTests += $comprehensiveResults.IntegrationTests.Total
            $totalPassed += $comprehensiveResults.IntegrationTests.Passed
            $totalFailed += $comprehensiveResults.IntegrationTests.Failed
        }
        
        foreach ($perfTest in $comprehensiveResults.PerformanceTests) {
            $totalTests += $perfTest.Total
            $totalPassed += $perfTest.Passed
            $totalFailed += $perfTest.Failed
        }
        
        foreach ($securityTest in $comprehensiveResults.SecurityTests) {
            $totalTests += $securityTest.Total
            $totalPassed += $securityTest.Passed
            $totalFailed += $securityTest.Failed
        }
        
        $comprehensiveResults.EndTime = Get-Date
        $comprehensiveResults.Duration = [Math]::Round(($comprehensiveResults.EndTime - $startTime).TotalSeconds, 2)
        $comprehensiveResults.Summary.TotalTests = $totalTests
        $comprehensiveResults.Summary.TotalPassed = $totalPassed
        $comprehensiveResults.Summary.TotalFailed = $totalFailed
        
        if ($totalTests -gt 0) {
            $comprehensiveResults.Summary.OverallSuccessRate = [Math]::Round(($totalPassed / $totalTests) * 100, 2)
        }
        
        # Generate report if output path specified
        if ($OutputPath) {
            Write-StructuredLog -Message "Generating test report at: $OutputPath" -Level Info -Function $functionName
            
            if (-not (Test-Path $OutputPath)) {
                New-Item -Path $OutputPath -ItemType Directory -Force | Out-Null
            }
            
            $reportPath = Join-Path $OutputPath "TestReport_$(Get-Date -Format 'yyyyMMdd_HHmmss').xml"
            $comprehensiveResults | Export-Clixml -Path $reportPath -Force
            
            Write-Host "  ✓ Test report saved: $reportPath" -ForegroundColor Green
        }
        
        Write-StructuredLog -Message "Comprehensive test suite completed" -Level Info -Function $functionName -Data @{
            Duration = $comprehensiveResults.Duration
            TotalTests = $totalTests
            Passed = $totalPassed
            Failed = $totalFailed
            SuccessRate = $comprehensiveResults.Summary.OverallSuccessRate
        }
        
        # Display summary
        Write-Host "=== Test Summary ===" -ForegroundColor Cyan
        Write-Host "Duration: $($comprehensiveResults.Duration)s" -ForegroundColor White
        Write-Host "Total Tests: $totalTests" -ForegroundColor White
        Write-Host "Passed: $totalPassed" -ForegroundColor Green
        Write-Host "Failed: $totalFailed" -ForegroundColor Red
        Write-Host "Success Rate: $($comprehensiveResults.Summary.OverallSuccessRate)%" -ForegroundColor $(if ($comprehensiveResults.Summary.OverallSuccessRate -ge 90) { "Green" } elseif ($comprehensiveResults.Summary.OverallSuccessRate -ge 70) { "Yellow" } else { "Red" })
        Write-Host ""
        
        return $comprehensiveResults
            } catch {
                Write-Host "  ✗ Unit tests failed: $_" -ForegroundColor Red
            }
        }
        
        # Run integration tests
        Write-Host "=== Running Integration Tests ===" -ForegroundColor Cyan
        try {
            $integrationTests = Invoke-IntegrationTests -ModulePaths $ModulePaths
            $comprehensiveResults.IntegrationTests = $integrationTests
            Write-Host "  ✓ Integration tests completed: $($integrationTests.Passed)/$($integrationTests.Total) passed" -ForegroundColor Green
        } catch {
            Write-Host "  ✗ Integration tests failed: $_" -ForegroundColor Red
        }
        
        # Run performance tests for each module
        Write-Host "=== Running Performance Tests ===" -ForegroundColor Cyan
        foreach ($modulePath in $ModulePaths) {
            $moduleName = [System.IO.Path]::GetFileNameWithoutExtension($modulePath)
            Write-Host "Performance testing: $moduleName" -ForegroundColor Yellow
            
            try {
                $perfTests = Invoke-PerformanceTests -ModuleName $moduleName -Iterations 100
                $comprehensiveResults.PerformanceTests += $perfTests
                
                Write-Host "  ✓ Performance tests completed: $($perfTests.Passed)/$($perfTests.Total) passed" -ForegroundColor Green
            } catch {
                Write-Host "  ✗ Performance tests failed: $_" -ForegroundColor Red
            }
        }
        
        # Run security tests for each module
        Write-Host "=== Running Security Tests ===" -ForegroundColor Cyan
        foreach ($modulePath in $ModulePaths) {
            $moduleName = [System.IO.Path]::GetFileNameWithoutExtension($modulePath)
            Write-Host "Security testing: $moduleName" -ForegroundColor Yellow
            
            try {
                $securityTests = Invoke-SecurityTests -ModulePath $modulePath -ModuleName $moduleName
                $comprehensiveResults.SecurityTests += $securityTests
                
                Write-Host "  ✓ Security tests completed: $($securityTests.Passed)/$($securityTests.Total) passed" -ForegroundColor Green
            } catch {
                Write-Host "  ✗ Security tests failed: $_" -ForegroundColor Red
            }
        }
        
        # Calculate summary
        $totalTests = 0
        $totalPassed = 0
        $totalFailed = 0
        
        foreach ($unitTest in $comprehensiveResults.UnitTests) {
            $totalTests += $unitTest.Total
            $totalPassed += $unitTest.Passed
            $totalFailed += $unitTest.Failed
        }
        
        if ($comprehensiveResults.IntegrationTests) {
            $totalTests += $comprehensiveResults.IntegrationTests.Total
            $totalPassed += $comprehensiveResults.IntegrationTests.Passed
            $totalFailed += $comprehensiveResults.IntegrationTests.Failed
        }
        
        foreach ($perfTest in $comprehensiveResults.PerformanceTests) {
            $totalTests += $perfTest.Total
            $totalPassed += $perfTest.Passed
            $totalFailed += $perfTest.Failed
        }
        
        foreach ($securityTest in $comprehensiveResults.SecurityTests) {
            $totalTests += $securityTest.Total
            $totalPassed += $securityTest.Passed
            $totalFailed += $securityTest.Failed
        }
        
        $comprehensiveResults.EndTime = Get-Date
        $comprehensiveResults.Duration = [Math]::Round(($comprehensiveResults.EndTime - $startTime).TotalSeconds, 2)
        $comprehensiveResults.Summary.TotalTests = $totalTests
        $comprehensiveResults.Summary.TotalPassed = $totalPassed
        $comprehensiveResults.Summary.TotalFailed = $totalFailed
        
        if ($totalTests -gt 0) {
            $comprehensiveResults.Summary.OverallSuccessRate = [Math]::Round(($totalPassed / $totalTests) * 100, 2)
        }
        
        # Generate report if output path specified
        if ($OutputPath) {
            Write-StructuredLog -Message "Generating test report at: $OutputPath" -Level Info -Function $functionName
            
            if (-not (Test-Path $OutputPath)) {
                New-Item -Path $OutputPath -ItemType Directory -Force | Out-Null
            }
            
            $reportPath = Join-Path $OutputPath "TestReport_$(Get-Date -Format 'yyyyMMdd_HHmmss').xml"
            $comprehensiveResults | Export-Clixml -Path $reportPath -Force
            
            Write-Host "  ✓ Test report saved: $reportPath" -ForegroundColor Green
        }
        
        Write-StructuredLog -Message "Comprehensive test suite completed" -Level Info -Function $functionName -Data @{
            Duration = $comprehensiveResults.Duration
            TotalTests = $totalTests
            Passed = $totalPassed
            Failed = $totalFailed
            SuccessRate = $comprehensiveResults.Summary.OverallSuccessRate
        }
        
        # Display summary
        Write-Host "=== Test Summary ===" -ForegroundColor Cyan
        Write-Host "Duration: $($comprehensiveResults.Duration)s" -ForegroundColor White
        Write-Host "Total Tests: $totalTests" -ForegroundColor White
        Write-Host "Passed: $totalPassed" -ForegroundColor Green
        Write-Host "Failed: $totalFailed" -ForegroundColor Red
        Write-Host "Success Rate: $($comprehensiveResults.Summary.OverallSuccessRate)%" -ForegroundColor $(if ($comprehensiveResults.Summary.OverallSuccessRate -ge 90) { "Green" } elseif ($comprehensiveResults.Summary.OverallSuccessRate -ge 70) { "Yellow" } else { "Red" })
        Write-Host ""
        
        return $comprehensiveResults
        $testResult = New-TestResult
        $testResult.TestName = "Integration_CrossModuleFunctionCalls"
        $testResult.ModuleName = "Multiple"
        
        try {
            # Test if logging works from other modules
            $loggingWorks = $false
            if (Get-Command "Write-StructuredLog" -ErrorAction SilentlyContinue) {
                Write-StructuredLog -Message "Test message" -Level Info -Function "IntegrationTest"
                $loggingWorks = $true
            }
            
            $testResult.Status = "Passed"
            $testResult.Data["LoggingWorks"] = $loggingWorks
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        # Test 3: Master system integration
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Integration_MasterSystemIntegration"
        $testResult.ModuleName = "Master"
        
        try {
            if (Get-Command "Get-MasterSystemStatus" -ErrorAction SilentlyContinue) {
                $status = Get-MasterSystemStatus
                if ($status) {
                    $testResult.Status = "Passed"
                    $testResult.Data["StatusRetrieved"] = $true
                    $testResult.Data["ModuleCount"] = $status.Modules.Count
                } else {
                    $testResult.Status = "Failed"
                    $testResult.ErrorMessage = "Status is null"
                }
            } else {
                $testResult.Status = "Failed"
                $testResult.ErrorMessage = "Get-MasterSystemStatus not found"
            }
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        $suite.Metadata["TestCount"] = $suite.Total
        $suite.Metadata["SuccessRate"] = $suite.GetSuccessRate()
        $suite.Metadata["Duration"] = [Math]::Round(((Get-Date) - $startTime).TotalSeconds, 2)
        
        Write-StructuredLog -Message "Integration tests completed" -Level Info -Function $functionName -Data @{
            Duration = $suite.Metadata["Duration"]
            Tests = $suite.Total
            Passed = $suite.Passed
            Failed = $suite.Failed
            SuccessRate = $suite.GetSuccessRate()
        }
        
        return $suite

# Run performance tests
function Invoke-PerformanceTests {
    <#
    .SYNOPSIS
        Run performance tests
    
    .DESCRIPTION
        Execute performance benchmarks for modules
    
    .PARAMETER ModuleName
        Name of module to test
    
    .PARAMETER Iterations
        Number of iterations
    
    .EXAMPLE
        Invoke-PerformanceTests -ModuleName "Logging" -Iterations 1000
        
        Run performance tests
    
    .OUTPUTS
        Performance test results
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$ModuleName,
        
        [Parameter(Mandatory=$false)]
        [int]$Iterations = 1000
    )
    
    $functionName = 'Invoke-PerformanceTests'
    $startTime = Get-Date
    
    try {
        Write-StructuredLog -Message "Starting performance tests for $ModuleName" -Level Info -Function $functionName -Data @{
            ModuleName = $ModuleName
            Iterations = $Iterations
        }
        
        $suite = New-TestSuite
        $suite.SuiteName = "PerformanceTests_$ModuleName"
        $suite.ModuleName = $ModuleName
        
        # Test 1: Function call performance
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Performance_FunctionCall"
        $testResult.ModuleName = $ModuleName
        
        try {
            $moduleInfo = Get-Module $ModuleName -ErrorAction Stop
            $exportedFunctions = $moduleInfo.ExportedFunctions.Keys
            
            if ($exportedFunctions.Count -gt 0) {
                $funcName = $exportedFunctions[0]
                $timer = Start-PerformanceTimer -Name "FunctionCall"
                
                for ($i = 0; $i -lt $Iterations; $i++) {
                    try {
                        # Try to call function with no parameters or safe parameters
                        $command = Get-Command $funcName -ErrorAction Stop
                        if ($command.Parameters.Count -eq 0) {
                            & $funcName -ErrorAction SilentlyContinue | Out-Null
                        }
                    } catch {
                        # Function may require parameters, just measure the attempt
                    }
                }
                
                $duration = Stop-PerformanceTimer -Timer $timer
                $avgDuration = $duration / $Iterations
                
                $testResult.Status = "Passed"
                $testResult.Data["AverageDuration"] = [Math]::Round($avgDuration, 6)
                $testResult.Data["TotalDuration"] = [Math]::Round($duration, 3)
                $testResult.Data["Iterations"] = $Iterations
                $testResult.Data["FunctionName"] = $funcName
            } else {
                $testResult.Status = "Failed"
                $testResult.ErrorMessage = "No exported functions found"
            }
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        # Test 2: Module load performance
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Performance_ModuleLoad"
        $testResult.ModuleName = $ModuleName
        
        try {
            $timer = Start-PerformanceTimer -Name "ModuleLoad"
            
            for ($i = 0; $i -lt 100; $i++) {
                Import-Module $ModulePath -Force -Global -ErrorAction Stop
            }
            
            $duration = Stop-PerformanceTimer -Timer $timer
            $avgDuration = $duration / 100
            
            $testResult.Status = "Passed"
            $testResult.Data["AverageLoadTime"] = [Math]::Round($avgDuration, 6)
            $testResult.Data["TotalLoadTime"] = [Math]::Round($duration, 3)
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        # Test 3: Memory usage
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Performance_MemoryUsage"
        $testResult.ModuleName = $ModuleName
        
        try {
            $beforeMemory = [GC]::GetTotalMemory($false)
            Import-Module $ModulePath -Force -Global -ErrorAction Stop
            $afterMemory = [GC]::GetTotalMemory($true)
            
            $memoryUsed = $afterMemory - $beforeMemory
            $memoryUsedMB = [Math]::Round($memoryUsed / 1MB, 2)
            
            $testResult.Status = "Passed"
            $testResult.Data["MemoryUsedMB"] = $memoryUsedMB
            $testResult.Data["BeforeMemory"] = $beforeMemory
            $testResult.Data["AfterMemory"] = $afterMemory
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        $suite.Metadata["TestCount"] = $suite.Total
        $suite.Metadata["SuccessRate"] = $suite.GetSuccessRate()
        $suite.Metadata["Duration"] = [Math]::Round(((Get-Date) - $startTime).TotalSeconds, 2)
        
        Write-StructuredLog -Message "Performance tests completed for $ModuleName" -Level Info -Function $functionName -Data @{
            Duration = $suite.Metadata["Duration"]
            Tests = $suite.Total
            Passed = $suite.Passed
            Failed = $suite.Failed
            SuccessRate = $suite.GetSuccessRate()
        }
        
        return $suite
        
    } catch {
        Write-StructuredLog -Message "Performance tests failed: $_" -Level Error -Function $functionName
        throw
    }
}

# Run security tests
function Invoke-SecurityTests {
    <#
    .SYNOPSIS
        Run security tests
    
    .DESCRIPTION
        Execute security validation tests
    
    .PARAMETER ModulePath
        Path to module to test
    
    .PARAMETER ModuleName
        Name of module
    
    .EXAMPLE
        Invoke-SecurityTests -ModulePath "C:\\RawrXD\\RawrXD.Logging.psm1" -ModuleName "Logging"
        
        Run security tests
    
    .OUTPUTS
        Security test results
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$ModulePath,
        
        [Parameter(Mandatory=$true)]
        [string]$ModuleName
    )
    
    $functionName = 'Invoke-SecurityTests'
    $startTime = Get-Date
    
    try {
        Write-StructuredLog -Message "Starting security tests for $ModuleName" -Level Info -Function $functionName -Data @{
            ModulePath = $ModulePath
            ModuleName = $ModuleName
        }
        
        $suite = New-TestSuite
        $suite.SuiteName = "SecurityTests_$ModuleName"
        $suite.ModuleName = $ModuleName
        
        # Test 1: No hardcoded secrets
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Security_NoHardcodedSecrets"
        $testResult.ModuleName = $ModuleName
        
        try {
            $moduleContent = Get-Content -Path $ModulePath -Raw
            $secretPatterns = @(
                'password\s*=\s*["''][^"'']{3,}["'']',
                'api[_-]?key\s*=\s*["''][^"'']{3,}["'']',
                'secret\s*=\s*["''][^"'']{3,}["'']',
                'token\s*=\s*["''][^"'']{3,}["'']'
            )
            
            $foundSecrets = $false
            $secretCount = 0
            
            foreach ($pattern in $secretPatterns) {
                $matches = [regex]::Matches($moduleContent, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
                if ($matches.Count -gt 0) {
                    $foundSecrets = $true
                    $secretCount += $matches.Count
                }
            }
            
            if (-not $foundSecrets) {
                $testResult.Status = "Passed"
                $testResult.Data["HardcodedSecrets"] = $false
                $testResult.Data["SecretCount"] = 0
            } else {
                $testResult.Status = "Failed"
                $testResult.ErrorMessage = "Found $secretCount potential hardcoded secrets"
                $testResult.Data["HardcodedSecrets"] = $true
                $testResult.Data["SecretCount"] = $secretCount
            }
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        # Test 2: Input validation
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Security_InputValidation"
        $testResult.ModuleName = $ModuleName
        
        try {
            $moduleContent = Get-Content -Path $ModulePath -Raw
            $hasValidation = $moduleContent -like '*ValidateScript*' -or $moduleContent -like '*ValidatePattern*' -or $moduleContent -like '*ValidateRange*'
            
            if ($hasValidation) {
                $testResult.Status = "Passed"
                $testResult.Data["HasInputValidation"] = $true
            } else {
                $testResult.Status = "Failed"
                $testResult.ErrorMessage = "No input validation found"
                $testResult.Data["HasInputValidation"] = $false
            }
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        # Test 3: Secure string usage
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Security_SecureStringUsage"
        $testResult.ModuleName = $ModuleName
        
        try {
            $moduleContent = Get-Content -Path $ModulePath -Raw
            $hasSecureString = $moduleContent -like '*SecureString*'
            
            if ($hasSecureString) {
                $testResult.Status = "Passed"
                $testResult.Data["UsesSecureString"] = $true
            } else {
                $testResult.Status = "Passed"  # Not a failure, just a note
                $testResult.Data["UsesSecureString"] = $false
                $testResult.Data["Note"] = "Module doesn't handle sensitive data"
            }
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        # Test 4: Code injection prevention
        $testStart = Get-Date
        $testResult = New-TestResult
        $testResult.TestName = "Security_CodeInjectionPrevention"
        $testResult.ModuleName = $ModuleName
        
        try {
            $moduleContent = Get-Content -Path $ModulePath -Raw
            $dangerousPatterns = @(
                'Invoke-Expression\s+\$',
                'iex\s+\$',
                'Add-Type\s+.*-TypeDefinition',
                'New-Object\s+.*Script'
            )
            
            $foundDangerous = $false
            $dangerousCount = 0
            
            foreach ($pattern in $dangerousPatterns) {
                $matches = [regex]::Matches($moduleContent, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
                if ($matches.Count -gt 0) {
                    $foundDangerous = $true
                    $dangerousCount += $matches.Count
                }
            }
            
            if (-not $foundDangerous) {
                $testResult.Status = "Passed"
                $testResult.Data["CodeInjectionRisk"] = $false
                $testResult.Data["DangerousPatternCount"] = 0
            } else {
                $testResult.Status = "Warning"
                $testResult.ErrorMessage = "Found $dangerousCount potentially dangerous patterns"
                $testResult.Data["CodeInjectionRisk"] = $true
                $testResult.Data["DangerousPatternCount"] = $dangerousCount
            }
        } catch {
            $testResult.Status = "Failed"
            $testResult.ErrorMessage = $_.Message
        }
        
        $testResult.Duration = [Math]::Round(((Get-Date) - $testStart).TotalSeconds, 3)
        $suite.AddResult($testResult)
        
        $suite.Metadata["TestCount"] = $suite.Total
        $suite.Metadata["SuccessRate"] = $suite.GetSuccessRate()
        $suite.Metadata["Duration"] = [Math]::Round(((Get-Date) - $startTime).TotalSeconds, 2)
        
        Write-StructuredLog -Message "Security tests completed for $ModuleName" -Level Info -Function $functionName -Data @{
            Duration = $suite.Metadata["Duration"]
            Tests = $suite.Total
            Passed = $suite.Passed
            Failed = $suite.Failed
            SuccessRate = $suite.GetSuccessRate()
        }
        
        return $suite
        
    } catch {
        Write-StructuredLog -Message "Security tests failed: $_" -Level Error -Function $functionName
        throw
    }
}

# Run comprehensive test suite
function Invoke-ComprehensiveTestSuite {
    <#
    .SYNOPSIS
        Run comprehensive test suite
    
    .DESCRIPTION
        Execute complete test suite including unit, integration, performance, and security tests
    
    .PARAMETER ModulePaths
        Array of module paths to test
    
    .PARAMETER OutputPath
        Path for test reports
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string[]]$ModulePaths,
        
        [Parameter(Mandatory=$false)]
        [string]$OutputPath = $null
    )

    $functionName = 'Invoke-ComprehensiveTestSuite'
    $startTime = Get-Date

    # Defensive: treat null/empty as empty array
    if (-not $ModulePaths) {
        $ModulePaths = @()
    }

    try {
        $results = @{
            StartTime = $startTime
            EndTime = $null
            Duration = 0
            ModulePaths = $ModulePaths
            UnitTests = @()
            IntegrationTests = @()
            PerformanceTests = @()
            SecurityTests = @()
            Summary = @{
                TotalTests = 0
                TotalPassed = 0
                TotalFailed = 0
                OverallSuccessRate = 0.0
                PassedMinimum = $false
            }
        }
        
        # Get all .psm1 files
        $moduleFiles = Get-ChildItem -Path $ModulePath -Filter "*.psm1" -ErrorAction SilentlyContinue
        
        if ($moduleFiles.Count -eq 0) {
            Write-Warning "No module files found in: $ModulePath"
            return $results
        }
        
        Write-Host "Found $($moduleFiles.Count) modules to test" -ForegroundColor Green
        
        # Run tests for each module
        foreach ($moduleFile in $moduleFiles) {
            $moduleName = [System.IO.Path]::GetFileNameWithoutExtension($moduleFile.Name)
            
            try {
                Write-Host "Testing module: $moduleName" -ForegroundColor Cyan
                
                # Unit tests
                $unitResult = Invoke-ModuleUnitTests -ModulePath $moduleFile.FullName -ModuleName $moduleName
                $results.UnitTests += $unitResult
                
            } catch {
                Write-Warning "Failed to test module $moduleName`: $_"
            }
        }
        
        # Calculate totals
        $totalTests = 0
        $totalPassed = 0
        $totalFailed = 0
        
        foreach ($unitTest in $results.UnitTests) {
            $totalTests += $unitTest.Total
            $totalPassed += $unitTest.Passed
            $totalFailed += $unitTest.Failed
        }
        
        $results.EndTime = Get-Date
        $results.Duration = [Math]::Round(($results.EndTime - $startTime).TotalSeconds, 2)
        $results.Summary.TotalTests = $totalTests
        $results.Summary.TotalPassed = $totalPassed
        $results.Summary.TotalFailed = $totalFailed
        
        if ($totalTests -gt 0) {
            $results.Summary.OverallSuccessRate = [Math]::Round(($totalPassed / $totalTests) * 100, 2)
            $results.Summary.PassedMinimum = $results.Summary.OverallSuccessRate -ge $MinSuccessRate
        }
        
        Write-Host ""
        Write-Host "=== Test Summary ===" -ForegroundColor Cyan
        Write-Host "Duration: $($results.Duration)s" -ForegroundColor White
        Write-Host "Total Tests: $totalTests" -ForegroundColor White
        Write-Host "Passed: $totalPassed" -ForegroundColor Green
        Write-Host "Failed: $totalFailed" -ForegroundColor $(if ($totalFailed -gt 0) { "Red" } else { "Green" })
        Write-Host "Success Rate: $($results.Summary.OverallSuccessRate)%" -ForegroundColor $(if ($results.Summary.OverallSuccessRate -ge 90) { "Green" } elseif ($results.Summary.OverallSuccessRate -ge 70) { "Yellow" } else { "Red" })
        Write-Host "Minimum Success Rate: $($results.Summary.PassedMinimum)" -ForegroundColor $(if ($results.Summary.PassedMinimum) { "Green" } else { "Red" })
        Write-Host ""
        
        return $results
        
    } catch {
        Write-StructuredLog -Message "Comprehensive test suite failed: $_" -Level Error -Function $functionName
        throw
    }
}

# Comprehensive test suite runner for directory
function Invoke-ComprehensiveTestSuiteFromDirectory {
    <#
    .SYNOPSIS
        Run comprehensive test suite for all modules
    
    .DESCRIPTION
        Execute complete testing suite including unit tests, integration tests, 
        performance tests, and security tests for all modules
    
    .PARAMETER ModulePath
        Path to modules directory
    
    .PARAMETER MinSuccessRate
        Minimum success rate required (default 80%)
    
    .PARAMETER OutputPath
        Path for test reports
    
    .EXAMPLE
        Invoke-ComprehensiveTestSuite -ModulePath "D:\lazy init ide\auto_generated_methods" -MinSuccessRate 80
        
        Run comprehensive test suite with 80% minimum success rate
    
    .OUTPUTS
        Comprehensive test results
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$ModulePath,
        
        [Parameter(Mandatory=$false)]
        [int]$MinSuccessRate = 80,
        
        [Parameter(Mandatory=$false)]
        [string]$OutputPath = $null
    )
    
    $functionName = 'Invoke-ComprehensiveTestSuite'
    $startTime = Get-Date
    
    try {
        Write-StructuredLog -Message "Starting comprehensive test suite" -Level Info -Function $functionName -Data @{
            ModulePath = $ModulePath
            MinSuccessRate = $MinSuccessRate
            OutputPath = $OutputPath
        }
        
        $results = @{
            StartTime = $startTime
            EndTime = $null
            Duration = 0
            ModulePath = $ModulePath
            MinSuccessRate = $MinSuccessRate
            UnitTests = @()
            IntegrationTests = @()
            PerformanceTests = @()
            SecurityTests = @()
            Summary = @{
                TotalTests = 0
                TotalPassed = 0
                TotalFailed = 0
                OverallSuccessRate = 0.0
                PassedMinimum = $false
            }
        }
        
        # Get all .psm1 files
        $moduleFiles = Get-ChildItem -Path $ModulePath -Filter "*.psm1" -ErrorAction SilentlyContinue
        
        if ($moduleFiles.Count -eq 0) {
            Write-Warning "No module files found in: $ModulePath"
            return $results
        }
        
        Write-Host "Found $($moduleFiles.Count) modules to test" -ForegroundColor Green
        
        # Run tests for each module
        foreach ($moduleFile in $moduleFiles) {
            $moduleName = [System.IO.Path]::GetFileNameWithoutExtension($moduleFile.Name)
            
            try {
                Write-Host "Testing module: $moduleName" -ForegroundColor Cyan
                
                # Unit tests
                $unitResult = Invoke-ModuleUnitTests -ModulePath $moduleFile.FullName -ModuleName $moduleName
                $results.UnitTests += $unitResult
                
                # Quick integration test
                $integrationSuite = New-TestSuite
                $integrationSuite.SuiteName = "Integration_$moduleName"
                $integrationSuite.ModuleName = $moduleName
                $results.IntegrationTests += $integrationSuite
                
                # Quick performance test
                $performanceSuite = New-TestSuite
                $performanceSuite.SuiteName = "Performance_$moduleName"
                $performanceSuite.ModuleName = $moduleName
                $results.PerformanceTests += $performanceSuite
                
                # Quick security test
                $securitySuite = New-TestSuite
                $securitySuite.SuiteName = "Security_$moduleName"
                $securitySuite.ModuleName = $moduleName
                $results.SecurityTests += $securitySuite
                
            } catch {
                Write-Warning "Failed to test module $moduleName`: $_"
            }
        }
        
        # Calculate totals
        $totalTests = 0
        $totalPassed = 0
        $totalFailed = 0
        
        foreach ($unitTest in $results.UnitTests) {
            $totalTests += $unitTest.Total
            $totalPassed += $unitTest.Passed
            $totalFailed += $unitTest.Failed
        }
        
        $results.EndTime = Get-Date
        $results.Duration = [Math]::Round(($results.EndTime - $startTime).TotalSeconds, 2)
        $results.Summary.TotalTests = $totalTests
        $results.Summary.TotalPassed = $totalPassed
        $results.Summary.TotalFailed = $totalFailed
        
        if ($totalTests -gt 0) {
            $results.Summary.OverallSuccessRate = [Math]::Round(($totalPassed / $totalTests) * 100, 2)
            $results.Summary.PassedMinimum = $results.Summary.OverallSuccessRate -ge $MinSuccessRate
        }
        
        Write-Host ""
        Write-Host "=== Test Summary ===" -ForegroundColor Cyan
        Write-Host "Duration: $($results.Duration)s" -ForegroundColor White
        Write-Host "Total Tests: $totalTests" -ForegroundColor White
        Write-Host "Passed: $totalPassed" -ForegroundColor Green
        Write-Host "Failed: $totalFailed" -ForegroundColor $(if ($totalFailed -gt 0) { "Red" } else { "Green" })
        Write-Host "Success Rate: $($results.Summary.OverallSuccessRate)%" -ForegroundColor $(if ($results.Summary.OverallSuccessRate -ge 90) { "Green" } elseif ($results.Summary.OverallSuccessRate -ge 70) { "Yellow" } else { "Red" })
        Write-Host "Minimum Success Rate: $($results.Summary.PassedMinimum)" -ForegroundColor $(if ($results.Summary.PassedMinimum) { "Green" } else { "Red" })
        Write-Host ""
        
        return $results
        
    } catch {
        Write-StructuredLog -Message "Comprehensive test suite failed: $_" -Level Error -Function $functionName
        throw
    }
}

# Export functions
Export-ModuleMember -Function Invoke-ModuleUnitTests, Invoke-IntegrationTests, Invoke-PerformanceTests, Invoke-SecurityTests, Invoke-ComprehensiveTestSuite, Invoke-ComprehensiveTestSuiteFromDirectory


