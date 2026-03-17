param(
  [switch]$NoEnv,
  [switch]$Bootstrap
)

$ErrorActionPreference = 'Stop'

function Ensure-DevEnv {
  if ($NoEnv) { return }
  # Try to locate VS DevCmd via vswhere
  $vswhere = "$Env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe"
  if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) {
      $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
      if (Test-Path $vcvars) {
        Write-Host "Initializing VS build environment..."
        cmd /c "`"$vcvars`" && set" | ForEach-Object {
          $name, $value = $_ -split '=', 2
          if ($name -and $value) { Set-Item -Path Env:$name -Value $value }
        }
      }
    }
  }
}

function Check-Tools {
  $ml = Get-Command ml64 -ErrorAction SilentlyContinue
  $link = Get-Command link -ErrorAction SilentlyContinue
  if (-not $ml) { throw 'ml64.exe not found in PATH. Open a VS x64 Developer Command Prompt or run without -NoEnv.' }
  if (-not $link) { throw 'link.exe not found in PATH. Open a VS x64 Developer Command Prompt or run without -NoEnv.' }
}

function Build-All {
  $src = @(
    'copilot_http_client.asm',
    'copilot_auth.asm',
    'copilot_chat_protocol.asm',
    'copilot_token_parser.asm',
    'chat_stream_ui.asm',
    'tool_integration.asm',
    'agentic_loop.asm',
    'cursor_cmdk.asm',
    'diff_engine.asm',
    'model_router.asm'
  )
  foreach ($f in $src) {
    & ml64 /c /Fo ($f -replace '\.asm$','.obj') $f
  }
  & link /OUT:RawrXD-AI-MASM.exe /SUBSYSTEM:CONSOLE *.obj kernel32.lib user32.lib winhttp.lib crypt32.lib bcrypt.lib shell32.lib
}

function Build-Bootstrap {
  Write-Host "Building bootstrap (main.asm) to validate toolchain..."
  & ml64 /c /Fo main.obj main.asm
  & link /OUT:RawrXD-AI-MASM.exe /SUBSYSTEM:CONSOLE /ENTRY:main main.obj kernel32.lib
}

Push-Location $PSScriptRoot
try {
  Ensure-DevEnv
  Check-Tools
  if ($Bootstrap) {
    Build-Bootstrap
  } else {
    Build-All
  }
  Write-Host 'Build complete: RawrXD-AI-MASM.exe'
} finally {
  Pop-Location
}
