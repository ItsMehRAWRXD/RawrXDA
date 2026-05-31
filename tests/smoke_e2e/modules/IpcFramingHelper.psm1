# Thin module wrapper — dot-source IpcFramingHelper.ps1 for smoke harness + Import-Module callers
. (Join-Path $PSScriptRoot "IpcFramingHelper.ps1")

Export-ModuleMember -Function @(
    'Get-IpcConstants',
    'Get-Crc32',
    'New-LegacyFrame',
    'New-SegmentFrame',
    'New-PhysicalFramesFromLogicalPayload',
    'Add-WirePrefix',
    'Write-PrefixedFramesToStream',
    'Start-ExtensionPipeMockJob',
    'Stop-ExtensionPipeMockJob'
)
