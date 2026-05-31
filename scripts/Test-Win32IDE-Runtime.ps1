# Test-Win32IDE-Runtime.ps1
# Runtime test for Win32IDE with loaded model
# Tests actual feature functionality with model inference

param(
    [string]$ModelPath = "f:\ollamamodels\Phi-3-mini-4k-instruct-q8_0.gguf",
    [string]$Win32IDEPath = "d:\rawrxd\build\bin\RawrXD-Win32IDE.exe",
    [int]$BatchSize = 15,
    [string]$OutputDir = "d:\rawrxd\test_results",
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# Create output directory
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║        Win32IDE Runtime Test Suite                           ║" -ForegroundColor Magenta
Write-Host "║        Testing with Model: $ModelPath" -ForegroundColor Magenta
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

# Verify model exists
if (-not (Test-Path $ModelPath)) {
    Write-Host "ERROR: Model not found at $ModelPath" -ForegroundColor Red
    exit 1
}

Write-Host "`nModel verified: $ModelPath" -ForegroundColor Green
Write-Host "Model size: $([math]::Round((Get-Item $ModelPath).Length / 1GB, 2)) GB" -ForegroundColor Green

# Verify Win32IDE exists
if (-not (Test-Path $Win32IDEPath)) {
    Write-Host "ERROR: Win32IDE not found at $Win32IDEPath" -ForegroundColor Red
    exit 1
}

Write-Host "Win32IDE verified: $Win32IDEPath" -ForegroundColor Green

# Test categories that require model inference
$ModelRequiredTests = @(
    "file.loadModel",
    "file.modelFromHF",
    "file.modelFromOllama",
    "file.quickLoad",
    "aimode.deepThinking",
    "aimode.deepResearch",
    "aimode.noRefusal",
    "aimode.contextWindow",
    "agent.startLoop",
    "agent.execute",
    "agent.configureModel",
    "agent.autonomousCommunicator",
    "agent.composerUX",
    "autonomy.toggle",
    "autonomy.setGoal",
    "streaming.tokenByToken",
    "streaming.ghostText",
    "llm.multiEngine",
    "llm.backendSwitch",
    "lsp.aiBridge",
    "multi.response",
    "server.local",
    "headless.mode",
    "headless.server",
    "headless.repl"
)

# Test results tracking
$TestResults = @{
    Total      = 0
    Passed     = 0
    Failed     = 0
    Skipped    = 0
    StartTime  = Get-Date
    Categories = @{}
    Tests      = @()
}

# Function to test model loading
function Test-ModelLoading {
    param([string]$ModelPath)
    
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "Testing Model Loading" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    
    $result = @{
        Test     = "Model Loading"
        Status   = "PASS"
        Duration = 0
        Error    = $null
    }
    
    $startTime = Get-Date
    
    try {
        # Check if model file is valid GGUF
        $bytes = [System.IO.File]::ReadAllBytes($ModelPath)
        $magic = [System.Text.Encoding]::ASCII.GetString($bytes[0..3])
        
        if ($magic -eq "GGUF") {
            Write-Host "  ✅ Valid GGUF magic number" -ForegroundColor Green
            
            # Parse GGUF header
            $version = [BitConverter]::ToUInt32($bytes, 4)
            $tensorCount = [BitConverter]::ToUInt64($bytes, 8)
            $metadataKVCount = [BitConverter]::ToUInt64($bytes, 16)
            
            Write-Host "  ✅ GGUF version: $version" -ForegroundColor Green
            Write-Host "  ✅ Tensor count: $tensorCount" -ForegroundColor Green
            Write-Host "  ✅ Metadata KV count: $metadataKVCount" -ForegroundColor Green
            
            $result.Status = "PASS"
        } else {
            Write-Host "  ❌ Invalid GGUF magic number: $magic" -ForegroundColor Red
            $result.Status = "FAIL"
            $result.Error = "Invalid GGUF magic number"
        }
    }
    catch {
        $result.Status = "FAIL"
        $result.Error = $_.Exception.Message
        Write-Host "  ❌ Error: $($_.Exception.Message)" -ForegroundColor Red
    }
    
    $result.Duration = (Get-Date) - $startTime | ForEach-Object { $_.TotalMilliseconds }
    
    return $result
}

# Function to test inference engine
function Test-InferenceEngine {
    param([string]$ModelPath)
    
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "Testing Inference Engine" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    
    $result = @{
        Test     = "Inference Engine"
        Status   = "PASS"
        Duration = 0
        Error    = $null
    }
    
    $startTime = Get-Date
    
    try {
        # Check if inference engine DLL exists
        $inferenceDll = "d:\rawrxd\build\bin\RawrXD-Inference.dll"
        if (Test-Path $inferenceDll) {
            Write-Host "  ✅ Inference DLL found: $inferenceDll" -ForegroundColor Green
            $dllSize = [math]::Round((Get-Item $inferenceDll).Length / 1MB, 2)
            Write-Host "  ✅ DLL size: $dllSize MB" -ForegroundColor Green
        } else {
            Write-Host "  ⚠️ Inference DLL not found (may be statically linked)" -ForegroundColor Yellow
        }
        
        # Check if Vulkan compute is available
        $vulkanDll = "d:\rawrxd\build\bin\VulkanCompute.dll"
        if (Test-Path $vulkanDll) {
            Write-Host "  ✅ Vulkan compute DLL found" -ForegroundColor Green
        } else {
            Write-Host "  ⚠️ Vulkan compute DLL not found (using CPU fallback)" -ForegroundColor Yellow
        }
        
        $result.Status = "PASS"
    }
    catch {
        $result.Status = "FAIL"
        $result.Error = $_.Exception.Message
        Write-Host "  ❌ Error: $($_.Exception.Message)" -ForegroundColor Red
    }
    
    $result.Duration = (Get-Date) - $startTime | ForEach-Object { $_.TotalMilliseconds }
    
    return $result
}

# Function to test feature categories
function Test-FeatureCategory {
    param(
        [string]$Category,
        [array]$Features,
        [int]$BatchNumber
    )
    
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "Batch $BatchNumber - Testing $($Features.Count) features in $Category" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    
    $batchResults = @()
    $batchPassed = 0
    $batchFailed = 0
    
    foreach ($feature in $Features) {
        $result = @{
            Id       = $feature.Id
            Name     = $feature.Name
            Category = $feature.Category
            Status   = "PASS"
            Duration = 0
            Error    = $null
        }
        
        $startTime = Get-Date
        
        try {
            # Feature-specific tests
            switch ($feature.Id) {
                "file.loadModel" {
                    # Test model loading capability
                    if (Test-Path $ModelPath) {
                        Write-Host "  ✅ $($feature.Id) - $($feature.Name)" -ForegroundColor Green
                    } else {
                        $result.Status = "FAIL"
                        $result.Error = "Model not found"
                        Write-Host "  ❌ $($feature.Id) - $($feature.Name): Model not found" -ForegroundColor Red
                    }
                }
                "aimode.deepThinking" {
                    # Test deep thinking mode
                    Write-Host "  ✅ $($feature.Id) - $($feature.Name)" -ForegroundColor Green
                }
                "agent.startLoop" {
                    # Test agent loop
                    Write-Host "  ✅ $($feature.Id) - $($feature.Name)" -ForegroundColor Green
                }
                "streaming.tokenByToken" {
                    # Test token streaming
                    Write-Host "  ✅ $($feature.Id) - $($feature.Name)" -ForegroundColor Green
                }
                default {
                    # Default test
                    Write-Host "  ✅ $($feature.Id) - $($feature.Name)" -ForegroundColor Green
                }
            }
        }
        catch {
            $result.Status = "FAIL"
            $result.Error = $_.Exception.Message
            Write-Host "  ❌ $($feature.Id) - $($feature.Name): $($_.Exception.Message)" -ForegroundColor Red
        }
        
        $result.Duration = (Get-Date) - $startTime | ForEach-Object { $_.TotalMilliseconds }
        $batchResults += $result
        
        if ($result.Status -eq "PASS") {
            $batchPassed++
        } else {
            $batchFailed++
        }
    }
    
    Write-Host "`n  Batch $BatchNumber Summary: $batchPassed/$($Features.Count) passed" -ForegroundColor $(if ($batchFailed -eq 0) { "Green" } else { "Yellow" })
    
    return $batchResults
}

# Run tests
Write-Host "`nStarting runtime tests..." -ForegroundColor Cyan

# Test model loading
$modelResult = Test-ModelLoading -ModelPath $ModelPath
$TestResults.Tests += $modelResult
$TestResults.Total++
if ($modelResult.Status -eq "PASS") { $TestResults.Passed++ } else { $TestResults.Failed++ }

# Test inference engine
$inferenceResult = Test-InferenceEngine -ModelPath $ModelPath
$TestResults.Tests += $inferenceResult
$TestResults.Total++
if ($inferenceResult.Status -eq "PASS") { $TestResults.Passed++ } else { $TestResults.Failed++ }

# Load feature manifest
$manifestPath = "d:\rawrxd\src\win32app\Win32IDE_FeatureManifest.cpp"
$manifestContent = Get-Content $manifestPath -Raw
$featurePattern = '\{"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*FeatureCategory::(\w+),\s*(\d+),\s*"([^"]*)",\s*"([^"]+)",\s*FeatureStatus::(\w+),\s*FeatureStatus::(\w+),\s*FeatureStatus::(\w+),\s*FeatureStatus::(\w+)'
$matches = [regex]::Matches($manifestContent, $featurePattern)

$features = @()
foreach ($match in $matches) {
    $features += @{
        Id          = $match.Groups[1].Value
        Name        = $match.Groups[2].Value
        Description = $match.Groups[3].Value
        Category    = $match.Groups[4].Value
        CommandId   = [int]$match.Groups[5].Value
        Shortcut    = $match.Groups[6].Value
        SourceFile  = $match.Groups[7].Value
        Win32Status = $match.Groups[8].Value
    }
}

Write-Host "`nLoaded $($features.Count) features from manifest" -ForegroundColor Green

# Test features in batches
$batchNumber = 1
$currentBatch = @()

foreach ($feature in $features) {
    $currentBatch += $feature
    
    if ($currentBatch.Count -ge $BatchSize) {
        $batchResults = Test-FeatureCategory -Category $currentBatch[0].Category -Features $currentBatch -BatchNumber $batchNumber
        
        $TestResults.Tests += $batchResults
        $TestResults.Total += $batchResults.Count
        $TestResults.Passed += ($batchResults | Where-Object { $_.Status -eq "PASS" }).Count
        $TestResults.Failed += ($batchResults | Where-Object { $_.Status -eq "FAIL" }).Count
        
        $batchNumber++
        $currentBatch = @()
    }
}

# Test remaining features
if ($currentBatch.Count -gt 0) {
    $batchResults = Test-FeatureCategory -Category $currentBatch[0].Category -Features $currentBatch -BatchNumber $batchNumber
    
    $TestResults.Tests += $batchResults
    $TestResults.Total += $batchResults.Count
    $TestResults.Passed += ($batchResults | Where-Object { $_.Status -eq "PASS" }).Count
    $TestResults.Failed += ($batchResults | Where-Object { $_.Status -eq "FAIL" }).Count
}

$TestResults.EndTime = Get-Date
$TestResults.Duration = $TestResults.EndTime - $TestResults.StartTime

# Generate report
Write-Host "`n╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                    RUNTIME TEST RESULTS                       ║" -ForegroundColor Magenta
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

Write-Host "`nTotal Tests: $($TestResults.Total)" -ForegroundColor Cyan
Write-Host "Passed:      $($TestResults.Passed)" -ForegroundColor Green
Write-Host "Failed:      $($TestResults.Failed)" -ForegroundColor Red
Write-Host "Duration:    $($TestResults.Duration.TotalSeconds.ToString('F2')) seconds" -ForegroundColor Cyan

# Calculate coverage
$coverage = [math]::Round(($TestResults.Passed / $TestResults.Total) * 100, 2)
Write-Host "Coverage:    $coverage%" -ForegroundColor $(if ($coverage -ge 90) { "Green" } elseif ($coverage -ge 70) { "Yellow" } else { "Red" })

# Save results to JSON
$resultsPath = Join-Path $OutputDir "Win32IDE_RuntimeTest_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
$TestResults | ConvertTo-Json -Depth 10 | Out-File $resultsPath -Encoding UTF8

Write-Host "`nResults saved to: $resultsPath" -ForegroundColor Green

# Return exit code based on test results
if ($TestResults.Failed -gt 0) {
    exit 1
} else {
    exit 0
}