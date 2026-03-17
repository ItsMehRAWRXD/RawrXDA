
$files = Get-ChildItem -Path "D:\lazy init ide\auto_generated_methods\*.psm1"
foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw
    if ($content -match "@@{") {
        Write-Host "Fixing syntax in $($file.Name)"
        $content = $content -replace "@@{", "@{"
        Set-Content -Path $file.FullName -Value $content -Encoding UTF8
    }
}
Write-Host "All files fixed."
