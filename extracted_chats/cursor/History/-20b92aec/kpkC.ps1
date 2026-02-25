<#
.SYNOPSIS
Optimize IR with multiple optimization passes
.DESCRIPTION
Applies dead code elimination, constant folding, and other optimizations
#>

function Optimize-IR {
    param(
        [array]$IR,
        [string]$Level = "O2"
    )
    
    Write-Host "[OPTIMIZER] Applying $Level optimizations"
    
    if ($Level -eq "O0") {
        return $IR
    }
    
    $optimized = $IR
    
    if ($Level -match "O1|O2|Os") {
        $optimized = Eliminate-DeadCode $optimized
        $optimized = Fold-Constants $optimized
    }
    
    if ($Level -eq "O2") {
        $optimized = Inline-SimpleCalls $optimized
        $optimized = Reorder-Instructions $optimized
    }
    
    return $optimized
}

function Eliminate-DeadCode {
    param([array]$IR)
    
    $result = [System.Collections.Generic.List[string]]::new()
    foreach ($instr in $IR) {
        # Skip no-ops and unreachable code
        if ($instr -match "^nop$|^int3$") {
            continue
        }
        $result.Add($instr)
    }
    return ,$result.ToArray()
}

function Fold-Constants {
    param([array]$IR)
    
    $result = [System.Collections.Generic.List[string]]::new()
    foreach ($instr in $IR) {
        # Constant folding: mov rax, 0 + mov rax, 1 = mov rax, 1
        if ($instr -match "mov rax, 0$" -and $result.Count -gt 0 -and $result[-1] -match "mov rax,") {
            $result.RemoveAt($result.Count - 1)
        }
        $result.Add($instr)
    }
    return ,$result.ToArray()
}

function Inline-SimpleCalls {
    param([array]$IR)
    
    # Placeholder for inlining
    return $IR
}

function Reorder-Instructions {
    param([array]$IR)
    
    # Placeholder for instruction reordering
    return $IR
}

Export-ModuleMember -Function Optimize-IR, Eliminate-DeadCode, Fold-Constants, Inline-SimpleCalls, Reorder-Instructions
