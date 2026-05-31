# agent_tool_bridge.ps1
# PowerShell dispatch bridge for /agent/tool endpoint.
# Called by server.js via child_process.exec.
# Outputs a single JSON object to stdout; never writes non-JSON to stdout.

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet(
        'Start-SelfAnalysis',
        'Start-AutonomousTesting',
        'Start-AutomaticFeatureGeneration',
        'Get-AutonomousAgentStatus',
        'Initialize-AutonomousAgentState'
    )]
    [string]$Tool,

    [Parameter(Mandatory = $false)]
    [string]$ArgsJson = '{}'
)

# --- redirect verbose/warning channels away from stdout so JSON stays clean ---
$VerbosePreference   = 'SilentlyContinue'
$WarningPreference   = 'SilentlyContinue'
$InformationPreference = 'SilentlyContinue'
$ProgressPreference  = 'SilentlyContinue'
$ErrorActionPreference = 'Stop'

function Write-Result {
    param([hashtable]$obj)
    Write-Output ($obj | ConvertTo-Json -Depth 10 -Compress)
}

$modulePath = Join-Path $PSScriptRoot 'RawrXD.AutonomousAgent.psm1'
if (-not (Test-Path $modulePath)) {
    Write-Result @{ ok = $false; tool = $Tool; error = "Module not found: $modulePath" }
    exit 1
}

# Load module — strip #Requires so non-admin sessions can use analysis/testing tools.
# The allowlist in server.js already enforces which operations are permitted.
try {
    $content = Get-Content $modulePath -Raw -ErrorAction Stop
    $content = $content -replace '#Requires\s+-Version\s+\S+', ''
    $content = $content -replace '#Requires\s+-RunAsAdministrator', ''
    $content = [regex]::Replace($content, '(?s)Export-ModuleMember\s*-Function\s*@\(.*?\)', '')

    # Strict stdout contract: suppress Write-Output (stream 1) and Write-Host (stream 6)
    # during module load so only our explicit Write-Result call ever reaches stdout.
    # PS 7: stream 6 (information) is where Write-Host writes.  6>$null kills it.
    # Piping to Out-Null kills stream 1 (Write-Output) from module-level code.
    . ([scriptblock]::Create($content)) 6>$null | Out-Null
} catch {
    Write-Result @{ ok = $false; tool = $Tool; error = "Module load failed: $($_.Exception.Message)" }
    exit 1
}

# Override Write-AutonomousAgentLog: in a non-interactive subprocess Write-Host
# routes to stdout and contaminates the JSON output.  Redirect everything to
# stderr (captured separately by server.js) and still update agent state.
function Write-AutonomousAgentLog {
    param(
        [Parameter(Mandatory=$true)][string]$Message,
        [string]$Level = 'Info',
        [string]$Phase = 'AutonomousOperation',
        [hashtable]$Data = $null
    )
    [Console]::Error.WriteLine("[$( Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff')][$Phase][$Level] $Message")
    if ($null -ne $script:AutonomousAgentState) {
        $script:AutonomousAgentState.CurrentOperation = $Message
        if ($Level -in 'Error','Critical') { $script:AutonomousAgentState.Errors.Add($Message) }
        elseif ($Level -eq 'Warning')      { $script:AutonomousAgentState.Warnings.Add($Message) }
        $script:AutonomousAgentState.LearningHistory.Add(@{
            Timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
            Level     = $Level
            Message   = $Message
            Data      = $Data
        })
    }
}

# Override Write-StartupLog (Core module) — same reasoning as Write-AutonomousAgentLog above
function Write-StartupLog {
    param(
        [Parameter(Mandatory=$true)][string]$Message,
        [string]$Level = 'INFO'
    )
    [Console]::Error.WriteLine("[$( Get-Date -Format 'yyyy-MM-dd HH:mm:ss')][$Level] $Message")
}

# Get-AutonomousAgentStatus is not defined in the module; provide it as a bridge shim
function Get-AutonomousAgentStatus {
    if ($null -ne $script:AutonomousAgentState) { return $script:AutonomousAgentState }
    return @{ status = 'not-initialized' }
}

# Parse caller-supplied arguments (ValidateSet above prevents command injection; no eval)
$callArgs = @{}
try {
    if ($ArgsJson -and $ArgsJson -ne '{}') {
        $parsed = $ArgsJson | ConvertFrom-Json -ErrorAction Stop
        $parsed.PSObject.Properties | ForEach-Object { $callArgs[$_.Name] = $_.Value }
    }
} catch {
    Write-Result @{ ok = $false; tool = $Tool; error = "ArgsJson parse error: $($_.Exception.Message)" }
    exit 1
}

# Initialize agent state so path-dependent operations work
$sourcePath = $PSScriptRoot
try {
    # 6>$null: suppress Write-Host/Info banners from Init; 2>$null: suppress stderr noise
    Initialize-AutonomousAgentState `
        -SourcePath $sourcePath `
        -TargetPath (Join-Path $env:LOCALAPPDATA 'RawrXD\Autonomous') `
        -LogPath    (Join-Path $env:LOCALAPPDATA 'RawrXD\Logs') `
        -BackupPath (Join-Path $env:LOCALAPPDATA 'RawrXD\Backups') `
        -ErrorAction Stop 6>$null 2>$null | Out-Null
} catch {
    # Non-fatal — WinDeploy module load may fail under non-admin; continue
}

# Dispatch — wrap in scriptblock with 6>$null so that any Write-Host calls
# inside the tools (or modules they load) never reach stdout.  Stream 1 (the
# return value) flows out normally and is captured in $result.
$startTime = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
try {
    $result = & {
        switch ($Tool) {
            'Start-SelfAnalysis'              { Start-SelfAnalysis @callArgs }
            'Start-AutonomousTesting'         { Start-AutonomousTesting @callArgs }
            'Start-AutomaticFeatureGeneration' {
                $analysis = if ($callArgs.ContainsKey('AnalysisResults')) {
                    $callArgs['AnalysisResults']
                } else {
                    Start-SelfAnalysis
                }
                Start-AutomaticFeatureGeneration -AnalysisResults $analysis
            }
            'Get-AutonomousAgentStatus'       { Get-AutonomousAgentStatus }
            'Initialize-AutonomousAgentState' { Initialize-AutonomousAgentState @callArgs; @{ initialized = $true } }
        }
    } 6>$null

    $durationMs = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() - $startTime

    # Serialize result — convert PS-specific types to plain objects
    $resultJson = $result | ConvertTo-Json -Depth 8 -Compress -ErrorAction SilentlyContinue
    if (-not $resultJson) { $resultJson = '{}' }
    $resultObj  = $resultJson | ConvertFrom-Json

    Write-Result @{
        ok         = $true
        tool       = $Tool
        durationMs = $durationMs
        result     = $resultObj
    }
    exit 0

} catch {
    $durationMs = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() - $startTime
    Write-Result @{
        ok         = $false
        tool       = $Tool
        durationMs = $durationMs
        error      = $_.Exception.Message
        at         = "$($_.InvocationInfo.ScriptName):$($_.InvocationInfo.ScriptLineNumber)"
    }
    exit 1
}
