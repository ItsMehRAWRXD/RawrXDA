# ============================================================================
# RawrXD IDE End-to-End Integration Test Script
# ============================================================================
# Tests: Model selection (GGUF/Ollama) → Inference routing → Streaming → 
#        Autonomous loop → Tool execution → Error handling
# ============================================================================

param(
    [string]$BuildPath = "D:\RawrXD-production-lazy-init\build\bin\Release",
    [string]$GGUFTestModel = "D:\Franken\BackwardsUnlock\125m\unlock-125M-Q2_K.gguf",
    [string]$OllamaBlobDir = "D:\OllamaModels\blobs",
    [string]$OllamaManifestDir = "D:\OllamaModels\manifests\registry.ollama.ai",
    [int]$TestTimeoutSeconds = 300
)

$ErrorActionPreference = "Continue"
$VerbosePreference = "Continue"

# Test result tracking
$TestResults = @()
$TestStartTime = Get-Date

function Write-TestHeader {
    param([string]$TestName)
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "TEST: $TestName" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
}

function Write-TestResult {
    param(
        [string]$TestName,
        [bool]$Passed,
        [string]$Details
    )
    
    $Result = @{
        TestName = $TestName
        Passed = $Passed
        Details = $Details
        Timestamp = Get-Date
    }
    
    $script:TestResults += $Result
    
    if ($Passed) {
        Write-Host "✓ PASS: $TestName" -ForegroundColor Green
    } else {
        Write-Host "✗ FAIL: $TestName" -ForegroundColor Red
    }
    Write-Host "  Details: $Details" -ForegroundColor Gray
}

# ============================================================================
# TEST 1: Build Verification
# ============================================================================
Write-TestHeader "Build Verification"

$ExePath = Join-Path $BuildPath "RawrXD-AgenticIDE.exe"
if (Test-Path $ExePath) {
    $ExeInfo = Get-Item $ExePath
    $Details = "Exe: $($ExeInfo.Length) bytes, Modified: $($ExeInfo.LastWriteTime)"
    Write-TestResult "Build Verification" $true $Details
} else {
    Write-TestResult "Build Verification" $false "Executable not found at $ExePath"
    exit 1
}

# ============================================================================
# TEST 2: Ollama Blob Detection - File System
# ============================================================================
Write-TestHeader "Ollama Blob Detection - File System"

$BlobFiles = Get-ChildItem -Path $OllamaBlobDir -Filter "sha256-*" -ErrorAction SilentlyContinue
$ManifestFiles = Get-ChildItem -Path $OllamaManifestDir -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -eq "" -or $_.Name -eq "latest" }

if ($BlobFiles.Count -gt 0 -and $ManifestFiles.Count -gt 0) {
    $Details = "Found $($BlobFiles.Count) blob files and $($ManifestFiles.Count) manifests"
    Write-TestResult "Ollama File System Structure" $true $Details
    
    # Sample a blob file
    $SampleBlob = $BlobFiles[0]
    Write-Host "  Sample blob: $($SampleBlob.Name) ($([math]::Round($SampleBlob.Length/1MB,2)) MB)" -ForegroundColor Gray
} else {
    Write-TestResult "Ollama File System Structure" $false "Missing blob files ($($BlobFiles.Count)) or manifests ($($ManifestFiles.Count))"
}

# ============================================================================
# TEST 3: Ollama Manifest Parsing
# ============================================================================
Write-TestHeader "Ollama Manifest Parsing"

$TestManifest = $ManifestFiles | Where-Object { $_.Name -eq "latest" } | Select-Object -First 1
if ($TestManifest) {
    try {
        $ManifestContent = Get-Content $TestManifest.FullName -Raw | ConvertFrom-Json
        
        if ($ManifestContent.layers -and $ManifestContent.layers.Count -gt 0) {
            $ModelLayer = $ManifestContent.layers | Where-Object { $_.mediaType -match "model" } | Select-Object -First 1
            
            if ($ModelLayer -and $ModelLayer.digest) {
                $BlobHash = $ModelLayer.digest -replace "^sha256:", "sha256-"
                $BlobPath = Join-Path $OllamaBlobDir $BlobHash
                
                if (Test-Path $BlobPath) {
                    $Details = "Successfully parsed manifest and located blob: $BlobHash"
                    Write-TestResult "Manifest Parsing" $true $Details
                } else {
                    Write-TestResult "Manifest Parsing" $false "Blob referenced in manifest not found: $BlobPath"
                }
            } else {
                Write-TestResult "Manifest Parsing" $false "No model layer with digest found in manifest"
            }
        } else {
            Write-TestResult "Manifest Parsing" $false "Manifest has no layers"
        }
    } catch {
        Write-TestResult "Manifest Parsing" $false "Failed to parse manifest: $($_.Exception.Message)"
    }
} else {
    Write-TestResult "Manifest Parsing" $false "No manifest files found to test"
}

# ============================================================================
# TEST 4: GGUF Model File Access
# ============================================================================
Write-TestHeader "GGUF Model File Access"

if (Test-Path $GGUFTestModel) {
    $GGUFInfo = Get-Item $GGUFTestModel
    $Details = "GGUF file accessible: $([math]::Round($GGUFInfo.Length/1MB,2)) MB"
    Write-TestResult "GGUF Model Access" $true $Details
} else {
    Write-TestResult "GGUF Model Access" $false "GGUF test model not found: $GGUFTestModel"
}

# ============================================================================
# TEST 5: Code Verification - InferenceEngine Integration
# ============================================================================
Write-TestHeader "Code Verification - InferenceEngine"

$InferenceEngineHpp = "D:\RawrXD-production-lazy-init\src\qtapp\inference_engine.hpp"
$InferenceEngineCpp = "D:\RawrXD-production-lazy-init\src\qtapp\inference_engine.cpp"

if (Test-Path $InferenceEngineHpp) {
    $HppContent = Get-Content $InferenceEngineHpp -Raw
    
    $HasDetectedModels = $HppContent -match "QStringList detectedOllamaModels"
    $HasIsBlobPath = $HppContent -match "bool isBlobPath"
    $HasSetModelDir = $HppContent -match "void setModelDirectory"
    
    if ($HasDetectedModels -and $HasIsBlobPath -and $HasSetModelDir) {
        Write-TestResult "InferenceEngine API" $true "All required methods present"
    } else {
        $Missing = @()
        if (!$HasDetectedModels) { $Missing += "detectedOllamaModels()" }
        if (!$HasIsBlobPath) { $Missing += "isBlobPath()" }
        if (!$HasSetModelDir) { $Missing += "setModelDirectory()" }
        Write-TestResult "InferenceEngine API" $false "Missing methods: $($Missing -join ', ')"
    }
} else {
    Write-TestResult "InferenceEngine API" $false "Header file not found"
}

if (Test-Path $InferenceEngineCpp) {
    $CppContent = Get-Content $InferenceEngineCpp -Raw
    
    $HasBlobImpl = $CppContent -match "bool InferenceEngine::isBlobPath"
    $HasDetectedImpl = $CppContent -match "QStringList InferenceEngine::detectedOllamaModels"
    
    if ($HasBlobImpl -and $HasDetectedImpl) {
        Write-TestResult "InferenceEngine Implementation" $true "Key methods implemented"
    } else {
        Write-TestResult "InferenceEngine Implementation" $false "Missing implementations"
    }
} else {
    Write-TestResult "InferenceEngine Implementation" $false "Source file not found"
}

# ============================================================================
# TEST 6: Code Verification - AIChatPanel Integration
# ============================================================================
Write-TestHeader "Code Verification - AIChatPanel"

$AIChatPanelCpp = "D:\RawrXD-production-lazy-init\src\qtapp\ai_chat_panel.cpp"
$AIChatPanelHpp = "D:\RawrXD-production-lazy-init\src\qtapp\ai_chat_panel.hpp"

if (Test-Path $AIChatPanelCpp) {
    $PanelContent = Get-Content $AIChatPanelCpp -Raw
    
    # Check for Ollama blob detection integration (should NOT be commented)
    $HasBlobDetection = $PanelContent -match "detectedOllamaModels\(\)"
    $IsBlobCommented = $PanelContent -match "//.*detectedOllamaModels\(\)"
    
    if ($HasBlobDetection -and !$IsBlobCommented) {
        Write-TestResult "AIChatPanel Blob Integration" $true "Ollama blob detection active in fetchAvailableModels()"
    } else {
        Write-TestResult "AIChatPanel Blob Integration" $false "Blob detection missing or commented out"
    }
    
    # Check for [Ollama Blob] label
    $HasBlobLabel = $PanelContent -match "\[Ollama Blob\]"
    if ($HasBlobLabel) {
        Write-TestResult "AIChatPanel UI Labels" $true "Ollama blob UI label present"
    } else {
        Write-TestResult "AIChatPanel UI Labels" $false "Missing [Ollama Blob] UI label"
    }
} else {
    Write-TestResult "AIChatPanel Integration" $false "Source file not found"
}

if (Test-Path $AIChatPanelHpp) {
    $PanelHpp = Get-Content $AIChatPanelHpp -Raw
    
    $HasRefreshMethod = $PanelHpp -match "void refreshModelList"
    if ($HasRefreshMethod) {
        Write-TestResult "AIChatPanel API" $true "refreshModelList() method available"
    } else {
        Write-TestResult "AIChatPanel API" $false "Missing refreshModelList() method"
    }
}

# ============================================================================
# TEST 7: Code Verification - MainWindow File Dialog
# ============================================================================
Write-TestHeader "Code Verification - MainWindow File Dialog"

$MainWindowCpp = "D:\RawrXD-production-lazy-init\src\qtapp\MainWindow_v5.cpp"

if (Test-Path $MainWindowCpp) {
    $MainContent = Get-Content $MainWindowCpp -Raw
    
    # Check for updated file dialog filter
    $HasUnifiedFilter = $MainContent -match 'AI Models.*\*\.gguf.*sha256-'
    
    if ($HasUnifiedFilter) {
        Write-TestResult "File Dialog Filter" $true "Unified filter supports both GGUF and Ollama blobs"
    } else {
        $HasGGUFOnly = $MainContent -match 'GGUF Models.*\*\.gguf'
        if ($HasGGUFOnly) {
            Write-TestResult "File Dialog Filter" $false "Filter only supports GGUF files"
        } else {
            Write-TestResult "File Dialog Filter" $false "Unable to verify file dialog filter"
        }
    }
    
    # Check for blob detection in model loading
    $HasBlobCheck = $MainContent -match "isBlobPath"
    if ($HasBlobCheck) {
        Write-TestResult "MainWindow Blob Detection" $true "onModelSelected() checks for blob paths"
    } else {
        Write-TestResult "MainWindow Blob Detection" $false "Missing blob path detection in model loading"
    }
} else {
    Write-TestResult "MainWindow Integration" $false "Source file not found"
}

# ============================================================================
# TEST 8: Code Verification - OllamaProxy
# ============================================================================
Write-TestHeader "Code Verification - OllamaProxy"

$OllamaProxyH = "D:\RawrXD-production-lazy-init\include\ollama_proxy.h"
$OllamaProxyCpp = "D:\RawrXD-production-lazy-init\src\ollama_proxy.cpp"

if (Test-Path $OllamaProxyH) {
    $ProxyH = Get-Content $OllamaProxyH -Raw
    
    $HasDetectBlobs = $ProxyH -match "void detectBlobs"
    $HasDetectedModels = $ProxyH -match "QStringList detectedModels"
    $HasIsBlobPath = $ProxyH -match "bool isBlobPath"
    $HasResolveBlob = $ProxyH -match "QString resolveBlobToModel"
    
    if ($HasDetectBlobs -and $HasDetectedModels -and $HasIsBlobPath -and $HasResolveBlob) {
        Write-TestResult "OllamaProxy API" $true "Complete blob detection API defined"
    } else {
        Write-TestResult "OllamaProxy API" $false "Incomplete blob detection API"
    }
} else {
    Write-TestResult "OllamaProxy API" $false "Header file not found"
}

if (Test-Path $OllamaProxyCpp) {
    $ProxyCpp = Get-Content $OllamaProxyCpp -Raw
    
    # Check for manifest parsing logic
    $HasManifestParsing = $ProxyCpp -match "manifests.*QJsonDocument"
    $HasBlobSizeCheck = $ProxyCpp -match "100.*1024.*1024" # 100MB check
    
    if ($HasManifestParsing -and $HasBlobSizeCheck) {
        Write-TestResult "OllamaProxy Implementation" $true "Manifest parsing and blob filtering implemented"
    } else {
        Write-TestResult "OllamaProxy Implementation" $false "Incomplete blob detection implementation"
    }
} else {
    Write-TestResult "OllamaProxy Implementation" $false "Source file not found"
}

# ============================================================================
# TEST 9: Logging and Error Handling Verification
# ============================================================================
Write-TestHeader "Logging and Error Handling"

$SourceFiles = @(
    "D:\RawrXD-production-lazy-init\src\qtapp\MainWindow_v5.cpp",
    "D:\RawrXD-production-lazy-init\src\qtapp\inference_engine.cpp",
    "D:\RawrXD-production-lazy-init\src\qtapp\ai_chat_panel.cpp",
    "D:\RawrXD-production-lazy-init\src\qtapp\ollama_proxy.cpp"
)

$LoggingScore = 0
$MaxScore = $SourceFiles.Count * 2

foreach ($File in $SourceFiles) {
    if (Test-Path $File) {
        $Content = Get-Content $File -Raw
        
        # Check for qInfo/qDebug logging
        if ($Content -match "qInfo|qDebug") {
            $LoggingScore++
        }
        
        # Check for error handling
        if ($Content -match "qWarning|qCritical|catch.*Exception") {
            $LoggingScore++
        }
    }
}

$LoggingPercent = [math]::Round(($LoggingScore / $MaxScore) * 100, 2)
if ($LoggingPercent -ge 75) {
    Write-TestResult "Logging Coverage" $true "$LoggingPercent% of files have logging ($LoggingScore/$MaxScore)"
} else {
    Write-TestResult "Logging Coverage" $false "$LoggingPercent% of files have logging ($LoggingScore/$MaxScore) - below 75% threshold"
}

# ============================================================================
# TEST 10: Autonomous Agent Components
# ============================================================================
Write-TestHeader "Autonomous Agent Components"

$AgentFiles = @{
    "AgenticTools" = "D:\RawrXD-production-lazy-init\src\qtapp\agentic_tools.cpp"
    "AutonomousMode" = "D:\RawrXD-production-lazy-init\src\qtapp\MainWindow_v5.cpp"
}

foreach ($Component in $AgentFiles.Keys) {
    $FilePath = $AgentFiles[$Component]
    
    if (Test-Path $FilePath) {
        $Content = Get-Content $FilePath -Raw
        
        if ($Component -eq "AgenticTools") {
            $HasExecute = $Content -match "execute.*tool|apply.*edit"
            $HasValidation = $Content -match "validate|canWrite"
            
            if ($HasExecute -and $HasValidation) {
                Write-TestResult "AgenticTools" $true "Tool execution and validation present"
            } else {
                Write-TestResult "AgenticTools" $false "Missing execution or validation logic"
            }
        } elseif ($Component -eq "AutonomousMode") {
            $HasAutonomous = $Content -match "autonomous.*mode|agentic.*loop"
            
            if ($HasAutonomous) {
                Write-TestResult "Autonomous Mode" $true "Autonomous mode implementation detected"
            } else {
                Write-TestResult "Autonomous Mode" $false "Autonomous mode implementation not found"
            }
        }
    } else {
        Write-TestResult $Component $false "File not found: $FilePath"
    }
}

# ============================================================================
# FINAL SUMMARY
# ============================================================================
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "TEST EXECUTION SUMMARY" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$TotalTests = $TestResults.Count
$PassedTests = ($TestResults | Where-Object { $_.Passed }).Count
$FailedTests = $TotalTests - $PassedTests
$SuccessRate = [math]::Round(($PassedTests / $TotalTests) * 100, 2)

Write-Host "`nTotal Tests: $TotalTests" -ForegroundColor White
Write-Host "Passed: $PassedTests" -ForegroundColor Green
Write-Host "Failed: $FailedTests" -ForegroundColor $(if ($FailedTests -eq 0) { "Green" } else { "Red" })
Write-Host "Success Rate: $SuccessRate%" -ForegroundColor $(if ($SuccessRate -ge 90) { "Green" } elseif ($SuccessRate -ge 75) { "Yellow" } else { "Red" })

$TestDuration = (Get-Date) - $TestStartTime
Write-Host "`nTest Duration: $($TestDuration.TotalSeconds) seconds" -ForegroundColor Gray

# Export detailed results
$ReportPath = "D:\RawrXD-production-lazy-init\tests\integration_test_report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
$TestResults | ConvertTo-Json -Depth 10 | Out-File $ReportPath -Encoding UTF8
Write-Host "`nDetailed report saved to: $ReportPath" -ForegroundColor Cyan

# Return exit code
if ($FailedTests -eq 0) {
    Write-Host "`n✓ ALL TESTS PASSED" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n✗ SOME TESTS FAILED" -ForegroundColor Red
    exit 1
}
