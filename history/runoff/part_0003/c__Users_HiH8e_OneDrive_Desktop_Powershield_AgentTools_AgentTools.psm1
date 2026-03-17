# AgentTools PowerShell Module
# Provides filesystem and command utilities with basic safety checks

$script:AllowedRoots = @(
    [IO.Path]::GetFullPath("C:\\Users\\HiH8e\\OneDrive\\Desktop\\"),
    [IO.Path]::GetFullPath("D:\\"),
    [IO.Path]::GetFullPath("E:\\")
)

# Ensure legacy encodings are available and provide a safe encoding resolver
[System.Text.Encoding]::RegisterProvider([System.Text.CodePagesEncodingProvider]::Instance)

function Get-EncodingSafe {
    param([string]$Name = 'utf8')
    if ([string]::IsNullOrWhiteSpace($Name)) { $Name = 'utf8' }
    switch -Regex ($Name.ToLowerInvariant()) {
        '^(utf8|utf-8)$' { return [Text.Encoding]::UTF8 }
        '^(utf16|unicode)$' { return [Text.Encoding]::Unicode }
        '^ascii$' { return [Text.Encoding]::ASCII }
        default {
            try { return [Text.Encoding]::GetEncoding($Name) } catch { return [Text.Encoding]::UTF8 }
        }
    }
}

function Test-AllowedPath {
    param(
        [Parameter(Mandatory)] [string] $Path
    )
    try {
        $full = [IO.Path]::GetFullPath($Path)
    } catch { return $false }
    # Normalize roots to end with separator, and allow exact match to root
    foreach ($root in $script:AllowedRoots) {
        $normRoot = if ($root.EndsWith([IO.Path]::DirectorySeparatorChar)) { $root } else { $root + [IO.Path]::DirectorySeparatorChar }
        $fullWithSep = if ((Test-Path -LiteralPath $full) -and (Get-Item -LiteralPath $full).PSIsContainer) {
            if ($full.EndsWith([IO.Path]::DirectorySeparatorChar)) { $full } else { $full + [IO.Path]::DirectorySeparatorChar }
        } else { $full }
        if ($full.Equals($root, [System.StringComparison]::OrdinalIgnoreCase) -or $full.Equals($normRoot.TrimEnd([IO.Path]::DirectorySeparatorChar), [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
        if ($fullWithSep.StartsWith($normRoot, [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
    }
    return $false
}

function Resolve-FullPath {
    param([Parameter(Mandatory)][string]$Path)
    return [IO.Path]::GetFullPath($Path)
}

function Get-FileContentSafe {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] [string] $Path,
        [int] $StartLine,
        [int] $EndLine,
        [string] $Encoding = 'utf8',
        [int] $MaxBytes = 200000
    )
    if (-not (Test-AllowedPath -Path $Path)) { throw "Access denied for path: $Path" }
    $full = Resolve-FullPath $Path
    if (-not (Test-Path -LiteralPath $full)) { throw "Path not found: $full" }

    $fi = Get-Item -LiteralPath $full -ErrorAction Stop
    if ($fi.PSIsContainer) { throw "Path is a directory: $full" }

    $bytes = [IO.File]::ReadAllBytes($full)
    if ($bytes.Length -gt $MaxBytes) {
        $bytes = $bytes[0..($MaxBytes-1)]
    }
    $enc = Get-EncodingSafe -Name $Encoding
    $content = $enc.GetString($bytes)

    if ($PSBoundParameters.ContainsKey('StartLine') -and $PSBoundParameters.ContainsKey('EndLine')) {
        if ($StartLine -lt 1 -or $EndLine -lt $StartLine) { throw "Invalid line range" }
        $lines = $content -split "`n"
        $maxIndex = [Math]::Min($EndLine, $lines.Count)
        $minIndex = [Math]::Min($StartLine, $maxIndex)
        $slice = $lines[($minIndex-1)..($maxIndex-1)] -join "`n"
        return $slice
    }
    return $content
}

function Set-FileContentSafe {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Content,
        [string] $Encoding = 'utf8'
    )
    if (-not (Test-AllowedPath -Path $Path)) { throw "Access denied for path: $Path" }
    $full = Resolve-FullPath $Path
    $dir = Split-Path -Path $full -Parent
    if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    [IO.File]::WriteAllText($full, $Content, (Get-EncodingSafe -Name $Encoding))
    Get-Item -LiteralPath $full | Select-Object FullName, Length, LastWriteTime
}

function Add-FileContentSafe {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Content,
        [string] $Encoding = 'utf8'
    )
    if (-not (Test-AllowedPath -Path $Path)) { throw "Access denied for path: $Path" }
    $full = Resolve-FullPath $Path
    $dir = Split-Path -Path $full -Parent
    if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    [IO.File]::AppendAllText($full, $Content, (Get-EncodingSafe -Name $Encoding))
    Get-Item -LiteralPath $full | Select-Object FullName, Length, LastWriteTime
}

function Get-DirectoryListingSafe {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] [string] $Path,
        [switch] $Recurse
    )
    if (-not (Test-AllowedPath -Path $Path)) { throw "Access denied for path: $Path" }
    $full = Resolve-FullPath $Path
    if (-not (Test-Path -LiteralPath $full)) { throw "Path not found: $full" }
    $params = @{ LiteralPath=$full; ErrorAction='Stop' }
    if ($Recurse) { $params['Recurse'] = $true }
    Get-ChildItem @params | Select-Object Name, FullName, Length, LastWriteTime, Attributes, @{N='Type';E={ if ($_.PSIsContainer) { 'Directory' } else { 'File' } }}
}

function Remove-PathSafe {
    [CmdletBinding(SupportsShouldProcess)]
    param(
        [Parameter(Mandatory)] [string] $Path,
        [switch] $Recurse
    )
    if (-not (Test-AllowedPath -Path $Path)) { throw "Access denied for path: $Path" }
    $full = Resolve-FullPath $Path
    if (-not (Test-Path -LiteralPath $full)) { return $true }
    if ($PSCmdlet.ShouldProcess($full, 'Remove')) {
        Remove-Item -LiteralPath $full -Force -ErrorAction Stop -Recurse:$Recurse
        return $true
    }
}

function Copy-PathSafe {
    [CmdletBinding(SupportsShouldProcess)]
    param(
        [Parameter(Mandatory)] [string] $Source,
        [Parameter(Mandatory)] [string] $Destination,
        [switch] $Recurse
    )
    if (-not (Test-AllowedPath -Path $Source)) { throw "Access denied for source: $Source" }
    if (-not (Test-AllowedPath -Path $Destination)) { throw "Access denied for destination: $Destination" }
    $src = Resolve-FullPath $Source
    $dst = Resolve-FullPath $Destination
    $dstDir = Split-Path -Path $dst -Parent
    if (-not (Test-Path -LiteralPath $dstDir)) { New-Item -ItemType Directory -Path $dstDir -Force | Out-Null }
    if ($PSCmdlet.ShouldProcess("$src -> $dst", 'Copy')) {
        Copy-Item -LiteralPath $src -Destination $dst -Force -Recurse:$Recurse -ErrorAction Stop
        return (Get-Item -LiteralPath $dst)
    }
}

function Move-PathSafe {
    [CmdletBinding(SupportsShouldProcess)]
    param(
        [Parameter(Mandatory)] [string] $Source,
        [Parameter(Mandatory)] [string] $Destination
    )
    if (-not (Test-AllowedPath -Path $Source)) { throw "Access denied for source: $Source" }
    if (-not (Test-AllowedPath -Path $Destination)) { throw "Access denied for destination: $Destination" }
    $src = Resolve-FullPath $Source
    $dst = Resolve-FullPath $Destination
    $dstDir = Split-Path -Path $dst -Parent
    if (-not (Test-Path -LiteralPath $dstDir)) { New-Item -ItemType Directory -Path $dstDir -Force | Out-Null }
    if ($PSCmdlet.ShouldProcess("$src -> $dst", 'Move')) {
        Move-Item -LiteralPath $src -Destination $dst -Force -ErrorAction Stop
        return (Get-Item -LiteralPath $dst)
    }
}

function Search-TextSafe {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [switch] $IsRegex,
        [string] $Include,
        [int] $MaxResults = 200
    )
    if (-not (Test-AllowedPath -Path $Path)) { throw "Access denied for path: $Path" }
    $full = Resolve-FullPath $Path
    if (-not (Test-Path -LiteralPath $full)) { throw "Path not found: $full" }
    $results = @()
    try {
        $fileList = @()
        $item = Get-Item -LiteralPath $full -ErrorAction Stop
        if ($item.PSIsContainer) {
            $gciParams = @{ LiteralPath=$full; Recurse=$true; File=$true; ErrorAction='Stop' }
            if ($Include) { $gciParams['Include'] = $Include }
            $fileList = Get-ChildItem @gciParams | Select-Object -ExpandProperty FullName
        } else {
            $fileList = @($item.FullName)
        }
        if (-not $fileList -or $fileList.Count -eq 0) { return @() }

        $ssParams = @{ Pattern=$Pattern; ErrorAction='Stop' }
        if (-not $IsRegex) { $ssParams['SimpleMatch'] = $true }
        $matches = Select-String @ssParams -Path $fileList
        foreach ($m in $matches) {
            $results += [PSCustomObject]@{
                File = $m.Path
                Line = $m.Line
                LineNumber = $m.LineNumber
                ColumnNumber = $m.ColumnNumber
                Match = ($m.Matches | ForEach-Object Value) -join ', '
            }
            if ($results.Count -ge $MaxResults) { break }
        }
    } catch {
        throw $_
    }
    return $results
}

function Invoke-CommandSafe {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] [string] $Command,
        [string[]] $Arguments,
        [string] $Cwd,
        [int] $TimeoutSec = 0, # 0 = no timeout
        [switch] $Background
    )
    if ($Cwd) {
        if (-not (Test-AllowedPath -Path $Cwd)) { throw "Access denied for cwd: $Cwd" }
        Push-Location -LiteralPath (Resolve-FullPath $Cwd)
    }
    try {
        if ($Background) {
            $scriptBlock = {
                param($cmd, $args)
                & $cmd @args 2>&1 | ForEach-Object { $_ | Out-String }
            }
            $job = Start-Job -ScriptBlock $scriptBlock -ArgumentList @($Command, $Arguments)
            return [PSCustomObject]@{ Background = $true; JobId = $job.Id; State = $job.State }
        } else {
            $ps = [System.Diagnostics.ProcessStartInfo]::new()
            $ps.FileName = $Command
            if ($Arguments) { $ps.Arguments = [string]::Join(' ', ($Arguments | ForEach-Object { '"' + ($_ -replace '"','\"') + '"' })) }
            $ps.RedirectStandardOutput = $true
            $ps.RedirectStandardError = $true
            $ps.UseShellExecute = $false
            if ($Cwd) { $ps.WorkingDirectory = (Get-Location).Path }
            $proc = [System.Diagnostics.Process]::Start($ps)
            if ($TimeoutSec -gt 0) { $null = $proc.WaitForExit($TimeoutSec * 1000) } else { $proc.WaitForExit() }
            $stdout = $proc.StandardOutput.ReadToEnd()
            $stderr = $proc.StandardError.ReadToEnd()
            return [PSCustomObject]@{ Background=$false; ExitCode=$proc.ExitCode; Stdout=$stdout; Stderr=$stderr }
        }
    } finally {
        if ($Cwd) { Pop-Location }
    }
}

Export-ModuleMember -Function *Safe, Test-AllowedPath, Resolve-FullPath
