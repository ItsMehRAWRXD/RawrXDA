#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Voice-Enabled IDE Assistant with 1MB Context Memory

.DESCRIPTION
    Complete voice-enabled assistant with:
    - Speech-to-Text: Speak your commands
    - Text-to-Speech: Hear responses
    - Live subtitles for accessibility
    - 1MB context memory with learning
    - Natural language command translation
    - Web search capabilities
    - Full codebase knowledge

.PARAMETER Mode
    interactive, voice, api

.PARAMETER EnableVoice
    Enable voice input/output

.PARAMETER EnableSubtitles
    Show live subtitles

.EXAMPLE
    .\voice_assistant.ps1 -Mode voice -EnableSubtitles
    
.EXAMPLE
    .\voice_assistant.ps1 -Mode interactive -EnableVoice
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('interactive', 'voice', 'api')]
    [string]$Mode = "interactive",
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableVoice,
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableSubtitles,
    
    [Parameter(Mandatory=$false)]
    [int]$Port = 8080,
    
    [Parameter(Mandatory=$false)]
    [string]$VoiceName = "David"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. "$PSScriptRoot\\RawrXD_Root.ps1"

# ═══════════════════════════════════════════════════════════════════════════════
# VOICE-ENABLED ASSISTANT
# ═══════════════════════════════════════════════════════════════════════════════

class VoiceAssistant {
    [object]$ContextMemory
    [object]$SpeechInterface
    [hashtable]$DigestedKB
    [bool]$HasDigestedKB
    [bool]$VoiceEnabled
    [bool]$SubtitlesEnabled
    [string]$VoiceName
    
    VoiceAssistant([bool]$voiceEnabled, [bool]$subtitlesEnabled, [string]$voiceName) {
        $this.VoiceEnabled = $voiceEnabled
        $this.SubtitlesEnabled = $subtitlesEnabled
        $this.VoiceName = $voiceName
        $this.HasDigestedKB = $false
        
        # Initialize context memory
        Write-Host "`n  Loading context memory..." -ForegroundColor Yellow
        $this.LoadContextMemory()
        
        # Initialize speech if enabled
        if ($voiceEnabled) {
            Write-Host "  Initializing speech system..." -ForegroundColor Yellow
            $this.InitializeSpeech()
        }
        
        # Load digested knowledge base
        Write-Host "  Loading knowledge base..." -ForegroundColor Yellow
        $this.LoadDigestedKB()
    }
    
    [void] LoadContextMemory() {
        # Context memory is loaded via the script
        Write-Host "  ✓ Context memory ready (1MB capacity)" -ForegroundColor Green
    }
    
    [void] InitializeSpeech() {
        Add-Type -AssemblyName System.Speech
        $this.SpeechInterface = New-Object System.Speech.Synthesis.SpeechSynthesizer
        
        try {
            $this.SpeechInterface.SelectVoice($this.VoiceName)
        }
        catch {
            Write-Host "  ⚠ Voice not found, using default" -ForegroundColor Yellow
        }
        
        Write-Host "  ✓ Speech system ready" -ForegroundColor Green
    }
    
    [void] LoadDigestedKB() {
        $projectRoot = Get-RawrXDRoot
        $kbPath = Join-Path $projectRoot "data" "knowledge_base.json"
        $kbPath = Resolve-RawrXDPath $kbPath
        
        if (Test-Path $kbPath) {
            try {
                $json = Get-Content $kbPath -Raw | ConvertFrom-Json -AsHashtable
                $this.DigestedKB = $json
                $this.HasDigestedKB = $true
                Write-Host "  ✓ Loaded: $($json.Metadata.FileCount) files, $($json.Functions.Count) functions" -ForegroundColor Green
            }
            catch {
                $this.HasDigestedKB = $false
            }
        }
    }
    
    [string] ProcessQuestion([string]$question) {
        # Save to context memory
        & "$PSScriptRoot\context_memory.ps1" -Operation add -Context $question 2>&1 | Out-Null
        
        $answer = ""
        
        # Try command translation for action verbs
        if ($question -match '^\s*(send|deploy|create|make|add|find|search|open|train|quantize|monitor|stop|benchmark|prune)') {
            try {
                $result = & "$PSScriptRoot\command_translator.ps1" -Request $question 2>&1 | Out-String
                
                if ($result -match '"Success":\s*true') {
                    $json = ($result | Select-String -Pattern '\{.*\}' -AllMatches).Matches[0].Value | ConvertFrom-Json
                    
                    $answer = "🤖 **Command Translation:**`n`n"
                    $answer += "📝 **Machine Command:**`n``````powershell`n$($json.Command)`n```````n`n"
                    $answer += "💡 **Explanation:** $($json.Explanation)`n`n"
                    $answer += "⚡ **Tip:** Copy and run the command, or I can execute it for you!"
                    
                    # Save command to memory
                    & "$PSScriptRoot\context_memory.ps1" -Operation add -Context "Translated: $($json.Command)" 2>&1 | Out-Null
                    
                    return $answer
                }
            }
            catch {}
        }
        
        # Try web search for questions
        if ($question -match '(search|look|google|find).*(online|web)|what\s+is|how\s+does') {
            try {
                $searchQuery = $question -replace '(search|look|google|find|what\s+is|how\s+does|explain|online|web|internet)', '' -replace '\s+', ' '
                $searchQuery = $searchQuery.Trim()
                
                if ($searchQuery.Length -gt 3) {
                    $result = & "$PSScriptRoot\browser_helper.ps1" -Operation search -Query $searchQuery -MaxResults 3 2>&1 | Out-String
                    
                    if ($result -match '"Success":\s*true') {
                        $json = ($result | Select-String -Pattern '\{.*\}' -AllMatches).Matches[0].Value | ConvertFrom-Json
                        
                        $answer = "🌐 **Web Search Results:**`n`n"
                        foreach ($item in $json.Results) {
                            $answer += "**$($item.Position). $($item.Title)**`n"
                            $answer += "   $($item.URL)`n`n"
                        }
                        
                        return $answer
                    }
                }
            }
            catch {}
        }
        
        # Search digested knowledge base
        if ($this.HasDigestedKB) {
            $results = $this.SearchDigestedKB($question)
            if ($results.Score -gt 8) {
                $answer = $this.FormatDigestedResults($results)
                return $answer
            }
        }
        
        # Get context-based suggestions
        try {
            $suggestions = & "$PSScriptRoot\context_memory.ps1" -Operation tune 2>&1 | Out-String
            if ($suggestions) {
                $answer = "💡 **Based on your preferences:**`n`n$suggestions"
                return $answer
            }
        }
        catch {}
        
        # Default response
        return "I'm not sure about that. Try asking about swarms, models, todos, or search the web!"
    }
    
    [hashtable] SearchDigestedKB([string]$query) {
        $results = @{ Score = 0; Files = @(); Functions = @() }
        
        $queryLower = $query.ToLower()
        $keywords = $queryLower -split '\s+' | Where-Object { $_.Length -gt 2 }
        
        foreach ($filePath in $this.DigestedKB.Files.Keys) {
            $fileInfo = $this.DigestedKB.Files[$filePath]
            $score = 0
            
            foreach ($keyword in $keywords) {
                if ($fileInfo.Name.ToLower() -match $keyword) { $score += 10 }
                if ($fileInfo.Synopsis.ToLower() -match $keyword) { $score += 5 }
            }
            
            if ($score -gt 0) {
                $results.Files += @{ Name = $fileInfo.Name; Synopsis = $fileInfo.Synopsis; Score = $score }
                $results.Score += $score
            }
        }
        
        $results.Files = $results.Files | Sort-Object -Property Score -Descending
        return $results
    }
    
    [string] FormatDigestedResults([hashtable]$results) {
        $response = "📚 **From your codebase:**`n`n"
        
        foreach ($file in ($results.Files | Select-Object -First 3)) {
            $response += "📄 ``$($file.Name)``"
            if ($file.Synopsis) {
                $response += " - $($file.Synopsis)"
            }
            $response += "`n"
        }
        
        return $response
    }
    
    [void] Speak([string]$text) {
        if ($this.VoiceEnabled) {
            # Clean text for speaking (remove markdown)
            $cleanText = $text -replace '\*\*', '' -replace '`', '' -replace '#', ''
            $cleanText = $cleanText -replace '📚|📄|🤖|💡|⚡|🌐|📝', ''
            $cleanText = $cleanText -split "`n" | Where-Object { $_.Trim().Length -gt 0 } | Select-Object -First 10 | Join-String -Separator '. '
            
            if ($this.SubtitlesEnabled) {
                & "$PSScriptRoot\speech_interface.ps1" -Operation speak -Text $cleanText -Voice $this.VoiceName -ShowSubtitles 2>&1 | Out-Null
            }
            else {
                $this.SpeechInterface.Speak($cleanText)
            }
        }
    }
    
    [string] Listen() {
        if ($this.VoiceEnabled) {
            $result = & "$PSScriptRoot\speech_interface.ps1" -Operation listen 2>&1 | Out-String
            
            if ($result -match '"Text":\s*"([^"]+)"') {
                return $matches[1]
            }
        }
        
        return ""
    }
    
    [string] GetGreeting() {
        return @"

╔═══════════════════════════════════════════════════════════════╗
║       🎙️  VOICE-ENABLED IDE ASSISTANT 🎙️                     ║
╚═══════════════════════════════════════════════════════════════╝

✨ **Features Active:**
   🗣️  Voice Input: $(if ($this.VoiceEnabled) { 'Enabled' } else { 'Disabled' })
   🔊 Voice Output: $(if ($this.VoiceEnabled) { 'Enabled' } else { 'Disabled' })
   📝 Subtitles: $(if ($this.SubtitlesEnabled) { 'Enabled' } else { 'Disabled' })
   🧠 Context Memory: 1MB capacity
   📚 Knowledge Base: $(if ($this.HasDigestedKB) { 'Loaded' } else { 'Not loaded' })

**How to use:**
$(if ($this.VoiceEnabled) {
'   • Type "voice" to use voice input'
'   • Type your question normally for text input'
} else {
'   • Type your questions normally'
'   • Use -EnableVoice to enable voice features'
})
   • Ask about swarms, models, todos, or anything!
   • I learn from your preferences over time

**Examples:**
   "send 5 agents to D:\code"
   "create a 7B model"
   "search the web for async programming"

Type 'help' for more commands or 'exit' to quit.
"@
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# INTERACTIVE MODE
# ═══════════════════════════════════════════════════════════════════════════════

function Start-VoiceInteractive {
    param([bool]$voiceEnabled, [bool]$subtitlesEnabled, [string]$voiceName)
    
    $assistant = [VoiceAssistant]::new($voiceEnabled, $subtitlesEnabled, $voiceName)
    
    Write-Host $assistant.GetGreeting() -ForegroundColor Cyan
    
    if ($voiceEnabled) {
        $assistant.Speak("Hello! I am your voice enabled IDE assistant. How can I help you today?")
    }
    
    while ($true) {
        Write-Host "`n" -NoNewline
        
        # Voice input option
        if ($voiceEnabled) {
            Write-Host "You> (type 'voice' for voice input, or type message): " -NoNewline -ForegroundColor Green
        }
        else {
            Write-Host "You> " -NoNewline -ForegroundColor Green
        }
        
        $userInput = Read-Host
        
        # Handle voice input
        if ($userInput -eq 'voice' -and $voiceEnabled) {
            Write-Host "`n🎤 Listening..." -ForegroundColor Cyan
            $userInput = $assistant.Listen()
            
            if ($userInput) {
                Write-Host "You (voice)> $userInput" -ForegroundColor Green
            }
            else {
                Write-Host "⚠ No speech detected" -ForegroundColor Yellow
                continue
            }
        }
        
        # Handle exit
        if ($userInput -match '^\s*(exit|quit|bye)\s*$') {
            $farewell = "Goodbye! Happy coding!"
            Write-Host "`n👋 $farewell`n" -ForegroundColor Yellow
            if ($voiceEnabled) {
                $assistant.Speak($farewell)
            }
            break
        }
        
        if ($userInput.Trim() -eq "") {
            continue
        }
        
        # Special commands
        if ($userInput -match '^\s*stats\s*$') {
            & "$PSScriptRoot\context_memory.ps1" -Operation stats
            continue
        }
        
        if ($userInput -match '^\s*memory\s*$') {
            & "$PSScriptRoot\context_memory.ps1" -Operation tune
            continue
        }
        
        # Process question
        Write-Host "`nAssistant> " -ForegroundColor Cyan
        $answer = $assistant.ProcessQuestion($userInput)
        Write-Host $answer -ForegroundColor White
        
        # Speak answer if voice enabled
        if ($voiceEnabled) {
            $assistant.Speak($answer)
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# VOICE-ONLY MODE
# ═══════════════════════════════════════════════════════════════════════════════

function Start-VoiceOnly {
    param([bool]$subtitlesEnabled, [string]$voiceName)
    
    $assistant = [VoiceAssistant]::new($true, $subtitlesEnabled, $voiceName)
    
    Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║              VOICE-ONLY MODE ACTIVATED                        ║" -ForegroundColor Magenta
    Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta
    
    $assistant.Speak("Voice only mode activated. Speak your commands clearly.")
    
    while ($true) {
        Write-Host "`n🎤 Listening for command... (say 'exit' to quit)" -ForegroundColor Cyan
        
        $command = $assistant.Listen()
        
        if (-not $command) {
            Write-Host "⚠ No speech detected, trying again..." -ForegroundColor Yellow
            continue
        }
        
        Write-Host "📝 Recognized: $command" -ForegroundColor Green
        
        if ($command -match 'exit|quit|stop') {
            $assistant.Speak("Goodbye!")
            break
        }
        
        $answer = $assistant.ProcessQuestion($command)
        Write-Host "`n$answer`n" -ForegroundColor White
        $assistant.Speak($answer)
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║         VOICE-ENABLED IDE ASSISTANT INITIALIZING              ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

switch ($Mode) {
    "interactive" {
        Start-VoiceInteractive -voiceEnabled $EnableVoice -subtitlesEnabled $EnableSubtitles -voiceName $VoiceName
    }
    
    "voice" {
        if (-not $EnableVoice) {
            $EnableVoice = $true
        }
        Start-VoiceOnly -subtitlesEnabled $EnableSubtitles -voiceName $VoiceName
    }
    
    "api" {
        # API mode with voice support
        Write-Host "  API mode with voice features coming soon!" -ForegroundColor Yellow
        Write-Host "  Use interactive mode for now.`n" -ForegroundColor Gray
    }
}

Write-Host ""
