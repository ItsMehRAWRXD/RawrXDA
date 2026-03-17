# Test Model's Adaptive Ability: Discover Environment, Then Execute
# A truly agentic model should check what's available before making a plan

param(
    [string]$Model = "quantumide-performance:latest",
    [string]$BaseUrl = "http://localhost:11434",
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = "Continue"

function Write-Log {
    param([string]$Level, [string]$Message)
    $timestamp = Get-Date -Format "yyyy-MM-ddTHH:mm:ss.fffzzz"
    Write-Host "time=$timestamp level=$Level msg=`"$Message`""
}

function Invoke-ModelQuery {
    param([string]$Prompt)
    
    $requestBody = @{
        model = $Model
        prompt = $Prompt
        stream = $false
        options = @{ temperature = 0.1 }
    } | ConvertTo-Json
    
    try {
        Write-Log "INFO" "sending prompt to model $Model"
        $response = Invoke-RestMethod -Uri "$BaseUrl/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec $TimeoutSeconds
        return $response.response
    } catch {
        Write-Log "ERROR" "model query failed: $($_.Exception.Message)"
        return $null
    }
}

Write-Log "INFO" "=========================================="
Write-Log "INFO" "Adaptive C++ Project Creation Test"
Write-Log "INFO" "=========================================="

# PHASE 1: Ask model to discover environment
Write-Log "INFO" "PHASE 1: Environment Discovery"

$discoveryPrompt = @"
TASK: You need to create and compile a C++ Hello World program.

FIRST, you need to discover what C++ compiler is available on this Windows system.

Provide the EXACT PowerShell command to check for available C++ compilers (cl, g++, clang++).
Respond with ONLY the command, nothing else:
"@

Write-Log "INFO" "asking model how to discover compilers"
$discoveryCommand = Invoke-ModelQuery -Prompt $discoveryPrompt

if (-not $discoveryCommand) {
    Write-Log "ERROR" "no discovery command from model"
    exit 1
}

Write-Host "`n=== MODEL'S DISCOVERY COMMAND ===" -ForegroundColor Cyan
Write-Host $discoveryCommand
Write-Host "=================================`n" -ForegroundColor Cyan

# Execute the discovery command
Write-Log "INFO" "executing model's discovery command"
$discoveryResult = ""
try {
    $discoveryResult = Invoke-Expression $discoveryCommand 2>&1 | Out-String
    Write-Log "INFO" "discovery command executed"
} catch {
    Write-Log "WARN" "discovery command failed: $($_.Exception.Message)"
    # Fallback: run our own discovery
    $discoveryResult = @(
        (where.exe cl 2>&1)
        (where.exe g++ 2>&1)
        (where.exe clang++ 2>&1)
    ) | Out-String
}

Write-Host "`n=== DISCOVERY RESULTS ===" -ForegroundColor Cyan
Write-Host $discoveryResult
Write-Host "=========================`n" -ForegroundColor Cyan

# PHASE 2: Give results to model and ask for adaptive plan
Write-Log "INFO" "PHASE 2: Adaptive Planning"

$adaptivePlanPrompt = @"
CONTEXT: You ran a discovery command and got these results:

$discoveryResult

Based on these results, create a complete plan to:
1. Create a new workspace directory at D:\temp\adaptive-cpp-test
2. Create a main.cpp Hello World file using Set-Content or Out-File (NOT echo)
3. Compile it using whichever compiler IS ACTUALLY AVAILABLE
4. Name the output hello.exe

Provide commands in this format (no explanations):

COMMAND_1: [command]
COMMAND_2: [command]
COMMAND_3: [command]
...

IMPORTANT: For creating the C++ file, use Set-Content with -Value parameter or a here-string @' '@.
Do NOT use echo with \n sequences - they don't work in PowerShell.
Use the compiler that is ACTUALLY available based on the discovery results above.
"@

Write-Log "INFO" "asking model to create adaptive plan"
$adaptivePlan = Invoke-ModelQuery -Prompt $adaptivePlanPrompt

if (-not $adaptivePlan) {
    Write-Log "ERROR" "no adaptive plan from model"
    exit 1
}

Write-Host "`n=== MODEL'S ADAPTIVE PLAN ===" -ForegroundColor Cyan
Write-Host $adaptivePlan
Write-Host "==============================`n" -ForegroundColor Cyan

# Parse commands
$commands = @()
foreach ($line in ($adaptivePlan -split "`n")) {
    if ($line -match '^COMMAND_\d+:\s*(.+)$') {
        $cmd = $matches[1].Trim()
        $commands += $cmd
        Write-Log "INFO" "extracted command: $cmd"
    }
}

# Validate plan
Write-Log "INFO" "=========================================="
Write-Log "INFO" "Validating Adaptive Plan"
Write-Log "INFO" "=========================================="

$score = 0
$maxScore = 0

# Check 1: Uses available compiler
$maxScore++
$usesAvailableCompiler = $false
if ($discoveryResult -match 'g\+\+\.exe' -and ($adaptivePlan -match 'g\+\+')) {
    Write-Log "INFO" "✓ Correctly chose g++ (available)"
    $score++
    $usesAvailableCompiler = $true
}
elseif ($discoveryResult -match 'cl\.exe' -and ($adaptivePlan -match '\bcl\b|\bcl\.exe')) {
    Write-Log "INFO" "✓ Correctly chose cl (available)"
    $score++
    $usesAvailableCompiler = $true
}
elseif ($discoveryResult -match 'clang\+\+\.exe' -and ($adaptivePlan -match 'clang\+\+')) {
    Write-Log "INFO" "✓ Correctly chose clang++ (available)"
    $score++
    $usesAvailableCompiler = $true
}
else {
    Write-Log "WARN" "✗ Did not adapt to available compiler"
}

# Check 2: Creates directory
$maxScore++
if ($commands | Where-Object { $_ -match 'New-Item|mkdir|md ' }) {
    Write-Log "INFO" "✓ Includes directory creation"
    $score++
} else {
    Write-Log "WARN" "✗ Missing directory creation"
}

# Check 3: Creates C++ file
$maxScore++
if ($commands | Where-Object { $_ -match 'main\.cpp' }) {
    Write-Log "INFO" "✓ Includes C++ file creation"
    $score++
} else {
    Write-Log "WARN" "✗ Missing C++ file creation"
}

# Check 4: Compiles to hello.exe
$maxScore++
if ($commands | Where-Object { $_ -match 'hello\.exe' }) {
    Write-Log "INFO" "✓ Specifies output executable"
    $score++
} else {
    Write-Log "WARN" "✗ Missing output specification"
}

$planScore = [math]::Round(($score / $maxScore) * 100, 1)
Write-Log "INFO" "Adaptive Plan Score: $score/$maxScore ($planScore%)"

# PHASE 3: Execute the plan
Write-Log "INFO" "=========================================="
Write-Log "INFO" "PHASE 3: Executing Adaptive Plan"
Write-Log "INFO" "=========================================="

$executionSuccess = $false
$workspace = "D:\temp\adaptive-cpp-test-$(Get-Date -Format 'yyyyMMdd-HHmmss')"

try {
    # Create workspace
    New-Item -ItemType Directory -Path $workspace -Force | Out-Null
    Write-Log "INFO" "created workspace: $workspace"
    
    Set-Location $workspace
    
    # Execute commands
    $fullCommandText = $adaptivePlan -replace 'D:\\temp\\adaptive-cpp-test', $workspace
    
    # Check if this is a multi-line script with here-strings
    if ($fullCommandText -match "@'") {
        Write-Log "INFO" "detected multi-line script, executing as script block"
        
        try {
            # Save to temp script file - remove COMMAND_N: prefixes
            $tempScript = "$workspace\temp_execute.ps1"
            $cleanScript = $fullCommandText -replace 'COMMAND_\d+:\s*', ''
            $cleanScript | Out-File -FilePath $tempScript -Encoding UTF8
            
            Write-Log "INFO" "executing script block"
            Push-Location $workspace
            & $tempScript 2>&1 | ForEach-Object { Write-Log "INFO" "output: $_" }
            Pop-Location
            
            Remove-Item $tempScript -ErrorAction SilentlyContinue
            Write-Log "INFO" "script execution completed"
        } catch {
            Write-Log "WARN" "script execution failed: $($_.Exception.Message)"
        }
    } else {
        # Execute line by line
        foreach ($cmd in $commands) {
            # Skip if it's creating the directory we already made
            if ($cmd -match 'New-Item.*adaptive-cpp-test|mkdir.*adaptive-cpp-test|md.*adaptive-cpp-test') {
                Write-Log "INFO" "skipping redundant directory creation"
                continue
            }
            
            # Modify paths
            $modifiedCmd = $cmd -replace 'D:\\temp\\adaptive-cpp-test', $workspace
            
            Write-Log "INFO" "executing: $modifiedCmd"
            
            try {
                $output = Invoke-Expression $modifiedCmd 2>&1
                if ($output) {
                    Write-Log "INFO" "output: $output"
                }
                Write-Log "INFO" "command succeeded"
            } catch {
                Write-Log "WARN" "command failed: $($_.Exception.Message)"
            }
        }
    }
    
    # Check results
    $exeFiles = Get-ChildItem -Path $workspace -Filter "*.exe" -ErrorAction SilentlyContinue
    
    if ($exeFiles) {
        Write-Log "INFO" "✓ Executable created: $($exeFiles[0].Name)"
        
        # Run it
        try {
            $output = & $exeFiles[0].FullName 2>&1
            Write-Log "INFO" "✓ Executable runs successfully"
            Write-Host "`n=== PROGRAM OUTPUT ===" -ForegroundColor Green
            Write-Host $output -ForegroundColor Green
            Write-Host "======================`n" -ForegroundColor Green
            
            if ($output -match "Hello|hello") {
                Write-Log "INFO" "✓ Produces correct output"
                $executionSuccess = $true
            }
        } catch {
            Write-Log "WARN" "executable failed: $($_.Exception.Message)"
        }
    } else {
        Write-Log "WARN" "✗ No executable found"
        
        # Debug: show what files exist
        $files = Get-ChildItem -Path $workspace
        Write-Log "INFO" "files in workspace: $($files.Name -join ', ')"
    }
    
} catch {
    Write-Log "ERROR" "execution failed: $($_.Exception.Message)"
}

# Final Results
Write-Log "INFO" "=========================================="
Write-Log "INFO" "FINAL RESULTS"
Write-Log "INFO" "=========================================="
Write-Log "INFO" "Model: $Model"
Write-Log "INFO" "Discovery Phase: $(if ($discoveryCommand) { 'SUCCESS' } else { 'FAILED' })"
Write-Log "INFO" "Adaptive Planning: $planScore%"
Write-Log "INFO" "Used Available Compiler: $usesAvailableCompiler"
Write-Log "INFO" "Execution Success: $executionSuccess"

Write-Host ""

if ($usesAvailableCompiler -and $executionSuccess) {
    Write-Host "✅ VERDICT: MODEL IS TRULY ADAPTIVE" -ForegroundColor Green
    Write-Host "   - Discovered environment" -ForegroundColor Green
    Write-Host "   - Adapted plan to available tools" -ForegroundColor Green
    Write-Host "   - Successfully executed" -ForegroundColor Green
}
elseif ($usesAvailableCompiler) {
    Write-Host "⚠️  VERDICT: MODEL ADAPTS BUT EXECUTION FAILED" -ForegroundColor Yellow
    Write-Host "   - Correctly identified available compiler" -ForegroundColor Yellow
    Write-Host "   - Execution had issues" -ForegroundColor Yellow
}
elseif ($executionSuccess) {
    Write-Host "🔧 VERDICT: EXECUTION WORKED BY LUCK" -ForegroundColor Cyan
    Write-Host "   - Did not properly adapt to environment" -ForegroundColor Cyan
    Write-Host "   - But execution succeeded anyway" -ForegroundColor Cyan
}
else {
    Write-Host "❌ VERDICT: MODEL IS NOT ADAPTIVE" -ForegroundColor Red
    Write-Host "   - Failed to adapt to available tools" -ForegroundColor Red
    Write-Host "   - Execution failed" -ForegroundColor Red
}

Write-Host "`nWorkspace: $workspace" -ForegroundColor Gray
Write-Host "Discovery Results saved in memory" -ForegroundColor Gray
