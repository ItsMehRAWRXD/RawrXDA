@echo off
setlocal
if not exist web-mini-ide\package.json (
  echo web-mini-ide\package.json not found. Make sure your web project is at web-mini-ide\ .
  exit /b 1
)

powershell -NoProfile -Command ^
  "$c=@'
{
  "name": "star5ide-root",
  "private": true,
  "version": "0.0.0",
  "scripts": {
    "postinstall": "npm -C web-mini-ide ci || npm -C web-mini-ide install",
    "dev": "npm -C web-mini-ide run dev",
    "build": "npm -C web-mini-ide run build",
    "preview": "npm -C web-mini-ide run preview"
  }
}
'@; Set-Content -Path package.json -Value $c -Encoding UTF8"

echo Wrote root package.json for StackBlitz proxy scripts.
endlocal
