#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Unified System Launcher - Integration Hub for All Three Systems
    Multi-Language Translator + Codex Reverse Engineering + Plugin Craft Room

.DESCRIPTION
    Master control center for:
    1. Multi-Language Model Translation
    2. Codex Reverse Engineering Framework
    3. Plugin Craft Room - On-the-Fly Extension Creation

.EXAMPLE
    .\unified_system_launcher.ps1
#>

param()

# Import all three systems
$ScriptRoot = "D:\lazy init ide\scripts"

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  🚀 UNIFIED SYSTEM LAUNCHER - AI Enhancement Hub               ║" -ForegroundColor Cyan
Write-Host "║  Version 1.0 - Complete Integration                            ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

Write-Host "📦 Loading Systems..." -ForegroundColor Yellow
Write-Host ""

# Load modules
try {
    Write-Host "  [1/3] Loading Model Translator Engine..." -ForegroundColor Cyan
    Import-Module (Join-Path $ScriptRoot "model_translator_engine.psm1") -Force -DisableNameChecking
    Write-Host "        ✅ Translator ready" -ForegroundColor Green
    
    Write-Host "  [2/3] Loading Codex Accessibility Layer..." -ForegroundColor Cyan
    Import-Module (Join-Path $ScriptRoot "codex_accessibility_layer.psm1") -Force -DisableNameChecking
    Write-Host "        ✅ Codex framework ready" -ForegroundColor Green
    
    Write-Host "  [3/3] Loading Plugin Craft Room..." -ForegroundColor Cyan
    Import-Module (Join-Path $ScriptRoot "plugin_craft_room.psm1") -Force -DisableNameChecking
    Write-Host "        ✅ Craft Room ready" -ForegroundColor Green
    
    Write-Host ""
    Write-Host "✅ ALL SYSTEMS LOADED" -ForegroundColor Green
    
} catch {
    Write-Host ""
    Write-Host "❌ ERROR Loading Systems:" -ForegroundColor Red
    Write-Host "   $_" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "═" * 70
Write-Host ""

# Main menu
while ($true) {
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  🎯 UNIFIED AI ENHANCEMENT SYSTEM                              ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "Main Menu:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  TRANSLATION SYSTEM:" -ForegroundColor Cyan
    Write-Host "  [1] Multi-Language Model Translation" -ForegroundColor White
    Write-Host "  [2] Show Supported Languages" -ForegroundColor White
    Write-Host "  [3] Detect Input Language" -ForegroundColor White
    Write-Host ""
    
    Write-Host "  REVERSE ENGINEERING:" -ForegroundColor Cyan
    Write-Host "  [4] Codex Interactive Analysis" -ForegroundColor White
    Write-Host "  [5] Analyze PE Binary" -ForegroundColor White
    Write-Host "  [6] Generate C Header File" -ForegroundColor White
    Write-Host ""
    
    Write-Host "  PLUGIN CREATION:" -ForegroundColor Cyan
    Write-Host "  [7] Plugin Craft Room" -ForegroundColor White
    Write-Host "  [8] Create Quick Plugin" -ForegroundColor White
    Write-Host "  [9] View Plugin Templates" -ForegroundColor White
    Write-Host ""
    
    Write-Host "  SYSTEM:" -ForegroundColor Cyan
    Write-Host "  [10] System Status" -ForegroundColor White
    Write-Host "  [0] Exit" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "Selection: " -NoNewline -ForegroundColor Cyan
    $choice = Read-Host
    
    switch ($choice) {
        '1' {
            Write-Host ""
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host "MULTI-LANGUAGE MODEL TRANSLATION" -ForegroundColor Cyan
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host ""
            
            $text = Read-Host "Enter text"
            Write-Host ""
            Show-LanguageSupport
            Write-Host ""
            $targetLang = Read-Host "Target language"
            
            try {
                Write-Host ""
                $result = Invoke-ModelWithTranslation -Text $text -TargetLanguage $targetLang -ModelName "GPT-4"
                Write-Host ""
                Write-Host "📝 Result: $result" -ForegroundColor Green
                Write-Host ""
            } catch {
                Write-Host "❌ Error: $_" -ForegroundColor Red
            }
        }
        
        '2' {
            Show-LanguageSupport
        }
        
        '3' {
            Write-Host ""
            $text = Read-Host "Enter text to analyze"
            try {
                $detected = Detect-Language -Text $text
                Write-Host ""
                Write-Host "🔍 Detected Language: $detected" -ForegroundColor Green
                Write-Host ""
            } catch {
                Write-Host "❌ Error: $_" -ForegroundColor Red
            }
        }
        
        '4' {
            Start-InteractiveAnalysis
        }
        
        '5' {
            Write-Host ""
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host "PE BINARY ANALYSIS" -ForegroundColor Cyan
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host ""
            
            $binaryPath = Read-Host "Enter path to PE binary"
            
            if (Test-Path $binaryPath) {
                try {
                    Write-Host ""
                    Write-Host "🔍 Analyzing: $binaryPath" -ForegroundColor Cyan
                    $result = Invoke-PEAnalysis -BinaryPath $binaryPath
                    
                    Write-Host ""
                    Write-Host "═" * 70 -ForegroundColor Green
                    Write-Host "ANALYSIS COMPLETE" -ForegroundColor Green
                    Write-Host "═" * 70 -ForegroundColor Green
                    
                } catch {
                    Write-Host "❌ Analysis failed: $_" -ForegroundColor Red
                }
            } else {
                Write-Host "❌ File not found: $binaryPath" -ForegroundColor Red
            }
            Write-Host ""
        }
        
        '6' {
            Write-Host ""
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host "GENERATE C HEADER FILE" -ForegroundColor Cyan
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host ""
            
            $binaryPath = Read-Host "Enter path to PE binary"
            $outputPath = Read-Host "Output header file path (leave blank for auto)"
            
            if (-not $outputPath) {
                $outputPath = [System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($binaryPath), 
                    ([System.IO.Path]::GetFileNameWithoutExtension($binaryPath) + ".h"))
            }
            
            if (Test-Path $binaryPath) {
                try {
                    Write-Host ""
                    $header = Generate-HeaderFile -BinaryPath $binaryPath -OutputPath $outputPath
                    Write-Host ""
                    Write-Host "✅ Header file generated: $outputPath" -ForegroundColor Green
                    Write-Host ""
                } catch {
                    Write-Host "❌ Generation failed: $_" -ForegroundColor Red
                }
            } else {
                Write-Host "❌ File not found: $binaryPath" -ForegroundColor Red
            }
            Write-Host ""
        }
        
        '7' {
            Start-PluginCraftRoom
        }
        
        '8' {
            Write-Host ""
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host "QUICK PLUGIN CREATION" -ForegroundColor Cyan
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host ""
            
            Write-Host "Plugin Types:" -ForegroundColor Yellow
            Write-Host "  1. Translator" -ForegroundColor White
            Write-Host "  2. Analyzer" -ForegroundColor White
            Write-Host "  3. Connector" -ForegroundColor White
            Write-Host "  4. Generator" -ForegroundColor White
            Write-Host "  5. Processor" -ForegroundColor White
            Write-Host "  6. Custom" -ForegroundColor White
            Write-Host ""
            
            Write-Host "Type (1-6): " -NoNewline -ForegroundColor Cyan
            $typeChoice = Read-Host
            
            $typeMap = @{
                '1' = 'Translator'
                '2' = 'Analyzer'
                '3' = 'Connector'
                '4' = 'Generator'
                '5' = 'Processor'
                '6' = 'Custom'
            }
            
            if ($typeMap.ContainsKey($typeChoice)) {
                $type = $typeMap[$typeChoice]
                $name = Read-Host "Plugin name"
                
                try {
                    $path = New-CraftRoomPlugin -Name $name -Type $type
                    Write-Host ""
                    Write-Host "✅ Plugin created: $path" -ForegroundColor Green
                    Write-Host ""
                } catch {
                    Write-Host "❌ Error: $_" -ForegroundColor Red
                }
            } else {
                Write-Host "❌ Invalid selection" -ForegroundColor Red
            }
            Write-Host ""
        }
        
        '9' {
            Show-PluginTemplates
        }
        
        '10' {
            Write-Host ""
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host "SYSTEM STATUS" -ForegroundColor Cyan
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
            Write-Host ""
            
            Write-Host "✅ SYSTEMS LOADED:" -ForegroundColor Green
            Write-Host "   • Model Translator Engine" -ForegroundColor Gray
            Write-Host "   • Codex Accessibility Layer" -ForegroundColor Gray
            Write-Host "   • Plugin Craft Room" -ForegroundColor Gray
            Write-Host ""
            
            Write-Host "📊 STATISTICS:" -ForegroundColor Yellow
            
            try {
                $context = Get-SessionContext
                Write-Host "   • Translations cached: $($context.TranslationsCount)" -ForegroundColor Gray
            } catch {
                Write-Host "   • Translations cached: N/A" -ForegroundColor Gray
            }
            
            try {
                $plugins = Show-CreatedPlugins 2>&1
                Write-Host "   • Plugins available: Multiple" -ForegroundColor Gray
            } catch {
                Write-Host "   • Plugins available: 0" -ForegroundColor Gray
            }
            
            Write-Host ""
            Write-Host "📁 LOCATIONS:" -ForegroundColor Yellow
            Write-Host "   • Scripts: D:\lazy init ide\scripts\" -ForegroundColor Gray
            Write-Host "   • Craft Room: D:\lazy init ide\craft_room\" -ForegroundColor Gray
            Write-Host "   • Codex: D:\CodexReverseEngine\" -ForegroundColor Gray
            Write-Host ""
        }
        
        '0' {
            Write-Host ""
            Write-Host "👋 Shutting down... Goodbye!" -ForegroundColor Yellow
            Write-Host ""
            break
        }
        
        default {
            Write-Host "❌ Invalid selection" -ForegroundColor Red
        }
    }
    
    if ($choice -ne '0' -and $choice -ne '4' -and $choice -ne '7') {
        Read-Host "Press Enter to continue"
    }
}

Write-Host ""
Write-Host "✅ System shutdown complete" -ForegroundColor Green
Write-Host ""
