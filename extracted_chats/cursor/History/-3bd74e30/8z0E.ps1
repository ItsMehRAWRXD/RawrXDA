<#
.SYNOPSIS
Apply relocations and resolve symbols in compiled code
.DESCRIPTION
Patches binary code with resolved symbol addresses from relocation table
#>

function Resolve-Symbol {
    param(
        [string]$Name,
        [hashtable]$SymbolTable
    )
    
    if ($SymbolTable.ContainsKey($Name)) {
        return $SymbolTable[$Name]
    } else {
        Write-Warning "[LINKER] Undefined symbol: $Name"
        return 0x400000
    }
}

function Apply-Relocations {
    param(
        [byte[]]$Binary,
        [array]$Relocations,
        [hashtable]$SymbolTable
    )
    
    Write-Host "[LINKER] Applying $($Relocations.Count) relocations"
    
    $result = $Binary.Clone()
    
    foreach ($reloc in $Relocations) {
        if ($reloc.PSObject.Properties.Name -contains 'offset' -and $reloc.PSObject.Properties.Name -contains 'symbol') {
            $offset = $reloc.offset
            $targetAddr = Resolve-Symbol $reloc.symbol $SymbolTable
            $patch = [BitConverter]::GetBytes($targetAddr)
            
            for ($i = 0; $i -lt 8; $i++) {
                if ($offset + $i -lt $result.Length) {
                    $result[$offset + $i] = $patch[$i]
                }
            }
        }
    }
    
    return $result
}

Export-ModuleMember -Function Resolve-Symbol, Apply-Relocations
