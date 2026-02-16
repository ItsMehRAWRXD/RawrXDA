<#
.SYNOPSIS
    Deep feature validation for Amazon Q and GitHub Copilot in RawrXD IDE
.DESCRIPTION
    Tests specific extension features, APIs, and integration points
#>

param(
    [string]$Extension = "both",  # "amazonq", "copilot", or "both"
    [switch]$Interactive
)

$ErrorActionPreference = "Continue"

function Write-Feature {
    param([string]$Name, [string]$Status, [string]$Details = "")
    $color = switch ($Status) {
        "✓" { "Green" }
        "✗" { "Red" }
        "⚠" { "Yellow" }
        "→" { "Cyan" }
        default { "White" }
    }
    Write-Host "  $Status " -ForegroundColor $color -NoNewline
    Write-Host "$Name" -NoNewline
    if ($Details) {
        Write-Host " — $Details" -ForegroundColor Gray
    } else {
        Write-Host ""
    }
}

function Test-CopilotAutocomplete {
    Write-Host "`n╔═══ GitHub Copilot: Code Completion Test ═══╗" -ForegroundColor Magenta
    
    # Create test file
    $testCode = @"
def calculate_fibonacci(n):
    # This function should calculate fibonacci
"@
    
    $testFile = "d:\rawrxd\test_code_completion.py"
    $testCode | Out-File -FilePath $testFile -Encoding UTF8
    
    # Test completion request
    $completionTest = @"
!plugin enable github.copilot
/suggest Calculate the nth Fibonacci number
/exit
"@
    
    $batchFile = "d:\rawrxd\test_copilot_suggest.txt"
    $completionTest | Out-File -FilePath $batchFile -Encoding UTF8
    
    try {
        Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
            -ArgumentList "--cli", "--batch", $batchFile `
            -Wait -NoNewWindow `
            -RedirectStandardOutput "d:\rawrxd\copilot_suggest_out.txt" `
            -RedirectStandardError "d:\rawrxd\copilot_suggest_err.txt" `
            -TimeoutSec 30
        
        if (Test-Path "d:\rawrxd\copilot_suggest_out.txt") {
            $output = Get-Content "d:\rawrxd\copilot_suggest_out.txt" -Raw
            
            if ($output -match "def|return|fibonacci") {
                Write-Feature "Code Completion API" "✓" "Copilot generated code suggestions"
                return $true
            } elseif ($output -match "not available|not installed|error") {
                Write-Feature "Code Completion API" "✗" "Copilot not responding"
                return $false
            } else {
                Write-Feature "Code Completion API" "⚠" "Unexpected response format"
                return $false
            }
        }
    } catch {
        Write-Feature "Code Completion API" "✗" $_.Exception.Message
        return $false
    } finally {
        Remove-Item $testFile -ErrorAction SilentlyContinue
        Remove-Item $batchFile -ErrorAction SilentlyContinue
    }
}

function Test-CopilotChat {
    Write-Host "`n╔═══ GitHub Copilot: Chat Interface Test ═══╗" -ForegroundColor Magenta
    
    $queries = @(
        @{ Query = "How do I reverse a string in Python?"; Expected = "python|reverse|string|\[::-1\]" },
        @{ Query = "Explain async/await in JavaScript"; Expected = "async|await|promise|javascript" },
        @{ Query = "What is a REST API?"; Expected = "REST|API|HTTP|endpoint" }
    )
    
    foreach ($q in $queries) {
        $chatTest = @"
!plugin enable github.copilot
/chat $($q.Query)
/exit
"@
        
        $batchFile = "d:\rawrxd\test_copilot_chat_$(Get-Random).txt"
        $chatTest | Out-File -FilePath $batchFile -Encoding UTF8
        
        try {
            Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
                -ArgumentList "--cli", "--batch", $batchFile `
                -Wait -NoNewWindow `
                -RedirectStandardOutput "${batchFile}.out" `
                -RedirectStandardError "${batchFile}.err" `
                -TimeoutSec 30
            
            if (Test-Path "${batchFile}.out") {
                $output = Get-Content "${batchFile}.out" -Raw
                
                if ($output -match $q.Expected) {
                    Write-Feature "Chat: $($q.Query)" "✓" "Relevant response received"
                } elseif ($output.Length -gt 50) {
                    Write-Feature "Chat: $($q.Query)" "⚠" "Response received but content unclear"
                } else {
                    Write-Feature "Chat: $($q.Query)" "✗" "No meaningful response"
                }
            }
        } catch {
            Write-Feature "Chat: $($q.Query)" "✗" $_.Exception.Message
        } finally {
            Remove-Item $batchFile -ErrorAction SilentlyContinue
            Remove-Item "${batchFile}.out" -ErrorAction SilentlyContinue
            Remove-Item "${batchFile}.err" -ErrorAction SilentlyContinue
        }
        
        Start-Sleep -Milliseconds 500
    }
}

function Test-CopilotExplain {
    Write-Host "`n╔═══ GitHub Copilot: Code Explanation Test ═══╗" -ForegroundColor Magenta
    
    $codeToExplain = @"
function quickSort(arr) {
    if (arr.length <= 1) return arr;
    const pivot = arr[arr.length - 1];
    const left = arr.filter((el, i) => el <= pivot && i < arr.length - 1);
    const right = arr.filter(el => el > pivot);
    return [...quickSort(left), pivot, ...quickSort(right)];
}
"@
    
    $explainTest = @"
!plugin enable github.copilot
/analyze explain this code: $codeToExplain
/exit
"@
    
    $batchFile = "d:\rawrxd\test_copilot_explain.txt"
    $explainTest | Out-File -FilePath $batchFile -Encoding UTF8
    
    try {
        Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
            -ArgumentList "--cli", "--batch", $batchFile `
            -Wait -NoNewWindow `
            -RedirectStandardOutput "d:\rawrxd\copilot_explain_out.txt" `
            -RedirectStandardError "d:\rawrxd\copilot_explain_err.txt" `
            -TimeoutSec 30
        
        if (Test-Path "d:\rawrxd\copilot_explain_out.txt") {
            $output = Get-Content "d:\rawrxd\copilot_explain_out.txt" -Raw
            
            if ($output -match "sort|algorithm|quicksort|pivot|recursive") {
                Write-Feature "Code Explanation" "✓" "Accurate code analysis"
            } else {
                Write-Feature "Code Explanation" "⚠" "Explanation quality unclear"
            }
        }
    } catch {
        Write-Feature "Code Explanation" "✗" $_.Exception.Message
    } finally {
        Remove-Item $batchFile -ErrorAction SilentlyContinue
    }
}

function Test-AmazonQChat {
    Write-Host "`n╔═══ Amazon Q: Chat Interface Test ═══╗" -ForegroundColor Blue
    
    $queries = @(
        @{ Query = "What is AWS Lambda?"; Expected = "Lambda|serverless|AWS|function" },
        @{ Query = "How do I use S3 buckets?"; Expected = "S3|bucket|storage|object" },
        @{ Query = "Explain IAM roles"; Expected = "IAM|role|permission|policy" }
    )
    
    foreach ($q in $queries) {
        $chatTest = @"
!plugin enable amazonwebservices.amazon-q-vscode
/chat $($q.Query)
/exit
"@
        
        $batchFile = "d:\rawrxd\test_amazonq_chat_$(Get-Random).txt"
        $chatTest | Out-File -FilePath $batchFile -Encoding UTF8
        
        try {
            Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
                -ArgumentList "--cli", "--batch", $batchFile `
                -Wait -NoNewWindow `
                -RedirectStandardOutput "${batchFile}.out" `
                -RedirectStandardError "${batchFile}.err" `
                -TimeoutSec 30
            
            if (Test-Path "${batchFile}.out") {
                $output = Get-Content "${batchFile}.out" -Raw
                
                if ($output -match $q.Expected) {
                    Write-Feature "Chat: $($q.Query)" "✓" "AWS-relevant response"
                } elseif ($output.Length -gt 50) {
                    Write-Feature "Chat: $($q.Query)" "⚠" "Response received"
                } else {
                    Write-Feature "Chat: $($q.Query)" "✗" "No meaningful response"
                }
            }
        } catch {
            Write-Feature "Chat: $($q.Query)" "✗" $_.Exception.Message
        } finally {
            Remove-Item $batchFile -ErrorAction SilentlyContinue
            Remove-Item "${batchFile}.out" -ErrorAction SilentlyContinue
            Remove-Item "${batchFile}.err" -ErrorAction SilentlyContinue
        }
        
        Start-Sleep -Milliseconds 500
    }
}

function Test-AmazonQSecurity {
    Write-Host "`n╔═══ Amazon Q: Security Scan Test ═══╗" -ForegroundColor Blue
    
    $vulnCode = @"
import sqlite3

def get_user(username):
    conn = sqlite3.connect('users.db')
    cursor = conn.cursor()
    query = "SELECT * FROM users WHERE username = '" + username + "'"
    cursor.execute(query)
    return cursor.fetchone()
"@
    
    $testFile = "d:\rawrxd\test_vuln_code.py"
    $vulnCode | Out-File -FilePath $testFile -Encoding UTF8
    
    $securityTest = @"
!plugin enable amazonwebservices.amazon-q-vscode
/security $testFile
/exit
"@
    
    $batchFile = "d:\rawrxd\test_amazonq_security.txt"
    $securityTest | Out-File -FilePath $batchFile -Encoding UTF8
    
    try {
        Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
            -ArgumentList "--cli", "--batch", $batchFile `
            -Wait -NoNewWindow `
            -RedirectStandardOutput "d:\rawrxd\amazonq_security_out.txt" `
            -RedirectStandardError "d:\rawrxd\amazonq_security_err.txt" `
            -TimeoutSec 30
        
        if (Test-Path "d:\rawrxd\amazonq_security_out.txt") {
            $output = Get-Content "d:\rawrxd\amazonq_security_out.txt" -Raw
            
            if ($output -match "SQL injection|vulnerability|security|injection") {
                Write-Feature "Security Scanning" "✓" "Detected SQL injection vulnerability"
            } elseif ($output -match "scan|check|analysis") {
                Write-Feature "Security Scanning" "⚠" "Scan completed, results unclear"
            } else {
                Write-Feature "Security Scanning" "✗" "Security scan failed"
            }
        }
    } catch {
        Write-Feature "Security Scanning" "✗" $_.Exception.Message
    } finally {
        Remove-Item $testFile -ErrorAction SilentlyContinue
        Remove-Item $batchFile -ErrorAction SilentlyContinue
    }
}

function Test-AmazonQCodegen {
    Write-Host "`n╔═══ Amazon Q: Code Generation Test ═══╗" -ForegroundColor Blue
    
    $codegenTest = @"
!plugin enable amazonwebservices.amazon-q-vscode
/suggest Create a Lambda function that processes S3 events
/exit
"@
    
    $batchFile = "d:\rawrxd\test_amazonq_codegen.txt"
    $codegenTest | Out-File -FilePath $batchFile -Encoding UTF8
    
    try {
        Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
            -ArgumentList "--cli", "--batch", $batchFile `
            -Wait -NoNewWindow `
            -RedirectStandardOutput "d:\rawrxd\amazonq_codegen_out.txt" `
            -RedirectStandardError "d:\rawrxd\amazonq_codegen_err.txt" `
            -TimeoutSec 30
        
        if (Test-Path "d:\rawrxd\amazonq_codegen_out.txt") {
            $output = Get-Content "d:\rawrxd\amazonq_codegen_out.txt" -Raw
            
            if ($output -match "def lambda_handler|import boto3|event|context") {
                Write-Feature "Lambda Code Generation" "✓" "Generated AWS Lambda code"
            } elseif ($output.Length -gt 100) {
                Write-Feature "Lambda Code Generation" "⚠" "Code generated, quality unclear"
            } else {
                Write-Feature "Lambda Code Generation" "✗" "No code generated"
            }
        }
    } catch {
        Write-Feature "Lambda Code Generation" "✗" $_.Exception.Message
    } finally {
        Remove-Item $batchFile -ErrorAction SilentlyContinue
    }
}

function Test-GUIIntegration {
    param([string]$ExtensionName)
    
    Write-Host "`n╔═══ GUI Integration: $ExtensionName ═══╗" -ForegroundColor Cyan
    
    # Check if GUI menu items exist
    $configPath = "d:\rawrxd\rawrxd.config.json"
    if (Test-Path $configPath) {
        try {
            $config = Get-Content $configPath -Raw | ConvertFrom-Json
            
            if ($config.extensions -and $config.extensions.$ExtensionName) {
                Write-Feature "Configuration Entry" "✓" "Extension configured"
            } else {
                Write-Feature "Configuration Entry" "⚠" "No config entry found"
            }
        } catch {
            Write-Feature "Configuration Entry" "✗" "Could not parse config"
        }
    }
    
    # Check for GUI integration files
    $pluginDir = "d:\rawrxd\plugins\$ExtensionName"
    if (Test-Path $pluginDir) {
        $jsFiles = Get-ChildItem $pluginDir -Filter "*.js" -Recurse
        $guiFiles = $jsFiles | Where-Object { $_.Name -match "view|panel|ui|widget" }
        
        if ($guiFiles.Count -gt 0) {
            Write-Feature "GUI Components" "✓" "$($guiFiles.Count) UI files found"
        } else {
            Write-Feature "GUI Components" "→" "No specific GUI files detected"
        }
    }
}

function Test-ExtensionPerformance {
    param([string]$ExtensionID)
    
    Write-Host "`n╔═══ Performance Test: $ExtensionID ═══╗" -ForegroundColor Yellow
    
    $performanceTest = @"
!plugin load d:\rawrxd\$ExtensionID-latest.vsix
!plugin enable $ExtensionID
/chat Hello
/exit
"@
    
    $batchFile = "d:\rawrxd\test_perf_${ExtensionID}.txt"
    $performanceTest | Out-File -FilePath $batchFile -Encoding UTF8
    
    try {
        $startTime = Get-Date
        $process = Start-Process -FilePath "d:\rawrxd\RawrXD.exe" `
            -ArgumentList "--cli", "--batch", $batchFile `
            -Wait -NoNewWindow -PassThru `
            -RedirectStandardOutput "d:\rawrxd\perf_${ExtensionID}_out.txt" `
            -RedirectStandardError "d:\rawrxd\perf_${ExtensionID}_err.txt"
        
        $endTime = Get-Date
        $duration = ($endTime - $startTime).TotalSeconds
        
        if ($duration -lt 10) {
            Write-Feature "Load Time" "✓" "$([math]::Round($duration, 2))s — Fast"
        } elseif ($duration -lt 30) {
            Write-Feature "Load Time" "⚠" "$([math]::Round($duration, 2))s — Acceptable"
        } else {
            Write-Feature "Load Time" "✗" "$([math]::Round($duration, 2))s — Slow"
        }
        
        # Check memory usage
        if ($process.PeakWorkingSet64) {
            $memoryMB = [math]::Round($process.PeakWorkingSet64 / 1MB, 2)
            if ($memoryMB -lt 500) {
                Write-Feature "Memory Usage" "✓" "${memoryMB}MB — Efficient"
            } elseif ($memoryMB -lt 1000) {
                Write-Feature "Memory Usage" "⚠" "${memoryMB}MB — Moderate"
            } else {
                Write-Feature "Memory Usage" "✗" "${memoryMB}MB — High"
            }
        }
    } catch {
        Write-Feature "Performance Test" "✗" $_.Exception.Message
    } finally {
        Remove-Item $batchFile -ErrorAction SilentlyContinue
    }
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

Clear-Host
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║      RawrXD IDE - Extension Feature Validation Suite         ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Host "Testing Extension: $($Extension.ToUpper())" -ForegroundColor White
Write-Host "Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`n" -ForegroundColor Gray

# GitHub Copilot Tests
if ($Extension -eq "copilot" -or $Extension -eq "both") {
    Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Magenta
    Write-Host " GITHUB COPILOT FEATURE TESTS" -ForegroundColor Magenta
    Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Magenta
    
    Test-CopilotAutocomplete
    Test-CopilotChat
    Test-CopilotExplain
    Test-GUIIntegration -ExtensionName "copilot"
    Test-ExtensionPerformance -ExtensionID "copilot"
}

# Amazon Q Tests
if ($Extension -eq "amazonq" -or $Extension -eq "both") {
    Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Blue
    Write-Host " AMAZON Q FEATURE TESTS" -ForegroundColor Blue
    Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Blue
    
    Test-AmazonQChat
    Test-AmazonQSecurity
    Test-AmazonQCodegen
    Test-GUIIntegration -ExtensionName "amazon-q-vscode"
    Test-ExtensionPerformance -ExtensionID "amazon-q-vscode"
}

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                Feature Validation Complete                    ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "Full test logs saved in d:\rawrxd\*_out.txt files" -ForegroundColor Gray
Write-Host "`nNote: Some features may require authentication or API keys." -ForegroundColor Yellow
Write-Host "Set GITHUB_TOKEN or AWS credentials for full functionality.`n" -ForegroundColor Yellow
