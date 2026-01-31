# 🎯 Quick Reference - Unified AI Enhancement System

## 🚀 Start Here

```powershell
# Launch the complete system
.\unified_system_launcher.ps1
```

---

## 🌍 Translation System - Cheat Sheet

| Function | Purpose | Example |
|----------|---------|---------|
| `Invoke-ModelWithTranslation` | **Main function** - translate + process + translate back | `Invoke-ModelWithTranslation -Text "Hola" -TargetLanguage "French"` |
| `Detect-Language` | Auto-detect language | `Detect-Language -Text "Bonjour"` |
| `Invoke-LanguageTranslation` | Pure translation | `Invoke-LanguageTranslation -Text "Hi" -SourceLanguage "English" -TargetLanguage "Spanish"` |
| `Show-LanguageSupport` | List all languages | `Show-LanguageSupport` |
| `Get-SessionContext` | View history | `Get-SessionContext` |
| `Clear-TranslationCache` | Free memory | `Clear-TranslationCache` |

### Common Usage
```powershell
# Spanish input → English output
Invoke-ModelWithTranslation -Text "¿Cuál es el significado de la vida?" `
    -TargetLanguage "English" -ModelName "GPT-4"

# Detect language first
$lang = Detect-Language -Text "Hola mundo"
# Returns: Spanish

# See all supported languages
Show-LanguageSupport
```

---

## 🔧 Reverse Engineering - Cheat Sheet

| Function | Purpose | Example |
|----------|---------|---------|
| `Invoke-PEAnalysis` | **Main function** - complete binary analysis | `Invoke-PEAnalysis -BinaryPath "C:\lib.dll"` |
| `Read-PEHeaders` | Get header info | `Read-PEHeaders -BinaryPath "lib.dll"` |
| `Extract-PEExports` | Get exported functions | `Extract-PEExports -BinaryPath "lib.dll"` |
| `Extract-PEImports` | Get imported functions | `Extract-PEImports -BinaryPath "lib.dll"` |
| `Reconstruct-FunctionSignatures` | Get function signatures | `Reconstruct-FunctionSignatures -BinaryPath "lib.dll"` |
| `Generate-HeaderFile` | Create .h file | `Generate-HeaderFile -BinaryPath "lib.dll" -OutputPath "lib.h"` |
| `Generate-SourceSkeleton` | Create .c file | `Generate-SourceSkeleton -BinaryPath "lib.dll" -OutputPath "lib_stub.c"` |
| `Start-InteractiveAnalysis` | Interactive menu | `Start-InteractiveAnalysis` |

### Common Workflows
```powershell
# 1. Analyze binary completely
$analysis = Invoke-PEAnalysis -BinaryPath "mysterious.dll"

# 2. Generate usable C code
$header = Generate-HeaderFile -BinaryPath "mysterious.dll"
$source = Generate-SourceSkeleton -BinaryPath "mysterious.dll"

# 3. Get specific exports
$exports = Extract-PEExports -BinaryPath "kernel32.dll"

# 4. Interactive analysis
Start-InteractiveAnalysis
```

---

## 🎨 Plugin Craft Room - Cheat Sheet

| Function | Purpose | Example |
|----------|---------|---------|
| `New-CraftRoomPlugin` | **Create plugin** | `New-CraftRoomPlugin -Name "MyPlugin" -Type "Translator"` |
| `Start-PluginCraftRoom` | Interactive menu | `Start-PluginCraftRoom` |
| `Show-PluginTemplates` | List templates | `Show-PluginTemplates` |
| `Show-CreatedPlugins` | List your plugins | `Show-CreatedPlugins` |
| `Import-CraftPlugin` | Load plugin | `Import-CraftPlugin -PluginName "MyPlugin"` |
| `Remove-CraftPlugin` | Delete plugin | `Remove-CraftPlugin -PluginName "MyPlugin" -Force` |

### Plugin Types
- **Translator** - Multi-language translation
- **Analyzer** - Data analysis
- **Connector** - API/service connections
- **Generator** - Code/content generation
- **Processor** - Data processing pipelines
- **Custom** - Your own implementation

### Quick Plugin Creation
```powershell
# Translator plugin (most common)
New-CraftRoomPlugin -Name "TranslatorX" -Type "Translator" `
    -Parameters @{ Languages = @("English","Spanish","French") }

# API Connector
New-CraftRoomPlugin -Name "GitHubAPI" -Type "Connector" `
    -Parameters @{ ServiceName = "GitHub"; Endpoint = "https://api.github.com" }

# Data Analyzer
New-CraftRoomPlugin -Name "SentimentAnalyzer" -Type "Analyzer" `
    -Parameters @{ AnalysisType = "Sentiment" }

# Interactive creation
Start-PluginCraftRoom
# Then use menu to create
```

---

## 📊 System Status Command

```powershell
# From launcher menu, select option [10]
# Or manually:
Get-SessionContext  # Translator stats
Show-CreatedPlugins # Your plugins
Show-PluginTemplates # Available templates
```

---

## 🔄 Common Integration Patterns

### Pattern 1: Translate Then Analyze
```powershell
# Ask question in any language
$result = Invoke-ModelWithTranslation `
    -Text "Was ist eine DLL?" `
    -TargetLanguage "English"

# Use answer to guide analysis
$binary = Invoke-PEAnalysis -BinaryPath "something.dll"
```

### Pattern 2: Create Language-Specific Plugin
```powershell
# Create translator for specific language pair
$plugin = New-CraftRoomPlugin `
    -Name "SpanishPortuguese" `
    -Type "Translator" `
    -Parameters @{ Languages = @("Spanish","Portuguese","English") }

Import-CraftPlugin -PluginName "SpanishPortuguese"
```

### Pattern 3: Analyze Then Generate Plugin
```powershell
# Analyze a DLL
$analysis = Invoke-PEAnalysis -BinaryPath "mylib.dll"

# Create a connector plugin for it
$plugin = New-CraftRoomPlugin `
    -Name "MyLibConnector" `
    -Type "Connector" `
    -Parameters @{ ServiceName = "mylib"; Endpoint = "mylib.dll" }
```

---

## 🎯 Top 5 Use Cases

### 1. Multi-Language Chat
```powershell
Invoke-ModelWithTranslation -Text "Spanish question" -TargetLanguage "French"
```
**Result**: Automatic translation through any model

### 2. Reverse Engineer DLLs
```powershell
Invoke-PEAnalysis -BinaryPath "mystery.dll"
Generate-HeaderFile -BinaryPath "mystery.dll"
```
**Result**: Complete C code skeleton

### 3. Create Custom Translator
```powershell
New-CraftRoomPlugin -Name "MyTranslator" -Type "Translator" `
    -Parameters @{ Languages = @("Portuguese","Spanish") }
```
**Result**: Ready-to-use translation plugin

### 4. API Integration
```powershell
New-CraftRoomPlugin -Name "SlackAPI" -Type "Connector" `
    -Parameters @{ ServiceName = "Slack"; Endpoint = "https://slack.com/api" }
```
**Result**: Plug-and-play API connector

### 5. Data Processing Pipeline
```powershell
New-CraftRoomPlugin -Name "DataETL" -Type "Processor" `
    -Parameters @{ ProcessType = "ETL" }
```
**Result**: Reusable data pipeline

---

## 📂 File Locations

```
D:\lazy init ide\
  ├── scripts\
  │   ├── model_translator_engine.psm1
  │   ├── codex_accessibility_layer.psm1
  │   └── plugin_craft_room.psm1
  ├── craft_room\
  │   ├── templates\
  │   └── plugins\
  ├── unified_system_launcher.ps1
  └── UNIFIED_SYSTEM_GUIDE.md (full guide)

D:\CodexReverseEngine\
  ├── CodexUltimate.exe
  └── codex_reverse_engine.c
```

---

## ⚡ Speed Tips

| Task | Fastest Way |
|------|-------------|
| Translate text | `Invoke-ModelWithTranslation -Text "X" -TargetLanguage "Y"` |
| Analyze binary | `Invoke-PEAnalysis -BinaryPath "file.dll"` |
| Create plugin | `Start-PluginCraftRoom` (interactive) |
| Load plugin | `Import-CraftPlugin -PluginName "X"` |
| See all languages | `Show-LanguageSupport` |
| See all plugins | `Show-CreatedPlugins` |

---

## 🐛 Quick Fixes

| Problem | Solution |
|---------|----------|
| Translation not working | `Clear-TranslationCache` then retry |
| Binary analysis fails | Check file path and permissions |
| Plugin won't load | Verify name with `Show-CreatedPlugins` |
| Memory issues | `Clear-TranslationCache` (clears translation cache) |
| Not sure what to do | Run `.\unified_system_launcher.ps1` and explore menu |

---

## 📞 Getting Help

```powershell
# View full documentation
Get-Content "D:\lazy init ide\UNIFIED_SYSTEM_GUIDE.md"

# List all functions
Get-Command -Module model_translator_engine
Get-Command -Module codex_accessibility_layer
Get-Command -Module plugin_craft_room

# Get function help
Get-Help Invoke-ModelWithTranslation -Detailed
Get-Help Invoke-PEAnalysis -Detailed
Get-Help New-CraftRoomPlugin -Detailed
```

---

## ✅ Verification Checklist

After setup, verify everything works:

- [ ] Translation engine loads: `Import-Module model_translator_engine.psm1`
- [ ] Codex layer loads: `Import-Module codex_accessibility_layer.psm1`
- [ ] Craft room loads: `Import-Module plugin_craft_room.psm1`
- [ ] Translator works: `Show-LanguageSupport`
- [ ] Codex works: `Invoke-PEAnalysis -BinaryPath "notepad.exe"`
- [ ] Plugins work: `Show-PluginTemplates`
- [ ] Launcher works: `.\unified_system_launcher.ps1`

---

**Version**: 1.0 Quick Reference  
**Status**: Ready to Use ✅  
**See**: `UNIFIED_SYSTEM_GUIDE.md` for complete documentation
