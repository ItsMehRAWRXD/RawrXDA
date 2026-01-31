#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"

# 1) Encrypt a message using Carmilla (OpenSSL) CLI
echo "secret text" | node "$ROOT/tools/carmilla-openssl.js" encrypt --pass demo --hint demo-hint > "$DIR/secret.pkg"
echo "[demo] packaged to $DIR/secret.pkg"

# 2) Write multi-line injection file
cat > "$DIR/inject.js" <<'EOF'
const { spawnSync } = require('child_process');
const pkg = require('fs').readFileSync('/workspace/demo/secret.pkg','utf8');
const out = spawnSync(process.execPath, ['${ROOT}/tools/carmilla-openssl.js','decrypt','--pass','demo'], { input: pkg, encoding: 'utf8' });
console.log('[injected] decrypted =>', out.stdout.toString());
EOF

# 3) Patch and run target with two injection points (first from file, second inline)
node "$ROOT/tools/carpatcher.js" js "$DIR/car-target.js" --patch-file "$DIR/inject.js" --patch "console.log('[injected] second patch');" --run

