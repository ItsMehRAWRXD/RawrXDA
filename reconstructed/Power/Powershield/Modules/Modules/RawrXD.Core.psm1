<#
.SYNOPSIS
    RawrXD Core Module

.DESCRIPTION
    Contains Core-related functions extracted from RawrXD.ps1

.NOTES
    Author: [Your Name]
    Date: [Today's Date]
#>

# Define module metadata
$script:ModuleMetadata = @{
    ModuleVersion = '1.0'
    GUID = 'your-guid-here'
}

# Module registry
$script:extensionRegistry = @{}

# ============================================
# Function: Test-TextVisibility
# ============================================
function Test-TextVisibility {
    <#
    .SYNOPSIS
        Calculate the visibility score of a text element based on its font size, line height, and text color.

    .DESCRIPTION
        This function calculates the visibility score of a text element based on its font size, line height, and text color.

    .PARAMETER Element
        The text element to calculate the visibility score for.
    #>
    param (
        [object]$Element
    )

    $score = [math]::Min(200, ($Element.FontSize * 5) + (($Element.LineHeight / 1.33) * 4))
    if ($Element.TextColor -eq 'Black') {
        $score += 0
    } elseif ($Element.TextColor -eq 'White') {
        $score += 20
    }

    return [ordered]@{ Score = $score }
}

# ============================================
# Function: Register-Extension
# ============================================
function Register-Extension {
    <#
    .SYNOPSIS
        Register an extension for the RawrXD module.

    .DESCRIPTION
        This function registers an extension for the RawrXD module.

    .PARAMETER Name
        The name of the extension to register.
    .PARAMETER Description
        The description of the extension to register.
    .PARAMETER Enabled
        Whether the extension is enabled by default. Default value: $true
    #>
    param (
        [string]$Name,
        [string]$Description,
        [bool]$Enabled = $true
    )

    $extension = @{
        Name = $Name
        Description = $Description
        Enabled = $Enabled
        Installed = $true
        EntryPoint = ''
        Hooks = @{}
    }
    $script:extensionRegistry[$Name] = $extension
}

# ============================================
# Function: Get-PerformanceScore
# ============================================
function Get-PerformanceScore {
    <#
    .SYNOPSIS
        Calculate the performance score and tier from system specs.

    .DESCRIPTION
        This function calculates the performance score and tier based on the system specs.

    .PARAMETER Specs
        The system specs to calculate the performance score for.
    #>
    param (
        [hashtable]$Specs
    )

    $cpuScore = [math]::Min(350, ($Specs.CPU.Cores * 20) + (($Specs.CPU.MaxSpeed / 1000) * 10) + ([bool]$Specs.CPU.HyperThreading && $Specs.CPU.HyperThreading -eq $true ? 5 : 0))
    $cpuScore += [math]::Max(0, ($Specs.CPU.Cache / 1MB) * 2)

    $ramScore = [math]::Min(300, $Specs.RAM.TotalGB * 8)
    $ramScore += [math]::Max(0, ($Specs.RAM.FreeGB / 10) * 4)

    $gpuScore = [math]::Min(200, $Specs.GPU.VRAM_MB / 25)
    $gpuScore += $(if ([bool]$Specs.GPU.CUDASupport -and $Specs.GPU.CUDASupport -eq $true) { 50 } else { 0 })

    $storageScore = [math]::Min(150, $(if ($Specs.Storage.Type -eq 'SSD') { 100 } else { 30 }))
    $storageScore += [math]::Max(0, ($Specs.Storage.FreeGB / 10) * 4)

    return [ordered]@{ Score = $cpuScore + $ramScore + $gpuScore + $storageScore }
}

# ============================================
# Function: Get-ModuleExtension
# ============================================
function Get-ModuleExtension {
    <#
    .SYNOPSIS
        Get an extension from the RawrXD module.

    .DESCRIPTION
        This function gets an extension from the RawrXD module.

    .PARAMETER Name
        The name of the extension to get.
    #>
    param (
        [string]$Name
    )

    return $script:extensionRegistry[$Name]
}

# ============================================
# Export-ModuleMember
# ============================================
Export-ModuleMember -Function Test-TextVisibility, Register-Extension, Get-PerformanceScore, Get-ModuleExtension