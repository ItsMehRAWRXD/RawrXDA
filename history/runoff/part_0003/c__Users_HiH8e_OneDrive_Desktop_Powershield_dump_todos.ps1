$global:currentWorkingDir = Get-Location
$script:agentTodoExcludePaths = @('.git', 'node_modules', '.vs', 'bin', 'obj', 'dist', 'packages', '__pycache__', 'vendor', 'training_output', '.venv')

function Get-AgentTodoList {
    param(
        [string]$RootPath = $global:currentWorkingDir,
        [switch]$IncludeAllFiles
    )

    if (-not $RootPath) {
        $RootPath = $global:currentWorkingDir
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

    $matches = Select-String -Path ($files | Select-Object -ExpandProperty FullName) -Pattern 'TODO|FIXME|XXX' -AllMatches -ErrorAction SilentlyContinue
    $todoList = @()

    foreach ($match in $matches) {
        $token = ($match.Matches | Select-Object -First 1).Value
        $todoList += [ordered]@{
            File    = $match.Path
            Line    = $match.LineNumber
            Text    = $match.Line.Trim()
            Token   = $token
        }
    }
    return $todoList
}

$todos = Get-AgentTodoList
$todos | Export-Csv -Path "all_todos.csv" -NoTypeInformation
Write-Host "Found $($todos.Count) TODOs. Saved to all_todos.csv"