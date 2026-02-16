#!/bin/sh
set -eu

for candidate in \
  /opt/rawrxd/Ship/RawrEngine.py \
  /opt/rawrxd/backend/RawrEngine.py \
  /opt/rawrxd/backend/rawr_engine.py \
  /opt/rawrxd/Ship/chat_server.py \
  /opt/rawrxd/backend/chat_server.py
do
  if [ -f "$candidate" ]; then
    exec python "$candidate"
  fi
done

echo "No backend entrypoint found in container image." >&2
exit 1