$p = 'C:\Users\HiH8e\.ollama.backup-20251009-034437'
if (Test-Path $p) {
    $s = (Get-ChildItem -LiteralPath $p -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
    $gb = [math]::Round(($s/1GB),3)
    Write-Host "Found backup at: $p"
    Write-Host "Size: $s bytes (~$gb GB)"
    Write-Host "Deleting backup now..."
    try{
        Remove-Item -LiteralPath $p -Recurse -Force -ErrorAction Stop
        Write-Host "Deleted backup: $p"
    }catch{
        Write-Host "Failed to delete backup: $_"
        exit 1
    }
} else {
    Write-Host "Backup not found at: $p"
}
