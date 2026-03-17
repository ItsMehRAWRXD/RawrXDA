# Validation Script - Test A/B Testing Framework Installation
# Checks that all components are in place and working

Write-Host "╔════════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║        A/B Testing Framework - Installation Validator                        ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Color codes
$checkmark = "✓"
$cross = "✗"

# Track results
$passed = 0
$failed = 0

# Test 1: Required files exist
Write-Host "Step 1: Checking required files..." -ForegroundColor Yellow

$requiredFiles = @(
    "A-B-Testing-RealTime-Models.ps1",
    "A-B-Testing-RealTime-Streaming.ps1",
    "ab-testing-real-models.py",
    "A-B-Testing-Launcher.ps1",
    "A-B-TESTING-GUIDE.md",
    "A-B-TESTING-QUICK-REFERENCE.md",
    "A-B-TESTING-CURL-COMMANDS.md",
    "A-B-Testing-Framework-Summary.md",
    "README-A-B-TESTING.md"
)

foreach ($file in $requiredFiles) {
    $path = Join-Path "e:\" $file
    if (Test-Path $path) {
        Write-Host "  $checkmark $file" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "  $cross $file - NOT FOUND" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""

# Test 2: PowerShell version
Write-Host "Step 2: Checking PowerShell version..." -ForegroundColor Yellow

$psVersion = $PSVersionTable.PSVersion.Major
if ($psVersion -ge 5) {
    Write-Host "  $checkmark PowerShell $psVersion (required: 5+)" -ForegroundColor Green
    $passed++
} else {
    Write-Host "  $cross PowerShell $psVersion (required: 5+)" -ForegroundColor Red
    $failed++
}

Write-Host ""

# Test 3: curl availability
Write-Host "Step 3: Checking curl availability..." -ForegroundColor Yellow

try {
    $curlVersion = & curl.exe --version 2>&1
    if ($LASTEXITCODE -eq 0) {
        $versionLine = $curlVersion -split "`n" | Select-Object -First 1
        Write-Host "  $checkmark curl is available" -ForegroundColor Green
        Write-Host "    $versionLine" -ForegroundColor Gray
        $passed++
    } else {
        Write-Host "  $cross curl returned error code: $LASTEXITCODE" -ForegroundColor Red
        $failed++
    }
} catch {
    Write-Host "  $cross curl not found or error executing" -ForegroundColor Red
    $failed++
}

Write-Host ""

# Test 4: Ollama connectivity
Write-Host "Step 4: Checking Ollama connectivity..." -ForegroundColor Yellow

try {
    $response = & curl.exe -s --max-time 2 "http://localhost:11434/api/tags" 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  $checkmark Ollama is responding" -ForegroundColor Green
        $passed++
        
        # Check if models are available
        try {
            $models = $response | ConvertFrom-Json
            $modelCount = $models.models.Count
            Write-Host "    Models available: $modelCount" -ForegroundColor Gray
            
            if ($modelCount -gt 0) {
                Write-Host "    First model: $($models.models[0].name)" -ForegroundColor Gray
            } else {
                Write-Host "    Warning: No models loaded. Run: ollama pull mistral:latest" -ForegroundColor Yellow
            }
        } catch {
            Write-Host "    Note: Could not parse model list" -ForegroundColor Gray
        }
    } else {
        Write-Host "  $cross Ollama is not responding (http://localhost:11434)" -ForegroundColor Red
        Write-Host "    Make sure Ollama is running: ollama serve" -ForegroundColor Yellow
        $failed++
    }
} catch {
    Write-Host "  $cross Connection test failed: $_" -ForegroundColor Red
    $failed++
}

Write-Host ""

# Test 5: Python availability (optional)
Write-Host "Step 5: Checking Python availability (optional)..." -ForegroundColor Yellow

try {
    $pythonVersion = & python --version 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  $checkmark Python is available: $pythonVersion" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "  $cross Python returned error code: $LASTEXITCODE" -ForegroundColor Yellow
        Write-Host "    Python is optional - install from https://www.python.org/" -ForegroundColor Gray
    }
} catch {
    Write-Host "  ⚠ Python not found (optional)" -ForegroundColor Yellow
    Write-Host "    Python is optional - install from https://www.python.org/" -ForegroundColor Gray
}

Write-Host ""

# Test 6: File permissions (PowerShell scripts executable)
Write-Host "Step 6: Checking script permissions..." -ForegroundColor Yellow

$scriptFiles = @(
    "A-B-Testing-RealTime-Models.ps1",
    "A-B-Testing-RealTime-Streaming.ps1",
    "A-B-Testing-Launcher.ps1"
)

foreach ($script in $scriptFiles) {
    $path = Join-Path "e:\" $script
    if (Test-Path $path) {
        $item = Get-Item $path
        if ($item.PSIsContainer -eq $false) {
            Write-Host "  $checkmark $script is readable" -ForegroundColor Green
            $passed++
        }
    }
}

Write-Host ""

# Test 7: Quick API test (generate text)
Write-Host "Step 7: Quick API functionality test..." -ForegroundColor Yellow

try {
    $payload = @{
        model = "mistral:latest"
        prompt = "hi"
        stream = $false
    } | ConvertTo-Json
    
    Write-Host "  Testing text generation..." -ForegroundColor Gray
    $testStart = Get-Date
    $result = & curl.exe -s --max-time 30 -X POST "http://localhost:11434/api/generate" `
        -H "Content-Type: application/json" `
        -d $payload 2>$null
    $testEnd = Get-Date
    
    if ($LASTEXITCODE -eq 0) {
        try {
            $json = $result | ConvertFrom-Json
            $responseTime = ($testEnd - $testStart).TotalMilliseconds
            Write-Host "  $checkmark API test successful" -ForegroundColor Green
            Write-Host "    Response time: $([Math]::Round($responseTime, 2))ms" -ForegroundColor Gray
            Write-Host "    Response length: $($json.response.Length) characters" -ForegroundColor Gray
            $passed++
        } catch {
            Write-Host "  $cross API returned non-JSON response" -ForegroundColor Red
            $failed++
        }
    } else {
        Write-Host "  $cross API test failed or timed out" -ForegroundColor Red
        $failed++
    }
} catch {
    Write-Host "  $cross API test error: $_" -ForegroundColor Red
    $failed++
}

Write-Host ""

# Summary
Write-Host "╔════════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                            VALIDATION SUMMARY                                  ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

Write-Host "Passed: $passed" -ForegroundColor Green
Write-Host "Failed: $failed" -ForegroundColor Red

Write-Host ""

if ($failed -eq 0) {
    Write-Host "✓ Framework is ready to use!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Yellow
    Write-Host "  1. Start quick test:" -ForegroundColor White
    Write-Host "     .\A-B-Testing-Launcher.ps1 -Mode quick" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  2. Or run directly:" -ForegroundColor White
    Write-Host "     .\A-B-Testing-RealTime-Models.ps1 -NumTests 3" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  3. Or view documentation:" -ForegroundColor White
    Write-Host "     Get-Content A-B-TESTING-QUICK-REFERENCE.md | more" -ForegroundColor Cyan
} else {
    Write-Host "✗ Some issues need to be fixed before using the framework" -ForegroundColor Red
    Write-Host ""
    Write-Host "Common fixes:" -ForegroundColor Yellow
    Write-Host "  • Make sure Ollama is running: ollama serve" -ForegroundColor White
    Write-Host "  • Check Ollama has models: ollama list" -ForegroundColor White
    Write-Host "  • Pull a model if needed: ollama pull mistral:latest" -ForegroundColor White
    Write-Host "  • For PowerShell errors, run as Administrator" -ForegroundColor White
}

Write-Host ""
Write-Host "For more help, see: Get-Content README-A-B-TESTING.md | more" -ForegroundColor Gray
