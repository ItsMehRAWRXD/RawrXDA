# ============================================================================
# File: D:\lazy init ide\scripts\TODO-RollbackSystem.psm1
# Purpose: Complete undo/rollback for all TODO auto-fixes
# Version: 1.0.0 - Production Safety
# ============================================================================

#Requires -Version 7.0

$script:RollbackConfig = @{
    BackupRoot = "D:\lazy init ide\.backups"
    MaxBackups = 100
    AutoBackup = $true
    Compression = $true
    LogPath = "D:\lazy init ide\logs\rollback.log"
}

# Ensure directories exist
New-Item -ItemType Directory -Path $script:RollbackConfig.BackupRoot -Force -ErrorAction SilentlyContinue | Out-Null
New-Item -ItemType Directory -Path (Split-Path $script:RollbackConfig.LogPath) -Force -ErrorAction SilentlyContinue | Out-Null

#region Logging
function Write-RollbackLog {
    param(
        [string]$Message,
        [ValidateSet("INFO", "WARN", "ERROR", "DEBUG")]
        [string]$Level = "INFO"
    )
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    $logEntry = "[$timestamp] [$Level] $Message"
    
    Add-Content -Path $script:RollbackConfig.LogPath -Value $logEntry -ErrorAction SilentlyContinue
    
    $color = switch ($Level) {
        "ERROR" { "Red" }
        "WARN"  { "Yellow" }
        "DEBUG" { "Gray" }
        default { "White" }
    }
    
    if ($Level -ne "DEBUG" -or $VerbosePreference -eq 'Continue') {
        Write-Host $logEntry -ForegroundColor $color
    }
}
#endregion

#region Core Backup System
function New-TODORollbackPoint {
    <#
    .SYNOPSIS
        Creates a rollback point before making destructive changes.
    .DESCRIPTION
        Backs up specified files with compression and manifest tracking.
        Automatically cleans up old backups beyond MaxBackups limit.
    .PARAMETER OperationName
        Name of the operation (e.g., "BUG-AutoFix", "FIXME-Resolve")
    .PARAMETER TargetFiles
        Array of file paths to backup
    .PARAMETER Metadata
        Additional metadata to store in manifest
    .EXAMPLE
        $id = New-TODORollbackPoint -OperationName "BUG-Fix" -TargetFiles @("C:\src\file.ps1")
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$OperationName,
        
        [Parameter(Mandatory = $true)]
        [string[]]$TargetFiles,
        
        [Parameter(Mandatory = $false)]
        [hashtable]$Metadata = @{}
    )
    
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $backupId = "$OperationName-$timestamp"
    $backupPath = Join-Path $script:RollbackConfig.BackupRoot $backupId
    
    Write-RollbackLog "Creating rollback point: $backupId" -Level INFO
    
    New-Item -ItemType Directory -Path $backupPath -Force | Out-Null
    
    $manifest = @{
        BackupId      = $backupId
        OperationName = $OperationName
        Timestamp     = Get-Date -Format "o"
        PowerShellVersion = $PSVersionTable.PSVersion.ToString()
        TargetFiles   = @()
        Metadata      = $Metadata
        Stats         = @{
            TotalFiles = 0
            TotalBytes = 0
            CompressedBytes = 0
        }
    }
    
    foreach ($file in $TargetFiles) {
        if (Test-Path $file) {
            try {
                $relativePath = $file -replace [regex]::Escape("D:\lazy init ide\"), ''
                $safePath = $relativePath -replace '[\\:/<>"|?*]', '_'
                $backupFile = Join-Path $backupPath "$safePath.bak"
                
                # Ensure subdirectory exists
                $backupDir = Split-Path $backupFile -Parent
                if (-not (Test-Path $backupDir)) {
                    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
                }
                
                $content = Get-Content -Path $file -Raw -ErrorAction Stop
                $originalSize = $content.Length
                
                # Compress large files (>10KB)
                if ($script:RollbackConfig.Compression -and $originalSize -gt 10240) {
                    $bytes = [System.Text.Encoding]::UTF8.GetBytes($content)
                    $ms = [System.IO.MemoryStream]::new()
                    $gz = [System.IO.Compression.GZipStream]::new($ms, [System.IO.Compression.CompressionLevel]::Optimal)
                    $gz.Write($bytes, 0, $bytes.Length)
                    $gz.Close()
                    $compressedBytes = $ms.ToArray()
                    $ms.Close()
                    
                    [System.IO.File]::WriteAllBytes("$backupFile.gz", $compressedBytes)
                    
                    $manifest.TargetFiles += @{
                        Original     = $file
                        Backup       = "$backupFile.gz"
                        Compressed   = $true
                        OriginalSize = $originalSize
                        BackupSize   = $compressedBytes.Length
                        Hash         = (Get-FileHash -Path $file -Algorithm SHA256).Hash
                        Encoding     = "UTF8"
                    }
                    
                    $manifest.Stats.CompressedBytes += $compressedBytes.Length
                    Write-RollbackLog "  Backed up (compressed): $file ($originalSize -> $($compressedBytes.Length) bytes)" -Level DEBUG
                }
                else {
                    Copy-Item -Path $file -Destination $backupFile -Force
                    
                    $manifest.TargetFiles += @{
                        Original     = $file
                        Backup       = $backupFile
                        Compressed   = $false
                        OriginalSize = $originalSize
                        BackupSize   = $originalSize
                        Hash         = (Get-FileHash -Path $file -Algorithm SHA256).Hash
                        Encoding     = "UTF8"
                    }
                    
                    Write-RollbackLog "  Backed up: $file ($originalSize bytes)" -Level DEBUG
                }
                
                $manifest.Stats.TotalFiles++
                $manifest.Stats.TotalBytes += $originalSize
            }
            catch {
                Write-RollbackLog "  Failed to backup: $file - $_" -Level ERROR
            }
        }
        else {
            Write-RollbackLog "  File not found (skipped): $file" -Level WARN
        }
    }
    
    # Write manifest
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -Path (Join-Path $backupPath "manifest.json") -Encoding UTF8
    
    # Cleanup old backups
    $allBackups = Get-ChildItem -Path $script:RollbackConfig.BackupRoot -Directory | 
        Sort-Object CreationTime -Descending
    
    $toDelete = $allBackups | Select-Object -Skip $script:RollbackConfig.MaxBackups
    
    foreach ($old in $toDelete) {
        try {
            Remove-Item -Path $old.FullName -Recurse -Force
            Write-RollbackLog "Cleaned up old backup: $($old.Name)" -Level DEBUG
        }
        catch {
            Write-RollbackLog "Failed to cleanup: $($old.Name) - $_" -Level WARN
        }
    }
    
    Write-RollbackLog "Rollback point created: $backupId ($($manifest.Stats.TotalFiles) files, $($manifest.Stats.TotalBytes) bytes)" -Level INFO
    
    return $backupId
}

function Get-TODORollbackPoints {
    <#
    .SYNOPSIS
        Lists all available rollback points.
    .PARAMETER OperationName
        Filter by operation name prefix
    .PARAMETER MaxResults
        Maximum number of results to return
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $false)]
        [string]$OperationName,
        
        [Parameter(Mandatory = $false)]
        [int]$MaxResults = 50
    )
    
    $filter = if ($OperationName) { "$OperationName-*" } else { "*" }
    
    Get-ChildItem -Path $script:RollbackConfig.BackupRoot -Directory -Filter $filter | 
        Sort-Object CreationTime -Descending | 
        Select-Object -First $MaxResults |
        ForEach-Object {
            $manifestPath = Join-Path $_.FullName "manifest.json"
            if (Test-Path $manifestPath) {
                $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
                
                [PSCustomObject]@{
                    BackupId      = $manifest.BackupId
                    OperationName = $manifest.OperationName
                    Timestamp     = [datetime]::Parse($manifest.Timestamp)
                    FileCount     = $manifest.TargetFiles.Count
                    TotalBytes    = $manifest.Stats.TotalBytes
                    SizeMB        = [math]::Round($manifest.Stats.TotalBytes / 1MB, 2)
                    Path          = $_.FullName
                }
            }
        }
}

function Restore-TODORollbackPoint {
    <#
    .SYNOPSIS
        Restores files from a rollback point.
    .DESCRIPTION
        Restores all files from a backup, with optional verification and force modes.
    .PARAMETER BackupId
        The ID of the backup to restore
    .PARAMETER OperationName
        Restore the most recent backup for this operation
    .PARAMETER Force
        Override hash mismatch warnings
    .PARAMETER VerifyOnly
        Only verify what would be restored, don't make changes
    #>
    [CmdletBinding(SupportsShouldProcess = $true)]
    param(
        [Parameter(Mandatory = $true, ParameterSetName = "ById")]
        [string]$BackupId,
        
        [Parameter(Mandatory = $true, ParameterSetName = "ByOperation")]
        [string]$OperationName,
        
        [Parameter(Mandatory = $false)]
        [switch]$Force,
        
        [Parameter(Mandatory = $false)]
        [switch]$VerifyOnly
    )
    
    # Find backup
    if ($PSCmdlet.ParameterSetName -eq "ByOperation") {
        $backups = Get-TODORollbackPoints -OperationName $OperationName -MaxResults 1
        
        if (-not $backups) {
            throw "No backups found for operation: $OperationName"
        }
        $BackupId = $backups[0].BackupId
    }
    
    $backupPath = Join-Path $script:RollbackConfig.BackupRoot $BackupId
    $manifestPath = Join-Path $backupPath "manifest.json"
    
    if (-not (Test-Path $manifestPath)) {
        throw "Backup manifest not found: $manifestPath"
    }
    
    $manifest = Get-Content -Path $manifestPath -Raw | ConvertFrom-Json
    
    Write-RollbackLog "Restoring rollback point: $($manifest.BackupId)" -Level INFO
    Write-Host "`n╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  ROLLBACK RESTORE                                            ║" -ForegroundColor Cyan
    Write-Host "╠══════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
    Write-Host "║  Operation: $($manifest.OperationName.PadRight(47))║" -ForegroundColor Yellow
    $tsDisplay = if ($manifest.Timestamp -is [string]) { $manifest.Timestamp.Substring(0,19) } else { $manifest.Timestamp.ToString("yyyy-MM-dd HH:mm:ss") }
    Write-Host "║  Timestamp: $($tsDisplay.PadRight(47))║" -ForegroundColor Gray
    Write-Host "║  Files:     $($manifest.TargetFiles.Count.ToString().PadRight(47))║" -ForegroundColor Gray
    Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    
    $restored = 0
    $skipped = 0
    $failed = 0
    $modified = 0
    
    foreach ($fileEntry in $manifest.TargetFiles) {
        $originalPath = $fileEntry.Original
        $backupFile = $fileEntry.Backup
        
        # Check if backup file exists
        if (-not (Test-Path $backupFile)) {
            Write-RollbackLog "  Backup file missing: $backupFile" -Level ERROR
            $failed++
            continue
        }
        
        # Verify current file hasn't been modified since backup
        $currentModified = $false
        if (Test-Path $originalPath) {
            $currentHash = (Get-FileHash -Path $originalPath -Algorithm SHA256).Hash
            if ($currentHash -ne $fileEntry.Hash) {
                $currentModified = $true
                $modified++
                
                if (-not $Force -and -not $VerifyOnly) {
                    Write-Host "  ⚠️  Modified since backup: $originalPath" -ForegroundColor Yellow
                    $skipped++
                    continue
                }
            }
        }
        
        if ($VerifyOnly) {
            $status = if ($currentModified) { "[MODIFIED]" } else { "[OK]" }
            Write-Host "  $status Would restore: $originalPath" -ForegroundColor $(if($currentModified){"Yellow"}else{"Cyan"})
            $restored++
            continue
        }
        
        if ($PSCmdlet.ShouldProcess($originalPath, "Restore from backup")) {
            try {
                # Ensure target directory exists
                $targetDir = Split-Path $originalPath -Parent
                if (-not (Test-Path $targetDir)) {
                    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
                }
                
                # Decompress if needed
                if ($fileEntry.Compressed) {
                    $compressedBytes = [System.IO.File]::ReadAllBytes($backupFile)
                    $ms = [System.IO.MemoryStream]::new($compressedBytes)
                    $gz = [System.IO.Compression.GZipStream]::new($ms, [System.IO.Compression.CompressionMode]::Decompress)
                    $reader = [System.IO.StreamReader]::new($gz, [System.Text.Encoding]::UTF8)
                    $content = $reader.ReadToEnd()
                    $reader.Close()
                    $gz.Close()
                    $ms.Close()
                    
                    Set-Content -Path $originalPath -Value $content -NoNewline -Force -Encoding UTF8
                }
                else {
                    Copy-Item -Path $backupFile -Destination $originalPath -Force
                }
                
                # Verify restoration
                $restoredHash = (Get-FileHash -Path $originalPath -Algorithm SHA256).Hash
                if ($restoredHash -eq $fileEntry.Hash) {
                    Write-Host "  ✓ Restored: $originalPath" -ForegroundColor Green
                    $restored++
                }
                else {
                    Write-Host "  ✗ Hash mismatch after restore: $originalPath" -ForegroundColor Red
                    $failed++
                }
            }
            catch {
                Write-RollbackLog "  Failed to restore $originalPath : $_" -Level ERROR
                $failed++
            }
        }
    }
    
    Write-Host "`n[Rollback] Complete:" -ForegroundColor Cyan
    Write-Host "  ✓ Restored: $restored" -ForegroundColor Green
    Write-Host "  ⊘ Skipped:  $skipped" -ForegroundColor Yellow
    Write-Host "  ✗ Failed:   $failed" -ForegroundColor Red
    if ($modified -gt 0) {
        Write-Host "  ⚠ Modified: $modified (use -Force to override)" -ForegroundColor Yellow
    }
    
    Write-RollbackLog "Restore complete: $restored restored, $skipped skipped, $failed failed" -Level INFO
    
    return [PSCustomObject]@{
        BackupId = $BackupId
        Restored = $restored
        Skipped  = $skipped
        Failed   = $failed
        Modified = $modified
        Success  = ($failed -eq 0)
    }
}

function Remove-TODORollbackPoint {
    <#
    .SYNOPSIS
        Removes a specific rollback point or old rollbacks.
    #>
    [CmdletBinding(SupportsShouldProcess = $true)]
    param(
        [Parameter(Mandatory = $true, ParameterSetName = "ById")]
        [string]$BackupId,
        
        [Parameter(Mandatory = $true, ParameterSetName = "ByAge")]
        [int]$OlderThanDays
    )
    
    if ($PSCmdlet.ParameterSetName -eq "ById") {
        $backupPath = Join-Path $script:RollbackConfig.BackupRoot $BackupId
        if (Test-Path $backupPath) {
            if ($PSCmdlet.ShouldProcess($BackupId, "Remove rollback point")) {
                Remove-Item -Path $backupPath -Recurse -Force
                Write-RollbackLog "Removed rollback point: $BackupId" -Level INFO
            }
        }
        else {
            Write-Warning "Rollback point not found: $BackupId"
        }
    }
    else {
        $cutoff = (Get-Date).AddDays(-$OlderThanDays)
        $removed = 0
        
        Get-ChildItem -Path $script:RollbackConfig.BackupRoot -Directory | 
            Where-Object { $_.CreationTime -lt $cutoff } | 
            ForEach-Object {
                if ($PSCmdlet.ShouldProcess($_.Name, "Remove old rollback")) {
                    Remove-Item -Path $_.FullName -Recurse -Force
                    $removed++
                }
            }
        
        Write-RollbackLog "Removed $removed old rollback points (older than $OlderThanDays days)" -Level INFO
    }
}

function Export-TODORollbackPoint {
    <#
    .SYNOPSIS
        Exports a rollback point to a ZIP archive.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$BackupId,
        
        [Parameter(Mandatory = $false)]
        [string]$DestinationPath = "D:\lazy init ide\exports"
    )
    
    $backupPath = Join-Path $script:RollbackConfig.BackupRoot $BackupId
    
    if (-not (Test-Path $backupPath)) {
        throw "Rollback point not found: $BackupId"
    }
    
    New-Item -ItemType Directory -Path $DestinationPath -Force -ErrorAction SilentlyContinue | Out-Null
    
    $zipPath = Join-Path $DestinationPath "$BackupId.zip"
    
    Compress-Archive -Path $backupPath -DestinationPath $zipPath -Force
    
    Write-RollbackLog "Exported rollback to: $zipPath" -Level INFO
    Write-Host "Exported to: $zipPath" -ForegroundColor Green
    
    return $zipPath
}
#endregion

#region Interactive UI
function Show-RollbackManager {
    <#
    .SYNOPSIS
        Interactive TUI for managing rollback points.
    #>
    [CmdletBinding()]
    param()
    
    while ($true) {
        Clear-Host
        Write-Host @"
╔════════════════════════════════════════════════════════════════╗
║           🛡️  RawrXD Rollback Manager v1.0                     ║
╠════════════════════════════════════════════════════════════════╣
║                                                                ║
║  [1] 📋 List all rollback points                               ║
║  [2] 🔍 Verify rollback integrity                              ║
║  [3] 👁️  Preview restore (dry-run)                             ║
║  [4] ♻️  Execute full restore                                  ║
║  [5] 🗑️  Delete old rollbacks                                  ║
║  [6] 📦 Export rollback to ZIP                                 ║
║  [7] 📊 Show rollback statistics                               ║
║  [Q] 🚪 Exit                                                   ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan
        
        $choice = Read-Host "`nSelect operation"
        
        switch ($choice.ToUpper()) {
            '1' {
                Write-Host "`n[Rollback Points]" -ForegroundColor Yellow
                $backups = Get-TODORollbackPoints -MaxResults 20
                if ($backups) {
                    $backups | Format-Table -Property @(
                        @{L = 'ID'; E = { $_.BackupId.Substring(0, [Math]::Min(40, $_.BackupId.Length)) } },
                        @{L = 'Operation'; E = { $_.OperationName } },
                        @{L = 'Date'; E = { $_.Timestamp.ToString("yyyy-MM-dd HH:mm") } },
                        @{L = 'Files'; E = { $_.FileCount } },
                        @{L = 'Size (MB)'; E = { $_.SizeMB } }
                    ) -AutoSize
                }
                else {
                    Write-Host "No rollback points found." -ForegroundColor Gray
                }
                Read-Host "`nPress Enter to continue"
            }
            '2' {
                $id = Read-Host "`nEnter rollback ID (or partial match)"
                $backups = Get-TODORollbackPoints | Where-Object { $_.BackupId -like "*$id*" }
                if ($backups) {
                    Restore-TODORollbackPoint -BackupId $backups[0].BackupId -VerifyOnly
                }
                else {
                    Write-Host "No matching rollback found." -ForegroundColor Red
                }
                Read-Host "`nPress Enter to continue"
            }
            '3' {
                $id = Read-Host "`nEnter rollback ID (or partial match)"
                $backups = Get-TODORollbackPoints | Where-Object { $_.BackupId -like "*$id*" }
                if ($backups) {
                    Restore-TODORollbackPoint -BackupId $backups[0].BackupId -WhatIf
                }
                else {
                    Write-Host "No matching rollback found." -ForegroundColor Red
                }
                Read-Host "`nPress Enter to continue"
            }
            '4' {
                $id = Read-Host "`nEnter rollback ID (or partial match)"
                $backups = Get-TODORollbackPoints | Where-Object { $_.BackupId -like "*$id*" }
                if ($backups) {
                    Write-Host "`n⚠️  This will restore files to their backed-up state!" -ForegroundColor Red
                    $confirm = Read-Host "Type 'RESTORE' to confirm"
                    if ($confirm -eq "RESTORE") {
                        Restore-TODORollbackPoint -BackupId $backups[0].BackupId -Force
                    }
                    else {
                        Write-Host "Cancelled." -ForegroundColor Yellow
                    }
                }
                else {
                    Write-Host "No matching rollback found." -ForegroundColor Red
                }
                Read-Host "`nPress Enter to continue"
            }
            '5' {
                $days = Read-Host "`nDelete rollbacks older than how many days? [30]"
                if (-not $days) { $days = 30 }
                Remove-TODORollbackPoint -OlderThanDays ([int]$days) -Confirm
                Read-Host "`nPress Enter to continue"
            }
            '6' {
                $id = Read-Host "`nEnter rollback ID to export"
                $backups = Get-TODORollbackPoints | Where-Object { $_.BackupId -like "*$id*" }
                if ($backups) {
                    Export-TODORollbackPoint -BackupId $backups[0].BackupId
                }
                else {
                    Write-Host "No matching rollback found." -ForegroundColor Red
                }
                Read-Host "`nPress Enter to continue"
            }
            '7' {
                Write-Host "`n[Rollback Statistics]" -ForegroundColor Yellow
                $allBackups = Get-TODORollbackPoints -MaxResults 1000
                $totalSize = ($allBackups | Measure-Object -Property TotalBytes -Sum).Sum
                $totalFiles = ($allBackups | Measure-Object -Property FileCount -Sum).Sum
                
                Write-Host "  Total rollback points: $($allBackups.Count)" -ForegroundColor White
                Write-Host "  Total files backed up: $totalFiles" -ForegroundColor White
                Write-Host "  Total backup size:     $([math]::Round($totalSize / 1MB, 2)) MB" -ForegroundColor White
                Write-Host "  Backup root:           $($script:RollbackConfig.BackupRoot)" -ForegroundColor Gray
                Write-Host "  Max backups retained:  $($script:RollbackConfig.MaxBackups)" -ForegroundColor Gray
                Read-Host "`nPress Enter to continue"
            }
            'Q' { return }
        }
    }
}
#endregion

# Export module members
Export-ModuleMember -Function @(
    'New-TODORollbackPoint',
    'Get-TODORollbackPoints',
    'Restore-TODORollbackPoint',
    'Remove-TODORollbackPoint',
    'Export-TODORollbackPoint',
    'Show-RollbackManager'
)
