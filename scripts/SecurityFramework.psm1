# Security Framework for IDE Modules
# Provides safe alternatives to dangerous PowerShell patterns

param()

# ============================================================================
# CONFIGURATION
# ============================================================================

$script:SecurityLogPath = Join-Path $env:TEMP "IDE_Security.log"
$script:ExecutionPolicy = Get-ExecutionPolicy
$script:AllowedExecutionPolicies = @('RemoteSigned', 'Unrestricted', 'Bypass')
$script:ModuleCache = @{}
$script:FileCache = @{}

# ============================================================================
# SECURITY VALIDATION
# ============================================================================

function Test-ExecutionPolicy {
    <#
    .SYNOPSIS
    Validates current execution policy is safe for module operations
    #>
    [CmdletBinding()]
    param()
    
    $currentPolicy = Get-ExecutionPolicy
    if ($currentPolicy -eq 'Restricted') {
        throw "Execution policy is Restricted. Cannot perform module operations. Current policy: $currentPolicy"
    }
    
    Write-Log -Level Info -Message "Execution policy validated: $currentPolicy"
    return $true
}

function Test-ModuleSecurity {
    <#
    .SYNOPSIS
    Scans module file for dangerous patterns before loading
    .PARAMETER ModulePath
    Path to the module file to scan
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$ModulePath
    )
    
    if (-not (Test-Path $ModulePath)) {
        throw "Module path not found: $ModulePath"
    }
    
    $dangerousPatterns = @(
        @{ Pattern = 'Invoke-Expression'; Severity = 'Critical'; Description = 'Arbitrary code execution' },
        @{ Pattern = 'iex\s'; Severity = 'Critical'; Description = 'Alias for Invoke-Expression' },
        @{ Pattern = '\.\s+\$'; Severity = 'Critical'; Description = 'Dynamic dot-sourcing' },
        @{ Pattern = 'Set-ExecutionPolicy\s+.*-Force'; Severity = 'High'; Description = 'Forces execution policy change' },
        @{ Pattern = 'Start-Process.*-Wait'; Severity = 'Medium'; Description = 'Process execution without validation' },
        @{ Pattern = 'Remove-Item.*-Recurse.*-Force'; Severity = 'High'; Description = 'Recursive deletion without confirmation' }
    )
    
    $content = Get-Content $ModulePath -Raw
    $violations = @()
    
    foreach ($item in $dangerousPatterns) {
        if ($content -match $item.Pattern) {
            $violations += [PSCustomObject]@{
                Pattern = $item.Pattern
                Severity = $item.Severity
                Description = $item.Description
                LineNumber = ($content -split "\n" | Select-String -Pattern $item.Pattern).LineNumber
            }
        }
    }
    
    if ($violations.Count -gt 0) {
        $message = "Security violations found in ${ModulePath}:`n"
        $violations | ForEach-Object {
            $message += "  [$($_.Severity)] $($_.Description) (Pattern: $($_.Pattern)) at line $($_.LineNumber)`n"
        }
        Write-Log -Level Error -Message $message
        throw $message
    }
    
    Write-Log -Level Info -Message "Module security scan passed: ${ModulePath}"
    return $true
}

function Test-InputValidation {
    <#
    .SYNOPSIS
    Validates user input for dangerous characters and patterns
    .PARAMETER Input
    The input string to validate
    .PARAMETER AllowWildcards
    Whether to allow wildcard characters
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Input,
        [switch]$AllowWildcards
    )
    
    $dangerousChars = @('<', '>', '|', '`0', '`a', '`b', '`f', '`n', '`r', '`t', '`v')
    if (-not $AllowWildcards) {
        $dangerousChars += '*', '?', '[', ']'
    }
    
    foreach ($char in $dangerousChars) {
        if ($Input.Contains($char)) {
            throw "Invalid input: Contains dangerous character '$char'"
        }
    }
    
    # Check for path traversal attempts
    if ($Input -match '\.\.[\\/]') {
        throw "Invalid input: Path traversal attempt detected"
    }
    
    # Check for command injection patterns
    $injectionPatterns = @(';', '&', '|', '`$', '$(', ')', '`"', "`'")
    foreach ($pattern in $injectionPatterns) {
        if ($Input.Contains($pattern)) {
            throw "Invalid input: Potential command injection pattern detected"
        }
    }
    
    return $true
}

# ============================================================================
# SAFE EXECUTION ALTERNATIVES
# ============================================================================

function Invoke-SafeModule {
    <#
    .SYNOPSIS
    Safely executes a module function in a sandboxed environment
    .PARAMETER ModuleName
    Name of the module containing the function
    .PARAMETER FunctionName
    Name of the function to execute
    .PARAMETER Arguments
    Hashtable of arguments to pass to the function
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$ModuleName,
        [Parameter(Mandatory = $true)]
        [string]$FunctionName,
        [hashtable]$Arguments = @{}
    )
    
    # Validate execution policy
    Test-ExecutionPolicy
    
    # Validate module security
    $modulePath = Get-Module -ListAvailable -Name $ModuleName | Select-Object -First 1 -ExpandProperty ModuleBase
    if ($modulePath) {
        $moduleFile = Join-Path $modulePath "$ModuleName.psm1"
        if (Test-Path $moduleFile) {
            Test-ModuleSecurity -ModulePath $moduleFile
        }
    }
    
    # Create isolated PowerShell instance
    $ps = [PowerShell]::Create()
    
    try {
        # Import module in isolated environment
        $ps.AddCommand("Import-Module").AddParameter("Name", $ModuleName).AddParameter("DisableNameChecking").Invoke()
        
        if ($ps.Streams.Error.Count -gt 0) {
            throw "Failed to import module: $($ps.Streams.Error[0].Exception.Message)"
        }
        
        # Execute function
        $ps.Commands.Clear()
        $ps.AddCommand($FunctionName)
        
        foreach ($param in $Arguments.Keys) {
            $ps.AddParameter($param, $Arguments[$param])
        }
        
        $result = $ps.Invoke()
        
        if ($ps.Streams.Error.Count -gt 0) {
            throw "Function execution failed: $($ps.Streams.Error[0].Exception.Message)"
        }
        
        return $result
    }
    finally {
        $ps.Dispose()
    }
}

function Import-SafeModule {
    <#
    .SYNOPSIS
    Safely imports a module with conflict detection and validation
    .PARAMETER Name
    Name of the module to import
    .PARAMETER Force
    Whether to force import (requires confirmation)
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [switch]$Force
    )
    
    # Validate execution policy
    Test-ExecutionPolicy
    
    # Check for existing commands that would be overwritten
    $moduleInfo = Get-Module -ListAvailable -Name $Name | Select-Object -First 1
    if ($moduleInfo) {
        $exportedCommands = $moduleInfo.ExportedCommands.Keys
        $existingCommands = Get-Command | Where-Object { $exportedCommands -contains $_.Name }
        
        if ($existingCommands.Count -gt 0 -and -not $Force) {
            $message = "The following commands would be overwritten: $($existingCommands.Name -join ', ')"
            throw $message
        }
        
        if ($Force) {
            $confirmation = Read-Host "Force import will overwrite existing commands. Continue? (y/N)"
            if ($confirmation -ne 'y') {
                throw "Import cancelled by user"
            }
        }
    }
    
    # Validate module security
    $modulePath = $moduleInfo.ModuleBase
    $moduleFile = Join-Path $modulePath "$Name.psm1"
    if (Test-Path $moduleFile) {
        Test-ModuleSecurity -ModulePath $moduleFile
    }
    
    # Import module
    Import-Module -Name $Name -Global -DisableNameChecking
    
    Write-Log -Level Info -Message "Module imported safely: $Name"
}

function Invoke-SafeCommand {
    <#
    .SYNOPSIS
    Safely executes a shell command with input validation
    .PARAMETER Command
    The command to execute
    .PARAMETER ValidateInput
    Whether to validate command parameters
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [switch]$ValidateInput
    )
    
    if ($ValidateInput) {
        Test-InputValidation -Input $Command
    }
    
    # Use Start-Process for better control
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "pwsh"
    $psi.Arguments = "-NoProfile -Command `"$Command`""
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    
    try {
        $process.Start() | Out-Null
        $stdout = $process.StandardOutput.ReadToEnd()
        $stderr = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        
        if ($process.ExitCode -ne 0) {
            throw "Command failed with exit code $($process.ExitCode): $stderr"
        }
        
        return $stdout
    }
    finally {
        $process.Dispose()
    }
}

# ============================================================================
# CACHING AND PERFORMANCE
# ============================================================================

function Get-CachedFileContent {
    <#
    .SYNOPSIS
    Gets file content with caching to reduce I/O operations
    .PARAMETER Path
    Path to the file
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )
    
    if (-not (Test-Path $Path)) {
        throw "File not found: $Path"
    }
    
    $fileInfo = Get-Item $Path
    $cacheKey = "$Path|$($fileInfo.LastWriteTime.Ticks)"
    
    if (-not $script:FileCache.ContainsKey($cacheKey)) {
        $script:FileCache[$cacheKey] = Get-Content $Path -Raw
        
        # Clean old cache entries (keep last 100)
        if ($script:FileCache.Count -gt 100) {
            $keysToRemove = $script:FileCache.Keys | Select-Object -First ($script:FileCache.Count - 100)
            foreach ($key in $keysToRemove) {
                $script:FileCache.Remove($key)
            }
        }
    }
    
    return $script:FileCache[$cacheKey]
}

function Clear-FileCache {
    <#
    .SYNOPSIS
    Clears the file content cache
    #>
    [CmdletBinding()]
    param()
    
    $script:FileCache.Clear()
    Write-Log -Level Info -Message "File cache cleared"
}

# ============================================================================
# LOGGING AND AUDITING
# ============================================================================

function Write-Log {
    <#
    .SYNOPSIS
    Writes security events to log file
    .PARAMETER Level
    Log level: Info, Warning, Error, Critical
    .PARAMETER Message
    The message to log
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet('Info', 'Warning', 'Error', 'Critical')]
        [string]$Level,
        [Parameter(Mandatory = $true)]
        [string]$Message
    )
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logEntry = "[$timestamp] [$Level] $Message"
    
    try {
        Add-Content -Path $script:SecurityLogPath -Value $logEntry -ErrorAction Stop
    }
    catch {
        # If logging fails, write to event log as fallback
        Write-EventLog -LogName Application -Source "IDE Security Framework" -EventId 1001 -EntryType Warning -Message "Failed to write to security log: $_"
    }
    
    # Also write to console for critical errors
    if ($Level -in @('Error', 'Critical')) {
        Write-Host $logEntry -ForegroundColor Red
    }
}

function Get-SecurityLog {
    <#
    .SYNOPSIS
    Retrieves security log entries
    .PARAMETER LastHours
    Number of hours to look back
    #>
    [CmdletBinding()]
    param(
        [int]$LastHours = 24
    )
    
    if (-not (Test-Path $script:SecurityLogPath)) {
        return @()
    }
    
    $cutoffTime = (Get-Date).AddHours(-$LastHours)
    
    return Get-Content $script:SecurityLogPath | Where-Object {
        $_ -match '\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\]' 
    } | Where-Object {
        $logTime = [DateTime]::ParseExact($matches[1], "yyyy-MM-dd HH:mm:ss", $null)
        $logTime -gt $cutoffTime
    }
}

# ============================================================================
# BACKUP AND RECOVERY
# ============================================================================

function New-SafeBackup {
    <#
    .SYNOPSIS
    Creates a safe backup of a file before modification
    .PARAMETER Path
    Path to the file to backup
    .PARAMETER BackupDirectory
    Directory to store backups (defaults to same directory)
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [string]$BackupDirectory
    )
    
    if (-not (Test-Path $Path)) {
        throw "File not found: $Path"
    }
    
    if (-not $BackupDirectory) {
        $BackupDirectory = Split-Path $Path -Parent
    }
    
    $fileName = [System.IO.Path]::GetFileName($Path)
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $backupName = "$fileName.backup.$timestamp"
    $backupPath = Join-Path $BackupDirectory $backupName
    
    try {
        Copy-Item -Path $Path -Destination $backupPath -ErrorAction Stop
        Write-Log -Level Info -Message "Backup created: $backupPath"
        return $backupPath
    }
    catch {
        Write-Log -Level Error -Message "Backup failed: $_"
        throw
    }
}

function Restore-FromBackup {
    <#
    .SYNOPSIS
    Restores a file from its most recent backup
    .PARAMETER Path
    Path to the original file
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )
    
    $directory = Split-Path $Path -Parent
    $fileName = [System.IO.Path]::GetFileName($Path)
    
    $backups = Get-ChildItem -Path $directory -Filter "$fileName.backup.*" | Sort-Object LastWriteTime -Descending
    
    if ($backups.Count -eq 0) {
        throw "No backup found for $Path"
    }
    
    $latestBackup = $backups[0]
    
    try {
        Copy-Item -Path $latestBackup.FullName -Destination $Path -Force -ErrorAction Stop
        Write-Log -Level Info -Message "Restored from backup: $($latestBackup.FullName)"
        return $Path
    }
    catch {
        Write-Log -Level Error -Message "Restore failed: $_"
        throw
    }
}

# ============================================================================
# MODULE LIFECYCLE MANAGEMENT
# ============================================================================

class ModuleLifecycleManager {
    [string]$ModulesPath
    [hashtable]$LoadedModules
    
    ModuleLifecycleManager([string]$modulesPath) {
        $this.ModulesPath = $modulesPath
        $this.LoadedModules = @{}
    }
    
    [void] InstallModule([string]$moduleName, [string]$sourcePath) {
        $destination = Join-Path $this.ModulesPath "$moduleName.psm1"
        
        # Validate source module security
        Test-ModuleSecurity -ModulePath $sourcePath
        
        # Create backup if module exists
        if (Test-Path $destination) {
            New-SafeBackup -Path $destination
        }
        
        # Copy module
        Copy-Item -Path $sourcePath -Destination $destination -Force
        
        Write-Log -Level Info -Message "Module installed: $moduleName"
    }
    
    [void] UninstallModule([string]$moduleName) {
        $modulePath = Join-Path $this.ModulesPath "$moduleName.psm1"
        
        if (-not (Test-Path $modulePath)) {
            throw "Module not found: $moduleName"
        }
        
        # Create backup before uninstall
        New-SafeBackup -Path $modulePath
        
        # Remove module
        Remove-Item -Path $modulePath -Force
        
        # Remove from loaded modules if present
        if ($this.LoadedModules.ContainsKey($moduleName)) {
            $this.LoadedModules.Remove($moduleName)
        }
        
        Write-Log -Level Info -Message "Module uninstalled: $moduleName"
    }
    
    [void] EnableModule([string]$moduleName) {
        $modulePath = Join-Path $this.ModulesPath "$moduleName.psm1"
        
        if (-not (Test-Path $modulePath)) {
            throw "Module not found: $moduleName"
        }
        
        # Validate security before loading
        Test-ModuleSecurity -ModulePath $modulePath
        
        # Load module
        Import-Module -Path $modulePath -Global -DisableNameChecking
        $this.LoadedModules[$moduleName] = $true
        
        Write-Log -Level Info -Message "Module enabled: $moduleName"
    }
    
    [void] DisableModule([string]$moduleName) {
        if ($this.LoadedModules.ContainsKey($moduleName)) {
            Remove-Module -Name $moduleName -Force -ErrorAction SilentlyContinue
            $this.LoadedModules.Remove($moduleName)
            Write-Log -Level Info -Message "Module disabled: $moduleName"
        }
    }
    
    [bool] IsModuleLoaded([string]$moduleName) {
        return $this.LoadedModules.ContainsKey($moduleName)
    }
    
    [string[]] GetInstalledModules() {
        $modules = Get-ChildItem -Path $this.ModulesPath -Filter "*.psm1" | ForEach-Object {
            $_.BaseName
        }
        return $modules
    }
}

# ============================================================================
# EXPORTS
# ============================================================================

Export-ModuleMember -Function @(
    'Test-ExecutionPolicy',
    'Test-ModuleSecurity',
    'Test-InputValidation',
    'Invoke-SafeModule',
    'Import-SafeModule',
    'Invoke-SafeCommand',
    'Get-CachedFileContent',
    'Clear-FileCache',
    'Write-Log',
    'Get-SecurityLog',
    'New-SafeBackup',
    'Restore-FromBackup'
)

Export-ModuleMember -Variable @('SecurityLogPath')
