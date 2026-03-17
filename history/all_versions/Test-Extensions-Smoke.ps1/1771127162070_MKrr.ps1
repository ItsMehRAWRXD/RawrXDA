<#
.SYNOPSIS
    Comprehensive smoke test for Amazon Q and GitHub Copilot extensions in RawrXD IDE
.DESCRIPTION
    Tests both GUI and CLI modes with full feature verification
#>

param(
    [string]$Mode = "both",  # "gui", "cli", or "both"
    [switch]$Verbose
)

$ErrorActionPreference = "Continue"
$TestResults = @()

function Write-TestHeader {
    param([string]$Message)
    Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║ $($Message.PadRight(61)) ║" -ForegroundColor Cyan
    Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
}

function Write-TestResult {
    param(
        [string]$Test,
        [string]$Result,
        [string]$Details = ""
    )
    
    $color = if ($Result -eq "PASS") { "Green" } elseif ($Result -eq "FAIL") { "Red" } else { "Yellow" }
    Write-Host "  [$Result]" -ForegroundColor $color -NoNewline
    Write-Host " $Test"
    if ($Details) {
        Write-Host "        → $Details" -ForegroundColor Gray
    }
    
    $script:TestResults += [PSCustomObject]@{
        Test = $Test
        Result = $Result
        Details = $Details
        Timestamp = Get-Date
    }
}

function Test-VSIXFile {
    param([string]$Path, [string]$Name)
    
    Write-Host "`n  Testing $Name VSIX file..." -ForegroundColor Yellow
    
    if (!(Test-Path $Path)) {
        Write-TestResult "VSIX File Exists: $Name" "FAIL" "File not found: $Path"
        return $false
    }
    
    Write-TestResult "VSIX File Exists: $Name" "PASS" "Found at: $Path"
    
    # Test file integrity (basic check)
    try {
        $fileInfo = Get-Item $Path
        if ($fileInfo.Length -lt 1KB) {
            Write-TestResult "VSIX File Integrity: $Name" "FAIL" "File too small ($($fileInfo.Length) bytes)"
            return $false
        }
        Write-TestResult "VSIX File Integrity: $Name" "PASS" "Size: $([math]::Round($fileInfo.Length/1MB, 2)) MB"
    } catch {
        Write-TestResult "VSIX File Integrity: $Name" "FAIL" $_.Exception.Message
        return $false
    }
    
    # Test ZIP structure (VSIX is a ZIP)
    try {
        Add-Type -Assembly System.IO.Compression.FileSystem
        $zip = [System.IO.Compression.ZipFile]::OpenRead($Path)
        $entries = $zip.Entries.Count
        $zip.Dispose()
        
        Write-TestResult "VSIX Structure: $Name" "PASS" "$entries entries found"
    } catch {
        Write-TestResult "VSIX Structure: $Name" "FAIL" $_.Exception.Message
        return $false
    }
    
    return $true
}

function Test-PluginDirectory {
    param([string]$ExtensionName)
    
    $pluginDir = "d:\rawrxd\plugins\$ExtensionName"
    
    if (Test-Path $pluginDir) {
        $files = Get-ChildItem $pluginDir -Recurse | Measure-Object
        Write-TestResult "Plugin Directory: $ExtensionName" "PASS" "$($files.Count) files extracted"
        
        # Check for key files
        $packageJson = Join-Path $pluginDir "package.json"
        $extensionPackageJson = Join-Path $pluginDir "extension\package.json"
        
        if ((Test-Path $packageJson) -or (Test-Path $extensionPackageJson)) {
            Write-TestResult "Package Manifest: $ExtensionName" "PASS" "package.json found"
        } else {
            Write-TestResult "Package Manifest: $ExtensionName" "WARN" "package.json not found"
        }
        
        return $true
    } else {
        Write-TestResult "Plugin Directory: $ExtensionName" "FAIL" "Directory not found: $pluginDir"
        return $false
    }
}

function Test-CLIExtensionLoad {
    param([string]$ExtensionName, [string]$VSIXPath)
    
    Write-Host "`n  Testing CLI Extension Load: $ExtensionName..." -ForegroundColor Yellow
    
    # Create test script
    $testScript = @"
!plugin load $VSIXPath
!plugin list
!plugin enable $ExtensionName
/exit
"@
    
    $testFile = "d:\rawrxd\test_cli_$ExtensionName.txt"
    $testScript | Out-File -FilePath $testFile -Encoding UTF8
    
    try {
        $process = Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
            -ArgumentList "--cli", "--batch", $testFile `
            -PassThru `
            -NoNewWindow `
            -RedirectStandardOutput "d:\rawrxd\test_cli_${ExtensionName}_out.txt" `
            -RedirectStandardError "d:\rawrxd\test_cli_${ExtensionName}_err.txt" `
            -Wait `
            -TimeoutSec 30
        
        Start-Sleep -Seconds 2
        
        # Check output
        if (Test-Path "d:\rawrxd\test_cli_${ExtensionName}_out.txt") {
            $output = Get-Content "d:\rawrxd\test_cli_${ExtensionName}_out.txt" -Raw
            
            if ($output -match "Plugin loaded|Extension loaded|successfully loaded") {
                Write-TestResult "CLI Load: $ExtensionName" "PASS" "Extension loaded successfully"
                return $true
            } elseif ($output -match "already loaded|already installed") {
                Write-TestResult "CLI Load: $ExtensionName" "PASS" "Extension already loaded"
                return $true
            } else {
                Write-TestResult "CLI Load: $ExtensionName" "FAIL" "No success message in output"
                if ($Verbose) {
                    Write-Host "Output:" -ForegroundColor Gray
                    Write-Host $output -ForegroundColor DarkGray
                }
                return $false
            }
        } else {
            Write-TestResult "CLI Load: $ExtensionName" "FAIL" "No output file generated"
            return $false
        }
    } catch {
        Write-TestResult "CLI Load: $ExtensionName" "FAIL" $_.Exception.Message
        return $false
    } finally {
        Remove-Item $testFile -ErrorAction SilentlyContinue
    }
}

function Test-GUIExtensionLoad {
    param([string]$ExtensionName, [string]$VSIXPath)
    
    Write-Host "`n  Testing GUI Extension Load: $ExtensionName..." -ForegroundColor Yellow
    
    try {
        # Start GUI IDE
        $process = Start-Process -FilePath "d:\rawrxd\RawrXD_IDE.exe" `
            -ArgumentList "--load-extension", $VSIXPath `
            -PassThru `
            -NoNewWindow
        
        Start-Sleep -Seconds 5
        
        # Check if process is running
        $runningProcess = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
        
        if ($runningProcess) {
            Write-TestResult "GUI Launch: $ExtensionName" "PASS" "IDE started (PID: $($process.Id))"
            
            # Give it time to load
            Start-Sleep -Seconds 3
            
            # Check log files for extension loading
            $logPath = "d:\rawrxd\rawrxd_ide.log"
            if (Test-Path $logPath) {
                $logContent = Get-Content $logPath -Tail 100 -Raw
                
                if ($logContent -match "$ExtensionName|$($ExtensionName -replace '-','')") {
                    Write-TestResult "GUI Extension Detection: $ExtensionName" "PASS" "Extension mentioned in logs"
                } else {
                    Write-TestResult "GUI Extension Detection: $ExtensionName" "WARN" "Extension not found in logs"
                }
            }
            
            # Gracefully close
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            Start-Sleep -Seconds 2
            
            return $true
        } else {
            Write-TestResult "GUI Launch: $ExtensionName" "FAIL" "IDE process not running"
            return $false
        }
    } catch {
        Write-TestResult "GUI Launch: $ExtensionName" "FAIL" $_.Exception.Message
        return $false
    }
}

function Test-ExtensionCommands {
    param([string]$ExtensionName)
    
    Write-Host "`n  Testing Extension Commands: $ExtensionName..." -ForegroundColor Yellow
    
    $commands = @(
        "!plugin list",
        "!plugin config $ExtensionName",
        "!plugin help $ExtensionName"
    )
    
    foreach ($cmd in $commands) {
        $testScript = @"
$cmd
/exit
"@
        
        $testFile = "d:\rawrxd\test_cmd_${ExtensionName}_$([guid]::NewGuid().ToString('N').Substring(0,8)).txt"
        $testScript | Out-File -FilePath $testFile -Encoding UTF8
        
        try {
            $process = Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
                -ArgumentList "--cli", "--batch", $testFile `
                -PassThru `
                -NoNewWindow `
                -RedirectStandardOutput "${testFile}.out" `
                -RedirectStandardError "${testFile}.err" `
                -Wait `
                -TimeoutSec 15
            
            Start-Sleep -Milliseconds 500
            
            if (Test-Path "${testFile}.out") {
                $output = Get-Content "${testFile}.out" -Raw
                if ($output -match "$ExtensionName|success|available") {
                    Write-TestResult "Command: $cmd" "PASS" "Response received"
                } else {
                    Write-TestResult "Command: $cmd" "WARN" "Unexpected response"
                }
            }
        } catch {
            Write-TestResult "Command: $cmd" "FAIL" $_.Exception.Message
        } finally {
            Remove-Item $testFile -ErrorAction SilentlyContinue
            Remove-Item "${testFile}.out" -ErrorAction SilentlyContinue
            Remove-Item "${testFile}.err" -ErrorAction SilentlyContinue
        }
    }
}

function Test-CopilotFeatures {
    Write-Host "`n  Testing Copilot-Specific Features..." -ForegroundColor Yellow
    
    # Test Copilot chat
    $chatTest = @"
/chat What is a variable in programming?
/exit
"@
    
    $testFile = "d:\rawrxd\test_copilot_chat.txt"
    $chatTest | Out-File -FilePath $testFile -Encoding UTF8
    
    try {
        Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
            -ArgumentList "--cli", "--batch", $testFile `
            -Wait `
            -NoNewWindow `
            -RedirectStandardOutput "d:\rawrxd\test_copilot_chat_out.txt" `
            -RedirectStandardError "d:\rawrxd\test_copilot_chat_err.txt" `
            -TimeoutSec 30
        
        Start-Sleep -Seconds 1
        
        if (Test-Path "d:\rawrxd\test_copilot_chat_out.txt") {
            $output = Get-Content "d:\rawrxd\test_copilot_chat_out.txt" -Raw
            if ($output.Length -gt 100) {
                Write-TestResult "Copilot Chat Response" "PASS" "Received response ($($output.Length) chars)"
            } else {
                Write-TestResult "Copilot Chat Response" "WARN" "Short response"
            }
        }
    } catch {
        Write-TestResult "Copilot Chat Response" "FAIL" $_.Exception.Message
    } finally {
        Remove-Item $testFile -ErrorAction SilentlyContinue
    }
}

function Test-AmazonQFeatures {
    Write-Host "`n  Testing Amazon Q-Specific Features..." -ForegroundColor Yellow
    
    # Test Q chat
    $chatTest = @"
/chat Explain AWS Lambda
/exit
"@
    
    $testFile = "d:\rawrxd\test_amazonq_chat.txt"
    $chatTest | Out-File -FilePath $testFile -Encoding UTF8
    
    try {
        Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
            -ArgumentList "--cli", "--batch", $testFile `
            -Wait `
            -NoNewWindow `
            -RedirectStandardOutput "d:\rawrxd\test_amazonq_chat_out.txt" `
            -RedirectStandardError "d:\rawrxd\test_amazonq_chat_err.txt" `
            -TimeoutSec 30
        
        Start-Sleep -Seconds 1
        
        if (Test-Path "d:\rawrxd\test_amazonq_chat_out.txt") {
            $output = Get-Content "d:\rawrxd\test_amazonq_chat_out.txt" -Raw
            if ($output.Length -gt 100) {
                Write-TestResult "Amazon Q Chat Response" "PASS" "Received response ($($output.Length) chars)"
            } else {
                Write-TestResult "Amazon Q Chat Response" "WARN" "Short response"
            }
        }
    } catch {
        Write-TestResult "Amazon Q Chat Response" "FAIL" $_.Exception.Message
    } finally {
        Remove-Item $testFile -ErrorAction SilentlyContinue
    }
}

# ============================================================================
# MAIN TEST EXECUTION
# ============================================================================

Clear-Host
Write-TestHeader "RawrXD IDE - Extension Smoke Test Suite"
Write-Host "Testing Mode: $($Mode.ToUpper())" -ForegroundColor Cyan
Write-Host "Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray
Write-Host ""

# Define extensions
$extensions = @(
    @{
        Name = "Amazon Q"
        ID = "amazonwebservices.amazon-q-vscode"
        VSIX = "d:\rawrxd\amazon-q-vscode-latest.vsix"
        PluginDir = "amazon-q-vscode"
    },
    @{
        Name = "GitHub Copilot"
        ID = "github.copilot"
        VSIX = "d:\rawrxd\copilot-latest.vsix"
        PluginDir = "copilot"
    }
)

# Phase 1: VSIX File Validation
Write-TestHeader "Phase 1: VSIX File Validation"
foreach ($ext in $extensions) {
    Test-VSIXFile -Path $ext.VSIX -Name $ext.Name
}

# Phase 2: CLI Mode Tests
if ($Mode -eq "cli" -or $Mode -eq "both") {
    Write-TestHeader "Phase 2: CLI Mode Extension Loading"
    
    foreach ($ext in $extensions) {
        if (Test-VSIXFile -Path $ext.VSIX -Name $ext.Name) {
            Test-CLIExtensionLoad -ExtensionName $ext.ID -VSIXPath $ext.VSIX
            Test-PluginDirectory -ExtensionName $ext.PluginDir
            Test-ExtensionCommands -ExtensionName $ext.ID
        }
    }
    
    # CLI-specific feature tests
    Write-TestHeader "Phase 2.1: CLI Feature Tests"
    Test-CopilotFeatures
    Test-AmazonQFeatures
}

# Phase 3: GUI Mode Tests  
if ($Mode -eq "gui" -or $Mode -eq "both") {
    Write-TestHeader "Phase 3: GUI Mode Extension Loading"
    
    foreach ($ext in $extensions) {
        if (Test-VSIXFile -Path $ext.VSIX -Name $ext.Name) {
            Test-GUIExtensionLoad -ExtensionName $ext.ID -VSIXPath $ext.VSIX
            Test-PluginDirectory -ExtensionName $ext.PluginDir
        }
    }
}

# Phase 4: Integration Tests
Write-TestHeader "Phase 4: Integration Tests"

# Test extension registry
$registryPath = "d:\rawrxd\plugins\registry.json"
if (Test-Path $registryPath) {
    try {
        $registry = Get-Content $registryPath -Raw | ConvertFrom-Json
        $loadedExtensions = $registry.PSObject.Properties.Count
        Write-TestResult "Extension Registry" "PASS" "$loadedExtensions extensions registered"
    } catch {
        Write-TestResult "Extension Registry" "WARN" "Could not parse registry"
    }
} else {
    Write-TestResult "Extension Registry" "INFO" "Registry not yet created"
}

# Test simultaneous loading
Write-Host "`n  Testing simultaneous extension loading..." -ForegroundColor Yellow
$bothTest = @"
!plugin load d:\rawrxd\amazon-q-vscode-latest.vsix
!plugin load d:\rawrxd\copilot-latest.vsix
!plugin list
/exit
"@

$testFile = "d:\rawrxd\test_both_extensions.txt"
$bothTest | Out-File -FilePath $testFile -Encoding UTF8

try {
    Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
        -ArgumentList "--cli", "--batch", $testFile `
        -Wait `
        -NoNewWindow `
        -RedirectStandardOutput "d:\rawrxd\test_both_out.txt" `
        -RedirectStandardError "d:\rawrxd\test_both_err.txt" `
        -TimeoutSec 45
    
    if (Test-Path "d:\rawrxd\test_both_out.txt") {
        $output = Get-Content "d:\rawrxd\test_both_out.txt" -Raw
        $amazonQLoaded = $output -match "amazon-q|Amazon Q"
        $copilotLoaded = $output -match "copilot|Copilot"
        
        if ($amazonQLoaded -and $copilotLoaded) {
            Write-TestResult "Simultaneous Loading" "PASS" "Both extensions loaded"
        } elseif ($amazonQLoaded -or $copilotLoaded) {
            Write-TestResult "Simultaneous Loading" "WARN" "Only one extension loaded"
        } else {
            Write-TestResult "Simultaneous Loading" "FAIL" "Neither extension loaded"
        }
    }
} catch {
    Write-TestResult "Simultaneous Loading" "FAIL" $_.Exception.Message
} finally {
    Remove-Item $testFile -ErrorAction SilentlyContinue
}

# ============================================================================
# RESULTS SUMMARY
# ============================================================================

Write-TestHeader "Test Results Summary"

$passed = ($TestResults | Where-Object { $_.Result -eq "PASS" }).Count
$failed = ($TestResults | Where-Object { $_.Result -eq "FAIL" }).Count
$warned = ($TestResults | Where-Object { $_.Result -eq "WARN" }).Count
$info = ($TestResults | Where-Object { $_.Result -eq "INFO" }).Count
$total = $TestResults.Count

Write-Host "  Total Tests: $total" -ForegroundColor White
Write-Host "  Passed:      $passed" -ForegroundColor Green
Write-Host "  Failed:      $failed" -ForegroundColor Red
Write-Host "  Warnings:    $warned" -ForegroundColor Yellow
Write-Host "  Info:        $info" -ForegroundColor Cyan

if ($failed -eq 0) {
    Write-Host "`n✓ All critical tests passed!" -ForegroundColor Green
} elseif ($failed -lt 3) {
    Write-Host "`n⚠ Some tests failed, but extensions may still work" -ForegroundColor Yellow
} else {
    Write-Host "`n✗ Multiple test failures detected" -ForegroundColor Red
}

# Export detailed results
$reportPath = "d:\rawrxd\extension_smoketest_report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
$TestResults | ConvertTo-Json -Depth 10 | Out-File $reportPath
Write-Host "`nDetailed report saved to: $reportPath" -ForegroundColor Gray

# Cleanup test files
Write-Host "`nCleaning up test artifacts..." -ForegroundColor Gray
Get-ChildItem "d:\rawrxd\test_*.txt" -ErrorAction SilentlyContinue | Remove-Item -Force
Write-Host "Cleanup complete.`n" -ForegroundColor Gray
