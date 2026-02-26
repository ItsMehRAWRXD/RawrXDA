$srcDir = "D:\rawrxd\src"
$files = Get-ChildItem -Path $srcDir -Recurse -Include *.cpp, *.h, *.hpp

# Replace qHash() with std::hash
$content = Get-Content -Path "D:\rawrxd\src\qtapp\agentic_puppeteer.cpp" -Raw
$content = $content -replace 'qHash\(', 'std::hash<std::string>()('
Set-Content -Path "D:\rawrxd\src\qtapp\agentic_puppeteer.cpp" -Value $content -Encoding UTF8
Write-Host "Fixed: agentic_puppeteer.cpp"

$content = Get-Content -Path "D:\rawrxd\src\qtapp\inference_engine.cpp" -Raw
$content = $content -replace 'qHash\(', 'std::hash<std::string>()('
Set-Content -Path "D:\rawrxd\src\qtapp\inference_engine.cpp" -Value $content -Encoding UTF8
Write-Host "Fixed: inference_engine.cpp"

$content = Get-Content -Path "D:\rawrxd\src\agentic_configuration.cpp" -Raw
$content = $content -replace 'qHash\(', 'std::hash<std::string>()('
Set-Content -Path "D:\rawrxd\src\agentic_configuration.cpp" -Value $content -Encoding UTF8
Write-Host "Fixed: agentic_configuration.cpp"

Write-Host ""
Write-Host "All qHash() functions replaced with std::hash()"
