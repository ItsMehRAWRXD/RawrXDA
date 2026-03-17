$global:currentWorkingDir = Get-Location
$script:agentTodoExcludePaths = @('.git', 'node_modules', '.vs', 'bin', 'obj', 'dist', 'packages', '__pycache__', 'vendor')

function Get-AgentTodoList {
    param(
        [string]$RootPath = $global:currentWorkingDir,
        [switch]$IncludeAllFiles
    )

    if (-not $RootPath) {
        $RootPath = $global:currentWorkingDir
    }

    if (-not (Test-Path $RootPath)) {
        return @{ success = $false; error = "Root path not found: $RootPath" }
    }

    $normalizedRoot = [System.IO.Path]::GetFullPath($RootPath)
    try {
        $files = Get-ChildItem -Path $normalizedRoot -Recurse -File -ErrorAction SilentlyContinue
    }
    catch {
        return @{ success = $false; error = $_.Exception.Message }
    }

    $files = $files | Where-Object {
        $full = $_.FullName.ToLower()
        -not ($script:agentTodoExcludePaths | Where-Object { $full -like "*$_*" } | Select-Object -First 1)
    }

    if (-not $IncludeAllFiles) {
        $extensions = @('.ps1','.psm1','.psd1','.psm','.txt','.md','.json','.js','.ts','.py','.cs','.cpp','.h','.csproj','.sln','.yaml','.yml','.config','.xml')
        $files = $files | Where-Object { $extensions -contains $_.Extension.ToLower() }
    }

    if (-not $files) {
        return @{
            success    = $true
            root_path  = $normalizedRoot
            todos      = @()
            summary    = @{ total = 0; todo = 0; fixme = 0; xxx = 0 }
        }
    }

    $matches = Select-String -Path ($files | Select-Object -ExpandProperty FullName) -Pattern 'TODO|FIXME|XXX' -AllMatches -ErrorAction SilentlyContinue
    $todoList = @()

    foreach ($match in $matches) {
        $token = ($match.Matches | Select-Object -First 1).Value
        $todoList += [ordered]@{
            File    = $match.Path
            Line    = $match.LineNumber
            Text    = $match.Line.Trim()
            Token   = $token
            Context = $match.Line.Trim()
        }
    }

    $summary = @{
        total = $todoList.Count
        todo  = ($todoList | Where-Object { $_.Token -eq 'TODO' }).Count
        fixme = ($todoList | Where-Object { $_.Token -eq 'FIXME' }).Count
        xxx   = ($todoList | Where-Object { $_.Token -eq 'XXX' }).Count
    }

    return @{
        success   = $true
        root_path = $normalizedRoot
        todos     = $todoList
        summary   = $summary
    }
}

$result = Get-AgentTodoList
Write-Host "Summary:"
$result.summary | Format-Table
Write-Host "First 5 Todos:"
$result.todos | Select-Object -First 5 | Format-Table
