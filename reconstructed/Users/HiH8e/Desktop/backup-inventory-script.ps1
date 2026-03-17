$src = 'C:\Users\HiH8e\Desktop\inventory-ollama-and-folders.ps1'
$dst = 'C:\Users\HiH8e\Desktop\inventory-ollama-and-folders.ps1.backup'
$content = Get-Content -Raw -Path $src
Set-Content -Path $dst -Value $content
Write-Output "BACKUP_CREATED:$dst"