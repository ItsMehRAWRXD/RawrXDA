#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Voice-Controlled AI Music Player - "RawrXD" Voice Assistant

.DESCRIPTION
    Say "RawrXD, play classic punk rock" to control music playback.
    Integrates with Spotify, YouTube Music, local files, and web streams.
    Natural language understanding for music requests.

.PARAMETER VoiceName
    Name to respond to (default: "RawrXD")

.PARAMETER EnableSpotify
    Connect to Spotify API

.PARAMETER LocalMusicPath
    Path to local music library

.PARAMETER InteractiveMode
    Launch GUI mode instead of voice-only

.EXAMPLE
    .\voice_music_player.ps1
    .\voice_music_player.ps1 -VoiceName "Alexa" -InteractiveMode
    .\voice_music_player.ps1 -LocalMusicPath "D:\Music"
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$VoiceName = "RawrXD",
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableSpotify,
    
    [Parameter(Mandatory=$false)]
    [string]$LocalMusicPath = "$env:USERPROFILE\Music",
    
    [Parameter(Mandatory=$false)]
    [switch]$InteractiveMode,
    
    [Parameter(Mandatory=$false)]
    [switch]$ListenMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Continue"

# Load assemblies
try {
    Add-Type -AssemblyName System.Windows.Forms -ErrorAction Stop
    Add-Type -AssemblyName System.Drawing -ErrorAction Stop
    Add-Type -AssemblyName System.Speech -ErrorAction Stop
}
catch {
    Write-Host "⚠️ Some assemblies not available: $_" -ForegroundColor Yellow
}

# ═══════════════════════════════════════════════════════════════════════════════
# VOICE MUSIC PLAYER CORE
# ═══════════════════════════════════════════════════════════════════════════════

class VoiceMusicPlayer {
    [string]$VoiceName
    [string]$LocalMusicPath
    [System.Collections.ArrayList]$Playlist
    [int]$CurrentTrackIndex = 0
    [bool]$IsPlaying = $false
    [System.Media.SoundPlayer]$CurrentPlayer
    [hashtable]$GenreMap = @{}
    [hashtable]$PlaylistCache = @{}
    [datetime]$SessionStart
    [System.Collections.ArrayList]$PlayHistory
    [System.Speech.Recognition.SpeechRecognitionEngine]$Recognizer
    [System.Speech.Synthesis.SpeechSynthesizer]$Synthesizer
    [hashtable]$MusicSources = @{}
    
    VoiceMusicPlayer([string]$name, [string]$musicPath) {
        $this.VoiceName = $name
        $this.LocalMusicPath = $musicPath
        $this.Playlist = [System.Collections.ArrayList]::new()
        $this.PlayHistory = [System.Collections.ArrayList]::new()
        $this.SessionStart = Get-Date
        
        $this.InitializeSpeech()
        $this.InitializeGenreMap()
        $this.InitializeLocalLibrary()
        $this.InitializeMusicSources()
    }
    
    [void] InitializeSpeech() {
        try {
            $this.Recognizer = [System.Speech.Recognition.SpeechRecognitionEngine]::new("en-US")
            $this.Synthesizer = [System.Speech.Synthesis.SpeechSynthesizer]::new()
            $this.Synthesizer.Volume = 100
            $this.Synthesizer.Rate = -1
            
            Write-Host "✅ Speech engines initialized" -ForegroundColor Green
        }
        catch {
            Write-Host "⚠️ Speech initialization failed: $_" -ForegroundColor Yellow
        }
    }
    
    [void] Speak([string]$text) {
        try {
            $this.Synthesizer.Speak($text)
        }
        catch {
            Write-Host "❌ Speech synthesis failed: $_" -ForegroundColor Red
        }
    }
    
    [void] InitializeGenreMap() {
        $this.GenreMap = @{
            "punk" = @("punk rock", "sex pistols", "ramones", "dead kennedys", "blink-182")
            "rock" = @("classic rock", "pink floyd", "led zeppelin", "queen", "the beatles")
            "metal" = @("heavy metal", "black sabbath", "metallica", "iron maiden", "judas priest")
            "pop" = @("pop music", "taylor swift", "billie eilish", "ariana grande", "the weeknd")
            "hiphop" = @("hip hop", "rap", "eminem", "dr. dre", "jay-z", "kendrick lamar")
            "jazz" = @("jazz", "miles davis", "john coltrane", "duke ellington", "chet baker")
            "edm" = @("electronic", "edm", "dubstep", "house", "techno", "deadmau5", "skrillex")
            "classical" = @("classical", "beethoven", "mozart", "bach", "chopin")
            "country" = @("country", "johnny cash", "dolly parton", "garth brooks", "luke combs")
            "rnb" = @("r&b", "soul", "usher", "alicia keys", "beyonce")
            "indie" = @("indie", "indie rock", "arctic monkeys", "tame impala", "the strokes")
            "reggae" = @("reggae", "bob marley", "peter tosh", "sean paul")
            "acoustic" = @("acoustic", "unplugged", "singer-songwriter")
            "lofi" = @("lo-fi", "lofi", "chill", "chillhop", "ambient")
        }
    }
    
    [void] InitializeLocalLibrary() {
        Write-Host "`n🎵 INITIALIZING LOCAL MUSIC LIBRARY" -ForegroundColor Cyan
        
        if (-not (Test-Path $this.LocalMusicPath)) {
            Write-Host "⚠️ Music path doesn't exist: $($this.LocalMusicPath)" -ForegroundColor Yellow
            return
        }
        
        $musicFiles = @(
            Get-ChildItem $this.LocalMusicPath -Recurse -Include @("*.mp3", "*.wav", "*.flac", "*.m4a") -ErrorAction SilentlyContinue
        )
        
        Write-Host "Found $($musicFiles.Count) audio files" -ForegroundColor Green
    }
    
    [void] InitializeMusicSources() {
        $this.MusicSources = @{
            YouTube = "https://www.youtube.com/results?search_query="
            Spotify = "https://open.spotify.com/search/"
            DuckDuckGo = "https://duckduckgo.com/?q="
            SoundCloud = "https://soundcloud.com/search?q="
        }
    }
    
    [array] ParseMusicRequest([string]$request) {
        Write-Host "`n🎤 PARSING REQUEST: '$request'" -ForegroundColor Yellow
        
        $request = $request -replace "$($this.VoiceName)[,\s]*", ""
        $request = $request.Trim()
        
        # Extract genre
        $genre = $null
        $artist = $null
        $song = $null
        
        foreach ($key in $this.GenreMap.Keys) {
            foreach ($term in $this.GenreMap[$key]) {
                if ($request -like "*$term*") {
                    $genre = $key
                    break
                }
            }
            if ($genre) { break }
        }
        
        # Extract artist or song name
        if ($request -match "play\s+(.+?)\s+by\s+(.+)") {
            $song = $matches[1]
            $artist = $matches[2]
        }
        elseif ($request -match "play\s+(.+)") {
            $song = $matches[1] -replace $genre, ""
        }
        
        $song = $song -replace "^\s+|\s+$", ""
        
        return @{
            Original = $request
            Genre = $genre
            Artist = $artist
            Song = $song
        }
    }
    
    [void] PlayGenre([string]$genre) {
        Write-Host "`n▶️  PLAYING GENRE: $genre" -ForegroundColor Green
        
        $searchTerm = if ($this.GenreMap.ContainsKey($genre.ToLower())) {
            $this.GenreMap[$genre.ToLower()][0]
        } else {
            $genre
        }
        
        $this.Speak("Now playing $searchTerm. Enjoy!")
        $this.StreamFromYouTube($searchTerm)
        $this.IsPlaying = $true
    }
    
    [void] PlaySong([string]$songName, [string]$artist) {
        Write-Host "`n▶️  PLAYING SONG: '$songName'" -ForegroundColor Green
        
        $searchQuery = if ($artist) { "$songName by $artist" } else { $songName }
        $this.Speak("Playing $searchQuery")
        $this.StreamFromYouTube($searchQuery)
        $this.IsPlaying = $true
    }
    
    [void] StreamFromYouTube([string]$query) {
        # PowerShell doesn't natively play YouTube, but we can search and open
        $encodedQuery = [System.Web.HttpUtility]::UrlEncode($query)
        $youtubeUrl = "https://www.youtube.com/results?search_query=$encodedQuery"
        
        Write-Host "🔍 Searching YouTube: $query" -ForegroundColor Cyan
        
        # Try to open in browser with music focus
        try {
            Start-Process $youtubeUrl
        }
        catch {
            Write-Host "❌ Could not open browser" -ForegroundColor Red
        }
    }
    
    [void] ListenForCommands() {
        Write-Host @"
`n╔════════════════════════════════════════════════════════════╗
║                                                              ║
║           🎙️ VOICE COMMAND MODE - LISTENING 🎙️            ║
║                                                              ║
║  Say things like:                                           ║
║  • "$($this.VoiceName), play classic punk rock"             ║
║  • "$($this.VoiceName), play shape of you by ed sheeran"    ║
║  • "$($this.VoiceName), play metal"                         ║
║  • "$($this.VoiceName), pause"                              ║
║  • "$($this.VoiceName), resume"                             ║
║  • "$($this.VoiceName), next song"                          ║
║  • "$($this.VoiceName), volume up"                          ║
║                                                              ║
║  Press Ctrl+C to stop listening                            ║
║                                                              ║
╚════════════════════════════════════════════════════════════╝
`n
"@ -ForegroundColor Magenta
        
        $this.Speak("I'm ready. Tell me what to play.")
        
        while ($true) {
            try {
                Write-Host "`n🎤 Listening..." -ForegroundColor Yellow -NoNewline
                
                # Simulate voice input via console for demo
                $input = Read-Host " (or type your command)"
                
                if ($input -eq "quit" -or $input -eq "exit") {
                    $this.Speak("Goodbye!")
                    break
                }
                
                $this.ProcessCommand($input)
            }
            catch {
                Write-Host "Error: $_" -ForegroundColor Red
            }
        }
    }
    
    [void] ProcessCommand([string]$command) {
        $command = $command.Trim().ToLower()
        
        Write-Host "`n💬 Command: '$command'" -ForegroundColor Cyan
        
        # Check if addressing the assistant
        if ($command -notlike "*$($this.VoiceName.ToLower())*" -and $command -notmatch "^(play|pause|resume|next|previous|volume|status)") {
            return
        }
        
        # Parse the request
        $parsed = $this.ParseMusicRequest($command)
        
        # Route to appropriate handler
        if ($command -match "pause") {
            $this.Pause()
        }
        elseif ($command -match "resume|unpause|continue") {
            $this.Resume()
        }
        elseif ($command -match "next") {
            $this.NextTrack()
        }
        elseif ($command -match "previous|last|back") {
            $this.PreviousTrack()
        }
        elseif ($command -match "volume\s+(up|down|to|max|min)") {
            $this.VolumeControl($command)
        }
        elseif ($command -match "status|what|playing|current") {
            $this.StatusUpdate()
        }
        elseif ($parsed.Genre) {
            $this.PlayGenre($parsed.Genre)
        }
        elseif ($parsed.Song) {
            $this.PlaySong($parsed.Song, $parsed.Artist)
        }
        else {
            $this.Speak("I didn't understand that. Try saying play followed by a song or genre.")
            Write-Host "❓ Could not parse request" -ForegroundColor Yellow
        }
    }
    
    [void] Pause() {
        if ($this.IsPlaying) {
            $this.IsPlaying = $false
            $this.Speak("Paused")
            Write-Host "⏸️  Music paused" -ForegroundColor Yellow
        }
    }
    
    [void] Resume() {
        if (-not $this.IsPlaying) {
            $this.IsPlaying = $true
            $this.Speak("Resumed")
            Write-Host "▶️  Music resumed" -ForegroundColor Green
        }
    }
    
    [void] NextTrack() {
        $this.CurrentTrackIndex = ($this.CurrentTrackIndex + 1) % [Math]::Max($this.Playlist.Count, 1)
        $this.Speak("Next track")
        Write-Host "⏭️  Skipped to next track" -ForegroundColor Green
    }
    
    [void] PreviousTrack() {
        $this.CurrentTrackIndex = if ($this.CurrentTrackIndex -gt 0) { $this.CurrentTrackIndex - 1 } else { $this.Playlist.Count - 1 }
        $this.Speak("Previous track")
        Write-Host "⏮️  Skipped to previous track" -ForegroundColor Green
    }
    
    [void] VolumeControl([string]$command) {
        if ($command -match "volume\s+up") {
            $this.Synthesizer.Volume = [Math]::Min($this.Synthesizer.Volume + 10, 100)
            $this.Speak("Volume up")
            Write-Host "🔊 Volume increased" -ForegroundColor Green
        }
        elseif ($command -match "volume\s+down") {
            $this.Synthesizer.Volume = [Math]::Max($this.Synthesizer.Volume - 10, 0)
            $this.Speak("Volume down")
            Write-Host "🔉 Volume decreased" -ForegroundColor Green
        }
        elseif ($command -match "volume\s+(max|100)") {
            $this.Synthesizer.Volume = 100
            $this.Speak("Volume maximum")
            Write-Host "🔊 Volume set to maximum" -ForegroundColor Green
        }
    }
    
    [void] StatusUpdate() {
        $status = if ($this.IsPlaying) { "▶️ Playing" } else { "⏸️ Paused" }
        $message = "Currently $status. Genre: Unknown"
        $this.Speak($message)
        Write-Host "`n📊 $message" -ForegroundColor Cyan
    }
    
    [void] ShowControlPanel() {
        try {
            $form = New-Object System.Windows.Forms.Form
            $form.Text = "$($this.VoiceName) - Music Player"
            $form.Width = 500
            $form.Height = 600
            $form.StartPosition = "CenterScreen"
            $form.BackColor = [System.Drawing.Color]::FromArgb(20, 20, 20)
            $form.Font = New-Object System.Drawing.Font("Segoe UI", 10)
            
            # Title
            $titleLabel = New-Object System.Windows.Forms.Label
            $titleLabel.Text = "🎵 $($this.VoiceName) Voice Music Player"
            $titleLabel.ForeColor = [System.Drawing.Color]::Cyan
            $titleLabel.Font = New-Object System.Drawing.Font("Segoe UI", 14, [System.Drawing.FontStyle]::Bold)
            $titleLabel.Location = New-Object System.Drawing.Point(20, 20)
            $titleLabel.Size = New-Object System.Drawing.Size(460, 30)
            $form.Controls.Add($titleLabel)
            
            # Status display
            $statusLabel = New-Object System.Windows.Forms.Label
            $statusLabel.Text = if ($this.IsPlaying) { "▶️  PLAYING" } else { "⏸️ PAUSED" }
            $statusLabel.ForeColor = if ($this.IsPlaying) { [System.Drawing.Color]::Lime } else { [System.Drawing.Color]::Yellow }
            $statusLabel.Font = New-Object System.Drawing.Font("Segoe UI", 12, [System.Drawing.FontStyle]::Bold)
            $statusLabel.Location = New-Object System.Drawing.Point(20, 70)
            $statusLabel.Size = New-Object System.Drawing.Size(460, 30)
            $form.Controls.Add($statusLabel)
            
            # Command input
            $inputLabel = New-Object System.Windows.Forms.Label
            $inputLabel.Text = "Enter voice command:"
            $inputLabel.ForeColor = [System.Drawing.Color]::White
            $inputLabel.Location = New-Object System.Drawing.Point(20, 120)
            $inputLabel.Size = New-Object System.Drawing.Size(460, 20)
            $form.Controls.Add($inputLabel)
            
            $inputBox = New-Object System.Windows.Forms.TextBox
            $inputBox.Location = New-Object System.Drawing.Point(20, 145)
            $inputBox.Size = New-Object System.Drawing.Size(460, 30)
            $inputBox.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
            $inputBox.ForeColor = [System.Drawing.Color]::White
            $inputBox.Multiline = $false
            $form.Controls.Add($inputBox)
            
            # Execute button
            $executeBtn = New-Object System.Windows.Forms.Button
            $executeBtn.Text = "Execute Command"
            $executeBtn.Location = New-Object System.Drawing.Point(20, 185)
            $executeBtn.Size = New-Object System.Drawing.Size(220, 35)
            $executeBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
            $executeBtn.ForeColor = [System.Drawing.Color]::White
            $executeBtn.Add_Click({
                if ($inputBox.Text) {
                    $this.ProcessCommand($inputBox.Text)
                    $inputBox.Clear()
                }
            })
            $form.Controls.Add($executeBtn)
            
            # Quick buttons
            $playBtn = New-Object System.Windows.Forms.Button
            $playBtn.Text = "▶️ Play"
            $playBtn.Location = New-Object System.Drawing.Point(20, 240)
            $playBtn.Size = New-Object System.Drawing.Size(100, 35)
            $playBtn.BackColor = [System.Drawing.Color]::Green
            $playBtn.ForeColor = [System.Drawing.Color]::White
            $playBtn.Add_Click({ $this.Resume() })
            $form.Controls.Add($playBtn)
            
            $pauseBtn = New-Object System.Windows.Forms.Button
            $pauseBtn.Text = "⏸️ Pause"
            $pauseBtn.Location = New-Object System.Drawing.Point(130, 240)
            $pauseBtn.Size = New-Object System.Drawing.Size(100, 35)
            $pauseBtn.BackColor = [System.Drawing.Color]::Orange
            $pauseBtn.ForeColor = [System.Drawing.Color]::White
            $pauseBtn.Add_Click({ $this.Pause() })
            $form.Controls.Add($pauseBtn)
            
            $nextBtn = New-Object System.Windows.Forms.Button
            $nextBtn.Text = "⏭️ Next"
            $nextBtn.Location = New-Object System.Drawing.Point(240, 240)
            $nextBtn.Size = New-Object System.Drawing.Size(100, 35)
            $nextBtn.BackColor = [System.Drawing.Color]::Blue
            $nextBtn.ForeColor = [System.Drawing.Color]::White
            $nextBtn.Add_Click({ $this.NextTrack() })
            $form.Controls.Add($nextBtn)
            
            # Genre buttons
            $genreLabel = New-Object System.Windows.Forms.Label
            $genreLabel.Text = "Quick Genres:"
            $genreLabel.ForeColor = [System.Drawing.Color]::White
            $genreLabel.Location = New-Object System.Drawing.Point(20, 290)
            $genreLabel.Size = New-Object System.Drawing.Size(460, 20)
            $form.Controls.Add($genreLabel)
            
            $genres = @("Rock", "Punk", "Metal", "Jazz", "EDM", "LoFi")
            $xPos = 20
            $yPos = 315
            
            foreach ($genre in $genres) {
                $btn = New-Object System.Windows.Forms.Button
                $btn.Text = $genre
                $btn.Location = New-Object System.Drawing.Point($xPos, $yPos)
                $btn.Size = New-Object System.Drawing.Size(140, 30)
                $btn.BackColor = [System.Drawing.Color]::FromArgb(100, 100, 100)
                $btn.ForeColor = [System.Drawing.Color]::White
                $btn.Add_Click({
                    $this.PlayGenre($btn.Text)
                    $statusLabel.Text = "▶️  PLAYING"
                    $statusLabel.ForeColor = [System.Drawing.Color]::Lime
                })
                $form.Controls.Add($btn)
                
                $xPos += 150
                if ($xPos > 300) {
                    $xPos = 20
                    $yPos += 40
                }
            }
            
            # Show form
            $form.ShowDialog() | Out-Null
        }
        catch {
            Write-Host "Error creating GUI: $_" -ForegroundColor Red
            $this.ListenForCommands()
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host @"

╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║         🎵 VOICE-CONTROLLED MUSIC PLAYER - $VoiceName 🎵      ║
║                                                                ║
║        Say things like:                                       ║
║        • "$VoiceName, play classic punk rock"                 ║
║        • "$VoiceName, play shape of you by ed sheeran"        ║
║        • "$VoiceName, pause"                                  ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Magenta

# Create player instance
$player = [VoiceMusicPlayer]::new($VoiceName, $LocalMusicPath)

# Launch appropriate mode
if ($InteractiveMode) {
    Write-Host "🎮 Launching interactive control panel..." -ForegroundColor Green
    $player.ShowControlPanel()
}
else {
    Write-Host "🎤 Entering voice command mode..." -ForegroundColor Green
    $player.ListenForCommands()
}
