# RawrZ generate_poly_key.ps1 - One-click polymorphism generator
# Generates a new random key for each build
"#pragma once`n#define RANDOM_KEY 0x$('{0:X8}' -f (Get-Random))u" | Out-File -FilePath "poly_key.h" -Encoding ASCII
Write-Host "Generated new polymorphic key in poly_key.h" -ForegroundColor Yellow
