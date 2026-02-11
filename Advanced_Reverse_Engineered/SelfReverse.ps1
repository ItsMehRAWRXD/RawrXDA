# Self-Reversing Mechanism for Cursor IDE
# This script can reverse the reconstruction process if needed

param(
    [switch]$Reverse,
    [switch]$Verify,
    [switch]$Release
)

$manifest = Get-Content "MANIFEST.json" | ConvertFrom-Json
$reconstruction = Get-Content "RECONSTRUCTION.json" | ConvertFrom-Json

if ($Reverse) {
    Write-Host "Reversing reconstruction..."
    foreach ($mapping in $reconstruction.FileMappings) {
        $srcFile = Join-Path "src" $mapping.Reconstructed
        $outFile = Join-Path "out" $mapping.Original
        
        if (Test-Path $srcFile) {
            # Reverse the reconstruction
            Copy-Item -Path $srcFile -Destination $outFile -Force
            Write-Host "Reversed: $($mapping.Reconstructed) -> $($mapping.Original)"
        }
    }
    Write-Host "Reversal complete!"
}

if ($Verify) {
    Write-Host "Verifying reconstruction integrity..."
    $verified = 0
    $failed = 0
    
    foreach ($mapping in $reconstruction.FileMappings) {
        $srcFile = Join-Path "src" $mapping.Reconstructed
        $outFile = Join-Path "out" $mapping.Original
        
        if (Test-Path $srcFile -and Test-Path $outFile) {
            $srcHash = Get-FileHash $srcFile -Algorithm SHA256
            $outHash = Get-FileHash $outFile -Algorithm SHA256
            
            if ($srcHash.Hash -eq $outHash.Hash) {
                $verified++
            } else {
                $failed++
                Write-Warning "Hash mismatch: $($mapping.Reconstructed)"
            }
        }
    }
    
    Write-Host "Verified: $verified, Failed: $failed"
}

if ($Release) {
    Write-Host "Releasing control..."
    # Release any locked resources
    # Reset any modified files
    # Restore original state
    
    foreach ($mapping in $reconstruction.FileMappings) {
        $srcFile = Join-Path "src" $mapping.Reconstructed
        $backupFile = Join-Path "src" "$($mapping.Reconstructed).backup"
        
        if (Test-Path $backupFile) {
            # Restore from backup
            Copy-Item -Path $backupFile -Destination $srcFile -Force
            Remove-Item -Path $backupFile -Force
            Write-Host "Released: $($mapping.Reconstructed)"
        }
    }
    
    Write-Host "Control released!"
}
