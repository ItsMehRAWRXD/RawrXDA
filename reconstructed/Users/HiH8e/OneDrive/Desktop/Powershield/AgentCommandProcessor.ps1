# =============================================================================
# RawrXD Agent Command Processor - Full Long Stable Edition
# =============================================================================
# Centralizes all chat-driven agent commands with:
#   • Deterministic parsing (token + flag aware)
#   • Natural language intent inference
#   • Registry + metadata driven command wiring
#   • Shared state for history, downloads, and help caching
#   • Dependency guards so optional modules can be toggled safely
# =============================================================================

if (-not $script:AgentCommandState) {
    $script:AgentCommandState = [ordered]@{
        Initialized        = $false
        RegistryVersion    = 0
        RegisteredCommands = @{}
        Aliases            = @{}
        History            = New-Object System.Collections.Generic.List[object]
        LastSearchResults  = @()
        ActiveDownloads    = New-Object System.Collections.Generic.List[object]
        LastHelpRender     = $null
        InitializationLog  = New-Object System.Collections.Generic.List[string]
    }
}

function Initialize-AgentCommandProcessor {
    param(
        [switch]$ForceRefresh,
        [switch]$SuppressLog
    )

    if ($script:AgentCommandState.Initialized -and -not $ForceRefresh) {
        return
    }

    $script:AgentCommandState.RegisteredCommands.Clear()
    $script:AgentCommandState.Aliases = @{}
    $script:AgentCommandState.RegistryVersion++
    $script:AgentCommandState.Initialized = $true

    if (-not $SuppressLog) {
        Write-DevConsole "🤖 Initializing AgentCommandProcessor (v$($script:AgentCommandState.RegistryVersion))" "INFO"
    }

    Register-DefaultAgentCommands
}

function Register-AgentCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Description,
        [Parameter(Mandatory = $true)][string]$Usage,
        [Parameter(Mandatory = $true)][scriptblock]$Handler,
        [string[]]$Aliases = @(),
        [hashtable]$Defaults = @{},
        [scriptblock]$Validator = $null,
        [string]$Category = "General",
        [string]$Example = ""
    )

    Initialize-AgentCommandProcessor

    $canonical = $Name.ToLower()
    $metadata = [ordered]@{
        Name        = $canonical
        Description = $Description
        Usage       = $Usage
        Aliases     = $Aliases
        Defaults    = $Defaults
        Validator   = $Validator
        Category    = $Category
        Example     = $Example
        Handler     = $Handler
    }

    $script:AgentCommandState.RegisteredCommands[$canonical] = $metadata

    foreach ($alias in $Aliases) {
        $script:AgentCommandState.Aliases[$alias.ToLower()] = $canonical
    }
}

function Get-AgentCommandRegistry {
    Initialize-AgentCommandProcessor
    return $script:AgentCommandState.RegisteredCommands.GetEnumerator() | Sort-Object { $_.Value.Category }, { $_.Key }
}

function Process-AgentCommand {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [scriptblock]$OnProgress,
        [string]$Source = "chat",
        [hashtable]$Context = @{}
    )

    Initialize-AgentCommandProcessor

    $commandText = $Command.Trim()
    if ([string]::IsNullOrWhiteSpace($commandText)) {
        return New-AgentCommandResponse -Status "error" -Message "Command was empty"
    }

    $resolved = Resolve-AgentCommand -CommandText $commandText -Context $Context
    if (-not $resolved.Success) {
        return New-AgentCommandResponse -Status "error" -Message $resolved.Message
    }

    $commandName = $resolved.Command.Name
    $handler = $resolved.Command.Handler

    Write-DevConsole "🤖 Agent command: $commandName ($($resolved.RawText))" "INFO"
    Add-AgentCommandHistory -CommandName $commandName -RawText $resolved.RawText -Context $Context

    if ($resolved.Command.Validator) {
        $validation = & $resolved.Command.Validator $resolved
        if ($validation -is [hashtable] -and -not $validation.Success) {
            return New-AgentCommandResponse -Status "error" -Message ($validation.Message ?? "Validation failed")
        }
    }

    try {
        $executionContext = [ordered]@{
            CommandName = $commandName
            Arguments   = $resolved.Arguments
            Flags       = $resolved.Flags
            RawText     = $resolved.RawText
            Source      = $Source
            OnProgress  = $OnProgress
            RequestTime = Get-Date
            Context     = $Context
        }

        $result = & $handler $executionContext
        $response = Normalize-AgentCommandResult $result
    }
    catch {
        Write-ErrorLog -ErrorMessage "Agent command '$commandName' failed: $_" -ErrorCategory "AGENTIC" -Severity "HIGH" -SourceFunction "Process-AgentCommand" -AdditionalData @{ Raw = $resolved.RawText }
        $response = New-AgentCommandResponse -Status "error" -Message $_.Exception.Message
    }

    Write-AgentCommandTelemetry -CommandName $commandName -Response $response
    return $response
}

function Resolve-AgentCommand {
    param(
        [string]$CommandText,
        [hashtable]$Context
    )

    $rawText = $CommandText.Trim()
    $textForParsing = $rawText

    if (-not $rawText.StartsWith("/")) {
        $inferred = Infer-AgentCommandFromText -UserInput $rawText -Context $Context
        if (-not $inferred) {
            return @{ Success = $false; Message = "Unknown command or intent" }
        }
        $textForParsing = $inferred
    }

    $tokens = Split-AgentCommandTokens -CommandText $textForParsing
    if (-not $tokens -or $tokens.Count -eq 0) {
        return @{ Success = $false; Message = "Unable to parse command" }
    }

    $actionToken = $tokens[0].TrimStart('/')
    $canonical = Get-AgentCanonicalName -Name $actionToken
    if (-not $canonical) {
        return @{ Success = $false; Message = "Unknown command '$actionToken'" }
    }

    $commandMetadata = $script:AgentCommandState.RegisteredCommands[$canonical]
    $argumentInfo = Convert-AgentTokensToArguments -Tokens $tokens

    return @{
        Success   = $true
        Command   = $commandMetadata
        Arguments = $argumentInfo.Arguments
        Flags     = $argumentInfo.Flags
        RawText   = $textForParsing
        Original  = $CommandText
    }
}

function Get-AgentCanonicalName {
    param([string]$Name)
    $lower = $Name.ToLower()
    if ($script:AgentCommandState.RegisteredCommands.ContainsKey($lower)) {
        return $lower
    }
    if ($script:AgentCommandState.Aliases.ContainsKey($lower)) {
        return $script:AgentCommandState.Aliases[$lower]
    }
    return $null
}

function Split-AgentCommandTokens {
    param([string]$CommandText)

    $errors = $null
    $tokens = [System.Management.Automation.PSParser]::Tokenize($CommandText, [ref]$errors)
    if ($errors -and $errors.Count -gt 0) {
        return $CommandText.Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries)
    }

    $parts = @()
    foreach ($token in $tokens) {
        if ($token.Type -in ('Command', 'CommandArgument', 'String', 'Parameter', 'Number')) {
            $parts += $token.Content
        }
    }
    return $parts
}

function Convert-AgentTokensToArguments {
    param([string[]]$Tokens)

    $args = @()
    $flags = @{}

    for ($i = 1; $i -lt $Tokens.Count; $i++) {
        $token = $Tokens[$i]
        if ($token.StartsWith('--')) {
            $flagName = $token.TrimStart('-').ToLower()
            $flagValue = $true
            if ($i + 1 -lt $Tokens.Count -and -not $Tokens[$i + 1].StartsWith('-')) {
                $flagValue = $Tokens[$i + 1]
                $i++
            }
            $flags[$flagName] = $flagValue
        }
        elseif ($token.StartsWith('-') -and $token.Length -gt 1) {
            $flags[$token.TrimStart('-').ToLower()] = $true
        }
        else {
            $args += $token
        }
    }

    return @{ Arguments = $args; Flags = $flags }
}

function Infer-AgentCommandFromText {
    param(
        [string]$UserInput,
        [hashtable]$Context
    )

    $input = $UserInput.Trim()
    if (-not $input) { return $null }

    $lower = $input.ToLower()
    switch -regex ($lower) {
        '^(search|find|look for|show me)\s+' {
            $query = $input -replace '^(search|find|look for|show me)\s+', ''
            return "/search youtube $query"
        }
        '^(download|get|save)\s+' {
            $query = $input -replace '^(download|get|save)\s+', ''
            return "/download $query"
        }
        '^(play|watch|open)\s+' {
            $query = $input -replace '^(play|watch|open)\s+', ''
            return "/play $query"
        }
        '^(go to|navigate to)\s+' {
            $url = $input -replace '^(go to|navigate to)\s+', ''
            return "/navigate $url"
        }
        default {
            return $null
        }
    }
}

function Add-AgentCommandHistory {
    param(
        [string]$CommandName,
        [string]$RawText,
        [hashtable]$Context
    )

    $entry = [ordered]@{
        Timestamp   = Get-Date
        CommandName = $CommandName
        RawText     = $RawText
        Context     = $Context
    }

    $script:AgentCommandState.History.Add($entry)
    if ($script:AgentCommandState.History.Count -gt 200) {
        $script:AgentCommandState.History.RemoveAt(0)
    }
}

function Normalize-AgentCommandResult {
    param($Result)

    if ($Result -is [hashtable]) {
        if (-not $Result.ContainsKey('Status')) { $Result['Status'] = 'success' }
        if (-not $Result.ContainsKey('Message')) { $Result['Message'] = '' }
        return $Result
    }

    return New-AgentCommandResponse -Status "success" -Message ($Result ?? 'OK')
}

function New-AgentCommandResponse {
    param(
        [string]$Status,
        [string]$Message,
        $Data = $null
    )

    return [ordered]@{
        Status  = $Status
        Message = $Message
        Data    = $Data
    }
}

function Invoke-Progress {
    param(
        [string]$Message,
        [scriptblock]$OnProgress,
        [string]$Level = "INFO"
    )

    if ($OnProgress) {
        & $OnProgress @{
            Message   = $Message
            Level     = $Level
            Timestamp = Get-Date
        }
    }

    Write-DevConsole $Message $Level
}

function Test-AgentOptionalTool {
    param(
        [string]$ToolKey,
        [bool]$Default = $true
    )

    $state = $null
    if ($script:OptionalToolState) {
        $state = $script:OptionalToolState
    }
    elseif ($script:CurrentSettings -and $script:CurrentSettings.ContainsKey('OptionalTools')) {
        $state = $script:CurrentSettings.OptionalTools
    }

    if ($state -and $state.ContainsKey($ToolKey)) {
        $value = $state[$ToolKey]
        if ($value -is [hashtable] -and $value.ContainsKey('Enabled')) {
            return [bool]$value.Enabled
        }
        return [bool]$value
    }

    return $Default
}

function Ensure-AgentDependency {
    param(
        [string]$CommandName,
        [string]$FriendlyName,
        [string]$ToolKey
    )

    if ($ToolKey -and -not (Test-AgentOptionalTool -ToolKey $ToolKey)) {
        throw "Optional tool '$FriendlyName' is disabled in settings"
    }

    if (-not (Get-Command $CommandName -ErrorAction SilentlyContinue)) {
        throw "Required dependency '$FriendlyName' ($CommandName) is not loaded"
    }
}

function Invoke-AgentSearch {
    param([hashtable]$ExecutionContext)

    $args       = $ExecutionContext.Arguments
    $flags      = $ExecutionContext.Flags
    $onProgress = $ExecutionContext.OnProgress

    $source = if ($args.Count -gt 0) { $args[0] } elseif ($flags['source']) { $flags['source'] } else { 'youtube' }
    $query  = if ($args.Count -gt 1) { [string]::Join(' ', $args[1..($args.Count - 1)]) } elseif ($flags['query']) { $flags['query'] } else { '' }

    if (-not $query) {
        return New-AgentCommandResponse -Status "error" -Message "Search query missing"
    }

    Invoke-Progress "🔍 Searching $source for '$query'" $onProgress

    Ensure-AgentDependency -CommandName Search-YouTubeFromBrowser -FriendlyName "BrowserAutomation" -ToolKey 'browserAutomation'

    $results = @()
    switch -Wildcard ($source.ToLower()) {
        { $_ -like '*youtube*' -or $_ -eq 'yt' } {
            $results = Search-YouTubeFromBrowser -Query $query -MaxResults 10
        }
        default {
            $results = Search-YouTubeFromBrowser -Query $query -MaxResults 10
        }
    }

    if (-not $results -or $results.Count -eq 0) {
        Invoke-Progress "❌ No results found" $onProgress "WARNING"
        return New-AgentCommandResponse -Status "error" -Message "No results found"
    }

    $script:AgentCommandState.LastSearchResults = $results
    $formatted = Format-AgentSearchResults -Results $results

    Invoke-Progress "✅ Found $($results.Count) results" $onProgress

    return [ordered]@{
        Status  = 'success'
        Message = "Found $($results.Count) results"
        Results = $results
        Display = $formatted
    }
}

function Format-AgentSearchResults {
    param([array]$Results)

    $lines = @("Agent > Found $($Results.Count) videos:", '')
    for ($i = 0; $i -lt $Results.Count; $i++) {
        $video = $Results[$i]
        $num = $i + 1
        $lines += "  $num. $($video.Title) [$($video.Duration)]"
        $lines += "     Channel: $($video.Channel) | Views: $($video.Views)"
        $lines += ''
    }
    $lines += "Agent > Use '/play 1' to play or '/download 1' to save"
    return [string]::Join("`r`n", $lines)
}

function Invoke-AgentDownload {
    param([hashtable]$ExecutionContext)

    $args       = $ExecutionContext.Arguments
    $flags      = $ExecutionContext.Flags
    $onProgress = $ExecutionContext.OnProgress

    if ($args.Count -lt 1) {
        return New-AgentCommandResponse -Status "error" -Message "Download requires a URL or result number"
    }

    $target = $args[0]
    $quality = $flags['quality'] ?? $args[1] ?? 'best'
    $destination = $flags['dest'] ?? $flags['destination'] ?? $args[2] ?? (Join-Path $env:USERPROFILE 'Videos')

    if ($target -match '^[0-9]+$' -and $script:AgentCommandState.LastSearchResults.Count -gt 0) {
        $index = [int]$target - 1
        if ($index -lt 0 -or $index -ge $script:AgentCommandState.LastSearchResults.Count) {
            return New-AgentCommandResponse -Status "error" -Message "Search result number '$target' is invalid"
        }
        $target = $script:AgentCommandState.LastSearchResults[$index].URL
    }

    Invoke-Progress "📥 Starting download ($quality): $target" $onProgress

    Ensure-AgentDependency -CommandName Invoke-MultiThreadedDownload -FriendlyName "DownloadManager" -ToolKey 'downloadManager'

    $validatedPath = Get-ValidatedDownloadTarget -Destination $destination -Url $target
    $downloadResult = Invoke-MultiThreadedDownload -URL $target -OutputPath $validatedPath -ThreadCount 4 -MaxRetries 3

    if (-not $downloadResult.Success) {
        Invoke-Progress "❌ Download failed: $($downloadResult.Error)" $onProgress "ERROR"
        return New-AgentCommandResponse -Status "error" -Message $downloadResult.Error
    }

    $downloadEntry = [ordered]@{
        Path   = $validatedPath
        Size   = $downloadResult.FileSize
        Speed  = $downloadResult.Speed
        Status = 'Completed'
        Time   = Get-Date
    }

    $script:AgentCommandState.ActiveDownloads.Add($downloadEntry)
    if ($script:AgentCommandState.ActiveDownloads.Count -gt 25) {
        $script:AgentCommandState.ActiveDownloads.RemoveAt(0)
    }

    Invoke-Progress "✅ Download complete: $validatedPath" $onProgress

    return [ordered]@{
        Status   = 'success'
        Message  = "Saved to $validatedPath"
        FilePath = $validatedPath
        Size     = $downloadResult.FileSize
        Speed    = $downloadResult.Speed
    }
}

function Get-ValidatedDownloadTarget {
    param(
        [string]$Destination,
        [string]$Url
    )

    if (-not (Test-Path $Destination)) {
        New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    }

    $fileName = [System.IO.Path]::GetFileName($Url)
    if (-not $fileName) {
        $hash = [System.BitConverter]::ToString((New-Guid).ToByteArray()).Replace('-', '')
        $fileName = "download_$hash.bin"
    }

    return Join-Path $Destination $fileName
}

function Invoke-AgentPlayback {
    param([hashtable]$ExecutionContext)

    $args       = $ExecutionContext.Arguments
    $onProgress = $ExecutionContext.OnProgress

    if ($args.Count -lt 1) {
        return New-AgentCommandResponse -Status "error" -Message "Play needs a URL or result number"
    }

    $target = $args[0]
    if ($target -match '^[0-9]+$' -and $script:AgentCommandState.LastSearchResults.Count -gt 0) {
        $index = [int]$target - 1
        if ($index -lt 0 -or $index -ge $script:AgentCommandState.LastSearchResults.Count) {
            return New-AgentCommandResponse -Status "error" -Message "Search result number '$target' is invalid"
        }
        $target = $script:AgentCommandState.LastSearchResults[$index].URL
    }

    Invoke-Progress "▶️  Opening: $target" $onProgress
    Ensure-AgentDependency -CommandName Open-Browser -FriendlyName "BrowserAutomation" -ToolKey 'browserAutomation'
    Open-Browser $target
    Invoke-Progress "✅ Video started" $onProgress

    return New-AgentCommandResponse -Status "success" -Message "Playing via browser" -Data @{ URL = $target }
}

function Invoke-AgentPlaylistCommand {
    param([hashtable]$ExecutionContext)

    $args       = $ExecutionContext.Arguments
    $flags      = $ExecutionContext.Flags
    $onProgress = $ExecutionContext.OnProgress

    if ($args.Count -lt 1) {
        return New-AgentCommandResponse -Status "error" -Message "Playlist action missing"
    }

    $action = $args[0].ToLower()
    switch ($action) {
        'create' {
            $query = if ($args.Count -gt 1) { [string]::Join(' ', $args[1..($args.Count - 1)]) } else { $flags['query'] }
            if (-not $query) {
                return New-AgentCommandResponse -Status "error" -Message "Playlist query missing"
            }

            Invoke-Progress "📝 Building playlist from '$query'" $onProgress
            Ensure-AgentDependency -CommandName Search-YouTubeFromBrowser -FriendlyName "BrowserAutomation" -ToolKey 'browserAutomation'

            $results = Search-YouTubeFromBrowser -Query $query -MaxResults 20
            if (-not $results) {
                return New-AgentCommandResponse -Status "error" -Message "No videos for playlist"
            }

            $playlistDir = Join-Path $env:USERPROFILE 'Videos'
            if (-not (Test-Path $playlistDir)) {
                New-Item -Path $playlistDir -ItemType Directory -Force | Out-Null
            }
            $playlistPath = Join-Path $playlistDir "Playlist_$([DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss')).m3u"

            $content = @('#EXTM3U')
            foreach ($video in $results) {
                $content += "#EXTINF:-1,$($video.Title)"
                $content += $video.URL
            }
            $content | Set-Content -Path $playlistPath -Encoding UTF8

            Invoke-Progress "✅ Playlist saved: $playlistPath" $onProgress
            return [ordered]@{ Status = 'success'; Message = 'Playlist created'; Path = $playlistPath; Count = $results.Count }
        }
        'list' {
            $files = Get-ChildItem (Join-Path $env:USERPROFILE 'Videos') -Filter '*.m3u' -ErrorAction SilentlyContinue
            return [ordered]@{ Status = 'success'; Message = "Found $($files.Count) playlists"; Files = $files }
        }
        default {
            return New-AgentCommandResponse -Status "error" -Message "Unsupported playlist action '$action'"
        }
    }
}

function Invoke-AgentNavigate {
    param([hashtable]$ExecutionContext)

    $args       = $ExecutionContext.Arguments
    $onProgress = $ExecutionContext.OnProgress

    if ($args.Count -lt 1) {
        return New-AgentCommandResponse -Status "error" -Message "URL missing"
    }

    $url = $args -join ' '
    if (-not $url.StartsWith('http')) { $url = "https://$url" }

    Invoke-Progress "🌐 Navigating to $url" $onProgress
    Ensure-AgentDependency -CommandName Open-Browser -FriendlyName "BrowserAutomation" -ToolKey 'browserAutomation'
    Open-Browser $url
    Invoke-Progress "✅ Navigation complete" $onProgress

    return New-AgentCommandResponse -Status "success" -Message "Opened $url" -Data @{ URL = $url }
}

function Invoke-AgentClick {
    param([hashtable]$ExecutionContext)

    $args       = $ExecutionContext.Arguments
    $onProgress = $ExecutionContext.OnProgress

    if ($args.Count -lt 1) {
        return New-AgentCommandResponse -Status "error" -Message "CSS selector required"
    }

    $selector = $args[0]
    Invoke-Progress "🖱️  Clicking $selector" $onProgress
    Ensure-AgentDependency -CommandName Invoke-BrowserClick -FriendlyName "BrowserAutomation" -ToolKey 'browserAutomation'
    $result = Invoke-BrowserClick -Selector $selector

    if ($result) {
        Invoke-Progress "✅ Click succeeded" $onProgress
        return New-AgentCommandResponse -Status "success" -Message "Clicked $selector"
    }

    return New-AgentCommandResponse -Status "error" -Message "Element '$selector' not found"
}

function Invoke-AgentScreenshot {
    param([hashtable]$ExecutionContext)

    $onProgress = $ExecutionContext.OnProgress
    Invoke-Progress "📸 Capturing screenshot" $onProgress
    Ensure-AgentDependency -CommandName Get-BrowserScreenshot -FriendlyName "BrowserAutomation" -ToolKey 'browserAutomation'
    $path = Get-BrowserScreenshot
    if ($path) {
        Invoke-Progress "✅ Screenshot saved: $path" $onProgress
        return New-AgentCommandResponse -Status "success" -Message "Saved screenshot" -Data @{ Path = $path }
    }
    return New-AgentCommandResponse -Status "error" -Message "Screenshot failed"
}

function Invoke-AgentStatusCommand {
    param([hashtable]$ExecutionContext)

    $dependencies = @(
        @{ Name = 'BrowserAutomation'; Command = 'Open-Browser'; ToolKey = 'browserAutomation' },
        @{ Name = 'YouTube Search'; Command = 'Search-YouTubeFromBrowser'; ToolKey = 'browserAutomation' },
        @{ Name = 'Download Manager'; Command = 'Invoke-MultiThreadedDownload'; ToolKey = 'downloadManager' }
    )

    $report = @()
    foreach ($dep in $dependencies) {
        $available = [bool](Get-Command $dep.Command -ErrorAction SilentlyContinue)
        $enabled = if ($dep.ToolKey) { Test-AgentOptionalTool -ToolKey $dep.ToolKey } else { $true }
        $report += [ordered]@{
            Component = $dep.Name
            Command   = $dep.Command
            Available = $available
            Enabled   = $enabled
        }
    }

    $optionalInfo = @()
    foreach ($tool in @(
            @{ Key = 'browserAutomation'; Name = 'Browser Automation' },
            @{ Key = 'downloadManager'; Name = 'Download Manager' },
            @{ Key = 'videoEngine'; Name = 'Video Engine' }
        )) {
        $optionalInfo += [ordered]@{
            ToolKey = $tool.Key
            Name    = $tool.Name
            Enabled = Test-AgentOptionalTool -ToolKey $tool.Key
        }
    }

    return [ordered]@{
        Status        = 'success'
        Message       = 'Agent status'
        Components    = $report
        OptionalTools = $optionalInfo
        History       = $script:AgentCommandState.History
        Downloads     = $script:AgentCommandState.ActiveDownloads
    }
}

function Invoke-AgentHelpCommand {
    param([hashtable]$ExecutionContext)

    $registry = Get-AgentCommandRegistry
    $lines = @('Agent Command Reference', '-----------------------')

    foreach ($entry in $registry) {
        $cmd = $entry.Value
        $aliasText = if ($cmd.Aliases.Count -gt 0) { " (aliases: " + ($cmd.Aliases -join ', ') + ")" } else { '' }
        $lines += "• /$($cmd.Name)$aliasText"
        $lines += "  $($cmd.Description)"
        $lines += "  Usage: $($cmd.Usage)"
        if ($cmd.Example) {
            $lines += "  Example: $($cmd.Example)"
        }
        $lines += ''
    }

    $text = [string]::Join("`r`n", $lines)
    $script:AgentCommandState.LastHelpRender = $text

    return [ordered]@{ Status = 'success'; Message = 'Help generated'; Display = $text }
}

function Write-AgentCommandTelemetry {
    param(
        [string]$CommandName,
        [hashtable]$Response
    )

    try {
        $entry = [ordered]@{
            Timestamp = Get-Date
            Command   = $CommandName
            Status    = $Response.Status
            Message   = $Response.Message
        }
        $script:AgentCommandState.InitializationLog.Add((ConvertTo-Json $entry -Compress))
        if ($script:AgentCommandState.InitializationLog.Count -gt 50) {
            $script:AgentCommandState.InitializationLog.RemoveAt(0)
        }
    }
    catch {
        Write-DevConsole "Telemetry logging failed: $_" "WARNING"
    }
}

function Register-DefaultAgentCommands {
    Register-AgentCommand -Name 'search' -Description 'Search supported sources (default YouTube)' -Usage '/search [source] <query>' -Handler { Invoke-AgentSearch $_ } -Aliases @('find') -Category 'Media' -Example '/search youtube power metal'

    Register-AgentCommand -Name 'download' -Description 'Download by URL or last search index' -Usage '/download <url | result#> [dest] [quality]' -Handler { Invoke-AgentDownload $_ } -Aliases @('dl') -Category 'Media' -Example '/download 1 --quality best'

    Register-AgentCommand -Name 'play' -Description 'Play a URL or search result inside the browser panel' -Usage '/play <url | result#>' -Handler { Invoke-AgentPlayback $_ } -Aliases @('watch') -Category 'Media' -Example '/play 2'

    Register-AgentCommand -Name 'stream' -Description 'Stream content with a requested quality' -Usage '/stream <url | result#> [quality]' -Handler { Invoke-AgentPlayback $_ } -Category 'Media' -Example '/stream 1 720p'

    Register-AgentCommand -Name 'playlist' -Description 'Create or list playlists based on searches' -Usage '/playlist <create | list> [query]' -Handler { Invoke-AgentPlaylistCommand $_ } -Category 'Media' -Example '/playlist create synthwave'

    Register-AgentCommand -Name 'navigate' -Description 'Navigate browser panel to URL' -Usage '/navigate <url>' -Handler { Invoke-AgentNavigate $_ } -Aliases @('nav', 'goto') -Category 'Browser' -Example '/navigate github.com'

    Register-AgentCommand -Name 'click' -Description 'Click DOM element in browser via CSS selector' -Usage '/click <selector>' -Handler { Invoke-AgentClick $_ } -Category 'Browser' -Example "'/click button[data-id=\'subscribe\']'"

    Register-AgentCommand -Name 'screenshot' -Description 'Capture current browser view' -Usage '/screenshot' -Handler { Invoke-AgentScreenshot $_ } -Category 'Browser'

    Register-AgentCommand -Name 'status' -Description 'Show agent dependency health and history' -Usage '/status' -Handler { Invoke-AgentStatusCommand $_ } -Category 'Diagnostics'

    Register-AgentCommand -Name 'help' -Description 'Render command list' -Usage '/help' -Handler { Invoke-AgentHelpCommand $_ } -Category 'Diagnostics' -Aliases @('commands')
}

Initialize-AgentCommandProcessor -SuppressLog
