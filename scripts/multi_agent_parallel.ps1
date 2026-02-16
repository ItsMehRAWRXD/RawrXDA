#!/usr/bin/env pwsh
param([Parameter(Mandatory)]$Tasks,[int]$MaxParallel=3,[scriptblock]$ScriptBlock)
& "$PSScriptRoot\RawrXD_Drive.ps1" -Action parallel -Tasks $Tasks -MaxParallel $MaxParallel
