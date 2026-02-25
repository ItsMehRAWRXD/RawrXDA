# ============================================
# Check Missing Models from Agentic Guide
# ============================================

Write-Host "`n🔍 Checking installed Ollama models..." -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray

# Models from the guide
$guideModels = @(
    @{ Name = "hf.co/bartowski/Llama-3.2-1B-Instruct-GGUF:Q4_K_M"; DisplayName = "Llama-3.2-1B-Instruct (Q4_K_M)"; Size = "~0.7 GB" }
    @{ Name = "hf.co/bartowski/Llama-3.2-3B-Instruct-GGUF:Q4_K_M"; DisplayName = "Llama-3.2-3B-Instruct (Q4_K_M)"; Size = "~2.7 GB" }
    @{ Name = "hf.co/bartowski/DeepSeek-Coder-V2-Lite-Instruct-GGUF:Q5_K_M"; DisplayName = "DeepSeek-Coder-V2-Lite (Q5_K_M)"; Size = "~5.5 GB" }
    @{ Name = "hf.co/bartowski/DeepSeek-Coder-V2-Lite-Instruct-GGUF:Q4_K_M"; DisplayName = "DeepSeek-Coder-V2-Lite (Q4_K_M)"; Size = "~4.3 GB" }
    @{ Name = "hf.co/bartowski/DeepSeek-Coder-V2-Lite-Instruct-GGUF:Q3_K_S"; DisplayName = "DeepSeek-Coder-V2-Lite (Q3_K_S)"; Size = "~3.2 GB" }
    @{ Name = "hf.co/bartowski/Dolphin-Llama-3.1-8B-GGUF:Q5_K_S"; DisplayName = "Dolphin-Llama-3.1-8B (Q5_K_S)"; Size = "~6.8 GB" }
    @{ Name = "dolphin-llama3.1:8b-q5_0"; DisplayName = "Dolphin-Llama-3.1-8B (Q5_0) - Alternative"; Size = "~6.8 GB" }
    @{ Name = "hf.co/bartowski/MythoLogic-L2-13B-GGUF:Q4_K_M"; DisplayName = "MythoLogic-L2-13B (Q4_K_M)"; Size = "~7.9 GB" }
)

# Get installed models
$installedModels = @()
try {
    $response = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method Get -TimeoutSec 5 -ErrorAction Stop
    if ($response.models) {
        foreach ($model in $response.models) {
            $installedModels += $model.name
        }
    }
}
catch {
    Write-Host "❌ Failed to connect to Ollama API" -ForegroundColor Red
    exit 1
}

# Check each guide model
$installed = @()
$missing = @()

foreach ($guideModel in $guideModels) {
    $found = $false
    $matchedName = $null
    
    # Check exact match
    if ($installedModels -contains $guideModel.Name) {
        $found = $true
        $matchedName = $guideModel.Name
    }
    else {
        # Extract model identifier (remove prefix and quantization)
        $modelId = $guideModel.Name -replace '^hf\.co/bartowski/', '' -replace ':[^:]+$', '' -replace '-GGUF$', ''
        
        # Check all installed models for match
        foreach ($installedName in $installedModels) {
            $installedId = $installedName -replace '^hf\.co/bartowski/', '' -replace ':[^:]+$', '' -replace '-GGUF$', ''
            
            # Match if the model identifiers are the same
            if ($modelId -eq $installedId) {
                $found = $true
                $matchedName = $installedName
                break
            }
        }
        
        # Special case for dolphin-llama alternative name
        if (-not $found -and $guideModel.Name -eq "dolphin-llama3.1:8b-q5_0") {
            foreach ($installedName in $installedModels) {
                if ($installedName -like "*dolphin*" -and $installedName -like "*llama*3.1*") {
                    $found = $true
                    $matchedName = $installedName
                    break
                }
            }
        }
    }
    
    if ($found) {
        $installed += @{
            GuideName = $guideModel.DisplayName
            InstalledName = $matchedName
            Size = $guideModel.Size
        }
    }
    else {
        $missing += @{
            Name = $guideModel.Name
            DisplayName = $guideModel.DisplayName
            Size = $guideModel.Size
        }
    }
}

# Display results
Write-Host "`n✅ INSTALLED MODELS ($($installed.Count)):" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray
if ($installed.Count -eq 0) {
    Write-Host "   None found" -ForegroundColor Yellow
}
else {
    foreach ($model in $installed) {
        Write-Host "   ✓ $($model.GuideName)" -ForegroundColor Green
        Write-Host "     → $($model.InstalledName)" -ForegroundColor DarkGray
        Write-Host "     Size: $($model.Size)" -ForegroundColor DarkGray
        Write-Host ""
    }
}

Write-Host "`n❌ MISSING MODELS ($($missing.Count)):" -ForegroundColor Red
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray
if ($missing.Count -eq 0) {
    Write-Host "   🎉 All guide models are installed!" -ForegroundColor Green
}
else {
    foreach ($model in $missing) {
        Write-Host "   ✗ $($model.DisplayName)" -ForegroundColor Red
        Write-Host "     Size: $($model.Size)" -ForegroundColor DarkGray
        Write-Host "     Command: ollama run $($model.Name)" -ForegroundColor Cyan
        Write-Host ""
    }
}

# Install commands
if ($missing.Count -gt 0) {
    Write-Host "`n📋 INSTALL COMMANDS:" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray
    foreach ($model in $missing) {
        Write-Host "ollama run $($model.Name)" -ForegroundColor White
    }
}

# Summary
Write-Host "`n📊 SUMMARY:" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray
Write-Host "   Guide models: $($guideModels.Count)" -ForegroundColor White
Write-Host "   ✅ Installed: $($installed.Count)" -ForegroundColor Green
Write-Host "   ❌ Missing: $($missing.Count)" -ForegroundColor Red
Write-Host "   Total Ollama models: $($installedModels.Count)" -ForegroundColor White
Write-Host ""

