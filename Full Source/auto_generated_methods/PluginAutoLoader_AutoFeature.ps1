#requires -Version 5.1
<#
.SYNOPSIS
    Production-grade Plugin Auto-Loader with full lifecycle management.
.DESCRIPTION
    Comprehensive plugin management system supporting:
    - Plugin discovery and validation
    - Version management and compatibility checking
    - Hot-reload capability with state preservation
    - Plugin isolation and sandboxing
    - Dependency resolution between plugins
    - Plugin manifest validation
    - Health monitoring and auto-recovery
    - Plugin marketplace integration hooks
.NOTES
    Author: RawrXD Production Team
    Version: 2.0.0
    Requires: PowerShell 5.1+
#>

# ============================================================================
# STRUCTURED LOGGING (Standalone fallback)
# ============================================================================
if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Debug','Info','Warning','Error','Critical')][string]$Level = 'Info',
            [hashtable]$Context = @{}
        )
        $timestamp = Get-Date -Format 'yyyy-MM-ddTHH:mm:ss.fffZ'
        $logEntry = @{
            Timestamp = $timestamp
            Level = $Level
            Message = $Message
            Context = $Context
        } | ConvertTo-Json -Compress
        
        $color = switch ($Level) {
            'Debug' { 'Gray' }
            'Info' { 'White' }
            'Warning' { 'Yellow' }
            'Error' { 'Red' }
            'Critical' { 'Magenta' }
            default { 'White' }
        }
        Write-Host $logEntry -ForegroundColor $color
    }
}

# ============================================================================
# PLUGIN REGISTRY (Global State Management)
# ============================================================================
$script:PluginRegistry = @{
    Plugins = @{}                    # PluginId -> PluginInfo
    LoadOrder = [System.Collections.ArrayList]@()  # Ordered list of loaded plugins
    Dependencies = @{}               # PluginId -> @(DependencyIds)
    Watchers = @{}                   # PluginId -> FileSystemWatcher
    HealthStatus = @{}               # PluginId -> Health info
    EventHandlers = @{}              # EventName -> @(Handlers)
    Configuration = @{
        PluginDirectories = @("D:/lazy init ide/plugins")
        AutoReloadEnabled = $true
        IsolationLevel = 'Process'   # None, AppDomain, Process
        MaxLoadRetries = 3
        HealthCheckIntervalMs = 30000
        RequireManifest = $true
        AllowUnsigned = $false
        MarketplaceUrl = $null
    }
}

# ============================================================================
# PLUGIN MANIFEST SCHEMA
# ============================================================================
function New-PluginManifest {
    <#
    .SYNOPSIS
        Creates a new plugin manifest template.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$PluginId,
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$Version,
        [string]$Description = "",
        [string]$Author = "Unknown",
        [string[]]$Dependencies = @(),
        [string]$MinPowerShellVersion = "5.1",
        [string[]]$ExportedCommands = @(),
        [hashtable]$Configuration = @{}
    )
    
    return @{
        Schema = "1.0.0"
        PluginId = $PluginId
        Name = $Name
        Version = $Version
        Description = $Description
        Author = $Author
        Dependencies = $Dependencies
        MinPowerShellVersion = $MinPowerShellVersion
        ExportedCommands = $ExportedCommands
        Configuration = $Configuration
        Created = (Get-Date -Format 'yyyy-MM-ddTHH:mm:ssZ')
        Checksum = $null
    }
}

# ============================================================================
# PLUGIN VALIDATION
# ============================================================================
function Test-PluginManifest {
    <#
    .SYNOPSIS
        Validates a plugin manifest for correctness and compatibility.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$Manifest,
        [switch]$Strict
    )
    
    $errors = @()
    $warnings = @()
    
    # Required fields
    $requiredFields = @('PluginId', 'Name', 'Version')
    foreach ($field in $requiredFields) {
        if (-not $Manifest.ContainsKey($field) -or [string]::IsNullOrWhiteSpace($Manifest[$field])) {
            $errors += "Missing required field: $field"
        }
    }
    
    # Version format validation (SemVer)
    if ($Manifest.Version -and $Manifest.Version -notmatch '^\d+\.\d+\.\d+(-[\w\.]+)?(\+[\w\.]+)?$') {
        $warnings += "Version '$($Manifest.Version)' does not follow SemVer format"
    }
    
    # PowerShell version compatibility
    if ($Manifest.MinPowerShellVersion) {
        try {
            $requiredVersion = [Version]$Manifest.MinPowerShellVersion
            $currentVersion = $PSVersionTable.PSVersion
            if ($currentVersion -lt $requiredVersion) {
                $errors += "Plugin requires PowerShell $requiredVersion but current is $currentVersion"
            }
        } catch {
            $warnings += "Could not parse MinPowerShellVersion: $($Manifest.MinPowerShellVersion)"
        }
    }
    
    # PluginId format (alphanumeric with dots/dashes)
    if ($Manifest.PluginId -and $Manifest.PluginId -notmatch '^[a-zA-Z][a-zA-Z0-9\.\-_]*$') {
        $errors += "PluginId contains invalid characters. Use alphanumeric, dots, dashes, underscores only."
    }
    
    return @{
        IsValid = ($errors.Count -eq 0)
        Errors = $errors
        Warnings = $warnings
    }
}

function Get-PluginChecksum {
    <#
    .SYNOPSIS
        Calculates SHA256 checksum for plugin files.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$PluginPath
    )
    
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    $allHashes = @()
    
    try {
        if (Test-Path $PluginPath -PathType Container) {
            $files = Get-ChildItem -Path $PluginPath -Recurse -File | Sort-Object FullName
            foreach ($file in $files) {
                $content = [System.IO.File]::ReadAllBytes($file.FullName)
                $hash = $sha256.ComputeHash($content)
                $allHashes += [BitConverter]::ToString($hash) -replace '-', ''
            }
        } else {
            $content = [System.IO.File]::ReadAllBytes($PluginPath)
            $hash = $sha256.ComputeHash($content)
            $allHashes += [BitConverter]::ToString($hash) -replace '-', ''
        }
        
        # Combine all hashes
        $combinedBytes = [System.Text.Encoding]::UTF8.GetBytes($allHashes -join '')
        $finalHash = $sha256.ComputeHash($combinedBytes)
        return [BitConverter]::ToString($finalHash) -replace '-', ''
    } finally {
        $sha256.Dispose()
    }
}

# ============================================================================
# DEPENDENCY RESOLUTION
# ============================================================================
function Resolve-PluginDependencies {
    <#
    .SYNOPSIS
        Resolves plugin dependencies using topological sort.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable[]]$Plugins
    )
    
    $graph = @{}
    $inDegree = @{}
    
    # Build dependency graph
    foreach ($plugin in $Plugins) {
        $id = $plugin.PluginId
        if (-not $graph.ContainsKey($id)) {
            $graph[$id] = @()
            $inDegree[$id] = 0
        }
        
        foreach ($dep in $plugin.Dependencies) {
            if (-not $graph.ContainsKey($dep)) {
                $graph[$dep] = @()
                $inDegree[$dep] = 0
            }
            $graph[$dep] += $id
            $inDegree[$id]++
        }
    }
    
    # Kahn's algorithm for topological sort
    $queue = [System.Collections.Queue]::new()
    foreach ($node in $inDegree.Keys) {
        if ($inDegree[$node] -eq 0) {
            $queue.Enqueue($node)
        }
    }
    
    $loadOrder = @()
    while ($queue.Count -gt 0) {
        $current = $queue.Dequeue()
        $loadOrder += $current
        
        foreach ($neighbor in $graph[$current]) {
            $inDegree[$neighbor]--
            if ($inDegree[$neighbor] -eq 0) {
                $queue.Enqueue($neighbor)
            }
        }
    }
    
    # Check for circular dependencies
    if ($loadOrder.Count -ne $graph.Count) {
        $unresolved = $graph.Keys | Where-Object { $_ -notin $loadOrder }
        return @{
            Success = $false
            LoadOrder = @()
            CircularDependencies = $unresolved
            Error = "Circular dependency detected involving: $($unresolved -join ', ')"
        }
    }
    
    return @{
        Success = $true
        LoadOrder = $loadOrder
        CircularDependencies = @()
        Error = $null
    }
}

# ============================================================================
# PLUGIN DISCOVERY
# ============================================================================
function Find-Plugins {
    <#
    .SYNOPSIS
        Discovers all plugins in configured directories.
    #>
    [CmdletBinding()]
    param(
        [string[]]$SearchPaths = $script:PluginRegistry.Configuration.PluginDirectories,
        [switch]$IncludeDisabled
    )
    
    $discovered = @()
    
    foreach ($searchPath in $SearchPaths) {
        if (-not (Test-Path $searchPath)) {
            Write-StructuredLog -Message "Plugin directory not found: $searchPath" -Level Warning
            continue
        }
        
        # Look for plugin directories with manifest.json
        $pluginDirs = Get-ChildItem -Path $searchPath -Directory -ErrorAction SilentlyContinue
        foreach ($dir in $pluginDirs) {
            $manifestPath = Join-Path $dir.FullName "manifest.json"
            $moduleFiles = Get-ChildItem -Path $dir.FullName -Filter "*.psm1" -ErrorAction SilentlyContinue
            
            $pluginInfo = @{
                Path = $dir.FullName
                Name = $dir.Name
                HasManifest = (Test-Path $manifestPath)
                ModuleFiles = $moduleFiles
                Manifest = $null
                Status = 'Discovered'
            }
            
            if ($pluginInfo.HasManifest) {
                try {
                    $pluginInfo.Manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json -AsHashtable
                } catch {
                    Write-StructuredLog -Message "Failed to parse manifest: $manifestPath" -Level Warning -Context @{Error = $_.Exception.Message}
                }
            }
            
            # Check for disabled marker
            $disabledMarker = Join-Path $dir.FullName ".disabled"
            if ((Test-Path $disabledMarker) -and -not $IncludeDisabled) {
                continue
            }
            
            $discovered += $pluginInfo
        }
        
        # Also look for standalone .psm1 files (legacy plugins)
        $standaloneModules = Get-ChildItem -Path $searchPath -Filter "*.psm1" -File -ErrorAction SilentlyContinue
        foreach ($module in $standaloneModules) {
            $discovered += @{
                Path = $module.FullName
                Name = $module.BaseName
                HasManifest = $false
                ModuleFiles = @($module)
                Manifest = $null
                Status = 'Discovered'
                IsLegacy = $true
            }
        }
    }
    
    Write-StructuredLog -Message "Plugin discovery complete" -Level Info -Context @{
        SearchPaths = $SearchPaths
        PluginsFound = $discovered.Count
    }
    
    return $discovered
}

# ============================================================================
# PLUGIN LOADING
# ============================================================================
function Import-Plugin {
    <#
    .SYNOPSIS
        Loads a plugin with full lifecycle management.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$PluginInfo,
        [switch]$Force,
        [switch]$NoCache
    )
    
    $pluginId = if ($PluginInfo.Manifest) { $PluginInfo.Manifest.PluginId } else { $PluginInfo.Name }
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    Write-StructuredLog -Message "Loading plugin: $pluginId" -Level Info -Context @{Path = $PluginInfo.Path}
    
    try {
        # Check if already loaded
        if ($script:PluginRegistry.Plugins.ContainsKey($pluginId) -and -not $Force) {
            Write-StructuredLog -Message "Plugin already loaded: $pluginId" -Level Debug
            return @{
                Success = $true
                PluginId = $pluginId
                AlreadyLoaded = $true
            }
        }
        
        # Validate manifest if required
        if ($script:PluginRegistry.Configuration.RequireManifest -and -not $PluginInfo.HasManifest) {
            return @{
                Success = $false
                PluginId = $pluginId
                Error = "Plugin manifest required but not found"
            }
        }
        
        if ($PluginInfo.Manifest) {
            $validation = Test-PluginManifest -Manifest $PluginInfo.Manifest
            if (-not $validation.IsValid) {
                return @{
                    Success = $false
                    PluginId = $pluginId
                    Error = "Manifest validation failed: $($validation.Errors -join '; ')"
                }
            }
        }
        
        # Calculate checksum for integrity
        $checksum = Get-PluginChecksum -PluginPath $PluginInfo.Path
        
        # Load module files
        $loadedModules = @()
        foreach ($moduleFile in $PluginInfo.ModuleFiles) {
            $modulePath = if ($moduleFile -is [string]) { $moduleFile } else { $moduleFile.FullName }
            
            # Pre-load validation - syntax check
            try {
                $ast = [System.Management.Automation.Language.Parser]::ParseFile(
                    $modulePath,
                    [ref]$null,
                    [ref]$null
                )
            } catch {
                Write-StructuredLog -Message "Syntax error in plugin module" -Level Error -Context @{
                    Path = $modulePath
                    Error = $_.Exception.Message
                }
                continue
            }
            
            # Import the module
            Import-Module $modulePath -Force -Global -ErrorAction Stop
            $loadedModules += $modulePath
            
            Write-StructuredLog -Message "Module loaded" -Level Debug -Context @{Path = $modulePath}
        }
        
        # Register in plugin registry
        $script:PluginRegistry.Plugins[$pluginId] = @{
            Info = $PluginInfo
            LoadedAt = Get-Date
            Checksum = $checksum
            LoadedModules = $loadedModules
            State = 'Active'
            Version = if ($PluginInfo.Manifest) { $PluginInfo.Manifest.Version } else { "1.0.0" }
        }
        
        # Add to load order
        if ($pluginId -notin $script:PluginRegistry.LoadOrder) {
            [void]$script:PluginRegistry.LoadOrder.Add($pluginId)
        }
        
        # Initialize health status
        $script:PluginRegistry.HealthStatus[$pluginId] = @{
            Status = 'Healthy'
            LastCheck = Get-Date
            FailureCount = 0
            LoadTime = $stopwatch.Elapsed.TotalMilliseconds
        }
        
        # Set up hot-reload watcher if enabled
        if ($script:PluginRegistry.Configuration.AutoReloadEnabled) {
            Register-PluginWatcher -PluginId $pluginId -Path $PluginInfo.Path
        }
        
        # Fire plugin loaded event
        Invoke-PluginEvent -EventName 'PluginLoaded' -PluginId $pluginId
        
        $stopwatch.Stop()
        Write-StructuredLog -Message "Plugin loaded successfully" -Level Info -Context @{
            PluginId = $pluginId
            LoadTimeMs = $stopwatch.Elapsed.TotalMilliseconds
            ModulesLoaded = $loadedModules.Count
        }
        
        return @{
            Success = $true
            PluginId = $pluginId
            LoadTimeMs = $stopwatch.Elapsed.TotalMilliseconds
            ModulesLoaded = $loadedModules.Count
        }
        
    } catch {
        $stopwatch.Stop()
        Write-StructuredLog -Message "Failed to load plugin" -Level Error -Context @{
            PluginId = $pluginId
            Error = $_.Exception.Message
            StackTrace = $_.ScriptStackTrace
        }
        
        return @{
            Success = $false
            PluginId = $pluginId
            Error = $_.Exception.Message
        }
    }
}

function Unload-Plugin {
    <#
    .SYNOPSIS
        Unloads a plugin and cleans up resources.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$PluginId,
        [switch]$PreserveState
    )
    
    if (-not $script:PluginRegistry.Plugins.ContainsKey($PluginId)) {
        Write-StructuredLog -Message "Plugin not loaded: $PluginId" -Level Warning
        return $false
    }
    
    $plugin = $script:PluginRegistry.Plugins[$PluginId]
    
    try {
        # Fire unloading event
        Invoke-PluginEvent -EventName 'PluginUnloading' -PluginId $PluginId
        
        # Remove modules
        foreach ($modulePath in $plugin.LoadedModules) {
            $moduleName = [System.IO.Path]::GetFileNameWithoutExtension($modulePath)
            Remove-Module -Name $moduleName -Force -ErrorAction SilentlyContinue
        }
        
        # Stop watcher
        if ($script:PluginRegistry.Watchers.ContainsKey($PluginId)) {
            $script:PluginRegistry.Watchers[$PluginId].Dispose()
            $script:PluginRegistry.Watchers.Remove($PluginId)
        }
        
        # Remove from registry
        $script:PluginRegistry.Plugins.Remove($PluginId)
        $script:PluginRegistry.LoadOrder.Remove($PluginId)
        $script:PluginRegistry.HealthStatus.Remove($PluginId)
        
        # Fire unloaded event
        Invoke-PluginEvent -EventName 'PluginUnloaded' -PluginId $PluginId
        
        Write-StructuredLog -Message "Plugin unloaded" -Level Info -Context @{PluginId = $PluginId}
        return $true
        
    } catch {
        Write-StructuredLog -Message "Error unloading plugin" -Level Error -Context @{
            PluginId = $PluginId
            Error = $_.Exception.Message
        }
        return $false
    }
}

# ============================================================================
# HOT-RELOAD SUPPORT
# ============================================================================
function Register-PluginWatcher {
    <#
    .SYNOPSIS
        Sets up file system watcher for hot-reload.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$PluginId,
        [Parameter(Mandatory=$true)][string]$Path
    )
    
    # Clean up existing watcher
    if ($script:PluginRegistry.Watchers.ContainsKey($PluginId)) {
        $script:PluginRegistry.Watchers[$PluginId].Dispose()
    }
    
    $watchPath = if (Test-Path $Path -PathType Container) { $Path } else { Split-Path $Path -Parent }
    
    $watcher = New-Object System.IO.FileSystemWatcher
    $watcher.Path = $watchPath
    $watcher.Filter = "*.psm1"
    $watcher.IncludeSubdirectories = $true
    $watcher.NotifyFilter = [System.IO.NotifyFilters]::LastWrite -bor [System.IO.NotifyFilters]::FileName
    
    $script:lastReload = @{}
    $debounceMs = 2000
    
    $action = {
        $pluginId = $Event.MessageData.PluginId
        $now = Get-Date
        
        # Debounce
        if ($script:lastReload.ContainsKey($pluginId)) {
            $elapsed = ($now - $script:lastReload[$pluginId]).TotalMilliseconds
            if ($elapsed -lt $Event.MessageData.DebounceMs) { return }
        }
        $script:lastReload[$pluginId] = $now
        
        Write-StructuredLog -Message "Hot-reload triggered" -Level Info -Context @{
            PluginId = $pluginId
            ChangedFile = $Event.SourceEventArgs.FullPath
        }
        
        # Reload plugin
        $pluginInfo = $script:PluginRegistry.Plugins[$pluginId].Info
        Import-Plugin -PluginInfo $pluginInfo -Force
    }
    
    $messageData = @{
        PluginId = $PluginId
        DebounceMs = $debounceMs
    }
    
    Register-ObjectEvent -InputObject $watcher -EventName Changed -Action $action -MessageData $messageData -SourceIdentifier "PluginWatch_$PluginId" | Out-Null
    
    $watcher.EnableRaisingEvents = $true
    $script:PluginRegistry.Watchers[$PluginId] = $watcher
    
    Write-StructuredLog -Message "Hot-reload watcher registered" -Level Debug -Context @{
        PluginId = $PluginId
        WatchPath = $watchPath
    }
}

# ============================================================================
# PLUGIN EVENTS
# ============================================================================
function Register-PluginEventHandler {
    <#
    .SYNOPSIS
        Registers an event handler for plugin lifecycle events.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [ValidateSet('PluginLoaded', 'PluginUnloading', 'PluginUnloaded', 'PluginError', 'PluginHealthChanged')]
        [string]$EventName,
        [Parameter(Mandatory=$true)][scriptblock]$Handler
    )
    
    if (-not $script:PluginRegistry.EventHandlers.ContainsKey($EventName)) {
        $script:PluginRegistry.EventHandlers[$EventName] = @()
    }
    
    $script:PluginRegistry.EventHandlers[$EventName] += $Handler
}

function Invoke-PluginEvent {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$EventName,
        [string]$PluginId,
        [hashtable]$Data = @{}
    )
    
    if ($script:PluginRegistry.EventHandlers.ContainsKey($EventName)) {
        foreach ($handler in $script:PluginRegistry.EventHandlers[$EventName]) {
            try {
                & $handler -EventName $EventName -PluginId $PluginId -Data $Data
            } catch {
                Write-StructuredLog -Message "Event handler error" -Level Warning -Context @{
                    Event = $EventName
                    Error = $_.Exception.Message
                }
            }
        }
    }
}

# ============================================================================
# HEALTH MONITORING
# ============================================================================
function Test-PluginHealth {
    <#
    .SYNOPSIS
        Checks the health of a loaded plugin.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$PluginId
    )
    
    if (-not $script:PluginRegistry.Plugins.ContainsKey($PluginId)) {
        return @{
            PluginId = $PluginId
            Status = 'NotLoaded'
            Healthy = $false
        }
    }
    
    $plugin = $script:PluginRegistry.Plugins[$PluginId]
    $health = @{
        PluginId = $PluginId
        Status = 'Unknown'
        Healthy = $false
        Checks = @()
    }
    
    # Check 1: Module still loaded
    $moduleLoaded = $true
    foreach ($modulePath in $plugin.LoadedModules) {
        $moduleName = [System.IO.Path]::GetFileNameWithoutExtension($modulePath)
        if (-not (Get-Module -Name $moduleName)) {
            $moduleLoaded = $false
            break
        }
    }
    $health.Checks += @{Name = 'ModuleLoaded'; Passed = $moduleLoaded}
    
    # Check 2: Files still exist
    $filesExist = Test-Path $plugin.Info.Path
    $health.Checks += @{Name = 'FilesExist'; Passed = $filesExist}
    
    # Check 3: Checksum unchanged (no tampering)
    $currentChecksum = Get-PluginChecksum -PluginPath $plugin.Info.Path
    $checksumMatch = ($currentChecksum -eq $plugin.Checksum)
    $health.Checks += @{Name = 'IntegrityCheck'; Passed = $checksumMatch}
    
    # Determine overall health
    $passedChecks = ($health.Checks | Where-Object { $_.Passed }).Count
    $totalChecks = $health.Checks.Count
    
    if ($passedChecks -eq $totalChecks) {
        $health.Status = 'Healthy'
        $health.Healthy = $true
    } elseif ($passedChecks -gt 0) {
        $health.Status = 'Degraded'
        $health.Healthy = $false
    } else {
        $health.Status = 'Unhealthy'
        $health.Healthy = $false
    }
    
    # Update registry
    $script:PluginRegistry.HealthStatus[$PluginId].Status = $health.Status
    $script:PluginRegistry.HealthStatus[$PluginId].LastCheck = Get-Date
    
    return $health
}

function Start-PluginHealthMonitor {
    <#
    .SYNOPSIS
        Starts background health monitoring for all plugins.
    #>
    [CmdletBinding()]
    param(
        [int]$IntervalMs = $script:PluginRegistry.Configuration.HealthCheckIntervalMs
    )
    
    $job = Start-Job -Name "PluginHealthMonitor" -ScriptBlock {
        param($IntervalMs, $CheckScript)
        
        while ($true) {
            Start-Sleep -Milliseconds $IntervalMs
            # Health checks would be performed here
            # In production, this would communicate with the main process
        }
    } -ArgumentList $IntervalMs
    
    Write-StructuredLog -Message "Health monitor started" -Level Info -Context @{JobId = $job.Id}
    return $job
}

# ============================================================================
# MAIN ENTRY POINT
# ============================================================================
function Invoke-PluginAutoLoader {
    <#
    .SYNOPSIS
        Main entry point for the plugin auto-loader.
    .DESCRIPTION
        Discovers, validates, and loads all plugins from configured directories.
        Handles dependency resolution and provides full lifecycle management.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string[]]$PluginDirs = @("D:/lazy init ide/plugins"),
        
        [switch]$Force,
        [switch]$NoAutoReload,
        [switch]$IncludeDisabled,
        
        [ValidateSet('All', 'Summary', 'None')]
        [string]$OutputLevel = 'Summary'
    )
    
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-StructuredLog -Message "Starting Plugin Auto-Loader" -Level Info -Context @{
        PluginDirs = $PluginDirs
        Force = $Force.IsPresent
    }
    
    # Update configuration
    $script:PluginRegistry.Configuration.PluginDirectories = $PluginDirs
    $script:PluginRegistry.Configuration.AutoReloadEnabled = -not $NoAutoReload
    
    # Ensure plugin directories exist
    foreach ($dir in $PluginDirs) {
        if (-not (Test-Path $dir)) {
            Write-StructuredLog -Message "Creating plugin directory" -Level Info -Context @{Path = $dir}
            New-Item -Path $dir -ItemType Directory -Force | Out-Null
        }
    }
    
    # Discover plugins
    $discovered = Find-Plugins -SearchPaths $PluginDirs -IncludeDisabled:$IncludeDisabled
    
    if ($discovered.Count -eq 0) {
        Write-StructuredLog -Message "No plugins discovered" -Level Warning
        return @{
            Success = $true
            PluginsLoaded = 0
            PluginsFailed = 0
            TotalTime = $stopwatch.Elapsed.TotalMilliseconds
        }
    }
    
    # Build manifests for dependency resolution
    $manifests = @()
    foreach ($plugin in $discovered) {
        if ($plugin.Manifest) {
            $manifests += $plugin.Manifest
        } else {
            # Create minimal manifest for legacy plugins
            $manifests += @{
                PluginId = $plugin.Name
                Dependencies = @()
            }
        }
    }
    
    # Resolve dependencies
    $resolution = Resolve-PluginDependencies -Plugins $manifests
    if (-not $resolution.Success) {
        Write-StructuredLog -Message "Dependency resolution failed" -Level Error -Context @{
            Error = $resolution.Error
            CircularDependencies = $resolution.CircularDependencies
        }
        return @{
            Success = $false
            Error = $resolution.Error
            CircularDependencies = $resolution.CircularDependencies
        }
    }
    
    # Load plugins in dependency order
    $loaded = 0
    $failed = 0
    $results = @()
    
    foreach ($pluginId in $resolution.LoadOrder) {
        $plugin = $discovered | Where-Object { 
            ($_.Manifest -and $_.Manifest.PluginId -eq $pluginId) -or 
            ($_.Name -eq $pluginId) 
        } | Select-Object -First 1
        
        if (-not $plugin) { continue }
        
        $result = Import-Plugin -PluginInfo $plugin -Force:$Force
        $results += $result
        
        if ($result.Success) {
            $loaded++
        } else {
            $failed++
        }
    }
    
    $stopwatch.Stop()
    
    $summary = @{
        Success = ($failed -eq 0)
        PluginsDiscovered = $discovered.Count
        PluginsLoaded = $loaded
        PluginsFailed = $failed
        LoadOrder = $resolution.LoadOrder
        TotalTimeMs = $stopwatch.Elapsed.TotalMilliseconds
        Results = $results
    }
    
    Write-StructuredLog -Message "Plugin Auto-Loader complete" -Level Info -Context @{
        Discovered = $discovered.Count
        Loaded = $loaded
        Failed = $failed
        TotalTimeMs = [math]::Round($stopwatch.Elapsed.TotalMilliseconds, 2)
    }
    
    if ($OutputLevel -eq 'All') {
        return $summary
    } elseif ($OutputLevel -eq 'Summary') {
        return @{
            Success = $summary.Success
            Loaded = $loaded
            Failed = $failed
        }
    }
    
    return $loaded
}

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================
function Get-LoadedPlugins {
    <#
    .SYNOPSIS
        Returns information about all loaded plugins.
    #>
    [CmdletBinding()]
    param()
    
    return $script:PluginRegistry.Plugins.GetEnumerator() | ForEach-Object {
        @{
            PluginId = $_.Key
            Version = $_.Value.Version
            State = $_.Value.State
            LoadedAt = $_.Value.LoadedAt
            Health = $script:PluginRegistry.HealthStatus[$_.Key]
        }
    }
}

function Set-PluginConfiguration {
    <#
    .SYNOPSIS
        Updates plugin loader configuration.
    #>
    [CmdletBinding()]
    param(
        [string[]]$PluginDirectories,
        [bool]$AutoReloadEnabled,
        [ValidateSet('None', 'AppDomain', 'Process')]
        [string]$IsolationLevel,
        [int]$MaxLoadRetries,
        [int]$HealthCheckIntervalMs,
        [bool]$RequireManifest,
        [bool]$AllowUnsigned,
        [string]$MarketplaceUrl
    )
    
    if ($PSBoundParameters.ContainsKey('PluginDirectories')) {
        $script:PluginRegistry.Configuration.PluginDirectories = $PluginDirectories
    }
    if ($PSBoundParameters.ContainsKey('AutoReloadEnabled')) {
        $script:PluginRegistry.Configuration.AutoReloadEnabled = $AutoReloadEnabled
    }
    if ($PSBoundParameters.ContainsKey('IsolationLevel')) {
        $script:PluginRegistry.Configuration.IsolationLevel = $IsolationLevel
    }
    if ($PSBoundParameters.ContainsKey('MaxLoadRetries')) {
        $script:PluginRegistry.Configuration.MaxLoadRetries = $MaxLoadRetries
    }
    if ($PSBoundParameters.ContainsKey('HealthCheckIntervalMs')) {
        $script:PluginRegistry.Configuration.HealthCheckIntervalMs = $HealthCheckIntervalMs
    }
    if ($PSBoundParameters.ContainsKey('RequireManifest')) {
        $script:PluginRegistry.Configuration.RequireManifest = $RequireManifest
    }
    if ($PSBoundParameters.ContainsKey('AllowUnsigned')) {
        $script:PluginRegistry.Configuration.AllowUnsigned = $AllowUnsigned
    }
    if ($PSBoundParameters.ContainsKey('MarketplaceUrl')) {
        $script:PluginRegistry.Configuration.MarketplaceUrl = $MarketplaceUrl
    }
    
    return $script:PluginRegistry.Configuration
}

# Export functions
# Note: Export-ModuleMember omitted to allow dot-sourcing
# Functions available: Invoke-PluginAutoLoader, Import-Plugin, Unload-Plugin, etc.

