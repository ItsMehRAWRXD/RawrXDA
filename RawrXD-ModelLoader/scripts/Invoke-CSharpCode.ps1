function Invoke-CSharpCode {
    param(
        [Parameter(Mandatory, ValueFromPipeline)]
        [string]$Source,

        [string[]]$References = @('System.dll', 'System.Core.dll')
    )

    begin {
        Add-Type -AssemblyName System.CodeDom
        $provider = [System.CodeDom.Compiler.CodeDomProvider]::CreateProvider('CSharp')
    }

    process {
        $cp = New-Object System.CodeDom.Compiler.CompilerParameters
        $cp.GenerateInMemory   = $true
        $cp.GenerateExecutable = $false          # DLL only
        $cp.WarningLevel       = 4
        $cp.ReferencedAssemblies.AddRange($References)

        $results = $provider.CompileAssemblyFromSource($cp, $Source)

        if ($results.Errors.Count) {
            $results.Errors | ForEach-Object { Write-Error $_.ToString() }
            return
        }

        $asm  = $results.CompiledAssembly
        $type = $asm.GetTypes() |
                Where-Object { $_.GetMethod('Main') } |
                Select-Object -First 1

        if (-not $type) {
            Write-Error "No type with a Main method found."
            return
        }

        $main = $type.GetMethod('Main')

        # Decide how to invoke:
        #  - static Main  → $null
        #  - instance Main → create object
        $target = if ($main.IsStatic) { $null }
                  else { [Activator]::CreateInstance($type) }

        # Accept args if present, otherwise empty string[]
        $args = [string[]]@()
        $main.Invoke($target, $args)
    }
}
