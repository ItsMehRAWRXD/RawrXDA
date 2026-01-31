# RawrZ HotDrop.ps1 - One-liner drag-and-drop encryption
# Uses Windows ProtectedData for secure encryption
$args|%{ $f=$_; $p=Read-Host -AsSecureString "Enter passphrase"; $d=[System.IO.File]::ReadAllBytes($f); $s=[System.Security.Cryptography.ProtectedData]::Protect($d,($p|ConvertFrom-SecureString -AsPlainText),'CurrentUser'); [System.IO.File]::WriteAllBytes("$f.hp",$s); Write-Host "→ $f.hp" -ForegroundColor Green }
