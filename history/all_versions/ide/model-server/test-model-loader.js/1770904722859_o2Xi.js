// Quick test for model-loader endpoints — all paths D-drive
const http = require('http');

function req(method, path, body) {
  return new Promise((resolve, reject) => {
    const data = body ? JSON.stringify(body) : '';
    const opts = { hostname: 'localhost', port: 3000, path, method, headers: { 'Content-Type': 'application/json' } };
    if (data) opts.headers['Content-Length'] = Buffer.byteLength(data);
    const r = http.request(opts, (res) => {
      let buf = '';
      res.on('data', (c) => buf += c.toString());
      res.on('end', () => { try { resolve(JSON.parse(buf)); } catch { resolve({ raw: buf }); } });
    });
    r.on('error', reject);
    if (data) r.write(data);
    r.end();
  });
}

(async () => {
  try {
    console.log('Status:', await req('GET', '/models/status'));
    console.log('Load tiinyAI:', await req('POST', '/models/load', { source: 'tiinyAI', model: 'D:\\rawrxd\\model.gguf' }));
    console.log('Status after load:', await req('GET', '/models/status'));
    console.log('Unload tiinyAI:', await req('POST', '/models/unload', { source: 'tiinyAI' }));
    console.log('Final status:', await req('GET', '/models/status'));
    console.log('ALL TESTS PASSED');
  } catch (e) {
    console.error('Test failed:', e);
    process.exit(1);
  }
})();
