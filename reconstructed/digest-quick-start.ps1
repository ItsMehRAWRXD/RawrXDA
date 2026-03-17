#!/usr/bin/env powershell
<#
.SYNOPSIS
    Model Digestion Quick Start - Prepare 800B model for RawrXD IDE

.DESCRIPTION
    This script automates the entire model digestion pipeline:
    1. Validates input model
    2. Runs TypeScript digestion engine
    3. Compiles MASM x64 stub
    4. Generates integration package
    5. Deploys to IDE

.PARAMETER ModelPath
    Path to input model (GGUF format)

.PARAMETER OutputDir
    Output directory for digested model

.PARAMETER ModelName
    Friendly name for model

.EXAMPLE
    .\digest-quick-start.ps1 -ModelPath "d:\OllamaModels\llama2-800b.gguf" `
                            -OutputDir "d:\digested-models\llama2" `
                            -ModelName "Llama 2 800B"
#>

param(
    [string]$ModelPath = "d:\OllamaModels\llama2-800b.gguf",
    [string]$OutputDir = "d:\digested-models\llama2-800b",
    [string]$ModelName = "Llama 2 800B"
)

# Configuration
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$MASM_PATH = "ModelDigestion_x64.asm"
$ENGINE_PATH = "model-digestion-engine.ts"
$ML64_PATH = "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\ml64.exe"
$LIB_PATH = "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\lib.exe"

# Color output
function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Error { Write-Host $args -ForegroundColor Red }
function Write-Info { Write-Host $args -ForegroundColor Cyan }
function Write-Warning { Write-Host $args -ForegroundColor Yellow }

# ============================================================================
# PHASE 1: VALIDATION
# ============================================================================

function Validate-Environment {
    Write-Info "=== PHASE 1: VALIDATION ==="
    
    # Check input model exists
    if (!(Test-Path $ModelPath)) {
        Write-Error "❌ Model not found: $ModelPath"
        exit 1
    }
    Write-Success "✅ Model found: $ModelPath"
    
    # Check model size
    $modelSize = (Get-Item $ModelPath).Length / 1GB
    Write-Info "📊 Model size: $([math]::Round($modelSize, 2)) GB"
    
    if ($modelSize -lt 0.5) {
        Write-Warning "⚠️  Model seems small for 800B"
    }
    
    # Check output directory
    if (Test-Path $OutputDir) {
        Write-Warning "⚠️  Output directory exists, will overwrite"
    }
    else {
        New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
        Write-Success "✅ Created output directory: $OutputDir"
    }
    
    # Check ML64
    if (!(Test-Path $ML64_PATH)) {
        Write-Error "❌ ml64.exe not found: $ML64_PATH"
        Write-Error "   Please install Visual Studio C++ Build Tools"
        exit 1
    }
    Write-Success "✅ ml64.exe found"
    
    # Check Node.js for TypeScript engine
    try {
        $nodeVersion = (node --version 2>&1)
        Write-Success "✅ Node.js found: $nodeVersion"
    }
    catch {
        Write-Error "❌ Node.js not found"
        exit 1
    }
}

# ============================================================================
# PHASE 2: MODEL DIGESTION
# ============================================================================

function Run-ModelDigestion {
    Write-Info "`n=== PHASE 2: MODEL DIGESTION ==="
    
    # Create digestion script
    $digestScript = @"
import { ModelDigestionEngine, ModelDigestionConfig } from './model-digestion-engine';

const config: ModelDigestionConfig = {
    inputFormat: 'gguf',
    outputFormat: 'encrypted-blob',
    targetArch: 'x64-asm',
    encryptionMethod: 'carmilla-aes256',
    compressionLevel: 6,
    obfuscationLevel: 'heavy',
    antiAnalysisEnabled: true,
    includeMetadata: true
};

const engine = new ModelDigestionEngine(config);

async function main() {
    console.log('🚀 Starting model digestion...');
    console.log(`📁 Input: $ModelPath`);
    console.log(`📦 Output: $OutputDir`);
    
    try {
        const pkg = await engine.generateIntegrationPackage(
            '$ModelPath',
            '$OutputDir'
        );
        console.log('✅ Digestion complete!');
        console.log('📂 Package:', pkg);
    } catch (error: any) {
        console.error('❌ Digestion failed:', error.message);
        process.exit(1);
    }
}

main();
"@
    
    $tempScript = "$OutputDir\digest.ts"
    Set-Content -Path $tempScript -Value $digestScript
    
    Write-Info "📝 Running digestion engine..."
    
    # Compile and run TypeScript
    try {
        npx ts-node $tempScript
        Write-Success "✅ Model digestion complete"
    }
    catch {
        Write-Error "❌ Digestion failed: $_"
        exit 1
    }
}

# ============================================================================
# PHASE 3: COMPILE MASM STUB
# ============================================================================

function Compile-MasmStub {
    Write-Info "`n=== PHASE 3: COMPILE MASM STUB ==="
    
    $asmFile = "$OutputDir\model.digested.asm"
    $objFile = "$OutputDir\model.digested.obj"
    $libFile = "$OutputDir\model.digestion.lib"
    
    if (!(Test-Path $asmFile)) {
        Write-Error "❌ ASM file not found: $asmFile"
        exit 1
    }
    
    Write-Info "📝 Compiling with ml64.exe..."
    Write-Info "   Input:  $asmFile"
    Write-Info "   Output: $objFile"
    
    try {
        & $ML64_PATH /c /Zd $asmFile /Fo $objFile 2>&1 | ForEach-Object {
            if ($_ -match "error") {
                Write-Error $_
            }
            else {
                Write-Info $_
            }
        }
        
        Write-Success "✅ Object file created: $objFile"
    }
    catch {
        Write-Error "❌ ml64.exe failed: $_"
        exit 1
    }
    
    # Create static library
    Write-Info "📚 Creating static library..."
    
    try {
        & $LIB_PATH $objFile /out:$libFile 2>&1 | ForEach-Object {
            Write-Info $_
        }
        
        Write-Success "✅ Library created: $libFile"
    }
    catch {
        Write-Error "❌ lib.exe failed: $_"
        exit 1
    }
}

# ============================================================================
# PHASE 4: VERIFY OUTPUT
# ============================================================================

function Verify-Output {
    Write-Info "`n=== PHASE 4: VERIFY OUTPUT ==="
    
    $requiredFiles = @(
        "model.digested.blob",
        "model.digested.meta.json",
        "model.digested.asm",
        "model.digested.obj",
        "model.digestion.lib",
        "model.digested.manifest.json",
        "ModelDigestionConfig.hpp",
        "INTEGRATION_GUIDE.md"
    )
    
    foreach ($file in $requiredFiles) {
        $fullPath = "$OutputDir\$file"
        if (Test-Path $fullPath) {
            $size = (Get-Item $fullPath).Length
            Write-Success "✅ $file ($([math]::Round($size/1MB, 2)) MB)"
        }
        else {
            Write-Warning "⚠️  Missing: $file"
        }
    }
    
    # Compute checksums
    Write-Info "`n📋 Checksums:"
    Get-ChildItem "$OutputDir\*" -Include "*.blob", "*.lib" | ForEach-Object {
        $hash = Get-FileHash -Path $_.FullName -Algorithm SHA256
        Write-Info "   $($_.Name): $($hash.Hash.Substring(0, 16))..."
    }
}

# ============================================================================
# PHASE 5: DEPLOYMENT
# ============================================================================

function Deploy-Model {
    Write-Info "`n=== PHASE 5: DEPLOYMENT ==="
    
    $deployDir = "d:\rawrxd\Ship\encrypted_models"
    
    if (!(Test-Path $deployDir)) {
        Write-Warning "⚠️  Deployment directory doesn't exist: $deployDir"
        Write-Info "   Skipping deployment (manual copy required)"
        return
    }
    
    Write-Info "📦 Copying files to: $deployDir"
    
    try {
        Copy-Item "$OutputDir\model.digested.blob" "$deployDir\$($ModelName.ToLower() -replace ' ', '-').blob" -Force
        Copy-Item "$OutputDir\model.digested.manifest.json" "$deployDir\$($ModelName.ToLower() -replace ' ', '-').manifest.json" -Force
        
        Write-Success "✅ Model deployed to IDE"
    }
    catch {
        Write-Warning "⚠️  Deployment failed: $_"
        Write-Info "   Copy manually: $OutputDir -> $deployDir"
    }
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

function Main {
    Write-Info @"
╔════════════════════════════════════════════════════════════════╗
║       MODEL DIGESTION QUICK START - 800B MODEL PIPELINE       ║
╚════════════════════════════════════════════════════════════════╝
"@
    
    Write-Info "Configuration:"
    Write-Info "  Model:     $ModelName"
    Write-Info "  Input:     $ModelPath"
    Write-Info "  Output:    $OutputDir"
    Write-Info ""
    
    # Execute phases
    Validate-Environment
    Run-ModelDigestion
    Compile-MasmStub
    Verify-Output
    Deploy-Model
    
    Write-Success @"
`n╔════════════════════════════════════════════════════════════════╗
║                    ✅ DIGESTION COMPLETE                       ║
╚════════════════════════════════════════════════════════════════╝

Next Steps:
1. Add ModelDigestion.hpp to RawrXD IDE project
2. Link model.digestion.lib in Visual Studio
3. Call EncryptedModelLoader::AutoLoad() in LoadGGUFModel()
4. Rebuild IDE executable
5. Deploy encrypted model to encrypted_models/

Documentation: $OutputDir\INTEGRATION_GUIDE.md
"@
}

# Run
Main
