
# Production-ready feature: SelfHealingModule
function Invoke-SelfHealingModule {
    [CmdletBinding()]
    param(
        [string]$ModuleDir = $null,
        [switch]$Recurse,
        [switch]$NoInvoke,
        [int]$MaxRetries = 2
    )

    Begin {
        $results = @()
        $scriptRoot = Split-Path -Parent $PSScriptRoot
        if (-not $ModuleDir) { $ModuleDir = Join-Path $scriptRoot 'auto_generated_methods' }

        # Try to import shared helpers if present (non-fatal)
        $loggingModule = Join-Path $scriptRoot 'RawrXD.Logging.psm1'
        $configModule = Join-Path $scriptRoot 'RawrXD.Config.psm1'
        if (Test-Path $loggingModule) {
            try { Import-Module $loggingModule -Force -ErrorAction Stop } catch { }
        }
        if (Test-Path $configModule) {
            try { Import-Module $configModule -Force -ErrorAction Stop } catch { }
        }

        if (-not (Test-Path -Path $ModuleDir -PathType Container)) {
            $msg = "[SelfHealingModule] ModuleDir '$ModuleDir' not found."
            if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
                try { & Write-StructuredLog -Message $msg -Level 'Error' } catch { Write-Host $msg }
            } else { Write-Host $msg }
            throw $msg
        }

        $gciParams = @{ Path = $ModuleDir; Filter = '*_AutoFeature.ps1'; ErrorAction = 'SilentlyContinue' }
        if ($Recurse) { $gciParams['Recurse'] = $true }
        $moduleFiles = Get-ChildItem @gciParams
    }

    Process {
        foreach ($file in $moduleFiles) {
            $entry = [PSCustomObject]@{
                File       = $file.FullName
                Reloaded   = $false
                Invoked    = $false
                Error      = $null
                DurationMs = 0
            }

            # Dot-source the script (safe, measured)
            try {
                $dotDuration = Measure-Command { . $file.FullName }
                $entry.DurationMs += $dotDuration.TotalMilliseconds
                $msg = "[SelfHealingModule] Dot-sourced $($file.Name) in $([math]::Round($dotDuration.TotalMilliseconds,2))ms"
                if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) { try { & Write-StructuredLog -Message $msg -Level 'Debug' -Context @{ File = $file.Name } } catch { Write-Host $msg } } else { Write-Host $msg }
            } catch {
                $entry.Error = $_.Exception.Message
                $msg = "[SelfHealingModule][ERROR] Failed to dot-source $($file.Name): $($entry.Error)"
                if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) { try { & Write-StructuredLog -Message $msg -Level 'Error' -Context @{ File = $file.Name; Exception = $_.Exception.Message } } catch { Write-Host $msg } } else { Write-Error $msg }

                # Attempt reloads (only if the script exposes a module by the basename)
                $baseName = $file.BaseName -replace '_AutoFeature$',''
                $moduleLoaded = Get-Module -Name $baseName -ErrorAction SilentlyContinue
                if ($moduleLoaded) {
                    try { Remove-Module -Name $baseName -ErrorAction SilentlyContinue } catch { }
                }

                for ($i=0; $i -lt $MaxRetries; $i++) {
                    try {
                        $reloadDuration = Measure-Command { . $file.FullName }
                        $entry.DurationMs += $reloadDuration.TotalMilliseconds
                        $entry.Reloaded = $true
                        $entry.Error = $null
                        $msg = "[SelfHealingModule] Reloaded $($file.Name) attempt $($i+1) in $([math]::Round($reloadDuration.TotalMilliseconds,2))ms"
                        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) { try { & Write-StructuredLog -Message $msg -Level 'Info' -Context @{ File = $file.Name; Attempt = $i+1 } } catch { Write-Host $msg } } else { Write-Host $msg }
                        break
                    } catch {
                        $entry.Error = $_.Exception.Message
                    }
                }

                $results += $entry
                continue
            }

            # Resolve the expected function name and optionally invoke it
            $funcName = 'Invoke-' + ($file.BaseName -replace '_AutoFeature$','')
            $cmd = Get-Command -Name $funcName -ErrorAction SilentlyContinue
            if (-not $cmd) {
                $msg = "[SelfHealingModule] Function $funcName not found after dot-source."
                if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) { try { & Write-StructuredLog -Message $msg -Level 'Warning' -Context @{ File = $file.Name } } catch { Write-Host $msg } } else { Write-Host $msg }
                $entry.Error = 'FunctionNotFound'
                $results += $entry
                continue
            }

            if ($NoInvoke) {
                $results += $entry
                continue
            }

            try {
                $invokeDuration = Measure-Command { & $funcName -ErrorAction Stop }
                $entry.DurationMs += $invokeDuration.TotalMilliseconds
                $entry.Invoked = $true
                $msg = "[SelfHealingModule] Invoked $funcName in $([math]::Round($invokeDuration.TotalMilliseconds,2))ms"
                if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) { try { & Write-StructuredLog -Message $msg -Level 'Info' -Context @{ File = $file.Name; Function = $funcName } } catch { Write-Host $msg } } else { Write-Host $msg }
            } catch {
                $entry.Error = $_.Exception.Message
                $msg = "[SelfHealingModule][ERROR] Invocation failed for $($funcName): $($entry.Error). Attempting reload..."
                if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) { try { & Write-StructuredLog -Message $msg -Level 'Error' -Context @{ File = $file.Name; Function = $funcName } } catch { Write-Host $msg } } else { Write-Host $msg }

                # Attempt reload and retry invoke
                $baseName = $file.BaseName -replace '_AutoFeature$',''
                if (Get-Module -Name $baseName -ErrorAction SilentlyContinue) { try { Remove-Module -Name $baseName -ErrorAction SilentlyContinue } catch { } }

                for ($i=0; $i -lt $MaxRetries; $i++) {
                    try {
                        Measure-Command { . $file.FullName } | Out-Null
                        $entry.Reloaded = $true
                        $msg = "[SelfHealingModule] Reloaded $($file.Name) attempt $($i+1)"
                        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) { try { & Write-StructuredLog -Message $msg -Level 'Info' -Context @{ File = $file.Name; Attempt = $i+1 } } catch { Write-Host $msg } } else { Write-Host $msg }
                        try {
                            $retryDuration = Measure-Command { & $funcName -ErrorAction Stop }
                            $entry.DurationMs += $retryDuration.TotalMilliseconds
                            $entry.Invoked = $true
                            $entry.Error = $null
                            break
                        } catch {
                            $entry.Error = $_.Exception.Message
                        }
                    } catch {
                        $entry.Error = $_.Exception.Message
                    }
                }
            }

            $results += $entry
        }
    }

    End {
        return $results
    }
}

