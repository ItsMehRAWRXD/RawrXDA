# Create a disposable test workspace and open it in VS Code
$testDir = Join-Path $env:USERPROFILE 'Desktop\amazonq-test'
if (-not (Test-Path $testDir)) { New-Item -Path $testDir -ItemType Directory | Out-Null }
$file = Join-Path $testDir 'test.py'
if (-not (Test-Path $file)) { Set-Content -Path $file -Value 'def greet(name):\n    return f"Hello, {name}!"\n' -Encoding UTF8 }
# Open VS Code with the folder
$code = 'code'
Start-Process -FilePath $code -ArgumentList @($testDir) -NoNewWindow
Write-Output "OPENED:$testDir"