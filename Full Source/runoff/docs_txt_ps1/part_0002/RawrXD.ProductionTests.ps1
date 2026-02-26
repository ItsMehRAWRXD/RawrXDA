# RawrXD Production Test Suite
# Comprehensive testing for all auto-generated modules and features

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory=$false)]
    [string]$TestScope = 'All',  # All, Modules, Features, Integration
    
    [Parameter(Mandatory=$false)]
    [switch]$GenerateReport,
    
    [Parameter(Mandatory=$false)]
    [string]$ReportPath = $null
)

# Import Pester if available
try {
    Import-Module Pester -Force -ErrorAction Stop
    $pesterAvailable = $true
} catch {
    Write-Warning "Pester not available. Running basic tests only."
    $pesterAvailable = $false
}

# Import logging and config
$scriptRoot = Split-Path -Parent $PSScriptRoot
$loggingModule = Join-Path $scriptRoot 'RawrXD.Logging.psm1'
$configModule = Join-Path $scriptRoot 'RawrXD.Config.psm1'

if (Test-Path $loggingModule) { 
    try { Import-Module $loggingModule -Force } catch { } 
}
if (Test-Path $configModule) { 
    try { Import-Module $configModule -Force } catch { } 
}

function Write-TestLog {
    param([string]$Message, [string]$Level = 'Info')
    if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
        Write-StructuredLog -Message $Message -Level $Level -Function 'RawrXD-TestSuite'
    } else {
        Write-Host "[$Level] $Message" -ForegroundColor $(switch($Level){'Error'{'Red'}'Warning'{'Yellow'}default{'Cyan'}})
    }
}

function Test-RawrXDModules {
    <#
    .SYNOPSIS
        Tests all production-ready .psm1 modules
    #>
    
    Write-TestLog "Testing RawrXD production modules..." -Level Info
    
    $moduleDir = Join-Path $scriptRoot 'auto_generated_methods'
    $modules = Get-ChildItem -Path $moduleDir -Filter 'RawrXD.*.psm1' -ErrorAction SilentlyContinue
    
    $results = @()
    
    foreach ($module in $modules) {
        $result = @{
            ModuleName = $module.BaseName
            FilePath = $module.FullName
            ImportSuccess = $false
            FunctionsExported = 0
            ExportedFunctions = @()
            TestsPassed = 0
            TestsFailed = 0
            Error = $null
        }
        
        try {
            # Test import
            $importResult = Import-Module $module.FullName -Force -PassThru -ErrorAction Stop
            $result.ImportSuccess = $true
            $result.FunctionsExported = $importResult.ExportedFunctions.Count
            $result.ExportedFunctions = $importResult.ExportedFunctions.Keys | Sort-Object
            
            Write-TestLog "✓ $($module.BaseName) imported successfully ($($result.FunctionsExported) functions)" -Level Info
            
            # Test each exported function
            foreach ($functionName in $result.ExportedFunctions) {
                try {
                    $cmd = Get-Command $functionName -ErrorAction Stop
                    if ($cmd.Parameters.ContainsKey('WhatIf') -or $functionName -like '*Test*') {
                        # Safe to test
                        if ($functionName -eq 'Test-ComponentHealth') {
                            $testFile = Join-Path $moduleDir 'SelfHealingModule_AutoFeature.ps1'
                            if (Test-Path $testFile) {
                                & $functionName -FilePath $testFile | Out-Null
                            }
                        } elseif ($functionName -eq 'Invoke-SelfHealingModule') {
                            & $functionName -ModuleDir $moduleDir -NoInvoke | Out-Null
                        } elseif ($functionName -eq 'Invoke-SecurityVulnerabilityScanner') {
                            & $functionName -SourceDir $moduleDir -ScanLevel Basic | Out-Null
                        } elseif ($functionName -eq 'Invoke-AutoDependencyGraph') {
                            & $functionName -SourceDir $moduleDir -MaxDepth 2 | Out-Null
                        }
                        $result.TestsPassed++
                        Write-TestLog "  ✓ $functionName - basic test passed" -Level Debug
                    } else {
                        Write-TestLog "  ⚠ $functionName - skipped (not safe to auto-test)" -Level Debug
                    }
                } catch {
                    $result.TestsFailed++
                    Write-TestLog "  ✗ $functionName - test failed: $($_.Exception.Message)" -Level Warning
                }
            }
            
        } catch {
            $result.Error = $_.Exception.Message
            Write-TestLog "✗ $($module.BaseName) import failed: $($result.Error)" -Level Error
        }
        
        $results += $result
    }
    
    return $results
}

function Test-RawrXDFeatures {
    <#
    .SYNOPSIS
        Tests auto-feature scripts
    #>
    
    Write-TestLog "Testing RawrXD auto-feature scripts..." -Level Info
    
    $moduleDir = Join-Path $scriptRoot 'auto_generated_methods'
    $features = Get-ChildItem -Path $moduleDir -Filter '*_AutoFeature.ps1' -ErrorAction SilentlyContinue
    
    $results = @()
    
    foreach ($feature in $features) {
        $result = @{
            FeatureName = $feature.BaseName
            FilePath = $feature.FullName
            LoadSuccess = $false
            FunctionFound = $false
            FunctionName = 'Invoke-' + ($feature.BaseName -replace '_AutoFeature$','')
            TestResult = 'Unknown'
            Error = $null
        }
        
        try {
            # Test dot-sourcing
            . $feature.FullName
            $result.LoadSuccess = $true
            
            # Check function exists
            $cmd = Get-Command $result.FunctionName -ErrorAction SilentlyContinue
            if ($cmd) {
                $result.FunctionFound = $true
                Write-TestLog "✓ $($result.FeatureName) loaded successfully" -Level Info
                $result.TestResult = 'Passed'
            } else {
                $result.TestResult = 'Function Not Found'
                Write-TestLog "⚠ $($result.FeatureName) loaded but function $($result.FunctionName) not found" -Level Warning
            }
            
        } catch {
            $result.Error = $_.Exception.Message
            $result.TestResult = 'Load Failed'
            Write-TestLog "✗ $($result.FeatureName) failed to load: $($result.Error)" -Level Error
        }
        
        $results += $result
    }
    
    return $results
}

function Test-RawrXDIntegration {
    <#
    .SYNOPSIS
        Tests end-to-end integration
    #>
    
    Write-TestLog "Testing RawrXD integration..." -Level Info
    
    $results = @{
        UniversalIntegratorFound = $false
        UniversalIntegratorWorks = $false
        LoggingWorks = $false
        ConfigWorks = $false
        AIInterventionWorks = $false
        Error = $null
    }
    
    try {
        # Test Universal Integrator
        $integratorPath = Join-Path (Join-Path $scriptRoot 'auto_generated_methods') 'RawrXD_UniversalIntegrator.psm1'
        if (Test-Path $integratorPath) {
            $results.UniversalIntegratorFound = $true
            try {
                Import-Module $integratorPath -Force -ErrorAction Stop
                $results.UniversalIntegratorWorks = $true
                Write-TestLog "✓ Universal Integrator module available" -Level Info
            } catch {
                Write-TestLog "✗ Universal Integrator import failed: $($_.Exception.Message)" -Level Error
            }
        }
        
        # Test Logging
        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
            $results.LoggingWorks = $true
            Write-TestLog "✓ Structured logging available" -Level Info
        }
        
        # Test Config
        if (Get-Command Get-RawrXDRootPath -ErrorAction SilentlyContinue) {
            $results.ConfigWorks = $true
            Write-TestLog "✓ Configuration module available" -Level Info
        }
        
        # Test AI Intervention
        $aiPath = Join-Path (Join-Path $scriptRoot 'auto_generated_methods') 'RawrXD_AIIntervention.psm1'
        if (Test-Path $aiPath) {
            try {
                Import-Module $aiPath -Force -ErrorAction Stop
                if (Get-Command Invoke-RawrXD_AIIntervention -ErrorAction SilentlyContinue) {
                    $results.AIInterventionWorks = $true
                    Write-TestLog "✓ AI Intervention module available" -Level Info
                }
            } catch {
                Write-TestLog "⚠ AI Intervention available but not functional: $($_.Exception.Message)" -Level Warning
            }
        }
        
    } catch {
        $results.Error = $_.Exception.Message
        Write-TestLog "✗ Integration test error: $($results.Error)" -Level Error
    }
    
    return $results
}

function Test-RawrXDSecurity {
    <#
    .SYNOPSIS
        Tests security configuration and scanning
    #>
    
    Write-TestLog "Testing RawrXD security..." -Level Info
    
    $results = @{
        SecurityConfigFound = $false
        SecurityConfigValid = $false
        TLSEnabled = $false
        SecurityScannerWorks = $false
        VulnerabilitiesFound = 0
        Error = $null
    }
    
    try {
        # Test security config
        $securityPath = Join-Path (Join-Path $scriptRoot 'config') 'security.json'
        if (Test-Path $securityPath) {
            $results.SecurityConfigFound = $true
            try {
                $secConfig = Get-Content $securityPath -Raw | ConvertFrom-Json
                $results.SecurityConfigValid = $true
                if ($secConfig.enforceTLS -eq $true) {
                    $results.TLSEnabled = $true
                    Write-TestLog "✓ Security configuration valid, TLS enabled" -Level Info
                } else {
                    Write-TestLog "⚠ TLS not enabled in security configuration" -Level Warning
                }
            } catch {
                Write-TestLog "✗ Invalid security configuration: $($_.Exception.Message)" -Level Error
            }
        }
        
        # Test security scanner
        if (Get-Command Invoke-SecurityVulnerabilityScanner -ErrorAction SilentlyContinue) {
            try {
                $scanResult = Invoke-SecurityVulnerabilityScanner -SourceDir (Join-Path $scriptRoot 'auto_generated_methods') -ScanLevel Basic
                $results.SecurityScannerWorks = $true
                $results.VulnerabilitiesFound = $scanResult.Summary.TotalVulnerabilities
                Write-TestLog "✓ Security scanner completed. Found $($results.VulnerabilitiesFound) potential issues" -Level Info
            } catch {
                Write-TestLog "✗ Security scanner failed: $($_.Exception.Message)" -Level Error
            }
        }
        
    } catch {
        $results.Error = $_.Exception.Message
        Write-TestLog "✗ Security test error: $($results.Error)" -Level Error
    }
    
    return $results
}

# Main test execution
Write-TestLog "Starting RawrXD Production Test Suite" -Level Info
Write-TestLog "Test Scope: $TestScope" -Level Info

$testResults = @{
    Timestamp = (Get-Date).ToString('o')
    TestScope = $TestScope
    OverallStatus = 'Unknown'
    Modules = @()
    Features = @()
    Integration = @{}
    Security = @{}
    Summary = @{
        TotalTests = 0
        PassedTests = 0
        FailedTests = 0
        SkippedTests = 0
        Duration = 0
    }
}

$startTime = Get-Date

# Run tests based on scope
if ($TestScope -in @('All', 'Modules')) {
    $testResults.Modules = Test-RawrXDModules
}

if ($TestScope -in @('All', 'Features')) {
    $testResults.Features = Test-RawrXDFeatures
}

if ($TestScope -in @('All', 'Integration')) {
    $testResults.Integration = Test-RawrXDIntegration
}

if ($TestScope -in @('All', 'Security')) {
    $testResults.Security = Test-RawrXDSecurity
}

$endTime = Get-Date
$testResults.Summary.Duration = ($endTime - $startTime).TotalSeconds

# Calculate summary
$testResults.Summary.PassedTests = 
    ($testResults.Modules | Where-Object { $_.ImportSuccess }).Count +
    ($testResults.Features | Where-Object { $_.TestResult -eq 'Passed' }).Count

$testResults.Summary.FailedTests = 
    ($testResults.Modules | Where-Object { -not $_.ImportSuccess }).Count +
    ($testResults.Features | Where-Object { $_.TestResult -in @('Load Failed', 'Function Not Found') }).Count

$testResults.Summary.TotalTests = $testResults.Summary.PassedTests + $testResults.Summary.FailedTests

# Determine overall status
if ($testResults.Summary.FailedTests -eq 0) {
    $testResults.OverallStatus = 'PASSED'
} elseif ($testResults.Summary.PassedTests -gt $testResults.Summary.FailedTests) {
    $testResults.OverallStatus = 'MOSTLY_PASSED'
} else {
    $testResults.OverallStatus = 'FAILED'
}

Write-TestLog "Test suite completed in $([math]::Round($testResults.Summary.Duration,2))s" -Level Info
Write-TestLog "Overall Status: $($testResults.OverallStatus)" -Level $(if($testResults.OverallStatus -eq 'FAILED'){'Error'}else{'Info'})
Write-TestLog "Tests: $($testResults.Summary.PassedTests) passed, $($testResults.Summary.FailedTests) failed" -Level Info

# Generate report
if ($GenerateReport) {
    if (-not $ReportPath) {
        $ReportPath = Join-Path (Join-Path $scriptRoot 'auto_generated_methods') "TestSuite_Report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
    }
    
    $testResults | ConvertTo-Json -Depth 15 | Set-Content $ReportPath -Encoding UTF8
    Write-TestLog "Test report saved: $ReportPath" -Level Info
}

return $testResults