# 🎯 Unified AI Enhancement System - Complete Guide

## Overview

Three powerful systems integrated into one complete solution:

1. **Multi-Language Translator Engine** - Models understand any language, respond in any language
2. **Codex Accessibility Layer** - Full reverse engineering framework made easy
3. **Plugin Craft Room** - Create custom plugins on-the-fly for anything

---

## 🌍 System 1: Multi-Language Translator Engine

### What It Does
Automatically detects input language, processes through AI models, and responds in target language. Users never need to specify language pairs - the system handles everything transparently.

### Core Functions

#### `Detect-Language`
Auto-detect input language from text.

```powershell
$detectedLang = Detect-Language -Text "Hola, ¿cómo estás?"
# Returns: "Spanish"
```

#### `Invoke-LanguageTranslation`
Translate between languages with caching.

```powershell
$translated = Invoke-LanguageTranslation -Text "Hello" `
    -SourceLanguage "English" `
    -TargetLanguage "Spanish"
# Returns: "Hola"
```

#### `Invoke-ModelWithTranslation` ⭐ PRIMARY FUNCTION
**The main function** - 3-stage pipeline that handles everything:
1. Auto-detect input language
2. Translate input to model's primary language
3. Process through model
4. Translate output back to user's target language

```powershell
# Spanish input → GPT-4 → French output
$result = Invoke-ModelWithTranslation `
    -Text "¿Cuál es el significado de la vida?" `
    -TargetLanguage "French" `
    -ModelName "GPT-4"
```

#### `Show-LanguageSupport`
Display all supported languages.

```powershell
Show-LanguageSupport
# Lists: English, Spanish, French, German, Japanese, Chinese, Arabic, etc. (18+ languages)
```

#### `Get-SessionContext`
View translation history and preferences.

```powershell
$context = Get-SessionContext
# Returns: Last 50 conversations, language preferences, model usage stats
```

### Supported Languages
- **European**: English, Spanish, French, German, Italian, Portuguese, Russian, Polish
- **Asian**: Chinese (Simplified & Traditional), Japanese, Korean, Thai, Vietnamese
- **Middle Eastern**: Arabic, Hebrew
- **Other**: Indonesian, Dutch, Swedish

### How It Works

```
User Input (Any Language)
         ↓
    Language Detection
    (pattern matching)
         ↓
    Translate to Model's
    Primary Language
         ↓
    Model Inference
    (GPT-4, Claude, etc.)
         ↓
    Translate to Target
    Language
         ↓
    User Output (Target Language)
```

### Example Workflows

**Workflow 1: Multi-Lingual Conversation**
```powershell
# German speaker using Japanese model
$response1 = Invoke-ModelWithTranslation `
    -Text "Was ist maschinelles Lernen?" `
    -TargetLanguage "English" `
    -ModelName "Qwen"

# French speaker using English model
$response2 = Invoke-ModelWithTranslation `
    -Text "Comment fonctionne l'IA?" `
    -TargetLanguage "Spanish" `
    -ModelName "GPT-4"
```

**Workflow 2: Language Detection + Analysis**
```powershell
$userInput = "おはようございます"
$detected = Detect-Language -Text $userInput
# Returns: "Japanese"

$response = Invoke-ModelWithTranslation `
    -Text $userInput `
    -TargetLanguage "English"
```

### Performance Tips
- Translations are cached - repeated language pairs are instant
- Session context limited to 50 conversations (configurable)
- Clear cache when memory is critical: `Clear-TranslationCache`

---

## 🔧 System 2: Codex Accessibility Layer

### What It Does
Wraps the professional Codex reverse engineering framework in an easy-to-use PowerShell interface. Analyze PE binaries, extract signatures, generate C headers and source code.

### Core Functions

#### `Invoke-PEAnalysis` ⭐ PRIMARY FUNCTION
Complete PE binary analysis in one call.

```powershell
$analysis = Invoke-PEAnalysis -BinaryPath "C:\Windows\System32\kernel32.dll"
# Returns: Headers, sections, exports, imports, signatures
```

#### `Read-PEHeaders`
Extract PE header information.

```powershell
$headers = Read-PEHeaders -BinaryPath "mylib.dll"
# Returns: DOS header, NT headers, machine type, sections count
```

#### `Analyze-PESections`
Analyze all PE sections (.text, .data, .rsrc, etc.).

```powershell
$sections = Analyze-PESections -BinaryPath "mylib.dll"
# Returns: Section names, sizes, characteristics
```

#### `Extract-PEExports`
Get all exported functions from a DLL.

```powershell
$exports = Extract-PEExports -BinaryPath "mylib.dll"
# Returns: [ExportedFunc1, ExportedFunc2, ...]
```

#### `Extract-PEImports`
Get all imported DLL functions.

```powershell
$imports = Extract-PEImports -BinaryPath "mylib.dll"
# Returns: @{kernel32.dll = [func1, func2...], user32.dll = [...]...}
```

#### `Reconstruct-FunctionSignatures`
Infer function signatures from binary.

```powershell
$signatures = Reconstruct-FunctionSignatures -BinaryPath "mylib.dll"
# Returns: Function names with inferred parameters and return types
```

#### `Infer-CallingConvention`
Determine calling convention (cdecl, stdcall, fastcall, thiscall).

```powershell
$convention = Infer-CallingConvention -FunctionName "MyFunction" -BinaryPath "mylib.dll"
# Returns: "stdcall" or "cdecl" or "fastcall"
```

#### `Generate-HeaderFile` ⭐ CODE GENERATION
Generate C header file from binary analysis.

```powershell
$headerPath = Generate-HeaderFile `
    -BinaryPath "mylib.dll" `
    -OutputPath "mylib.h"
# Creates: mylib.h with all exports/imports
```

#### `Generate-SourceSkeleton` ⭐ CODE GENERATION
Generate C source skeleton with function stubs.

```powershell
$sourcePath = Generate-SourceSkeleton `
    -BinaryPath "mylib.dll" `
    -OutputPath "mylib_stub.c"
# Creates: mylib_stub.c with all functions stubbed
```

#### `Start-InteractiveAnalysis`
Launch interactive 8-option analysis menu.

```powershell
Start-InteractiveAnalysis
# Menu:
# [1] Analyze PE Binary
# [2] View PE Headers
# [3] List Exports
# [4] List Imports
# [5] Extract Signatures
# [6] Generate Header
# [7] Generate Source
# [8] Exit
```

### PE Analysis Workflow

```
Binary File (DLL/EXE)
       ↓
PE Header Parsing
       ↓
Section Analysis
       ↓
Export/Import Extraction
       ↓
Signature Reconstruction
       ↓
Calling Convention Inference
       ↓
Code Generation (Headers + Source)
```

### Example Workflows

**Workflow 1: Complete Binary Reverse Engineering**
```powershell
# Analyze a mysterious DLL
$analysis = Invoke-PEAnalysis -BinaryPath "C:\unknown.dll"

# Generate header file for inclusion in your project
$headerPath = Generate-HeaderFile -BinaryPath "C:\unknown.dll"

# Generate source skeleton for implementation
$sourcePath = Generate-SourceSkeleton -BinaryPath "C:\unknown.dll"

# Now you have complete C code to work with!
```

**Workflow 2: Understanding System DLLs**
```powershell
# Analyze Windows kernel DLL
$exports = Extract-PEExports -BinaryPath "C:\Windows\System32\kernel32.dll"
# Shows all kernel functions you can call

# Get detailed signatures
$signatures = Reconstruct-FunctionSignatures -BinaryPath "C:\Windows\System32\kernel32.dll"
# Shows function parameters and return types
```

**Workflow 3: Interactive Analysis**
```powershell
# Launch the interactive menu for non-scripting users
Start-InteractiveAnalysis
# Browse through analysis options, generate code on-the-fly
```

### Output Formats

**Generated Header File Format**:
```c
#ifndef MYLIB_H
#define MYLIB_H

// Exports from mylib.dll
typedef int (__stdcall *PFUNC_DoSomething)(int param1, const char* param2);

int __stdcall DoSomething(int param1, const char* param2);

// Imports from kernel32.dll
...

#endif
```

**Generated Source Format**:
```c
#include "mylib.h"

int __stdcall DoSomething(int param1, const char* param2) {
    // TODO: Implement function
    return 0;
}
```

---

## 🎨 System 3: Plugin Craft Room

### What It Does
Dynamically create custom plugins on-the-fly for any purpose. No compilation needed - creates ready-to-use PowerShell modules instantly.

### Plugin Templates

**Translator Plugin**
```powershell
New-CraftRoomPlugin -Name "MyTranslator" `
    -Type "Translator" `
    -Parameters @{ 
        Languages = @("English", "Spanish", "French", "German") 
    }
# Creates: plugin_craft_room/plugins/MyTranslator.psm1
```

**Analyzer Plugin**
```powershell
New-CraftRoomPlugin -Name "DataAnalyzer" `
    -Type "Analyzer" `
    -Parameters @{ 
        AnalysisType = "Statistical" 
    }
# Creates: data analysis pipeline
```

**Connector Plugin**
```powershell
New-CraftRoomPlugin -Name "APIConnector" `
    -Type "Connector" `
    -Parameters @{ 
        ServiceName = "GitHub"
        Endpoint = "https://api.github.com"
    }
# Creates: API connection handler
```

**Generator Plugin**
```powershell
New-CraftRoomPlugin -Name "CodeGenerator" `
    -Type "Generator" `
    -Parameters @{ 
        OutputType = "C#" 
    }
# Creates: code generation pipeline
```

**Processor Plugin**
```powershell
New-CraftRoomPlugin -Name "DataProcessor" `
    -Type "Processor" `
    -Parameters @{ 
        ProcessType = "ETL" 
    }
# Creates: data processing pipeline
```

**Custom Plugin**
```powershell
New-CraftRoomPlugin -Name "CustomTool" -Type "Custom"
# Creates: blank skeleton for your own implementation
```

### Core Functions

#### `New-CraftRoomPlugin`
Create new plugin from template.

```powershell
$pluginPath = New-CraftRoomPlugin `
    -Name "MyPlugin" `
    -Type "Analyzer" `
    -Parameters @{ AnalysisType = "Sentiment" }
```

#### `Show-PluginTemplates`
Display all available templates.

```powershell
Show-PluginTemplates
# Lists: Translator, Analyzer, Connector, Generator, Processor, Custom
```

#### `Show-CreatedPlugins`
List all created plugins.

```powershell
Show-CreatedPlugins
# Shows creation date, type, path for each plugin
```

#### `Import-CraftPlugin`
Load a created plugin into current session.

```powershell
Import-CraftPlugin -PluginName "MyTranslator"
# Now you can use all exported functions from MyTranslator
```

#### `Remove-CraftPlugin`
Delete a plugin.

```powershell
Remove-CraftPlugin -PluginName "MyTranslator" -Force
# Removes from disk and registry
```

#### `Start-PluginCraftRoom` ⭐ INTERACTIVE MODE
Launch interactive plugin creation menu.

```powershell
Start-PluginCraftRoom
# Full menu:
# [1] Create New Plugin
# [2] View Available Templates
# [3] List Created Plugins
# [4] Load Plugin
# [5] Delete Plugin
# [6-10] Quick creation shortcuts
# [0] Exit
```

### Craft Room Workflow

```
User Request
    ↓
Choose Template Type
    ↓
Specify Parameters
    ↓
Generate Plugin Code
    ↓
Save to Disk
    ↓
Register in System
    ↓
Ready to Use!
```

### Example Plugin Creation Workflows

**Workflow 1: Create Multi-Language Translator Plugin**
```powershell
$plugin = New-CraftRoomPlugin `
    -Name "SpanishToFrench" `
    -Type "Translator" `
    -Parameters @{ 
        Languages = @("Spanish", "French", "English") 
    }

# Now import and use
Import-CraftPlugin -PluginName "SpanishToFrench"

# Use exported functions
$translated = Translate-Text -Text "Hola" `
    -SourceLanguage "Spanish" `
    -TargetLanguage "French"
```

**Workflow 2: Create API Connector Plugin**
```powershell
$plugin = New-CraftRoomPlugin `
    -Name "GitHubConnector" `
    -Type "Connector" `
    -Parameters @{ 
        ServiceName = "GitHub"
        Endpoint = "https://api.github.com"
    }

Import-CraftPlugin -PluginName "GitHubConnector"

# Use to connect and make API calls
Connect-Service
$repos = Invoke-ServiceCall -Method "GetUserRepos"
Disconnect-Service
```

**Workflow 3: Create Custom Data Processor**
```powershell
$plugin = New-CraftRoomPlugin `
    -Name "CSVProcessor" `
    -Type "Processor" `
    -Parameters @{ 
        ProcessType = "CSV" 
    }

Import-CraftPlugin -PluginName "CSVProcessor"

# Build processing pipeline
Add-PipelineStage -StageName "LoadData" `
    -StageLogic { param($InputData) 
        Import-Csv $InputData 
    }

Add-PipelineStage -StageName "FilterData" `
    -StageLogic { param($InputData) 
        $InputData | Where-Object { $_.Active -eq $true } 
    }

# Run pipeline
$result = Invoke-Pipeline -InputData "data.csv"
```

**Workflow 4: Interactive Plugin Creation**
```powershell
Start-PluginCraftRoom
# [1] Create New Plugin
# Name: SentimentAnalyzer
# Type: Analyzer
# [Done] Plugin created!

# [3] List Created Plugins
# Shows: SentimentAnalyzer (created just now)

# [4] Load Plugin
# Name: SentimentAnalyzer
# [Done] Module loaded!

# Now use the analyzer's exported functions
```

### Plugin Registry

All created plugins are automatically registered with metadata:

```powershell
$registry = Show-CreatedPlugins

# Shows:
# ✓ MyTranslator
#   Type: Translator
#   Path: D:\lazy init ide\craft_room\plugins\MyTranslator.psm1
#   Created: 2024-01-15 14:32:50
```

---

## 🚀 Unified System Launcher

Launch all three systems from one master control interface:

```powershell
.\unified_system_launcher.ps1
```

### Main Menu Options

**Translation System (Options 1-3)**
- Multi-language model translation
- Show supported languages
- Detect input language

**Reverse Engineering (Options 4-6)**
- Codex interactive analysis
- Analyze PE binary
- Generate C header file

**Plugin Creation (Options 7-9)**
- Plugin craft room (interactive)
- Create quick plugin
- View plugin templates

**System (Option 10)**
- Show system status
- Display statistics
- Show file locations

---

## 📋 Integration Examples

### Example 1: Translate & Reverse Engineer

```powershell
# 1. Translate a question to English
$englishQuestion = Invoke-ModelWithTranslation `
    -Text "Was ist eine DLL?" `
    -TargetLanguage "English"

# 2. Use that knowledge to reverse engineer
$analysis = Invoke-PEAnalysis -BinaryPath "mylib.dll"

# 3. Generate header file
$header = Generate-HeaderFile -BinaryPath "mylib.dll"

# 4. Create a plugin that uses the header
$plugin = New-CraftRoomPlugin `
    -Name "MyDLLWrapper" `
    -Type "Connector" `
    -Parameters @{ 
        ServiceName = "mylib"
        Endpoint = "mylib.dll"
    }
```

### Example 2: Multi-Language Binary Analysis

```powershell
# German speaker analyzing C++ library
$germanQuestion = "Was sind die Funktionen dieser DLL?"

$englishExplanation = Invoke-ModelWithTranslation `
    -Text $germanQuestion `
    -TargetLanguage "English"

# Now analyze the binary
$exports = Extract-PEExports -BinaryPath "cpp_lib.dll"

# Generate code in C# (different language than C header)
$plugin = New-CraftRoomPlugin `
    -Name "CppLibWrapper" `
    -Type "Generator" `
    -Parameters @{ OutputType = "CSharp" }

# Translate explanation back to German
$germanResponse = Invoke-LanguageTranslation `
    -Text $englishExplanation `
    -SourceLanguage "English" `
    -TargetLanguage "German"
```

### Example 3: Create Language-Specific Plugin

```powershell
# User speaks Portuguese, wants Spanish plugin
$plugin = New-CraftRoomPlugin `
    -Name "PortugueseToSpanish" `
    -Type "Translator" `
    -Parameters @{ 
        Languages = @("Portuguese", "Spanish", "English") 
    }

# Import it
Import-CraftPlugin -PluginName "PortugueseToSpanish"

# Use for translations
$spanish = Translate-Text `
    -Text "Olá, mundo!" `
    -SourceLanguage "Portuguese" `
    -TargetLanguage "Spanish"
```

---

## 🎓 Quick Start Cheat Sheet

### Start Everything
```powershell
.\unified_system_launcher.ps1
```

### Translate Text
```powershell
Invoke-ModelWithTranslation -Text "Hello" -TargetLanguage "Spanish"
```

### Analyze DLL
```powershell
Invoke-PEAnalysis -BinaryPath "mylib.dll"
```

### Create Plugin
```powershell
New-CraftRoomPlugin -Name "MyPlugin" -Type "Translator"
```

### Interactive Mode
```powershell
Start-PluginCraftRoom
```

---

## 📁 File Locations

- **Scripts**: `D:\lazy init ide\scripts\`
- **Modules**:
  - `model_translator_engine.psm1`
  - `codex_accessibility_layer.psm1`
  - `plugin_craft_room.psm1`
- **Craft Room**: `D:\lazy init ide\craft_room\`
- **Plugins**: `D:\lazy init ide\craft_room\plugins\`
- **Codex**: `D:\CodexReverseEngine\`
- **Launcher**: `unified_system_launcher.ps1`

---

## 🆘 Troubleshooting

**Translation not working?**
- Check language spelling: `Show-LanguageSupport`
- Clear cache: `Clear-TranslationCache`
- Verify model is available: `Get-ModelPrimaryLanguage -ModelName "GPT-4"`

**Binary analysis failing?**
- Ensure file path is correct and file is readable
- Check if it's a valid PE binary
- Try: `Invoke-PEAnalysis -BinaryPath $fullPath -Verbose`

**Plugin creation issues?**
- Verify plugin name is unique
- Check Craft Room directory exists: `D:\lazy init ide\craft_room\`
- Remove corrupted plugin: `Remove-CraftPlugin -PluginName "BadPlugin" -Force`

---

## 🔐 Security Notes

- Craft Room plugins are executed with current user privileges
- Translate cache is stored in memory (session-based)
- PE analysis requires read access to binary files
- Always review generated code before executing

---

**Version**: 1.0  
**Last Updated**: 2024  
**Status**: Production Ready ✅
