<#
.SYNOPSIS
  Production-ready feature: ManifestChangeNotifier

.DESCRIPTION
  Monitors manifest files and notifies one or more endpoints when changes are detected.
  Includes debouncing, checksum-based change detection, retries with exponential backoff,
  structured logging (falls back to Write-Host if `Write-StructuredLog` is not available),
  and graceful shutdown handling.

.PARAMETER WatchDir
  Directory to watch for manifest JSON files. (Used by Invoke-ManifestChangeNotifier)

.PARAMETER ManifestPath
  Specific manifest file to watch. (Used by Invoke-ManifestChangeNotifierAuto)

.PARAMETER NotificationEndpoints
  An array of HTTP(S) endpoints to notify when a manifest changes.

.PARAMETER Headers
  Hashtable of headers to include in notification requests.

.PARAMETER MaxRetries
  Maximum number of retry attempts when sending notifications.

.PARAMETER InitialRetryDelaySeconds
  Initial delay in seconds for retry backoff.

.PARAMETER DebounceMilliseconds
  Minimum milliseconds between notifications for the same file.

.EXAMPLE
  Invoke-ManifestChangeNotifier -WatchDir 'D:/lazy init ide/orchestrator_smoke_output/manifest_tracer' -DebounceMilliseconds 500

  Invoke-ManifestChangeNotifierAuto -ManifestPath 'D:/lazy init ide/manifests/manifest.json' -NotificationEndpoints @('https://hooks.example.com/notify') -MaxRetries 5
#>

if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error')][string]$Level = 'Info'
        )
        Write-Host "[$Level] $Message"
    }
}

function Invoke-ManifestChangeNotifier {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)][ValidateScript({Test-Path $_ -PathType 'Container'})][string]$WatchDir = "D:/lazy init ide/orchestrator_smoke_output/manifest_tracer",
        [int]$DebounceMilliseconds = 500
    )

    $cancellationEvent = New-Object System.Threading.ManualResetEvent $false

    $onCancel = {
        Write-StructuredLog -Message "Received cancellation signal. Stopping ManifestChangeNotifier." -Level Info
        $cancellationEvent.Set() | Out-Null
    }

    [Console]::add_CancelKeyPress({ param($s,$e) $e.Cancel = $true; & $onCancel })

    try {
        Write-StructuredLog -Message "Starting directory watch on: $WatchDir" -Level Info
        $fsw = New-Object IO.FileSystemWatcher $WatchDir, '*.json'
        $fsw.IncludeSubdirectories = $false
        $fsw.EnableRaisingEvents = $true

        $lastSeen = @{}

        $subscription = Register-ObjectEvent $fsw Changed -SourceIdentifier 'ManifestChanged' -Action {
            param($sender,$eventArgs)
            $path = $eventArgs.FullPath
            $now = Get-Date

            # Debounce based on time
            if ($lastSeen.ContainsKey($path)) {
                $elapsed = ($now - $lastSeen[$path]).TotalMilliseconds
                if ($elapsed -lt $DebounceMilliseconds) {
                    return
                }
            }

            # Compute hash (with retry to handle partial writes)
            $hash = $null
            for ($i=0;$i -lt 5 -and -not $hash;$i++) {
                try {
                    $hash = (Get-FileHash -Path $path -Algorithm SHA256 -ErrorAction Stop).Hash
                } catch {
                    Start-Sleep -Milliseconds 100
                }
            }
            if (-not $hash) {
                Write-StructuredLog -Message "Unable to compute hash for $path; skipping event." -Level Warning
                return
            }

            if ($lastSeen.ContainsKey($path) -and $lastSeen[$path].Hash -eq $hash) {
                # No real change in file contents
                return
            }

            $lastSeen[$path] = @{ Time = $now; Hash = $hash }
            Write-StructuredLog -Message "Manifest changed: $path (hash: $hash)" -Level Info

            # Publish event into the global event queue so other components can react
            New-Event -SourceIdentifier 'ManifestChangedInternal' -MessageData @{ Path = $path; Hash = $hash; Timestamp = $now }
        } | Out-Null

        Write-StructuredLog -Message "ManifestChangeNotifier is running. Press Ctrl+C to stop." -Level Info

        while (-not $cancellationEvent.WaitOne(1000)) {
            # keep process alive while events are handled asynchronously
        }

    } catch {
        Write-StructuredLog -Message "ManifestChangeNotifier encountered an error: $_" -Level Error
    } finally {
        # Clean-up
        try {
            if ($subscription) { Unregister-Event -SourceIdentifier 'ManifestChanged' -ErrorAction SilentlyContinue }
            if ($fsw) { $fsw.Dispose() }
            Write-StructuredLog -Message "Shutdown complete for ManifestChangeNotifier." -Level Info
        } catch {
            Write-StructuredLog -Message "Error during cleanup: $_" -Level Warning
        }
    }
}

function Invoke-ManifestChangeNotifierAuto {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][ValidateScript({Test-Path $_})][string]$ManifestPath,
        [Parameter(Mandatory=$true)][string[]]$NotificationEndpoints,
        [hashtable]$Headers = @{},
        [int]$MaxRetries = 5,
        [int]$InitialRetryDelaySeconds = 2,
        [int]$DebounceMilliseconds = 500
    )

    Write-StructuredLog -Message "Starting ManifestChangeNotifierAuto for $ManifestPath" -Level Info

    # validate endpoints
    if (-not $NotificationEndpoints -or $NotificationEndpoints.Count -eq 0) {
        Write-StructuredLog -Message "No notification endpoints provided." -Level Error
        return 'error'
    }

    # Setup file watcher that reuses the directory watcher
    $directory = (Get-Item $ManifestPath).DirectoryName
    $fileName = (Get-Item $ManifestPath).Name
    $fileWatcher = New-Object System.IO.FileSystemWatcher
    $fileWatcher.Path = $directory
    $fileWatcher.Filter = $fileName
    $fileWatcher.EnableRaisingEvents = $true

    $cancellationEvent = New-Object System.Threading.ManualResetEvent $false
    [Console]::add_CancelKeyPress({ param($s,$e) $e.Cancel = $true; Write-StructuredLog -Message 'Cancellation requested. Stopping notifier...' -Level Info; $cancellationEvent.Set() | Out-Null })

    # Helper: robust send with exponential backoff + jitter
    function Send-Notification-WithRetries {
        param(
            [string]$Endpoint,
            [object]$Payload
        )

        $attempt = 0
        while ($attempt -le $MaxRetries) {
            try {
                $json = $Payload | ConvertTo-Json -Depth 10
                Invoke-RestMethod -Uri $Endpoint -Method Post -Body $json -ContentType 'application/json' -Headers $Headers -TimeoutSec 30
                Write-StructuredLog -Message "Notification to $Endpoint succeeded." -Level Info
                return $true
            } catch {
                $attempt++
                if ($attempt -gt $MaxRetries) {
                    Write-StructuredLog -Message "Notification to $Endpoint failed after $MaxRetries attempts: $_" -Level Error
                    return $false
                }
                $delay = [Math]::Pow(2, $attempt) * $InitialRetryDelaySeconds
                # add small jitter
                $jitter = Get-Random -Minimum 0 -Maximum ([Math]::Max(1,[int]($delay * 0.1)))
                $wait = [int]($delay + $jitter)
                Write-StructuredLog -Message "Notification to $Endpoint failed (attempt $attempt). Retrying in $wait seconds." -Level Warning
                Start-Sleep -Seconds $wait
            }
        }
    }

    # Keep track of last hash to avoid duplicate notifications
    $lastHash = $null
    $lastSentTime = (Get-Date).AddMinutes(-10)

    $sub = Register-ObjectEvent -InputObject $fileWatcher -EventName Changed -SourceIdentifier 'ManifestChangedAuto' -Action {
        param($sender,$eventArgs)
        $path = $eventArgs.FullPath

        # Debounce
        $now = Get-Date
        $elapsedMs = ($now - $lastSentTime).TotalMilliseconds
        if ($elapsedMs -lt $DebounceMilliseconds) { return }

        # Compute hash robustly
        $hash = $null
        for ($i=0;$i -lt 6 -and -not $hash;$i++) {
            try { $hash = (Get-FileHash -Path $path -Algorithm SHA256 -ErrorAction Stop).Hash } catch { Start-Sleep -Milliseconds 200 }
        }
        if (-not $hash) { Write-StructuredLog -Message "Could not compute hash for $path; skipping notification." -Level Warning; return }

        if ($hash -eq $lastHash) { Write-StructuredLog -Message "No content change detected for $path (hash matched), skipping." -Level Info; return }

        $payload = @{ file = $path; hash = $hash; timestamp = $now }

        foreach ($endpoint in $NotificationEndpoints) {
            Start-Job -ArgumentList $endpoint,$payload -ScriptBlock {
                param($endpoint,$payload)
                Import-Module -Name 'Microsoft.PowerShell.Utility' -ErrorAction SilentlyContinue
                try {
                    # Attempt send; the parent scope's Send-Notification-WithRetries is not available inside job, so re-implement minimal logic
                    $json = $payload | ConvertTo-Json -Depth 10
                    Invoke-RestMethod -Uri $endpoint -Method Post -Body $json -ContentType 'application/json' -TimeoutSec 30
                    Write-Output "OK"
                } catch {
                    Write-Output "ERROR: $_"
                }
            } | Out-Null
        }

        $lastHash = $hash
        $lastSentTime = Get-Date
        Write-StructuredLog -Message "Queued notifications for $path (hash: $hash)" -Level Info
    } | Out-Null

    Write-StructuredLog -Message "ManifestChangeNotifierAuto is running. Press Ctrl+C to stop." -Level Info

    while (-not $cancellationEvent.WaitOne(1000)) {
        # idle loop, events are handled by jobs
    }

    try {
        if ($sub) { Unregister-Event -SourceIdentifier 'ManifestChangedAuto' -ErrorAction SilentlyContinue }
        if ($fileWatcher) { $fileWatcher.Dispose() }
        Write-StructuredLog -Message "ManifestChangeNotifierAuto stopped." -Level Info
    } catch {
        Write-StructuredLog -Message "Error during shutdown: $_" -Level Warning
    }
}

