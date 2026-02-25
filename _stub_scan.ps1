# Stub detector for MASM .asm files
$paths = @("D:\rawrxd\Ship", "D:\rawrxd\src\asm")
$stubKeywords = @("TODO", "STUB", "FIXME", "placeholder", "not implemented", "simplified", "mock")
$criticalNames = @("MatMul", "Conv", "FFT", "RMSNorm", "SoftMax", "Quant", "Q4", "Q8", "NF4", "GPTQ", "Vulkan", "DMA", "Tokeniz", "Infer", "Attention", "KV_Cache", "Dequant", "GGUF", "Tensor", "Swarm", "GPU", "SIMD", "AVX", "Flash")
$allStubs = @()

foreach ($basePath in $paths) {
    $files = Get-ChildItem -Path $basePath -Filter "*.asm" -Recurse -ErrorAction SilentlyContinue
    foreach ($file in $files) {
        $lines = Get-Content $file.FullName -ErrorAction SilentlyContinue
        if (-not $lines) { continue }
        
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match '^\s*(\w+)\s+PROC\b') {
                $procName = $Matches[1]
                $procStart = $i
                $procEnd = -1
                
                # Find ENDP
                for ($j = $i + 1; $j -lt [Math]::Min($i + 100, $lines.Count); $j++) {
                    if ($lines[$j] -match '^\s*\w+\s+ENDP\b') {
                        $procEnd = $j
                        break
                    }
                }
                if ($procEnd -eq -1) { continue }
                
                $procLines = $lines[$procStart..$procEnd]
                $procBody = $procLines -join "`n"
                $bodyLines = $lines[($procStart+1)..($procEnd-1)]
                
                # Count real instructions (skip blank, comments, labels, directives)
                $realInstructions = 0
                $hasStubComment = $false
                $matchedKeyword = ""
                
                foreach ($bl in $bodyLines) {
                    $trimmed = $bl.Trim()
                    if ($trimmed -eq "" -or $trimmed -match '^\s*;' -or $trimmed -match '^\s*\w+:$' -or $trimmed -match '^\s*LOCAL\b' -or $trimmed -match '^\s*USES\b') {
                        # Check comments for stub keywords
                        foreach ($kw in $stubKeywords) {
                            if ($trimmed -match [regex]::Escape($kw)) {
                                $hasStubComment = $true
                                $matchedKeyword = $kw
                            }
                        }
                        continue
                    }
                    # Also check inline comments
                    foreach ($kw in $stubKeywords) {
                        if ($trimmed -match [regex]::Escape($kw)) {
                            $hasStubComment = $true
                            $matchedKeyword = $kw
                        }
                    }
                    $realInstructions++
                }
                
                # Detect stub patterns
                $isRetOnly = ($realInstructions -le 1) -and ($procBody -match '\bret\b')
                $isXorRet = ($procBody -match 'xor\s+(eax|rax)\s*,\s*(eax|rax)') -and ($procBody -match '\bret\b') -and ($realInstructions -le 2)
                $isMinimal = $realInstructions -lt 5
                
                $isStub = $false
                $reason = ""
                if ($isRetOnly) { $isStub = $true; $reason = "ret-only" }
                elseif ($isXorRet) { $isStub = $true; $reason = "xor+ret" }
                elseif ($hasStubComment -and $isMinimal) { $isStub = $true; $reason = "stub-comment($matchedKeyword)+minimal($realInstructions instr)" }
                elseif ($hasStubComment) { $isStub = $true; $reason = "stub-comment($matchedKeyword)" }
                elseif ($isMinimal -and $realInstructions -le 3) { $isStub = $true; $reason = "minimal($realInstructions instr)" }
                
                if ($isStub) {
                    # Check if it matches critical names
                    $isCritical = $false
                    $critMatch = ""
                    foreach ($cn in $criticalNames) {
                        if ($procName -match $cn) { $isCritical = $true; $critMatch = $cn; break }
                    }
                    
                    # Estimate complexity
                    $complexity = 1
                    if ($procName -match "MatMul|Conv|Attention|Flash") { $complexity = 10 }
                    elseif ($procName -match "FFT|RMSNorm|SoftMax|Dequant|Quant") { $complexity = 8 }
                    elseif ($procName -match "Vulkan|GPU|DirectML|CUDA") { $complexity = 9 }
                    elseif ($procName -match "Token|Infer|GGUF|Tensor") { $complexity = 7 }
                    elseif ($procName -match "DMA|KV_Cache|Swarm|Stream") { $complexity = 6 }
                    elseif ($procName -match "AVX|SIMD|SSE") { $complexity = 5 }
                    elseif ($procName -match "Patch|Bridge|Hook") { $complexity = 4 }
                    elseif ($isCritical) { $complexity = 5 }
                    elseif ($isRetOnly -or $isXorRet) { $complexity = 3 }
                    
                    # Truncate to 20 lines
                    $displayLines = $procLines
                    if ($displayLines.Count -gt 20) {
                        $displayLines = $procLines[0..19]
                    }
                    
                    $allStubs += [PSCustomObject]@{
                        File = $file.FullName
                        Proc = $procName
                        Reason = $reason
                        InstrCount = $realInstructions
                        IsCritical = $isCritical
                        CritMatch = $critMatch
                        Complexity = $complexity
                        LineNum = $procStart + 1
                        Code = ($displayLines -join "`n")
                    }
                }
            }
        }
    }
}

# Sort by complexity desc, then by critical
$sorted = $allStubs | Sort-Object -Property @{Expression={$_.Complexity}; Descending=$true}, @{Expression={$_.IsCritical}; Descending=$true}

Write-Host "=== TOTAL STUBS FOUND: $($allStubs.Count) ==="
Write-Host ""

$top = $sorted | Select-Object -First 30
$rank = 0
foreach ($s in $top) {
    $rank++
    Write-Host "=== STUB #$rank (Complexity: $($s.Complexity)/10) ==="
    Write-Host "File: $($s.File)"
    Write-Host "Proc: $($s.Proc) (Line $($s.LineNum))"
    Write-Host "Reason: $($s.Reason)"
    Write-Host "Instructions: $($s.InstrCount)"
    Write-Host "Critical Match: $($s.CritMatch)"
    Write-Host "--- Code ---"
    Write-Host $s.Code
    Write-Host "--- End ---"
    Write-Host ""
}
