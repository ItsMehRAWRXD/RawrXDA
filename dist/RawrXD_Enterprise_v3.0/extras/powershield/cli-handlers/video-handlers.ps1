<#
.SYNOPSIS
    Video-related CLI command handlers
.DESCRIPTION
    Handles video-search, video-download, video-play, and video-help commands
#>

function Invoke-VideoSearchHandler {
    param([string]$Query)
    
    Write-Host "`n=== 🎬 Agentic Video Search ===" -ForegroundColor Cyan
    if (-not $Query) {
        Write-Host "Error: -Prompt parameter required for search query" -ForegroundColor Red
        Write-Host "Usage: .\RawrXD.ps1 -CliMode -Command video-search -Prompt 'python tutorial'" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Examples:" -ForegroundColor Gray
        Write-Host "  .\RawrXD.ps1 -CliMode -Command video-search -Prompt 'machine learning'" -ForegroundColor Gray
        Write-Host "  curl equivalent:" -ForegroundColor Gray
        Write-Host "  curl -X POST http://localhost:11434/api/generate -d '{\"model\":\"llama2\",\"prompt\":\"search youtube for python\"}'" -ForegroundColor DarkGray
        return 1
    }
    
    try {
        Write-Host "Searching YouTube for: '$Query'" -ForegroundColor Yellow
        Write-Host ""
        
        if (Get-Command Search-YouTubeFromBrowser -ErrorAction SilentlyContinue) {
            $results = Search-YouTubeFromBrowser -Query $Query -MaxResults 10
            
            if ($results.Count -eq 0) {
                Write-Host "❌ No results found for '$Query'" -ForegroundColor Red
            }
            else {
                Write-Host "✅ Found $($results.Count) video(s):`n" -ForegroundColor Green
                
                $counter = 1
                foreach ($video in $results) {
                    Write-Host "  $counter. 🎬 $($video.Title)" -ForegroundColor White
                    Write-Host "     Channel: $($video.Channel) | Duration: $($video.Duration)" -ForegroundColor Gray
                    Write-Host "     Views: $($video.Views) | URL: $($video.URL)" -ForegroundColor DarkGray
                    Write-Host ""
                    $counter++
                }
                
                Write-Host "💡 Use video-play or video-download with -URL parameter" -ForegroundColor Yellow
                
                Write-Host "`n--- JSON Output (for curl/API) ---" -ForegroundColor Cyan
                $jsonOutput = @{
                    status  = "success"
                    query   = $Query
                    count   = $results.Count
                    results = $results
                } | ConvertTo-Json -Depth 5
                Write-Host $jsonOutput -ForegroundColor Gray
            }
            return 0
        }
        else {
            Write-Host "❌ Video engine not loaded. Ensure BrowserAutomation.ps1 exists." -ForegroundColor Red
            return 1
        }
    }
    catch {
        Write-Host "❌ Error searching: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

function Invoke-VideoDownloadHandler {
    param(
        [string]$URL,
        [string]$OutputPath
    )
    
    Write-Host "`n=== 📥 Agentic Video Download ===" -ForegroundColor Cyan
    if (-not $URL) {
        Write-Host "Error: -URL parameter required for download" -ForegroundColor Red
        Write-Host "Usage: .\RawrXD.ps1 -CliMode -Command video-download -URL 'https://...' [-OutputPath 'C:\Videos']" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Examples:" -ForegroundColor Gray
        Write-Host "  .\RawrXD.ps1 -CliMode -Command video-download -URL 'https://youtube.com/watch?v=...' -OutputPath '~/Videos'" -ForegroundColor Gray
        return 1
    }
    
    try {
        $destination = if ($OutputPath) { $OutputPath } else { "$env:USERPROFILE\Videos" }
        Write-Host "Downloading: $URL" -ForegroundColor Yellow
        Write-Host "Destination: $destination" -ForegroundColor Gray
        Write-Host ""
        
        if (Get-Command Invoke-MultiThreadedDownload -ErrorAction SilentlyContinue) {
            $filename = if ($URL -match 'v=([a-zA-Z0-9_-]+)') {
                "video_$($matches[1]).mp4"
            }
            else {
                "download_$(Get-Date -Format 'yyyyMMdd_HHmmss').mp4"
            }
            
            $outputFile = Join-Path $destination $filename
            
            Write-Host "⏳ Starting multi-threaded download (4 threads)..." -ForegroundColor Yellow
            
            $result = Invoke-MultiThreadedDownload -URL $URL -OutputPath $outputFile -ThreadCount 4 -MaxRetries 3
            
            if ($result.Success) {
                Write-Host ""
                Write-Host "✅ Download Complete!" -ForegroundColor Green
                Write-Host "   File: $($result.FilePath)" -ForegroundColor White
                Write-Host "   Size: $(Format-Bytes $result.FileSize)" -ForegroundColor Gray
                Write-Host "   Speed: $($result.Speed)" -ForegroundColor Gray
                Write-Host "   Duration: $($result.Duration)" -ForegroundColor Gray
                
                Write-Host "`n--- JSON Output ---" -ForegroundColor Cyan
                $result | ConvertTo-Json | Write-Host -ForegroundColor Gray
                return 0
            }
            else {
                Write-Host "❌ Download failed: $($result.Error)" -ForegroundColor Red
                return 1
            }
        }
        else {
            Write-Host "❌ Download manager not loaded. Ensure DownloadManager.ps1 exists." -ForegroundColor Red
            return 1
        }
    }
    catch {
        Write-Host "❌ Error downloading: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

function Invoke-VideoPlayHandler {
    param([string]$URL)
    
    Write-Host "`n=== ▶️ Agentic Video Play ===" -ForegroundColor Cyan
    if (-not $URL) {
        Write-Host "Error: -URL parameter required for playback" -ForegroundColor Red
        Write-Host "Usage: .\RawrXD.ps1 -CliMode -Command video-play -URL 'https://...'" -ForegroundColor Yellow
        return 1
    }
    
    try {
        Write-Host "Opening video: $URL" -ForegroundColor Yellow
        
        Start-Process $URL
        
        Write-Host "✅ Video opened in default browser" -ForegroundColor Green
        Write-Host ""
        Write-Host "--- JSON Output ---" -ForegroundColor Cyan
        @{
            status = "success"
            action = "play"
            url    = $URL
            method = "default_browser"
        } | ConvertTo-Json | Write-Host -ForegroundColor Gray
        return 0
    }
    catch {
        Write-Host "❌ Error playing: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

function Invoke-VideoHelpHandler {
    Write-Host "`n=== 🎬 Agentic Video Engine Help ===" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "COMMANDS:" -ForegroundColor Yellow
    Write-Host "  video-search    Search YouTube for videos" -ForegroundColor White
    Write-Host "  video-download  Download video from URL" -ForegroundColor White
    Write-Host "  video-play      Play video in browser" -ForegroundColor White
    Write-Host "  browser-navigate Open URL in browser" -ForegroundColor White
    Write-Host "  browser-click   Click element by selector" -ForegroundColor White
    Write-Host "  browser-screenshot Capture browser screenshot" -ForegroundColor White
    Write-Host ""
    Write-Host "PARAMETERS:" -ForegroundColor Yellow
    Write-Host "  -Prompt     Search query (for video-search)" -ForegroundColor Gray
    Write-Host "  -URL        Video/page URL (for download/play/navigate)" -ForegroundColor Gray
    Write-Host "  -OutputPath Destination folder (for downloads)" -ForegroundColor Gray
    Write-Host "  -Selector   CSS selector (for browser-click)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "EXAMPLES:" -ForegroundColor Yellow
    Write-Host "  # Search for videos" -ForegroundColor Gray
    Write-Host "  .\RawrXD.ps1 -CliMode -Command video-search -Prompt 'powershell tutorial'" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "  # Download a video" -ForegroundColor Gray
    Write-Host "  .\RawrXD.ps1 -CliMode -Command video-download -URL 'https://...' -OutputPath '~/Videos'" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "  # Play a video" -ForegroundColor Gray
    Write-Host "  .\RawrXD.ps1 -CliMode -Command video-play -URL 'https://youtube.com/watch?v=...'" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "CURL INTEGRATION:" -ForegroundColor Yellow
    Write-Host "  All commands output JSON for easy scripting/curl integration." -ForegroundColor Gray
    Write-Host "  Pipe output through 'ConvertFrom-Json' for structured data." -ForegroundColor Gray
    Write-Host ""
    Write-Host "GUI COMMANDS (in chat):" -ForegroundColor Yellow
    Write-Host "  /search youtube <query>  - Search YouTube" -ForegroundColor Gray
    Write-Host "  /download <url|number>   - Download video" -ForegroundColor Gray
    Write-Host "  /play <url|number>       - Play video" -ForegroundColor Gray
    Write-Host "  /playlist create <topic> - Create playlist" -ForegroundColor Gray
    Write-Host "  /video-help              - Show this help" -ForegroundColor Gray
    return 0
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
# Functions exported:
#   'Invoke-VideoSearchHandler',
#   'Invoke-VideoDownloadHandler',
#   'Invoke-VideoPlayHandler',
#   'Invoke-VideoHelpHandler'






































