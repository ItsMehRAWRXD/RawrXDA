# Agentic Browser Automation Functions
# Phase 1: JavaScript Injection & YouTube Search
# Location: To be integrated into RawrXD.ps1

# ============================================================================
# BROWSER AUTOMATION ENGINE - JAVASCRIPT INJECTION FRAMEWORK
# ============================================================================

<#
.SYNOPSIS
    Injects JavaScript into WebView2 and captures return values
    
.DESCRIPTION
    Core framework for executing JavaScript in the browser tab
    Handles error cases, timeout, and return value marshalling
    
.PARAMETER ScriptCode
    JavaScript code to execute (string)
    
.PARAMETER Timeout
    Timeout in seconds (default: 10)
    
.EXAMPLE
    $result = Invoke-BrowserScript -ScriptCode "document.title"
    Returns the current page title
#>
function Invoke-BrowserScript {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ScriptCode,
        
        [int]$Timeout = 10,
        [bool]$CaptureReturn = $true
    )
    
    try {
        if (-not $script:webBrowser -or -not $script:webBrowser.CoreWebView2) {
            Write-DevConsole "⚠️ WebView2 not initialized" "ERROR"
            return $null
        }
        
        # Wrap script to handle return values
        $wrappedScript = if ($CaptureReturn) {
            "(async function() { 
                try { 
                    let result = await (async function() { $ScriptCode })();
                    return JSON.stringify({ success: true, result: result });
                } catch(e) { 
                    return JSON.stringify({ success: false, error: e.message });
                }
            })()"
        } else {
            $ScriptCode
        }
        
        # Execute with timeout
        $task = $script:webBrowser.CoreWebView2.ExecuteScriptAsync($wrappedScript)
        $completed = $task.Wait([timespan]::FromSeconds($Timeout))
        
        if (-not $completed) {
            Write-DevConsole "⏱️ Browser script timeout after $Timeout seconds" "WARNING"
            return $null
        }
        
        # Parse result
        $resultJson = $task.Result
        if ([string]::IsNullOrWhiteSpace($resultJson)) {
            return $null
        }
        
        $parsed = $resultJson | ConvertFrom-Json
        if ($parsed.success) {
            return $parsed.result
        } else {
            Write-DevConsole "❌ Script error: $($parsed.error)" "ERROR"
            return $null
        }
        
    }
    catch {
        Write-DevConsole "Browser automation error: $_" "ERROR"
        Write-ErrorLog -ErrorMessage "Invoke-BrowserScript failed: $_" -ErrorCategory "BROWSER" -Severity "MEDIUM"
        return $null
    }
}

# ============================================================================
# YOUTUBE SEARCH AUTOMATION
# ============================================================================

<#
.SYNOPSIS
    Searches YouTube from the browser for videos
    
.DESCRIPTION
    Uses JavaScript injection to search YouTube and extract results
    Returns: videoId, title, channelName, duration, views, uploadDate
    
.PARAMETER Query
    Search query string (e.g., "machine learning tutorials")
    
.PARAMETER MaxResults
    Number of results to return (default: 10, max: 50)
    
.EXAMPLE
    $results = Search-YouTubeFromBrowser "python programming"
    
    Returns:
    @(
        @{
            VideoId = "dQw4w9WgXcQ"
            Title = "Python Programming Tutorial"
            Channel = "Coding Academy"
            Duration = "3:45:20"
            Views = "1.2M"
            UploadDate = "2 weeks ago"
            URL = "https://youtube.com/watch?v=dQw4w9WgXcQ"
            Thumbnail = "https://i.ytimg.com/vi/dQw4w9WgXcQ/maxresdefault.jpg"
        }
        ...
    )
#>
function Search-YouTubeFromBrowser {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Query,
        
        [int]$MaxResults = 10
    )
    
    try {
        if ($MaxResults -gt 50) { $MaxResults = 50 }
        
        Write-DevConsole "🔍 Searching YouTube: '$Query'" "INFO"
        
        # Navigate to YouTube if not already there
        if ($script:webBrowser.CoreWebView2.Source -notlike "*youtube.com*") {
            Open-Browser "https://www.youtube.com"
            Start-Sleep -Milliseconds 1000
        }
        
        # JavaScript to search YouTube
        $searchScript = @"
        (async function() {
            try {
                // Click search box
                let searchBox = document.querySelector('input[id="search"]') || 
                                document.querySelector('input[placeholder*="Search"]');
                if (!searchBox) {
                    return { success: false, error: "Search box not found" };
                }
                
                // Clear existing search
                searchBox.value = '';
                searchBox.focus();
                
                // Type search query
                searchBox.value = '$Query';
                searchBox.dispatchEvent(new Event('input', { bubbles: true }));
                
                // Wait a bit for suggestions
                await new Promise(r => setTimeout(r, 500));
                
                // Press Enter
                let event = new KeyboardEvent('keydown', { 
                    key: 'Enter', 
                    code: 'Enter', 
                    keyCode: 13 
                });
                searchBox.dispatchEvent(event);
                
                // Wait for results to load
                await new Promise(r => setTimeout(r, 2000));
                
                // Extract video results
                let results = [];
                let videoElements = document.querySelectorAll('ytd-video-renderer, ytd-grid-video-renderer');
                
                for (let i = 0; i < Math.min(videoElements.length, $MaxResults); i++) {
                    let elem = videoElements[i];
                    
                    try {
                        // Get video link
                        let link = elem.querySelector('a#video-title-link') || 
                                   elem.querySelector('a#video-title');
                        if (!link) continue;
                        
                        let videoId = link.href.split('v=')[1]?.split('&')[0];
                        if (!videoId) continue;
                        
                        // Get title
                        let title = link.getAttribute('title') || link.textContent.trim();
                        
                        // Get channel
                        let channelLink = elem.querySelector('a#channel-name');
                        let channel = channelLink?.textContent.trim() || 'Unknown';
                        
                        // Get duration
                        let durationElem = elem.querySelector('span.style-scope.ytd-thumbnail-overlay-time-status-renderer');
                        let duration = durationElem?.textContent.trim() || 'N/A';
                        
                        // Get views
                        let metadataLines = Array.from(elem.querySelectorAll('#metadata-line')).map(el => el.textContent.trim());
                        let views = metadataLines[0] || 'N/A';
                        let uploadDate = metadataLines[1] || 'N/A';
                        
                        // Get thumbnail
                        let imgElem = elem.querySelector('img[alt*="Thumbnail"]') || 
                                      elem.querySelector('img[loading="lazy"]');
                        let thumbnail = imgElem?.src || '';
                        
                        results.push({
                            videoId: videoId,
                            title: title,
                            channel: channel,
                            duration: duration,
                            views: views,
                            uploadDate: uploadDate,
                            url: 'https://youtube.com/watch?v=' + videoId,
                            thumbnail: thumbnail
                        });
                        
                    } catch (e) {
                        // Skip this result
                    }
                }
                
                return {
                    success: true,
                    count: results.length,
                    results: results
                };
                
            } catch (e) {
                return { success: false, error: e.message };
            }
        })()
"@
        
        $result = Invoke-BrowserScript -ScriptCode $searchScript -Timeout 15
        
        if ($result -and $result.success) {
            Write-DevConsole "✅ Found $($result.count) YouTube videos" "SUCCESS"
            return $result.results
        } else {
            Write-DevConsole "❌ YouTube search failed: $($result.error)" "ERROR"
            return @()
        }
        
    }
    catch {
        Write-DevConsole "YouTube search error: $_" "ERROR"
        Write-ErrorLog -ErrorMessage "Search-YouTubeFromBrowser failed: $_" -ErrorCategory "BROWSER" -Severity "MEDIUM"
        return @()
    }
}

# ============================================================================
# VIDEO EXTRACTION
# ============================================================================

<#
.SYNOPSIS
    Extracts video metadata from current YouTube page
    
.DESCRIPTION
    Gets title, description, channel, duration from the video page
    Useful for confirming video before playing
#>
function Get-YouTubeVideoMetadata {
    param([string]$VideoId)
    
    try {
        # Navigate to video if not already there
        if ($script:webBrowser.CoreWebView2.Source -notlike "*youtube.com/watch?v=$VideoId*") {
            Open-Browser "https://www.youtube.com/watch?v=$VideoId"
            Start-Sleep -Milliseconds 2000
        }
        
        $metadataScript = @"
        (async function() {
            try {
                let data = {
                    title: document.querySelector('h1.title yt-formatted-string')?.textContent.trim() || 
                            document.querySelector('h1 yt-formatted-string')?.textContent.trim() || 
                            'Unknown',
                    channel: document.querySelector('a.yt-user-name')?.textContent.trim() || 
                             document.querySelector('ytd-channel-tagline-renderer-replacement a')?.textContent.trim() ||
                             'Unknown',
                    duration: document.querySelector('span.ytp-time-duration')?.textContent.trim() || 
                              'N/A',
                    views: document.querySelector('yt-formatted-string[aria-label*="views"]')?.textContent.trim() || 
                           'N/A',
                    uploadDate: document.querySelector('div#info-strings yt-formatted-string')?.textContent.trim() || 
                                'N/A'
                };
                
                // Get description
                let descElem = document.querySelector('yt-formatted-string#content');
                data.description = descElem?.textContent.trim() || 'N/A';
                
                return { success: true, data: data };
            } catch(e) {
                return { success: false, error: e.message };
            }
        })()
"@
        
        $result = Invoke-BrowserScript -ScriptCode $metadataScript -Timeout 10
        
        if ($result -and $result.success) {
            Write-DevConsole "📹 Got metadata: $($result.data.title)" "SUCCESS"
            return $result.data
        }
        
        return $null
    }
    catch {
        Write-DevConsole "Metadata extraction error: $_" "ERROR"
        return $null
    }
}

# ============================================================================
# GENERIC DOM MANIPULATION
# ============================================================================

<#
.SYNOPSIS
    Clicks an element by CSS selector
    
.PARAMETER Selector
    CSS selector (e.g., "#button-id", ".class-name", "button:first-of-type")
#>
function Invoke-BrowserClick {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Selector
    )
    
    try {
        $script = "document.querySelector('$Selector')?.click();"
        Invoke-BrowserScript -ScriptCode $script -CaptureReturn $false
        
        Write-DevConsole "🖱️ Clicked: $Selector" "INFO"
        return $true
    }
    catch {
        Write-DevConsole "Click failed: $_" "ERROR"
        return $false
    }
}

<#
.SYNOPSIS
    Fills a form field with text
    
.PARAMETER Selector
    CSS selector for the input element
    
.PARAMETER Text
    Text to enter in the field
#>
function Invoke-BrowserFormFill {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Selector,
        
        [Parameter(Mandatory=$true)]
        [string]$Text
    )
    
    try {
        $escapedText = $Text -replace '"', '\"'
        $script = @"
        let elem = document.querySelector('$Selector');
        if (elem) {
            elem.value = '$escapedText';
            elem.dispatchEvent(new Event('change', { bubbles: true }));
            elem.dispatchEvent(new Event('input', { bubbles: true }));
            true;
        } else {
            false;
        }
"@
        
        $result = Invoke-BrowserScript -ScriptCode $script
        
        if ($result) {
            Write-DevConsole "✏️ Filled: $Selector" "INFO"
            return $true
        } else {
            Write-DevConsole "⚠️ Element not found: $Selector" "WARNING"
            return $false
        }
    }
    catch {
        Write-DevConsole "Form fill error: $_" "ERROR"
        return $false
    }
}

<#
.SYNOPSIS
    Extracts all links from the current page
    
.PARAMETER Selector
    CSS selector filter (default: all 'a' tags)
    
.PARAMETER MaxResults
    Maximum number of links to return
#>
function Get-BrowserLinks {
    param(
        [string]$Selector = "a",
        [int]$MaxResults = 50
    )
    
    try {
        $script = @"
        Array.from(document.querySelectorAll('$Selector'))
            .slice(0, $MaxResults)
            .map(el => ({
                href: el.href,
                text: el.textContent.trim(),
                title: el.title
            }))
            .filter(l => l.href && l.href.length > 0)
"@
        
        $result = Invoke-BrowserScript -ScriptCode $script
        
        if ($result) {
            Write-DevConsole "🔗 Extracted $($result.Count) links" "INFO"
            return $result
        }
        
        return @()
    }
    catch {
        Write-DevConsole "Link extraction error: $_" "ERROR"
        return @()
    }
}

<#
.SYNOPSIS
    Gets text content from an element
#>
function Get-BrowserElementText {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Selector
    )
    
    try {
        $script = "document.querySelector('$Selector')?.textContent.trim() || '';"
        $result = Invoke-BrowserScript -ScriptCode $script
        return $result
    }
    catch {
        return $null
    }
}

<#
.SYNOPSIS
    Takes a screenshot of the browser window
    
.PARAMETER OutputPath
    Path to save screenshot (PNG format)
#>
function Get-BrowserScreenshot {
    param([string]$OutputPath = "$env:TEMP\browser_screenshot.png")
    
    try {
        # Use WebView2 native screenshot capability
        $task = $script:webBrowser.CoreWebView2.CapturePreviewAsync(
            [Microsoft.Web.WebView2.Core.CoreWebView2CapturePreviewImageFormat]::Png,
            [System.IO.File]::Create($OutputPath)
        )
        
        $task.Wait()
        
        if (Test-Path $OutputPath) {
            Write-DevConsole "📸 Screenshot saved: $OutputPath" "SUCCESS"
            return $OutputPath
        }
        
        return $null
    }
    catch {
        Write-DevConsole "Screenshot error: $_" "ERROR"
        return $null
    }
}

# ============================================================================
# AGENT COMMAND INTEGRATION
# ============================================================================

<#
.SYNOPSIS
    Processes agentic browser control commands
    
.DESCRIPTION
    Parses and executes commands like:
    /search youtube "query"
    /navigate "https://example.com"
    /click "#button-id"
    /fill "#input" "text value"
#>
function Process-AgentBrowserCommand {
    param(
        [Parameter(Mandatory=$true)]
        [string]$CommandText
    )
    
    try {
        # Parse command format: /command arg1 arg2 arg3
        $parts = $CommandText.Trim().Split(' ', 3)
        $command = $parts[0] -replace '^/', ''
        $arg1 = if ($parts.Count -gt 1) { $parts[1] } else { "" }
        $arg2 = if ($parts.Count -gt 2) { $parts[2] } else { "" }
        
        $result = $null
        
        switch -Wildcard ($command.ToLower()) {
            # YouTube search
            "search" {
                if ($arg1 -like "youtube*" -or $arg1 -like "yt*") {
                    $query = $arg2
                    $results = Search-YouTubeFromBrowser -Query $query -MaxResults 10
                    if ($results.Count -gt 0) {
                        Write-DevConsole "🎬 Agent > Found $($results.Count) videos" "SUCCESS"
                        return @{ Command = "search"; Results = $results; Status = "success" }
                    }
                }
            }
            
            # Navigate browser
            { $_ -eq "navigate" -or $_ -eq "nav" } {
                $url = $arg1
                if (-not $url.StartsWith("http")) {
                    $url = "https://" + $url
                }
                Open-Browser $url
                Write-DevConsole "🌐 Agent > Navigating to: $url" "SUCCESS"
                return @{ Command = "navigate"; Status = "success" }
            }
            
            # Click element
            "click" {
                $selector = $arg1
                Invoke-BrowserClick -Selector $selector
                return @{ Command = "click"; Selector = $selector; Status = "success" }
            }
            
            # Fill form field
            "fill" {
                $selector = $arg1
                # Reconstruct text value (may have spaces)
                $text = $arg2
                Invoke-BrowserFormFill -Selector $selector -Text $text
                return @{ Command = "fill"; Selector = $selector; Status = "success" }
            }
            
            # Get links
            "links" {
                $links = Get-BrowserLinks -MaxResults 20
                Write-DevConsole "🔗 Agent > Found $($links.Count) links" "INFO"
                return @{ Command = "links"; Links = $links; Status = "success" }
            }
            
            # Screenshot
            "screenshot" {
                $screenshotPath = Get-BrowserScreenshot
                return @{ Command = "screenshot"; Path = $screenshotPath; Status = "success" }
            }
            
            default {
                Write-DevConsole "❓ Unknown browser command: $command" "WARNING"
                return @{ Status = "error"; Message = "Unknown command: $command" }
            }
        }
        
        return $result
        
    }
    catch {
        Write-DevConsole "Browser command error: $_" "ERROR"
        return @{ Status = "error"; Message = $_.Exception.Message }
    }
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
# Export functions for use in RawrXD
# Export-ModuleMember -Function @(
#     'Invoke-BrowserScript',
#     'Search-YouTubeFromBrowser',
#     'Get-YouTubeVideoMetadata',
#     'Invoke-BrowserClick',
#     'Invoke-BrowserFormFill',
#     'Get-BrowserLinks',
#     'Get-BrowserElementText',
#     'Get-BrowserScreenshot',
#     'Process-AgentBrowserCommand'
# )
