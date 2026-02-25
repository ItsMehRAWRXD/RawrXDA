 #!/bin/bash
set -e
export WINEPREFIX="${WINEPREFIX:-/data/space}" DISPLAY=:99
Xvfb :99 -screen 0 1920x1080x24 -ac &
sleep 2
mkdir -p ~/.vnc
x11vnc -storepasswd "${VNC_PASSWORD:-rawrxd}" ~/.vnc/passwd 2>/dev/null || true
x11vnc -display :99 -forever -shared -rfbport 5900 -rfbauth ~/.vnc/passwd -noxdamage &
[ ! -d "$WINEPREFIX/drive_c" ] && wine wineboot --init 2>/dev/null || true
for d in /data/ide /data/space/drive_c/rawrxd /data/repo/build_ide/bin /data/repo/build/bin; do
  [ -f "$d/RawrXD-Win32IDE.exe" ] && exec wine "$d/RawrXD-Win32IDE.exe" "$@"
done
wait
