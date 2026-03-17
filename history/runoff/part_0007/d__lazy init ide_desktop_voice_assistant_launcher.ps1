#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD Voice Assistant Launcher - Quick Start Menu

.DESCRIPTION
    Interactive menu to launch the voice assistant in different modes
#>

Write-Host @"

╔══════════════════════════════════════════════════════════════════════════════╗
║                                                                              ║
║                 🎤 RawrXD VOICE AI ASSISTANT LAUNCHER 🎤                  ║
║                                                                              ║
║                       Choose Your Launch Mode                               ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Magenta

$menu = @"

┌──────────────────────────────────────────────────────────────────────────────┐
│  1️⃣  CLI MODE (Recommended for First Time)                                 │
│      └─ Type commands in terminal                                           │
│      └─ Full control, detailed feedback                                    │
│      └─ Example: "RawrXD, play punk rock"                                  │
│                                                                              │
│  2️⃣  GUI MODE (Best for Visual Users)                                      │
│      └─ Professional control panel                                         │
│      └─ One-click buttons for genres & websites                            │
│      └─ Real-time status display                                           │
│                                                                              │
│  3️⃣  TEST MODE (See a Demo)                                                │
│      └─ Auto-runs sample commands                                          │
│      └─ No user input required                                             │
│      └─ See all features in action                                         │
│                                                                              │
│  4️⃣  CUSTOM NAME MODE                                                       │
│      └─ Change voice name (Alexa, JARVIS, etc.)                           │
│      └─ Specify your preferred mode                                        │
│                                                                              │
│  5️⃣  OPEN DOCUMENTATION                                                     │
│      └─ Complete user guide                                                │
│      └─ Command reference                                                  │
│      └─ Troubleshooting                                                    │
│                                                                              │
│  6️⃣  EXIT                                                                   │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘

"@

Write-Host $menu -ForegroundColor Cyan

while ($true) {
    Write-Host "`nSelect option [1-6]: " -ForegroundColor Yellow -NoNewline
    $choice = Read-Host
    
    switch ($choice) {
        "1" {
            Write-Host "`n🎤 Launching CLI Mode...`n" -ForegroundColor Green
            & ".\voice_assistant_full.ps1"
            break
        }
        "2" {
            Write-Host "`n🎮 Launching GUI Mode...`n" -ForegroundColor Green
            & ".\voice_assistant_full.ps1" -GUI
            break
        }
        "3" {
            Write-Host "`n🧪 Launching Test Mode...`n" -ForegroundColor Green
            & ".\voice_assistant_full.ps1" -TestMode
            break
        }
        "4" {
            Write-Host "`n🎙️ Custom Voice Name Mode`n" -ForegroundColor Green
            Write-Host "Examples: Alexa, JARVIS, Friday, Echo, Cortana`n" -ForegroundColor Gray
            $name = Read-Host "Enter voice name"
            Write-Host "`nSelect mode for '$name':" -ForegroundColor Cyan
            Write-Host "  1 = CLI (text commands)" -ForegroundColor White
            Write-Host "  2 = GUI (buttons)" -ForegroundColor White
            Write-Host "  3 = Test (demo)" -ForegroundColor White
            $mode = Read-Host "`nMode [1-3]"
            
            switch ($mode) {
                "1" { & ".\voice_assistant_full.ps1" -VoiceName $name }
                "2" { & ".\voice_assistant_full.ps1" -GUI -VoiceName $name }
                "3" { & ".\voice_assistant_full.ps1" -TestMode -VoiceName $name }
                default { Write-Host "Invalid mode" -ForegroundColor Red }
            }
            break
        }
        "5" {
            Write-Host "`n📚 Opening Documentation...`n" -ForegroundColor Green
            
            $docMenu = @"
            
┌────────────────────────────────────────────┐
│  Documentation Files:                      │
│                                            │
│  1 = Complete User Guide                  │
│  2 = Quick Reference Card                 │
│  3 = System Status & Architecture         │
│  4 = Back to Main Menu                    │
└────────────────────────────────────────────┘

"@
            Write-Host $docMenu -ForegroundColor Cyan
            
            $docChoice = Read-Host "Select [1-4]"
            switch ($docChoice) {
                "1" { 
                    if (Test-Path "..\docs\VOICE_ASSISTANT_GUIDE.md") {
                        Get-Content "..\docs\VOICE_ASSISTANT_GUIDE.md" | more
                    } else {
                        Write-Host "Guide file not found" -ForegroundColor Red
                    }
                }
                "2" { 
                    if (Test-Path "..\docs\VOICE_ASSISTANT_QUICK_REF.md") {
                        Get-Content "..\docs\VOICE_ASSISTANT_QUICK_REF.md" | more
                    } else {
                        Write-Host "Quick ref file not found" -ForegroundColor Red
                    }
                }
                "3" { 
                    if (Test-Path "..\docs\VOICE_ASSISTANT_STATUS.md") {
                        Get-Content "..\docs\VOICE_ASSISTANT_STATUS.md" | more
                    } else {
                        Write-Host "Status file not found" -ForegroundColor Red
                    }
                }
                "4" { continue }
                default { Write-Host "Invalid choice" -ForegroundColor Red }
            }
            continue
        }
        "6" {
            Write-Host "`n👋 Goodbye!`n" -ForegroundColor Green
            exit
        }
        default {
            Write-Host "❌ Invalid option. Please select 1-6." -ForegroundColor Red
        }
    }
}
