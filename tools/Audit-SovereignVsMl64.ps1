# Compare first-token "mnemonic" tokens in .asm files against SovereignAssembler's supported subset.
# Heuristic only (same spirit as tools/audit_asm_mnemonics.ps1).
# Usage: pwsh -File tools/Audit-SovereignVsMl64.ps1

$ErrorActionPreference = 'Stop'

$Supported = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
@(
    'add', 'or', 'and', 'sub', 'xor', 'cmp', 'test',
    'ret', 'nop', 'syscall', 'push', 'pop',
    'mov', 'movzx', 'movsx', 'lea', 'movsxd',
    'jmp', 'call',
    'jo', 'jno', 'jb', 'jc', 'jnae', 'jnb', 'jae', 'jnc', 'je', 'jz', 'jne', 'jnz',
    'jbe', 'jna', 'ja', 'jnbe', 'js', 'jns', 'jp', 'jpe', 'jnp', 'jpo', 'jl', 'jnge',
    'jge', 'jnl', 'jle', 'jng', 'jg', 'jnle'
) | ForEach-Object { [void]$Supported.Add($_) }

$roots = @(
    (Join-Path $PSScriptRoot '..\src'),
    (Join-Path $PSScriptRoot '..\Ship')
) | ForEach-Object { Resolve-Path $_ -ErrorAction SilentlyContinue } | ForEach-Object { $_.Path }

$unknown = @{}
$lines = 0
foreach ($root in $roots) {
    if (-not (Test-Path -LiteralPath $root)) { continue }
    Get-ChildItem -LiteralPath $root -Recurse -Filter *.asm -File -ErrorAction SilentlyContinue | ForEach-Object {
        Get-Content -LiteralPath $_.FullName -ErrorAction SilentlyContinue | ForEach-Object {
            $line = $_
            $sc = $line.IndexOf(';')
            if ($sc -ge 0) { $line = $line.Substring(0, $sc) }
            $line = $line.Trim()
            if ($line.Length -eq 0) { return }
            if ($line.StartsWith('.')) { return }
            $parts = $line -split '\s+', 20, [System.StringSplitOptions]::RemoveEmptyEntries
            if ($parts.Count -lt 1) { return }
            $t = $parts[0]
            if ($t -match ':$') { return }
            $tok = $t.ToLowerInvariant()
            if ($tok -match '^(rep|repe|repz|repne|repnz|lock|xacquire|xrelease)$') { return }
            $lines++
            if (-not $Supported.Contains($tok)) {
                if (-not $unknown.ContainsKey($tok)) { $unknown[$tok] = 0 }
                $unknown[$tok]++
            }
        }
    }
}

Write-Host "--- Sovereign subset: $($Supported.Count) mnemonics ---"
Write-Host "--- Scanned heuristic lines: $lines ---"
Write-Host "--- Tokens NOT in supported set (top 80 by count) ---"
$unknown.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 80 | ForEach-Object { "$($_.Value)`t$($_.Key)" }
Write-Host "--- Done ---"
