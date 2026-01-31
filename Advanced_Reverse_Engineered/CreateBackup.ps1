# Backup mechanism for self-reversing
# Creates backups before modifications

param(
    [string[]]$Files
)

foreach ($file in $Files) {
    if (Test-Path $file) {
        $backupPath = "$file.backup"
        Copy-Item -Path $file -Destination $backupPath -Force
        Write-Host "Backup created: $backupPath"
    }
}
