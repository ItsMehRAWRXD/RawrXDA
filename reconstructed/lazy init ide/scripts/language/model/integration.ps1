#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Language-Model Registry Integration Module
    Provides menu integration for Language-Model Registry with Model-Making-Station

.DESCRIPTION
    This module integrates the 60+ Language-Model Registry system with the
    Model/Agent Making Station, allowing users to:
    - Load languages for created models
    - Reset model language states
    - View language-model associations
    - Initialize models with multiple languages
    - Manage compiler caching

.EXPORTED FUNCTIONS
    - Show-LanguageModelIntegrationMenu
    - Initialize-LanguagesForModel
    - Show-LoadedLanguagesForModel
    - Reset-ModelLanguageState
    - Get-LanguageModelStatus

.EXAMPLE
    Import-Module .\language_model_integration.ps1
    Show-LanguageModelIntegrationMenu
#>

param()

# ============================================================================
# IMPORT REGISTRY MODULE
# ============================================================================

$registryPath = Join-Path $PSScriptRoot "language_model_registry.psm1"
if (-not (Test-Path $registryPath)) {
    Write-Error "Language-Model Registry module not found at: $registryPath"
    exit 1
}

Import-Module $registryPath -Force -DisableNameChecking

# ============================================================================
# CONFIGURATION
# ============================================================================

$script:CompilerPath = "D:\lazy init ide\compilers"
$script:StationRoot = "D:\lazy init ide"

# ============================================================================
# DISPLAY FUNCTIONS
# ============================================================================

function Show-LanguageModelIntegrationMenu {
    <#
    .SYNOPSIS
        Display language-model management menu for integration with Making Station
    #>
    
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        🌍 LANGUAGE-MODEL REGISTRY INTEGRATION MENU                         ║" -ForegroundColor Cyan
    Write-Host "║        60+ Custom Languages with Model-Specific Loading                    ║" -ForegroundColor Magenta
    Write-Host "╚════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "Language Management:" -ForegroundColor Yellow
    Write-Host "  [LM1] Load Languages for Active Model" -ForegroundColor White
    Write-Host "  [LM2] View Loaded Languages" -ForegroundColor White
    Write-Host "  [LM3] Unload Language from Model" -ForegroundColor White
    Write-Host "  [LM4] Get Language Details" -ForegroundColor White
    Write-Host "  [LM5] Show All Languages" -ForegroundColor White
    Write-Host "  [LM6] Show Languages by Category" -ForegroundColor White
    Write-Host ""
    
    Write-Host "Model Management:" -ForegroundColor Yellow
    Write-Host "  [LM7] Show Languages for Model" -ForegroundColor White
    Write-Host "  [LM8] Initialize Model with Languages" -ForegroundColor White
    Write-Host "  [LM9] Get Model Language Status" -ForegroundColor White
    Write-Host ""
    
    Write-Host "Reset & Maintenance:" -ForegroundColor Yellow
    Write-Host "  [LM10] Reset All Languages" -ForegroundColor Red
    Write-Host "  [LM11] Reset Languages for Model" -ForegroundColor Red
    Write-Host "  [LM12] Full System Reset" -ForegroundColor Red
    Write-Host ""
    
    Write-Host "System Status:" -ForegroundColor Yellow
    Write-Host "  [LM13] Show System Status" -ForegroundColor White
    Write-Host "  [LM14] Get Compiler Cache Info" -ForegroundColor White
    Write-Host "  [LM15] Validate Compiler Paths" -ForegroundColor White
    Write-Host ""
    
    Write-Host "Advanced:" -ForegroundColor Yellow
    Write-Host "  [LM16] Launch Registry Manager CLI" -ForegroundColor Cyan
    Write-Host "  [LM17] Show Integration Help" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  [LMBACK] Back to Main Menu" -ForegroundColor DarkGray
    Write-Host ""
}

function Show-AllLanguagesSummary {
    <#
    .SYNOPSIS
        Show quick summary of all 60+ languages
    #>
    
    $languages = Get-AllAvailableLanguages
    
    Write-Host "`n📚 LANGUAGE INVENTORY ($($languages.Count) languages)" -ForegroundColor Green
    Write-Host "═" * 80
    
    $byCategory = $languages.Values | Group-Object Category
    
    foreach ($cat in $byCategory) {
        Write-Host "`n  🗺️  $($cat.Name): $($cat.Count) languages" -ForegroundColor Yellow
        
        $langNames = $cat.Group | ForEach-Object { $_.Name } | Sort-Object
        $cols = [math]::Ceiling($langNames.Count / 3)
        
        for ($i = 0; $i -lt $cols; $i++) {
            $line = ""
            for ($j = 0; $j -lt 3; $j++) {
                $idx = $i + ($j * $cols)
                if ($idx -lt $langNames.Count) {
                    $line += $langNames[$idx].PadRight(20)
                }
            }
            Write-Host "    $line" -ForegroundColor Gray
        }
    }
    
    Write-Host "`n" + ("═" * 80)
}

function Show-LanguagesByCategory {
    <#
    .SYNOPSIS
        Show languages grouped by category
    #>
    
    Write-Host "`n📂 SELECT LANGUAGE CATEGORY" -ForegroundColor Green
    Write-Host "═" * 80
    Write-Host ""
    
    $languages = Get-AllAvailableLanguages
    $categories = $languages.Values | Select-Object -ExpandProperty Category -Unique | Sort-Object
    
    $i = 1
    foreach ($cat in $categories) {
        Write-Host "  [$i] $cat" -ForegroundColor White
        $i++
    }
    Write-Host "  [0] Back" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "Select category: " -NoNewline -ForegroundColor Cyan
    $choice = Read-Host
    
    if ($choice -eq "0") { return }
    
    $idx = [int]$choice - 1
    if ($idx -lt 0 -or $idx -ge $categories.Count) {
        Write-Host "Invalid choice" -ForegroundColor Red
        return
    }
    
    $selectedCategory = $categories[$idx]
    
    Write-Host "`n✓ Languages in $selectedCategory category:" -ForegroundColor Green
    Write-Host ""
    
    $categoryLangs = $languages.GetEnumerator() | Where-Object { $_.Value.Category -eq $selectedCategory }
    
    foreach ($item in $categoryLangs) {
        $lang = $item.Value
        Write-Host "  • $($item.Key) [$($lang.Code)]" -ForegroundColor Cyan
        Write-Host "    Features: $($lang.Features -join ', ')" -ForegroundColor Gray
        Write-Host "    Models: $($lang.Models -join ', ')" -ForegroundColor Gray
    }
}

function Initialize-LanguagesForModel {
    <#
    .SYNOPSIS
        Initialize multiple languages for a specific model
    #>
    
    Write-Host "`n🚀 MODEL INITIALIZATION WITH LANGUAGES" -ForegroundColor Green
    Write-Host "═" * 80
    Write-Host ""
    
    Write-Host "Enter model name (e.g., GPT-4, Claude-3, Llama-2): " -NoNewline -ForegroundColor Cyan
    $model = Read-Host
    
    if ([string]::IsNullOrWhiteSpace($model)) {
        Write-Host "❌ Model name required" -ForegroundColor Red
        return
    }
    
    $supported = Get-LanguagesForModel -ModelName $model
    
    if ($supported.Count -eq 0) {
        Write-Host "❌ No languages configured for model: $model" -ForegroundColor Red
        return
    }
    
    Write-Host "`n✓ $model supports $($supported.Count) languages:" -ForegroundColor Green
    Write-Host ""
    
    $primary = $supported | Where-Object { $_.Tier -eq 'Primary' }
    $secondary = $supported | Where-Object { $_.Tier -eq 'Secondary' }
    
    if ($primary) {
        Write-Host "  Primary languages ($($primary.Count)):" -ForegroundColor Green
        $primary | ForEach-Object { Write-Host "    ✓ $($_.Language)" -ForegroundColor Green }
    }
    
    if ($secondary) {
        Write-Host "  Secondary languages ($($secondary.Count)):" -ForegroundColor Yellow
        $secondary | ForEach-Object { Write-Host "    • $($_.Language)" -ForegroundColor Yellow }
    }
    
    Write-Host ""
    Write-Host "Initialize options:" -ForegroundColor Cyan
    Write-Host "  [1] Load all primary languages" -ForegroundColor White
    Write-Host "  [2] Load all languages (primary + secondary)" -ForegroundColor White
    Write-Host "  [3] Select specific languages" -ForegroundColor White
    Write-Host "  [0] Cancel" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "Choice: " -NoNewline -ForegroundColor Cyan
    $choice = Read-Host
    
    $languagesToLoad = @()
    
    switch ($choice) {
        '1' {
            $languagesToLoad = $primary | Select-Object -ExpandProperty Language
        }
        '2' {
            $languagesToLoad = $supported | Select-Object -ExpandProperty Language
        }
        '3' {
            Write-Host "`nAvailable languages:" -ForegroundColor Cyan
            $i = 1
            $supportedList = $supported.Language
            foreach ($lang in $supportedList) {
                Write-Host "  [$i] $lang" -ForegroundColor White
                $i++
            }
            Write-Host ""
            
            Write-Host "Enter language numbers (comma-separated, e.g., 1,2,3): " -NoNewline -ForegroundColor Cyan
            $selectedStr = Read-Host
            
            $selected = $selectedStr -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '^\d+$' }
            
            foreach ($num in $selected) {
                $idx = [int]$num - 1
                if ($idx -ge 0 -and $idx -lt $supportedList.Count) {
                    $languagesToLoad += $supportedList[$idx]
                }
            }
        }
        default { return }
    }
    
    if ($languagesToLoad.Count -eq 0) {
        Write-Host "❌ No languages selected" -ForegroundColor Red
        return
    }
    
    Write-Host "`n⏳ Initializing $($languagesToLoad.Count) languages for $model..." -ForegroundColor Yellow
    Write-Host ""
    
    $result = Initialize-LanguageForModel -ModelName $model -Languages $languagesToLoad -CompilerPath $script:CompilerPath
    
    Write-Host "✅ Initialization Complete" -ForegroundColor Green
    Write-Host "   Successfully initialized: $($result.InitializedCount)" -ForegroundColor Green
    Write-Host "   Failed: $($result.FailureCount)" -ForegroundColor Yellow
    
    if ($result.SuccessfullyInitialized.Count -gt 0) {
        Write-Host "`n   Loaded languages:" -ForegroundColor Gray
        $result.SuccessfullyInitialized | ForEach-Object {
            Write-Host "     ✓ $_" -ForegroundColor Green
        }
    }
    
    if ($result.Failed.Count -gt 0) {
        Write-Host "`n   Failed to load:" -ForegroundColor Gray
        $result.Failed | ForEach-Object {
            Write-Host "     ✗ $_" -ForegroundColor Red
        }
    }
}

function Show-ModelLanguageStatus {
    <#
    .SYNOPSIS
        Show detailed status of languages for a model
    #>
    
    Write-Host "`n📊 MODEL LANGUAGE STATUS" -ForegroundColor Green
    Write-Host "═" * 80
    Write-Host ""
    
    Write-Host "Enter model name: " -NoNewline -ForegroundColor Cyan
    $model = Read-Host
    
    if ([string]::IsNullOrWhiteSpace($model)) {
        Write-Host "❌ Model name required" -ForegroundColor Red
        return
    }
    
    $supported = Get-LanguagesForModel -ModelName $model
    $loaded = Get-LoadedLanguages -ModelName $model
    
    Write-Host "`n✓ Model: $model" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "  Total Supported Languages: $($supported.Count)" -ForegroundColor Yellow
    Write-Host "  Currently Loaded: $($loaded.Count)" -ForegroundColor Green
    
    if ($loaded.Count -gt 0) {
        Write-Host "`n  Loaded Languages:" -ForegroundColor Green
        foreach ($item in $loaded) {
            Write-Host "    ✓ $($item.Language)" -ForegroundColor Green
            Write-Host "      Loaded: $($item.LoadedAt)" -ForegroundColor Gray
        }
    } else {
        Write-Host "`n  No languages currently loaded" -ForegroundColor Yellow
    }
}

function Show-IntegrationHelp {
    <#
    .SYNOPSIS
        Show help for language-model integration
    #>
    
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║           LANGUAGE-MODEL REGISTRY INTEGRATION HELP                         ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "OVERVIEW" -ForegroundColor Yellow
    Write-Host "  The Language-Model Registry provides 60+ custom languages that pair with" -ForegroundColor Gray
    Write-Host "  AI models for task-specific language handling and compilation." -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "KEY CONCEPTS" -ForegroundColor Yellow
    Write-Host "  • Registry: Master catalog of 60+ custom languages" -ForegroundColor Gray
    Write-Host "  • Compilers: Custom compiler for each language (user-built)" -ForegroundColor Gray
    Write-Host "  • Pairing: Model-to-Language associations (not all langs for all models)" -ForegroundColor Gray
    Write-Host "  • Caching: Compilers loaded on-demand for performance" -ForegroundColor Gray
    Write-Host "  • State: Tracking of loaded/unloaded languages per model" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "WORKFLOW" -ForegroundColor Yellow
    Write-Host "  1. Create or select a model in Making Station" -ForegroundColor Gray
    Write-Host "  2. Use LM1 to load supported languages for that model" -ForegroundColor Gray
    Write-Host "  3. Check loaded languages with LM2" -ForegroundColor Gray
    Write-Host "  4. Use languages for model-specific tasks" -ForegroundColor Gray
    Write-Host "  5. Reset states with LM10-LM12 as needed" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "LANGUAGE CATEGORIES" -ForegroundColor Yellow
    Write-Host "  • European: English, Spanish, French, German, Italian, Portuguese, etc." -ForegroundColor Gray
    Write-Host "  • Asian: Japanese, Chinese, Korean, Hindi, Thai, Vietnamese, etc." -ForegroundColor Gray
    Write-Host "  • African: Swahili, Yoruba, Amharic, Igbo, Hausa, Somali, etc." -ForegroundColor Gray
    Write-Host "  • Slavic: Russian, Polish, Czech, Slovak, Hungarian, Romanian, etc." -ForegroundColor Gray
    Write-Host "  • Nordic: Norwegian, Danish, Finnish, Icelandic, Estonian" -ForegroundColor Gray
    Write-Host "  • Specialized: Domain-specific and modern variants" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "COMMON TASKS" -ForegroundColor Yellow
    Write-Host "  Load languages for model: Use LM1 (interactive)" -ForegroundColor Gray
    Write-Host "  View what's loaded: Use LM2" -ForegroundColor Gray
    Write-Host "  Manage by category: Use LM6" -ForegroundColor Gray
    Write-Host "  Get model support info: Use LM7" -ForegroundColor Gray
    Write-Host "  Full reset: Use LM12 (careful!)" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "COMPILER PATHS" -ForegroundColor Yellow
    Write-Host "  All compilers expected in: $script:CompilerPath" -ForegroundColor DarkCyan
    Write-Host "  Format: <Language>-Compiler-v<Version>" -ForegroundColor Gray
    Write-Host "  Example: Spanish-Compiler-v1.0, Japanese-Compiler-v2.1, etc." -ForegroundColor Gray
    Write-Host ""
}

# ============================================================================
# MENU HANDLER
# ============================================================================

function Invoke-LanguageModelIntegrationMenu {
    <#
    .SYNOPSIS
        Main menu loop for language-model integration
    #>
    
    while ($true) {
        Show-LanguageModelIntegrationMenu
        
        Write-Host "Enter command: " -NoNewline -ForegroundColor Cyan
        $cmd = Read-Host
        
        switch ($cmd.ToUpper()) {
            'LM1' { 
                Write-Host "`n📥 LOAD LANGUAGES FOR MODEL" -ForegroundColor Cyan
                $model = Read-Host "Enter model name"
                if ($model) {
                    Write-Host ""
                    $langs = Get-LanguagesForModel -ModelName $model
                    if ($langs) {
                        $langs | ForEach-Object { Load-LanguageForModel -Language $_.Language -ModelName $model -CompilerPath $script:CompilerPath }
                    }
                }
                Read-Host "`nPress Enter to continue"
            }
            'LM2' { 
                Write-Host "`n✅ LOADED LANGUAGES" -ForegroundColor Cyan
                Get-LoadedLanguages
                Read-Host "`nPress Enter to continue"
            }
            'LM3' {
                Write-Host "`n📤 UNLOAD LANGUAGE" -ForegroundColor Cyan
                $lang = Read-Host "Enter language name"
                $model = Read-Host "Enter model name"
                if ($lang -and $model) {
                    Unload-LanguageForModel -Language $lang -ModelName $model
                }
                Read-Host "`nPress Enter to continue"
            }
            'LM4' {
                Write-Host "`n📖 LANGUAGE DETAILS" -ForegroundColor Cyan
                $lang = Read-Host "Enter language name"
                if ($lang) {
                    $info = Get-LanguageCompilerInfo -Language $lang
                    if ($info) {
                        Write-Host "`n  Code: $($info.Code)" -ForegroundColor White
                        Write-Host "  Category: $($info.Category)" -ForegroundColor White
                        Write-Host "  Compiler: $($info.Compiler)" -ForegroundColor White
                        Write-Host "  Features: $($info.Features -join ', ')" -ForegroundColor Gray
                        Write-Host "  Models: $($info.SupportedModels -join ', ')" -ForegroundColor Gray
                        Write-Host "  Tasks: $($info.OptimalTaskTypes -join ', ')" -ForegroundColor Gray
                    }
                }
                Read-Host "`nPress Enter to continue"
            }
            'LM5' { 
                Show-AllLanguagesSummary
                Read-Host "Press Enter to continue"
            }
            'LM6' { 
                Show-LanguagesByCategory
                Read-Host "Press Enter to continue"
            }
            'LM7' {
                Write-Host "`n🔹 LANGUAGES FOR MODEL" -ForegroundColor Green
                $model = Read-Host "Enter model name"
                if ($model) {
                    $langs = Get-LanguagesForModel -ModelName $model
                    if ($langs) {
                        Write-Host ""
                        $langs | ForEach-Object {
                            Write-Host "  [$($_.Tier)] $($_.Language)" -ForegroundColor Cyan
                        }
                    } else {
                        Write-Host "No languages found for model: $model" -ForegroundColor Yellow
                    }
                }
                Read-Host "`nPress Enter to continue"
            }
            'LM8' {
                Initialize-LanguagesForModel
                Read-Host "`nPress Enter to continue"
            }
            'LM9' {
                Show-ModelLanguageStatus
                Read-Host "`nPress Enter to continue"
            }
            'LM10' {
                Write-Host "`n⚠️  RESET ALL LANGUAGES" -ForegroundColor Yellow
                $confirm = Read-Host "Type YES to confirm"
                if ($confirm -eq 'YES') {
                    Reset-AllLanguages
                    Write-Host "✅ All languages reset" -ForegroundColor Green
                }
                Read-Host "`nPress Enter to continue"
            }
            'LM11' {
                Write-Host "`n⚠️  RESET MODEL LANGUAGES" -ForegroundColor Yellow
                $model = Read-Host "Enter model name"
                if ($model) {
                    $confirm = Read-Host "Type YES to confirm"
                    if ($confirm -eq 'YES') {
                        Reset-ModelLanguages -ModelName $model
                        Write-Host "✅ Model languages reset" -ForegroundColor Green
                    }
                }
                Read-Host "`nPress Enter to continue"
            }
            'LM12' {
                Write-Host "`n⚠️  FULL SYSTEM RESET" -ForegroundColor Red
                Write-Host "This will reset ALL models, languages, and states!" -ForegroundColor Red
                $confirm = Read-Host "Type YES to confirm"
                if ($confirm -eq 'YES') {
                    Reset-AllModels
                    Write-Host "✅ Full system reset" -ForegroundColor Green
                }
                Read-Host "`nPress Enter to continue"
            }
            'LM13' {
                Write-Host "`n📊 SYSTEM STATUS" -ForegroundColor Green
                $status = Get-LanguageState
                Write-Host "  Loaded Compilers: $($status.LoadedCompilers)" -ForegroundColor White
                Write-Host "  Active Languages: $($status.ActiveLanguages)" -ForegroundColor White
                Write-Host "  Timestamp: $($status.Timestamp)" -ForegroundColor Gray
                Read-Host "`nPress Enter to continue"
            }
            'LM14' {
                Write-Host "`n💾 COMPILER CACHE INFO" -ForegroundColor Green
                $status = Get-LanguageState
                Write-Host "  Cache entries: $($status.LoadedCompilers)" -ForegroundColor White
                if ($status.LoadedCompilers -gt 0) {
                    Write-Host "`n  Loaded compilers:" -ForegroundColor Cyan
                    $status.StateDetails.Cache.GetEnumerator() | ForEach-Object {
                        Write-Host "    • $($_.Key)" -ForegroundColor Gray
                    }
                }
                Read-Host "`nPress Enter to continue"
            }
            'LM15' {
                Write-Host "`n✓ COMPILER PATH VALIDATION" -ForegroundColor Green
                Write-Host "  Expected path: $script:CompilerPath" -ForegroundColor Cyan
                if (Test-Path $script:CompilerPath) {
                    Write-Host "  Status: ✅ Path exists" -ForegroundColor Green
                    $compilers = Get-ChildItem $script:CompilerPath -ErrorAction SilentlyContinue | Measure-Object
                    Write-Host "  Compilers found: $($compilers.Count)" -ForegroundColor White
                } else {
                    Write-Host "  Status: ❌ Path does not exist" -ForegroundColor Red
                    Write-Host "  Create it: New-Item -Path '$script:CompilerPath' -ItemType Directory" -ForegroundColor Yellow
                }
                Read-Host "`nPress Enter to continue"
            }
            'LM16' {
                Write-Host "`n🚀 Launching Language Model Registry Manager CLI..." -ForegroundColor Cyan
                $managerPath = Join-Path $PSScriptRoot "language_model_manager.ps1"
                if (Test-Path $managerPath) {
                    & $managerPath -Action get-status
                } else {
                    Write-Host "❌ Manager script not found" -ForegroundColor Red
                }
                Read-Host "`nPress Enter to continue"
            }
            'LM17' {
                Show-IntegrationHelp
                Read-Host "Press Enter to continue"
            }
            'LMBACK' { return }
            default { }
        }
    }
}

# ============================================================================
# EXPORT FUNCTIONS
# ============================================================================

Export-ModuleMember -Function @(
    'Show-LanguageModelIntegrationMenu',
    'Initialize-LanguagesForModel',
    'Show-AllLanguagesSummary',
    'Show-LanguagesByCategory',
    'Show-ModelLanguageStatus',
    'Show-IntegrationHelp',
    'Invoke-LanguageModelIntegrationMenu'
)
