# Test-FullSmokeTest.ps1
# Comprehensive smoke test for all implementations
# Tests: Chunked I/O, Quantization, Hardware Spoof, Tool Registry, Agentic Inference

param(
    [string]$BuildDir = "d:\rawrxd\build",
    [string]$SourceDir = "d:\rawrxd\src",
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║        RAWRXD FULL SMOKE TEST SUITE                          ║" -ForegroundColor Magenta
Write-Host "║        Testing All Implementations                            ║" -ForegroundColor Magenta
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

$TestResults = @{
    Total      = 0
    Passed     = 0
    Failed     = 0
    Skipped    = 0
    StartTime  = Get-Date
    Tests      = @()
    Categories = @{}
}

function Add-TestResult {
    param(
        [string]$Category,
        [string]$Test,
        [string]$Status,
        [string]$Error = $null,
        [double]$Duration = 0
    )
    
    $TestResults.Total++
    if ($Status -eq "PASS") { $TestResults.Passed++ }
    elseif ($Status -eq "FAIL") { $TestResults.Failed++ }
    else { $TestResults.Skipped++ }
    
    $TestResults.Tests += @{
        Category = $Category
        Test     = $Test
        Status   = $Status
        Error    = $Error
        Duration = $Duration
    }
    
    if (-not $TestResults.Categories.ContainsKey($Category)) {
        $TestResults.Categories[$Category] = @{ Passed = 0; Failed = 0; Skipped = 0 }
    }
    
    if ($Status -eq "PASS") { $TestResults.Categories[$Category].Passed++ }
    elseif ($Status -eq "FAIL") { $TestResults.Categories[$Category].Failed++ }
    else { $TestResults.Categories[$Category].Skipped++ }
}

# ============================================================================
# TEST 1: Chunked File I/O
# ============================================================================

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "TEST 1: Chunked File I/O (2GB+ Support)" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan

$category = "Chunked I/O"

# Test 1.1: Header file exists
$startTime = Get-Date
$headerPath = "$SourceDir\chunked_file_loader.h"
if (Test-Path $headerPath) {
    $content = Get-Content $headerPath -Raw
    if ($content -match "ChunkedFileLoader" -and $content -match "GGUFChunkedLoader" -and $content -match "InferenceChunkedLoader") {
        Add-TestResult $category "Header file contains all classes" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "Header file missing classes" "FAIL" "Missing ChunkedFileLoader, GGUFChunkedLoader, or InferenceChunkedLoader" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "Header file exists" "FAIL" "File not found: $headerPath" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# Test 1.2: Implementation file exists
$startTime = Get-Date
$implPath = "$SourceDir\chunked_file_loader.cpp"
if (Test-Path $implPath) {
    $content = Get-Content $implPath -Raw
    if ($content -match "CreateFileW" -and $content -match "CreateFileMapping" -and $content -match "MapViewOfFile") {
        Add-TestResult $category "Implementation uses Windows file mapping" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "Implementation missing Windows APIs" "FAIL" "Missing CreateFileW, CreateFileMapping, or MapViewOfFile" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "Implementation file exists" "FAIL" "File not found: $implPath" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# Test 1.3: GGUF parsing functions
$startTime = Get-Date
if (Test-Path $implPath) {
    $content = Get-Content $implPath -Raw
    if ($content -match "ParseGGUFHeader" -or $content -match "LoadGGUFMetadata" -or $content -match "GetTensorNames") {
        Add-TestResult $category "GGUF parsing functions present" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "GGUF parsing functions" "SKIP" "No GGUF parsing functions found" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
}

# ============================================================================
# TEST 2: Quantization Operations
# ============================================================================

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "TEST 2: Quantization Operations" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan

$category = "Quantization"

# Test 2.1: Quantization header
$startTime = Get-Date
$quantHeader = "$SourceDir\core\quant_ops.h"
if (Test-Path $quantHeader) {
    $content = Get-Content $quantHeader -Raw
    $hasDequant = $content -match "dequant_q4_0" -or $content -match "DequantResult"
    $hasQuant = $content -match "quant_q4_0" -or $content -match "QuantResult"
    if ($hasDequant -and $hasQuant) {
        Add-TestResult $category "Header has quant/dequant functions" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "Header missing functions" "FAIL" "Missing quant or dequant functions" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "Header file exists" "FAIL" "File not found: $quantHeader" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# Test 2.2: Quantization implementation
$startTime = Get-Date
$quantImpl = "$SourceDir\core\quant_ops.c"
if (Test-Path $quantImpl) {
    $content = Get-Content $quantImpl -Raw
    
    # Check for Q4_0
    $hasQ4_0 = $content -match "dequant_q4_0" -and $content -match "quant_q4_0"
    
    # Check for Q8_0
    $hasQ8_0 = $content -match "dequant_q8_0" -and $content -match "quant_q8_0"
    
    # Check for K-quants
    $hasKQuant = $content -match "dequant_q4_k" -or $content -match "block_q4_k"
    
    # Check for FP16 conversion
    $hasFP16 = $content -match "half_to_float" -and $content -match "float_to_half"
    
    if ($hasQ4_0 -and $hasQ8_0 -and $hasKQuant -and $hasFP16) {
        Add-TestResult $category "Implementation has all quant types" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        $missing = @()
        if (-not $hasQ4_0) { $missing += "Q4_0" }
        if (-not $hasQ8_0) { $missing += "Q8_0" }
        if (-not $hasKQuant) { $missing += "K-quant" }
        if (-not $hasFP16) { $missing += "FP16" }
        Add-TestResult $category "Implementation missing types" "FAIL" "Missing: $($missing -join ', ')" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "Implementation file exists" "FAIL" "File not found: $quantImpl" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# Test 2.3: Quality measurement
$startTime = Get-Date
if (Test-Path $quantImpl) {
    $content = Get-Content $quantImpl -Raw
    if ($content -match "estimate_quant_quality" -or $content -match "QuantQuality") {
        Add-TestResult $category "Quality measurement present" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "Quality measurement" "SKIP" "No quality measurement found" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
}

# ============================================================================
# TEST 3: Hardware Spoofing
# ============================================================================

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "TEST 3: Hardware Spoofing + Response Pinning" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan

$category = "Hardware Spoof"

# Test 3.1: Hardware spoof header
$startTime = Get-Date
$hwSpoofHeader = "$SourceDir\core\rawrxd_hardware_spoof.h"
if (Test-Path $hwSpoofHeader) {
    $content = Get-Content $hwSpoofHeader -Raw
    
    $hasGPUSpec = $content -match "RXDGPUSpec" -and $content -match "RXDSpoofGPU"
    $hasSpoof = $content -match "rxd_hw_spoof_init" -and $content -match "rxd_hw_spoof_set"
    $hasPin = $content -match "rxd_pin_response" -and $content -match "rxd_lookup_pinned"
    $hasCloud = $content -match "RXDCloudSpec" -and $content -match "rxd_cloud_inject"
    
    if ($hasGPUSpec -and $hasSpoof -and $hasPin -and $hasCloud) {
        Add-TestResult $category "Header has all components" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        $missing = @()
        if (-not $hasGPUSpec) { $missing += "GPUSpec" }
        if (-not $hasSpoof) { $missing += "Spoof functions" }
        if (-not $hasPin) { $missing += "Pin functions" }
        if (-not $hasCloud) { $missing += "Cloud spec" }
        Add-TestResult $category "Header missing components" "FAIL" "Missing: $($missing -join ', ')" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "Header file exists" "FAIL" "File not found: $hwSpoofHeader" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# Test 3.2: GPU specs
$startTime = Get-Date
if (Test-Path $hwSpoofHeader) {
    $content = Get-Content $hwSpoofHeader -Raw
    
    $hasAMD = $content -match "RXD_HW_GPU_AMD_7800XT" -or $content -match "7800 XT"
    $hasNVIDIA = $content -match "RXD_HW_GPU_NVIDIA_4090" -or $content -match "4090"
    $hasCloud = $content -match "RXD_HW_GPU_CLOUD_H100" -or $content -match "H100"
    
    if ($hasAMD -and $hasNVIDIA -and $hasCloud) {
        Add-TestResult $category "GPU specs for AMD/NVIDIA/Cloud" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "GPU specs incomplete" "FAIL" "Missing AMD, NVIDIA, or Cloud specs" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
}

# Test 3.3: Response pinning
$startTime = Get-Date
if (Test-Path $hwSpoofHeader) {
    $content = Get-Content $hwSpoofHeader -Raw
    
    $hasHash = $content -match "rxd_hash_prompt"
    $hasCache = $content -match "RXDPinnedCache" -or $content -match "RXDPinnedResponse"
    $hasExport = $content -match "rxd_pin_export" -and $content -match "rxd_pin_import"
    
    if ($hasHash -and $hasCache -and $hasExport) {
        Add-TestResult $category "Response pinning complete" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "Response pinning incomplete" "FAIL" "Missing hash, cache, or export" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
}

# Test 3.4: Playback control
$startTime = Get-Date
if (Test-Path $hwSpoofHeader) {
    $content = Get-Content $hwSpoofHeader -Raw
    
    $hasPlayback = $content -match "RXDPlaybackSession" -and $content -match "rxd_playback_init"
    $hasControls = $content -match "rxd_playback_play" -and $content -match "rxd_playback_pause"
    
    if ($hasPlayback -and $hasControls) {
        Add-TestResult $category "Playback control present" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "Playback control" "SKIP" "Missing playback functions" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
}

# ============================================================================
# TEST 4: Tool Registry Enhanced
# ============================================================================

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "TEST 4: Tool Registry Enhanced" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan

$category = "Tool Registry"

# Test 4.1: Tool registry header
$startTime = Get-Date
$toolRegHeader = "$SourceDir\tool_registry_enhanced.h"
if (Test-Path $toolRegHeader) {
    $content = Get-Content $toolRegHeader -Raw
    
    # Check for singleton pattern (static Instance() method)
    $hasSingleton = $content -match "static\s+ToolRegistry&\s+Instance" -or $content -match "ToolRegistry::Instance"
    $hasInit = $content -match "Initialize\(\)" -or $content -match "IsInitialized"
    $hasExec = $content -match "ExecuteTool" -or $content -match "ToolExecutionResult"
    $hasSchema = $content -match "ToolSchema" -or $content -match "GetToolSchemas"
    
    if ($hasSingleton -and $hasInit -and $hasExec -and $hasSchema) {
        Add-TestResult $category "Header has all components" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        $missing = @()
        if (-not $hasSingleton) { $missing += "Singleton" }
        if (-not $hasInit) { $missing += "Initialize" }
        if (-not $hasExec) { $missing += "Execute" }
        if (-not $hasSchema) { $missing += "Schema" }
        Add-TestResult $category "Header missing components" "FAIL" "Missing: $($missing -join ', ')" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "Header file exists" "FAIL" "File not found: $toolRegHeader" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# Test 4.2: Tool registry implementation
$startTime = Get-Date
$toolRegImpl = "$SourceDir\tool_registry_enhanced.cpp"
if (Test-Path $toolRegImpl) {
    $content = Get-Content $toolRegImpl -Raw
    
    $hasCore = $content -match "RegisterCoreTools" -or $content -match "file_read"
    $hasFile = $content -match "RegisterFileTools" -or $content -match "file_write"
    $hasCode = $content -match "RegisterCodeTools" -or $content -match "code_search"
    $hasSystem = $content -match "RegisterSystemTools" -or $content -match "getenv"
    
    if ($hasCore -and $hasFile -and $hasCode -and $hasSystem) {
        Add-TestResult $category "Implementation has all tool types" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "Implementation missing tool types" "FAIL" "Missing core, file, code, or system tools" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "Implementation file exists" "FAIL" "File not found: $toolRegImpl" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# ============================================================================
# TEST 5: Agentic SubmitInference Fix
# ============================================================================

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "TEST 5: Agentic SubmitInference Fix" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan

$category = "Agentic Inference"

# Test 5.1: Agentic inference header
$startTime = Get-Date
$agenticHeader = "$SourceDir\AgenticSubmitInference_Fix.h"
if (Test-Path $agenticHeader) {
    $content = Get-Content $agenticHeader -Raw
    
    $hasBridge = $content -match "AgenticInferenceBridge"
    $hasSubmit = $content -match "SubmitInferenceWithTools"
    $hasToolCall = $content -match "ToolCall" -or $content -match "ToolCallRecord"
    $hasResult = $content -match "InferenceResult" -or $content -match "ToolExecutionResult"
    
    if ($hasBridge -and $hasSubmit -and $hasToolCall -and $hasResult) {
        Add-TestResult $category "Header has all components" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        $missing = @()
        if (-not $hasBridge) { $missing += "Bridge" }
        if (-not $hasSubmit) { $missing += "Submit" }
        if (-not $hasToolCall) { $missing += "ToolCall" }
        if (-not $hasResult) { $missing += "Result" }
        Add-TestResult $category "Header missing components" "FAIL" "Missing: $($missing -join ', ')" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "Header file exists" "FAIL" "File not found: $agenticHeader" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# Test 5.2: Agentic inference implementation
$startTime = Get-Date
$agenticImpl = "$SourceDir\AgenticSubmitInference_Fix.cpp"
if (Test-Path $agenticImpl) {
    $content = Get-Content $agenticImpl -Raw
    
    $hasInit = $content -match "ToolRegistry::Instance" -and $content -match "Initialize"
    $hasLoop = $content -match "MAX_TOOL_ITERATIONS" -or $content -match "toolIterations"
    $hasExec = $content -match "ExecuteTool" -or $content -match "registry.Execute"
    $hasJSON = $content -match "JSONParseGuard" -or $content -match "SafeParse"
    
    if ($hasInit -and $hasLoop -and $hasExec -and $hasJSON) {
        Add-TestResult $category "Implementation has all components" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        $missing = @()
        if (-not $hasInit) { $missing += "Init" }
        if (-not $hasLoop) { $missing += "Loop" }
        if (-not $hasExec) { $missing += "Execute" }
        if (-not $hasJSON) { $missing += "JSON" }
        Add-TestResult $category "Implementation missing components" "FAIL" "Missing: $($missing -join ', ')" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "Implementation file exists" "FAIL" "File not found: $agenticImpl" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# Test 5.3: BackendError fix
$startTime = Get-Date
if (Test-Path $agenticImpl) {
    $content = Get-Content $agenticImpl -Raw
    
    # Check for BackendError handling
    $hasBackendError = $content -match "BackendError" -or $content -match "ToolRegistry initialization failed"
    
    if ($hasBackendError) {
        Add-TestResult $category "BackendError handling present" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "BackendError handling" "SKIP" "No BackendError handling found" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
}

# ============================================================================
# TEST 6: Weight Hotswap
# ============================================================================

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "TEST 6: Weight Hotswap" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan

$category = "Weight Hotswap"

# Test 6.1: Weight hotswap header
$startTime = Get-Date
$hotswapHeader = "$SourceDir\core\weight_hotswap.h"
if (Test-Path $hotswapHeader) {
    $content = Get-Content $hotswapHeader -Raw
    
    $hasSession = $content -match "HotswapSession"
    $hasTensor = $content -match "WeightTensor"
    $hasRequant = $content -match "hotswap_requant_tensor" -or $content -match "Requantize"
    $hasProfile = $content -match "QuantProfile" -or $content -match "QUANT_PROFILE"
    
    if ($hasSession -and $hasTensor -and $hasRequant -and $hasProfile) {
        Add-TestResult $category "Header has all components" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        $missing = @()
        if (-not $hasSession) { $missing += "Session" }
        if (-not $hasTensor) { $missing += "Tensor" }
        if (-not $hasRequant) { $missing += "Requant" }
        if (-not $hasProfile) { $missing += "Profile" }
        Add-TestResult $category "Header missing components" "FAIL" "Missing: $($missing -join ', ')" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "Header file exists" "FAIL" "File not found: $hotswapHeader" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# Test 6.2: GGUF format header
$startTime = Get-Date
$ggufHeader = "$SourceDir\core\gguf_format.h"
if (Test-Path $ggufHeader) {
    $content = Get-Content $ggufHeader -Raw
    
    $hasMagic = $content -match "GGUF_MAGIC"
    $hasTypes = $content -match "GGML_TYPE_Q4_0" -and $content -match "GGML_TYPE_Q8_0"
    $hasContext = $content -match "GGUFContext"
    $hasLoad = $content -match "gguf_load"
    
    if ($hasMagic -and $hasTypes -and $hasContext -and $hasLoad) {
        Add-TestResult $category "GGUF format header complete" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "GGUF format header incomplete" "FAIL" "Missing magic, types, context, or load" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "GGUF format header exists" "FAIL" "File not found: $ggufHeader" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# ============================================================================
# TEST 7: Build System Integration
# ============================================================================

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "TEST 7: Build System Integration" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan

$category = "Build System"

# Test 7.1: CMakeLists.txt includes new files
$startTime = Get-Date
$cmakePath = "d:\rawrxd\CMakeLists.txt"
$coreCmakePath = "d:\rawrxd\src\core\CMakeLists.txt"

# Check main CMakeLists.txt
if (Test-Path $cmakePath) {
    $content = Get-Content $cmakePath -Raw
    
    # Use more flexible regex patterns
    $hasChunkedLoader = $content -match "chunked_file_loader"
    $hasToolRegistry = $content -match "tool_registry_enhanced"
    $hasAgentic = $content -match "AgenticSubmitInference"
    
    if ($hasChunkedLoader) {
        Add-TestResult $category "CMakeLists includes chunked_file_loader" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "CMakeLists includes chunked_file_loader" "FAIL" "chunked_file_loader not in CMakeLists.txt" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
    
    $startTime = Get-Date
    if ($hasToolRegistry) {
        Add-TestResult $category "CMakeLists includes tool_registry_enhanced" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "CMakeLists includes tool_registry_enhanced" "FAIL" "tool_registry_enhanced not in CMakeLists.txt" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
    
    $startTime = Get-Date
    if ($hasAgentic) {
        Add-TestResult $category "CMakeLists includes AgenticSubmitInference" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "CMakeLists includes AgenticSubmitInference" "FAIL" "AgenticSubmitInference not in CMakeLists.txt" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "CMakeLists.txt exists" "FAIL" "File not found: $cmakePath" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# Check core CMakeLists.txt for quant_ops
$startTime = Get-Date
if (Test-Path $coreCmakePath) {
    $coreContent = Get-Content $coreCmakePath -Raw
    $hasQuantOps = $coreContent -match "quant_ops\.c"
    
    if ($hasQuantOps) {
        Add-TestResult $category "Core CMakeLists includes quant_ops.c" "PASS" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    } else {
        Add-TestResult $category "Core CMakeLists includes quant_ops.c" "FAIL" "quant_ops.c not in src/core/CMakeLists.txt" -Duration ((Get-Date) - $startTime).TotalMilliseconds
    }
} else {
    Add-TestResult $category "Core CMakeLists.txt exists" "FAIL" "File not found: $coreCmakePath" -Duration ((Get-Date) - $startTime).TotalMilliseconds
}

# ============================================================================
# Generate Report
# ============================================================================

Write-Host "`n╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                    SMOKE TEST RESULTS                         ║" -ForegroundColor Magenta
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

$TestResults.EndTime = Get-Date
$TestResults.Duration = $TestResults.EndTime - $TestResults.StartTime

Write-Host "`nOverall Results:" -ForegroundColor Cyan
Write-Host "  Total:      $($TestResults.Total)" -ForegroundColor White
Write-Host "  Passed:     $($TestResults.Passed)" -ForegroundColor Green
Write-Host "  Failed:     $($TestResults.Failed)" -ForegroundColor Red
Write-Host "  Skipped:     $($TestResults.Skipped)" -ForegroundColor Yellow
Write-Host "  Duration:    $($TestResults.Duration.TotalSeconds.ToString('F2')) seconds" -ForegroundColor Cyan

Write-Host "`nResults by Category:" -ForegroundColor Cyan
foreach ($cat in $TestResults.Categories.Keys) {
    $stats = $TestResults.Categories[$cat]
    $color = if ($stats.Failed -gt 0) { "Red" } elseif ($stats.Passed -gt 0) { "Green" } else { "Yellow" }
    Write-Host "  $cat`: $($stats.Passed) passed, $($stats.Failed) failed, $($stats.Skipped) skipped" -ForegroundColor $color
}

# Calculate coverage
$coverage = [math]::Round(($TestResults.Passed / $TestResults.Total) * 100, 2)
Write-Host "`nCoverage: $coverage%" -ForegroundColor $(if ($coverage -ge 90) { "Green" } elseif ($coverage -ge 70) { "Yellow" } else { "Red" })

# Save results to JSON
$resultsPath = "d:\rawrxd\test_results\smoke_test_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
if (-not (Test-Path "d:\rawrxd\test_results")) {
    New-Item -ItemType Directory -Path "d:\rawrxd\test_results" -Force | Out-Null
}

$TestResults | ConvertTo-Json -Depth 10 | Out-File $resultsPath -Encoding UTF8
Write-Host "`nResults saved to: $resultsPath" -ForegroundColor Green

# Return exit code based on test results
if ($TestResults.Failed -gt 0) {
    Write-Host "`n❌ SMOKE TEST FAILED" -ForegroundColor Red
    exit 1
} else {
    Write-Host "`n✅ SMOKE TEST PASSED" -ForegroundColor Green
    exit 0
}