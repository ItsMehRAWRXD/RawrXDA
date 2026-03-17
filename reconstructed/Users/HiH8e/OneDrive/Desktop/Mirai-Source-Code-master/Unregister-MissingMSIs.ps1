#Requires -Version 3

# The MIT License (MIT)
# Copyright (C) Microsoft Corporation. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

[CmdletBinding(SupportsShouldProcess = $true)]
param (
  [Parameter(Position = 0, ValueFromPipeline = $true)]
  [ValidateNotNullOrEmpty()]
  [string[]] $ProductCode
)

$ErrorActionPreference = 'Stop'
[int[]] $translation = 7, 6, 5, 4, 3, 2, 1, 0, 11, 10, 9, 8, 15, 14, 13, 12, 17, 16, 19, 18, 21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30

$loc = data {
  ConvertFrom-StringData @'
        Error_Elevation_Required = You must run this script in an elevated command prompt
        Error_64Bit_Required = You must run this in a 64-bit command prompt
        Error_PackageManagement_Required = Please install PackageManagement from http://go.microsoft.com/fwlink/?LinkID=746217
        Process_Remove_Args1 = Remove registration for {0}
        Verbose_Install_MSI = Installing the "MSI" module
        Verbose_Scan_Missing = Scanning for products missing cached packages
        Verbose_Remove_Key_Args1 = Removing registry key {0}
        Verbose_Remove_Value_Args2 = Removing registry value {1} from {0}
        Verbose_Remove_Source_Reg = Removing source registration
        Verbose_Remove_Product_Reg = Removing product registration
        Verbose_Remove_Upgrade_Reg = Removing upgrade registration
        Verbose_Remove_Component_Reg = Removing component registration
        Verbose_Found_Source_Args2 = Cache missing for {0} but found source at {1}
'@
}

$identity = [System.Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object System.Security.Principal.WindowsPrincipal $identity
if (!$principal.IsInRole([System.Security.Principal.WindowsBuiltInRole]::Administrator)) {
  throw $loc.Error_Elevation_Required
}

if ([System.Environment]::Is64BitOperatingSystem) {
  if (![System.Environment]::Is64BitProcess) {
    throw $loc.Error_64Bit_Required
  }
}

$pack = {
  param (
    [string] $Guid
  )

  if (!$Guid) {
    return
  }

  $Guid = (New-Object System.Guid $Guid).ToString("N").ToUpperInvariant()

  $sb = New-Object System.Text.StringBuilder $translation.Count
  foreach ($i in $translation) {
    $null = $sb.Append($Guid[$i])
  }

  $sb.ToString()
}

$test = {
  param (
    $Product
  )

  if ($Product.PSPath -and ($Product | Test-Path)) {
    return $true
  }

  if ($Product.PackageName) {
    $Product | Get-MSISource | ForEach-Object {
      $path = Join-Path $_.Path $Product.PackageName
      if ($path | Test-Path) {
        Write-Verbose ($loc.Verbose_Found_Source_Args2 -f $Product.ProductCode, $path)
        return $true
      }
    }
  }

  $false
}

$remove = {
  param (
    [string] $Key
  )

  if (Test-Path $Key) {
    Write-Verbose ($loc.Verbose_Remove_Key_Args1 -f $Key)
    Remove-Item -Recurse -Force $Key
  }
}

$removeChild = {
  param (
    [string] $Key,
    [string] $Name
  )

  if (Test-Path $Key) {
    Get-ChildItem $Key | ForEach-Object {
      $obj = $_ | Get-ItemProperty
      if ($obj.$Name -ne $null) {
        Write-Verbose ($loc.Verbose_Remove_Value_Args2 -f $_.Name, $Name)
        Remove-ItemProperty -Force -Name $Name -LiteralPath $_.PSPath

        $obj = Get-ItemProperty -LiteralPath $_.PSPath
        if (!$obj) {
          Write-Verbose ($loc.Verbose_Remove_Key_Args1 -f $_.Name)
          Remove-Item -Recurse -Force -LiteralPath $_.PSPath
        }
      }
    }
  }
}

if (!$ProductCode) {
  # Install the MSI module if missing.
  if (!(Get-Module -ListAvailable MSI)) {
    Write-Verbose $loc.Verbose_Install_MSI

    # Make sure PackageManagement is installed (comes with WMF 5.0 / Windows 10).
    if (!(Get-Module -ListAvailable PackageManagement)) {
      throw $loc.Error_PackageManagement_Required
    }

    Install-Module MSI -Scope CurrentUser -SkipPublisherCheck -Force
  }

  Write-Verbose $loc.Verbose_Scan_Missing
  foreach ($msi in (Get-MSIProductInfo -UserContext Machine)) {
    if (!(&$test $msi)) {
      $ProductCode += $msi.ProductCode
    }
  }
}

foreach ($code in $ProductCode) {
  if ($PSCmdlet.ShouldProcess($msi, $loc.Process_Remove_Args1 -f $code)) {
    $packedProductCode = &$pack $code

    Write-Verbose $loc.Verbose_Remove_Source_Reg
    &$remove "HKLM:\SOFTWARE\Classes\Installer\Products\$packedProductCode"
    &$remove "HKLM:\SOFTWARE\Classes\Installer\Features\$packedProductCode"

    Write-Verbose $loc.Verbose_Remove_Product_Reg
    &$remove "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Installer\UserData\S-1-5-18\Products\$packedProductCode"
    &$remove "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$code"
    &$remove "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\$code"

    Write-Verbose $loc.Verbose_Remove_Upgrade_Reg
    &$removeChild "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Installer\UpgradeCodes" $packedProductCode

    Write-Verbose $loc.Verbose_Remove_Component_Reg
    &$removeChild "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Installer\UserData\S-1-5-18\Components" $packedProductCode
  }
}

<#
.SYNOPSIS
Removes Windows Installer product registrtation for missing or specified MSIs

.DESCRIPTION
If Windows Installer product registration is corrupt (exit code 1610) or package
sources are missing (exit code 1603, error message 1714; or exit code 1612),
you can use this script in an elevated PowerShell command shell to clean up the
registration is a transactional manner to avoid making machine state worse.

Please note that this should be a last resort and only for those issues above.
The old msizap.exe program was frought with issues and can make matters worse
if not used properly.

.PARAMETER ProductCode
Optional list of ProductCode to clean up; otherwise, ProductCodes are scanned
from products with missing sources.

.EXAMPLE
PS> Unregister-MissingMSIs.ps1

Removes per-machine product registration for products with missing cached MSIs.

.EXAMPLE
PS> Unregister-MissingMSIs.ps1 '{7B88D6BB-A664-4E5A-AB81-C435C8639A4D}'

Remove per-machine product registration for the specified ProductCode only.
#>
