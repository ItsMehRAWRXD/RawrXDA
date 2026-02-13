#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Browser Integration Helper - Fetch web content for chatbot

.DESCRIPTION
    Provides browser capabilities to the chatbot:
    - Search the web for information
    - Fetch webpage content
    - Extract relevant information
    - Summarize web pages
    - Open URLs
    
.PARAMETER Operation
    search, fetch, open, summarize

.PARAMETER Query
    Search query or URL

.EXAMPLE
    .\browser_helper.ps1 -Operation search -Query "PowerShell swarm deployment"
    
.EXAMPLE
    .\browser_helper.ps1 -Operation fetch -Query "https://example.com/docs"
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('search', 'fetch', 'open', 'summarize')]
    [string]$Operation,
    
    [Parameter(Mandatory=$true)]
    [string]$Query,
    
    [Parameter(Mandatory=$false)]
    [int]$MaxResults = 5
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# BROWSER HELPER
# ═══════════════════════════════════════════════════════════════════════════════

class BrowserHelper {
    [string]$UserAgent
    
    BrowserHelper() {
        $this.UserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) RawrXD-IDE-Assistant/1.0"
    }
    
    [hashtable] SearchWeb([string]$query, [int]$maxResults) {
        Write-Host "`n🔍 Searching the web for: $query" -ForegroundColor Cyan
        
        # Use DuckDuckGo HTML search (no API key needed)
        $encodedQuery = [System.Web.HttpUtility]::UrlEncode($query)
        $searchUrl = "https://html.duckduckgo.com/html/?q=$encodedQuery"
        
        try {
            $response = Invoke-WebRequest -Uri $searchUrl -UserAgent $this.UserAgent -TimeoutSec 10
            $content = $response.Content
            
            # Parse results (basic HTML parsing)
            $results = @()
            $pattern = '<a[^>]+class="result__a"[^>]+href="([^"]+)"[^>]*>([^<]+)</a>'
            $matches = [regex]::Matches($content, $pattern)
            
            $count = 0
            foreach ($match in $matches) {
                if ($count -ge $maxResults) { break }
                
                $url = $match.Groups[1].Value
                $title = $match.Groups[2].Value
                
                # Decode HTML entities
                $title = [System.Web.HttpUtility]::HtmlDecode($title)
                $url = [System.Web.HttpUtility]::UrlDecode($url)
                
                if ($url -notmatch '^http') { continue }
                
                $results += @{
                    Title = $title
                    URL = $url
                    Position = $count + 1
                }
                
                $count++
            }
            
            return @{
                Success = $true
                Query = $query
                Results = $results
                Count = $results.Count
            }
        }
        catch {
            return @{
                Success = $false
                Error = $_.Exception.Message
                Query = $query
                Results = @()
            }
        }
    }
    
    [hashtable] FetchPage([string]$url) {
        Write-Host "`n📄 Fetching: $url" -ForegroundColor Cyan
        
        try {
            $response = Invoke-WebRequest -Uri $url -UserAgent $this.UserAgent -TimeoutSec 15
            $content = $response.Content
            
            # Extract text content (remove HTML tags)
            $text = $content -replace '<script[^>]*>[\s\S]*?</script>', ''
            $text = $text -replace '<style[^>]*>[\s\S]*?</style>', ''
            $text = $text -replace '<[^>]+>', ' '
            $text = [System.Web.HttpUtility]::HtmlDecode($text)
            $text = $text -replace '\s+', ' '
            $text = $text.Trim()
            
            # Extract title
            $titleMatch = [regex]::Match($content, '<title[^>]*>([^<]+)</title>')
            $title = if ($titleMatch.Success) { $titleMatch.Groups[1].Value } else { "Untitled" }
            $title = [System.Web.HttpUtility]::HtmlDecode($title)
            
            return @{
                Success = $true
                URL = $url
                Title = $title
                Content = $text
                Length = $text.Length
                StatusCode = $response.StatusCode
            }
        }
        catch {
            return @{
                Success = $false
                URL = $url
                Error = $_.Exception.Message
            }
        }
    }
    
    [hashtable] SummarizePage([string]$url) {
        $pageData = $this.FetchPage($url)
        
        if (-not $pageData.Success) {
            return $pageData
        }
        
        # Extract key sentences (simple extractive summarization)
        $text = $pageData.Content
        $sentences = $text -split '[.!?]\s+'
        
        # Score sentences by keyword density and position
        $keywords = $pageData.Title -split '\s+' | Where-Object { $_.Length -gt 3 }
        $scored = @()
        
        for ($i = 0; $i -lt [Math]::Min($sentences.Count, 50); $i++) {
            $sentence = $sentences[$i].Trim()
            if ($sentence.Length -lt 30) { continue }
            
            $score = 0
            
            # Title keywords
            foreach ($keyword in $keywords) {
                if ($sentence -match [regex]::Escape($keyword)) { $score += 3 }
            }
            
            # Position bonus (earlier = better)
            if ($i -lt 10) { $score += 2 }
            elseif ($i -lt 20) { $score += 1 }
            
            # Length bonus (not too short, not too long)
            if ($sentence.Length -gt 50 -and $sentence.Length -lt 200) { $score += 1 }
            
            $scored += @{
                Sentence = $sentence
                Score = $score
                Position = $i
            }
        }
        
        # Get top sentences
        $topSentences = $scored | Sort-Object -Property Score -Descending | Select-Object -First 5
        $summary = ($topSentences | Sort-Object -Property Position | ForEach-Object { $_.Sentence }) -join '. '
        
        return @{
            Success = $true
            URL = $url
            Title = $pageData.Title
            Summary = $summary
            FullLength = $pageData.Length
            SummaryLength = $summary.Length
        }
    }
    
    [void] OpenBrowser([string]$url) {
        Write-Host "`n🌐 Opening browser: $url" -ForegroundColor Cyan
        Start-Process $url
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

# Load System.Web for HTML encoding
Add-Type -AssemblyName System.Web

$browser = [BrowserHelper]::new()

switch ($Operation) {
    "search" {
        $result = $browser.SearchWeb($Query, $MaxResults)
        
        if ($result.Success) {
            Write-Host "`n✅ Found $($result.Count) results:`n" -ForegroundColor Green
            
            foreach ($item in $result.Results) {
                Write-Host "  $($item.Position). " -NoNewline -ForegroundColor Yellow
                Write-Host "$($item.Title)" -ForegroundColor Cyan
                Write-Host "     $($item.URL)`n" -ForegroundColor Gray
            }
            
            # Return as JSON for programmatic use
            $result | ConvertTo-Json -Depth 5
        }
        else {
            Write-Host "`n❌ Search failed: $($result.Error)" -ForegroundColor Red
        }
    }
    
    "fetch" {
        $result = $browser.FetchPage($Query)
        
        if ($result.Success) {
            Write-Host "`n✅ Fetched successfully!" -ForegroundColor Green
            Write-Host "  Title: $($result.Title)" -ForegroundColor Cyan
            Write-Host "  Length: $($result.Length) characters" -ForegroundColor Gray
            Write-Host "  Status: $($result.StatusCode)`n" -ForegroundColor Gray
            
            # Show first 500 characters
            $preview = $result.Content.Substring(0, [Math]::Min(500, $result.Content.Length))
            Write-Host "  Preview:" -ForegroundColor Yellow
            Write-Host "  $preview...`n" -ForegroundColor White
            
            # Return full content as JSON
            $result | ConvertTo-Json -Depth 5
        }
        else {
            Write-Host "`n❌ Fetch failed: $($result.Error)" -ForegroundColor Red
        }
    }
    
    "summarize" {
        $result = $browser.SummarizePage($Query)
        
        if ($result.Success) {
            Write-Host "`n✅ Summary generated!" -ForegroundColor Green
            Write-Host "  Title: $($result.Title)" -ForegroundColor Cyan
            Write-Host "  Original: $($result.FullLength) chars → Summary: $($result.SummaryLength) chars`n" -ForegroundColor Gray
            
            Write-Host "  Summary:" -ForegroundColor Yellow
            Write-Host "  $($result.Summary)`n" -ForegroundColor White
            
            # Return as JSON
            $result | ConvertTo-Json -Depth 5
        }
        else {
            Write-Host "`n❌ Summarization failed: $($result.Error)" -ForegroundColor Red
        }
    }
    
    "open" {
        $browser.OpenBrowser($Query)
    }
}
