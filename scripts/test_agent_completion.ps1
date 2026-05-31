# Test script for AGENT_DONE completion contract system
# Validates both emission and parsing functionality

param(
    [switch] $TestEmission,
    [switch] $TestParsing,
    [switch] $TestValidation,
    [switch] $All
)

if ($All) {
    $TestEmission = $true
    $TestParsing = $true
    $TestValidation = $true
}

# Import the completion validator
. D:\RawrXD\scripts\agent_completion_validator.ps1

Write-Host "=== Testing AGENT_DONE Completion Contract System ===" -ForegroundColor Green

# Test 1: Emission
if ($TestEmission) {
    Write-Host "\n1. Testing Emission..." -ForegroundColor Yellow
    
    $result = Emit-AgentDone -Status "success" -Phase "2C" -TasksCompleted 7 -TasksTotal 7 `
        -Commit "7f9ca5ddb" -Artifacts @("run_kernel_ab_sweep.ps1", "inference_latency_breakdown.h") `
        -NextPhase "Phase D kernel sweep" -DurationMs 18234 -Branches 2 -ArtifactsVerified $true
    
    Write-Host "Emission Result:" -ForegroundColor Cyan
    Write-Host $result
    
    # Test JSON-only emission
    $jsonResult = Emit-AgentDone -Status "success" -Phase "2C" -TasksCompleted 7 -TasksTotal 7 `
        -Commit "7f9ca5ddb" -OutputFormat "json"
    
    Write-Host "JSON Emission Result:" -ForegroundColor Cyan
    Write-Host $jsonResult
}

# Test 2: Parsing
if ($TestParsing) {
    Write-Host "\n2. Testing Parsing..." -ForegroundColor Yellow
    
    # Create test content with AGENT_DONE block
    $testContent = @"
Some agent execution log
With various output lines

[AGENT_DONE]
status=success
phase=2C
tasks_completed=7
tasks_total=7
commit=7f9ca5ddb
artifacts=2
next=Phase D kernel sweep
duration_ms=18234
branches=2
artifacts_verified=true
timestamp=2026-05-05 12:34:56.789

More log content after the block
"@
    
    $contract = Parse-AgentDoneBlock $testContent
    
    if ($contract) {
        Write-Host "Parsed Contract:" -ForegroundColor Cyan
        $contract.GetEnumerator() | Sort-Object Name | ForEach-Object {
            Write-Host "  $($_.Key)=$($_.Value)"
        }
    } else {
        Write-Host "FAIL: Could not parse AGENT_DONE block" -ForegroundColor Red
    }
}

# Test 3: Validation
if ($TestValidation) {
    Write-Host "\n3. Testing Validation..." -ForegroundColor Yellow
    
    # Create test files
    $tempDir = "D:\temp_agent_test"
    if (-not (Test-Path $tempDir)) {
        New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
    }
    
    # Test case 1: Valid completion
    $validContent = @"
Agent execution log

[AGENT_DONE]
status=success
phase=2C
tasks_completed=7
tasks_total=7
commit=7f9ca5ddb
next=Phase D kernel sweep
"@
    
    $validFile = Join-Path $tempDir "valid_completion.log"
    $validContent | Out-File $validFile -Encoding UTF8
    
    Write-Host "Testing valid completion..." -ForegroundColor Cyan
    & D:\RawrXD\scripts\agent_completion_validator.ps1 -LogFile $validFile
    $validExit = $LASTEXITCODE
    Write-Host "Exit code: $validExit"
    
    # Test case 2: Missing AGENT_DONE
    $missingContent = @"
Agent execution log
No completion block here
Just regular output
"@
    
    $missingFile = Join-Path $tempDir "missing_completion.log"
    $missingContent | Out-File $missingFile -Encoding UTF8
    
    Write-Host "Testing missing completion..." -ForegroundColor Cyan
    & D:\RawrXD\scripts\agent_completion_validator.ps1 -LogFile $missingFile -Strict
    $missingExit = $LASTEXITCODE
    Write-Host "Exit code: $missingExit (expected 7 for strict mode)"
    
    # Test case 3: Failed status
    $failedContent = @"
Agent execution log

[AGENT_DONE]
status=failed
phase=2C
tasks_completed=5
tasks_total=7
commit=7f9ca5ddb
error=kernel sweep timeout
next=retry Phase D with reduced batch
"@
    
    $failedFile = Join-Path $tempDir "failed_completion.log"
    $failedContent | Out-File $failedFile -Encoding UTF8
    
    Write-Host "Testing failed completion..." -ForegroundColor Cyan
    & D:\RawrXD\scripts\agent_completion_validator.ps1 -LogFile $failedFile
    $failedExit = $LASTEXITCODE
    Write-Host "Exit code: $failedExit (expected 9 for failure)"
    
    # Cleanup
    Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "\n=== Test Complete ===" -ForegroundColor Green
Write-Host "Next: Integrate with existing agent workflows using the completion contract" -ForegroundColor Yellow