cd "d:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\build\bin\Release"
Write-Host "Starting IDE..."
$proc = Start-Process -FilePath ".\RawrXD-AgenticIDE.exe" -NoNewWindow -PassThru
Write-Host "Process started with ID: $($proc.Id)"
Start-Sleep -Seconds 3
Write-Host "Killing process..."
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Host "Done"
