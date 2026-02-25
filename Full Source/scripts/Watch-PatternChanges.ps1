# ============================================================================
# Watch-PatternChanges.ps1
# Real-time pattern detection using FileSystemWatcher
# Version: 2.0.0 - Production Ready with Caching, IDE Notification & Auto-Fix
# ============================================================================
#
# Usage:
#   .\Watch-PatternChanges.ps1 -WatchPath "D:\lazy init ide\src"
#   .\Watch-PatternChanges.ps1 -WatchPath "." -Extensions @("*.ps1", "*.cpp")
#   .\Watch-PatternChanges.ps1 -OutputLog "patterns.log" -Verbose
#   .\Watch-PatternChanges.ps1 -AutoFix -NotifyIDE
#
# ============================================================================

#Requires -Version 7.0

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$WatchPath = "D:\lazy init ide\src",
    
    [Parameter(Mandatory = $false)]
    [string[]]$Extensions = @("*.ps1", "*.psm1", "*.cpp", "*.h", "*.c", "*.asm", "*.cs", "*.py", "*.js", "*.ts", "*.java", "*.go", "*.rs"),
    
    [Parameter(Mandatory = $false)]
    [switch]$IncludeSubdirectories = $true,
    
    [Parameter(Mandatory = $false)]
    [string]$OutputLog,
    
    [Parameter(Mandatory = $false)]
    [int]$DebounceMs = 500,
    
    [Parameter(Mandatory = $false)]
    [switch]$ShowContent,
    
    [Parameter(Mandatory = $false)]
    [ValidateSet("All", "Critical", "High", "Medium", "Low")]
    [string]$MinPriority = "All",
    
    [Parameter(Mandatory = $false)]
    [switch]$AutoFix,
    
    [Parameter(Mandatory = $false)]
    [switch]$NotifyIDE,
    
    [Parameter(Mandatory = $false)]
    [string]$NotifyPipeName = "\\.\pipe\RawrXD_IDE_Notify",
    
    [Parameter(Mandatory = $false)]
    [switch]$EnableCache,
    
    [Parameter(Mandatory = $false)]
    [int]$CacheTTLSeconds = 30,
    
    [Parameter(Mandatory = $false)]
    [switch]$CreateRollback
)

$ErrorActionPreference = 'Continue'

# ============================================================================
# Load Pattern Engine
# ============================================================================

# Import the bridge signatures for direct P/Invoke calls
$bridgePath = Join-Path $PSScriptRoot "..\bin\RawrXD_PatternBridge_Signatures.ps1"
if (Test-Path $bridgePath) {
    Import-Module $bridgePath -Force
    Initialize-RawrXDBridge
} else {
    Write-Warning "RawrXD_PatternBridge_Signatures.ps1 not found at $bridgePath"
    # Fallback to loading signatures manually if needed, or exit
    throw "Fatal: Bridge signatures not found."
}

# ============================================================================
# Core Logic
# ============================================================================
# Wrapper to use Invoke-DirectClassify instead of raw P/Invoke
function Get-PatternClassification {
    param([string]$FilePath)
    
    if (-not (Test-Path $FilePath)) { return $null }
    
    try {
        $content = Get-Content -Path $FilePath -Raw -ErrorAction Stop
        if ([string]::IsNullOrWhiteSpace($content)) { return $null }
        
        # Use the direct bridge
        $result = Invoke-DirectClassify -Text $content
        
        if ($result.Type -gt 0) {
            return $result
        }
    }
    catch {
        Write-Verbose "Error reading/classifying $FilePath : $_"
    }
    return $null
}

# (Legacy PatternTypes map removed in favor of direct bridge metadata)

$PriorityFilter = switch ($MinPriority) {
    "Critical" { 3 }
    "High"     { 2 }
    "Medium"   { 1 }
    "Low"      { 0 }
    default    { -1 }
}

# ============================================================================
# Helper Functions
# ============================================================================

function Invoke-PatternClassification {
    param([string]$Text)
    
    $codeBytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    $codePtr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($codeBytes.Length)
    $ctxPtr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(1)
    
    try {
        [System.Runtime.InteropServices.Marshal]::Copy($codeBytes, 0, $codePtr, $codeBytes.Length)
        [double]$confidence = 0.0
        $type = [RawrXD_Watcher]::ClassifyPattern($codePtr, $codeBytes.Length, $ctxPtr, [ref]$confidence)
        
        $info = $PatternTypes[$type]
        if (-not $info) { $info = $PatternTypes[0] }
        
        return @{
            Type = $type
            TypeName = $info.Name
            Priority = $info.Priority
            Confidence = $confidence
            Color = $info.Color
        }
    }
    finally {
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($codePtr)
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ctxPtr)
    }
}

function Scan-FilePatterns {
    param([string]$FilePath)
    
    $results = @()
    
    try {
        $content = Get-Content -Path $FilePath -Raw -ErrorAction Stop
        if (-not $content) { return $results }
        
        $lines = $content -split "`n"
        
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $line = $lines[$i]
            
            # Quick regex pre-filter
            if ($line -imatch '\b(TODO|FIXME|XXX|HACK|BUG|NOTE|IDEA|REVIEW)\b') {
                $result = Invoke-PatternClassification -Text $line
                
                if ($result.Type -ne 0 -and $result.Priority -ge $PriorityFilter) {
                    $results += [PSCustomObject]@{
                        Line = $i + 1
                        Type = $result.TypeName
                        Priority = $result.Priority
                        Confidence = [math]::Round($result.Confidence, 2)
                        Color = $result.Color
                        Content = $line.Trim().Substring(0, [Math]::Min(80, $line.Trim().Length))
                    }
                }
            }
        }
    }
    catch {
        Write-Verbose "Failed to scan $FilePath : $_"
    }
    
    return $results
}

function Write-PatternAlert {
    param(
        [string]$FilePath,
        [PSCustomObject[]]$Patterns,
        [string]$EventType
    )
    
    $timestamp = Get-Date -Format "HH:mm:ss"
    $relPath = $FilePath -replace [regex]::Escape($WatchPath), ""
    if ($relPath.StartsWith("\")) { $relPath = $relPath.Substring(1) }
    
    # Group by priority using new Priority scheme (10=Critical, 8=High, 5-6=Medium, <5=Low)
    $critical = $Patterns | Where-Object { $_.Priority -ge 10 }
    $high     = $Patterns | Where-Object { $_.Priority -ge 7 -and $_.Priority -lt 10 }
    $medium   = $Patterns | Where-Object { $_.Priority -ge 5 -and $_.Priority -lt 7 }
    $low      = $Patterns | Where-Object { $_.Priority -lt 5 }
    
    # Header
    $icon = switch ($EventType) {
        "Changed" { "📝" }
        "Created" { "✨" }
        "Renamed" { "🔄" }
        default { "📋" }
    }
    
    Write-Host ""
    Write-Host "[$timestamp] $icon $relPath ($($Patterns.Count) patterns)" -ForegroundColor Cyan
    
    # Critical patterns
    foreach ($p in $critical) {
        Write-Host "  🚨 [$($p.TypeName)] Line $($p.Line): Prio $($p.Priority)" -ForegroundColor Red
    }
    
    # High priority
    foreach ($p in $high) {
        Write-Host "  ⚠️  [$($p.TypeName)] Line $($p.Line): Prio $($p.Priority)" -ForegroundColor Yellow
    }
    
    # Medium priority
    foreach ($p in $medium) {
        Write-Host "  📌 [$($p.TypeName)] Line $($p.Line): Prio $($p.Priority)" -ForegroundColor DarkYellow
    }
    
    # Low priority
    foreach ($p in $low) {
        Write-Host "  📋 [$($p.TypeName)] Line $($p.Line): Prio $($p.Priority)" -ForegroundColor Gray
    }
    
    # Log to file if specified
    if ($OutputLog) {
        $logEntry = "[$timestamp] $EventType $FilePath"
        foreach ($p in $Patterns) {
            $logEntry += "`n  [$($p.TypeName)] Line $($p.Line): Prio $($p.Priority)"
        }
        Add-Content -Path $OutputLog -Value $logEntry -ErrorAction SilentlyContinue
    }
}

# ============================================================================
# Debounce Logic
# ============================================================================

$script:LastProcessed = @{}
$script:ProcessQueue = [System.Collections.Concurrent.ConcurrentQueue[string]]::new()

# ============================================================================
# Cache System
# ============================================================================

$script:PatternCache = @{}
$script:CacheHits = 0
$script:CacheMisses = 0

function Get-CachedPatterns {
    param([string]$FilePath, [DateTime]$FileTime)
    
    if (-not $EnableCache) { return $null }
    
    $entry = $script:PatternCache[$FilePath]
    if ($entry) {
        $age = ([DateTime]::Now - $entry.Timestamp).TotalSeconds
        if ($age -lt $CacheTTLSeconds -and $entry.FileTime -eq $FileTime) {
            $script:CacheHits++
            return $entry.Patterns
        }
    }
    
    $script:CacheMisses++
    return $null
}

function Set-CachedPatterns {
    param([string]$FilePath, [DateTime]$FileTime, [object[]]$Patterns)
    
    if (-not $EnableCache) { return }
    
    $script:PatternCache[$FilePath] = @{
        FileTime = $FileTime
        Timestamp = [DateTime]::Now
        Patterns = $Patterns
    }
}

function Should-ProcessFile {
    param([string]$FilePath)
    
    $now = [DateTime]::Now
    $lastTime = $script:LastProcessed[$FilePath]
    
    if ($lastTime -and ($now - $lastTime).TotalMilliseconds -lt $DebounceMs) {
        return $false
    }
    
    $script:LastProcessed[$FilePath] = $now
    return $true
}

# ============================================================================
# IDE Notification System
# ============================================================================

$script:IDEPipeHandle = $null

function Connect-IDEPipe {
    if (-not $NotifyIDE) { return $false }
    
    try {
        # Check if pipe exists
        if (-not (Test-Path $NotifyPipeName)) {
            Write-Verbose "IDE notification pipe not available"
            return $false
        }
        
        # Create pipe client
        $script:IDEPipeHandle = [System.IO.Pipes.NamedPipeClientStream]::new(
            ".",
            ($NotifyPipeName -replace '^\\\\.\\pipe\\', ''),
            [System.IO.Pipes.PipeDirection]::Out
        )
        
        $script:IDEPipeHandle.Connect(1000)
        Write-Host "[IDE] Connected to notification pipe" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Verbose "Failed to connect to IDE pipe: $_"
        $script:IDEPipeHandle = $null
        return $false
    }
}

function Send-IDENotification {
    param(
        [string]$FilePath,
        [object[]]$Patterns
    )
    
    if (-not $NotifyIDE -or -not $script:IDEPipeHandle) { 
        # Try to reconnect
        if ($NotifyIDE -and -not (Connect-IDEPipe)) { return }
        if (-not $script:IDEPipeHandle) { return }
    }
    
    try {
        $notification = @{
            Type = "PatternAlert"
            Timestamp = (Get-Date -Format "o")
            File = $FilePath
            Patterns = $Patterns | ForEach-Object {
                @{
                    Line = $_.Line
                    Type = $_.TypeName
                    Priority = $_.Priority
                }
            }
        } | ConvertTo-Json -Depth 5 -Compress
        
        # Send length-prefixed message (ensure consistent binary protocol)
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($notification)
        $lenBytes = [BitConverter]::GetBytes([int]$bytes.Length)
        
        if ($script:IDEPipeHandle -and $script:IDEPipeHandle.IsConnected) {
             # Use the correct Write method from NamedPipeClientStream
             $script:IDEPipeHandle.Write($lenBytes, 0, 4)
             $script:IDEPipeHandle.Write($bytes, 0, $bytes.Length)
             # WaitForPipeDrain if mostly write-only, but Flush is safer for async
             try { $script:IDEPipeHandle.Flush() } catch {}
        }
        
        Write-Verbose "Sent IDE notification: $($Patterns.Count) patterns"
    }
    catch {
        Write-Verbose "Failed to send IDE notification: $_"
        try { $script:IDEPipeHandle.Dispose() } catch {}
        $script:IDEPipeHandle = $null
    }
}

# ============================================================================
# Auto-Fix Engine
# ============================================================================

$script:AutoFixPatterns = @{
    # Pattern regex => replacement template
    '(?i)//\s*TODO:\s*remove\s+this\s*$' = { param($line) "" }
    '(?i)//\s*TODO:\s*uncomment\s*$' = { param($line) $line -replace '^(\s*)//\s*', '$1' }
    '(?i)//\s*FIXME:\s*add\s+null\s+check\s*$' = { param($line, $nextLine) 
        if ($nextLine -match '(\w+)\s*\(' ) {
            "if ($($Matches[1]) == null) throw new ArgumentNullException();`n$line"
        } else { $line }
    }
}

$script:AutoFixCount = 0
$script:AutoFixFiles = @{}

function Invoke-AutoFix {
    param(
        [string]$FilePath,
        [object[]]$Patterns
    )
    
    if (-not $AutoFix) { return }
    
    # Only auto-fix specific low-risk patterns
    $fixablePatterns = $Patterns | Where-Object { 
        $_.Type -in @('TODO', 'FIXME') -and 
        $_.Priority -le 1 -and
        $_.Content -imatch '(remove|uncomment|cleanup|delete)'
    }
    
    if ($fixablePatterns.Count -eq 0) { return }
    
    # Create rollback point if enabled
    if ($CreateRollback) {
        try {
            Import-Module "$PSScriptRoot\TODO-RollbackSystem.psm1" -ErrorAction Stop
            New-TODORollbackPoint -Files @($FilePath) -Description "Auto-fix by watcher" | Out-Null
        }
        catch {
            Write-Warning "Could not create rollback point: $_"
        }
    }
    
    Write-Host "[AutoFix] Processing $($fixablePatterns.Count) patterns in $FilePath" -ForegroundColor Magenta
    
    try {
        $content = Get-Content -Path $FilePath -Raw
        $lines = $content -split "`r?`n"
        $modified = $false
        
        foreach ($pattern in ($fixablePatterns | Sort-Object { $_.Line } -Descending)) {
            $lineIdx = $pattern.Line - 1
            if ($lineIdx -lt 0 -or $lineIdx -ge $lines.Count) { continue }
            
            $line = $lines[$lineIdx]
            
            # Check for "remove" TODO
            if ($pattern.Content -imatch '(TODO|FIXME):?\s*(remove|delete)') {
                Write-Host "  [Line $($pattern.Line)] Removing: $($line.Trim().Substring(0, [Math]::Min(50, $line.Trim().Length)))..." -ForegroundColor Yellow
                $lines = $lines[0..($lineIdx-1)] + $lines[($lineIdx+1)..($lines.Count-1)]
                $modified = $true
                $script:AutoFixCount++
            }
            # Check for "uncomment" TODO
            elseif ($pattern.Content -imatch '(TODO|FIXME):?\s*uncomment') {
                $newLine = $line -replace '^\s*//\s*', ''
                Write-Host "  [Line $($pattern.Line)] Uncommenting" -ForegroundColor Yellow
                $lines[$lineIdx] = $newLine
                $modified = $true
                $script:AutoFixCount++
            }
        }
        
        if ($modified) {
            $newContent = $lines -join "`n"
            Set-Content -Path $FilePath -Value $newContent -NoNewline
            Write-Host "  ✓ File updated" -ForegroundColor Green
            
            if (-not $script:AutoFixFiles.ContainsKey($FilePath)) {
                $script:AutoFixFiles[$FilePath] = 0
            }
            $script:AutoFixFiles[$FilePath]++
        }
    }
    catch {
        Write-Warning "AutoFix failed for $FilePath : $_"
    }
}

# ============================================================================
# Initialize Engine
# ============================================================================

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║       RawrXD Real-Time Pattern Watcher v2.0                    ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Initialization handled by bridge import at top of script
Write-Host "Pattern engine active via bridge." -ForegroundColor Green
$initResult = 0

# (Legacy engine info call removed as bridge handles state)
Write-Host "[Engine] Mode: Hybrid (AVX-512 + Scalar)" -ForegroundColor Magenta

# Resolve watch path
$WatchPath = (Resolve-Path $WatchPath -ErrorAction Stop).Path
Write-Host "[Watch]  Path: $WatchPath" -ForegroundColor Yellow
Write-Host "[Watch]  Extensions: $($Extensions -join ', ')" -ForegroundColor Gray
Write-Host "[Watch]  Subdirectories: $IncludeSubdirectories" -ForegroundColor Gray
Write-Host "[Watch]  Min Priority: $MinPriority" -ForegroundColor Gray

# Show new feature status
if ($EnableCache) {
    Write-Host "[Cache]  Enabled (TTL: ${CacheTTLSeconds}s)" -ForegroundColor Green
}
if ($AutoFix) {
    Write-Host "[AutoFix] Enabled" -ForegroundColor Magenta
    if ($CreateRollback) {
        Write-Host "[AutoFix] Rollback points enabled" -ForegroundColor Magenta
    }
}
if ($NotifyIDE) {
    Write-Host "[IDE]    Notification pipe: $NotifyPipeName" -ForegroundColor Cyan
    Connect-IDEPipe | Out-Null
}

if ($OutputLog) {
    Write-Host "[Log]    Output: $OutputLog" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Watching for changes... (Press Ctrl+C to stop)" -ForegroundColor Green
Write-Host ""

# ============================================================================
# Setup FileSystemWatcher
# ============================================================================

$watcher = New-Object System.IO.FileSystemWatcher
$watcher.Path = $WatchPath
$watcher.IncludeSubdirectories = $IncludeSubdirectories
$watcher.Filter = "*.*"
$watcher.NotifyFilter = [System.IO.NotifyFilters]::LastWrite -bor `
                        [System.IO.NotifyFilters]::FileName -bor `
                        [System.IO.NotifyFilters]::CreationTime

# Event handler
$onFileChange = {
    param($source, $e)
    
    $path = $e.FullPath
    $changeType = $e.ChangeType
    
    # Check extension
    $ext = [System.IO.Path]::GetExtension($path)
    $matchesExt = $false
    foreach ($pattern in $using:Extensions) {
        $searchExt = $pattern.TrimStart('*')
        if ($ext -eq $searchExt) {
            $matchesExt = $true
            break
        }
    }
    
    if (-not $matchesExt) { return }
    
    # Check file exists (may be deleted)
    if (-not (Test-Path $path)) { return }
    
    # Debounce
    $now = [DateTime]::Now
    $key = $path
    if ($script:LastProcessed.ContainsKey($key)) {
        $lastTime = $script:LastProcessed[$key]
        if (($now - $lastTime).TotalMilliseconds -lt $using:DebounceMs) {
            return
        }
    }
    $script:LastProcessed[$key] = $now
    
    # Check cache first
    $fileTime = (Get-Item $path).LastWriteTime
    $patterns = Get-CachedPatterns -FilePath $path -FileTime $fileTime
    
    if (-not $patterns) {
        # Scan file
        $patterns = Scan-FilePatterns -FilePath $path
        Set-CachedPatterns -FilePath $path -FileTime $fileTime -Patterns $patterns
    }
    
    if ($patterns.Count -gt 0) {
        Write-PatternAlert -FilePath $path -Patterns $patterns -EventType $changeType.ToString()
        
        # Send IDE notification
        Send-IDENotification -FilePath $path -Patterns $patterns
        
        # Run auto-fix if enabled
        Invoke-AutoFix -FilePath $path -Patterns $patterns
    }
}

# Register events
$changedJob = Register-ObjectEvent -InputObject $watcher -EventName Changed -Action $onFileChange
$createdJob = Register-ObjectEvent -InputObject $watcher -EventName Created -Action $onFileChange
$renamedJob = Register-ObjectEvent -InputObject $watcher -EventName Renamed -Action $onFileChange

# Enable watching
$watcher.EnableRaisingEvents = $true

# Statistics
$script:TotalEvents = 0
$script:StartTime = Get-Date

# ============================================================================
# Main Loop
# ============================================================================

try {
    while ($true) {
        Start-Sleep -Milliseconds 100
        
        # Process any queued files
        $queuedPath = $null
        while ($script:ProcessQueue.TryDequeue([ref]$queuedPath)) {
            if (Test-Path $queuedPath) {
                $patterns = Scan-FilePatterns -FilePath $queuedPath
                if ($patterns.Count -gt 0) {
                    Write-PatternAlert -FilePath $queuedPath -Patterns $patterns -EventType "Queued"
                }
            }
        }
    }
}
finally {
    # Cleanup
    Write-Host ""
    Write-Host "[Stop] Cleaning up..." -ForegroundColor Yellow
    
    $watcher.EnableRaisingEvents = $false
    Unregister-Event -SourceIdentifier $changedJob.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $createdJob.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $renamedJob.Name -ErrorAction SilentlyContinue
    $watcher.Dispose()
    
    # Close IDE pipe
    if ($script:IDEPipeHandle) {
        try { $script:IDEPipeHandle.Dispose() } catch {}
    }
    
    [RawrXD_Watcher]::ShutdownPatternEngine() | Out-Null
    
    $runtime = (Get-Date) - $script:StartTime
    Write-Host "[Stats] Runtime: $($runtime.ToString('hh\:mm\:ss'))" -ForegroundColor Gray
    
    if ($EnableCache) {
        $total = $script:CacheHits + $script:CacheMisses
        if ($total -gt 0) {
            $hitRate = [math]::Round(($script:CacheHits / $total) * 100, 1)
            Write-Host "[Stats] Cache: $($script:CacheHits) hits, $($script:CacheMisses) misses ($hitRate% hit rate)" -ForegroundColor Gray
        }
    }
    
    if ($AutoFix -and $script:AutoFixCount -gt 0) {
        Write-Host "[Stats] Auto-fixed: $($script:AutoFixCount) patterns in $($script:AutoFixFiles.Count) files" -ForegroundColor Magenta
    }
    
    Write-Host "[Done]  Watcher stopped" -ForegroundColor Green
}
