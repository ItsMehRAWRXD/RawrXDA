# Language-Model Registry System Documentation

## Overview

The **Language-Model Registry** is a comprehensive system managing **60+ custom-built languages** with proprietary compilers, dynamically paired with AI models for task-specific language processing.

### Key Features

- ✅ **60+ Custom Languages** organized in 6 categories
- ✅ **Custom Compilers** per language (user-built style)
- ✅ **Dynamic Loading** - Load compilers only when needed
- ✅ **Model Pairing** - Not all languages for all models
- ✅ **Compiler Caching** - Performance optimization
- ✅ **State Management** - Complete tracking and introspection
- ✅ **Multi-Level Reset** - Language, model-specific, full system
- ✅ **Performance Ratings** - Model support tiers (Primary/Secondary)

---

## Architecture

### System Components

```
Language-Model Registry System
├── language_model_registry.psm1      (Core registry module)
├── language_model_manager.ps1         (CLI management tool)
├── language_model_integration.ps1     (Making Station integration)
└── Documentation (this file)
```

### Data Structures

#### 1. Master Language Registry
- **File**: `$script:AllLanguages`
- **Contains**: 60 language entries
- **Per Entry**: Code, Category, Compiler file, Aliases, Features, Models, Tasks

#### 2. Model-Language Pairings
- **File**: `$script:ModelLanguagePairings`
- **Contains**: 5 model definitions (GPT-4, Claude-3, Llama-2, Qwen, Custom-Model-v1)
- **Per Model**: Supported languages (Primary/Secondary tiers), Performance rating

#### 3. Compiler Cache
- **File**: `$script:CompilerCache`
- **Purpose**: Store loaded compilers by Language-Model pair
- **Key Format**: "Language-ModelName"
- **Value**: {Language, Model, CompilerFile, LoadedAt, Features, Status}

#### 4. Language State
- **File**: `$script:LanguageState`
- **Purpose**: Track active language states per model
- **Updates**: On load/unload operations

---

## Language Categories

### Tier 1: European (15 languages)
- English, Spanish, French, German, Italian
- Portuguese, Russian, Polish, Dutch, Swedish
- Norwegian, Danish, Finnish, Icelandic, Estonian

### Tier 2: Asian (15 languages)
- Japanese, Chinese, Korean, Hindi, Thai
- Vietnamese, Indonesian, Tagalog, Malay, Turkish
- Bengali, Punjabi, Urdu, Gujarati, Marathi

### Tier 3: Middle Eastern & African (12 languages)
- Arabic, Hebrew, Persian, Swahili, Yoruba
- Amharic, Igbo, Hausa, Somali, Afrikaans
- Zulu, Xhosa

### Tier 4: European/Slavic (10 languages)
- Greek, Czech, Slovak, Hungarian, Romanian
- Bulgarian, Serbian, Croatian, Slovenian, Lithuanian

### Tier 5: Nordic/Baltic (5 languages)
- Norwegian, Danish, Finnish, Icelandic, Estonian

### Tier 6: Specialized (8 languages)
- Ukrainian, Latvian, Vietnamese-Modern
- Simplified-Chinese, Traditional-Chinese, Cantonese
- Taiwanese, Khmer, Lao

---

## Model Support

### Model Ratings & Language Support

#### GPT-4 (Rating: 0.95)
- **Primary Support** (5): English, Spanish, French, German, Chinese
- **Secondary Support** (5): Italian, Portuguese, Russian, Japanese, Arabic
- **Use Case**: General purpose, coding, reasoning

#### Claude-3 (Rating: 0.93)
- **Primary Support** (3): English, French, German
- **Secondary Support** (5): Spanish, Japanese, Portuguese, Korean, Italian
- **Use Case**: Analysis, documentation, creative tasks

#### Llama-2 (Rating: 0.88)
- **Primary Support** (1): English
- **Secondary Support** (9+): Spanish, French, German, Italian, Portuguese, Chinese, Japanese, Korean, Arabic
- **Use Case**: Fast inference, code completion

#### Qwen (Rating: 0.92)
- **Primary Support** (1): Chinese
- **Secondary Support** (3): English, Japanese, Korean
- **Use Case**: Multilingual processing, Asian languages

#### Custom-Model-v1 (Rating: 0.85)
- **Primary Support** (3): English, Spanish, Portuguese
- **Extensible**: Add more languages as needed
- **Use Case**: Custom domains and tasks

---

## Core Functions

### Registry Functions

#### `Get-AllAvailableLanguages`
Returns all 60+ languages in the registry.

```powershell
$allLangs = Get-AllAvailableLanguages
# Returns: [ordered]@{ "Language-Name" = @{ Code, Category, Compiler, ... }, ... }
```

#### `Get-LanguagesByCategory`
Filter languages by category.

```powershell
$euroLangs = Get-LanguagesByCategory -Category "European"
$asianLangs = Get-LanguagesByCategory -Category "Asian"
```

#### `Get-LanguagesForModel`
Get languages supported by a specific model.

```powershell
$gpt4Langs = Get-LanguagesForModel -ModelName "GPT-4"
# Returns: [PSCustomObject]@{ Language, Tier, LanguageData, Rating }
```

### Compiler Management Functions

#### `Load-LanguageForModel`
Load a compiler for a specific Language-Model pair.

```powershell
$result = Load-LanguageForModel -Language "Spanish" -ModelName "GPT-4" -CompilerPath "D:\compilers"
# Returns: @{ Language, Model, CompilerFile, Status, Features, LoadedAt }
```

#### `Unload-LanguageForModel`
Remove a loaded compiler from cache.

```powershell
$result = Unload-LanguageForModel -Language "Spanish" -ModelName "GPT-4"
# Returns: $true on success, $false if not loaded
```

#### `Get-LoadedLanguages`
List currently loaded compilers, optionally filtered by model.

```powershell
# Get all loaded
$all = Get-LoadedLanguages

# Get loaded for specific model
$gpt4Loaded = Get-LoadedLanguages -ModelName "GPT-4"
# Returns: Array of @{ Language, Model, LoadedAt, Features, Status }
```

### State Management Functions

#### `Get-LanguageCompilerInfo`
Get detailed information about a language's compiler.

```powershell
$info = Get-LanguageCompilerInfo -Language "Spanish"
# Returns: @{
#   Code = "es-ES"
#   Category = "European"
#   Compiler = "Spanish-Compiler-v1.0"
#   Aliases = @("Castellano", "Español")
#   Features = @("nlp", "text", "grammar")
#   SupportedModels = @("GPT-4", "Claude-3", "Llama-2")
#   OptimalTaskTypes = @("general", "documentation")
# }
```

#### `Get-LanguageState`
Get complete state snapshot of all loaded languages and models.

```powershell
$state = Get-LanguageState
# Returns: @{
#   LoadedCompilers = 5
#   ActiveLanguages = @("Spanish", "French", "German", ...)
#   Timestamp = "2024-01-15T10:30:45.1234567Z"
#   StateDetails = @{ Cache = {...}, LanguageState = {...}, ActiveLanguages = {...} }
# }
```

### Batch Operations

#### `Initialize-LanguageForModel`
Batch initialize multiple languages for one model.

```powershell
$result = Initialize-LanguageForModel `
  -ModelName "GPT-4" `
  -Languages @("English", "Spanish", "French", "German", "Italian") `
  -CompilerPath "D:\compilers"

# Returns: @{
#   Model = "GPT-4"
#   InitializedCount = 5
#   FailureCount = 0
#   SuccessfullyInitialized = @("English", "Spanish", "French", "German", "Italian")
#   Failed = @()
# }
```

### Reset Functions

#### `Reset-AllLanguages`
**COMPLETE RESET** - Clear all caches, states, and active languages.

```powershell
Reset-AllLanguages
# Clears: CompilerCache, LanguageState, ActiveLanguages
# WARNING: Destructive operation
```

#### `Reset-ModelLanguages`
Reset languages for a specific model only.

```powershell
$result = Reset-ModelLanguages -ModelName "GPT-4"
# Returns: @{
#   Status = "Success"
#   LanguagesUnloaded = 5
#   Message = "Successfully reset languages for GPT-4"
# }
```

#### `Reset-AllModels`
**FULL SYSTEM RESET** - Clear all models, languages, and states.

```powershell
$result = Reset-AllModels
# Returns: @{
#   Status = "Success"
#   Message = "Full system reset complete"
#   NextStep = "Reinitialize languages as needed"
# }
# WARNING: Complete system wipe
```

---

## Usage Workflows

### Workflow 1: Initialize Model with Languages

```powershell
# Import registry
Import-Module "D:\lazy init ide\scripts\language_model_registry.psm1"

# Get languages for model
$languages = Get-LanguagesForModel -ModelName "GPT-4"

# Initialize model with primary languages
$result = Initialize-LanguageForModel `
  -ModelName "GPT-4" `
  -Languages @("English", "Spanish", "French", "German", "Chinese") `
  -CompilerPath "D:\lazy init ide\compilers"

Write-Host "Loaded $($result.InitializedCount) languages"
```

### Workflow 2: Check Loaded Languages

```powershell
# Get all loaded languages
$loaded = Get-LoadedLanguages

# Get loaded for specific model
$gpt4Loaded = Get-LoadedLanguages -ModelName "GPT-4"

# Get system status
$status = Get-LanguageState
Write-Host "Total loaded: $($status.LoadedCompilers)"
Write-Host "Active languages: $($status.ActiveLanguages -join ', ')"
```

### Workflow 3: Switch Models and Languages

```powershell
# Reset GPT-4 languages
Reset-ModelLanguages -ModelName "GPT-4"

# Initialize Claude-3 languages
$result = Initialize-LanguageForModel `
  -ModelName "Claude-3" `
  -Languages @("English", "French", "German")
```

### Workflow 4: Get Language Information

```powershell
# Get all languages
$all = Get-AllAvailableLanguages
Write-Host "Total languages: $($all.Count)"

# Get by category
$european = Get-LanguagesByCategory -Category "European"
$asian = Get-LanguagesByCategory -Category "Asian"

# Get specific language details
$info = Get-LanguageCompilerInfo -Language "Spanish"
Write-Host "Compiler: $($info.Compiler)"
Write-Host "Features: $($info.Features -join ', ')"
```

### Workflow 5: Full System Reset

```powershell
# WARNING: This clears everything!

# Perform full reset
$result = Reset-AllModels

# Reinitialize as needed
$newResult = Initialize-LanguageForModel `
  -ModelName "GPT-4" `
  -Languages @("English", "Spanish", "French")
```

---

## Management Tools

### 1. CLI Manager Script: `language_model_manager.ps1`

Full-featured command-line tool for managing the registry.

#### Usage Examples

```powershell
# Get all languages
.\language_model_manager.ps1 -Action get-languages

# Get languages for a model
.\language_model_manager.ps1 -Action get-model-languages -Model GPT-4

# Load a language
.\language_model_manager.ps1 -Action load-lang -Language Spanish -Model GPT-4

# Get system status
.\language_model_manager.ps1 -Action get-status

# Reset all
.\language_model_manager.ps1 -Action reset-all

# Initialize model with multiple languages
.\language_model_manager.ps1 -Action initialize-model -Model GPT-4 -LanguageList "English,Spanish,French"
```

#### Actions
- `get-languages` - Show all available languages
- `get-languages-detailed` - Show detailed language info
- `get-models` - Show available models
- `get-model-languages` - Show languages for a model
- `load-lang` - Load a language compiler
- `unload-lang` - Unload a language compiler
- `get-loaded` - Show loaded languages
- `initialize-model` - Batch initialize languages
- `reset-lang` - Reset a language
- `reset-model` - Reset model languages
- `reset-all` - Full system reset
- `get-status` - Show system status
- `get-compiler-info` - Get language details

### 2. Making Station Integration: `language_model_integration.ps1`

Integration module for the Model/Agent Making Station.

#### Usage

```powershell
# Import module
Import-Module "D:\lazy init ide\scripts\language_model_integration.ps1"

# Show integration menu
Invoke-LanguageModelIntegrationMenu

# Or use specific functions
Initialize-LanguagesForModel
Show-ModelLanguageStatus
Show-AllLanguagesSummary
```

#### Menu Options
- `LM1` - Load languages for active model
- `LM2` - View loaded languages
- `LM3` - Unload language from model
- `LM4` - Get language details
- `LM5` - Show all languages
- `LM6` - Show languages by category
- `LM7` - Show languages for model
- `LM8` - Initialize model with languages
- `LM9` - Get model language status
- `LM10` - Reset all languages
- `LM11` - Reset languages for model
- `LM12` - Full system reset
- `LM13` - Show system status
- `LM14` - Get compiler cache info
- `LM15` - Validate compiler paths
- `LM16` - Launch CLI manager
- `LM17` - Show help

---

## Configuration

### Compiler Path

Default compiler location:
```powershell
$CompilerPath = "D:\lazy init ide\compilers"
```

Compiler naming convention:
```
<LanguageName>-Compiler-v<Version>

Examples:
- Spanish-Compiler-v1.0
- English-Compiler-v1.0
- Japanese-Compiler-v2.1
- French-Compiler-v1.5
```

### Customization

Edit `language_model_registry.psm1` to:

1. **Add new languages**:
```powershell
$script:AllLanguages["NewLanguage"] = @{
    Code = "nl-NL"
    Category = "European"
    Compiler = "NewLanguage-Compiler-v1.0"
    Aliases = @("alias1", "alias2")
    Features = @("nlp", "text", "speech")
    Models = @("GPT-4", "Claude-3", "Llama-2")
    TaskTypes = @("general", "coding")
}
```

2. **Update model-language pairings**:
```powershell
$script:ModelLanguagePairings["NewModel"] = @{
    Name = "NewModel"
    PrimaryLanguages = @("English", "Spanish")
    SecondaryLanguages = @("French", "German")
    Rating = 0.90
}
```

3. **Modify compiler cache behavior**:
- Edit `Load-LanguageForModel` function
- Adjust cache keys, retention policies

---

## Troubleshooting

### Issue: Language not found

**Solution**:
```powershell
# Check available languages
$all = Get-AllAvailableLanguages
$all.Keys -contains "YourLanguage"

# Check spelling and capitalization (case-sensitive)
```

### Issue: Compiler file not found

**Solution**:
```powershell
# Verify compiler path
Test-Path "D:\lazy init ide\compilers"

# Check compiler naming
Get-ChildItem "D:\lazy init ide\compilers" | ForEach-Object { $_.Name }

# Validate Load-LanguageForModel syntax
Load-LanguageForModel -Language "Spanish" -ModelName "GPT-4" -CompilerPath "D:\lazy init ide\compilers"
```

### Issue: State gets corrupted

**Solution**:
```powershell
# Perform full system reset
Reset-AllModels

# Reinitialize models
Initialize-LanguageForModel -ModelName "GPT-4" -Languages @("English", "Spanish")
```

### Issue: Too many languages loaded

**Solution**:
```powershell
# Check what's loaded
Get-LoadedLanguages

# Reset specific model
Reset-ModelLanguages -ModelName "GPT-4"

# Or reset all
Reset-AllLanguages
```

---

## Performance Considerations

### Loading Performance
- **First Load**: ~100-500ms per language (compiler file I/O)
- **Subsequent Loads**: <10ms (cached)

### Memory Impact
- **Per Cached Compiler**: ~50-200MB (depends on compiler size)
- **5 Loaded Languages**: ~250-1000MB
- **Cache Management**: Automatic cleanup on unload

### Optimization Tips
1. Load only needed languages for each model
2. Use batch initialization (`Initialize-LanguageForModel`) for multiple languages
3. Unload languages not currently in use
4. Reset periodically to clear cache

---

## Integration with Making Station

### Adding to Model Creation Workflow

```powershell
# In Model/Agent Making Station
1. User creates a model (e.g., "MyGPT-4-Instance")
2. System offers: "Initialize with languages? [Y/N]"
3. Show language options for that model
4. Initialize selected languages
5. Done - model ready for use
```

### Menu Integration

Adding to Making Station main menu:
```powershell
# Add option in Show-MakingStationDashboard
'LM' { Invoke-LanguageModelIntegrationMenu }

# Or specific actions
'L1' { Initialize-LanguagesForModel }
'L2' { Show-ModelLanguageStatus }
```

---

## API Reference

### Quick Reference Table

| Function | Purpose | Returns |
|----------|---------|---------|
| `Get-AllAvailableLanguages` | Get all 60 languages | [ordered]@{} with language data |
| `Get-LanguagesByCategory` | Filter by category | Array of language entries |
| `Get-LanguagesForModel` | Get model's languages | Array of @{Language, Tier, Data} |
| `Load-LanguageForModel` | Load compiler | @{Language, Model, Status, ...} |
| `Unload-LanguageForModel` | Unload compiler | $true/$false |
| `Get-LoadedLanguages` | List loaded | Array of @{Language, Model, LoadedAt} |
| `Get-LanguageCompilerInfo` | Language details | @{Code, Category, Compiler, ...} |
| `Get-LanguageState` | System state | @{LoadedCompilers, ActiveLanguages, ...} |
| `Initialize-LanguageForModel` | Batch init | @{Model, InitializedCount, Success, Failed} |
| `Reset-AllLanguages` | Complete reset | Clears all state |
| `Reset-ModelLanguages` | Model reset | @{Status, LanguagesUnloaded} |
| `Reset-AllModels` | Full system reset | @{Status, Message} |

---

## Advanced Topics

### Custom Compiler Integration

To add a custom compiler:

1. Place compiler executable in: `D:\lazy init ide\compilers\`
2. Name it: `<Language>-Compiler-v<Version>`
3. Add language to registry:
   ```powershell
   $script:AllLanguages["CustomLang"] = @{
       Code = "cl-CL"
       Category = "Specialized"
       Compiler = "CustomLang-Compiler-v1.0"
       # ... other fields
   }
   ```
4. Update model pairings if needed
5. Load with: `Load-LanguageForModel -Language "CustomLang" -ModelName "GPT-4"`

### Monitoring and Logging

```powershell
# Log all loaded languages
Get-LoadedLanguages | Export-Csv "D:\logs\loaded_languages.csv"

# Get cache statistics
$status = Get-LanguageState
Write-Host "Compilers in cache: $($status.LoadedCompilers)"

# Track model usage
Get-LanguagesForModel -ModelName "GPT-4" | Measure-Object
```

### Extending for New Models

```powershell
# Add new model to pairings
$script:ModelLanguagePairings["NewModel"] = @{
    Name = "NewModel"
    PrimaryLanguages = @("English", "Spanish", "French")
    SecondaryLanguages = @("German", "Italian", "Portuguese")
    Rating = 0.92
}

# Initialize when ready
Initialize-LanguageForModel -ModelName "NewModel" -Languages @("English", "Spanish")
```

---

## Support & Resources

- **Registry Module**: `D:\lazy init ide\scripts\language_model_registry.psm1`
- **CLI Manager**: `D:\lazy init ide\scripts\language_model_manager.ps1`
- **Integration**: `D:\lazy init ide\scripts\language_model_integration.ps1`
- **Config**: `D:\lazy init ide\logs\swarm_config\`
- **Compilers**: `D:\lazy init ide\compilers\`

---

## Version History

### v1.0 (Current)
- Initial release with 60+ languages
- 5 model pairings
- Dynamic loading/unloading
- Multi-level reset system
- CLI manager tool
- Making Station integration

---

## License & Attribution

**Language-Model Registry System**
- Custom implementation for user's 60+ languages
- Built for dynamic model-language pairing
- Supports user's proprietary compilers
- Designed for task-specific language selection

---

**Last Updated**: January 2024
**Documentation Version**: 1.0
