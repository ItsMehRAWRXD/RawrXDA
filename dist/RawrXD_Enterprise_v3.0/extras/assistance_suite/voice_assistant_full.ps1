#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD - Voice-Controlled AI Assistant with Music & Web Browsing

.DESCRIPTION
    Say "RawrXD, play punk rock" or "RawrXD, go to amazon and show me deals"
    Includes music player, agentic web browser, and interactive GUI.

.EXAMPLE
    .\voice_assistant_full.ps1              # Launch voice mode
    .\voice_assistant_full.ps1 -GUI         # Launch GUI
    .\voice_assistant_full.ps1 -TestMode   # Test with demo commands
#>

param(
    [Parameter(Mandatory=$false)]
    [switch]$GUI,
    
    [Parameter(Mandatory=$false)]
    [switch]$TestMode,
    
    [Parameter(Mandatory=$false)]
    [string]$VoiceName = "RawrXD"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Continue"

# ═══════════════════════════════════════════════════════════════════════════════
# VOICE ASSISTANT CORE
# ═══════════════════════════════════════════════════════════════════════════════

class VoiceAssistant {
    [string]$Name
    [System.Collections.ArrayList]$CommandHistory
    [System.Collections.ArrayList]$SearchHistory
    [hashtable]$GenreMap
    [hashtable]$Websites
    [bool]$IsPlaying
    [string]$CurrentGenre
    [string]$CurrentURL
    
    VoiceAssistant([string]$name) {
        $this.Name = $name
        $this.CommandHistory = [System.Collections.ArrayList]::new()
        $this.SearchHistory = [System.Collections.ArrayList]::new()
        $this.IsPlaying = $false
        $this.CurrentGenre = "Unknown"
        $this.CurrentURL = ""
        $this.InitializeGenres()
        $this.InitializeWebsites()
    }
    
    [void] InitializeGenres() {
        $this.GenreMap = @{
            "punk" = @("Sex Pistols", "The Ramones", "Dead Kennedys", "Blink-182")
            "rock" = @("Pink Floyd", "Led Zeppelin", "Queen", "The Beatles")
            "metal" = @("Black Sabbath", "Metallica", "Iron Maiden", "Judas Priest")
            "pop" = @("Taylor Swift", "Billie Eilish", "The Weeknd", "Ariana Grande")
            "hiphop" = @("Eminem", "Dr. Dre", "Jay-Z", "Kendrick Lamar")
            "jazz" = @("Miles Davis", "John Coltrane", "Duke Ellington", "Chet Baker")
            "edm" = @("Deadmau5", "Skrillex", "Calvin Harris", "David Guetta")
            "lofi" = @("Chill Lo-Fi Beats", "Study Music", "Ambient Beats")
            "classical" = @("Beethoven", "Mozart", "Bach", "Chopin")
        }
    }
    
    [void] InitializeWebsites() {
        $this.Websites = @{
            "amazon" = "https://www.amazon.com"
            "amazon deals" = "https://www.amazon.com/gp/goldbox"
            "ebay" = "https://www.ebay.com"
            "youtube" = "https://www.youtube.com"
            "netflix" = "https://www.netflix.com"
            "weather" = "https://weather.com"
            "news" = "https://news.google.com"
            "reddit" = "https://reddit.com"
            "github" = "https://github.com"
            "stackoverflow" = "https://stackoverflow.com"
            "twitter" = "https://twitter.com"
            "facebook" = "https://facebook.com"
            "gmail" = "https://mail.google.com"
            "google" = "https://google.com"
            "wikipedia" = "https://wikipedia.org"
        }
    }
    
    [void] Speak([string]$text) {
        $text = $text -replace '"', '\"'
        $psCmd = "`$speak = New-Object System.Speech.Synthesis.SpeechSynthesizer; `$speak.Speak('$text')"
        powershell -NoProfile -Command $psCmd 2>$null
    }
    
    [hashtable] ParseCommand([string]$command) {
        $command = $command.Trim().ToLower()
        
        # Remove name prefix
        $command = $command -replace "$($this.Name.ToLower())[,\s]*", ""
        $command = $command.Trim()
        
        $result = @{
            Original = $command
            Action = ""
            Query = ""
            Type = ""
        }
        
        # Detect action type
        if ($command -match "^(play|music)\s+(.+)") {
            $result.Action = "play"
            $result.Type = "music"
            $result.Query = $matches[2]
        }
        elseif ($command -match "^(go to|goto|open|visit|browse)\s+(.+)") {
            $result.Action = "browse"
            $result.Type = "web"
            $result.Query = $matches[2]
        }
        elseif ($command -match "^(search|find)\s+(.+)") {
            $result.Action = "search"
            $result.Type = "web"
            $result.Query = $matches[2]
        }
        elseif ($command -match "pause") {
            $result.Action = "pause"
            $result.Type = "control"
        }
        elseif ($command -match "resume|continue") {
            $result.Action = "resume"
            $result.Type = "control"
        }
        elseif ($command -match "next|skip") {
            $result.Action = "next"
            $result.Type = "control"
        }
        elseif ($command -match "status|what|playing") {
            $result.Action = "status"
            $result.Type = "info"
        }
        
        return $result
    }
    
    [void] ExecuteCommand([hashtable]$cmd) {
        Write-Host "`n" -ForegroundColor White
        Write-Host "💬 COMMAND: " -ForegroundColor Yellow -NoNewline
        Write-Host $cmd.Original -ForegroundColor Cyan
        
        $this.CommandHistory.Add($cmd) | Out-Null
        
        switch ($cmd.Action) {
            "play" { $this.PlayMusic($cmd.Query) }
            "browse" { $this.BrowseWeb($cmd.Query) }
            "search" { $this.SearchWeb($cmd.Query) }
            "pause" { $this.PauseMusic() }
            "resume" { $this.ResumeMusic() }
            "next" { $this.NextTrack() }
            "status" { $this.ShowStatus() }
            default { 
                Write-Host "❌ I didn't understand that command" -ForegroundColor Red
                $this.Speak("I didn't understand that. Try saying play a genre or go to a website.")
            }
        }
    }
    
    [void] PlayMusic([string]$query) {
        $query = $query.Trim()
        
        # Check if it's a genre
        $genre = $null
        foreach ($key in $this.GenreMap.Keys) {
            if ($query -like "*$key*") {
                $genre = $key
                break
            }
        }
        
        if ($genre) {
            $artists = $this.GenreMap[$genre] -join ", "
            Write-Host "▶️  PLAYING: $genre" -ForegroundColor Green
            Write-Host "   Artists: $artists" -ForegroundColor Gray
            $this.CurrentGenre = $genre
            $this.Speak("Now playing $genre music. Enjoy!")
        }
        else {
            Write-Host "▶️  PLAYING: $query" -ForegroundColor Green
            $this.CurrentGenre = $query
            $this.Speak("Now playing $query.")
        }
        
        $this.IsPlaying = $true
        
        # Open YouTube with search
        $encodedQuery = [Uri]::EscapeDataString($query)
        Start-Process "https://www.youtube.com/results?search_query=$encodedQuery" -ErrorAction SilentlyContinue
    }
    
    [void] BrowseWeb([string]$site) {
        $site = $site.Trim().ToLower()
        
        $url = ""
        if ($this.Websites.ContainsKey($site)) {
            $url = $this.Websites[$site]
            Write-Host "🌐 GOING TO: $site" -ForegroundColor Green
            Write-Host "   URL: $url" -ForegroundColor Gray
        }
        else {
            # Try to match partial
            $match = $this.Websites.Keys | Where-Object { $_ -like "*$site*" } | Select-Object -First 1
            if ($match) {
                $url = $this.Websites[$match]
                Write-Host "🌐 GOING TO: $match" -ForegroundColor Green
                Write-Host "   URL: $url" -ForegroundColor Gray
            }
            else {
                # Assume it's a search query
                $encodedSite = [Uri]::EscapeDataString($site)
                $url = "https://www.google.com/search?q=$encodedSite"
                Write-Host "🔍 SEARCHING: $site" -ForegroundColor Green
            }
        }
        
        if ($url) {
            $this.CurrentURL = $url
            $this.SearchHistory.Add($site) | Out-Null
            $this.Speak("Opening $site")
            Start-Process $url -ErrorAction SilentlyContinue
        }
    }
    
    [void] SearchWeb([string]$query) {
        $encodedQuery = [Uri]::EscapeDataString($query)
        $url = "https://www.google.com/search?q=$encodedQuery"
        Write-Host "🔍 SEARCHING: $query" -ForegroundColor Green
        $this.SearchHistory.Add($query) | Out-Null
        $this.Speak("Searching for $query")
        Start-Process $url -ErrorAction SilentlyContinue
    }
    
    [void] PauseMusic() {
        $this.IsPlaying = $false
        Write-Host "⏸️  PAUSED" -ForegroundColor Yellow
        $this.Speak("Paused")
    }
    
    [void] ResumeMusic() {
        $this.IsPlaying = $true
        Write-Host "▶️  RESUMED" -ForegroundColor Green
        $this.Speak("Resumed")
    }
    
    [void] NextTrack() {
        Write-Host "⏭️  NEXT TRACK" -ForegroundColor Blue
        $this.Speak("Skipping to next track")
    }
    
    [void] ShowStatus() {
        $status = if ($this.IsPlaying) { "▶️ Playing" } else { "⏸️ Paused" }
        $info = "Status: $status | Genre: $($this.CurrentGenre)"
        Write-Host $info -ForegroundColor Cyan
        $this.Speak($info)
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# GUI INTERFACE
# ═══════════════════════════════════════════════════════════════════════════════

function Show-VoiceAssistantGUI {
    param([VoiceAssistant]$Assistant)
    
    [System.Reflection.Assembly]::LoadWithPartialName("System.Windows.Forms") | Out-Null
    [System.Reflection.Assembly]::LoadWithPartialName("System.Drawing") | Out-Null
    
    # Create main form
    $form = New-Object System.Windows.Forms.Form
    $form.Text = "$($Assistant.Name) - Voice Assistant Control Panel"
    $form.Width = 900
    $form.Height = 700
    $form.StartPosition = "CenterScreen"
    $form.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $form.ForeColor = [System.Drawing.Color]::White
    $form.Font = New-Object System.Drawing.Font("Segoe UI", 10)
    
    # Title
    $title = New-Object System.Windows.Forms.Label
    $title.Text = "🎤 $($Assistant.Name) - Voice AI Assistant"
    $title.Font = New-Object System.Drawing.Font("Segoe UI", 16, [System.Drawing.FontStyle]::Bold)
    $title.ForeColor = [System.Drawing.Color]::Cyan
    $title.Location = New-Object System.Drawing.Point(20, 20)
    $title.Size = New-Object System.Drawing.Size(860, 40)
    $form.Controls.Add($title)
    
    # Status panel
    $statusPanel = New-Object System.Windows.Forms.GroupBox
    $statusPanel.Text = "📊 STATUS"
    $statusPanel.ForeColor = [System.Drawing.Color]::Lime
    $statusPanel.Location = New-Object System.Drawing.Point(20, 70)
    $statusPanel.Size = New-Object System.Drawing.Size(860, 80)
    $statusPanel.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
    
    $statusLabel = New-Object System.Windows.Forms.Label
    $statusLabel.Text = "⏸️ Paused | Genre: None | URL: None"
    $statusLabel.ForeColor = [System.Drawing.Color]::White
    $statusLabel.Location = New-Object System.Drawing.Point(20, 30)
    $statusLabel.Size = New-Object System.Drawing.Size(820, 35)
    $statusPanel.Controls.Add($statusLabel)
    
    $form.Controls.Add($statusPanel)
    
    # Music Control Panel
    $musicPanel = New-Object System.Windows.Forms.GroupBox
    $musicPanel.Text = "🎵 MUSIC PLAYER"
    $musicPanel.ForeColor = [System.Drawing.Color]::Yellow
    $musicPanel.Location = New-Object System.Drawing.Point(20, 170)
    $musicPanel.Size = New-Object System.Drawing.Size(420, 230)
    $musicPanel.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
    
    # Genre buttons
    $genres = @("Punk", "Rock", "Metal", "Pop", "HipHop", "Jazz", "EDM", "LoFi")
    $x = 20
    $y = 25
    $col = 0
    
    foreach ($genre in $genres) {
        $btn = New-Object System.Windows.Forms.Button
        $btn.Text = $genre
        $btn.Location = New-Object System.Drawing.Point($x, $y)
        $btn.Size = New-Object System.Drawing.Size(90, 35)
        $btn.BackColor = [System.Drawing.Color]::FromArgb(70, 70, 100)
        $btn.ForeColor = [System.Drawing.Color]::White
        $btn.Font = New-Object System.Drawing.Font("Segoe UI", 9)
        
        $btn.Add_Click({
            $cmd = $Assistant.ParseCommand("play $($btn.Text)")
            $Assistant.ExecuteCommand($cmd)
            $statusLabel.Text = "▶️ Playing | Genre: $($btn.Text) | URL: None"
        })
        
        $musicPanel.Controls.Add($btn)
        
        $col++
        if ($col % 2 -eq 0) {
            $x = 20
            $y += 40
        }
        else {
            $x = 210
        }
    }
    
    $form.Controls.Add($musicPanel)
    
    # Web Browser Panel
    $webPanel = New-Object System.Windows.Forms.GroupBox
    $webPanel.Text = "🌐 WEB BROWSER"
    $webPanel.ForeColor = [System.Drawing.Color]::Cyan
    $webPanel.Location = New-Object System.Drawing.Point(460, 170)
    $webPanel.Size = New-Object System.Drawing.Size(420, 230)
    $webPanel.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
    
    $websites = @(
        "Amazon", "Amazon Deals", "Netflix", "YouTube",
        "Weather", "News", "Reddit", "GitHub",
        "StackOverflow", "Twitter", "Gmail", "Google"
    )
    
    $x = 20
    $y = 25
    $col = 0
    
    foreach ($site in $websites) {
        $btn = New-Object System.Windows.Forms.Button
        $btn.Text = $site
        $btn.Location = New-Object System.Drawing.Point($x, $y)
        $btn.Size = New-Object System.Drawing.Size(90, 35)
        $btn.BackColor = [System.Drawing.Color]::FromArgb(70, 100, 70)
        $btn.ForeColor = [System.Drawing.Color]::White
        $btn.Font = New-Object System.Drawing.Font("Segoe UI", 9)
        
        $btn.Add_Click({
            $cmd = $Assistant.ParseCommand("go to $($btn.Text)")
            $Assistant.ExecuteCommand($cmd)
            $statusLabel.Text = "⏸️ Paused | Genre: None | URL: $($btn.Text)"
        })
        
        $webPanel.Controls.Add($btn)
        
        $col++
        if ($col % 2 -eq 0) {
            $x = 20
            $y += 40
        }
        else {
            $x = 210
        }
    }
    
    $form.Controls.Add($webPanel)
    
    # Command Input Panel
    $inputPanel = New-Object System.Windows.Forms.GroupBox
    $inputPanel.Text = "📝 VOICE COMMAND INPUT"
    $inputPanel.ForeColor = [System.Drawing.Color]::White
    $inputPanel.Location = New-Object System.Drawing.Point(20, 420)
    $inputPanel.Size = New-Object System.Drawing.Size(860, 100)
    $inputPanel.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
    
    $inputBox = New-Object System.Windows.Forms.TextBox
    $inputBox.Location = New-Object System.Drawing.Point(20, 30)
    $inputBox.Size = New-Object System.Drawing.Size(650, 30)
    $inputBox.BackColor = [System.Drawing.Color]::FromArgb(70, 70, 70)
    $inputBox.ForeColor = [System.Drawing.Color]::White
    $inputBox.Multiline = $false
    $inputBox.Text = "Try: 'play punk rock' or 'go to amazon deals'"
    
    $inputPanel.Controls.Add($inputBox)
    
    $executeBtn = New-Object System.Windows.Forms.Button
    $executeBtn.Text = "Execute"
    $executeBtn.Location = New-Object System.Drawing.Point(690, 30)
    $executeBtn.Size = New-Object System.Drawing.Size(150, 30)
    $executeBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $executeBtn.ForeColor = [System.Drawing.Color]::White
    
    $executeBtn.Add_Click({
        if ($inputBox.Text -and $inputBox.Text -ne "Try: 'play punk rock' or 'go to amazon deals'") {
            $fullCmd = "$($Assistant.Name), $($inputBox.Text)"
            $cmd = $Assistant.ParseCommand($fullCmd)
            $Assistant.ExecuteCommand($cmd)
            
            if ($cmd.Type -eq "music") {
                $statusLabel.Text = "▶️ Playing | Genre: $($cmd.Query) | URL: YouTube"
            }
            elseif ($cmd.Type -eq "web") {
                $statusLabel.Text = "⏸️ Paused | Genre: None | URL: $($cmd.Query)"
            }
            
            $inputBox.Clear()
        }
    })
    
    $inputPanel.Controls.Add($executeBtn)
    $form.Controls.Add($inputPanel)
    
    # History Panel
    $historyPanel = New-Object System.Windows.Forms.GroupBox
    $historyPanel.Text = "📜 COMMAND HISTORY"
    $historyPanel.ForeColor = [System.Drawing.Color]::Magenta
    $historyPanel.Location = New-Object System.Drawing.Point(20, 540)
    $historyPanel.Size = New-Object System.Drawing.Size(860, 120)
    $historyPanel.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
    
    $historyBox = New-Object System.Windows.Forms.ListBox
    $historyBox.Location = New-Object System.Drawing.Point(20, 25)
    $historyBox.Size = New-Object System.Drawing.Size(820, 85)
    $historyBox.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $historyBox.ForeColor = [System.Drawing.Color]::Lime
    $historyBox.Font = New-Object System.Drawing.Font("Courier New", 9)
    
    $historyPanel.Controls.Add($historyBox)
    $form.Controls.Add($historyPanel)
    
    # Update history when commands execute
    $originalExecute = $Assistant | Get-Member -Name ExecuteCommand
    
    # Show form
    $form.ShowDialog() | Out-Null
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host @"

╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║         🎤 $VoiceName - VOICE AI ASSISTANT 🎤                ║
║                                                                ║
║  Commands:                                                    ║
║  • "$VoiceName, play punk rock"                               ║
║  • "$VoiceName, play jazz"                                    ║
║  • "$VoiceName, go to amazon deals"                           ║
║  • "$VoiceName, browse netflix"                               ║
║  • "$VoiceName, search python tutorials"                      ║
║  • "$VoiceName, pause"                                        ║
║  • "$VoiceName, resume"                                       ║
║  • "$VoiceName, next"                                         ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Magenta

$assistant = [VoiceAssistant]::new($VoiceName)

if ($GUI) {
    Write-Host "🎮 Launching GUI Mode..." -ForegroundColor Green
    Show-VoiceAssistantGUI -Assistant $assistant
}
elseif ($TestMode) {
    Write-Host "🧪 Test Mode - Executing demo commands`n" -ForegroundColor Yellow
    
    $testCommands = @(
        "$VoiceName, play punk rock",
        "$VoiceName, go to amazon deals",
        "$VoiceName, search python tutorials",
        "status",
        "pause",
        "resume"
    )
    
    foreach ($cmd in $testCommands) {
        $parsed = $assistant.ParseCommand($cmd)
        $assistant.ExecuteCommand($parsed)
        Start-Sleep -Milliseconds 800
    }
}
else {
    Write-Host "🎤 Voice Command Mode - Type your commands (or 'help' for examples)`n" -ForegroundColor Green
    
    while ($true) {
        Write-Host "`n💬 You: " -ForegroundColor Yellow -NoNewline
        $input = Read-Host
        
        if ($input -eq "quit" -or $input -eq "exit") {
            Write-Host "`n👋 Goodbye!" -ForegroundColor Green
            $assistant.Speak("Goodbye!")
            break
        }
        
        if ($input -eq "help") {
            Write-Host @"
EXAMPLES:
  play punk rock       - Play punk rock music
  play jazz            - Play jazz music
  go to amazon deals   - Open Amazon deals
  browse netflix       - Open Netflix
  search python        - Search for Python
  pause                - Pause music
  resume               - Resume music
  next                 - Skip to next
  status               - Show current status
  quit                 - Exit
"@ -ForegroundColor Cyan
            continue
        }
        
        $parsed = $assistant.ParseCommand($input)
        $assistant.ExecuteCommand($parsed)
    }
}
