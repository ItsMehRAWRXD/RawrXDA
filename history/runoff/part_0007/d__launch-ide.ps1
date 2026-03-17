param(
  [ValidateSet("amazonq","mycopilot")]
  [string]$Target = "mycopilot"
)

function Start-Ide {
  param(
    [Parameter(Mandatory=$true)][string]$root,
    [Parameter(Mandatory=$true)][string]$pattern
  )

  if (-not (Test-Path -LiteralPath $root)) {
    Write-Error "Path not found: $root"
    exit 1
  }

  $exe = Get-ChildItem -LiteralPath $root -Filter $pattern -File -Recurse -ErrorAction SilentlyContinue |
    Sort-Object -Property Length -Descending |
    Select-Object -First 1

  if (-not $exe) {
    # Fallback: pick any .exe in the root folder
    $exe = Get-ChildItem -LiteralPath $root -Filter *.exe -File -Recurse -ErrorAction SilentlyContinue |
      Sort-Object -Property LastWriteTime -Descending |
      Select-Object -First 1
  }

  if (-not $exe) {
    Write-Error "No executable found in $root"
    exit 1
  }

  Write-Host "Launching: $($exe.FullName)"
  Start-Process -FilePath $exe.FullName -WorkingDirectory $root
}

switch ($Target) {
  "amazonq"   { $root = $env:AMAZONQ_HOME  ; if (-not $root) { $root = "D:\\amazonq-ide" }
                try {
                  Start-Ide -root $root -pattern "*AmazonQ*.exe"
                } catch {
                  # Fallback to top-level known exe
                  if (Test-Path -LiteralPath "D:\\AmazonQ-IDE.exe") {
                    Start-Process -FilePath "D:\\AmazonQ-IDE.exe" -WorkingDirectory (Split-Path -Path "D:\\AmazonQ-IDE.exe")
                  } else {
                    throw
                  }
                } }
  "mycopilot" { $root = $env:MYCOPILOT_HOME; if (-not $root) { $root = "D:\MyCoPilot-Complete-Portable" }
                Start-Ide -root $root -pattern "*Copilot*.exe" }
}
