# Dependency Manager for IDE Modules
# Resolves dependencies and prevents circular imports

param()

class ModuleDependencyResolver {
    [hashtable]$DependencyGraph
    [hashtable]$LoadedModules

    ModuleDependencyResolver() {
        $this.DependencyGraph = @{}
        $this.LoadedModules = @{}
    }

    [void] AddModule([string]$ModuleName, [string[]]$Dependencies) {
        if (-not $this.DependencyGraph.ContainsKey($ModuleName)) {
            $this.DependencyGraph[$ModuleName] = @()
        }

        $this.DependencyGraph[$ModuleName] = $Dependencies
    }

    [void] LoadModule([string]$ModuleName) {
        $this.LoadModuleInternal($ModuleName, @())
    }

    hidden [void] LoadModuleInternal([string]$ModuleName, [string[]]$Stack) {
        if ($this.LoadedModules.ContainsKey($ModuleName)) {
            return
        }

        if ($Stack -contains $ModuleName) {
            $cycle = ($Stack + $ModuleName) -join ' -> '
            throw "Circular dependency detected: $cycle"
        }

        $nextStack = $Stack + $ModuleName

        if ($this.DependencyGraph.ContainsKey($ModuleName)) {
            foreach ($dep in $this.DependencyGraph[$ModuleName]) {
                $this.LoadModuleInternal($dep, $nextStack)
            }
        }

        Import-Module -Name $ModuleName -Global -ErrorAction Stop
        $this.LoadedModules[$ModuleName] = $true
    }

    [string[]] GetLoadOrder() {
        $order = New-Object System.Collections.Generic.List[string]
        $visited = @{}

        foreach ($module in $this.DependencyGraph.Keys) {
            $this.TopologicalVisit($module, $visited, $order)
        }

        return $order.ToArray()
    }

    hidden [void] TopologicalVisit([string]$ModuleName, [hashtable]$Visited, [System.Collections.Generic.List[string]]$Order) {
        if ($Visited.ContainsKey($ModuleName)) {
            return
        }

        $Visited[$ModuleName] = $true

        if ($this.DependencyGraph.ContainsKey($ModuleName)) {
            foreach ($dep in $this.DependencyGraph[$ModuleName]) {
                $this.TopologicalVisit($dep, $Visited, $Order)
            }
        }

        if (-not $Order.Contains($ModuleName)) {
            $Order.Add($ModuleName)
        }
    }
}

function New-ModuleDependencyResolver {
    <#
    .SYNOPSIS
    Creates a new dependency resolver instance
    #>
    [CmdletBinding()]
    param()
    return [ModuleDependencyResolver]::new()
}

Export-ModuleMember -Function @('New-ModuleDependencyResolver')
