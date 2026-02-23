#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Speech Interface - Text-to-Speech and Speech-to-Text with Subtitles

.DESCRIPTION
    Provides voice interface for the IDE assistant:
    - Speech-to-Text (STT): Voice commands to text
    - Text-to-Speech (TTS): Read responses aloud
    - Real-time subtitles for accessibility
    - Multiple voices and languages
    - Audio file support

.PARAMETER Operation
    listen, speak, subtitle, test

.PARAMETER Text
    Text to speak (for TTS)

.PARAMETER Voice
    Voice name for TTS (David, Zira, etc.)

.PARAMETER ShowSubtitles
    Display real-time subtitles while speaking

.EXAMPLE
    .\speech_interface.ps1 -Operation speak -Text "Hello, how can I help?"
    
.EXAMPLE
    .\speech_interface.ps1 -Operation listen -ShowSubtitles
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('listen', 'speak', 'subtitle', 'test', 'voices')]
    [string]$Operation,
    
    [Parameter(Mandatory=$false)]
    [string]$Text = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Voice = "David",
    
    [Parameter(Mandatory=$false)]
    [switch]$ShowSubtitles,
    
    [Parameter(Mandatory=$false)]
    [int]$Rate = 0,  # -10 to 10
    
    [Parameter(Mandatory=$false)]
    [int]$Volume = 100  # 0 to 100
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# SPEECH INTERFACE ENGINE
# ═══════════════════════════════════════════════════════════════════════════════

class SpeechInterface {
    [object]$SpeechSynthesizer
    [object]$SpeechRecognizer
    [bool]$IsListening
    [System.Collections.ArrayList]$RecognizedText
    [string]$SubtitleFile
    
    SpeechInterface() {
        # Load Speech API
        Add-Type -AssemblyName System.Speech
        
        $this.SpeechSynthesizer = New-Object System.Speech.Synthesis.SpeechSynthesizer
        $this.RecognizedText = [System.Collections.ArrayList]::new()
        $this.IsListening = $false
        $this.SubtitleFile = "D:\lazy init ide\data\current_subtitles.txt"
        
        # Initialize recognizer
        try {
            $this.SpeechRecognizer = New-Object System.Speech.Recognition.SpeechRecognitionEngine
            $this.SpeechRecognizer.SetInputToDefaultAudioDevice()
        }
        catch {
            Write-Verbose "Speech recognition not available: $_"
        }
    }
    
    [void] Speak([string]$text, [string]$voice, [int]$rate, [int]$volume, [bool]$showSubtitles) {
        # Set voice
        try {
            $this.SpeechSynthesizer.SelectVoice($voice)
        }
        catch {
            Write-Host "  ⚠ Voice '$voice' not found, using default" -ForegroundColor Yellow
        }
        
        # Set rate and volume
        $this.SpeechSynthesizer.Rate = $rate
        $this.SpeechSynthesizer.Volume = $volume
        
        # Display subtitle banner
        if ($showSubtitles) {
            $this.DisplaySubtitleBanner($text)
        }
        
        # Save subtitles to file
        $this.SaveSubtitle($text)
        
        Write-Host "`n🔊 Speaking: " -NoNewline -ForegroundColor Cyan
        Write-Host "$text`n" -ForegroundColor White
        
        # Speak asynchronously with word highlighting
        if ($showSubtitles) {
            $this.SpeakWithSubtitles($text)
        }
        else {
            $this.SpeechSynthesizer.Speak($text)
        }
        
        Write-Host "  ✅ Finished speaking`n" -ForegroundColor Green
    }
    
    [void] SpeakWithSubtitles([string]$text) {
        $words = $text -split '\s+'
        
        # Create subtitle display
        Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
        Write-Host "║                     LIVE SUBTITLES                            ║" -ForegroundColor Magenta
        Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
        
        $prompt = $this.SpeechSynthesizer.SpeakAsync($text)
        
        # Simulate word-by-word subtitle display
        $wordsPerSecond = 3 + ($this.SpeechSynthesizer.Rate / 2)
        $delayMs = [int](1000 / $wordsPerSecond)
        
        Write-Host "`n  " -NoNewline
        foreach ($word in $words) {
            Write-Host "$word " -NoNewline -ForegroundColor Yellow
            Start-Sleep -Milliseconds $delayMs
        }
        
        # Wait for speech to complete
        while (-not $prompt.IsCompleted) {
            Start-Sleep -Milliseconds 100
        }
        
        Write-Host "`n`n╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta
    }
    
    [void] DisplaySubtitleBanner([string]$text) {
        $maxWidth = 63
        $lines = $this.WrapText($text, $maxWidth)
        
        Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
        Write-Host "║                        SUBTITLES                              ║" -ForegroundColor Cyan
        Write-Host "╠═══════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
        
        foreach ($line in $lines) {
            $padding = $maxWidth - $line.Length
            $padLeft = [Math]::Floor($padding / 2)
            $padRight = $padding - $padLeft
            Write-Host "║ $(' ' * $padLeft)$line$(' ' * $padRight) ║" -ForegroundColor White
        }
        
        Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
    }
    
    [array] WrapText([string]$text, [int]$maxWidth) {
        $words = $text -split '\s+'
        $lines = @()
        $currentLine = ""
        
        foreach ($word in $words) {
            if (($currentLine + " " + $word).Length -le $maxWidth) {
                $currentLine += " " + $word
            }
            else {
                if ($currentLine) {
                    $lines += $currentLine.Trim()
                }
                $currentLine = $word
            }
        }
        
        if ($currentLine) {
            $lines += $currentLine.Trim()
        }
        
        return $lines
    }
    
    [void] SaveSubtitle([string]$text) {
        $subtitle = "[$(Get-Date -Format 'HH:mm:ss')] $text"
        $subtitle | Add-Content $this.SubtitleFile -Encoding UTF8
    }
    
    [string] Listen([int]$timeoutSeconds = 10) {
        if (-not $this.SpeechRecognizer) {
            Write-Host "  ❌ Speech recognition not available on this system" -ForegroundColor Red
            return ""
        }
        
        Write-Host "`n🎤 Listening... (speak now, $timeoutSeconds second timeout)" -ForegroundColor Cyan
        Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
        Write-Host "║                   VOICE INPUT ACTIVE                          ║" -ForegroundColor Magenta
        Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta
        
        # Build grammar from common commands
        $grammar = $this.BuildCommandGrammar()
        $this.SpeechRecognizer.LoadGrammar($grammar)
        
        try {
            # Set timeout
            $this.SpeechRecognizer.InitialSilenceTimeout = [TimeSpan]::FromSeconds($timeoutSeconds)
            $this.SpeechRecognizer.BabbleTimeout = [TimeSpan]::FromSeconds($timeoutSeconds)
            
            # Listen for single phrase
            $result = $this.SpeechRecognizer.Recognize()
            
            if ($result -and $result.Text) {
                $recognizedText = $result.Text
                $confidence = [Math]::Round($result.Confidence * 100, 1)
                
                Write-Host "  ✅ Recognized (${confidence}% confidence): " -NoNewline -ForegroundColor Green
                Write-Host "$recognizedText`n" -ForegroundColor Yellow
                
                # Save to subtitles
                $this.SaveSubtitle("[VOICE INPUT] $recognizedText")
                $this.RecognizedText.Add(@{
                    Text = $recognizedText
                    Confidence = $confidence
                    Timestamp = Get-Date -Format "o"
                }) | Out-Null
                
                return $recognizedText
            }
            else {
                Write-Host "  ⚠ No speech detected`n" -ForegroundColor Yellow
                return ""
            }
        }
        catch {
            Write-Host "  ❌ Recognition error: $_`n" -ForegroundColor Red
            return ""
        }
    }
    
    [object] BuildCommandGrammar() {
        $choices = New-Object System.Speech.Recognition.Choices
        
        # Common command phrases
        $commands = @(
            "send agents to",
            "create a model",
            "add a todo",
            "search the web",
            "monitor swarm",
            "stop swarm",
            "show my todos",
            "find files",
            "open file",
            "quantize model",
            "train model",
            "benchmark",
            "help",
            "exit"
        )
        
        $choices.Add($commands)
        
        $grammarBuilder = New-Object System.Speech.Recognition.GrammarBuilder($choices)
        $grammar = New-Object System.Speech.Recognition.Grammar($grammarBuilder)
        
        return $grammar
    }
    
    [array] GetAvailableVoices() {
        return $this.SpeechSynthesizer.GetInstalledVoices() | 
            Where-Object { $_.Enabled } | 
            ForEach-Object { $_.VoiceInfo }
    }
    
    [void] TestVoice([string]$voiceName) {
        $testPhrases = @(
            "Hello, I am your RawrXD IDE assistant."
            "I can help you with swarm deployment, model creation, and task management."
            "Try asking me to send agents to a directory or create a model."
        )
        
        foreach ($phrase in $testPhrases) {
            $this.Speak($phrase, $voiceName, 0, 100, $true)
            Start-Sleep -Seconds 1
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

$speech = [SpeechInterface]::new()

switch ($Operation) {
    "speak" {
        if (-not $Text) {
            Write-Error "Text parameter required for speak operation"
            exit 1
        }
        
        $speech.Speak($Text, $Voice, $Rate, $Volume, $ShowSubtitles)
    }
    
    "listen" {
        $recognizedText = $speech.Listen(10)
        
        if ($recognizedText) {
            # Return as JSON for programmatic use
            @{
                Success = $true
                Text = $recognizedText
                Timestamp = Get-Date -Format "o"
            } | ConvertTo-Json
        }
        else {
            @{
                Success = $false
                Message = "No speech detected"
            } | ConvertTo-Json
        }
    }
    
    "subtitle" {
        if (Test-Path $speech.SubtitleFile) {
            Write-Host "`n📝 Recent Subtitles:`n" -ForegroundColor Cyan
            Get-Content $speech.SubtitleFile -Tail 20 | ForEach-Object {
                Write-Host "  $_" -ForegroundColor White
            }
            Write-Host ""
        }
        else {
            Write-Host "`n  No subtitles yet`n" -ForegroundColor Yellow
        }
    }
    
    "voices" {
        $voices = $speech.GetAvailableVoices()
        
        Write-Host "`n🎙️  Available Voices:`n" -ForegroundColor Cyan
        
        foreach ($voice in $voices) {
            Write-Host "  📢 $($voice.Name)" -ForegroundColor Yellow
            Write-Host "     Gender: $($voice.Gender)" -ForegroundColor Gray
            Write-Host "     Age: $($voice.Age)" -ForegroundColor Gray
            Write-Host "     Language: $($voice.Culture.DisplayName)" -ForegroundColor Gray
            Write-Host ""
        }
        
        Write-Host "  💡 Use with: -Voice `"$($voices[0].Name)`"`n" -ForegroundColor Magenta
    }
    
    "test" {
        Write-Host "`n🎬 Testing Speech System...`n" -ForegroundColor Cyan
        
        $voices = $speech.GetAvailableVoices()
        if ($voices.Count -gt 0) {
            Write-Host "  Testing voice: $($voices[0].Name)`n" -ForegroundColor Yellow
            $speech.TestVoice($voices[0].Name)
        }
        
        Write-Host "`n  Testing speech recognition...`n" -ForegroundColor Yellow
        $result = $speech.Listen(5)
        
        Write-Host "`n✅ Speech system test complete`n" -ForegroundColor Green
    }
}
