# Language-Model Registry Quick Start Guide

## 5-Minute Setup

### Step 1: Verify Installation
```powershell
# Check registry module exists
Test-Path "D:\lazy init ide\scripts\language_model_registry.psm1"

# Check manager script exists
Test-Path "D:\lazy init ide\scripts\language_model_manager.ps1"

# Check integration module exists
Test-Path "D:\lazy init ide\scripts\language_model_integration.ps1"
```

### Step 2: Import the Module
```powershell
Import-Module "D:\lazy init ide\scripts\language_model_registry.psm1" -Force
```

### Step 3: View Available Languages
```powershell
# Get all languages
$allLangs = Get-AllAvailableLanguages
Write-Host "Available languages: $($allLangs.Count)"

# List them
$allLangs.Keys | Sort-Object
```

### Step 4: Check Model Support
```powershell
# See what languages GPT-4 supports
$gpt4Langs = Get-LanguagesForModel -ModelName "GPT-4"
$gpt4Langs | ForEach-Object { Write-Host "$($_.Language) [$($_.Tier)]" }
```

### Step 5: Initialize a Model
```powershell
# Load languages for GPT-4
$result = Initialize-LanguageForModel `
  -ModelName "GPT-4" `
  -Languages @("English", "Spanish", "French", "German") `
  -CompilerPath "D:\lazy init ide\compilers"

Write-Host "Loaded: $($result.InitializedCount) languages"
```

### Step 6: Check What's Loaded
```powershell
# See loaded languages
$loaded = Get-LoadedLanguages
$loaded | ForEach-Object { Write-Host "$($_.Language) for $($_.Model)" }

# Get system status
$status = Get-LanguageState
Write-Host "Total loaded: $($status.LoadedCompilers)"
```

---

## Common Tasks

### Task 1: Load Languages for a Model
```powershell
# First, see what's available for the model
$langs = Get-LanguagesForModel -ModelName "Claude-3"

# Initialize the model with all primary languages
$result = Initialize-LanguageForModel `
  -ModelName "Claude-3" `
  -Languages @("English", "French", "German")
```

### Task 2: Switch Models
```powershell
# Reset old model
Reset-ModelLanguages -ModelName "GPT-4"

# Initialize new model
Initialize-LanguageForModel `
  -ModelName "Llama-2" `
  -Languages @("English", "Spanish")
```

### Task 3: Add More Languages to Running Model
```powershell
# Get already loaded
$loaded = (Get-LoadedLanguages -ModelName "GPT-4").Language

# Load additional language
Load-LanguageForModel -Language "Chinese" -ModelName "GPT-4"
```

### Task 4: See What's Loaded
```powershell
# All loaded
Get-LoadedLanguages

# Just for one model
Get-LoadedLanguages -ModelName "GPT-4"

# System overview
Get-LanguageState
```

### Task 5: Get Language Information
```powershell
# Detailed info on a language
$info = Get-LanguageCompilerInfo -Language "Spanish"
Write-Host "Compiler: $($info.Compiler)"
Write-Host "Features: $($info.Features -join ', ')"
Write-Host "Models: $($info.SupportedModels -join ', ')"
```

### Task 6: Reset Everything
```powershell
# Full system reset (clears all languages, models, states)
Reset-AllModels

# Reinitialize as needed
Initialize-LanguageForModel -ModelName "GPT-4" -Languages @("English")
```

---

## Using the CLI Manager

### View All Languages
```powershell
.\language_model_manager.ps1 -Action get-languages
```

### View Languages for a Model
```powershell
.\language_model_manager.ps1 -Action get-model-languages -Model GPT-4
```

### Load a Language
```powershell
.\language_model_manager.ps1 -Action load-lang -Language Spanish -Model GPT-4
```

### Initialize Model with Multiple Languages
```powershell
.\language_model_manager.ps1 `
  -Action initialize-model `
  -Model "GPT-4" `
  -LanguageList @("English", "Spanish", "French", "German")
```

### Check System Status
```powershell
.\language_model_manager.ps1 -Action get-status
```

### Reset
```powershell
.\language_model_manager.ps1 -Action reset-all
```

---

## Using Integration with Making Station

### In PowerShell
```powershell
# Import integration module
Import-Module "D:\lazy init ide\scripts\language_model_integration.ps1"

# Show menu
Invoke-LanguageModelIntegrationMenu

# Then select options like LM1, LM2, etc.
```

### Menu Options Reference
- `LM1` - Load languages for model
- `LM2` - View what's loaded
- `LM5` - Show all 60+ languages
- `LM7` - Show what model supports
- `LM8` - Initialize model (guided)
- `LM13` - Check system status

---

## Directory Structure

```
D:\lazy init ide\
├── scripts\
│   ├── language_model_registry.psm1      <- Core registry
│   ├── language_model_manager.ps1         <- CLI tool
│   └── language_model_integration.ps1    <- Making Station addon
├── compilers\                            <- Compiler executables
│   ├── English-Compiler-v1.0
│   ├── Spanish-Compiler-v1.0
│   ├── French-Compiler-v1.0
│   ├── (... more compilers ...)
│   └── (60+ total compilers)
└── logs\swarm_config\
    ├── models.json
    └── agent_presets.json
```

---

## Language Categories

| Category | Count | Languages |
|----------|-------|-----------|
| **European** | 15 | English, Spanish, French, German, Italian, Portuguese, Russian, Polish, Dutch, Swedish, Norwegian, Danish, Finnish, Icelandic, Estonian |
| **Asian** | 15 | Japanese, Chinese, Korean, Hindi, Thai, Vietnamese, Indonesian, Tagalog, Malay, Turkish, Bengali, Punjabi, Urdu, Gujarati, Marathi |
| **Middle East/Africa** | 12 | Arabic, Hebrew, Persian, Swahili, Yoruba, Amharic, Igbo, Hausa, Somali, Afrikaans, Zulu, Xhosa |
| **Slavic** | 10 | Greek, Czech, Slovak, Hungarian, Romanian, Bulgarian, Serbian, Croatian, Slovenian, Lithuanian |
| **Nordic/Baltic** | 5 | (covered in European) |
| **Specialized** | 8 | Ukrainian, Latvian, Vietnamese-Modern, Simplified-Chinese, Traditional-Chinese, Cantonese, Taiwanese, Khmer, Lao |
| **TOTAL** | **60+** | All available for model pairing |

---

## Model Support Matrix

### GPT-4
- **Rating**: 0.95 ⭐⭐⭐⭐⭐
- **Primary Languages** (5): English, Spanish, French, German, Chinese
- **Secondary Languages** (5+): Italian, Portuguese, Russian, Japanese, Arabic
- **Best For**: General purpose, complex reasoning, coding

### Claude-3
- **Rating**: 0.93 ⭐⭐⭐⭐⭐
- **Primary Languages** (3): English, French, German
- **Secondary Languages** (5+): Spanish, Japanese, Portuguese, Korean, Italian
- **Best For**: Analysis, documentation, creative

### Llama-2
- **Rating**: 0.88 ⭐⭐⭐⭐
- **Primary Languages** (1): English
- **Secondary Languages** (9+): Spanish, French, German, Italian, Portuguese, Chinese, Japanese, Korean, Arabic
- **Best For**: Fast inference, code completion, embedding

### Qwen
- **Rating**: 0.92 ⭐⭐⭐⭐⭐
- **Primary Languages** (1): Chinese
- **Secondary Languages** (3): English, Japanese, Korean
- **Best For**: Multilingual processing, Asian languages

### Custom-Model-v1
- **Rating**: 0.85 ⭐⭐⭐⭐
- **Primary Languages** (3): English, Spanish, Portuguese
- **Extensible**: Add more as needed
- **Best For**: Custom domains and specialized tasks

---

## Troubleshooting

### "Module not found" error
```powershell
# Make sure you're in the correct directory
cd "D:\lazy init ide\scripts"

# Or use full path
Import-Module "D:\lazy init ide\scripts\language_model_registry.psm1" -Force
```

### "Language not found" error
```powershell
# Check available languages
$all = Get-AllAvailableLanguages
$all.Keys

# Make sure spelling is correct (case-sensitive)
# "Spanish" ✓ not "spanish"
```

### "Compiler not found" error
```powershell
# Check compiler path exists
Test-Path "D:\lazy init ide\compilers"

# Check compiler files
Get-ChildItem "D:\lazy init ide\compilers" | Select-Object Name
```

### "State is corrupted" or "Nothing loading"
```powershell
# Full system reset
Reset-AllModels

# Then reinitialize
Initialize-LanguageForModel -ModelName "GPT-4" -Languages @("English")
```

---

## Next Steps

1. **Load your first language**: `Initialize-LanguageForModel -ModelName "GPT-4" -Languages @("English", "Spanish")`
2. **Check what's loaded**: `Get-LoadedLanguages`
3. **View system status**: `Get-LanguageState`
4. **Explore languages by category**: Use `language_model_manager.ps1 -Action get-languages-detailed`
5. **Integrate with Making Station**: Use `language_model_integration.ps1` for interactive menu

---

## Key Concepts

- **Registry**: The master list of 60+ languages with metadata
- **Compiler**: The language processor executable (stored in `D:\lazy init ide\compilers\`)
- **Loading**: Bringing a compiler into active memory for a specific model
- **Pairing**: Association between a language and model (not all languages work with all models)
- **Caching**: Keeping loaded compilers in memory for fast reuse
- **State**: Tracking what languages are currently loaded and active
- **Reset**: Clearing loaded languages and returning to clean state

---

## Performance Notes

- **First load**: 100-500ms per language
- **Subsequent loads**: <10ms (cached)
- **Memory per language**: 50-200MB (depends on compiler)
- **5 loaded languages**: ~250-1000MB typical
- **Cache cleanup**: Automatic on unload

---

## Support Resources

- **Full Documentation**: See `LANGUAGE_MODEL_REGISTRY_DOCUMENTATION.md`
- **CLI Help**: `.\language_model_manager.ps1 -Action get-status`
- **Integration Help**: Import module, then call `Show-IntegrationHelp`
- **API Reference**: Check module exports with `Get-Module language_model_registry | Select -ExpandProperty ExportedFunctions`

---

**Version**: 1.0  
**Created**: January 2024  
**For**: Custom 60+ Language-Model Registry System
