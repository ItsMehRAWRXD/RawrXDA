# Fix 32-bit MASM directives in x64 ASM files
$files = Get-ChildItem "d:\rawrxd\src\asm\*.asm" | Where-Object {
    (Get-Content $_.FullName -Raw) -match '\.686p'
}

$count = 0
foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw
    
    # Replace the problematic directives block
    $pattern = '; Assembler directives[\r\n]+\.686p[\r\n]+\.xmm[\r\n]+\.model flat, stdcall[\r\n]+\.option casemap:none[\r\n]+\.option frame:auto[\r\n]+\.option win64:3[\r\n]+'
    $replacement = "; Assembler directives (x64)`r`n; Note: .686p, .xmm, .model flat are 32-bit only; ml64 doesn't use them`r`n`r`n"
    
    if ($content -match $pattern) {
        $newContent = $content -replace $pattern, $replacement
        Set-Content -Path $file.FullName -Value $newContent -NoNewline
        $count++
        Write-Host "Fixed: $($file.Name)"
    }
}

Write-Host "`nTotal files fixed: $count"
