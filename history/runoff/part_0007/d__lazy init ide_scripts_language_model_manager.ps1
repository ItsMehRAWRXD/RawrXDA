#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Advanced Language-Model Registry System Manager
    Manages 60+ custom languages paired with models for specific tasks
    Supports dynamic loading, unloading, and complete system reset

.DESCRIPTION
    Complete management system for:
    - 60+ custom-built languages with individual compilers
    - Language-Model pairings for optimal task execution
    - Dynamic language loading based on model selection
    - Complete state reset and reinitialization
    - Language performance metrics and statistics

.PARAMETER Action
    get-languages, get-models, load-lang, unload-lang, 
    reset-lang, reset-model, reset-all, initialize, status

.PARAMETER Language
    Specific language to target

.PARAMETER Model
    Specific model to target

.PARAMETER Category
    Language category (European, Asian, African, Middle-Eastern)

.EXAMPLE
    .\language_model_manager.ps1 -Action get-languages
    
.EXAMPLE
    .\language_model_manager.ps1 -Action get-models
    
.EXAMPLE
    .\language_model_manager.ps1 -Action load-lang -Language Spanish -Model GPT-4
    
.EXAMPLE
    .\language_model_manager.ps1 -Action reset-all
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('get-languages', 'get-languages-detailed', 'get-models', 'get-model-languages',
                 'load-lang', 'unload-lang', 'get-loaded', 'initialize-model',
                 'reset-lang', 'reset-model', 'reset-all', 'get-status', 'get-compiler-info')]
    [string]$Action = 'get-status',
    
    [Parameter(Mandatory=$false)]
    [string]$Language,
    
    [Parameter(Mandatory=$false)]
    [string]$Model,
    
    [Parameter(Mandatory=$false)]
    [string]$Category,
    
    [Parameter(Mandatory=$false)]
    [string[]]$LanguageList,
    
    [Parameter(Mandatory=$false)]
    [string]$CompilerPath = "D:\lazy init ide\compilers"
)

# Import the language-model registry module
Import-Module "$PSScriptRoot\language_model_registry.psm1" -Force

# ============================================================================
# DISPLAY FUNCTIONS
# ============================================================================

function Show-Header {
    Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  🌍 Language-Model Registry Manager (60+ Languages)             ║" -ForegroundColor Cyan
    Write-Host "║  Custom Compiler System with Dynamic Loading                    ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
}

function Show-AllLanguages {
    $languages = Get-AllAvailableLanguages
    
    Write-Host "`n📚 AVAILABLE LANGUAGES: $($languages.Count)" -ForegroundColor Green
    Write-Host "═" * 70
    
    $languagesByCategory = $languages.GetEnumerator() | Group-Object { $_.Value.Category }
    
    foreach ($categoryGroup in $languagesByCategory) {
        Write-Host "`n🗺️  $($categoryGroup.Name) ($($categoryGroup.Count) languages)" -ForegroundColor Yellow
        Write-Host "─" * 70
        
        foreach ($item in $categoryGroup.Group) {
            $lang = $item.Value
            $code = $lang.Code.PadRight(10)
            $models = ($lang.Models | ForEach-Object { "[$_]" }) -join " "
            $tasks = ($lang.TaskTypes | ForEach-Object { "$_" }) -join ", "
            
            Write-Host "  ✓ $($item.Name.PadRight(25)) [$code]" -ForegroundColor Cyan
            Write-Host "    Code: $($lang.Code) | Compiler: $($lang.Compiler)" -ForegroundColor Gray
            Write-Host "    Models: $models" -ForegroundColor Gray
            Write-Host "    Tasks: $tasks" -ForegroundColor Gray
        }
    }
    
    Write-Host "`n" + ("═" * 70)
    Write-Host "Total Languages Available: $($languages.Count)" -ForegroundColor Green
}

function Show-AllModels {
    $module = Get-Module language_model_registry
    $models = $module.ModuleBase | Get-Content -Path "$PSScriptRoot\language_model_registry.psm1" | 
        Select-String "'\w+-\d+'" | ForEach-Object { 
            $_ -replace ".*'(\w+-\d+)'.*", '$1' 
        } | Sort-Object -Unique
    
    Write-Host "`n🤖 AVAILABLE MODELS WITH LANGUAGE SUPPORT" -ForegroundColor Green
    Write-Host "═" * 70
    
    $languages = Get-AllAvailableLanguages
    $modelCount = 0
    
    foreach ($lang in $languages.Values) {
        foreach ($model in $lang.Models) {
            $modelCount++
        }
    }
    
    Write-Host "Unique Models Found: $($languages.Values | ForEach-Object { $_.Models } | Sort-Object -Unique | Measure-Object | Select-Object -ExpandProperty Count)" -ForegroundColor Yellow
    
    $languages.Values | ForEach-Object { $_.Models } | Sort-Object -Unique | ForEach-Object {
        $model = $_
        $supportedLangs = @($languages.GetEnumerator() | Where-Object { $model -in $_.Value.Models } | Select-Object -ExpandProperty Key)
        
        Write-Host "`n🔹 $model" -ForegroundColor Cyan
        Write-Host "   Supports $($supportedLangs.Count) languages:" -ForegroundColor Gray
        
        $supportedLangs | ForEach-Object {
            Write-Host "     • $_" -ForegroundColor Gray
        }
    }
}

function Show-LanguageDetails {
    param([string]$LanguageName)
    
    $info = Get-LanguageCompilerInfo -Language $LanguageName
    
    if (-not $info) {
        Write-Host "❌ Language '$LanguageName' not found" -ForegroundColor Red
        return
    }
    
    Write-Host "`n📖 LANGUAGE DETAILS: $LanguageName" -ForegroundColor Green
    Write-Host "═" * 70
    
    Write-Host "Language Code:      $($info.Code)" -ForegroundColor Cyan
    Write-Host "Category:           $($info.Category)" -ForegroundColor Cyan
    Write-Host "Compiler:           $($info.Compiler)" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Aliases:            $($info.Aliases -join ', ')" -ForegroundColor Yellow
    Write-Host "Features:           $($info.Features -join ', ')" -ForegroundColor Yellow
    Write-Host "Supported Models:   $($info.SupportedModels -join ', ')" -ForegroundColor Yellow
    Write-Host "Optimal Tasks:      $($info.OptimalTaskTypes -join ', ')" -ForegroundColor Yellow
}

function Show-LoadedLanguages {
    param([string]$ModelName)
    
    $loaded = Get-LoadedLanguages -ModelName $ModelName
    
    Write-Host "`n✅ LOADED LANGUAGES" -ForegroundColor Green
    Write-Host "═" * 70
    
    if ($ModelName) {
        Write-Host "Model: $ModelName" -ForegroundColor Yellow
    }
    
    if ($loaded.Count -eq 0) {
        Write-Host "No languages currently loaded" -ForegroundColor Yellow
        return
    }
    
    foreach ($item in $loaded) {
        Write-Host "`n▸ $($item.Language) (Model: $($item.Model))" -ForegroundColor Cyan
        Write-Host "  Loaded At:  $($item.LoadedAt)" -ForegroundColor Gray
        Write-Host "  Features:   $($item.Features -join ', ')" -ForegroundColor Gray
    }
}

function Show-SystemStatus {
    $status = Get-LanguageState
    
    Write-Host "`n📊 SYSTEM STATUS" -ForegroundColor Green
    Write-Host "═" * 70
    
    Write-Host "Loaded Compilers:   $($status.LoadedCompilers)" -ForegroundColor Cyan
    Write-Host "Active Languages:   $($status.ActiveLanguages)" -ForegroundColor Cyan
    Write-Host "Timestamp:          $($status.Timestamp)" -ForegroundColor Gray
    
    if ($status.LoadedCompilers -gt 0) {
        Write-Host "`n📋 Loaded Compiler Details:" -ForegroundColor Yellow
        foreach ($key in $status.StateDetails.Cache.Keys) {
            $compiler = $status.StateDetails.Cache[$key]
            Write-Host "  • $key" -ForegroundColor Gray
            Write-Host "    Language: $($compiler.Language) | Model: $($compiler.Model)" -ForegroundColor Gray
        }
    }
}

function Show-ResetConfirmation {
    param(
        [string]$ResetType = 'all'
    )
    
    Write-Host "`n⚠️  RESET CONFIRMATION" -ForegroundColor Yellow
    Write-Host "═" * 70
    
    switch ($ResetType) {
        'all' {
            Write-Host "This will reset ALL languages, models, and states." -ForegroundColor Yellow
            Write-Host "This operation is IRREVERSIBLE." -ForegroundColor Red
        }
        'model' {
            Write-Host "This will reset all languages for model: $Model" -ForegroundColor Yellow
        }
        'lang' {
            Write-Host "This will reset language: $Language" -ForegroundColor Yellow
        }
    }
    
    $response = Read-Host "`n⚡ Are you sure? Type 'YES' to confirm"
    
    if ($response -eq 'YES') {
        return $true
    }
    
    Write-Host "❌ Reset cancelled" -ForegroundColor Red
    return $false
}

# ============================================================================
# ACTION HANDLERS
# ============================================================================

function Handle-GetLanguages {
    Show-AllLanguages
}

function Handle-GetLanguagesDetailed {
    $languages = Get-AllAvailableLanguages
    
    Write-Host "`n📚 DETAILED LANGUAGE INVENTORY ($($ languages.Count) languages)" -ForegroundColor Green
    Write-Host "═" * 100
    
    foreach ($item in $languages.GetEnumerator() | Sort-Object Name) {
        $lang = $item.Value
        Write-Host "`n[$($lang.Code.ToUpper())] $($item.Name)" -ForegroundColor Cyan
        Write-Host "  Compiler:        $($lang.Compiler)" -ForegroundColor Gray
        Write-Host "  Category:        $($lang.Category)" -ForegroundColor Gray
        Write-Host "  Aliases:         $($lang.Aliases -join ', ')" -ForegroundColor Gray
        Write-Host "  Features:        $($lang.Features -join ', ')" -ForegroundColor Gray
        Write-Host "  Models:          $($lang.Models -join ', ')" -ForegroundColor Gray
        Write-Host "  Tasks:           $($lang.TaskTypes -join ', ')" -ForegroundColor Gray
    }
}

function Handle-GetModels {
    Show-AllModels
}

function Handle-GetModelLanguages {
    if (-not $Model) {
        Write-Host "❌ Model parameter required for this action" -ForegroundColor Red
        return
    }
    
    $languages = Get-LanguagesForModel -ModelName $Model
    
    Write-Host "`n🔹 LANGUAGES FOR MODEL: $Model" -ForegroundColor Green
    Write-Host "═" * 70
    
    if ($languages.Count -eq 0) {
        Write-Host "No languages configured for this model" -ForegroundColor Yellow
        return
    }
    
    foreach ($item in $languages) {
        Write-Host "`n✓ $($item.Language) [$($item.Tier)]" -ForegroundColor Cyan
        Write-Host "  Code: $($item.LanguageData.Code)" -ForegroundColor Gray
        Write-Host "  Features: $($item.LanguageData.Features -join ', ')" -ForegroundColor Gray
    }
}

function Handle-LoadLanguage {
    if (-not $Language -or -not $Model) {
        Write-Host "❌ Both Language and Model parameters required" -ForegroundColor Red
        return
    }
    
    Write-Host "`n📥 Loading language compiler..." -ForegroundColor Yellow
    
    $result = Load-LanguageForModel -Language $Language -ModelName $Model -CompilerPath $CompilerPath
    
    if ($result) {
        Write-Host "✅ Successfully loaded: $Language for $Model" -ForegroundColor Green
        Write-Host "   Compiler: $($result.CompilerFile)" -ForegroundColor Gray
        Write-Host "   Status: $($result.Status)" -ForegroundColor Gray
        Write-Host "   Features: $($result.Features -join ', ')" -ForegroundColor Gray
    } else {
        Write-Host "❌ Failed to load language compiler" -ForegroundColor Red
    }
}

function Handle-UnloadLanguage {
    if (-not $Language -or -not $Model) {
        Write-Host "❌ Both Language and Model parameters required" -ForegroundColor Red
        return
    }
    
    Write-Host "`n📤 Unloading language compiler..." -ForegroundColor Yellow
    
    $result = Unload-LanguageForModel -Language $Language -ModelName $Model
    
    if ($result) {
        Write-Host "✅ Successfully unloaded: $Language from $Model" -ForegroundColor Green
    } else {
        Write-Host "⚠️  Language was not loaded" -ForegroundColor Yellow
    }
}

function Handle-GetLoaded {
    Show-LoadedLanguages -ModelName $Model
}

function Handle-InitializeModel {
    if (-not $Model) {
        Write-Host "❌ Model parameter required" -ForegroundColor Red
        return
    }
    
    if (-not $LanguageList -or $LanguageList.Count -eq 0) {
        Write-Host "❌ LanguageList parameter required (comma-separated)" -ForegroundColor Red
        return
    }
    
    Write-Host "`n🚀 Initializing languages for model: $Model" -ForegroundColor Yellow
    
    $result = Initialize-LanguageForModel -ModelName $Model -Languages $LanguageList -CompilerPath $CompilerPath
    
    Write-Host "`n✅ Initialization Complete" -ForegroundColor Green
    Write-Host "   Model: $($result.Model)" -ForegroundColor Gray
    Write-Host "   Successfully Initialized: $($result.InitializedCount)" -ForegroundColor Green
    Write-Host "   Failed: $($result.FailureCount)" -ForegroundColor Yellow
    
    if ($result.SuccessfullyInitialized.Count -gt 0) {
        Write-Host "`n   Languages:" -ForegroundColor Gray
        $result.SuccessfullyInitialized | ForEach-Object { Write-Host "     ✓ $_" -ForegroundColor Green }
    }
    
    if ($result.Failed.Count -gt 0) {
        Write-Host "`n   Failed:" -ForegroundColor Gray
        $result.Failed | ForEach-Object { Write-Host "     ✗ $_" -ForegroundColor Red }
    }
}

function Handle-ResetLanguage {
    if (-not $Language) {
        Write-Host "❌ Language parameter required" -ForegroundColor Red
        return
    }
    
    if (-not (Show-ResetConfirmation -ResetType 'lang')) {
        return
    }
    
    Write-Host "`n🔄 Resetting language state..." -ForegroundColor Yellow
    
    $result = Unload-LanguageForModel -Language $Language -ModelName "*"
    
    Write-Host "✅ Language reset complete" -ForegroundColor Green
}

function Handle-ResetModel {
    if (-not $Model) {
        Write-Host "❌ Model parameter required" -ForegroundColor Red
        return
    }
    
    if (-not (Show-ResetConfirmation -ResetType 'model')) {
        return
    }
    
    Write-Host "`n🔄 Resetting model languages..." -ForegroundColor Yellow
    
    $result = Reset-ModelLanguages -ModelName $Model
    
    Write-Host "✅ Model languages reset complete" -ForegroundColor Green
    Write-Host "   Status: $($result.Status)" -ForegroundColor Gray
    Write-Host "   Languages Unloaded: $($result.LanguagesUnloaded)" -ForegroundColor Yellow
}

function Handle-ResetAll {
    if (-not (Show-ResetConfirmation -ResetType 'all')) {
        return
    }
    
    Write-Host "`n🔄 Performing full system reset..." -ForegroundColor Yellow
    
    $result = Reset-AllModels
    
    Write-Host "✅ Full system reset complete" -ForegroundColor Green
    Write-Host "   Status: $($result.Status)" -ForegroundColor Gray
    Write-Host "   Message: $($result.Message)" -ForegroundColor Yellow
    Write-Host "   Next Step: $($result.NextStep)" -ForegroundColor Gray
}

function Handle-GetStatus {
    Show-SystemStatus
}

function Handle-GetCompilerInfo {
    if (-not $Language) {
        Write-Host "❌ Language parameter required" -ForegroundColor Red
        return
    }
    
    $info = Get-LanguageCompilerInfo -Language $Language
    
    if (-not $info) {
        Write-Host "❌ Language '$Language' not found" -ForegroundColor Red
        return
    }
    
    Show-LanguageDetails -LanguageName $Language
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

Show-Header

switch ($Action) {
    'get-languages' { Handle-GetLanguages }
    'get-languages-detailed' { Handle-GetLanguagesDetailed }
    'get-models' { Handle-GetModels }
    'get-model-languages' { Handle-GetModelLanguages }
    'load-lang' { Handle-LoadLanguage }
    'unload-lang' { Handle-UnloadLanguage }
    'get-loaded' { Handle-GetLoaded }
    'initialize-model' { Handle-InitializeModel }
    'reset-lang' { Handle-ResetLanguage }
    'reset-model' { Handle-ResetModel }
    'reset-all' { Handle-ResetAll }
    'get-status' { Handle-GetStatus }
    'get-compiler-info' { Handle-GetCompilerInfo }
}

Write-Host "`n"
