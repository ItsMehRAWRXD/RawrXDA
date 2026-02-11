const { spawnSync } = require('child_process');
const pkg = require('fs').readFileSync('/workspace/demo/secret.pkg','utf8');
const out = spawnSync(process.execPath, ['${ROOT}/tools/carmilla-openssl.js','decrypt','--pass','demo'], { input: pkg, encoding: 'utf8' });
console.log('[injected] decrypted =>', out.stdout.toString());
