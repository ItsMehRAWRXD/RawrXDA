#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Context Memory Manager - 1MB Conversation Memory with Learning

.DESCRIPTION
    Maintains up to 1MB of conversation context with:
    - Long-term memory of conversations
    - Pattern learning from user interactions
    - Context-aware command suggestions
    - Adaptive response tuning
    - Memory compression and summarization

.PARAMETER Operation
    add, search, tune, compress, export, stats

.PARAMETER Context
    Context to add or search

.EXAMPLE
    .\context_memory.ps1 -Operation add -Context "User prefers 7B models"
    
.EXAMPLE
    .\context_memory.ps1 -Operation search -Context "model preferences"
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('add', 'search', 'tune', 'compress', 'export', 'stats', 'clear')]
    [string]$Operation,
    
    [Parameter(Mandatory=$false)]
    [string]$Context = "",
    
    [Parameter(Mandatory=$false)]
    [string]$MemoryPath = "D:\lazy init ide\data\context_memory.json"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# CONTEXT MEMORY ENGINE - 1MB CAPACITY
# ═══════════════════════════════════════════════════════════════════════════════

class ContextMemory {
    [System.Collections.ArrayList]$Conversations
    [hashtable]$Patterns
    [hashtable]$UserPreferences
    [hashtable]$CommandHistory
    [int]$MaxSizeBytes
    [int]$CurrentSizeBytes
    [string]$MemoryPath
    [hashtable]$Statistics
    
    ContextMemory([string]$memoryPath) {
        $this.MemoryPath = $memoryPath
        $this.MaxSizeBytes = 1MB  # 1 megabyte limit
        $this.CurrentSizeBytes = 0
        $this.Conversations = [System.Collections.ArrayList]::new()
        $this.Patterns = @{}
        $this.UserPreferences = @{}
        $this.CommandHistory = @{}
        $this.Statistics = @{
            TotalInteractions = 0
            PatternMatches = 0
            MemoryCompressions = 0
            LearningScore = 0
        }
        
        $this.LoadMemory()
    }
    
    [void] LoadMemory() {
        if (Test-Path $this.MemoryPath) {
            try {
                $json = Get-Content $this.MemoryPath -Raw | ConvertFrom-Json -AsHashtable
                
                $this.Conversations = [System.Collections.ArrayList]::new($json.Conversations)
                $this.Patterns = $json.Patterns
                $this.UserPreferences = $json.UserPreferences
                $this.CommandHistory = $json.CommandHistory
                $this.Statistics = $json.Statistics
                
                $this.CalculateSize()
                
                Write-Host "  ✓ Loaded context memory: $([Math]::Round($this.CurrentSizeBytes / 1KB, 2)) KB" -ForegroundColor Green
            }
            catch {
                Write-Host "  ⚠ Failed to load memory: $_" -ForegroundColor Yellow
                $this.InitializeEmpty()
            }
        }
        else {
            $this.InitializeEmpty()
        }
    }
    
    [void] InitializeEmpty() {
        Write-Host "  ℹ Creating new context memory" -ForegroundColor Cyan
    }
    
    [void] CalculateSize() {
        $json = @{
            Conversations = $this.Conversations
            Patterns = $this.Patterns
            UserPreferences = $this.UserPreferences
            CommandHistory = $this.CommandHistory
            Statistics = $this.Statistics
        } | ConvertTo-Json -Depth 10
        
        $this.CurrentSizeBytes = [System.Text.Encoding]::UTF8.GetByteCount($json)
    }
    
    [void] AddContext([string]$context, [string]$type = "conversation") {
        $entry = @{
            Timestamp = Get-Date -Format "o"
            Type = $type
            Content = $context
            Size = [System.Text.Encoding]::UTF8.GetByteCount($context)
        }
        
        # Check if adding this would exceed limit
        if (($this.CurrentSizeBytes + $entry.Size) -gt $this.MaxSizeBytes) {
            Write-Host "  ⚠ Memory limit reached, compressing..." -ForegroundColor Yellow
            $this.CompressMemory()
        }
        
        $this.Conversations.Add($entry) | Out-Null
        $this.Statistics.TotalInteractions++
        
        # Learn patterns from context
        $this.LearnFromContext($context, $type)
        
        $this.CalculateSize()
        $this.Save()
    }
    
    [void] LearnFromContext([string]$context, [string]$type) {
        # Extract patterns from user interactions
        
        # Model size preferences
        if ($context -match '(\d+)B\s+model') {
            $size = $matches[1] + "B"
            if (-not $this.UserPreferences.ContainsKey("PreferredModelSize")) {
                $this.UserPreferences["PreferredModelSize"] = @{}
            }
            if (-not $this.UserPreferences.PreferredModelSize.ContainsKey($size)) {
                $this.UserPreferences.PreferredModelSize[$size] = 0
            }
            $this.UserPreferences.PreferredModelSize[$size]++
        }
        
        # Swarm size preferences
        if ($context -match 'send\s+(\d+)\s+agents?') {
            $size = [int]$matches[1]
            if (-not $this.UserPreferences.ContainsKey("PreferredSwarmSize")) {
                $this.UserPreferences["PreferredSwarmSize"] = @()
            }
            $this.UserPreferences.PreferredSwarmSize += $size
        }
        
        # Directory patterns
        if ($context -match '(D:\\[^\s"]+)') {
            $dir = $matches[1]
            if (-not $this.UserPreferences.ContainsKey("FrequentDirectories")) {
                $this.UserPreferences["FrequentDirectories"] = @{}
            }
            if (-not $this.UserPreferences.FrequentDirectories.ContainsKey($dir)) {
                $this.UserPreferences.FrequentDirectories[$dir] = 0
            }
            $this.UserPreferences.FrequentDirectories[$dir]++
        }
        
        # Command patterns
        if ($context -match '^\.(\\[\w_-]+\.ps1)') {
            $script = $matches[1]
            if (-not $this.CommandHistory.ContainsKey($script)) {
                $this.CommandHistory[$script] = @{
                    Count = 0
                    LastUsed = Get-Date -Format "o"
                    Parameters = @()
                }
            }
            $this.CommandHistory[$script].Count++
            $this.CommandHistory[$script].LastUsed = Get-Date -Format "o"
        }
        
        # Topic patterns
        $keywords = $context.ToLower() -split '\s+' | Where-Object { $_.Length -gt 4 }
        foreach ($keyword in $keywords) {
            if (-not $this.Patterns.ContainsKey($keyword)) {
                $this.Patterns[$keyword] = 0
            }
            $this.Patterns[$keyword]++
        }
        
        $this.Statistics.PatternMatches++
        $this.Statistics.LearningScore = $this.Patterns.Count + $this.UserPreferences.Count
    }
    
    [array] SearchContext([string]$query) {
        $results = @()
        $queryLower = $query.ToLower()
        $keywords = $queryLower -split '\s+' | Where-Object { $_.Length -gt 2 }
        
        foreach ($entry in $this.Conversations) {
            $score = 0
            $contentLower = $entry.Content.ToLower()
            
            foreach ($keyword in $keywords) {
                if ($contentLower -match $keyword) {
                    $score += 10
                }
            }
            
            if ($score -gt 0) {
                $results += @{
                    Content = $entry.Content
                    Type = $entry.Type
                    Timestamp = $entry.Timestamp
                    Score = $score
                }
            }
        }
        
        return $results | Sort-Object -Property Score -Descending
    }
    
    [void] CompressMemory() {
        # Keep most recent and most important conversations
        $this.Statistics.MemoryCompressions++
        
        # Sort by timestamp and importance
        $sorted = $this.Conversations | Sort-Object -Property @{
            Expression = { [DateTime]::Parse($_.Timestamp) }
            Descending = $true
        }
        
        # Keep newest 70%, compress oldest 30%
        $keepCount = [Math]::Floor($sorted.Count * 0.7)
        $toCompress = $sorted | Select-Object -Skip $keepCount
        
        # Create summary of compressed entries
        if ($toCompress.Count -gt 0) {
            $summary = @{
                Timestamp = Get-Date -Format "o"
                Type = "compressed_summary"
                Content = "Compressed $($toCompress.Count) older entries. Topics: $($this.GetTopTopics($toCompress))"
                Size = 200
            }
            
            # Keep only newer entries plus summary
            $this.Conversations = [System.Collections.ArrayList]::new($sorted | Select-Object -First $keepCount)
            $this.Conversations.Insert(0, $summary) | Out-Null
        }
        
        $this.CalculateSize()
        Write-Host "  ✓ Memory compressed to $([Math]::Round($this.CurrentSizeBytes / 1KB, 2)) KB" -ForegroundColor Green
    }
    
    [string] GetTopTopics([array]$entries) {
        $topics = @{}
        
        foreach ($entry in $entries) {
            $words = $entry.Content -split '\s+' | Where-Object { $_.Length -gt 4 }
            foreach ($word in $words) {
                if (-not $topics.ContainsKey($word)) {
                    $topics[$word] = 0
                }
                $topics[$word]++
            }
        }
        
        $top = $topics.GetEnumerator() | Sort-Object -Property Value -Descending | Select-Object -First 5 -ExpandProperty Key
        return $top -join ', '
    }
    
    [hashtable] GetSuggestions() {
        $suggestions = @{
            ModelSizes = @()
            SwarmSizes = @()
            Directories = @()
            Commands = @()
        }
        
        # Most used model sizes
        if ($this.UserPreferences.ContainsKey("PreferredModelSize")) {
            $suggestions.ModelSizes = $this.UserPreferences.PreferredModelSize.GetEnumerator() | 
                Sort-Object -Property Value -Descending | 
                Select-Object -First 3 -ExpandProperty Key
        }
        
        # Average swarm size
        if ($this.UserPreferences.ContainsKey("PreferredSwarmSize") -and $this.UserPreferences.PreferredSwarmSize.Count -gt 0) {
            $avg = ($this.UserPreferences.PreferredSwarmSize | Measure-Object -Average).Average
            $suggestions.SwarmSizes = @([Math]::Round($avg))
        }
        
        # Frequent directories
        if ($this.UserPreferences.ContainsKey("FrequentDirectories")) {
            $suggestions.Directories = $this.UserPreferences.FrequentDirectories.GetEnumerator() | 
                Sort-Object -Property Value -Descending | 
                Select-Object -First 5 -ExpandProperty Key
        }
        
        # Recent commands
        $suggestions.Commands = $this.CommandHistory.GetEnumerator() | 
            Sort-Object -Property { $_.Value.Count } -Descending | 
            Select-Object -First 5 -ExpandProperty Key
        
        return $suggestions
    }
    
    [void] Save() {
        $memoryDir = Split-Path $this.MemoryPath -Parent
        if (-not (Test-Path $memoryDir)) {
            New-Item -ItemType Directory -Path $memoryDir -Force | Out-Null
        }
        
        $data = @{
            Conversations = $this.Conversations
            Patterns = $this.Patterns
            UserPreferences = $this.UserPreferences
            CommandHistory = $this.CommandHistory
            Statistics = $this.Statistics
            Metadata = @{
                LastSaved = Get-Date -Format "o"
                SizeBytes = $this.CurrentSizeBytes
                SizeKB = [Math]::Round($this.CurrentSizeBytes / 1KB, 2)
                SizeMB = [Math]::Round($this.CurrentSizeBytes / 1MB, 4)
            }
        }
        
        $data | ConvertTo-Json -Depth 10 | Set-Content $this.MemoryPath -Encoding UTF8
    }
    
    [void] DisplayStats() {
        Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
        Write-Host "║           CONTEXT MEMORY STATISTICS                           ║" -ForegroundColor Cyan
        Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
        
        Write-Host "  📊 Memory Usage:" -ForegroundColor Yellow
        Write-Host "     Current: $([Math]::Round($this.CurrentSizeBytes / 1KB, 2)) KB / 1024 KB" -ForegroundColor Gray
        Write-Host "     Usage: $([Math]::Round(($this.CurrentSizeBytes / $this.MaxSizeBytes) * 100, 1))%" -ForegroundColor Gray
        
        Write-Host "`n  💬 Conversations:" -ForegroundColor Yellow
        Write-Host "     Total Entries: $($this.Conversations.Count)" -ForegroundColor Gray
        Write-Host "     Total Interactions: $($this.Statistics.TotalInteractions)" -ForegroundColor Gray
        
        Write-Host "`n  🧠 Learning:" -ForegroundColor Yellow
        Write-Host "     Patterns Learned: $($this.Patterns.Count)" -ForegroundColor Gray
        Write-Host "     Preferences Tracked: $($this.UserPreferences.Count)" -ForegroundColor Gray
        Write-Host "     Commands Used: $($this.CommandHistory.Count)" -ForegroundColor Gray
        Write-Host "     Learning Score: $($this.Statistics.LearningScore)" -ForegroundColor Gray
        Write-Host "     Compressions: $($this.Statistics.MemoryCompressions)" -ForegroundColor Gray
        
        if ($this.UserPreferences.Count -gt 0) {
            Write-Host "`n  ⭐ Your Preferences:" -ForegroundColor Magenta
            
            if ($this.UserPreferences.ContainsKey("PreferredModelSize")) {
                $topModel = $this.UserPreferences.PreferredModelSize.GetEnumerator() | 
                    Sort-Object -Property Value -Descending | 
                    Select-Object -First 1
                Write-Host "     Favorite Model: $($topModel.Key) (used $($topModel.Value) times)" -ForegroundColor Gray
            }
            
            if ($this.UserPreferences.ContainsKey("PreferredSwarmSize") -and $this.UserPreferences.PreferredSwarmSize.Count -gt 0) {
                $avg = ($this.UserPreferences.PreferredSwarmSize | Measure-Object -Average).Average
                Write-Host "     Avg Swarm Size: $([Math]::Round($avg))" -ForegroundColor Gray
            }
            
            if ($this.UserPreferences.ContainsKey("FrequentDirectories")) {
                $topDir = $this.UserPreferences.FrequentDirectories.GetEnumerator() | 
                    Sort-Object -Property Value -Descending | 
                    Select-Object -First 1
                Write-Host "     Most Used Dir: $($topDir.Key) ($($topDir.Value) times)" -ForegroundColor Gray
            }
        }
        
        Write-Host ""
    }
    
    [void] ExportReadable([string]$outputPath) {
        $output = @"
╔═══════════════════════════════════════════════════════════════╗
║           CONTEXT MEMORY EXPORT                               ║
╚═══════════════════════════════════════════════════════════════╝

Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Memory Size: $([Math]::Round($this.CurrentSizeBytes / 1KB, 2)) KB

═══════════════════════════════════════════════════════════════
RECENT CONVERSATIONS
═══════════════════════════════════════════════════════════════

$($this.Conversations | Select-Object -Last 20 | ForEach-Object {
    "[$($_.Timestamp)] [$($_.Type)] $($_.Content)"
} | Out-String)

═══════════════════════════════════════════════════════════════
USER PREFERENCES
═══════════════════════════════════════════════════════════════

$($this.UserPreferences | ConvertTo-Json -Depth 5)

═══════════════════════════════════════════════════════════════
COMMAND HISTORY
═══════════════════════════════════════════════════════════════

$($this.CommandHistory.GetEnumerator() | Sort-Object -Property { $_.Value.Count } -Descending | ForEach-Object {
    "$($_.Key): $($_.Value.Count) uses (Last: $($_.Value.LastUsed))"
} | Out-String)

═══════════════════════════════════════════════════════════════
TOP PATTERNS
═══════════════════════════════════════════════════════════════

$($this.Patterns.GetEnumerator() | Sort-Object -Property Value -Descending | Select-Object -First 20 | ForEach-Object {
    "$($_.Key): $($_.Value)"
} | Out-String)
"@
        
        $output | Set-Content $outputPath -Encoding UTF8
        Write-Host "  ✓ Exported to: $outputPath" -ForegroundColor Green
    }
    
    [void] Clear() {
        $this.Conversations.Clear()
        $this.Patterns.Clear()
        $this.UserPreferences.Clear()
        $this.CommandHistory.Clear()
        $this.Statistics = @{
            TotalInteractions = 0
            PatternMatches = 0
            MemoryCompressions = 0
            LearningScore = 0
        }
        $this.CurrentSizeBytes = 0
        $this.Save()
        Write-Host "  ✓ Memory cleared" -ForegroundColor Green
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

$memory = [ContextMemory]::new($MemoryPath)

switch ($Operation) {
    "add" {
        if (-not $Context) {
            Write-Error "Context parameter required for add operation"
            exit 1
        }
        
        $memory.AddContext($Context)
        Write-Host "`n  ✅ Context added! Memory: $([Math]::Round($memory.CurrentSizeBytes / 1KB, 2)) KB / 1024 KB" -ForegroundColor Green
        
        # Show suggestions
        $suggestions = $memory.GetSuggestions()
        if ($suggestions.ModelSizes.Count -gt 0 -or $suggestions.Directories.Count -gt 0) {
            Write-Host "`n  💡 Learned Suggestions:" -ForegroundColor Magenta
            if ($suggestions.ModelSizes.Count -gt 0) {
                Write-Host "     Favorite models: $($suggestions.ModelSizes -join ', ')" -ForegroundColor Gray
            }
            if ($suggestions.Directories.Count -gt 0) {
                Write-Host "     Frequent dirs: $($suggestions.Directories[0])" -ForegroundColor Gray
            }
        }
    }
    
    "search" {
        if (-not $Context) {
            Write-Error "Context parameter required for search operation"
            exit 1
        }
        
        $results = $memory.SearchContext($Context)
        
        Write-Host "`n🔍 Search Results for: $Context`n" -ForegroundColor Cyan
        
        if ($results.Count -eq 0) {
            Write-Host "  No matches found" -ForegroundColor Yellow
        }
        else {
            foreach ($result in ($results | Select-Object -First 10)) {
                Write-Host "  [$($result.Score)] " -NoNewline -ForegroundColor Yellow
                Write-Host "$($result.Content)" -ForegroundColor White
                Write-Host "      $($result.Timestamp) | $($result.Type)" -ForegroundColor DarkGray
                Write-Host ""
            }
        }
    }
    
    "tune" {
        Write-Host "`n🎯 Context Tuning & Suggestions`n" -ForegroundColor Cyan
        
        $suggestions = $memory.GetSuggestions()
        
        Write-Host "  📊 Based on your usage patterns:`n" -ForegroundColor Yellow
        
        if ($suggestions.ModelSizes.Count -gt 0) {
            Write-Host "  🧠 Preferred Model Sizes:" -ForegroundColor Magenta
            foreach ($size in $suggestions.ModelSizes) {
                Write-Host "     • $size" -ForegroundColor Gray
            }
            Write-Host ""
        }
        
        if ($suggestions.SwarmSizes.Count -gt 0) {
            Write-Host "  🤖 Typical Swarm Size:" -ForegroundColor Magenta
            Write-Host "     • $($suggestions.SwarmSizes[0]) agents" -ForegroundColor Gray
            Write-Host ""
        }
        
        if ($suggestions.Directories.Count -gt 0) {
            Write-Host "  📁 Frequent Directories:" -ForegroundColor Magenta
            foreach ($dir in ($suggestions.Directories | Select-Object -First 3)) {
                Write-Host "     • $dir" -ForegroundColor Gray
            }
            Write-Host ""
        }
        
        if ($suggestions.Commands.Count -gt 0) {
            Write-Host "  ⚡ Most Used Commands:" -ForegroundColor Magenta
            foreach ($cmd in $suggestions.Commands) {
                $count = $memory.CommandHistory[$cmd].Count
                Write-Host "     • $cmd ($count uses)" -ForegroundColor Gray
            }
            Write-Host ""
        }
    }
    
    "compress" {
        Write-Host "`n🗜️ Compressing memory...`n" -ForegroundColor Cyan
        
        $beforeSize = $memory.CurrentSizeBytes
        $memory.CompressMemory()
        $memory.Save()
        $afterSize = $memory.CurrentSizeBytes
        
        $saved = $beforeSize - $afterSize
        $percent = [Math]::Round(($saved / $beforeSize) * 100, 1)
        
        Write-Host "  ✅ Compression complete!" -ForegroundColor Green
        Write-Host "     Before: $([Math]::Round($beforeSize / 1KB, 2)) KB" -ForegroundColor Gray
        Write-Host "     After: $([Math]::Round($afterSize / 1KB, 2)) KB" -ForegroundColor Gray
        Write-Host "     Saved: $([Math]::Round($saved / 1KB, 2)) KB ($percent%)" -ForegroundColor Gray
    }
    
    "export" {
        $exportPath = $MemoryPath -replace '\.json$', '_export.txt'
        $memory.ExportReadable($exportPath)
    }
    
    "stats" {
        $memory.DisplayStats()
    }
    
    "clear" {
        Write-Host "`n⚠️  This will clear all context memory. Are you sure? (y/n): " -NoNewline -ForegroundColor Yellow
        $confirm = Read-Host
        
        if ($confirm -eq 'y') {
            $memory.Clear()
        }
        else {
            Write-Host "  Cancelled" -ForegroundColor Gray
        }
    }
}

Write-Host ""
