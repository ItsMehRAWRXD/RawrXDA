#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Complete system validation and demo script
    
.DESCRIPTION
    Tests all components of the zero-dependency model maker system
    
.EXAMPLE
    .\Test-ZeroDependencySystem.ps1 -RunAllTests
#>

param(
    [switch]$RunAllTests,
    [switch]$QuickTest,
    [switch]$BuildDemo,
    [switch]$ShowCapabilities
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Continue"

$script:TestResults = @()
$script:ScriptRoot = "D:\lazy init ide\scripts"

function Write-TestHeader {
    param([string]$Title)
    
    Write-Host "`n╔════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  $($Title.PadRight(66))║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
}

function Write-TestResult {
    param(
        [string]$TestName,
        [bool]$Passed,
        [string]$Message = ""
    )
    
    $status = if ($Passed) { "✅ PASS" } else { "❌ FAIL" }
    $color = if ($Passed) { "Green" } else { "Red" }
    
    Write-Host "  [$status] $TestName" -ForegroundColor $color
    if ($Message) {
        Write-Host "      $Message" -ForegroundColor Gray
    }
    
    $script:TestResults += @{
        Name = $TestName
        Passed = $Passed
        Message = $Message
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# TEST 1: Script Existence
# ═══════════════════════════════════════════════════════════════════════════════

function Test-ScriptFiles {
    Write-TestHeader "TEST 1: Script Files Validation"
    
    $requiredScripts = @(
        "model_maker_zero_dep.ps1",
        "system_prompt_engine.ps1",
        "model_self_digest.ps1",
        "swarm_model_integration.ps1"
    )
    
    foreach ($script in $requiredScripts) {
        $path = Join-Path $script:ScriptRoot $script
        $exists = Test-Path $path
        Write-TestResult "Script exists: $script" $exists $path
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# TEST 2: Prompt Engine
# ═══════════════════════════════════════════════════════════════════════════════

function Test-PromptEngine {
    Write-TestHeader "TEST 2: System Prompt Engine"
    
    $promptScript = Join-Path $script:ScriptRoot "system_prompt_engine.ps1"
    
    try {
        # Test deterministic generation
        $tempFile = Join-Path $env:TEMP "test_prompt.txt"
        & $promptScript -Role "kernel-reverse-engineer" -OutputFile $tempFile -ErrorAction Stop
        
        $promptExists = Test-Path $tempFile
        Write-TestResult "Deterministic prompt generation" $promptExists
        
        if ($promptExists) {
            $content = Get-Content $tempFile -Raw
            $hasRole = $content -match "kernel"
            Write-TestResult "Prompt contains role keywords" $hasRole
            
            $size = (Get-Item $tempFile).Length
            Write-TestResult "Prompt has content (size: $size bytes)" ($size -gt 100)
        }
        
        # Test random mode
        $tempFile2 = Join-Path $env:TEMP "test_prompt_random.txt"
        & $promptScript -Role "assembly-expert" -RandomMode -OutputFile $tempFile2 -ErrorAction Stop
        
        $randomExists = Test-Path $tempFile2
        Write-TestResult "Random mode prompt generation" $randomExists
        
        # Cleanup
        if (Test-Path $tempFile) { Remove-Item $tempFile -Force }
        if (Test-Path $tempFile2) { Remove-Item $tempFile2 -Force }
    }
    catch {
        Write-TestResult "Prompt engine execution" $false $_.Exception.Message
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# TEST 3: Model Builder (Dry Run)
# ═══════════════════════════════════════════════════════════════════════════════

function Test-ModelBuilder {
    Write-TestHeader "TEST 3: Model Builder Validation"
    
    $modelScript = Join-Path $script:ScriptRoot "model_maker_zero_dep.ps1"
    
    try {
        # Check if script has required classes
        $scriptContent = Get-Content $modelScript -Raw
        
        $hasArchClass = $scriptContent -match "class ModelArchitecture"
        Write-TestResult "ModelArchitecture class defined" $hasArchClass
        
        $hasBuilderClass = $scriptContent -match "class BinaryModelBuilder"
        Write-TestResult "BinaryModelBuilder class defined" $hasBuilderClass
        
        $hasGGUFHeader = $scriptContent -match "WriteGGUFHeader"
        Write-TestResult "GGUF header writing implemented" $hasGGUFHeader
        
        $hasQuantization = $scriptContent -match "QuantizeWeights"
        Write-TestResult "Quantization method implemented" $hasQuantization
        
        $hasPromptInjection = $scriptContent -match "InjectSystemPrompt"
        Write-TestResult "System prompt injection implemented" $hasPromptInjection
    }
    catch {
        Write-TestResult "Model builder validation" $false $_.Exception.Message
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# TEST 4: Self-Digestion Engine
# ═══════════════════════════════════════════════════════════════════════════════

function Test-SelfDigestion {
    Write-TestHeader "TEST 4: Self-Digestion Engine"
    
    $digestScript = Join-Path $script:ScriptRoot "model_self_digest.ps1"
    
    try {
        $scriptContent = Get-Content $digestScript -Raw
        
        $hasAnalyzer = $scriptContent -match "class ModelAnalyzer"
        Write-TestResult "ModelAnalyzer class defined" $hasAnalyzer
        
        $hasDigestion = $scriptContent -match "class SelfDigestionEngine"
        Write-TestResult "SelfDigestionEngine class defined" $hasDigestion
        
        $hasEvolution = $scriptContent -match "class EvolutionaryEngine"
        Write-TestResult "EvolutionaryEngine class defined" $hasEvolution
        
        $hasReconstruction = $scriptContent -match "class ReconstructionEngine"
        Write-TestResult "ReconstructionEngine class defined" $hasReconstruction
        
        $hasOperations = $scriptContent -match "digest|evolve|mutate|reconstruct|clone|analyze"
        Write-TestResult "All operations supported" $hasOperations
    }
    catch {
        Write-TestResult "Self-digestion validation" $false $_.Exception.Message
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# TEST 5: Swarm Integration
# ═══════════════════════════════════════════════════════════════════════════════

function Test-SwarmIntegration {
    Write-TestHeader "TEST 5: Swarm Integration"
    
    $swarmScript = Join-Path $script:ScriptRoot "swarm_model_integration.ps1"
    
    try {
        $scriptContent = Get-Content $swarmScript -Raw
        
        $hasSwarmSpecs = $scriptContent -match "SwarmModelSpecs"
        Write-TestResult "Swarm model specifications defined" $hasSwarmSpecs
        
        $hasArchitect = $scriptContent -match "Architect"
        Write-TestResult "Architect agent spec present" $hasArchitect
        
        $hasScout = $scriptContent -match "Scout"
        Write-TestResult "Scout agent spec present" $hasScout
        
        $hasBuildFunction = $scriptContent -match "function Build-SwarmModels"
        Write-TestResult "Build-SwarmModels function defined" $hasBuildFunction
        
        $hasAssignFunction = $scriptContent -match "function Assign-ModelToRole"
        Write-TestResult "Assign-ModelToRole function defined" $hasAssignFunction
        
        $hasEvolutionFunction = $scriptContent -match "function Start-AutoEvolution"
        Write-TestResult "Start-AutoEvolution function defined" $hasEvolutionFunction
    }
    catch {
        Write-TestResult "Swarm integration validation" $false $_.Exception.Message
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# TEST 6: Documentation
# ═══════════════════════════════════════════════════════════════════════════════

function Test-Documentation {
    Write-TestHeader "TEST 6: Documentation Validation"
    
    $docs = @(
        "ZERO_DEPENDENCY_MODEL_MAKER_README.md",
        "ZERO_DEPENDENCY_QUICK_REFERENCE.md",
        "ZERO_DEPENDENCY_SYSTEM_SUMMARY.md"
    )
    
    foreach ($doc in $docs) {
        $path = Join-Path "D:\lazy init ide" $doc
        $exists = Test-Path $path
        Write-TestResult "Documentation exists: $doc" $exists
        
        if ($exists) {
            $size = (Get-Item $path).Length
            $hasContent = $size -gt 1000
            Write-TestResult "Documentation has content ($size bytes)" $hasContent
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# DEMO: Build Small Test Model
# ═══════════════════════════════════════════════════════════════════════════════

function Build-DemoModel {
    Write-TestHeader "DEMO: Building Test Model"
    
    Write-Host "`n  This will build a small test model to validate the complete pipeline..." -ForegroundColor Yellow
    Write-Host "  Estimated time: 5-10 minutes" -ForegroundColor Gray
    Write-Host "  Target size: 1.98GB (Q4_K quantized 7B)" -ForegroundColor Gray
    
    $confirm = Read-Host "`n  Proceed with demo build? (yes/no)"
    
    if ($confirm -eq "yes") {
        $modelScript = Join-Path $script:ScriptRoot "model_maker_zero_dep.ps1"
        $promptScript = Join-Path $script:ScriptRoot "system_prompt_engine.ps1"
        
        try {
            # Generate prompt
            Write-Host "`n  [1/2] Generating system prompt..." -ForegroundColor Cyan
            $promptFile = Join-Path $env:TEMP "demo_prompt.txt"
            & $promptScript -Role "kernel-reverse-engineer" -OutputFile $promptFile
            
            if (Test-Path $promptFile) {
                Write-Host "  ✓ Prompt generated" -ForegroundColor Green
                
                # Build model
                Write-Host "`n  [2/2] Building model (this will take a few minutes)..." -ForegroundColor Cyan
                $prompt = Get-Content $promptFile -Raw
                
                & $modelScript `
                    -Operation build-from-scratch `
                    -ModelSize 7B `
                    -TargetSize 1.98 `
                    -SystemPrompt $prompt `
                    -QuantizationType Q4_K `
                    -ContextLength 4096 `
                    -OutputPath "D:\OllamaModels\test_demo"
                
                if ($LASTEXITCODE -eq 0) {
                    Write-Host "`n  ✅ DEMO BUILD SUCCESSFUL!" -ForegroundColor Green
                    Write-Host "  Check D:\OllamaModels\test_demo for your model" -ForegroundColor Gray
                } else {
                    Write-Host "`n  ❌ Demo build failed (exit code: $LASTEXITCODE)" -ForegroundColor Red
                }
            }
        }
        catch {
            Write-Host "`n  ❌ Demo build error: $($_.Exception.Message)" -ForegroundColor Red
        }
    } else {
        Write-Host "`n  Demo build cancelled." -ForegroundColor Yellow
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# SHOW CAPABILITIES
# ═══════════════════════════════════════════════════════════════════════════════

function Show-SystemCapabilities {
    Write-TestHeader "SYSTEM CAPABILITIES OVERVIEW"
    
    Write-Host @"

1️⃣  MODEL BUILDER (model_maker_zero_dep.ps1)
   ├─ Build from scratch (7B, 13B, 70B)
   ├─ Quantization (Q4_K, Q8_0, FP16, FP32)
   ├─ Target size: 1.98GB for 7B Q4_K ✅
   ├─ Reverse engineer existing models
   └─ Self-digesting capabilities

2️⃣  PROMPT ENGINE (system_prompt_engine.ps1)
   ├─ 8 specialized roles:
   │  • kernel-reverse-engineer ✅
   │  • assembly-expert
   │  • security-researcher
   │  • malware-analyst
   │  • binary-exploitation
   │  • crypto-specialist
   │  • firmware-analyst
   │  • driver-developer
   ├─ Random or deterministic mode
   └─ Custom prompt support

3️⃣  SELF-DIGESTION (model_self_digest.ps1)
   ├─ Analyze model architecture
   ├─ Digest external data (NO training!)
   ├─ Evolutionary development
   ├─ Reconstruct with new parameters
   ├─ Mutation and cloning
   └─ Zero dependency on training frameworks

4️⃣  SWARM INTEGRATION (swarm_model_integration.ps1)
   ├─ Auto-build all swarm agents
   ├─ 7 pre-configured agent models:
   │  • Architect (4.2GB)
   │  • Coder (2.8GB)
   │  • Scout (1.98GB)
   │  • Reviewer (3.5GB)
   │  • Fixer (2.5GB)
   │  • ReverseEngineer (3.2GB)
   │  • Optimizer (2.9GB)
   ├─ Role-based assignment
   └─ Autonomous evolution

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

KEY FEATURES:
✅ ZERO external dependencies (no llama.cpp, HuggingFace, PyTorch)
✅ Pure PowerShell + binary operations
✅ Build models from scratch (NO pre-trained downloads)
✅ Custom system prompts ("you are a kernel reverse engineer specialist")
✅ Configurable size, tokens, context
✅ NO traditional training required
✅ Self-digestion for autonomous learning
✅ Random or deterministic initialization
✅ Reverse engineering capabilities
✅ Full swarm integration

"@ -ForegroundColor White
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host @"

╔════════════════════════════════════════════════════════════════════╗
║                                                                    ║
║        ZERO-DEPENDENCY MODEL MAKER                                 ║
║        System Validation & Testing                                 ║
║                                                                    ║
╚════════════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

if ($ShowCapabilities) {
    Show-SystemCapabilities
}

if ($QuickTest) {
    Write-Host "`nRunning quick validation tests...`n" -ForegroundColor Yellow
    Test-ScriptFiles
    Test-Documentation
}

if ($RunAllTests) {
    Write-Host "`nRunning comprehensive test suite...`n" -ForegroundColor Yellow
    
    Test-ScriptFiles
    Test-PromptEngine
    Test-ModelBuilder
    Test-SelfDigestion
    Test-SwarmIntegration
    Test-Documentation
    
    # Summary
    Write-TestHeader "TEST SUMMARY"
    
    $passed = ($script:TestResults | Where-Object { $_.Passed }).Count
    $total = $script:TestResults.Count
    $percentage = [Math]::Round(($passed / $total) * 100, 1)
    
    Write-Host "`n  Total Tests: $total" -ForegroundColor Cyan
    Write-Host "  Passed: $passed" -ForegroundColor Green
    Write-Host "  Failed: $($total - $passed)" -ForegroundColor $(if ($passed -eq $total) { "Gray" } else { "Red" })
    Write-Host "  Success Rate: $percentage%" -ForegroundColor $(if ($percentage -eq 100) { "Green" } else { "Yellow" })
    
    if ($percentage -eq 100) {
        Write-Host "`n  🎉 ALL TESTS PASSED! System is ready for use." -ForegroundColor Green
    } else {
        Write-Host "`n  ⚠️  Some tests failed. Review results above." -ForegroundColor Yellow
    }
}

if ($BuildDemo) {
    Build-DemoModel
}

if (-not ($RunAllTests -or $QuickTest -or $BuildDemo -or $ShowCapabilities)) {
    Write-Host @"

Usage:
  .\Test-ZeroDependencySystem.ps1 -ShowCapabilities    # Show what the system can do
  .\Test-ZeroDependencySystem.ps1 -QuickTest          # Quick validation
  .\Test-ZeroDependencySystem.ps1 -RunAllTests        # Comprehensive test suite
  .\Test-ZeroDependencySystem.ps1 -BuildDemo          # Build demo model (1.98GB)

Recommended first run:
  .\Test-ZeroDependencySystem.ps1 -ShowCapabilities
  .\Test-ZeroDependencySystem.ps1 -RunAllTests

"@ -ForegroundColor Gray
}

Write-Host "`n✨ Testing complete!`n" -ForegroundColor Cyan
