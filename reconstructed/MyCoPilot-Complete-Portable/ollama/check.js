// Simple Ollama status + tiny generation check via backend proxy
const http = require('http');

function get(path) {
  return new Promise((resolve, reject) => {
    const req = http.request({ host: 'localhost', port: 8080, path, method: 'GET' }, (res) => {
      let data = '';
      res.on('data', (c) => (data += c));
      res.on('end', () => {
        try { resolve(JSON.parse(data)); } catch (e) { reject(e); }
      });
    });
    req.on('error', reject);
    req.end();
  });
}

function post(path, body) {
  return new Promise((resolve, reject) => {
    const raw = JSON.stringify(body);
    const req = http.request({ host: 'localhost', port: 8080, path, method: 'POST', headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(raw) } }, (res) => {
      let data = '';
      res.on('data', (c) => (data += c));
      res.on('end', () => {
        try { resolve(JSON.parse(data)); } catch (e) { reject(e); }
      });
    });
    req.on('error', reject);
    req.write(raw);
    req.end();
  });
}

(async () => {
  const status = await get('/api/ollama/status').catch((e) => ({ online: false, error: e.message }));
  if (!status.online) {
    console.log('Ollama Status: Offline', status.error ? `(${status.error})` : '');
    process.exit(1);
  }
  console.log('Ollama Status: Online');
  if (Array.isArray(status.models)) {
    console.log('Available Models:');
    status.models.forEach((m) => console.log('-', m.name));
  }
  const model = (status.models && status.models[0] && status.models[0].name) || 'codellama:7b';
  console.log(`\nTesting generation with ${model}...`);
  const resp = await post('/api/ollama', { model, prompt: 'Hello, three word reply please.', stream: false });
  console.log('Response:\n', resp?.response || resp);
  console.log('\nStatus: ✅ Local LLM ready');
})().catch((e) => { console.error('Check failed:', e.message); process.exit(1); });
