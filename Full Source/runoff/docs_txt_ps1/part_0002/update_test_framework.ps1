
$path = "D:\lazy init ide\auto_generated_methods\RawrXD.TestFramework.psm1"
$content = Get-Content $path -Raw

# 1. Define the correct logic for Invoke-ComprehensiveTestSuite (Plural)
$newFunctionBody = @'
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
        Write-StructuredLog -Message "Comprehensive test suite failed: $_" -Level Error -Function $functionName
        throw
    }
}
'@

# 2. Find the FIRST Invoke-ComprehensiveTestSuite (approx lines 860-998)
# We look for the Pattern starting from the header I know exists (I just inserted the correct param block)
# But I need to replace the content AFTER the param block which is still the 'Old' logic.

# Actually, since I successfully updated the header/param block in the file already, 
# I can just search for the start of the function body and replace until the end of the function.

$startMarker = '$functionName = ''Invoke-ComprehensiveTestSuite''
    $startTime = Get-Date'

$endMarker = '} catch {
        Write-StructuredLog -Message "Comprehensive test suite failed: $_" -Level Error -Function $functionName
        throw
    }
}'

# We need to be careful not to replace the *second* function logic if it's similar.
# But I know the first function is what I want to target.

# Alternative: I replaced the header/param block. So now I have:
# function Invoke-ComprehensiveTestSuite { ... param([string[]]$ModulePaths ...) ...
# followed by the OLD body: $functionName = ... try { ... ModulePath = $ModulePath ... }

# I will use regex replacement in memory to fix the body.

# Regex to find the body of the first function.
# It starts after the param block.
# Param block ends with `)` followed by newline/whitespace.

$pattern = '(?ms)param\(\s+\[Parameter\(Mandatory=\$true\)\]\s+\[string\[\]\]\$ModulePaths.*?\)\s+(\$functionName = ''Invoke-ComprehensiveTestSuite''.*?return \$results)'

# Wait, `ModulePaths` is what I put in the header.
# I want to replace the capture group (the body) with the NEW logic body.

# Construct the NEW body only (without the header params)
$newBody = @'
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
'@

# Regex replacement
# The content is in $content.
# I want to find the parameter block I inserted (ModulePaths), and then replace the block that follows (which is still the old logic)
# The old logic ends with "return $results" and newlines, then "} catch".

# Let's target the exact string I see in the file for the OLD body start.
$oldBodyStart = '$functionName = ''Invoke-ComprehensiveTestSuite''
    $startTime = Get-Date
    
    try {
        Write-StructuredLog -Message "Starting comprehensive test suite" -Level Info -Function $functionName -Data @{
            ModulePath = $ModulePath'

# If I can find this, I am good.
# But simply replacing valid substrings is safer.
# I know the param block ends with:
#         [Parameter(Mandatory=$false)]
#         [string]$OutputPath = $null
#     )

# I will replace everything from AFTER that param block close, to the start of the `catch` block.

$split = $content -split '\s+\[Parameter\(Mandatory=\$false\)\]\s+\[string\]\$OutputPath = \$null\s+\)'
# This splits the file. Index 0 is before, Index 1 is after (which contains the body + the second function).

if ($split.Count -ge 2) {
    # Index 1 starts with the body of the first function.
    # It ends with `} catch {`.
    
    $bodyAndRest = $split[1]
    
    # Check if we split at the first function or the second function.
    # Since I updated the first function to match the singular/plural params? No, I updated the first function to match Plural logic.
    # So `string[]$ModulePaths` is in the first function.
    # But `string$ModulePath` (Singular) is in the SECOND function.
    
    # Wait, the split pattern relies on the `OutputPath` which is common to both.
    # But the Param block I updated HAS `ModulePaths` (Plural) before `OutputPath`.
    # The second function has `ModulePath` (Singular).
    
    # So I should split on the unique param:
    # `[string[]]$ModulePaths`
    
    # Let's restart the approach.
    # Pattern:
    # 1. Find `[string[]]$ModulePaths` ... `)`
    # 2. Capture everything until `} catch {`
    # 3. Replace captured part with new body.
    
    $pattern = '(?s)(\[string\[\]\]\$ModulePaths.*?\)[\r\n\s]+)(.*?)([\r\n\s]+\} catch \{)'
    
    if ($content -match $pattern) {
        $header = $matches[1]
        $oldBody = $matches[2]
        $footer = $matches[3]
        
        $newContent = $content.Replace($oldBody, $newBody)
        Set-Content -Path $path -Value $newContent
        Write-Host "Successfully replaced function body."
    } else {
        Write-Host "Could not match pattern."
        # Debug:
        $content | Select-String "string\[\]"
    }
} else {
    Write-Host "Could not split file."
}
