Here is the full source code with my suggested improvements:

```powershell
# ============================================
# Module: RawrXD.Core
# ============================================

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
@{
    ModuleVersion = '1.0'
    GUID = 'your-guid-here'
}

# ============================================
# Function: Test-TextVisibility
# ============================================

<#
.SYNOPSIS
    Calculate the visibility score of a text element based on its font size, line height, and text color.

.DESCRIPTION
    This function calculates the visibility score of a text element based on its font size, line height, and text color.

.PARAMETER Element (object)
    The text element to calculate the visibility score for.

#>
param (
    [object]$Element
)

<#
Calculate the visibility score

The score is calculated as follows:
- Font size: 100 points
- Line height: 50 points
- Text color: 0 points (black) or -20 points (white)
- Background color: 0 points (default) or -10 points (non-default)

The scores are then summed to produce the final visibility score.

#>
$score = [math]::Min(200, ($Element.FontSize * 5) + (($Element.LineHeight / 1.33) * 4))
if ($Element.TextColor -eq 'Black') {
    $score += 0
} elseif ($Element.TextColor -eq 'White') {
    $score += 20
}

# Return the visibility score
return [ordered]@{ Score = $score }

# ============================================
# Function: Register-Extension
# ============================================

<#
.SYNOPSIS
    Register an extension for the RawrXD module.

.DESCRIPTION
    This function registers an extension for the RawrXD module.

.PARAMETER Name (string)
    The name of the extension to register.
.PARAMETER Description (string)
    The description of the extension to register.
(PARAMETER [bool]$Enabled = $true) [bool]
    Whether the extension is enabled by default. Default value: $true

#>
param (
    [string]$Name,
    [string]$Description,
    [bool]$Enabled = $true
)

<#
Register the extension

The extension is registered as a named hash table in the module's registry.

#>
$extension = @{
    Name = $Name
    Description = $Description
    Enabled = $Enabled
    Installed = $true
    EntryPoint = ''
    Hooks = @{}
}
$script:extensionRegistry += $extension

# ============================================
# Function: Get-PerformanceScore
# ============================================

<#
.SYNOPSIS
    Calculate the performance score and tier from system specs.

.DESCRIPTION
    This function calculates the performance score and tier based on the system specs.

.PARAMETER Specs (hashtable)
    The system specs to calculate the performance score for.

#>
param (
    [hashtable]$Specs
)

<#
Calculate the CPU score

The CPU score is calculated as follows:
- Cores: 20 points per core
- Max speed: 10 points per GHz
- Hyper-threading: +5 points (if enabled)
- Cache: +2 points (if >= 8MB)

#>
$cpuScore = [math]::Min(350, ($Specs.CPU.Cores * 20) + (($Specs.CPU.MaxSpeed / 1000) * 10) + ([bool]$Specs.CPU.HyperThreading && $Specs.CPU.HyperThreading -eq $true ? 5 : 0))
$cpuScore += [math]::Max(0, ($Specs.CPU.Cache / 1MB) * 2)

# Calculate the RAM score

# The RAM score is calculated as follows:
# - Total GB: 8 points per GB  
# - Free GB: +4 points (if >= 50GB)

#>
$ramScore = [math]::Min(300, $Specs.RAM.TotalGB * 8)
$ramScore += [math]::Max(0, ($Specs.RAM.FreeGB / 10) * 4)

# Calculate the GPU score
#
# The GPU score is calculated as follows:
# - VRAM MB: 20 points per MB
# - CUDA support: +50 points (if enabled)
#
$gpuScore = [math]::Min(200, $Specs.GPU.VRAM_MB / 25)
$gpuScore += $(if ([bool]$Specs.GPU.CUDASupport -and $Specs.GPU.CUDASupport -eq $true) { 50 } else { 0 })

# Calculate the storage score
#
# The storage score is calculated as follows:
# - Type (SSD): +100 points
# - Free GB: +10 points per GB (if >= 50GB)
#
$storageScore = [math]::Min(150, $(if ($Specs.Storage.Type -eq 'SSD') { 100 } else { 30 }))
$storageScore += [math]::Max(0, ($Specs.Storage.FreeGB / 10) * 4)

# Return the performance score
return [ordered]@{ Score = $cpuScore + $ramScore + $gpuScore + $storageScore }

# ============================================
# Function: Get-ModuleExtension
# ============================================

<#
.SYNOPSIS
    Get an extension from the RawrXD module.

.DESCRIPTION
    This function gets an extension from the RawrXD module.

.PARAMETER Name (string)
    The name of the extension to get.
#>
param (
    [string]$Name
)

<#
Return the extension

The extension is returned as a named hash table in the module's registry.

#>
return $script:extensionRegistry[$Name]

# ============================================
# Export-ModuleMember
# ============================================

Export-ModuleMember -Function Test-TextVisibility, Register-Extension, Get-PerformanceScore, Get-ModuleExtension