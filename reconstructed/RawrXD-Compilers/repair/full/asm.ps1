# Repair script for RawrXD_PE_Generator_FULL.asm
# Removes the corrupted fragments that were appended to lines (e.g., "raxnitialize" → "rax")

$filePath = "D:\RawrXD-Compilers\RawrXD_PE_Generator_FULL.asm"
$content = Get-Content $filePath -Raw

# Pattern 1: Lines ending with instruction/register followed by lowercase (e.g., "raxnitialize")
# Remove everything after the register/instruction stops
$corrupted = @(
    @{ old = "raxnitialize encoding key with entropy"; new = "rax" },
    @{ old = "pDosHeadernumber"; new = "pDosHeader" },
    @{ old = "PE_MAGICnimal"; new = "PE_MAGIC" },
    @{ old = "rbxntext"; new = "rbx" },
    @{ old = "rdinumber"; new = "rdi" },
    @{ old = "encoderKeyncoder"; new = "encoderKey" }
)

foreach ($pair in $corrupted) {
    if ($content.Contains($pair.old)) {
        Write-Host "Fixing: $($pair.old) → $($pair.new)"
        $content = $content -replace [regex]::Escape($pair.old), $pair.new
    }
}

# General pattern: Remove inline garbage appended to common register/instruction names
# Match cases like: 
#   mov raxNtext... → mov rax
#   rcxNText... → rcx
#   word ptr [rdi]ninstruction... → word ptr [rdi]
# But only when followed by a newline

$generalPattern = @(
    '\br[a-z]{2}[a-z][a-z]+(?=\s|$)' # Register + corrupted suffix
    '(word|byte|dword|qword)\s+ptr\s+\[[^\]]+\][a-z][a-z]+' # Memory operand + suffix
)

# For now, use a targeted approach: lines that end with n[lowercaseword]
# Remove the corrupted fragment while preserving the instruction
$lines = $content -split "`n"
for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    # Match lines that have corruption: word ending in lowercase after a valid token
    if ($line -match '([a-zA-Z0-9_\]]+)n([a-z]+)\s*$') {
        # Extract the base instruction/register and remove the corruption
        $baseMatch = [regex]::Match($line, '^(.+?)([a-zA-Z0-9_\]]+)n[a-z]+\s*$')
        if ($baseMatch.Success) {
            $lines[$i] = $baseMatch.Groups[1].Value + $baseMatch.Groups[2].Value
            Write-Host "Line $($i+1): Removed corruption"
        }
    }
}
$content = $lines -join "`n"

# Write the repaired content
Set-Content $filePath $content -Encoding ASCII
Write-Host "`n✓ Repair complete. Verify with: ml64.exe /c /nologo /Zi /W3 /Fo obj\test.obj RawrXD_PE_Generator_FULL.asm"
